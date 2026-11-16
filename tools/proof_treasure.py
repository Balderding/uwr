"""proof_treasure.py: TREASURE.C (ovr150: a critter's loot, spilling containers) and COMBINE.C (ovr099: combining
objects by CMB.DAT's rules) proved beside the rules core, on the nine real levels. init_objects and init_combinables
load OBJECTS.DAT, COMOBJ.DAT and CMB.DAT on both sides (Pair.files). Then: every critter of the level gets its loot
(generate_inventory, and so generate_treasure, generate_food, generate_weapons, generate_equipment, with CreateObj and
the object store under them), dead or alive spills its inventory (drop_some_objects, drop_link_chain, with put_at and
drop_around_place), containers spill too; and pairs of objects are tried against the combining rules
(ObjsBeCombinable, CombineObjs, RemoveAfterCombine). After each call the whole level block, the returns, rand's state
and the outside calls must be the same.

    python tools/proof_treasure.py game build/coretest.exe [rounds per level]
"""
import os
import random
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coreproof import Differ, Pair  # noqa: E402

ROUTINES = ['drop_link_chain', 'drop_some_objects', 'generate_treasure', 'generate_food', 'generate_weapons',
            'generate_equipment', 'generate_inventory', 'init_combinables', 'ObjsBeCombinable', 'CombineObjs',
            'RemoveAfterCombine', 'make_stew']


def obj_off(i):
    return 0x4000 + i * 27 if i < 256 else 0x5b00 + (i - 256) * 8


def tile_objects(block):
    """(tile, index) of every object on the tiles' lists, top level"""
    out = []
    for t in range(4096):
        i = struct.unpack_from('<H', block, t * 4 + 2)[0] >> 6
        guard = 0
        while i and guard < 1100:
            out.append((t, i))
            i = struct.unpack_from('<H', block, obj_off(i) + 4)[0] >> 6
            guard += 1
    return out


def main():
    game, coretest = sys.argv[1], sys.argv[2]
    rounds = int(sys.argv[3]) if len(sys.argv) > 3 else 40
    p = Pair(game, coretest)
    p.coverage()
    p.files()
    p.same('init_objects', p.call('init_objects', [], 'w'))
    p.same('init_combinables', p.call('init_combinables', [], None))
    cmb = open(os.path.join(game, 'DATA', 'CMB.DAT'), 'rb').read()
    rules = [struct.unpack_from('<3H', cmb, k * 6) for k in range(len(cmb) // 6)]
    rng = random.Random(150)
    counts = {}

    def check(fn, res, ctx):
        p.same(fn, res, ctx)
        p.same_bytes('L', 0, 0x7c08, ctx)
        p.same('rand', p.gseed(), ctx)
        counts[fn] = counts.get(fn, 0) + 1

    for level in range(1, 10):
        p.level(level)
        p.w('PlayerLevel', 0, struct.pack('<h', level))
        for k in range(rounds):
            block = p.e.read(0xC000, 0, 0x7c08)
            objs = tile_objects(block)
            crits = [(t, i) for t, i in objs if 2 <= i < 256 and (struct.unpack_from('<H', block, obj_off(i))[0] >> 6 & 7) == 1]
            p.seed(rng.getrandbits(32))
            ctx = 'level %d round %d' % (level, k)
            op = rng.choice(['loot', 'loot', 'spill', 'container', 'combine', 'combine', 'stew'])
            if op == 'loot' and crits:
                t, i = rng.choice(crits)
                if rng.random() < 0.7:  # not yet looted
                    w = struct.unpack_from('<H', block, obj_off(i) + 0x0d)[0] & ~0x1000
                    p.w('L', obj_off(i) + 0x0d, struct.pack('<H', w))
                check('generate_inventory', p.call('generate_inventory', [('f', 'L+%x' % obj_off(i))], None), ctx)
            elif op == 'spill' and crits:
                t, i = rng.choice(crits)
                check('drop_some_objects', p.call('drop_some_objects', [('f', 'L+%x' % obj_off(i))], None), ctx)
            elif op == 'container' and objs:
                full = [(t, i) for t, i in objs if not struct.unpack_from('<H', block, obj_off(i))[0] & 0x8000
                        and struct.unpack_from('<H', block, obj_off(i) + 6)[0] >> 6]
                if full:
                    t, i = rng.choice(full)
                    p.w('MapObj_X', 0, struct.pack('<h', t % 64))
                    p.w('MapObj_Y', 0, struct.pack('<h', t // 64))
                    check('drop_link_chain', p.call('drop_link_chain', [('f', 'L+%x' % obj_off(i)), ('w', rng.randint(0, 31))], 'b'), ctx)
            elif op == 'stew':
                # a bowl in the inventory (or none) holding some of the recipe's ingredients (0xD9, 0xB8, 0xBE) and
                # perhaps something else: fresh static objects from the store
                p.w('Inventory', 0, b'\0' * 56)
                made = []
                for _ in range(rng.randint(1, 5)):
                    res = p.call('Obj_Alloc', [('w', 0)], 'f')
                    check('Obj_Alloc', res, ctx)
                    if res[0][0] != 'ret 0':
                        made.append(int(res[0][0].split()[1][2:], 16))
                if made:
                    bowl, rest = made[0], made[1:]
                    items = [rng.choice([0xd9, 0xb8, 0xbe, 0xd9, 0xb8, 0xbe, rng.randint(0, 0x1ff)]) for _ in rest]
                    for k, off in enumerate(rest):
                        nxt = ((rest[k + 1] - 0x5b00) // 8 + 256) if k + 1 < len(rest) else 0
                        p.w('L', off, struct.pack('<HHHH', items[k], 0, nxt << 6, 0))
                    first = ((rest[0] - 0x5b00) // 8 + 256) if rest else 0
                    p.w('L', bowl, struct.pack('<HHHH', rng.choice([0x8e, 0x8e, 0x8e, 0x8f]), 0, 0, first << 6))
                    if rng.random() < 0.85:
                        slot = rng.randint(0, 27)
                        p.w('Inventory', slot * 2, struct.pack('<H', ((bowl - 0x5b00) // 8 + 256) << 6))
                check('make_stew', p.call('make_stew', [], 'b'), ctx)
                p.same_bytes('Inventory', 0, 56, ctx)
                p.w('Inventory', 0, b'\0' * 56)
            elif op == 'combine' and len(objs) > 1:
                (_, a), (_, b) = rng.sample(objs, 2)
                if rng.random() < 0.6 and rules:  # make the pair one of the rules
                    r = rng.choice(rules)
                    for o, item in ((a, r[0] & 0x1ff), (b, r[1] & 0x1ff)):
                        w = struct.unpack_from('<H', p.e.read(0xC000, obj_off(o), 2))[0]
                        p.w('L', obj_off(o), struct.pack('<H', (w & 0xfe00) | item))
                res = p.call('ObjsBeCombinable', [('f', 'L+%x' % obj_off(a)), ('f', 'L+%x' % obj_off(b))], 'w')
                check('ObjsBeCombinable', res, ctx)
                combo = int(res[0][0].split()[1], 16)
                if combo != 0xffff:
                    check('RemoveAfterCombine', p.call('RemoveAfterCombine', [('f', 'L+%x' % obj_off(a)), ('w', combo)], 'b'), ctx)
                    res = p.call('CombineObjs', [('w', combo)], 'f')
                    check('CombineObjs', res, ctx)
                    spec = res[0][0].split()[1]
                    if spec != '0':  # put the new object somewhere, as the inventory would
                        t = rng.randint(0, 4095)
                        check('Obj_Add', p.call('Obj_Add', [('f', 'L+%x' % (t * 4 + 2)), ('f', spec)], None), ctx)
    print('treasure        %d operations on 9 levels  same (%s)' % (sum(counts.values()), ', '.join('%s %d' % kv for kv in sorted(counts.items()))))
    ran, notrun, nosym = p.ran(ROUTINES)
    print('  routines run: %d of %d%s%s' % (len(ran), len(ROUTINES), ('; not run: ' + ', '.join(notrun)) if notrun else '',
          ('; no address: ' + ', '.join(nosym)) if nosym else ''))
    print('all the same')


if __name__ == '__main__':
    try:
        main()
    except Differ as ex:
        print('treasure DIFFER:', ex)
        sys.exit(1)
