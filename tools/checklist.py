"""checklist.py: every routine of UW.EXE and where uo-new stands with it -> ROUTINES.md.

    uv run --no-project --with capstone python -I tools/checklist.py game/UW.EXE re/uw_map.txt re/routines_status.txt src ROUTINES.md

Each routine (from tools/exe_map.py's map) gets a status (DECOMP_RULESET.md as CLAUDE.md applies it):
  proved      our C proved identical: an emulator verifier calls it beside our port and every result matches; listed
              with its verifier in re/proved.txt (the ruleset's "matching")
  unproved    our source names its address ("UW.EXE 0x7e444", "(0x7e3ed)" ...) but no verifier has compared it yet: a
              claim, not progress (the ruleset's "non-matching / equivalent": counted apart, never as done)
  replaced    a graphics, sound, input or interface routine that our engine does its own way (a creature comfort)
  notneeded   DOS, memory, overlay, hardware and C runtime plumbing with nothing of the game in it
  todo        game logic not ported yet
The hand-kept file re/routines_status.txt: one line each, "<file offset> <status> <note>" (a status for a whole
segment or overlay: "seg 0ec5 notneeded Borland C runtime"). Hints shown for each routine: how many DOS / BIOS
interrupts (int) and port reads / writes (io) it has, the game messages it prints (from the map).
"""
import os
import re
import struct
import sys

from capstone import CS_ARCH_X86, CS_MODE_16, Cs

IMG, OVL_BASE = 0x3200, 0x65710


def regions_of(d):
    fbov = d.index(b'FBOV')
    tab, count = struct.unpack('<IH', d[fbov + 8:fbov + 14])
    segs = [struct.unpack('<4H', d[tab + k * 8:tab + k * 8 + 8]) for k in range(count)]
    regions = []
    for seg, size, typ, _ in segs:
        if typ in (0, 1) and seg < 0x5AAC and size:
            regions.append((IMG + seg * 16, IMG + seg * 16 + size, 'seg %04x' % seg))
        if typ == 3:
            a = IMG + seg * 16
            fo, = struct.unpack('<I', d[a + 4:a + 8])
            cs, = struct.unpack('<H', d[a + 8:a + 10])
            regions.append((OVL_BASE + fo, OVL_BASE + fo + cs, 'ovl %04x' % seg))
    return sorted(regions)


def main():
    exe, mapf, statf, srcdir, outp = sys.argv[1:6]
    d = open(exe, 'rb').read()
    regions = regions_of(d)
    # the map: routines with their callers and messages
    routines, cur = [], None
    for line in open(mapf, encoding='utf-8', errors='replace'):
        m = re.match(r'([0-9a-f]{6}) \(([^)]*)\)\s+callers (\d+)\s+calls (\d+)', line)
        if m:
            cur = {'off': int(m.group(1), 16), 'label': m.group(2), 'callers': int(m.group(3)), 'msg': ''}
            routines.append(cur)
        elif cur and line.strip().startswith('msg '):
            cur['msg'] += line.strip()[4:].strip()[:60] + ' / '
    routines.sort(key=lambda r: r['off'])
    for r in routines:
        r['region'] = next((g for g in regions if g[0] <= r['off'] < g[1]), None)
    for i, r in enumerate(routines):
        nxt = routines[i + 1]['off'] if i + 1 < len(routines) else len(d)
        end = r['region'][1] if r['region'] else nxt
        r['end'] = min(nxt, end)
        r['size'] = r['end'] - r['off']
    # hardware hints
    md = Cs(CS_ARCH_X86, CS_MODE_16)
    for r in routines:
        n_int = n_io = 0
        for ins in md.disasm(d[r['off']:r['end']], r['off']):
            if ins.mnemonic == 'int' and ins.op_str not in ('0x3f',):
                n_int += 1
            elif ins.mnemonic in ('in', 'out', 'insb', 'outsb', 'insw', 'outsw'):
                n_io += 1
        r['int'], r['io'] = n_int, n_io
    # what the source names
    named = set()
    for root, _, files in os.walk(srcdir):
        for f in files:
            if f.endswith(('.c', '.h', '.hlsl', '.hlsli')):
                for m in re.finditer(r'0x([0-9a-fA-F]{5})\b', open(os.path.join(root, f), encoding='utf-8', errors='replace').read()):
                    named.add(int(m.group(1), 16))
    # the proved routines (re/proved.txt beside the statuses)
    proved = {}
    pf = os.path.join(os.path.dirname(statf), 'proved.txt')
    if os.path.exists(pf):
        for line in open(pf, encoding='utf-8'):
            p = line.split('#')[0].split(None, 2)
            if len(p) >= 2:
                proved[int(p[0], 16)] = ' '.join(p[1:])
    # the hand-kept statuses
    status, note, segstatus = {}, {}, {}
    if os.path.exists(statf):
        for line in open(statf, encoding='utf-8'):
            line = line.split('#')[0].strip() if not line.startswith('seg') and not line.startswith('ovl') else line.strip()
            if not line:
                continue
            p = line.split(None, 3)
            if p[0] in ('seg', 'ovl') and len(p) >= 3:
                segstatus['%s %s' % (p[0], p[1])] = (p[2], p[3] if len(p) > 3 else '')
            elif len(p) >= 2:
                status[int(p[0], 16)] = p[1]
                note[int(p[0], 16)] = p[2] if len(p) > 2 else ''
    for r in routines:
        reg = r['region'][2] if r['region'] else '?'
        r['reg'] = reg
        if r['off'] in proved:
            r['status'], r['note'] = 'proved', proved[r['off']]
        elif r['off'] in status:
            r['status'], r['note'] = status[r['off']], note[r['off']]
        elif any(r['off'] <= a < r['end'] for a in named):
            r['status'], r['note'] = 'unproved', ''
        elif reg in segstatus:
            r['status'], r['note'] = segstatus[reg]
        else:
            r['status'], r['note'] = 'todo', ''
    # the report
    tot = {'proved': [0, 0], 'unproved': [0, 0], 'ported': [0, 0], 'replaced': [0, 0], 'notneeded': [0, 0], 'todo': [0, 0]}
    for r in routines:
        t = tot.setdefault(r['status'], [0, 0])
        t[0] += 1
        t[1] += r['size']
    allb = sum(r['size'] for r in routines)
    done = tot['proved'][1] + tot['ported'][1] + tot['replaced'][1] + tot['notneeded'][1]
    game = allb - tot['replaced'][1] - tot['notneeded'][1]  # the game's own logic
    out = ['# UW.EXE, routine by routine', '',
           'Made by tools/checklist.py from re/uw_map.txt, re/routines_status.txt and the addresses the source names. '
           'Status: **proved** (identical to UW.EXE by an emulator verifier: re/proved.txt), **unproved** (our source names '
           'it, no verifier yet: not counted as done), **ported** (set by hand), **replaced** (graphics, sound, input, '
           'interface: our engine\'s own way), **notneeded** (DOS, memory, hardware, C runtime), **todo**.', '',
           '**Done: %.1f%% of the code** (%d of %d bytes; %d of %d routines); **the game logic proved: %.1f%%** (%d of %d '
           'bytes); unproved claims: %d routines, %d bytes' % (100.0 * done / max(1, allb), done, allb,
                                                         len(routines) - tot['todo'][0] - tot['unproved'][0], len(routines),
                                                         100.0 * tot['proved'][1] / max(1, game), tot['proved'][1], game,
                                                         tot['unproved'][0], tot['unproved'][1]), '',
           '| Status | Routines | Bytes |', '|---|---|---|']
    for k in ('proved', 'unproved', 'ported', 'replaced', 'notneeded', 'todo'):
        out.append('| %s | %d | %d |' % (k, tot[k][0], tot[k][1]))
    out.append('')
    by = {}
    for r in routines:
        by.setdefault(r['reg'], []).append(r)
    for reg in sorted(by, key=lambda g: by[g][0]['off']):
        rs = by[reg]
        b = sum(x['size'] for x in rs)
        dn = sum(x['size'] for x in rs if x['status'] not in ('todo', 'unproved'))
        out.append('## %s (%d routines, %d bytes, %.0f%% done)' % (reg, len(rs), b, 100.0 * dn / max(1, b)))
        out.append('')
        out.append('| Routine | Size | Callers | Hints | Status | Note / messages |')
        out.append('|---|---|---|---|---|---|')
        for x in rs:
            hints = ' '.join(h for h in (('int %d' % x['int']) if x['int'] else '', ('io %d' % x['io']) if x['io'] else '') if h)
            txt = (x['note'] + ' ' + x['msg'][:90]).strip().replace('|', '/')
            out.append('| %06x (%s) | %d | %d | %s | %s | %s |' % (x['off'], x['label'], x['size'], x['callers'], hints, x['status'], txt))
        out.append('')
    open(outp, 'w', encoding='utf-8', newline='\r\n').write('\n'.join(out) + '\n')
    print('done %.1f%%, game logic proved %.1f%%: proved %d, unproved %d, ported %d, replaced %d, notneeded %d, todo %d routines' % (
        100.0 * done / max(1, allb), 100.0 * tot['proved'][1] / max(1, game), tot['proved'][0], tot['unproved'][0],
        tot['ported'][0], tot['replaced'][0], tot['notneeded'][0], tot['todo'][0]))


if __name__ == '__main__':
    main()
