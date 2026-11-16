"""coverage.py: every message UW.EXE prints (strings block 1, from re/uw_map.txt) against the uo-new source: which
ones our engine prints (by str_get(1, N) or the same text), grouped by the UW.EXE routine that prints them.

    python -I tools/coverage.py            -> re/coverage.txt
"""
import glob
import re

ROOT = __file__.replace('\\', '/').rsplit('/tools/', 1)[0]


def main():
    block1 = {}
    for line in open(ROOT + '/data/strings/block0001.txt', encoding='utf-8'):
        m = re.match(r'(\d+): (.*)', line.rstrip('\n'))
        if m:
            block1[int(m.group(1))] = m.group(2)
    src = ''
    for p in glob.glob(ROOT + '/src/*.c'):
        src += open(p, encoding='utf-8', errors='replace').read()
    used = {int(n) for n in re.findall(r'str_get\(\s*1\s*,\s*(\d+)\s*\)', src)}
    # str_get(1, 104 + ...) style ranges: the base counts, and the ones it can reach are noted by hand below
    for base, n in ((104, 9), (113, 6)):
        used |= set(range(base, base + n))
    routines = []  # (address, name, [msgs], [items])
    cur = None
    for line in open(ROOT + '/re/uw_map.txt', encoding='utf-8'):
        m = re.match(r'([0-9a-f]{6}) \(([^)]*)\)', line)
        if m:
            cur = [m.group(1), m.group(2), [], []]
            routines.append(cur)
            continue
        m = re.match(r'\s+msg (\d+): (.*)', line)
        if m and cur:
            cur[2].append(int(m.group(1)))
        m = re.match(r'\s+items: (.*)', line)
        if m and cur:
            cur[3].append(m.group(1))
    out = []
    have = miss = 0
    for addr, where, msgs, items in routines:
        if not msgs:
            continue
        rows = []
        for n in msgs:
            text = block1.get(n, '?').replace('\\n', ' ').strip()
            ok = n in used or (len(text) > 12 and text[:24] in src)
            have += ok
            miss += not ok
            rows.append('  %s %3d: %s' % ('ok  ' if ok else 'MISS', n, text))
        out.append('%s (%s)%s' % (addr, where, ('  items: ' + ' '.join(items)) if items else ''))
        out += rows
    head = ['# UW.EXE messages (block 1) by the routine printing them; ok = uo-new prints it too',
            '# %d ok, %d missing' % (have, miss), '']
    open(ROOT + '/re/coverage.txt', 'w', encoding='utf-8').write('\n'.join(head + out) + '\n')
    print('%d ok, %d missing -> re/coverage.txt' % (have, miss))


main()
