"""proof_skills.py: SKILLS.C (the skills, the derived maxima, advancing, eating, dreams, the silver seed, the moonstone and
the silver tree's return, the victory checks, trap detection and removal) proved beside the rules core, on random sound
player states of the nine real levels (see proof_bags.py): a random record and creature record, the player on a tile of the
real map, traps of the real levels. After every call: the return, the outside calls (the messages with their text), rand,
the level block, the player record and every global both sides have.

Not proved, and why: player_sleep and player_key_sleep (hours of the level pass at once: the critters' updates run to
hundreds of millions of instructions in the emulator, and the front end's screen fades), mantra_advance (the mantra is typed,
and it ends in UTIL.C's delay, which waits for the clock the proofs hold still), dream when no dream is due (it waits for the clock, which the
proofs hold still), drop_drunk_player (it runs the player's physics to rest, PLAYMOVE.C's finish_player, which
needs the physics set up as the motion proofs do), check_victory (the talisman branch creates the moongate, runs the vortex, fades and teleports
through new_player_pos, the front end's world: the original ends in its fatal exit path in the emulator; the end game
branch only draws and waits for clicks), player_is_dead (death, the cutscenes and the restart: the front end's).

    python tools/proof_skills.py game build/coretest.exe [operations per level]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from inv_common import Inv, main_wrap, obj_off, scenarios, set_pn, set_spells, u16  # noqa: E402

ROUTINES = ['use_skill', 'player_use_skill', 'player_compute', 'advance', 'prime', 'add_to_skill', 'get_skill', 'great_advance',
            'report_advance', 'game_stats', 'dream',
            'plant_seed', 'moveto', 'do_resurrect', 'do_mstone', 'DetectedTrap', 'RemoveTrap',
            'player_eat']


def borland_rand(seed):
    """Borland's rand(): the next state and the value"""
    seed = (seed * 0x015A4E35 + 1) & 0xffffffff
    return seed, (seed >> 16) & 0x7fff


def trap_hosts(block):
    """the objects of the level that have a trap or a trigger linked to them (their contents list holds a trap major)"""
    out = []
    for i in list(range(2, 256)) + list(range(256, 1024)):
        off = obj_off(i)
        w0 = u16(block, off)
        if not w0 & 0x1ff or w0 & 0x8000:
            continue
        link = u16(block, off + 6) >> 6
        n = 0
        while link and n < 20:
            lw = u16(block, obj_off(link))
            if (lw >> 6) & 7 == 6:  # MAJOR_TRAP
                out.append(i)
                break
            link = u16(block, obj_off(link) + 4) >> 6
            n += 1
    return out


def main():
    game = sys.argv[1]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 500
    w = Inv(game, 145, 'skills')
    p, rng = w.p, w.rng
    w.FNPTRS = dict(w.FNPTRS)
    w.FNPTRS['npp_func'] = False
    w.SKIP = set(w.SKIP) | {'npp_func'}
    w.gnames = [(n, s) for n, s in w.gnames if n != 'npp_func']

    inited = []

    def setup(w, level, scen):
        if not inited:
            # PLAYER.C's init_player (once): the player's physics record (PN.index, PN.b24) and square handler (PT), which
            # finish_player's physics steps need
            # (player_name_handle nonzero: make_string, GAMESTRN.C's dynamic strings, is proved with that file; the proofs have
            # no string heap set up and the original's copy goes wild into DGROUP)
            p.w('player_name_handle', 0, struct.pack('<h', 3))
            w.check('init_player', p.call('init_player', [], None), 'init_player')
            inited.append(1)
        set_pn(w)

    def op(w, level, scen, k, ctx):
        kind = rng.choice(['useskill', 'playuse', 'compute', 'advance', 'prime', 'addskill', 'addskill', 'getskill', 'getskill',
                           'great', 'report', 'stats', 'dream', 'eat', 'eat', 'plant', 'plant', 'moveto',
                           'resurrect', 'mstone', 'detect', 'detect', 'remove', 'remove'])
        # the player: random but sound
        p.w('PlayerDat', 0x1e, bytes(rng.randint(0, 30) for _ in range(0x40)))
        p.w('L', 0x4000 + 27 + 8, bytes([rng.randint(150, 250)]))
        c = bytearray(rng.randint(8, 40) for _ in range(3))
        p.w('Creature', 63 * 48 + 5, bytes(c))
        p.w('Creature', 63 * 48 + 4, bytes([rng.choice([60, 100, 255])]))
        p.w('PlayerDat', 0x3d, bytes([rng.randint(1, 30)]))  # level
        p.w('PlayerDat', 0x52, bytes([rng.randint(0, 5)]))  # skill points
        p.w('PlayerDat', 0x35, bytes([rng.randint(5, 60)]))
        p.w('PlayerDat', 0xb8, bytes([rng.choice([0, 0, 4, 8, 0x20, 0x40])]))  # motion_state: not swimming (that kills)
        set_spells(w, shrooms=0)
        w.setw('PlayerLevel', level)
        w.setw('NewPlayerX', 0)
        if w.trace:
            print(ctx, kind, flush=True)
        if kind == 'useskill':
            who = 'L+401b' if rng.random() < 0.7 else 'L+%x' % obj_off(rng.randint(2, 20))
            skill = rng.randint(0, 19)
            val = rng.randint(0, 30)
            w.check('use_skill', p.call('use_skill', [('f', who), ('w', skill), ('w', val)], 'b'), ctx + ' skill %d' % skill)
        elif kind == 'playuse':
            w.check('player_use_skill', p.call('player_use_skill', [('w', rng.randint(0, 9))], 'b'), ctx)
        elif kind == 'compute':
            w.check('player_compute', p.call('player_compute', [('w', rng.randint(0, 1))], None), ctx)
        elif kind == 'advance':
            w.check('advance', p.call('advance', [('w', rng.randint(1, 4))], None), ctx)
        elif kind == 'prime':
            s_ = rng.randint(0, 19)
            w.check('prime', p.call('prime', [('w', s_)], 'w'), ctx + ' %d' % s_)
        elif kind == 'addskill':
            s_ = rng.randint(0, 19)
            w.check('add_to_skill', p.call('add_to_skill', [('w', s_)], None), ctx + ' %d' % s_)
        elif kind == 'getskill':
            s_ = rng.choice([8, 8, rng.randint(0, 19)])
            w.check('get_skill', p.call('get_skill', [('w', s_)], 'b'), ctx + ' %d' % s_)
        elif kind == 'great':
            w.check('great_advance', p.call('great_advance', [('w', rng.randint(0, 19)), ('w', rng.randint(0, 1))], None), ctx)
        elif kind == 'report':
            lst = [rng.randint(0, 19) for _ in range(rng.randint(0, 4))] + [0xff]
            p.w('S', 0, bytes(lst + [0xff] * (5 - len(lst))))
            w.check('report_advance', p.call('report_advance', [('n', 'S+0')], None), ctx + ' %s' % lst)
        elif kind == 'stats':
            w.check('game_stats', p.call('game_stats', [], None), ctx)
        elif kind == 'dream':
            # a dream due now (the other branch waits for the clock, which stands still here)
            garamon = 0
            dreams = rng.choice([0, 0, 2, 3, 7, 0xb, 0xf, 0x3ff, 0x1])
            level_ = level
            found_ok = (dreams & 1 != 1) or (level_ > 1 and dreams & 2 != 2) or dreams & 4 == 4 or dreams & 8 == 8
            if not found_ok:
                # a dream 4..9 by chance: pick a seed whose first draw says yes
                sf = rng.randint(0, 3)
                for _ in range(2000):
                    seed = rng.getrandbits(32)
                    s1, r1 = borland_rand(seed)
                    if r1 % (4 + (sf << 2)) == 0:
                        s2, r2 = borland_rand(s1)
                        if not (1 << (4 + r2 % 6)) & dreams:
                            break
                else:
                    return
                p.seed(seed)
            else:
                sf = rng.randint(0, 3)
            wd = struct.unpack('<H', bytes.fromhex(p.r('PlayerDat', 0x6e, 2)[0]))[0]
            p.w('PlayerDat', 0x6e, struct.pack('<H', dreams))
            w5 = bytes.fromhex(p.r('PlayerDat', 0x62, 1)[0])[0]
            p.w('PlayerDat', 0x62, bytes([w5 & ~8]))  # garamon (bit 3 of byte 0x62) not buried
            w.check('dream', p.call('dream', [('w', sf)], 'b'), ctx + ' dreams %x' % dreams)
        elif kind == 'drunk':
            w.check('drop_drunk_player', p.call('drop_drunk_player', [], None), ctx)
        elif kind == 'eat':
            n = rng.choice([-70, -30, -12, -1, 0, 1, 12, 40, 100, 250])
            p.w('PlayerDat', 0x39, bytes([rng.choice([0, 5, 100, 200, 250, 255])]))
            p.w('PlayerDat', 0x3b, bytes([rng.randint(0, 255)]))
            w.check('player_eat', p.call('player_eat', [('w', n)], 'b'), ctx + ' %d' % n)
        elif kind == 'victory':
            # the end game (EndGameMode) waits for clicks and draws; the other branch (no talismans left) moves the
            # player to level 9: do_teleport, the moongate, the vortex
            if rng.random() < 0.5:
                p.w('PlayerDat', 0x6d, bytes([rng.choice([0, 1, 2, 0xff])]))
                # the vortex divides by the player's distance from the gate at (32, 32) (a divide by zero at it: the original's
                # own INT 0 trap, which the emulator does not have): the player is kept away
                px, py = struct.unpack('<2h', bytes.fromhex(p.r('PN', 0, 4)[0]))
                if abs(px - 0x2000) < 0x400 and abs(py - 0x2000) < 0x400:
                    p.w('PN', 0, struct.pack('<2h', 0x500, 0x700))
                w.check('check_victory', p.call('check_victory', [], None), ctx)
        elif kind == 'plant':
            w.check('plant_seed', p.call('plant_seed', [], 'b'), ctx)
        elif kind == 'moveto':
            lv = rng.choice([level, level, rng.randint(1, 9)])
            item = rng.choice([0xdb, 0x126, 0x1c0 | 5, rng.randint(0, 0x1ff), 0x120 + 3])
            w.check('moveto', p.call('moveto', [('w', lv), ('w', item)], 'b'), ctx)
        elif kind == 'resurrect':
            t = rng.randint(1, 9)
            b5e = bytes.fromhex(p.r('PlayerDat', 0x5e, 1)[0])[0]
            p.w('PlayerDat', 0x5e, bytes([(b5e & 0x0f) | (t & 15) << 4]))  # tree
            w.check('do_resurrect', p.call('do_resurrect', [], None), ctx)
        elif kind == 'mstone':
            b5e = bytes.fromhex(p.r('PlayerDat', 0x5e, 1)[0])[0]
            p.w('PlayerDat', 0x5e, bytes([(b5e & 0xf0) | rng.randint(1, 9)]))
            w.check('do_mstone', p.call('do_mstone', [], None), ctx)
        elif kind in ('detect', 'remove'):
            hosts = trap_hosts(w.block())
            if not hosts:
                return
            o = rng.choice(hosts)
            skill = rng.randint(0, 30)
            fn = 'DetectedTrap' if kind == 'detect' else 'RemoveTrap'
            p.w('L', 0, b'') if False else None
            w.setw('MapObj_X', rng.randint(0, 63))
            w.setw('MapObj_Y', rng.randint(0, 63))
            w.check(fn, p.call(fn, [('f', 'L+%x' % obj_off(o)), ('w', skill)], 'w'), ctx + ' obj %d skill %d' % (o, skill))

    scenarios(w, per, op, setup=setup)
    w.summary('skills', ROUTINES)


if __name__ == '__main__':
    main_wrap('skills', main)
