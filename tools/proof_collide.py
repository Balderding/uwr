"""proof_collide.py: COLLIDE.C (seg026, terrain and object collision, placing objects) proved: UW.EXE's routines in
the emulator beside the rules core's, on the nine real levels from LEV.ARK, with random sound movers (footprints on
open tiles, often beside real objects), items from the level's real objects, and placing with a fresh object from
the allocator. After every call the return, the outside calls, rand's state, the whole level block, the collision
record Ppd and oCollisions, the physics records and the other globals the routines touch must be the same.

    python -I tools/proof_collide.py game build/coretest.exe [per level]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from motion_common import Skip, World, run, obj_off  # noqa: E402

ROUTINES = ['GetHgt', 'GetSlopeHgt', 'SolvePnt', 'SolveCenter', 'TerrainCheck', 'ComputeHeading', 'obj_coll_check',
            'ObjectCheck', 'oCswap', 'process_objlist', 'can_place', 'put_at', 'near_mob_put_at', 'drop_around_place']
SCRATCH = 'dontchangedz'  # a byte both sides have, for GetHgt's steep flag


def set_ppd(w, x, y, z, heading, radius, height, index):
    w.p.w('Ppd', 0, struct.pack('<hhhHBBh', x, y, z, heading, radius, height, index))
    w.np('curP', '@Ppd')


def pick_item(w):
    """an item for a footprint: from the level's real objects, or at random"""
    rng = w.rng
    if rng.random() < 0.7:
        objs = w.all_objects()
        if objs:
            i, t = rng.choice(objs)
            return w.obj_word(i, 0) & 0x1ff
    return rng.randint(0, 0x1ff)


def pick_index(w):
    rng = w.rng
    r = rng.random()
    if r < 0.2:
        return 0
    objs = w.all_objects()
    if r < 0.4 or not objs:
        return 1
    return rng.choice(objs)[0]


def new_object(w, mobile):
    """Obj_Alloc on both sides, the object made a sound unlinked one of a random real item; returns its spec or None"""
    p, rng = w.p, w.rng
    res = p.call('Obj_Alloc', [('w', mobile)], 'f')
    p.same('Obj_Alloc', res, w.ctx)
    spec = res[0][0].split()[1]
    if spec == '0':
        return None
    off = int(spec[2:], 16)
    item = pick_item(w)
    if mobile:
        item = rng.choice([item, rng.randint(0x40, 0x7e), rng.randint(0x20, 0x3f)])
        rec = bytearray(27)
        struct.pack_into('<HHHH', rec, 0, item, rng.getrandbits(16) & 0xff80 | rng.randint(0, 0x7f), 0, 0)
        rec[8] = rng.randint(1, 255)
        rec[9] = rng.getrandbits(8)
        rec[0x0a] = rng.getrandbits(8)
        struct.pack_into('<H', rec, 0x0b, rng.getrandbits(16))
        struct.pack_into('<H', rec, 0x0d, rng.getrandbits(16))
        struct.pack_into('<H', rec, 0x0f, rng.getrandbits(16))
        rec[0x13] = rng.getrandbits(8)
        rec[0x14] = rng.getrandbits(8)
        rec[0x15] = rng.getrandbits(8) & 0x7f
        p.w('L', off, bytes(rec))
    else:
        item &= 0x1ff
        q = rng.getrandbits(6)
        # a link word only means something as a quantity or special property (is_quant): a plain object's would be a
        # chain of contents, and a random one a garbage chain
        quant = rng.random() < 0.2
        link = rng.choice([1, 2, 7, 0x200, 0x205]) if quant else 0
        p.w('L', off, struct.pack('<HHHH', item | (0x8000 if quant else 0), rng.getrandbits(16), q, link << 6))
    return spec


def main():
    game, coretest = sys.argv[1], sys.argv[2]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 250
    w = World(game, coretest, 11, 'collide')
    p, rng = w.p, w.rng
    for level in range(1, 10):
        w.level(level)
        w.setup_player()
        for n in range(per):
            try:
                w.refresh()
                p.seed(rng.getrandbits(32))
                kind = rng.choice(['terrain', 'terrain', 'objects', 'objects', 'place', 'place', 'drop', 'put'])
                x, y, z, t = w.pos_near()
                ctx = 'L%d %d %s (%d,%d,%d)' % (level, n, kind, x, y, z)
                w.ctx = ctx
                radius = rng.choice([0, 1, 2, 3, 3, 4, 4])
                height = rng.choice([0, 0, 1, 4, 8, 0x10, 0x18, 0x28, 0x30])
                rng_ = rng.choice([0, 8, 8, radius, rng.randint(0, 20)])
                if kind in ('terrain', 'objects'):
                    heading = rng.getrandbits(16)
                    set_ppd(w, x, y, z, heading, radius, height, pick_index(w))
                    p.w('Ppd', 0x0c, bytes(rng.getrandbits(8) for _ in range(11)))   # stale results, rewritten by the checks
                    r = p.call('TerrainCheck', [('w', rng_)], None)
                    w.check(ctx + ' TerrainCheck', r)
                    w.done('TerrainCheck')
                    r = p.call('ComputeHeading', [], None)
                    w.check(ctx + ' ComputeHeading', r)
                    w.done('ComputeHeading')
                    d = w.ppd()
                    w.cover('slope!=9', d['slope'] != 9)
                    w.cover('open!=9', d['open'] != 9)
                    w.cover('wall bits', bool(d['hits1'] & 0x200))
                    w.cover('drop bits', bool(d['hits1'] & 0x800))
                    w.cover('water/lava', bool(d['hits1'] & 0x30))
                    for k in range(rng.randint(1, 4)):
                        nn = rng.randint(0, 4)
                        p.w(SCRATCH, 0, bytes([rng.getrandbits(8)]))
                        r = p.call('GetHgt', [('w', nn), ('n', '@' + SCRATCH)], 'b')
                        w.check(ctx + ' GetHgt %d' % nn, r)
                        w.done('GetHgt')
                        nn = rng.randint(0, 3)
                        r = p.call('SolvePnt', [('w', nn), ('w', rng_)], 'b')
                        w.check(ctx + ' SolvePnt %d' % nn, r)
                        w.done('SolvePnt')
                    r = p.call('SolveCenter', [('w', rng_)], 'b')
                    w.check(ctx + ' SolveCenter', r)
                    w.done('SolveCenter')
                    r = p.call('GetSlopeHgt', [('w', rng.getrandbits(16)), ('w', rng.getrandbits(16))], 'w')
                    w.check(ctx + ' GetSlopeHgt', r)
                    w.done('GetSlopeHgt')
                if kind == 'objects':
                    r = p.call('ObjectCheck', [('w', rng.randint(0, 1)), ('w', rng.randint(0, 1))], None)
                    w.check(ctx + ' ObjectCheck', r)
                    w.done('ObjectCheck')
                    d = w.ppd()
                    w.cover('found>0', d['found'] > 0)
                    w.cover('found>2', d['found'] > 2)
                    for k in range(rng.randint(0, 2)):
                        objs = w.all_objects()
                        if objs:
                            i, tt = rng.choice(objs)
                            r = p.call('obj_coll_check', [('f', 'L+%x' % obj_off(i)), ('w', i), ('w', rng.randint(-2, 2)),
                                                           ('w', rng.randint(-2, 2)), ('w', rng.randint(0, 1))], None)
                            w.check(ctx + ' obj_coll_check', r)
                            w.done('obj_coll_check')
                    r = p.call('process_objlist', [], None)
                    w.check(ctx + ' process_objlist', r)
                    w.done('process_objlist')
                    d = w.ppd()
                    w.cover('count>0', d['count'] > 0)
                    w.cover('first>0', d['first'] > 0)
                    r = p.call('oCswap', [('w', rng.randint(0, 6))], None)
                    w.check(ctx + ' oCswap', r)
                    w.done('oCswap')
                elif kind == 'place':
                    item = pick_item(w)
                    flier = rng.randint(0, 1)
                    r = p.call('can_place', [('w', item), ('w', pick_index(w)), ('w', x), ('w', y), ('w', z), ('w', flier),
                                              ('w', rng.choice([0, 8, rng.randint(0, 12)]))], 'b')
                    w.check(ctx + ' can_place item %x' % item, r)
                    w.cover('can_place ok', r[0][0] == 'ret 1')
                    w.done('can_place')
                elif kind in ('drop', 'put'):
                    w.w8('stay_centered', rng.randint(0, 1))
                    spec = new_object(w, rng.randint(0, 1))
                    if spec is None:
                        continue
                    rg = rng.choice([0, 1, 2, 4, 6, 8])
                    fn = rng.choice(['drop_around_place', 'put_at', 'near_mob_put_at']) if kind == 'put' else 'drop_around_place'
                    if fn == 'drop_around_place':
                        r = p.call(fn, [('f', spec), ('w', x), ('w', y), ('w', z), ('w', rg)], 'b')
                    elif fn == 'put_at':
                        r = p.call(fn, [('w', x), ('w', y), ('w', z), ('f', spec), ('w', rg), ('w', rng.randint(0, 1))], 'b')
                    else:
                        objs = [o for o in w.all_objects(mobile=True)
                                if 2 <= (w.obj_word(o[0], 0x16) >> 10) < 62 and 2 <= ((w.obj_word(o[0], 0x16) >> 4) & 63) < 62]
                        if not objs:
                            continue
                        src = rng.choice(objs)[0]
                        if True:
                            r = p.call(fn, [('f', 'L+%x' % obj_off(src)), ('f', spec), ('w', rg), ('w', rng.randint(0, 1))], 'b')
                    w.check(ctx + ' ' + fn, r)
                    w.cover(fn + ' ok', r[0][0] == 'ret 1')
                    w.done(fn)
                w.total += 0
            except Skip:
                w.recover()
    w.summary('collide')


if __name__ == '__main__':
    run('collide', main)
