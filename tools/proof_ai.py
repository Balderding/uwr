"""proof_ai.py: AI.C (seg007_1798, critter goals and AI, damage_critter, the time bins) proved: UW.EXE's routines in the
emulator beside the rules core's, on the nine real levels from LEV.ARK. The critters are the level's own with their
AI fields scrambled soundly (goal, target, attitude, sequence, frame, home, b19 bits, hit points, last hit ...), the
player is put on an open tile near them with a random record (skills, attributes, drawn weapon, orb ...), the critter's
own globals are loaded the way the game does (set_critter_vars, set_up_target, the physics flags) and every routine
runs directly and through its callers: critter_mv and critter_ai for one critter, move_mobile for the whole active
list. After every call the return, the outside calls (with their arguments), Borland rand's state, the whole level
block, the path squares, stored paths and flood records, the player record and the critter globals must be the same.

    python -I tools/proof_ai.py game build/coretest.exe [per level]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coreproof import Differ  # noqa: E402
from cproof_lib import SCR, SCR_OFF, World, main_wrap  # noqa: E402

ROUTINES = ['crit_drunkwalk', 'seg007_1798_3E4', 'crit_mill', 'crit_guard', 'crit_offense', 'crit_attack',
            'crit_offense_find_target', 'maybe_cast_defensive_spell', 'crit_magik_attack', 'crit_missile_attack',
            'crit_defense', 'crit_flee', 'crit_avoid_player', 'crit_talk', 'check_out_player', 'crit_hover',
            'target_found', 'look_for_target', 'constrain_movement', 'turn_real_fine', 'compute_trz_or_try_rather',
            'set_critter_vars', 'critter_ai', 'critter_mv', 'set_up_target', 'should_i_flee', 'acceptable_danger',
            'critter_set_goal', 'critter_discard_goal', 'go_into_dying_sequence', 'crit_die', 'damage_critter',
            'timetodo', 'move_mobile']
# the goals' routines, called directly; the others through critter_mv / critter_ai
OPS = (['drunk'] * 3 + ['follow'] * 2 + ['mill'] * 2 + ['guard'] * 3 + ['offense'] * 5 + ['attack'] * 3 + ['findtarget'] * 3
       + ['castdef', 'magik', 'missile'] * 2 + ['defense'] * 2 + ['flee'] * 3 + ['avoid'] * 2 + ['talk'] + ['checkout'] * 2
       + ['hover'] + ['found'] * 3 + ['look'] * 2 + ['constrain'] * 2 + ['turn'] * 2 + ['trz'] * 2 + ['flee?'] * 2
       + ['danger'] + ['setgoal'] + ['discard'] + ['dying'] + ['die'] + ['damage'] * 4 + ['bin'] * 2 + ['mv'] * 8
       + ['ai'] * 12 + ['move'] * 1 + ['setup'])

# turn_real_fine divides a long by an `unsigned` distance: on DOS the unsigned is 16 bits and the division is the signed
# long's; on the host the unsigned is 32 bits and a negative dx or dy is divided as a huge unsigned. The core differs from
# UW.EXE there (reported, see the final report). Until the core is fixed (PROOF_NEG=1 runs the cases as they are) the
# proof keeps the arguments non-negative: look_for_target / crit_magik_attack / crit_missile_attack with the target to the
# south east, and the callers of those (crit_offense, crit_defense, critter_mv, critter_ai, move_mobile) with critters
# that neither cast nor fire a missile.
NEG = bool(os.environ.get('PROOF_NEG'))
TURN_DIRECT = {'look', 'magik', 'missile', 'turn'}
TURN_VIA = {'offense', 'defense', 'mv', 'ai'}
NEEDS = {'talk', 'turn', 'follow', 'offense', 'attack', 'findtarget', 'magik', 'missile', 'defense', 'flee', 'found', 'look', 'trz', 'castdef', 'setup'}


def main():
    game, coretest = sys.argv[1], sys.argv[2]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 400
    w = World(game, coretest, 44)
    p = w.p
    p.files()
    p.call('init_strings', [], 'b')
    rng = w.rng
    counts, total, active = {}, 0, {}
    trace = os.environ.get('PROOF_TRACE')
    extra = [('Inventory', 0x38), 'inanmMapX', 'inanmMapY', 'mspell_mused', 'ObjectActorArg', 'MoveCrits', 'nvokHgt', 'nvokTerr', 'stay_centered', 'MapObj_X', 'MapObj_Y']
    extra = [e for e in extra if (e[0] if isinstance(e, tuple) else e) in p.data]
    for level in range(1, 10):
        w.level(level)
        blk = w.block()
        for i in range(2, 256):
            o = 0x4000 + i * 27
            idv, = struct.unpack_from('<H', blk, o)
            if idv & 0x1ff in (0x4f, 0x7b, 0x7c, 0x7d, 0x7e):
                p.w('L', o, struct.pack('<H', (idv & 0xfe00) | 0x4e))
        for n in range(per):
            b = w.block()
            crits = w.critters(b)
            if not crits:
                continue
            op = rng.choice(OPS)
            ci = rng.choice(crits)
            if op in TURN_VIA and not NEG:
                safe = [c for c in crits if not w.creature[(struct.unpack_from('<H', b, w.obj_off(c))[0] & 0x3f) * 48 + 0x2D] >> 1
                        and (w.creature[(struct.unpack_from('<H', b, w.obj_off(c))[0] & 0x3f) * 48 + 0x20] >> 1) >> 4 != 1]
                if not safe:
                    continue
                ci = rng.choice(safe)
            p.seed(rng.getrandbits(32))
            ctx = 'level %d op %d %s crit %d' % (level, n, op, ci)
            if trace:
                print(ctx, flush=True)
            if op == 'move':
                for c in rng.sample(crits, min(len(crits), 30)):
                    w.scramble_critter(c, hostile=rng.choice([None, True]))
            elif rng.random() < 0.85:
                w.scramble_critter(ci, hostile=rng.choice([None, None, True]))
            if not w.player_near(ci, radius=rng.choice([0, 1, 1, 2, 2, 3, 4, 6, 9, 14])):
                continue
            b = w.block()
            w.zero_stdat()
            # the player's hit points; the AI's record of the last hit; the eye
            p.w('L', 0x4000 + 27 + 8, bytes([rng.randint(1, 60)]))
            px, py = w.crit_xy(b, 1)
            cx, cy = w.crit_xy(b, ci)
            # the eye is where the player is (the game's 3D view): the AI runs for the critters within 10 tiles of it
            p.w('lastXeye', 0, struct.pack('<h', rng.choice([px, px, cx])))
            p.w('lastYeye', 0, struct.pack('<h', rng.choice([py, py, cy])))
            clock = struct.unpack('<I', bytes.fromhex(p.r('PlayerDat', 0xCE, 4)[0]))[0]
            p.w('crithit', 0, bytes([rng.choice([1, ci, rng.choice(crits)])]))  # never 0: an ally would take it for its target (null far pointer)
            race = w.creature[(struct.unpack_from('<H', b, w.obj_off(ci))[0] & 0x3f) * 48 + 9]
            p.w('typehit', 0, bytes([rng.choice([0xFF, race, race, rng.randint(0, 31)])]))
            p.w('crithittime', 0, struct.pack('<I', max(0, clock - rng.choice([0, 3, 0x100, 0x1ff, 0x201, 0x400]))))
            p.w('hitx', 0, bytes([max(0, min(63, cx + rng.randint(-3, 3)))]))
            p.w('hity', 0, bytes([max(0, min(63, cy + rng.randint(-3, 3)))]))
            p.w('Inventory', SCR_OFF, bytes(16))
            p.w('MapObj_X', 0, struct.pack('<h', cx))
            p.w('MapObj_Y', 0, struct.pack('<h', cy))
            # no costly terrain (w6 0): beeline hands hyp_move an uninitialised danger count (see proof_pathfind)
            for ct in ('CT1', 'CT2', 'CT4'):
                p.w(ct, 4, struct.pack('<HH', rng.choice([0x1010, 0x1010, 0x80, 0x10A8]), 0))
            p.w('freepaths', 0, struct.pack('<H', rng.choice([0xFFFF, 0xFFFF, rng.getrandbits(16)])))
            p.w('curBin', 0, bytes([rng.randint(0, 15)]))
            p.w('lastbin', 0, bytes([rng.randint(0, 15)]))
            # set_up_target with target 0 reads a null far pointer (DOS: the low memory of the machine, the emulator: the
            # load image): the routines that need a target are given one
            o = w.obj_off(ci)
            gw, = struct.unpack_from('<H', w.block(), o + 0x0B)
            if op == 'talk':
                gw = (gw & 0xF00F) | 0x10    # crit_talk's target is the player
                p.w('L', o + 0x0B, struct.pack('<H', gw))
            elif not (gw >> 4) & 0xFF and op in NEEDS:
                gw |= 0x10
                p.w('L', o + 0x0B, struct.pack('<H', gw))
            w.set_vars(ci)
            w.rand_flags()
            if (gw >> 4) & 0xFF and (op in NEEDS or rng.random() < 0.5):
                r0 = p.call('set_up_target', [], 'b')
                p.same('set_up_target', r0, ctx)
            if op == 'turn':
                # cSqRt of a negative long (the 16-bit square sum of tdx and tdy wrapped) is not a state the game reaches
                tdx, tdy = struct.unpack('<hh', bytes.fromhex(p.r('tdx', 0, 2)[0] + p.r('tdy', 0, 2)[0]))
                if tdx * tdx + tdy * tdy > 0x7fff:
                    continue
            if op in TURN_DIRECT and op != 'turn':
                ok = False
                for attempt in range(30):
                    tdx, tdy = struct.unpack('<hh', bytes.fromhex(p.r('tdx', 0, 2)[0] + p.r('tdy', 0, 2)[0]))
                    if (-100 if NEG else 0) <= tdx <= 100 and (-100 if NEG else 0) <= tdy <= 100:  # (NEG: negative, but in reach: no wrapped square sum)   # (the arguments are chars: tdx 128 or more would be negative)
                        ok = True
                        break
                    w.player_near(ci, radius=rng.choice([0, 1, 2, 3, 5]))
                    r0 = p.call('set_up_target', [], 'b')
                    p.same('set_up_target', r0, ctx)
                if not ok:
                    continue
            res = None
            L = lambda o: 'L+%x' % w.obj_off(o)
            if op == 'drunk':
                res = p.call('crit_drunkwalk', [], None)
            elif op == 'follow':
                res = p.call('seg007_1798_3E4', [], None)
            elif op == 'mill':
                res = p.call('crit_mill', [], None)
            elif op == 'guard':
                res = p.call('crit_guard', [], None)
            elif op == 'offense':
                res = p.call('crit_offense', [], None)
            elif op == 'attack':
                res = p.call('crit_attack', [('w', rng.choice([rng.randint(0, 0x200), rng.randint(0, 0x60), 0x30, 0x64, 0x65, 0x51, 0x52]))], 'b')
            elif op == 'findtarget':
                res = p.call('crit_offense_find_target', [('w', rng.choice([cx, px, rng.randint(0, 63)])), ('w', rng.choice([cy, py, rng.randint(0, 63)])),
                                                           ('w', rng.choice([1, 4, 2, 3, 6]))], None)
            elif op == 'castdef':
                res = p.call('maybe_cast_defensive_spell', [], 'b')
            elif op == 'magik':
                res = p.call('crit_magik_attack', [], 'b')
            elif op == 'missile':
                res = p.call('crit_missile_attack', [], 'b')
            elif op == 'defense':
                res = p.call('crit_defense', [], None)
            elif op in ('flee', 'flee?'):
                res = p.call('crit_flee', [], None) if op == 'flee' else p.call('should_i_flee', [('w', rng.randint(0, 255)), ('w', rng.randint(0, 255)), ('w', rng.randint(0, 15)), ('w', rng.randint(0, 255))], 'b')
            elif op == 'avoid':
                res = p.call('crit_avoid_player', [('w', rng.getrandbits(8)), ('w', rng.choice([0, 10, 0x18, rng.randint(0, 40)]))], 'b')
            elif op == 'talk':
                # crit_talk starts a conversation when the player faces the critter: the player is made to face the same
                # way as the direction from the critter (rel 0), where it never does
                tdx, tdy = struct.unpack('<hh', bytes.fromhex(p.r('tdx', 0, 2)[0] + p.r('tdy', 0, 2)[0]))
                sb = lambda v: (v & 0xff) - 256 if v & 0x80 else v & 0xff
                x_, y_ = sb(tdx), sb(tdy)
                dx_, dy_ = sb(x_ << 1), sb(y_ << 1)
                if y_ > dx_:
                    head = (0 if y_ > -dx_ else 7) if x_ > -dy_ else (5 if x_ > dy_ else 6)
                else:
                    head = (2 if x_ > dy_ else 1) if x_ > -dy_ else (3 if y_ > -dx_ else 4)
                pp, = struct.unpack('<H', bytes.fromhex(p.r('L', 0x4000 + 27 + 2, 2)[0]))
                p.w('L', 0x4000 + 27 + 2, struct.pack('<H', (pp & 0xFC7F) | (head << 7)))
                res = p.call('crit_talk', [], None)
            elif op == 'checkout':
                res = p.call('check_out_player', [], None)
            elif op == 'hover':
                res = p.call('crit_hover', [], None)
            elif op == 'found':
                res = p.call('target_found', [('n', SCR), ('n', '@Inventory+%x' % (SCR_OFF + 1))], 'b')
            elif op == 'look':
                res = p.call('look_for_target', [('w', rng.randint(0, 1))], 'b')
            elif op == 'constrain':
                p.w('myoldfacing', 0, bytes([rng.getrandbits(8)]))
                p.w('myoldheading', 0, bytes([rng.getrandbits(8)]))
                p.w('myoldspeed', 0, bytes([rng.randint(0, 12)]))
                res = p.call('constrain_movement', [], None)
            elif op == 'turn':
                lo = -100 if NEG else 0
                res = p.call('turn_real_fine', [('w', rng.randint(lo, 100)), ('w', rng.randint(lo, 100))], 'b')
            elif op == 'trz':
                res = p.call('compute_trz_or_try_rather', [('w', rng.choice([0, 0x1E, rng.randint(0, 255)])), ('w', rng.randint(0, 1))], 'b')
            elif op == 'danger':
                res = p.call('acceptable_danger', [], 'b')
            elif op == 'setgoal':
                goal_ = rng.randint(0, 12)
                tgt_ = rng.choice([1, 0, ci, rng.randint(1, 255)])
                if goal_ in (3, 5, 6, 9) and not tgt_:
                    tgt_ = 1    # these goals call set_up_target later: target 0 is a null far pointer
                res = p.call('critter_set_goal', [('w', goal_), ('w', tgt_)], None)
            elif op == 'discard':
                res = p.call('critter_discard_goal', [], None)
            elif op == 'dying':
                res = p.call('go_into_dying_sequence', [('f', L(rng.choice(crits)))], 'b')
            elif op == 'die':
                res = p.call('crit_die', [('f', L(rng.choice(crits)))], 'b')
            elif op == 'damage':
                tgt = rng.choice(crits + [1]) if rng.random() < 0.9 else ci
                src = rng.choice(['0', L(1), L(1), L(rng.choice(crits)), L(rng.choice([m for m in w.mobiles(b)]))])
                res = p.call('damage_critter', [('f', L(tgt)), ('w', rng.choice([0, 1, 3, 8, 20, 60, 255])), ('f', src)], 'b')
            elif op == 'bin':
                res = p.call('timetodo', [('w', rng.randint(0, 15)), ('w', rng.randint(0, 7))], 'b')
            elif op == 'mv':
                res = p.call('critter_mv', [], None)
            elif op == 'ai':
                res = p.call('critter_ai', [], 'b')
            elif op == 'move':
                if not NEG:
                    # no caster and no missile weapon in the table for this call (they reach turn_real_fine)
                    for k in range(64):
                        c = w.creature[k * 48:(k + 1) * 48]
                        p.w('Creature', k * 48 + 0x20, bytes([0 if (c[0x20] >> 1) >> 4 == 1 else c[0x20]]))
                        p.w('Creature', k * 48 + 0x2D, bytes([c[0x2D] & 1]))
                res = p.call('move_mobile', [('w', rng.choice([1, 2, 3, 5, 8]))], None)
                if not NEG:
                    p.same(op, res, ctx)
                    w.compare(ctx, extra=extra, stdat=True)
                    for k in range(64):
                        p.w('Creature', k * 48 + 0x20, w.creature[k * 48 + 0x20:k * 48 + 0x21])
                        p.w('Creature', k * 48 + 0x2D, w.creature[k * 48 + 0x2D:k * 48 + 0x2E])
            elif op == 'setup':
                res = p.call('set_up_target', [], 'b')
            if res is None:
                continue
            p.same(op, res, ctx)
            w.compare(ctx, extra=extra, stdat=op in ('mv', 'ai', 'move', 'follow', 'offense', 'mill', 'drunk', 'guard'))
            w.rand_state(ctx)
            counts[op] = counts.get(op, 0) + 1
            if res[0][0] not in ('ret 0', 'ret -') or res[0][1] != 'calls 0':
                active[op] = active.get(op, 0) + 1
            total += 1
    print('ai              %d operations on 9 levels  same (%s; with a result or outside calls: %s)' % (
        total, ', '.join('%s %d' % kv for kv in sorted(counts.items())), ', '.join('%s %d' % kv for kv in sorted(active.items()))))
    print('all the same')


if __name__ == '__main__':
    main_wrap('ai', main)
