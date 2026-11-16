"""proof_look.py: LOOK.C (ovr122: what the player is told on looking at a thing) proved beside the rules core, on the
objects of the nine real levels. The strings come from STRINGS.PAK on both sides (Pair.files, init_strings), the
class tables from OBJECTS.DAT (init_objects). LookAt with every lore (0 nothing known to 3 identified) on objects of
every kind, and SpecialLook, BookLook, KeyLook, BonesLook and RectLook on their own; LookAt reaches do_mods, do_of,
CritterLook and talisman_desc. Every line printed (scroll_print's text), every other outside call, the returns, the
level block and rand's state must be the same.

    python tools/proof_look.py game build/coretest.exe [objects per level]
"""
import os
import random
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coreproof import Differ, Pair  # noqa: E402

ROUTINES = ['LookAt', 'do_mods', 'do_of', 'BookLook', 'RectLook', 'BonesLook', 'KeyLook', 'CritterLook', 'SpecialLook',
            'talisman_desc']


def obj_off(i):
    return 0x4000 + i * 27 if i < 256 else 0x5b00 + (i - 256) * 8


def main():
    game, coretest = sys.argv[1], sys.argv[2]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 150
    p = Pair(game, coretest)
    p.coverage()
    p.files()
    p.same('init_objects', p.call('init_objects', [], 'w'))
    p.same('init_strings', p.call('init_strings', [], 'b'))
    rng = random.Random(122)
    counts = {}
    lines = 0
    for level in range(1, 10):
        block = p.level(level)
        p.w('PlayerLevel', 0, struct.pack('<h', level))
        # the objects in use: on the tiles' lists, or inside something that is
        objs, todo = [], [struct.unpack_from('<H', block, t * 4 + 2)[0] >> 6 for t in range(4096)]
        seen = set()
        while todo:
            i = todo.pop()
            while i and i not in seen and len(seen) < 1100:
                seen.add(i)
                objs.append(i)
                w0, w3 = struct.unpack_from('<H', block, obj_off(i))[0], struct.unpack_from('<H', block, obj_off(i) + 6)[0]
                if not w0 & 0x8000 and w3 >> 6:
                    todo.append(w3 >> 6)
                i = struct.unpack_from('<H', block, obj_off(i) + 4)[0] >> 6
        for k in range(per):
            i = rng.choice(objs)
            ctx = 'level %d object %d' % (level, i)
            p.seed(rng.getrandbits(32))
            fn = rng.choice(['LookAt'] * 5 + ['SpecialLook', 'BookLook', 'KeyLook', 'BonesLook', 'RectLook'])
            arg = rng.randint(0, 3) if fn == 'LookAt' else rng.randint(0, 1)
            try:
                res = p.call(fn, [('f', 'L+%x' % obj_off(i)), ('w', arg)], None)
            except Exception as ex:
                raise Differ('%s(%d) on %s: %s' % (fn, arg, ctx, ex))
            p.same(fn, res, ctx)
            p.same_bytes('L', 0, 0x7c08, ctx)
            p.same('rand', p.gseed(), ctx)
            counts[fn] = counts.get(fn, 0) + 1
            lines += res[0][1].count('scroll_print(')
    print('look            %d looks on 9 levels (%d lines printed)  same (%s)' % (
        sum(counts.values()), lines, ', '.join('%s %d' % kv for kv in sorted(counts.items()))))
    ran, notrun, nosym = p.ran(ROUTINES)
    print('  routines run: %d of %d%s%s' % (len(ran), len(ROUTINES), ('; not run: ' + ', '.join(notrun)) if notrun else '',
          ('; no address: ' + ', '.join(nosym)) if nosym else ''))
    print('all the same')


if __name__ == '__main__':
    try:
        main()
    except Differ as ex:
        print('look DIFFER:', ex)
        sys.exit(1)
