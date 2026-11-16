"""proof_pathfind.py: PATHFIND.C (seg006_1477, critter motion, path finding and doors) proved: UW.EXE's routines in the
emulator beside the rules core's, on the nine real levels from LEV.ARK. The critters are the level's own (their AI
fields scrambled soundly), the player is put on an open tile near them, the walker / flier / swimmer physics records and
handlers (CN1..CN4, CT1..CT4) are the ones init_ai makes (their path-finding words varied), and the arguments are real
places of the map. After every call the return, the outside calls, Borland rand's state, the whole level block, the
path squares, the 16 stored paths, the flood records (stdat), the critter scratch globals, the physics records and the
out parameters must be the same.

    python -I tools/proof_pathfind.py game build/coretest.exe [per level]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coreproof import Differ  # noqa: E402
from cproof_lib import LSCR, SCR, SCR_OFF, World, main_wrap  # noqa: E402

# hyp_move has 11 arguments and tests/coretest.c passes 8: it is proved inside flood_path, beeline and add_to_beeline_path
# (all three of its forms: the start, the end and a middle step), whose outputs are the flood records and path squares
ROUTINES = ['build_corpse', 'move_me_joe', 'init_ai', 'get_terrain', 'crit_hndlr_walk', 'crit_hndlr_fly',
            'crit_hndlr_swim', 'do_crit_phys', 'hyp_move', 'hyp_see', 'flood_path', 'make_path_from_flood_data',
            'beeline', 'line_of_sight', 'add_to_beeline_path', 'find_free_path', 'clear_paths', 'store_path',
            'set_next_square_on_path', 'close_to_square', 'move_along_path', 'do_that_jump_kinda_thing',
            'deltatotheta', 'set_loc', 'crit_head_for_loc', 'adjust_height', 'try_to_open_door']
OPS = ['dtt', 'closesq', 'hypsee', 'flood', 'flood', 'flood', 'beeline', 'beeline',
       'addbee', 'los', 'los', 'findfree', 'clear', 'store', 'nextsq', 'along', 'along', 'jump', 'setloc', 'head',
       'head', 'head', 'head', 'adjh', 'door', 'hndlr', 'hndlr', 'terrain', 'phys', 'phys', 'corpse', 'mejoe', 'initai']


def main():
    game, coretest = sys.argv[1], sys.argv[2]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 300
    w = World(game, coretest, 21)
    p = w.p
    rng = w.rng
    counts, total, skipped_ret = {}, 0, 0
    trace = os.environ.get('PROOF_TRACE')

    def walk(b, x, y, n=63):
        """n adjacent steps from (x, y), as pathsq's bytes (x, y, unused, flag)"""
        out = bytearray()
        for _ in range(n):
            out += bytes([x, y, rng.randint(0, 16), rng.choice([0, 0, 0, 1])])
            dx, dy = rng.choice([(0, 1), (1, 0), (0, -1), (-1, 0), (0, 0)])
            x, y = max(1, min(62, x + dx)), max(1, min(62, y + dy))
        return bytes(out)

    def pathrec(x, y, jump=False):
        r = bytearray(28)
        r[0], r[1] = max(1, min(62, x)), max(1, min(62, y))
        cnt = rng.randint(0, 40)
        r[2] = cnt | (0x80 if jump or rng.random() < 0.2 else 0)
        r[3] = rng.randint(0, 63)
        for i in range(4, 28):
            r[i] = rng.getrandbits(8)
        return bytes(r)

    def dest_near(b, x, y, r):
        t = w.open_tiles_near(b, x, y, r)
        return rng.choice(t) if t else (x, y, w.tile(b, x, y)[1])

    for level in range(1, 10):
        w.level(level)
        for n in range(per):
            b = w.block()
            crits = w.critters(b)
            mobs = w.mobiles(b)
            if not crits:
                continue
            op = rng.choice(OPS) if n else 'initai'
            if op in ('head', 'door'):
                # a critter that bumps a door bashes it for rand() % attacks[0].damage: a divide by zero for 0
                crits = [c for c in crits if w.creature[(struct.unpack_from('<H', b, w.obj_off(c))[0] & 0x3f) * 48 + 0x14]] or crits
            ci = rng.choice(crits)
            p.seed(rng.getrandbits(32))
            ctx = 'level %d op %d %s crit %d' % (level, n, op, ci)
            if trace:
                print(ctx, flush=True)
            # a scrambled critter, the player near, the path words of the handlers, a path or none
            if rng.random() < 0.8:
                w.scramble_critter(ci)
            if rng.random() < 0.3:
                o = w.obj_off(ci)
                b15 = b[o + 0x15] | 0x80
                p.w('L', o + 0x15, bytes([b15]))
            if not w.player_near(ci):
                continue
            b = w.block()
            for ct in ('CT1', 'CT2', 'CT3', 'CT4'):
                nc = rng.choice([0x1010, 0x1010, 0x10, 0x80, 0x10A8, 0x1000, 0x8, 0])
                w6 = rng.choice([0x20, 0x20, 0, 0x8, 0x28, 0x10, 0x3F8])
                p.w(ct, 4, struct.pack('<HH', nc, w6))
            p.w('freepaths', 0, struct.pack('<H', rng.choice([0xFFFF, 0xFFFF, 0xFFFF, rng.getrandbits(16), 0])))
            w.wfar('paths', 0, b''.join(pathrec(*w.crit_xy(b, ci)) for _ in range(16)))
            w.wfar('pathsq', 0, walk(b, *w.crit_xy(b, ci)))
            w.zero_stdat() if op in ('flood', 'head', 'door') else None
            if op in ('flood', 'head'):
                w.wfar('pathsq', 0, b'\0' * 252)
            w.set_vars(ci)
            w.rand_flags()
            cx, cy = w.crit_xy(b, ci)
            ch = w.tile(b, cx, cy)[1]
            p.w('myheight', 0, bytes([rng.choice([rng.randint(0, 40), 12, 24])]) if rng.random() < 0.3 else p.r('myheight', 0, 1)[0] and bytes.fromhex(p.r('myheight', 0, 1)[0]))
            res = None
            stdat = False
            extra = ['MapObj_X', 'MapObj_Y'] if 'MapObj_X' in p.data else []
            if op == 'initai':
                res = p.call('init_ai', [], None)
            elif op == 'dtt':
                x, y = rng.choice([(rng.randint(-128, 127), rng.randint(-128, 127)), (rng.randint(-5, 5), rng.randint(-5, 5)),
                                   (0, rng.randint(-9, 9)), (rng.randint(-9, 9), 0)])
                res = p.call('deltatotheta', [('w', x), ('w', y)], 'b')
            elif op == 'closesq':
                fl = rng.choice([0, 1, -1])
                xh, yh = rng.randint(0, 63), rng.randint(0, 63)
                res = p.call('close_to_square', [('w', fl), ('w', xh), ('w', yh), ('w', rng.randint(0, 7)), ('w', rng.randint(0, 7)),
                                                 ('w', xh + rng.randint(-2, 2)), ('w', yh + rng.randint(-2, 2))], 'w')
            elif op == 'hypsee':
                x1, y1, _ = dest_near(b, cx, cy, 3)
                dx, dy = rng.choice([(0, 1), (1, 0), (0, -1), (-1, 0)])
                old = rng.choice([(0, 0), (x1, y1), (x1 - dx, y1 - dy), (x1 + rng.randint(-1, 1), y1)])
                res = p.call('hyp_see', [('w', old[0]), ('w', old[1]), ('w', x1), ('w', y1), ('w', rng.choice([0, x1 + dx, x1 + dx, x1 + dx])),
                                         ('w', y1 + dy), ('w', rng.randint(0, 255))], 'b')
            elif op == 'flood':
                stdat = True
                dx, dy, dz = dest_near(b, cx, cy, rng.choice([2, 4, 6, 9]))
                res = p.call('flood_path', [('w', cx), ('w', cy), ('w', ch), ('w', dx), ('w', dy), ('w', dz), ('w', rng.randint(0, 10))], 'b')
                if res[0][0] == res[1][0] and res[0][0].split()[1] == '1':
                    p.same('flood_path', res, ctx)
                    w.compare(ctx + ' flood', extra=extra, stdat=True)
                    pl = int(p.r('pathlen', 0, 1)[0], 16)
                    res = p.call('make_path_from_flood_data', [('w', pl - 1), ('w', dx), ('w', dy)], None)
                    op = 'makepath'
            elif op == 'beeline':
                # beeline and add_to_beeline_path hand hyp_move an uninitialised local as its danger count: the cost of
                # a terrain and a drop add to it before the test against 0 (it matters only when the garbage wraps to 0).
                # No costly terrain (w6 0); a drop of two or more along the path is not compared in the return
                for ct in ('CT1', 'CT2', 'CT3', 'CT4'):
                    p.w(ct, 6, bytes(2))
                dx, dy, dz = dest_near(b, cx, cy, rng.choice([2, 4, 6, 9]))
                res = p.call('beeline', [('w', cx), ('w', cy), ('w', dx), ('w', dy)], 'w')
            elif op == 'addbee':
                for ct in ('CT1', 'CT2', 'CT3', 'CT4'):
                    p.w(ct, 6, bytes(2))
                w.wfar('pathsq', 0, walk(b, cx, cy))
                p.w('pathlen', 0, bytes([rng.randint(1, 6)]))
                p.w('pathlen', 0, bytes([rng.choice([1, 2, 3, 5, 61, 62])]))
                x, y, _ = dest_near(b, cx, cy, 3)
                res = p.call('add_to_beeline_path', [('w', x), ('w', y)], 'b')
            elif op == 'los':
                x1 = (cx << 3) + rng.randint(0, 7)
                y1 = (cy << 3) + rng.randint(0, 7)
                tx, ty, th = dest_near(b, cx, cy, rng.choice([2, 5, 9, 12]))
                res = p.call('line_of_sight', [('w', x1), ('w', y1), ('w', rng.randint(0, 60)), ('w', (tx << 3) + rng.randint(0, 7)),
                                               ('w', (ty << 3) + rng.randint(0, 7)), ('w', rng.randint(0, 60))], 'b')
            elif op == 'findfree':
                p.w('Inventory', SCR_OFF, bytes([rng.getrandbits(8)]))
                res = p.call('find_free_path', [('n', SCR)], 'b')
                extra = ['Inventory']
            elif op == 'clear':
                res = p.call('clear_paths', [], None)
            elif op == 'store':
                p.w('pathlen', 0, bytes([rng.randint(1, 55)]))
                p.w('L', LSCR, bytes(rng.getrandbits(8) for _ in range(28)))
                res = p.call('store_path', [('f', 'L+%x' % LSCR)], None)
            elif op == 'nextsq':
                p.w('L', LSCR, pathrec(cx, cy))
                res = p.call('set_next_square_on_path', [('f', 'L+%x' % LSCR)], 'b')
            elif op == 'along':
                p.w('L', LSCR, pathrec(cx + rng.randint(-2, 2), cy + rng.randint(-2, 2)))
                res = p.call('move_along_path', [('f', 'L+%x' % LSCR)], 'b')
            elif op == 'jump':
                p.w('L', LSCR, pathrec(cx + rng.randint(-2, 2), cy + rng.randint(-2, 2), True))
                res = p.call('do_that_jump_kinda_thing', [('f', 'L+%x' % LSCR)], None)
            elif op == 'setloc':
                res = p.call('set_loc', [('w', rng.randint(0, 63)), ('w', rng.randint(0, 63)), ('w', rng.randint(0, 15))], None)
            elif op == 'head':
                dx, dy, dz = dest_near(b, cx, cy, rng.choice([0, 1, 3, 6, 9]))
                res = p.call('crit_head_for_loc', [('w', dx), ('w', dy), ('w', dz)], None)
                stdat = True
            elif op == 'adjh':
                res = p.call('adjust_height', [('w', rng.randint(0, 63)), ('w', rng.randint(0, 63))], None)
            elif op == 'door':
                ds = w.doors(b)
                if not ds:
                    continue
                p.w('doorx', 0, bytes([rng.randint(1, 62)]))
                p.w('doory', 0, bytes([rng.randint(1, 62)]))
                res = p.call('try_to_open_door', [('f', 'L+%x' % w.obj_off(rng.choice(ds)))], None)
            elif op == 'hndlr':
                kind = rng.choice(['crit_hndlr_walk', 'crit_hndlr_fly', 'crit_hndlr_swim'])
                w.rand_ppd(ci, doors=True)
                co = rng.choice(w.chain_tile_objects(b, cx, cy) or [1])
                p.p('collobject', 0, 'L+%x' % w.obj_off(co))
                p.w('crit_terr', 0, struct.pack('<H', rng.getrandbits(16) & rng.getrandbits(16)))
                p.w('Inventory', SCR_OFF, struct.pack('<H', rng.choice([rng.getrandbits(16) & rng.getrandbits(16), 0x1000, 0x10, 0x800, 0x20, 0x300, 0x400, 8, 0x100, 0x200, 0x410])))
                p.w('CN1', 6, struct.pack('<hhhhhh', rng.randint(-300, 300), rng.randint(-300, 300), rng.choice([0, 0, 0x8d, -0x40]), 0, 0, rng.choice([0, 0, -4])))
                res = p.call(kind, [('n', SCR)], 'b')
                extra = ['Inventory']
            elif op == 'terrain':
                res = p.call('get_terrain', [('f', 'L+%x' % w.obj_off(ci))], 'w')
            elif op == 'phys':
                pn = {'CN1': 'CT1', 'CN2': 'CT2', 'CN4': 'CT4'}
                k = rng.choice(list(pn))
                r1 = p.call('get_phys_data', [('f', 'L+%x' % w.obj_off(ci)), ('n', '@' + k)], None)
                p.same('get_phys_data', r1, ctx)
                w.compare(ctx + ' get_phys_data', extra=extra)
                res = p.call('do_crit_phys', [('n', '@' + k), ('n', '@' + pn[k])], 'b')
            elif op == 'corpse':
                p.w('XP', 0, struct.pack('<h', cx))
                p.w('YP', 0, struct.pack('<h', cy))
                res = p.call('build_corpse', [('f', 'L+%x' % w.obj_off(ci)), ('w', rng.choice([0, 1, 2, 3, 5, 7])),
                                              ('w', rng.choice([0, 1, 2, 3, 5, 7]))], None)
            elif op == 'mejoe':
                nc = [m for m in mobs if m not in crits and m != 1]
                if not nc:
                    continue
                mi = rng.choice(nc)
                p.p('meptr', 0, 'L+%x' % w.obj_off(mi))
                if rng.random() < 0.5:
                    p.w('L', w.obj_off(mi) + 8, bytes([rng.choice([0, 1, 5])]))
                res = p.call('move_me_joe', [], 'b')
            if res is None:
                continue
            if op in ('beeline', 'addbee') and res[0] != res[1]:
                # equal except for the uninitialised danger count: only if the path drops by two or more somewhere
                mine, ours = w.read_many([('pathsq', 0, 252), ('pathlen', 0, 1)])
                sq, pl = bytes.fromhex(mine[0]), int(mine[1], 16)
                hs = [w.tile(b, sq[i * 4], sq[i * 4 + 1])[1] for i in range(min(pl + 1, 63))]
                drop = any(hs[i] - hs[i + 1] >= 2 for i in range(len(hs) - 1))
                if drop and res[0][1] == res[1][1]:
                    skipped_ret += 1
                    res = (('ret', res[0][1]), ('ret', res[1][1]))
            p.same(op, res, ctx)
            w.compare(ctx, extra=extra, stdat=stdat)
            w.rand_state(ctx)
            counts[op] = counts.get(op, 0) + 1
            total += 1
    print('pathfind        %d operations on 9 levels  same (%s%s)' % (total, ', '.join('%s %d' % kv for kv in sorted(counts.items())),
                                                                  ('; %d beeline returns on a drop not compared: uninitialised danger' % skipped_ret) if skipped_ret else ''))
    print('all the same')


if __name__ == '__main__':
    main_wrap('pathfind', main)
