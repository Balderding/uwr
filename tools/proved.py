"""proved.py: adds the routines a passing core proof ran to re/proved.txt. Each proof (tools/proof_*.py, run through
tools/coreproof.py) writes work/ran/<script>.txt: the routines of its file that UW.EXE ran in the emulator during the
run, every call compared equal with the rules core's. Run this only after the proof printed "all the same".

    python tools/proved.py proof_objects.py [proof_look.py ...]
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    cross = {}
    for line in open(os.path.join(ROOT, 're', 'crosswalk.tsv')):
        if line.startswith('#'):
            continue
        p = line.rstrip('\n').split('\t')
        if len(p) >= 2:
            cross.setdefault(p[1], p[0])
    path = os.path.join(ROOT, 're', 'proved.txt')
    have = open(path).read()
    done = set(l.split()[0] for l in have.split('\n') if l and not l.startswith('#'))
    added = []
    for script in sys.argv[1:]:
        ran = [l.strip() for l in open(os.path.join(ROOT, 'work', 'ran', script + '.txt')) if l.strip()]
        for name in ran:
            off = cross.get(name)
            if off is None:
                print('%s: %s is not in re/crosswalk.tsv' % (script, name))
                continue
            if off in done:
                continue
            done.add(off)
            added.append('%s %s %s' % (off, script, name))
    if added:
        with open(path, 'a') as f:
            f.write('\n'.join(added) + '\n')
    print('re/proved.txt: %d added' % len(added))


if __name__ == '__main__':
    main()
