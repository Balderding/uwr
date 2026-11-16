"""proof_creature.py: CREATURE.C (ovr101, the critter class data and a new critter's starting state) proved: UW.EXE's
routines in the emulator beside the rules core's, on the nine real levels from LEV.ARK, for real creatures of the level
(creature_class_data, creature_obj_init on a creature's object scrambled first) and creature_init / creature_save (their
file calls are outside calls). After every call the return, the outside calls, Borland rand's state, the whole level
block, the Creature table and the lookup globals must be the same.

    python -I tools/proof_creature.py game build/coretest.exe [per level]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coreproof import Differ  # noqa: E402
from cproof_lib import World, main_wrap  # noqa: E402

ROUTINES = ['creature_class_data', 'creature_obj_init']  # creature_init and creature_save only fread / fwrite a file


def main():
    game, coretest = sys.argv[1], sys.argv[2]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 200
    w = World(game, coretest, 5)
    p = w.p
    counts, total = {}, 0
    for level in range(1, 10):
        w.level(level)
        crits = w.mobiles()
        for n in range(per):
            op = w.rng.choice(['class', 'class', 'init', 'init', 'init'])
            ctx = 'level %d op %d %s' % (level, n, op)
            p.seed(w.rng.getrandbits(32))
            ci = w.rng.choice(crits)
            if w.rng.random() < 0.3:
                w.scramble_critter(ci)
            if op == 'file':
                fn = w.rng.choice(['creature_init', 'creature_save_ovr101_17'])
                res = p.call(fn, [('n', '0')], None)
            else:
                p.p('ActiveObj', 0, 'L+%x' % w.obj_off(ci))
                if op == 'class':
                    res = p.call('creature_class_data', [], 'n')
                else:
                    res = p.call('creature_obj_init', [], 'b')
            p.same(op, res, ctx)
            w.compare(ctx, extra=['cr_class', 'cr_type', ('Creature', 3072)])
            p.same('cst', w.q('cst', 0, True), ctx)
            w.rand_state(ctx)
            counts[op] = counts.get(op, 0) + 1
            total += 1
    print('creature        %d operations on 9 levels  same (%s)' % (total, ', '.join('%s %d' % kv for kv in sorted(counts.items()))))
    print('all the same')


if __name__ == '__main__':
    main_wrap('creature', main)
