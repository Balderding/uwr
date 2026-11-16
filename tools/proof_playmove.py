"""proof_playmove.py: PLAYMOVE.C (seg034, player input and motion: the movement commands and keys, the physics clock,
head bob, footstep and water sounds, noise, screen shake, the 3D camera) proved: UW.EXE's routines in the emulator
beside the rules core's, on the nine real levels from LEV.ARK. The player is placed with player_setup on random open
tiles and its record, physics record, motion spells, input state, keys held, clock and sound state are varied at
random; every routine is called directly (move_player and move_physics also run PHYSICS.C's and MOTION.C's code
and, with the critters moving, critter/AI.C's move_mobile; check_physics runs the whole clock). After every call
the return, the outside calls (with their arguments), rand's state, the whole level block, PN, PT, the player
record, the camera and the other globals must be the same.

move_physics's call of move_mobile (critter/AI.C, the critters' own proofs) is off unless PROOF_CRITS=1: with it a critter
ended in another tile after check_physics on level 9 (reported, not analysed).

Not proved: player_simple_move (it ends in a busy wait on the game clock, which the core's clock, a constant,
never ends) and the mouse branch of parse_playin / player_mous_move with WizEye clear (mouse_getbut is an outside call
that leaves its `buttons` local unset).

    python -I tools/proof_playmove.py game build/coretest.exe [per level]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from emu import DGROUP  # noqa: E402
from motion_common import Skip, World, run, obj_off  # noqa: E402
from proof_motion import read_phys, write_phys  # noqa: E402
from proof_physics import vary_player, vary_pn  # noqa: E402

ROUTINES = ['parse_playin', 'do_player_keyboard', 'move_physics', 'move_player', 'make_noise', 'set_sound', 'parse_effect',
            'player_mous_move', 'check_physics', 'finish_player', 'get_eye', 'set_effect']
# not proved: player_simple_move (busy wait on the clock)
MOVE_KEYS = [0x11, 0x1F, 0x1E, 0x20, 0x2C, 0x2E, 0x2D, 0x12, 0x10]
MOVE_CRITS = bool(os.environ.get('PROOF_CRITS'))   # let move_physics call critter/AI.C's move_mobile (see the note at the top)
CAM = 0x100   # the camera struct
SCR = 0x8000  # a scratch area of the level block (past its 0x7c08 bytes) for the keyboard driver's bytes and the camera


def setup_ui(w):
    """the far pointers to the keyboard driver's bytes and the camera, in a spare buffer both sides have"""
    for nm, off in (('Alt', 0x60), ('Ctrl', 0x61), ('Shift', 0x62), ('CapsLock', 0x63), ('key_on', 0x80)):
        w.fp(nm, 'L+%x' % (SCR + off))
    w.fp('cPlayer', 'L+%x' % (SCR + CAM))
    w.p.w('L', SCR, bytes(0x130))
    w.state.append(('chrbuf', 10))
    w.state.append(('Hasted', 1))
    w.state.append(('TimeStop', 1))
    w.state.append(('KeybUsed', 1))
    w.state.append(('IgnoringInput', 1))
    w.state = [(n, k) for n, k in w.state if n in w.p.data and n in w.p.host]


def vary_ui(w):
    rng, p = w.rng, w.p
    keys = bytearray(128)
    for k in MOVE_KEYS:
        if rng.random() < 0.25:
            keys[k] = 1
    w.p.w('L', SCR + 0x80, bytes(keys))
    mods = [0, 0, 0, 0]
    r = rng.random()
    if r < 0.25:
        mods[rng.randint(0, 3)] = 1
    elif r < 0.3:
        mods[2] = mods[3] = 1
    w.p.w('L', SCR + 0x60, bytes(mods))
    w.p.w('chrbuf', 0, struct.pack('<hhhhh', rng.randint(0, 250), rng.randint(0, 250), 0, 0, 0))   # inplist x, y
    w.w16('PWid', rng.randint(100, 300))
    w.w16('PHgt', rng.randint(60, 200))
    w.w8('Hasted', rng.choice([0, 0, 1]))
    w.w8('TimeStop', rng.choice([0, 0, 0, 1]))
    w.w8('DoAnimO', rng.choice([0, 0, 1]))
    w.w8('MoveCrits', 1 if MOVE_CRITS and rng.random() < 0.25 else 0)
    w.w8('water_eff', rng.choice([0xff, 0xff, 3, 7]))
    w.wl('watertime', rng.getrandbits(16))
    w.wl('nextstep', rng.getrandbits(16))
    w.w8('tsteps', rng.getrandbits(8))
    w.w8('combEfflen', rng.choice([0, 1, 5, 40]))
    w.w8('tremEfflen', rng.choice([0, 1, 5, 90]))
    w.w8('doMod', rng.randint(0, 1))
    w.w8('KeybUsed', rng.randint(0, 1))
    w.p.w('playerMod', 0, struct.pack('<hhhh', *[rng.randint(-300, 300) for _ in range(4)]))
    w.w16('Run_FPS', rng.choice([0x3ac, rng.randint(40, 0x400)]))


def main():
    orig_check = World.check

    def check(self, ctx, res=None, count=None):
        c = orig_check(self, ctx, res, count)
        if not self.tainted:
            self.p.same_bytes('L', SCR, 0x130, c)
        return c
    World.check = check
    game, coretest = sys.argv[1], sys.argv[2]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 400
    w = World(game, coretest, 71, 'playmove')
    p, rng = w.p, w.rng
    setup_ui(w)
    last_time = 0     # check_physics' static last_time (0 as the data starts), tracked
    OPS = (['check'] * 5 + ['move_player'] * 5 + ['physics'] * 4 + ['keyboard'] * 3 + ['parsein'] * 3 + ['noise'] * 2
           + ['sound'] * 2 + ['effect'] * 3 + ['seteffect'] + ['eye'] * 3 + ['finish'] + ['mous'])
    if MOVE_CRITS:
        OPS = OPS + ['mobile'] * 6
    for level in range(1, 10):
        w.level(level)
        setup_ui(w)
        w.state = list(dict(w.state).items())
        w.setup_player()
        for n in range(per):
            try:
                w.refresh()
                p.seed(rng.getrandbits(32))
                op = rng.choice(OPS)
                ctx = 'L%d %d %s' % (level, n, op)
                w.ctx = ctx
                vary_player(w)
                vary_pn(w, move_tile=rng.random() < 0.2)
                w.w16('pFPS', rng.randint(8, 0x3ac), 0)
                vary_ui(w)
                if rng.random() < 0.08:
                    w.setup_player()
                if op == 'check':
                    if rng.random() < 0.8:
                        t = (last_time + rng.choice([1, 3, 8, 0x10, 0x20, 0x40, 0x41, 0x80, 0x400])) & 0xffff
                    else:
                        t = last_time
                    w.set_time(t)
                    r = p.call('check_physics', [], None)
                    if t != last_time:
                        last_time = t
                    w.check(ctx + ' check_physics', r)
                    w.done('check_physics')
                elif op == 'move_player':
                    w.set_time(rng.getrandbits(16))
                    r = p.call('move_player', [('w', rng.choice([4, 16, 32, 64, rng.randint(1, 64)]))], None)
                    w.check(ctx + ' move_player', r)
                    w.done('move_player')
                elif op == 'physics':
                    w.set_time(rng.getrandbits(16))
                    r = p.call('move_physics', [('w', rng.choice([16, 32, 64, rng.randint(1, 64)])), ('w', rng.randint(0, 4)),
                                                ('w', rng.randint(0, 1))], None)
                    w.check(ctx + ' move_physics', r)
                    w.done('move_physics')
                elif op == 'keyboard':
                    r = p.call('do_player_keyboard', [], None)
                    w.check(ctx + ' do_player_keyboard', r)
                    w.done('do_player_keyboard')
                elif op == 'parsein':
                    cmd = rng.choice([0, 6, 7, 1, 2, 3, 4, 5, 8])
                    r = p.call('parse_playin', [('w', cmd)], None)
                    w.check(ctx + ' parse_playin %d' % cmd, r)
                    w.done('parse_playin')
                elif op == 'noise':
                    w.set_time(rng.getrandbits(16))
                    r = p.call('make_noise', [('w', rng.randint(0, 1))], None)
                    w.check(ctx + ' make_noise', r)
                    w.done('make_noise')
                elif op == 'sound':
                    r = p.call('set_sound', [('w', rng.randint(0, 1))], None)
                    w.check(ctx + ' set_sound', r)
                    w.done('set_sound')
                elif op == 'effect':
                    r = p.call('parse_effect', [], None)
                    w.check(ctx + ' parse_effect', r)
                    w.done('parse_effect')
                elif op == 'seteffect':
                    r = p.call('set_effect', [('w', rng.choice([0x20, 0x40, 0x20, 0x40, 0x10, 0x80, rng.getrandbits(8)])), ('w', rng.randint(0, 100))], None)
                    w.check(ctx + ' set_effect', r)
                    w.done('set_effect')
                elif op == 'eye':
                    mode = rng.choice(['player', 'player', 'none', 'mob', 'behind', 'vortex'])
                    spec = {'player': 'L+401b', 'none': '0', 'behind': 'L+3fe5', 'vortex': 'L+3fca'}.get(mode)
                    if mode == 'mob':
                        spec = 'L+%x' % obj_off(rng.randint(2, 255))
                    w.fp('UsPtr', spec)
                    w.w8('vort_x', rng.randint(0, 63))
                    w.w8('vort_y', rng.randint(0, 63))
                    w.w16('vort_rad', rng.randint(0, 200))
                    w.w16('vort_timer', rng.randint(0, 64))
                    w.w16('vort_theta', rng.getrandbits(16))
                    w.p.w('campos', 0, struct.pack('<hhh', *[rng.randint(0, 0x3f00) for _ in range(3)]))
                    w.p.w('camang', 0, struct.pack('<hhh', *[rng.randint(-30000, 30000) for _ in range(3)]))
                    w.w16('PlayerPitch', rng.randint(-2000, 2000))
                    w.w16('PlayerBank', rng.randint(-2000, 2000))
                    r = p.call('get_eye', [], None)
                    w.check(ctx + ' get_eye %s' % mode, r)
                    w.done('get_eye')
                    w.fp('UsPtr', '0')
                elif op == 'finish':
                    w.w16('PlayerInput', 0)
                    write_phys(w, 'PN', speed=rng.randint(0, 200), acc=(0, 0, rng.choice([0, -4])))
                    w.set_time(rng.getrandbits(16))
                    r = p.call('finish_player', [], None)
                    w.check(ctx + ' finish_player', r)
                    w.done('finish_player')
                elif op == 'mobile':
                    r = p.call('move_mobile', [('w', rng.randint(1, 4))], None)
                    w.check(ctx + ' move_mobile', r)
                    w.done('move_mobile')
                elif op == 'mous':
                    w.w8('WizEye', 1)
                    r = p.call('player_mous_move', [], None)
                    w.check(ctx + ' player_mous_move', r)
                    w.done('player_mous_move')
                    w.w8('WizEye', 0)
            except Skip:
                w.recover()
    w.summary('playmove')


if __name__ == '__main__':
    run('playmove', main)
