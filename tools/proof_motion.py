"""proof_motion.py: MOTION.C (seg030, stepping a physics record through the tile grid, bouncing, the collision state)
proved: UW.EXE's routines in the emulator beside the rules core's, on the nine real levels from LEV.ARK. Movers are
the player (PN with PT), real critters of the level (CN1..CN4 with CT1..CT4, through get_phys_data and the
critter handlers' own globals) and new missiles / thrown objects (CN3 with CT3), on open tiles and beside real
objects, with random headings, speeds, times, vertical velocities and gravity. Each mover is run either through
do_physics whole, or step by step by calling the routines one at a time in a random order (space_to_motion, then
grid_move, check_positions, get_pcoll, set_targz, rehead, do_2dbounce, recalc_vecs, flat_move, full_move,
do_zbounce ...). After every call the return, the outside calls, rand's state, the whole level block, the physics
records, MP, Ppd, oCollisions and the handlers' globals must be the same.

    python -I tools/proof_motion.py game build/coretest.exe [per level]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from motion_common import Skip, World, run, obj_off, phys, unphys  # noqa: E402
from emu import DGROUP  # noqa: E402

ROUTINES = ['do_physics', 'space_to_pos', 'set_targz', 'space_to_motion', 'recalc_vecs', 'stop_me', 'back_to_space',
            'flat_move', 'rehead', 'do_2dbounce', 'bounce_that_guy', 'do_zbounce', 'full_move', 'grid_move',
            'set_resterr', 'get_pcoll', 'check_positions', 'IsaDoor', 'CollObject']
# (seg031_2CFA_A3F, the empty static compare, has no entry and nothing calls it)
PAIRS = [('PN', 'PT'), ('CN1', 'CT1'), ('CN2', 'CT2'), ('CN3', 'CT3'), ('CN4', 'CT4')]


def read_phys(w, name):
    return unphys(w.p.e.read(DGROUP, w.p.data[name], 40))


def write_phys(w, name, **kw):
    d = read_phys(w, name)
    d.update(kw)
    w.p.w(name, 0, phys(**d))


def point_cp(w, pn, tp):
    w.np('CP', '@' + pn)
    w.np('TP', '@' + tp)
    w.np('MP', '@%s+6' % pn)       # MP.vel = CP->vel


def mp_get(w, off):
    return struct.unpack('<h', w.p.e.read(DGROUP, w.p.data['MP'] + off, 2))[0]


def player_mover(w):
    rng = w.rng
    if rng.random() < 0.5:
        w.setup_player()
    x8, y8, z, t = w.pos_near(tall=rng.random() < 0.5)
    com_r, com_h = w.com_radius(0x7f), w.com_height(0x7f)
    az = rng.choice([0, 0, 0, 0, -4, -2])
    vz = rng.choice([0, 0, 0, 0, 0x263, 0x8d, -0x8d, -rng.randint(1, 600), rng.randint(1, 600)])
    if az == 0 and vz != 0 and rng.random() < 0.5:
        vz = 0
    write_phys(w, 'PN', x=x8 * 32 + rng.randint(0, 31), y=y8 * 32 + rng.randint(0, 31), z=z * 8 + rng.randint(0, 7),
               vel=(0, 0, vz), acc=(0, 0, az), time=rng.choice([4, 16, 32, 48, 64, rng.randint(1, 64)]),
               speed=rng.choice([0, rng.randint(0, 940), rng.randint(0, 940), 0x3ac // 2]), bounce=5,
               flags=rng.choice([0x80, 0x80, 0]), heading=rng.getrandbits(16), index=1, radius=com_r, height=com_h,
               b24=8, terrain=rng.choice([1, 1, 1, 2, 4, 0x10, 0x10]), impact=0)
    w.p.w('PT', 0, struct.pack('<H', rng.choice([0, 0, 0x1000])))
    return 'PN', 'PT', None, 'player'


def pick_creature(w):
    out = []
    for i, t in w.all_objects(mobile=True):
        item = w.obj_word(i, 0) & 0x1ff
        h = w.obj_word(i, 0x16)
        if item >> 6 == 1 and i != 1 and 2 <= (h >> 10) < 62 and 2 <= ((h >> 4) & 63) < 62:
            out.append(i)
    return w.rng.choice(out) if out else None


def crit_globals(w, idx):
    """what AI.C sets before a critter's physics step, and the handler's randomised bits"""
    rng = w.rng
    spec = 'L+%x' % obj_off(idx)
    w.fp('meptr', spec)
    for nm, v in (('failed', 0), ('control', 1), ('hitwall', 0), ('aligned', 0), ('didhitobj', 0), ('hitadoor', 0),
                  ('dontchangedz', 0)):
        w.w8(nm, v)
    r = w.p.call('get_terrain', [('f', spec)], 'w')
    w.check('get_terrain', r)
    w.w16('crit_terr', int(r[0][0].split()[1], 16))


def critter_mover(w):
    rng = w.rng
    idx = pick_creature(w)
    if idx is None:
        return None
    pn, tp = rng.choice(PAIRS[1:])
    spec = 'L+%x' % obj_off(idx)
    r = w.p.call('get_phys_data', [('f', spec), ('n', '@' + pn)], None)
    w.check('get_phys_data', r)
    ov = {}
    if rng.random() < 0.5 or read_phys(w, pn)['speed'] > 1000:
        ov['speed'] = rng.randint(0, 0x300)
    if rng.random() < 0.4:
        ov['heading'] = rng.getrandbits(16)
    if rng.random() < 0.25:
        ov['vel'] = (0, 0, rng.choice([0x8d, 0x80, -0x40, 0x263]))
        ov['acc'] = (0, 0, rng.choice([0, -4]))
    ov['time'] = rng.choice([16, 16, 32, 48, 64, 96])
    write_phys(w, pn, **ov)
    crit_globals(w, idx)
    if rng.random() < 0.3:
        w.p.w(tp, 0, struct.pack('<HHH', rng.choice([0, 0x1000, 0x10, 0x20, rng.getrandbits(16)]), rng.getrandbits(16), rng.getrandbits(16)))
    return pn, tp, idx, 'crit'


def projectile_mover(w, tall=False):
    rng = w.rng
    x8, y8, z, t = w.pos_near(tall=tall)
    item = rng.choice([rng.randint(0, 0x3f), rng.randint(0x80, 0xbf), rng.randint(0xc0, 0xff), rng.randint(0, 0x3f)])
    while w.lev == 9 and w.com_flags(item)['fate'] == 9:
        # mob_to_static on level 9 keeps nothing (st = 0) and still calls mts_doanim(st, ...) for a fate-9 object (the
        # fireball and the lightning bolt): the original reads the interrupt vector at 0:0 through the null pointer,
        # the core dereferences it and faults. Reported; those items are not thrown on level 9 here
        item = rng.choice([rng.randint(0, 0x3f), rng.randint(0x80, 0xbf), rng.randint(0xc0, 0xff)])
    idx = w.make_mobile(item, x8, y8, z)
    if idx is None:
        return None
    spec = 'L+%x' % obj_off(idx)
    r = w.p.call('get_phys_data', [('f', spec), ('n', '@CN3')], None)
    w.check('get_phys_data', r)
    ov = {'time': rng.choice([16, 32, 48, 64])}
    if rng.random() < 0.5 or read_phys(w, 'CN3')['speed'] > 1000:
        ov['speed'] = rng.randint(0, 1000)
    if rng.random() < 0.5:
        ov['heading'] = rng.getrandbits(16)
    if rng.random() < 0.4:
        ov['vel'] = (0, 0, rng.choice([0x8d, 0x120, -0x40, 0x263, -rng.randint(1, 300)]))
        ov['acc'] = (0, 0, -4)
    write_phys(w, 'CN3', **ov)
    w.fp('meptr', spec)
    w.p.w('CT3', 0, struct.pack('<H', 0x1000 if w.com_flags(item)['no_hit'] else 0))
    if rng.random() < 0.3:
        w.p.w('CT3', 2, struct.pack('<HH', rng.getrandbits(16), rng.getrandbits(16)))
    return 'CN3', 'CT3', idx, 'proj'


def manual(w, pn, tp, ctx):
    """do_physics written out with the routines called one at a time, in a random order after the first step"""
    p, rng = w.p, w.rng
    pt = 0
    r = p.call('space_to_motion', [('w', 1), ('w', 1)], 'b')
    w.check(ctx + ' space_to_motion', r)
    w.done('space_to_motion')
    if r[0][0] == 'ret 0':
        return
    r = p.call('IsaDoor', [('n', '@doorx'), ('n', '@doory')], 'f') if rng.random() < 0.3 else None
    if r:
        w.check(ctx + ' IsaDoor', r)
        w.done('IsaDoor')
    for it in range(80):
        steps, done = mp_get(w, 0x14), mp_get(w, 0x1a)
        if steps + 1 <= done:
            break
        vz = struct.unpack('<h', w.p.e.read(DGROUP, w.p.data[pn] + 6 + 4, 2))[0]
        act = rng.choice(['move'] * 10 + ['pcoll', 'setres', 'rehead', 'bounce2d', 'recalc', 'targz', 'flat', 'back', 'full',
                                           'zbounce', 'stop', 'door', 'coll', 'topos', 'btg', 'grid-'])
        if act == 'move':
            r = p.call('grid_move', [('w', 1)], 'b')
            w.check(ctx + ' grid_move', r)
            w.done('grid_move')
            if r[0][0] != 'ret 0':
                r = p.call('check_positions', [], None)
                w.check(ctx + ' check_positions', r)
                w.done('check_positions')
        elif act == 'pcoll':
            r = p.call('get_pcoll', [], 'w')
            w.check(ctx + ' get_pcoll', r)
            w.done('get_pcoll')
            if rng.random() < 0.5:
                r = p.call('set_resterr', [('w', int(r[0][0].split()[1], 16))], 'b')
                w.check(ctx + ' set_resterr', r)
                w.done('set_resterr')
        elif act == 'setres':
            r = p.call('set_resterr', [('w', rng.choice([rng.getrandbits(16), rng.getrandbits(16) & 0x13ff, 4 | rng.getrandbits(8),
                                                         0x1000, 0x0100]))], 'b')
            w.check(ctx + ' set_resterr', r)
            w.done('set_resterr')
        elif act == 'rehead':
            r = p.call('rehead', [('w', rng.getrandbits(16))], 'b')
            w.check(ctx + ' rehead', r)
            w.done('rehead')
        elif act == 'bounce2d':
            r = p.call('do_2dbounce', [('w', rng.randint(0, 1))], None)
            w.check(ctx + ' do_2dbounce', r)
            w.done('do_2dbounce')
        elif act == 'recalc':
            r = p.call('recalc_vecs', [('w', rng.randint(0, 1))], None)
            w.check(ctx + ' recalc_vecs', r)
            w.done('recalc_vecs')
        elif act == 'targz':
            r = p.call('set_targz', [('w', rng.randint(0, 1))], None)
            w.check(ctx + ' set_targz', r)
            w.done('set_targz')
        elif act == 'flat':
            dr = 1 if done <= 0 else rng.choice([1, -1])
            r = p.call('flat_move', [('w', rng.randint(0, 1)), ('w', dr)], 'b')
            w.check(ctx + ' flat_move', r)
            w.done('flat_move')
        elif act == 'full' and vz != 0:
            dr = 1 if done <= 0 else rng.choice([1, -1])
            r = p.call('full_move', [('w', rng.randint(0, 1)), ('w', dr)], 'b')
            w.check(ctx + ' full_move', r)
            w.done('full_move')
        elif act == 'grid-' and done > 0:
            r = p.call('grid_move', [('w', -1)], 'b')
            w.check(ctx + ' grid_move-', r)
            w.done('grid_move')
        elif act == 'zbounce' and vz != 0:
            r = p.call('do_zbounce', [], None)
            w.check(ctx + ' do_zbounce', r)
            w.done('do_zbounce')
        elif act == 'stop' and rng.random() < 0.3:
            r = p.call('stop_me', [], None)
            w.check(ctx + ' stop_me', r)
            w.done('stop_me')
        elif act == 'door':
            r = p.call('IsaDoor', [('n', '@doorx'), ('n', '@doory')], 'f')
            w.check(ctx + ' IsaDoor', r)
            w.done('IsaDoor')
        elif act == 'coll':
            r = p.call('CollObject', [], 'f')
            w.check(ctx + ' CollObject', r)
            w.done('CollObject')
        elif act == 'topos':
            r = p.call('space_to_pos', [], None)
            w.check(ctx + ' space_to_pos', r)
            w.done('space_to_pos')
        elif act == 'btg':
            r = p.call('bounce_that_guy', [], None)
            w.check(ctx + ' bounce_that_guy', r)
            w.done('bounce_that_guy')
    r = p.call('back_to_space', [], None)
    w.check(ctx + ' back_to_space', r)
    w.done('back_to_space')


def main():
    game, coretest = sys.argv[1], sys.argv[2]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 300
    w = World(game, coretest, 31, 'motion')
    p, rng = w.p, w.rng
    for level in range(1, 10):
        w.level(level)
        w.setup_player()
        for n in range(per):
            try:
                w.refresh()
                p.seed(rng.getrandbits(32))
                w.set_time(rng.getrandbits(16))
                kind = rng.choice(['player', 'crit', 'crit', 'proj', 'proj'])
                w.ctx = ctx = 'L%d %d %s' % (level, n, kind)
                m = {'player': player_mover, 'crit': critter_mover, 'proj': projectile_mover}[kind](w)
                if m is None:
                    continue
                pn, tp, idx, kind = m
                ctx = 'L%d %d %s %s idx %s' % (level, n, kind, pn, idx)
                w.ctx = ctx
                point_cp(w, pn, tp)
                w.np('CP', '@' + pn)
                ds = read_phys(w, pn)
                ctx += ' x=%d y=%d z=%d sp=%d hd=%x vz=%d az=%d t=%d' % (ds['x'], ds['y'], ds['z'], ds['speed'], ds['heading'] & 0xffff, ds['vel'][2], ds['acc'][2], ds['time'])
                w.ctx = ctx
                if rng.random() < 0.5:
                    r = p.call('do_physics', [('n', '@' + pn), ('n', '@' + tp)], None)
                    w.check(ctx + ' do_physics', r)
                    w.done('do_physics')
                else:
                    w.np('TP', '@' + tp)
                    manual(w, pn, tp, ctx)
                if kind != 'player' and idx is not None and rng.random() < 0.5:
                    # the object takes the result back (OBJPHYS.C's set_phys_data, as AI.C and move_me_joe do)
                    h = w.p.e.read(0xC000, obj_off(idx) + 0x16, 2)
                    hh = struct.unpack('<H', h)[0]
                    w.w16('XP', hh >> 10)
                    w.w16('YP', (hh >> 4) & 63)
                    r = p.call('set_phys_data', [('f', 'L+%x' % obj_off(idx)), ('n', '@' + pn)], 'b')
                    w.check(ctx + ' set_phys_data', r)
                    w.done('set_phys_data')
            except Skip:
                w.recover()
    w.summary('motion')


if __name__ == '__main__':
    run('motion', main)
