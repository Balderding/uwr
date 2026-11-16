"""exe_map.py: a map of UW.EXE's code: every routine (in the load image and in the Borland VROOMM overlays),
who calls it, what it calls, and the game messages (strings block 1) it prints.

    uv run --no-project --with capstone python -I tools/exe_map.py game/UW.EXE data/strings re/uw_map.txt

Layout (decoded 2026-11-07, see NOTES.md):
  - MZ header 800 paragraphs: the load image starts at file 0x3200; segment S, offset O = file 0x3200 + S*16 + O.
  - DGROUP (DS) = paragraph 0x5AAC.
  - "FBOV" at 0x65700 (after the load image): +4 overlay size, +8 file offset of the segment table (0x5c1c0),
    +12 its count (161). Entries: seg, size, type (3 = an overlay stub), 0.
  - A stub (at seg): CD 3F, +4 dword = the overlay's code at file 0x65710 + it, +8 code size, +0xA fixup size,
    +0xC entry count; entries from +0x20, 5 bytes each (CD 3F, word = the routine's offset in the overlay).
    A far call to stub:0x20+5k goes to entry k.
"""
import bisect
import collections
import os
import re
import struct
import sys

from capstone import CS_ARCH_X86, CS_MODE_16, Cs

IMG = 0x3200
OVL_BASE = 0x65710


def load_strings(folder, block):
    out = {}
    path = os.path.join(folder, 'block%04d.txt' % block)
    if os.path.exists(path):
        for line in open(path, encoding='utf-8', errors='replace'):
            m = re.match(r'(\d+): (.*)', line.rstrip('\r\n'))
            if m:
                out[int(m.group(1))] = m.group(2)
    return out


def main():
    exe, strdir, outp = sys.argv[1:4]
    d = open(exe, 'rb').read()
    msgs = load_strings(strdir, 1)
    names4 = load_strings(strdir, 4)
    fbov = d.index(b'FBOV')
    tab, count = struct.unpack('<IH', d[fbov + 8:fbov + 14])
    segs = [struct.unpack('<4H', d[tab + k * 8:tab + k * 8 + 8]) for k in range(count)]
    stubs = {}  # stub seg -> (overlay file start, code size, [entry offsets])
    for seg, size, typ, _ in segs:
        if typ != 3:
            continue
        a = IMG + seg * 16
        fo, = struct.unpack('<I', d[a + 4:a + 8])
        cs, rs, ne = struct.unpack('<3H', d[a + 8:a + 14])
        ents = [struct.unpack('<H', d[a + 0x22 + k * 5:a + 0x24 + k * 5])[0] for k in range(ne)]
        stubs[seg] = (OVL_BASE + fo, cs, ents)
    # code regions: load-image segments that are not stubs/data (types 0, 1) and the overlays
    regions = []  # (file start, file end, label seg, label base offset)
    for seg, size, typ, _ in segs:
        if typ in (0, 1) and seg < 0x5AAC and size:
            regions.append((IMG + seg * 16, IMG + seg * 16 + size, 'seg %04x' % seg, seg))
    for seg, (start, cs, ents) in stubs.items():
        regions.append((start, start + cs, 'ovl %04x' % seg, seg))
    regions.sort()

    def region_of(f):
        for r in regions:
            if r[0] <= f < r[1]:
                return r
        return None

    def far_target(s, o, in_overlay=False):
        # overlay code names segments by selector: table index * 8 (fixed up when the overlay loads)
        if in_overlay and s % 8 == 0 and s // 8 < count:
            s = segs[s // 8][0]
        if s in stubs:
            k = (o - 0x20) // 5
            st = stubs[s]
            if (o - 0x20) % 5 == 0 and 0 <= k < len(st[2]):
                return st[0] + st[2][k]
            return None
        return IMG + s * 16 + o

    md = Cs(CS_ARCH_X86, CS_MODE_16)
    md.skipdata = True
    # routine starts: prologues (push bp; mov bp, sp) and every call target
    starts = set()
    for r in regions:
        blob = d[r[0]:r[1]]
        for m in re.finditer(re.escape(b'\x55\x8b\xec'), blob):
            starts.add(r[0] + m.start())
    stub_entries = set()
    for seg, (start, cs, ents) in stubs.items():
        for e in ents:
            stub_entries.add(start + e)
    starts |= stub_entries
    calls = collections.defaultdict(set)
    callers = collections.defaultdict(set)
    printed = collections.defaultdict(list)
    items = collections.defaultdict(set)
    insn_cache = {}
    for r in regions:
        insns = list(md.disasm(d[r[0]:r[1]], r[0]))
        insn_cache[r[0]] = insns
        for i, ins in enumerate(insns):
            if ins.mnemonic == 'lcall' and ins.size == 5:
                o, s = struct.unpack('<HH', ins.bytes[1:5])
                t = far_target(s, o, r[2].startswith('ovl'))
                if t is not None:
                    starts.add(t)
            elif ins.mnemonic == 'call' and ins.bytes[0] == 0xE8:
                t = r[0] + ((ins.address - r[0] + 3 + struct.unpack('<h', ins.bytes[1:3])[0]) & 0xFFFF)
                if r[0] <= t < r[1]:
                    starts.add(t)
    sorted_starts = sorted(starts)

    def owner(addr):
        k = bisect.bisect_right(sorted_starts, addr) - 1
        return sorted_starts[k] if k >= 0 else None

    # the message printer: the routine most called right after "push imm" in the load image (found below)
    pushes_before = collections.Counter()
    for r in regions:
        insns = insn_cache[r[0]]
        for i, ins in enumerate(insns):
            if ins.mnemonic == 'lcall' and i and insns[i - 1].mnemonic == 'push' and insns[i - 1].op_str.startswith('0x'):
                o, s = struct.unpack('<HH', ins.bytes[1:5])
                if r[2].startswith('ovl') and s % 8 == 0 and s // 8 < count:
                    s = segs[s // 8][0]
                pushes_before[(s, o)] += 1
    printer = (0x32A8, 0x494)  # print strings block 1 message n (verified by the light code: 0x7b, 0x7c)
    for r in regions:
        insns = insn_cache[r[0]]
        for i, ins in enumerate(insns):
            f = owner(ins.address)
            if f is None:
                continue
            if ins.mnemonic == 'lcall' and ins.size == 5:
                o, s = struct.unpack('<HH', ins.bytes[1:5])
                if r[2].startswith('ovl') and s % 8 == 0 and s // 8 < count:
                    s = segs[s // 8][0]
                t = far_target(s, o)
                if t is not None:
                    calls[f].add(t)
                    callers[t].add(f)
                if (s, o) == printer and i and insns[i - 1].mnemonic == 'push':
                    try:
                        printed[f].append(int(insns[i - 1].op_str, 0))
                    except ValueError:
                        pass
            elif ins.mnemonic == 'call' and ins.bytes[0] == 0xE8:
                t = r[0] + ((ins.address - r[0] + 3 + struct.unpack('<h', ins.bytes[1:3])[0]) & 0xFFFF)
                calls[f].add(t)
                callers[t].add(f)
            elif ins.mnemonic == 'cmp' and re.search(r', 0x(1[0-9a-f]{2}|[4-9a-f][0-9a-f])$', ins.op_str):
                v = int(ins.op_str.rsplit(', ', 1)[1], 0)
                if 0x40 <= v < 0x200:
                    items[f].add(v)

    def label(f):
        r = region_of(f)
        if not r:
            return '%06x' % f
        if r[2].startswith('ovl'):
            return '%06x (%s+%04x)' % (f, r[2], f - r[0])
        return '%06x (%04x:%04x)' % (f, r[3], f - IMG - r[3] * 16)

    os.makedirs(os.path.dirname(outp), exist_ok=True)
    with open(outp, 'w', encoding='utf-8', newline='\r\n') as out:
        out.write('# UW.EXE routines (tools/exe_map.py). file offset (segment:offset, or overlay stub + offset)\n')
        out.write('#   callers / calls: counts; msg: strings block 1 messages it prints; items: object ids it compares\n')
        out.write('# %d routines; %d overlays; message printer %04x:%04x (pushes before it: %d)\n\n' % (
            len(sorted_starts), len(stubs), printer[0], printer[1], pushes_before[printer]))
        for f in sorted_starts:
            if not region_of(f):
                continue
            out.write('%s  callers %d  calls %d\n' % (label(f), len(callers[f]), len(calls[f])))
            for m in sorted(set(printed[f])):
                out.write('    msg %d: %s\n' % (m, msgs.get(m, '?')[:110]))
            if items[f]:
                out.write('    items: %s\n' % ' '.join('%x%s' % (v, '(' + names4.get(v, '')[:18] + ')' if v in names4 else '')
                                                   for v in sorted(items[f])))
    print('routines', len(sorted_starts), 'stubs', len(stubs), 'printer pushes', pushes_before[printer])
    print('top printers', pushes_before.most_common(5))


if __name__ == '__main__':
    main()
