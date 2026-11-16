"""proof_crpages.py: CRPAGES.C (ovr113, the critter art page cache) proved: UW.EXE's routines in the emulator beside the
rules core's, on random but sound caches (a number of EMS pages, a random set of loaded fragments with their ages, the
page tables kept consistent). preload_cr reads CRIT/ASSOC.ANM and the 32 critters' page headers: the emulator's file
calls are given the game's real files by Pair.files(), as the core's glue gives its own. After every call the return, the outside calls, Borland rand's state, the tables (CmapCache, CmaptoPg,
PgtoCmap, CmapFrm, ASSOC.ANM's buffer), the page out array and the lent workspace globals must be the same.

    python -I tools/proof_crpages.py game build/coretest.exe [count]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coreproof import Differ  # noqa: E402
from cproof_lib import World, main_wrap  # noqa: E402
from emu import DGROUP  # noqa: E402

ROUTINES = ['preload_cr', 'NightCleanCritPages', 'punt_idle_crpage', 'ovr113_2A2', 'swap_ws_out', 'unswap_ws']


def main():
    game, coretest = sys.argv[1], sys.argv[2]
    count = int(sys.argv[3]) if len(sys.argv) > 3 else 1500
    w = World(game, coretest, 9)
    p = w.p
    p.e.hook(*p.code['movedata'], lambda e: 0)   # a copy of a segment onto itself (see below): nothing to do
    p.files()  # the game's files for both sides (preload_cr reads CRIT/ASSOC.ANM and the page headers)
    rng = w.rng
    counts = {}
    names = ['CmapCache', 'CmaptoPg', 'PgtoCmap']

    def rnd_cache():
        nl = rng.choice([2, 4, 8, 16, 24, 40, 64])
        loaded = rng.randint(0, nl)
        pages = rng.sample(range(nl), loaded)
        cache = bytearray([0xFF] * 0x80)
        topg = bytearray([0xFF] * 0x80)
        tocmap = bytearray([0xFE] * nl + [0xFF] * (0x80 - nl))
        frags = rng.sample(range(0x80), loaded)
        for f, pg in zip(frags, pages):
            cache[f] = rng.choice([1, 1, 2, 3, 5, 9, 17, 100, 254])
            topg[f] = pg
            tocmap[pg] = f
        w.p.w('crit_nlpages', 0, struct.pack('<H', nl)) if 'crit_nlpages' in p.data else w.wfar('crit_nlpages', 0, struct.pack('<H', nl))
        w.wfar('CmapCache', 0, bytes(cache))
        w.wfar('CmaptoPg', 0, bytes(topg))
        w.wfar('PgtoCmap', 0, bytes(tocmap))
        w.wfar('crit_fpage', 0, struct.pack('<H', rng.randint(0, 0x2000)))
        return nl

    for n in range(count):
        nl = rnd_cache()
        w.wfar('seg051_C10F', 0, bytes(rng.getrandbits(8) for _ in range(0x80)))
        w.wfar('CmapFrm', 0, bytes(rng.getrandbits(8) for _ in range(0x60)))
        # swap_ws_out and unswap_ws movedata 64 KB from the workspace segment to the EMS frame and back: real copies on
        # both sides, through DOS memory in the emulator and the far-pointer map of the glue in the core, which are not
        # one memory; a copy of a segment onto itself is the same on both, and cannot leave the emulator's memory
        seg = rng.randint(0x1000, 0x9000)
        p.w('conv_ws_seg', 0, struct.pack('<H', seg))
        p.w('ems_frame', 0, struct.pack('<H', seg))
        p.w('Inventory', 0, bytes(rng.getrandbits(8) for _ in range(16)))
        p.seed(rng.getrandbits(32))
        op = rng.choice(['preload', 'night', 'punt', 'punt', 'two', 'two', 'swap', 'swap', 'unswap'])
        ctx = 'op %d %s' % (n, op)
        if op == 'preload':
            res = p.call('preload_cr', [('w', rng.choice([0, 1, 1]))], 'b')
        elif op == 'night':
            res = p.call('NightCleanCritPages', [], None)
        elif op == 'punt':
            res = p.call('punt_idle_crpage', [], 'w')
        elif op == 'two':
            res = p.call('ovr113_2A2', [('w', rng.choice([0, 1, 2, 3, 5, nl, nl + 1])), ('n', '@Inventory+0')], 'w')
        elif op == 'swap':
            res = p.call('swap_ws_out', [], 'w')
        else:
            res = p.call('unswap_ws', [('w', rng.getrandbits(16))], None)
        p.same(op, res, ctx)
        mine, ours = w.read_many([(nm, 0, 0x80) for nm in names] + [('CmapFrm', 0, 0x60), ('seg051_C10F', 0, 0x80),
                                                                   ('Inventory', 0, 16), ('crit_nlpages', 0, 2),
                                                                   ('conv_ws_seg', 0, 2), ('ems_frame', 0, 2)])
        for nm, a, b in zip(names + ['CmapFrm', 'seg051_C10F', 'Inventory', 'crit_nlpages', 'conv_ws_seg', 'ems_frame'], mine, ours):
            if a != b:
                raise Differ('%s differs (%s):\n  original %s\n  ours     %s' % (nm, ctx, a, b))
        w.rand_state(ctx)
        counts[op] = counts.get(op, 0) + 1
    print('crpages         %d operations  same (%s)' % (count, ', '.join('%s %d' % kv for kv in sorted(counts.items()))))
    print('all the same')


if __name__ == '__main__':
    main_wrap('crpages', main)
