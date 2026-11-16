"""crosswalk.py: underworld-exhumed's names and matched sources (uw1/map/crosswalk.tsv) on our routine addresses
(file offsets, re/uw_map.txt) -> re/crosswalk.tsv: "<file offset>\t<name>\t<exhumed source file:line>\t<size>".

The listing's load-image segments are ours + 0x1ED paragraphs (skill_check: listing seg037_32E6_C, ours 30f9:000c);
its overlays ovrNNN are numbered in the order of the overlay stubs; the numbering is checked by how many routines
land on our map's routine starts.

    uv run --no-project python -I tools/crosswalk.py game/UW.EXE underworld-exhumed/uw1/map/crosswalk.tsv re/uw_map.txt re/crosswalk.tsv
"""
import re
import struct
import sys

IMG, OVL_BASE, SHIFT = 0x3200, 0x65710, 0x1ED


def main():
    exe, cw, mapf, outp = sys.argv[1:5]
    d = open(exe, 'rb').read()
    fbov = d.index(b'FBOV')
    tab, count = struct.unpack('<IH', d[fbov + 8:fbov + 14])
    segs = [struct.unpack('<4H', d[tab + k * 8:tab + k * 8 + 8]) for k in range(count)]
    stubs = []
    for seg, size, typ, _ in segs:
        if typ == 3:
            a = IMG + seg * 16
            fo, = struct.unpack('<I', d[a + 4:a + 8])
            stubs.append((seg, OVL_BASE + fo))
    starts = set()
    for line in open(mapf, encoding='utf-8', errors='replace'):
        m = re.match(r'([0-9a-f]{6}) ', line)
        if m:
            starts.add(int(m.group(1), 16))
    rows = []
    for line in open(cw, encoding='utf-8'):
        if line.startswith('#'):
            continue
        p = line.rstrip('\n').split('\t')
        if len(p) < 5:
            continue
        rows.append(p)
    ovr_nums = sorted({int(m.group(1)) for p in rows for m in [re.match(r'ovr(\d+)_', p[1])] if m})

    # segments named with their paragraph somewhere (segNNN_PARA_OFF): the others' short form segNNN_OFF uses it
    seg_para = {}
    for p in rows:
        for m in re.finditer(r'seg(\d+)_([0-9A-F]{4})_[0-9A-F]+', '\t'.join(p)):
            seg_para.setdefault(int(m.group(1)), int(m.group(2), 16))

    def load_image(addr):
        m = re.match(r'seg(\d+)_([0-9A-F]{4})_([0-9A-F]+)$', addr)
        if m:
            return IMG + (int(m.group(2), 16) - SHIFT) * 16 + int(m.group(3), 16)
        m = re.match(r'seg(\d+)_([0-9A-F]+)$', addr)
        if m and int(m.group(1)) in seg_para:
            return IMG + (seg_para[int(m.group(1))] - SHIFT) * 16 + int(m.group(2), 16)
        return None

    # the overlay numbering: ovr k -> stubs[k - base]; pick the base that puts the most routines on our starts
    best = None
    for base in range(min(ovr_nums) - len(stubs), min(ovr_nums) + 1):
        hits = 0
        for p in rows:
            m = re.match(r'ovr(\d+)_([0-9A-F]+)$', p[1])
            if m and 0 <= int(m.group(1)) - base < len(stubs):
                hits += stubs[int(m.group(1)) - base][1] + int(m.group(2), 16) in starts
        if best is None or hits > best[1]:
            best = (base, hits)
    base = best[0]
    out, placed, onstart = [], 0, 0
    for p in rows:
        f = load_image(p[1])
        m = re.match(r'ovr(\d+)_([0-9A-F]+)$', p[1])
        if m and 0 <= int(m.group(1)) - base < len(stubs):
            f = stubs[int(m.group(1)) - base][1] + int(m.group(2), 16)
        if f is None:
            continue
        placed += 1
        onstart += f in starts
        out.append('%06x\t%s\t%s\t%s' % (f, p[3], p[4], p[2]))
    out.sort()
    open(outp, 'w', encoding='utf-8', newline='\r\n').write(
        '# file offset\tname (underworld-exhumed)\tmatched source\tsize   (tools/crosswalk.py)\n' + '\n'.join(out) + '\n')
    print('overlay base ovr%d; %d routines placed, %d on our map\'s starts (of %d)' % (base, placed, onstart, len(starts)))


if __name__ == '__main__':
    main()
