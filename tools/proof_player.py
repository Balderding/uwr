"""proof_player.py: PLAYER.C (ovr134: the player's set-up and the camera) proved beside the rules core: InitPlayerRec,
init_player (the player object, the physics record, the square handler, and the key and mouse bindings: both dispatch
tables are read on both sides and compared entry by entry: handle, key or region, mode mask, argument, and whether a
handler is set), mous_player and demous_player (the 3D view's regions), report_loc and show_version (the messages),
home_cam, move_cam, attach_eye, release_camera, crystal_ball, Vortex_ovr143_E09, chg_plys, RollView_ovr143_F58 and
chg_plyp (the view angles) on random sound player states of the nine real levels (see proof_bags.py). Punt_player_inv is
proved in proof_invsave.py. After every call: the return, the outside calls (the messages with their text), rand, the level
block, the player record, the dispatch tables and every global both sides have.

Not proved: port_pitch_bound (the port's own, not in the original).

    python tools/proof_player.py game build/coretest.exe [operations per level]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from inv_common import DGROUP, Differ, Inv, main_wrap, obj_off, scenarios, set_pn  # noqa: E402

ROUTINES = ['InitPlayerRec', 'init_player', 'mous_player', 'demous_player', 'report_loc', 'show_version', 'home_cam', 'move_cam',
            'attach_eye', 'release_camera', 'crystal_ball', 'Vortex_ovr143_E09', 'chg_plys', 'RollView_ovr143_F58', 'chg_plyp']


def tables(w):
    """(original, ours) the two dispatch tables: [(handle, key or the region's four numbers, mask, argument, handler set)]"""
    p = w.p
    e = p.e
    res = []
    for side in (0, 1):
        out = []
        if side == 0:
            kn = e.rw(DGROUP, p.addr('kdispcnt'))
            ko = e.rw(DGROUP, p.addr('key_dispatch'))
            for i in range(kn):
                a = ko + i * 12
                h, key, arg, mask = struct.unpack('<hhHh', e.read(DGROUP, a, 8))
                fn = e.rw(DGROUP, a + 8) | e.rw(DGROUP, a + 10)
                out.append(('k', h, key, mask, arg, bool(fn)))
            mn = e.rw(DGROUP, p.addr('mdispcnt'))
            mo = e.rw(DGROUP, p.addr('mous_dispatch'))
            for i in range(mn):
                a = mo + i * 18
                h, lrx, lry, ulx, uly, arg, mask = struct.unpack('<5hHh', e.read(DGROUP, a, 14))
                fn = e.rw(DGROUP, a + 14) | e.rw(DGROUP, a + 16)
                out.append(('m', h, (lrx, lry, ulx, uly), mask, arg, bool(fn)))
        else:
            kn = struct.unpack('<h', bytes.fromhex(p.r('kdispcnt', 0, 2)[1]))[0]
            ka = int(p._cmd('qn key_dispatch 0', True), 16)
            for i in range(kn):
                raw = bytes.fromhex(p._cmd('rm %x 20' % (ka + i * 32), True))
                h, key = struct.unpack_from('<hh', raw, 0)
                arg, = struct.unpack_from('<Q', raw, 8)
                mask, = struct.unpack_from('<h', raw, 16)
                fn, = struct.unpack_from('<Q', raw, 24)
                out.append(('k', h, key, mask, arg & 0xffff, bool(fn)))
            mn = struct.unpack('<h', bytes.fromhex(p.r('mdispcnt', 0, 2)[1]))[0]
            ma = int(p._cmd('qn mous_dispatch 0', True), 16)
            for i in range(mn):
                raw = bytes.fromhex(p._cmd('rm %x 28' % (ma + i * 40), True))
                h, lrx, lry, ulx, uly = struct.unpack_from('<5h', raw, 0)
                arg, = struct.unpack_from('<Q', raw, 16)
                mask, = struct.unpack_from('<h', raw, 24)
                fn, = struct.unpack_from('<Q', raw, 32)
                out.append(('m', h, (lrx, lry, ulx, uly), mask, arg & 0xffff, bool(fn)))
        res.append(out)
    return res


def main():
    game = sys.argv[1]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 400
    w = Inv(game, 134, 'player')
    p, rng = w.p, w.rng
    w.FNPTRS = dict(w.FNPTRS)
    for n in ('mous_dispatch', 'key_dispatch'):
        w.FNPTRS.pop(n, None)
    w.SKIP = set(w.SKIP) | {'mous_dispatch', 'key_dispatch'}
    w.gnames = [(n, s) for n, s in w.gnames if n not in ('mous_dispatch', 'key_dispatch')]
    state = {'init': 0}

    def tcheck(ctx):
        a, b = tables(w)
        if a != b:
            i = next((k for k in range(min(len(a), len(b))) if a[k] != b[k]), min(len(a), len(b)))
            raise Differ('dispatch tables differ at entry %d of %d / %d (%s):\n  original %s\n  ours     %s' % (
                i, len(a), len(b), ctx, a[i:i + 2], b[i:i + 2]))

    def setup(w, level, scen):
        set_pn(w)
        if not state['init']:
            state['init'] = 1
            # the player is set up once, as at start-up (the player name's string is GAMESTRN.C's make_string: its handle
            # is set here, as the proofs have no string heap)
            p.w('player_name_handle', 0, struct.pack('<h', 3))
            w.check('init_player', p.call('init_player', [], None), 'init_player')
            tcheck('init_player')
            p.w('Time' if False else 'PlayerLevel', 0, struct.pack('<h', level))

    def op(w, level, scen, k, ctx):
        kind = rng.choice(['rec', 'rec', 'mous', 'demous', 'loc', 'ver', 'home', 'home', 'move', 'attach', 'attach', 'release',
                           'crystal', 'vortex', 'plys', 'plys', 'roll', 'plyp', 'plyp'])
        w.setw('MoveCamera', rng.choice([0, 0, 1]), '<B')
        for nm in ('campos', 'camang'):
            p.w(nm, 0, struct.pack('<3h', *[rng.randint(-0x7000, 0x7000) for _ in range(3)]))
        w.setw('PlayerPitch', rng.randint(-0x1000, 0x1000))
        w.setw('PlayerBank', rng.randint(-0x400, 0x400))
        w.setw('UsPtr', 0) if False else p.p('UsPtr', 0, rng.choice(['0', 'L+401b']))
        if rng.random() < 0.5:
            p.p('curelem', 0, rng.choice(['0', 'L+%x' % obj_off(rng.randint(1, 255)), 'L+%x' % obj_off(rng.randint(256, 700))]))
        if w.trace:
            print(ctx, kind, flush=True)
        if kind == 'rec':
            p.w('L', 0x4000 + 27, bytes(rng.getrandbits(8) for _ in range(27)))
            w.check('InitPlayerRec', p.call('InitPlayerRec', [], None), ctx)
        elif kind == 'mous':
            left, bot, wid, hgt = rng.randint(0, 40), rng.randint(0, 60), rng.randint(60, 300), rng.randint(40, 150)
            w.check('mous_player', p.call('mous_player', [('w', left), ('w', bot), ('w', wid), ('w', hgt)], None), ctx)
            tcheck(ctx)
        elif kind == 'demous':
            w.check('demous_player', p.call('demous_player', [], None), ctx)
            tcheck(ctx)
        elif kind == 'loc':
            w.check('report_loc', p.call('report_loc', [], None), ctx)
        elif kind == 'ver':
            w.check('show_version', p.call('show_version', [], None), ctx)
        elif kind == 'home':
            idx = rng.choice([0, 1, 2, 5, 17, rng.randint(2, 255), rng.randint(256, 400)])
            w.check('home_cam', p.call('home_cam', [('w', idx)], None), ctx + ' %d' % idx)
        elif kind == 'move':
            w.setw('PWid', rng.randint(50, 300))
            w.setw('PHgt', rng.randint(40, 150))
            p.w('S', 0, struct.pack('<5h', rng.randint(0, 299), rng.randint(0, 149), 0, 0, 1))
            w.check('move_cam', p.call('move_cam', [('w', 0)], None), ctx)
        elif kind == 'attach':
            mode = rng.choice([-1, 0, 1, 2, 3])
            w.check('attach_eye', p.call('attach_eye', [('w', mode)], None), ctx + ' %d' % mode)
        elif kind == 'release':
            w.check('release_camera', p.call('release_camera', [('w', rng.choice([0, 1, rng.randint(2, 255)]))], None), ctx)
        elif kind == 'crystal':
            cands = [i for i in range(256, 700) if w.id_at(i) & 0x1ff]
            if cands:
                i = rng.choice(cands)
                x, y = rng.randint(0, 63), rng.randint(0, 63)
                w.check('crystal_ball', p.call('crystal_ball', [('f', 'L+%x' % obj_off(i)), ('w', x), ('w', y)], None), ctx)
        elif kind == 'vortex':
            # (the vortex divides by the player's distance from (32, 32): kept away from it)
            p.w('PN', 0, struct.pack('<2h', rng.randint(0x300, 0x1000), rng.randint(0x300, 0x1000)))
            w.check('Vortex_ovr143_E09', p.call('Vortex_ovr143_E09', [('w', -1)], None), ctx)
        elif kind == 'plys':
            p.w('S', 0, struct.pack('<h', rng.randint(-0x3000, 0x3000)))
            d, lim = rng.choice([-1, 0, 1, 2]), rng.choice([0, 0x1000, 0x400, 0x3000])
            w.check('chg_plys', p.call('chg_plys', [('n', 'S+0'), ('w', d), ('w', lim)], None), ctx + ' dir %d limit %x' % (d, lim))
            p.same('angle', p.r('S', 0, 2), ctx)
        elif kind == 'roll':
            w.check('RollView_ovr143_F58', p.call('RollView_ovr143_F58', [('w', rng.choice([-1, 0, 1]))], None), ctx)
        else:
            w.check('chg_plyp', p.call('chg_plyp', [('w', rng.choice([-1, 0, 1]))], None), ctx)

    scenarios(w, per, op, setup=setup)
    tcheck('end')
    w.summary('player', ROUTINES)


if __name__ == '__main__':
    main_wrap('player', main)
