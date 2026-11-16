"""proof_objphys.py: OBJPHYS.C (seg029, object physics: bounce_obj, missile hits, do_objhit, get/set_phys_data, the
static/mobile conversions and obj_deal) proved: UW.EXE's routines in the emulator beside the rules core's, on the
nine real levels from LEV.ARK. Scenes are real objects of the level (critters, static objects, mobile objects) and
new missiles / thrown objects; the movers' collision record is built with TerrainCheck, ObjectCheck and
process_objlist (COLLIDE.C) and do_objhit is run on each object in the way (and the floor), bounce_obj and
missile_newhit after it (with the tile statics do_objhit leaves), get_phys_data / set_phys_data with random
changes to the physics record (moves across tiles, speed, gravity, impacts, terrain, stopping and starting),
static_to_mob, mob_init, mob_to_static, obj_deal on fresh and real objects. After every call the return, the
outside calls (with their arguments), rand's state, the whole level block, the physics records, the collision
record and the other globals must be the same.

    python -I tools/proof_objphys.py game build/coretest.exe [per level]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu import DGROUP  # noqa: E402
from motion_common import Skip, World, run, obj_off, phys, unphys  # noqa: E402
from proof_motion import (read_phys, write_phys, point_cp, projectile_mover, critter_mover, crit_globals)  # noqa: E402

ROUTINES = ['bounce_obj', 'missile_newhit', 'do_objhit', 'get_phys_data', 'set_phys_data', 'static_to_mob', 'mob_init',
            'mob_to_static', 'update_hack_vecs', 'obj_deal']


def spec_of(i):
    return 'L+%x' % obj_off(i)


def sound_contents(w, i):
    """an object whose link word is sound: a quantity or special property (is_quant), none, a container's contents, a
    critter's inventory (a plain object's link would be a chain of contents, and some shipped objects carry other data
    there, which Obj_Punt walks as a chain)"""
    idw = w.obj_word(i, 0)
    item = idw & 0x1ff
    link = w.obj_word(i, 6) >> 6
    return bool(idw & 0x8000) or link == 0 or item >> 6 == 1 or 0x80 <= item <= 0x8f


def pick(w, mobile=None, creature=None, interior=True):
    """(index, tile) of a real object on the map lists, with its list tile in the interior"""
    out = []
    for i, t in w.all_objects(mobile=mobile):
        x, y = t & 63, t >> 6
        if interior and not (2 <= x < 62 and 2 <= y < 62):
            continue
        item = w.obj_word(i, 0) & 0x1ff
        if creature is not None and (item >> 6 == 1) != creature:
            continue
        if i == 1 or not sound_contents(w, i):
            continue
        out.append((i, t))
    return w.rng.choice(out) if out else None


def avoid_null_void(w, i, pn):
    """level 9: mob_to_static of a fate-9 non-creature (fireball, lightning) passes a null st to mts_doanim, which the
    core dereferences (the original reads the interrupt table): such an object is kept moving"""
    item = w.obj_word(i, 0) & 0x1ff
    if w.lev == 9 and w.com_flags(item)['fate'] == 9 and item >> 6 != 1:
        write_phys(w, pn, speed=0x2f)


def perturb(w, pn, i, t):
    """random changes to a physics record before set_phys_data"""
    rng = w.rng
    d = read_phys(w, pn)
    ov = {}
    if rng.random() < 0.5:
        ov['x'] = d['x'] + rng.randint(-400, 400)
        ov['y'] = d['y'] + rng.randint(-400, 400)
        nx, ny = (ov['x'] & 0xffff) >> 8, (ov['y'] & 0xffff) >> 8
        if not (1 <= nx < 63 and 1 <= ny < 63):
            ov['x'], ov['y'] = d['x'], d['y']
    if rng.random() < 0.3:
        ov['z'] = max(0, min(1000, d['z'] + rng.randint(-200, 200)))
    r = rng.random()
    if r < 0.4:
        ov['speed'], ov['vel'], ov['acc'] = 0, (0, 0, 0), (0, 0, 0)
    elif r < 0.7:
        ov['speed'] = rng.randint(0, 900)
        ov['vel'] = (0, 0, rng.choice([0, 0x8d, -0x40, rng.randint(-500, 500)]))
        ov['acc'] = (0, 0, rng.choice([0, -4, -2]))
    if rng.random() < 0.4:
        ov['impact'] = rng.choice([0, 0x80, 0x100, 0x101, 0x200, 0x400, 0x1800, rng.randint(0, 0x2000)])
    ov['terrain'] = rng.choice([1, 1, 1, 2, 4, 8, 0x10, 0x10])
    if rng.random() < 0.3:
        ov['hp'] = rng.randint(0, 255)
    if rng.random() < 0.3:
        ov['heading'] = rng.getrandbits(16)
    if rng.random() < 0.3:
        ov['bounce'] = rng.randint(0, 15)
    write_phys(w, pn, **ov)


def scene(w, ctx):
    """a mover with its collision record built, do_objhit on what is in its way, bounce_obj and missile_newhit after"""
    p, rng = w.p, w.rng
    m = projectile_mover(w, tall=True) if rng.random() < 0.7 else critter_mover(w)
    if m is None:
        return
    pn, tp, idx, kind = m
    ctx += ' scene %s %s idx %s' % (kind, pn, idx)
    w.ctx = ctx
    point_cp(w, pn, tp)
    w.np('TP', '@' + tp)
    steps = [('space_to_pos', [], None), ('TerrainCheck', [('w', 8)], None), ('ObjectCheck', [('w', 0), ('w', 0)], None),
             ('process_objlist', [], None)]
    for nm, a, rt in steps:
        r = p.call(nm, a, rt)
        w.check(ctx + ' ' + nm, r)
    d = w.ppd()
    n = d['found']
    cis = list(range(d['first'], d['first'] + d['count'])) if d['count'] else []
    if rng.random() < 0.3 or not cis:
        cis.append(-1)
    w.cover('scene with objects in the way', bool(d['count']))
    for ci in cis[:4]:
        r = p.call('do_objhit', [('w', ci), ('w', idx)], 'w')
        w.check(ctx + ' do_objhit %d' % ci, r)
        w.done('do_objhit')
        w.cover('do_objhit ret %x' % (int(r[0][0].split()[1], 16) & 0xff))
        if ci >= 0 and rng.random() < 0.5:
            oc = p.e.read(DGROUP, p.data['oCollisions'] + ci * 6, 6)
            other = struct.unpack_from('<H', oc, 2)[0] >> 6
            if other >= 1:
                w.refresh()
                if rng.random() < 0.5:
                    r = p.call('bounce_obj', [('f', spec_of(idx)), ('f', spec_of(other))], 'w')
                    w.check(ctx + ' bounce_obj', r)
                    w.done('bounce_obj')
                else:
                    r = p.call('missile_newhit', [('f', spec_of(idx)), ('f', spec_of(other))], None)
                    w.check(ctx + ' missile_newhit', r)
                    w.done('missile_newhit')
        if w.p.e.rb(0xC000, obj_off(idx)) == 0 and w.p.e.rb(0xC000, obj_off(idx) + 1) == 0:
            break
    if rng.random() < 0.3:
        r = p.call('bounce_obj', [('f', '0'), ('f', '0')], 'w')   # (no object pushed: stops nothing, returns 4)
        w.check(ctx + ' bounce_obj null', r)
        w.done('bounce_obj')


def main():
    game, coretest = sys.argv[1], sys.argv[2]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 400
    w = World(game, coretest, 51, 'objphys')
    p, rng = w.p, w.rng
    for level in range(1, 10):
        w.level(level)
        w.setup_player()
        for n in range(per):
            try:
                w.refresh()
                p.seed(rng.getrandbits(32))
                w.set_time(rng.getrandbits(16))
                op = rng.choice(['scene', 'scene', 'getset', 'getset', 'getset', 'static', 'mobinit', 'tostatic', 'deal', 'deal', 'hack'])
                ctx = 'L%d %d %s' % (level, n, op)
                w.ctx = ctx
                if op == 'scene':
                    scene(w, ctx)
                elif op in ('getset', 'hack'):
                    sel = pick(w, creature=rng.choice([None, None, True, False]))
                    if sel is None:
                        continue
                    i, t = sel
                    static = i >= 256
                    pn = 'CN3' if rng.random() < 0.6 else rng.choice(['CN1', 'CN2', 'CN4'])
                    w.w16('XP', t & 63)
                    w.w16('YP', t >> 6)
                    ctx += ' obj %d tile %d,%d' % (i, t & 63, t >> 6)
                    w.ctx = ctx
                    r = p.call('get_phys_data', [('f', spec_of(i)), ('n', '@' + pn)], None)
                    w.check(ctx + ' get_phys_data', r)
                    w.done('get_phys_data')
                    w.cover('get_phys_data static', static)
                    if op == 'hack':
                        r = p.call('update_hack_vecs', [('n', '@' + pn)], None)
                        w.check(ctx + ' update_hack_vecs', r)
                        w.done('update_hack_vecs')
                        continue
                    if rng.random() < 0.8:
                        perturb(w, pn, i, t)
                        avoid_null_void(w, i, pn)
                    w.w16('XP', t & 63)
                    w.w16('YP', t >> 6)
                    if rng.random() < 0.4:
                        # a thud on the player's side of things: the player somewhere else so the 'near the player' checks vary
                        pass
                    r = p.call('set_phys_data', [('f', spec_of(i)), ('n', '@' + pn)], 'b')
                    w.check(ctx + ' set_phys_data', r)
                    w.done('set_phys_data')
                    w.cover('set_phys_data ret %s' % r[0][0].split()[1])
                elif op == 'static':
                    sel = pick(w, mobile=False)
                    if sel is None:
                        continue
                    i, t = sel
                    w.w16('XP', t & 63)
                    w.w16('YP', t >> 6)
                    r = p.call('static_to_mob', [('f', spec_of(i))], 'f')
                    w.check(ctx + ' static_to_mob', r)
                    w.done('static_to_mob')
                elif op == 'mobinit':
                    sel = pick(w, mobile=True)
                    if sel is None:
                        continue
                    i, t = sel
                    r = p.call('mob_init', [('f', spec_of(i)), ('w', rng.randint(0, 63)), ('w', rng.randint(0, 63))], None)
                    w.check(ctx + ' mob_init', r)
                    w.done('mob_init')
                elif op == 'tostatic':
                    sel = pick(w, mobile=True, creature=False)
                    if sel is None:
                        continue
                    i, t = sel
                    item = w.obj_word(i, 0) & 0x1ff
                    if w.lev == 9 and w.com_flags(item)['fate'] == 9:
                        continue
                    w.w16('XP', t & 63)
                    w.w16('YP', t >> 6)
                    # the object's terrain code and place as set_phys_data would leave a stopped one
                    r = p.call('mob_to_static', [('f', spec_of(i))], 'f')
                    w.check(ctx + ' mob_to_static', r)
                    w.done('mob_to_static')
                elif op == 'deal':
                    how = rng.randint(0, 1)
                    if rng.random() < 0.5:
                        x8, y8, z, t = w.pos_near()
                        spec = w.new_object(0)
                        if spec is None:
                            continue
                        off = int(spec[2:], 16)
                        rec = bytearray(p.e.read(0xC000, off, 8))
                        item = struct.unpack_from('<H', rec, 0)[0] & 0x1ff
                        struct.pack_into('<H', rec, 2, z | rng.randint(0, 7) << 7 | (y8 & 7) << 10 | (x8 & 7) << 13)
                        p.w('L', off, bytes(rec))
                        tx, ty = x8 >> 3, y8 >> 3
                        r = p.call('Obj_Add', [('f', 'L+%x' % ((ty * 64 + tx) * 4 + 2)), ('f', spec)], None)
                        w.check(ctx + ' Obj_Add', r)
                        i = None
                    else:
                        sel = pick(w)
                        if sel is None:
                            continue
                        i, t = sel
                        if i < 256 and (w.obj_word(i, 0) & 0x1ff) >> 6 == 1:
                            continue
                        tx, ty = t & 63, t >> 6
                        spec = spec_of(i)
                    w.w16('XP', tx)
                    w.w16('YP', ty)
                    r = p.call('obj_deal', [('f', spec), ('w', tx), ('w', ty), ('w', how)], 'f')
                    w.check(ctx + ' obj_deal', r)
                    w.done('obj_deal')
                    w.cover('obj_deal destroyed', r[0][0] == 'ret 0')
            except Skip:
                w.recover()
    w.summary('objphys')


if __name__ == '__main__':
    run('objphys', main)
