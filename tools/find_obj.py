"""find_obj.py: where objects of the given kinds lie in a level (data/levels/levelN.txt): tile, position, fields.

    python -I tools/find_obj.py data/levels/level1.txt 358 221 128
"""
import sys


def main():
    path, kinds = sys.argv[1], {int(k, 0) for k in sys.argv[2:]}
    tiles, objs = {}, {}
    for line in open(path, encoding='utf-8'):
        f = line.split()
        if not f:
            continue
        if f[0] == 'tile':
            # tile x y type height floor wall door nomagic unknown8 first_object
            tiles[(int(f[1]), int(f[2]))] = int(f[10])
        elif f[0] == 'object':
            # object index item flags z heading y x quality next owner link
            objs[int(f[1])] = [int(v) for v in f[2:12]]
    for (x, y), first in sorted(tiles.items()):
        i, guard = first, 0
        while 0 < i < 1024 and guard < 1024:
            o = objs.get(i)
            if not o:
                break
            item, flags, z, heading, oy, ox, quality, nxt, owner, link = o
            if item in kinds:
                print('obj %4d item %3d (0x%03x) tile %2d,%2d  pos %.2f,%.2f z %d flags %d quality %d owner %d link %d' % (
                    i, item, item, x, y, x + (ox + 0.5) / 8, y + (oy + 0.5) / 8, z, flags, quality, owner, link))
            i, guard = nxt, guard + 1


if __name__ == '__main__':
    main()
