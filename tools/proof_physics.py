"""proof_physics.py: PHYSICS.C (seg008, the player's physics: terrain and movement state, the step-and-turn moves, the
per-tick setup from the input, applying the result to the player object, placing the player) proved: UW.EXE's
routines in the emulator beside the rules core's, on the nine real levels from LEV.ARK. The player is placed with
player_setup on random open tiles (the proof of that routine too) and its record, physics record and motion state
are varied at random (motion spells, terrain bits, speeds, weights, input commands, impacts, tile changes); every
routine is called directly. After every call the return, the outside calls (with their arguments), rand's state, the
whole level block, PN, PT, the player record and the other globals must be the same.

    python -I tools/proof_physics.py game build/coretest.exe [per level]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu import DGROUP  # noqa: E402
from motion_common import Skip, World, run, obj_off, phys, unphys  # noqa: E402
from proof_motion import read_phys, write_phys  # noqa: E402

ROUTINES = ['water_set', 'parse_player_terr', 'EtherealVoidSpecialEffects_seg008_150', 'simple_fizix',
            'set_player_phys_params', 'player_setup', 'phys_affect_player', 'player_sqhandler', 'do_player_input',
            'phys_bounce_up', 'fizix_update', 'BouncePlayer_seg008_D9E', 'DoTrapScreenShake_seg008_DD6',
            'QuakeTrap_seg008_DE7', 'newFPS']
TERRS = [1, 1, 1, 2, 4, 8, 0x10, 0x10, 0x20, 0x22, 0x21, 0x12, 0x14, 0x30, 0x03, 0x0f, 0]


def vary_player(w):
    """the player record and the motion state: random but plausible"""
    rng, p = w.rng, w.p
    w.p.w('PlayerDat', 0x4a, struct.pack('<HH', rng.randint(0, 900), rng.choice([0, 0, rng.randint(150, 1400)])))
    w.p.w('PlayerDat', 0x21 + 17, bytes([rng.randint(0, 30)]))                    # acrobat
    w.p.w('PlayerDat', 0xb8, bytes([rng.choice([0, 0, 1, 2, 4, 8, 0x20, 0x40, 0x61, rng.getrandbits(8)])]))
    w.p.w('PlayerDat', 0xb9, bytes([rng.choice([0, 0x10, 0x60, rng.getrandbits(8)])]))
    fps = rng.randint(0, 6)
    cur = struct.unpack('<H', p.e.read(DGROUP, p.data['PlayerDat'] + 0xb6, 2))[0]
    w.p.w('PlayerDat', 0xb6, struct.pack('<H', (cur & ~7) | fps))
    w.p.w('PlayerDat', 0x5f, struct.pack('<H', rng.getrandbits(16)))
    w.w8('motionbits', rng.choice([0, 0, 0, 1, 2, 4, 8, 0x10, 0x11, 0x12, 0x14, rng.getrandbits(5)]))
    w.w16('lastTerr', rng.choice(TERRS))
    w.w16('PlayerFacing', rng.getrandbits(16))
    w.w16('PlayerHeading', rng.getrandbits(16))
    w.w16('PlayerInput', rng.choice([0, 1, 1, 6, 7, 8, 9, 10, 12, 13, rng.randint(0, 20)]))
    w.w16('PlayerTurn', 15)                                 # (a constant of the game)
    w.w16('ForwInpRate', rng.choice([0, rng.randint(0, 0x73), 0x70, 0x32]))   # (the mouse's and keys' ranges: the products are 16-bit)
    w.w16('TurnInpRate', rng.choice([0, rng.randint(-0x80, 0x80), 0x5a, -0x5a]))
    w.w16('plyMoType', rng.randint(-2, 1))
    w.w8('fiz_update', rng.randint(0, 1))
    w.w16('MaxPlayerAccel', rng.choice([0x60, rng.randint(1, 0x100)]))
    w.w16('pFPS', rng.randint(1, 0x3ac), 0)
    w.w16('pFPS', rng.randint(1, 0x300), 2)
    w.w16('pFPS', rng.randint(1, 0x200), 4)
    w.w16('Run_FPS', rng.choice([0x3ac, rng.randint(20, 0x400)]))
    w.w16('Side_FPS', rng.choice([0xeb, rng.randint(20, 0x300)]))
    w.w16('Back_FPS', rng.choice([0xbc, rng.randint(20, 0x300)]))
    w.w8('plyNotice', rng.randint(1, 15))
    w.w8('plyNotice', rng.randint(1, 15), 1)


def vary_pn(w, move_tile=False):
    """PN near where the player stands (its tile is GrSq), with random motion; move_tile puts it in a neighbouring tile"""
    rng = w.rng
    d = read_phys(w, 'PN')
    ov = {}
    if rng.random() < 0.8:
        ox, oy = rng.randint(-120, 120), rng.randint(-120, 120)
        if move_tile:
            ox, oy = rng.choice([(-200, 0), (200, 0), (0, -200), (0, 200), (150, 150), (-150, -150)])
        nx, ny = (d['x'] + ox) & 0xffff, (d['y'] + oy) & 0xffff
        if 1 <= (nx >> 8) < 63 and 1 <= (ny >> 8) < 63:
            ov['x'], ov['y'] = nx, ny
    if rng.random() < 0.5:
        ov['z'] = max(0, min(1015, d['z'] + rng.choice([0, 8, -8, 64, rng.randint(-100, 100)])))
    ov['speed'] = rng.choice([0, 0, rng.randint(0, 940), rng.randint(0, 100)])
    ov['vel'] = (rng.randint(-400, 400), rng.randint(-400, 400), rng.choice([0, 0, 0, 0x263, 0x8d, -0x8d, -94, -200, rng.randint(-700, 700)]))
    ov['acc'] = (0, 0, rng.choice([0, 0, -4, -2]))
    ov['impact'] = rng.choice([0, 0, 0, 0x100, 0x180, 0x300, 0x500, 0x1000, rng.randint(0, 0x2000)])
    ov['bounce'] = rng.choice([0, 5, 5, 5, rng.randint(0, 15)])
    ov['flags'] = rng.choice([0, 0x80])
    ov['heading'] = rng.choice([rng.getrandbits(16), struct.unpack('<H', w.p.e.read(DGROUP, w.p.data['PlayerHeading'], 2))[0]])
    ov['terrain'] = rng.choice(TERRS)
    write_phys(w, 'PN', **ov)


def main():
    game, coretest = sys.argv[1], sys.argv[2]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 600
    w = World(game, coretest, 61, 'physics')
    p, rng = w.p, w.rng
    OPS = (['setup'] * 3 + ['fizix'] * 6 + ['params'] * 4 + ['affect'] * 6 + ['terr'] * 5 + ['newfps'] * 2 + ['input'] * 4
           + ['sqh'] * 2 + ['bounce_up'] + ['update'] + ['quake'] * 2 + ['void'] + ['water'])
    for level in range(1, 10):
        w.level(level)
        w.setup_player()
        for n in range(per):
            try:
                w.refresh()
                p.seed(rng.getrandbits(32))
                w.set_time(rng.getrandbits(16))
                op = rng.choice(OPS)
                ctx = 'L%d %d %s' % (level, n, op)
                w.ctx = ctx
                if op != 'setup':
                    vary_player(w)
                    vary_pn(w, move_tile=(op == 'affect' and rng.random() < 0.4))
                if op == 'setup':
                    if rng.random() < 0.5:
                        t = w.setup_player()
                    else:
                        t = rng.choice(w.open_tiles(1))
                        r = p.call('player_setup', [('w', t & 63), ('w', t >> 6)], None)
                        w.check(ctx + ' player_setup', r)
                    w.done('player_setup')
                elif op == 'fizix':
                    turn = rng.choice([-2, -1, 0, 0, 1, 2])
                    if rng.random() < 0.7:
                        write_phys(w, 'PN', acc=(0, 0, 0), speed=rng.randint(0, 0x60))
                    r = p.call('simple_fizix', [('w', turn)], 'b')
                    w.check(ctx + ' simple_fizix %d' % turn, r)
                    w.done('simple_fizix')
                    w.cover('simple_fizix moved', r[0][0] != 'ret 0')
                elif op == 'params':
                    r = p.call('set_player_phys_params', [('w', rng.choice([4, 16, 32, 64, rng.randint(1, 64)]))], None)
                    w.check(ctx + ' set_player_phys_params', r)
                    w.done('set_player_phys_params')
                elif op == 'affect':
                    r = p.call('phys_affect_player', [], None)
                    w.check(ctx + ' phys_affect_player', r)
                    w.done('phys_affect_player')
                elif op == 'terr':
                    terr = rng.choice(TERRS)
                    if rng.random() < 0.5:
                        r = p.call('parse_player_terr', [('w', terr), ('w', rng.randint(0, 1))], None)
                        w.check(ctx + ' parse_player_terr', r)
                        w.done('parse_player_terr')
                    else:
                        r = p.call('water_set', [('w', terr)], 'b')
                        w.check(ctx + ' water_set', r)
                        w.done('water_set')
                elif op == 'newfps':
                    r = p.call('newFPS', [('w', rng.randint(-1, 6))], None)
                    w.check(ctx + ' newFPS', r)
                    w.done('newFPS')
                elif op == 'input':
                    inp = rng.choice([0, 1, 6, 7, 8, 9, 10, 12, 13, rng.randint(-2, 20)])
                    w.w16('MapObj_Y', rng.randint(-500, 500))
                    r = p.call('do_player_input', [('w', inp), ('w', rng.choice([4, 16, 32, 64])), ('n', '@MapObj_Y')], None)
                    w.check(ctx + ' do_player_input %d' % inp, r)
                    w.done('do_player_input')
                elif op == 'sqh':
                    w.w16('MapObj_X', rng.choice([0x1000, 0x1100, 0x1800, rng.getrandbits(16)]))
                    r = p.call('player_sqhandler', [('n', '@MapObj_X')], 'b')
                    w.check(ctx + ' player_sqhandler', r)
                    w.done('player_sqhandler')
                elif op == 'bounce_up':
                    o = 'L+401b' if rng.random() < 0.7 else 'L+%x' % obj_off(rng.randint(2, 255))
                    r = p.call('phys_bounce_up', [('f', o)], None)
                    w.check(ctx + ' phys_bounce_up', r)
                    w.done('phys_bounce_up')
                elif op == 'update':
                    r = p.call('fizix_update', [], None)
                    w.check(ctx + ' fizix_update', r)
                    w.done('fizix_update')
                elif op == 'quake':
                    k = rng.choice(['BouncePlayer_seg008_D9E', 'DoTrapScreenShake_seg008_DD6', 'QuakeTrap_seg008_DE7'])
                    if k == 'QuakeTrap_seg008_DE7':
                        r = p.call(k, [('w', rng.randint(0, 3)), ('w', rng.randint(-20, 60))], None)
                    else:
                        r = p.call(k, [('w', rng.randint(-20, 60))], None)
                    w.check(ctx + ' ' + k, r)
                    w.done(k)
                elif op == 'void':
                    r = p.call('EtherealVoidSpecialEffects_seg008_150', [], None)
                    w.check(ctx + ' EtherealVoidSpecialEffects', r)
                    w.done('EtherealVoidSpecialEffects_seg008_150')
                elif op == 'water':
                    r = p.call('water_set', [('w', rng.choice(TERRS))], 'b')
                    w.check(ctx + ' water_set', r)
                    w.done('water_set')
            except Skip:
                w.recover()
    w.summary('physics')


if __name__ == '__main__':
    run('physics', main)
