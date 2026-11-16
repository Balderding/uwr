"""roadmap.py: ROADMAP.md, the work left by subsystem: underworld-exhumed's matched source files (re/crosswalk.tsv)
with each routine's status from ROUTINES.md (tools/checklist.py), the files in the order to finish them.

    uv run --no-project python -I tools/roadmap.py re/crosswalk.tsv ROUTINES.md ROADMAP.md
"""
import re
import sys

# the order to finish them: what the game's play rests on first (CLAUDE.md: finish before adding)
ORDER = ['game/SKILLCHK.C', 'motion/', 'combat/COMBAT.C', 'combat/DAMAGE.C', 'combat/MISSILE.C', 'critter/', 'obj/',
         'inv/', 'event/', 'combat/', 'game/', 'conv/', 'map/', 'sound/', 'ui/', 'gfx/', '3d/', 'sys/', 'lib/']


def main():
    cwf, routf, outp = sys.argv[1:4]
    status = {}
    for line in open(routf, encoding='utf-8'):
        m = re.match(r'\| ([0-9a-f]{6}) \([^)]*\) \| (\d+) \| \d+ \| [^|]* \| (\w+) \|', line)
        if m:
            status[int(m.group(1), 16)] = (m.group(3), int(m.group(2)))
    files = {}
    for line in open(cwf, encoding='utf-8'):
        if line.startswith('#'):
            continue
        off, name, src, size = line.rstrip('\r\n').split('\t')
        f = src.split(':')[0].replace('src/', '')
        st, sz = status.get(int(off, 16), ('?', int(size, 16)))
        files.setdefault(f, []).append((int(off, 16), name, src, st, sz))

    def rank(f):
        for i, p in enumerate(ORDER):
            if f.startswith(p):
                return (i, f)
        return (len(ORDER), f)

    out = ['# ROADMAP: everything left, subsystem by subsystem', '',
           'Made by tools/roadmap.py from re/crosswalk.tsv (underworld-exhumed\'s matched C on our addresses) and ROUTINES.md. '
           'A file is finished when no routine in it is todo or unproved (CLAUDE.md: finish before adding). '
           'Statuses: proved, unproved (claimed, no verifier), replaced, notneeded, todo.', '',
           '| # | Source file (underworld-exhumed uw1/src) | Routines | Bytes | proved | unproved | replaced / notneeded | todo |',
           '|---|---|---|---|---|---|---|---|']
    order = sorted(files, key=rank)
    for n, f in enumerate(order, 1):
        rs = files[f]
        c = lambda s: sum(1 for r in rs if r[3] == s)  # noqa: E731
        out.append('| %d | %s | %d | %d | %d | %d | %d | %d |' % (n, f, len(rs), sum(r[4] for r in rs), c('proved'), c('unproved'),
                                                                 c('replaced') + c('notneeded'), c('todo') + c('?')))
    out.append('')
    for f in order:
        out.append('## %s' % f)
        out.append('')
        for off, name, src, st, sz in sorted(files[f]):
            out.append('- [%s] %06x %s (%d bytes) %s' % ('x' if st == 'proved' else ' ', off, name, sz, '' if st in ('todo', '?') else st))
        out.append('')
    open(outp, 'w', encoding='utf-8', newline='\r\n').write('\n'.join(out) + '\n')
    tot = [r for f in files for r in files[f]]
    print('%d files, %d routines: proved %d, unproved %d, todo %d' % (len(files), len(tot), sum(r[3] == 'proved' for r in tot),
                                                                     sum(r[3] == 'unproved' for r in tot), sum(r[3] in ('todo', '?') for r in tot)))


if __name__ == '__main__':
    main()
