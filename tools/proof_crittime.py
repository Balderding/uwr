"""proof_crittime.py: CRITTIME.C (ovr104, critters between moments) proved: UW.EXE's routines in the emulator beside the
rules core's, on the nine real levels from LEV.ARK. The critters are the level's own (their attitude, loner, temporary,
no-heal, home, hit points and b19 fields scrambled soundly), the player is put on an open tile near them, and the
routines run both directly (up_crit, up_mob, the callbacks check_for_hostile_creature, wander_that_monster and
critter_get_told with a real tile and target, set_gridx_and_y_based_on_tile_type, change_critter_goal ...) and through
their callers (update_all_critters_whilst_player_snoozes, hostile_creatures_near, wandering_monster_check,
player_grabbed, yearly_checkup, the saved-game copies, init_level_creature_stuff). After every call the return, the
outside calls (with the printed text), Borland rand's state, the whole level block, the path squares and flood
records, the player record and the critter globals must be the same.

    python -I tools/proof_crittime.py game build/coretest.exe [per level]
"""
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import coreproof  # noqa: E402
from coreproof import Differ  # noqa: E402
from cproof_lib import SCR, SCR_OFF, World, main_wrap  # noqa: E402

ROUTINES = ['change_critter_goal', 'yearly_checkup', 'set_gridx_and_y_based_on_tile_type', 'up_crit', 'up_mob',
            'update_all_critters_whilst_player_snoozes', 'check_for_hostile_creature', 'hostile_creatures_near',
            'wander_that_monster', 'wandering_monster_check', 'set_creatures_from_saved_game',
            'set_creatures_to_saved_game', 'init_level_creature_stuff', 'critter_get_told', 'player_grabbed']
OPS = ['goal', 'goal', 'yearly', 'grid', 'grid', 'upcrit', 'upcrit', 'upcrit', 'upmob', 'upmob', 'all', 'hostnear',
       'hostnear', 'cbhostile', 'cbhostile', 'wander', 'wander', 'wandercb', 'wandercb', 'wandercb', 'from', 'to',
       'initlev', 'told', 'told', 'grabbed', 'grabbed', 'grabbed']
# UW.EXE's symbols for these two are cut at 31 letters
coreproof.HOSTNAME['set_gridx_and_y_based_on_tile_ty'] = 'set_gridx_and_y_based_on_tile_type'
GRID = 'set_gridx_and_y_based_on_tile_ty'
coreproof.HOSTNAME['update_all_critters_whilst_playe'] = 'update_all_critters_whilst_player_snoozes'
UPDATE_ALL = 'update_all_critters_whilst_playe'
CNT = 'editor_dispatch'  # 192 bytes of DS only a stub uses: the 64 race counts of up_crit


def relax(res):
    """critter_get_told prints get_name's buffer, which get_name leaves unwritten for an object with no name string (an
    item of the area with an empty name: the callback does not look at the class): the original then prints what its
    stack held, the host another thing. Where the original's name is empty the name is not compared."""
    (ra, ca), (rb, cb) = res
    if ca == cb or 'scroll_print(' not in ca:
        return res
    pat = re.compile('scroll_print[(]"(.*?)"[)](?:,|$)')
    A, B = pat.findall(ca), pat.findall(cb)
    if len(A) != len(B):
        return res
    for k in range(len(A)):
        if A[k] != B[k] and A[k].startswith(' ') and B[k].endswith(A[k]):
            cb = cb.replace('scroll_print("%s")' % B[k], 'scroll_print("%s")' % A[k], 1)
    return (ra, ca), (rb, cb)


def main():
    game, coretest = sys.argv[1], sys.argv[2]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 250
    w = World(game, coretest, 33)
    w.allow_whoami = True   # critter_get_told prints the personal names (nothing here kills a critter)
    p = w.p
    p.files()
    p.call('init_strings', [], 'b')
    rng = w.rng
    counts, total, active = {}, 0, {}
    trace = os.environ.get('PROOF_TRACE')
    for level in range(1, 10):
        w.level(level)
        # the player is an object of the area too and has no name string: his race is one no owner has here
        p.w('Creature', 63 * 48 + 9, bytes([0x1f]))
        # a few creature items (0x4f, 0x7b..0x7e) have no name string: get_name writes nothing and critter_get_told
        # prints the uninitialised buffer; those critters are given the item before
        blk = w.block()
        for i in range(2, 256):
            o = 0x4000 + i * 27
            idv, = struct.unpack_from('<H', blk, o)
            if idv & 0x1ff in (0x4f, 0x7b, 0x7c, 0x7d, 0x7e):
                p.w('L', o, struct.pack('<H', (idv & 0xfe00) | 0x4e))
        for n in range(per):
            b = w.block()
            # line_of_sight (critter_get_told, wander) overshoots its destination by up to 10 tiles when the line misses
            # it, reading tiles outside the map, which is another thing in DOS and on the host: keep 10 tiles from the edge
            crits = [c for c in w.critters(b) if 10 <= w.crit_xy(b, c)[0] <= 53 and 10 <= w.crit_xy(b, c)[1] <= 53]
            mobs = w.mobiles(b)
            if not crits:
                continue
            op = rng.choice(OPS)
            ci = rng.choice(crits)
            p.seed(rng.getrandbits(32))
            ctx = 'level %d op %d %s crit %d' % (level, n, op, ci)
            if trace:
                print(ctx, flush=True)
            # critters scrambled (a few, or many for the whole-level calls), the player near
            for c in (rng.sample(crits, min(len(crits), rng.choice([1, 3, 10, 40]))) if op in ('all', 'yearly') else [ci]):
                if rng.random() < 0.8:
                    w.scramble_critter(c, hostile=rng.choice([None, None, True]))
            if not w.player_near(ci, radius=rng.choice([0, 1, 2, 3, 5, 8]), margin=10):
                continue
            b = w.block()
            w.zero_stdat()
            p.w(CNT, 0, bytes(rng.getrandbits(8) if rng.random() < 0.3 else 0 for _ in range(64)))
            p.w('Inventory', SCR_OFF, bytes(16))
            mx, my = w.crit_xy(b, ci)
            p.w('MapObj_X', 0, struct.pack('<h', max(1, min(62, mx + rng.randint(-3, 3)))))
            p.w('MapObj_Y', 0, struct.pack('<h', max(1, min(62, my + rng.randint(-3, 3)))))
            w.rand_flags()
            for ct in ('CT1', 'CT2', 'CT4'):
                p.w(ct, 4, struct.pack('<HH', rng.choice([0x1010, 0x1010, 0x80, 0x10A8]), rng.choice([0x20, 0x20, 0, 0x28])))
            p.w('freepaths', 0, struct.pack('<H', 0xFFFF))
            res = None
            extra = ['MapObj_X', 'MapObj_Y', ('Inventory', 0x38), (CNT, 64)]
            tile = lambda x, y: 'L+%x' % ((y * 64 + x) * 4)
            tx, ty = w.crit_xy(b, ci)
            if op == 'goal':
                w.set_vars(rng.choice(crits))
                res = p.call('change_critter_goal', [('f', 'L+%x' % w.obj_off(ci)), ('w', rng.randint(0, 12)),
                                                      ('w', rng.choice([1, 1, ci, rng.randint(1, 255)]))], None)
            elif op == 'yearly':
                res = p.call('yearly_checkup', [], None)
            elif op == 'grid':
                res = p.call(GRID, [('w', rng.randint(0, 15)), ('n', SCR), ('n', '@Inventory+%x' % (SCR_OFF + 1))], 'b')
            elif op == 'upcrit':
                res = p.call('up_crit', [('f', 'L+%x' % w.obj_off(ci)), ('n', '@' + CNT)], None)
            elif op == 'upmob':
                nc = [m for m in mobs if m not in crits and m != 1] or [ci]
                m = rng.choice(nc)
                p.w('XP', 0, bytes(2))
                p.w('YP', 0, bytes(2))
                res = p.call('up_mob', [('f', 'L+%x' % w.obj_off(m))], 'b')
            elif op == 'all':
                res = p.call(UPDATE_ALL, [], None)
            elif op == 'hostnear':
                res = p.call('hostile_creatures_near', [], 'b')
            elif op == 'cbhostile':
                res = p.call('check_for_hostile_creature', [('w', tx), ('w', ty), ('f', 'L+%x' % w.obj_off(rng.choice(crits + [1]))),
                                                              ('f', tile(tx, ty)), ('w', rng.randint(0, 3))], 'b')
            elif op == 'wander':
                res = p.call('wandering_monster_check', [], 'b')
            elif op == 'wandercb':
                res = p.call('wander_that_monster', [('w', tx), ('w', ty), ('f', 'L+%x' % w.obj_off(rng.choice(crits + [1]) if rng.random() < 0.1 else ci)),
                                                       ('f', tile(tx, ty)), ('w', rng.randint(0, 3))], 'b')
            elif op == 'from':
                res = p.call('set_creatures_from_saved_game', [], None)
            elif op == 'to':
                res = p.call('set_creatures_to_saved_game', [], None)
            elif op == 'initlev':
                res = p.call('init_level_creature_stuff', [], None)
            elif op in ('told', 'grabbed'):
                stolen = rng.choice([o for o in mobs if o != 1] + [256 + rng.randint(0, 100)])
                owner = rng.choice([0, 0, rng.randint(0, 0x3f), rng.randint(0, 0x1f), 0x20, 0x2d, 0x0d, 0x8d])
                race = w.creature[(struct.unpack_from('<H', b, w.obj_off(ci))[0] & 0x3f) * 48 + 9]
                if rng.random() < 0.6:
                    owner = (owner & 0xe0) | race
                if owner & 0x1f == 0x1f:
                    owner ^= 1
                r1 = p.call('player_grabbed', [('f', 'L+%x' % w.obj_off(stolen)), ('w', owner)], None)
                p.same('player_grabbed', relax(r1), ctx)
                w.compare(ctx + ' player_grabbed', extra=extra)
                if op == 'told':
                    res = p.call('critter_get_told', [('w', tx), ('w', ty), ('f', 'L+%x' % w.obj_off(ci)), ('f', tile(tx, ty)), ('w', 0)], 'b')
                else:
                    res = r1
                    r1 = None
            if res is None:
                continue
            p.same(op, relax(res), ctx)
            w.compare(ctx, extra=extra, stdat=op in ('wander', 'wandercb'))
            w.rand_state(ctx)
            counts[op] = counts.get(op, 0) + 1
            if res[0][0] not in ('ret 0', 'ret -') or res[0][1] != 'calls 0':
                active[op] = active.get(op, 0) + 1
            total += 1
    print('crittime        %d operations on 9 levels  same (%s; with a result or outside calls: %s)' % (total, ', '.join('%s %d' % kv for kv in sorted(counts.items())), ', '.join('%s %d' % kv for kv in sorted(active.items()))))
    print('all the same')


if __name__ == '__main__':
    main_wrap('crittime', main)
