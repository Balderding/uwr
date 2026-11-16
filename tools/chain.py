"""chain.py: follow an object's link chain in a level (what a switch or trigger sets off), with names.

    python -I tools/chain.py data/levels/level1.txt data/strings/block0004.txt OBJ [OBJ ...]
"""
import sys


def main():
    path, names_path = sys.argv[1], sys.argv[2]
    names = {}
    for line in open(names_path, encoding='utf-8'):
        if ':' in line and line[0].isdigit():
            i, s = line.split(': ', 1)
            names[int(i)] = s.strip()
    objs = {}
    for line in open(path, encoding='utf-8'):
        f = line.split()
        if f and f[0] == 'object':
            objs[int(f[1])] = [int(v) for v in f[2:12]]

    def show(i, depth):
        o = objs.get(i)
        if not o or depth > 8:
            return
        item, flags, z, heading, y, x, quality, nxt, owner, link = o
        print('%s%4d 0x%03x %-22s flags %3d z %3d head %d x %d y %d quality %2d owner %2d link %4d next %4d' % (
            '  ' * depth, i, item, names.get(item, '')[:22], flags, z, heading, x, y, quality, owner, link, nxt))
        if 0 < link < 1024 and item >= 0x170:
            show(link, depth + 1)
        if depth > 0 and 0 < nxt < 1024 and item >= 0x180:
            show(nxt, depth)

    for a in sys.argv[3:]:
        show(int(a), 0)
        print()


if __name__ == '__main__':
    main()
