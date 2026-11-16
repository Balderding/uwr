"""proof_playtime.py: PLAYTIME.C (the player's timed updates: duration_check's slow clock, dispel_spell, DegradeLights,
sink_sink_sink, set_curmagic) and UTIL.C's rollem (the dice), proved beside the rules core, on random sound player
states of the nine real levels (a random record with real active spells and poison, an inventory with lit lights in the
light slots; see proof_bags.py). After every call: the return, the outside calls, rand, the level block, the player record
and every global both sides have.

    python tools/proof_playtime.py game build/coretest.exe [operations per level]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from inv_common import Inv, main_wrap, obj_off, scenarios, set_pn, set_spells, u16  # noqa: E402

ROUTINES = ['dispel_spell', 'duration_check', 'DegradeLights', 'sink_sink_sink', 'set_curmagic', 'rollem']


def main():
    game = sys.argv[1]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 400
    w = Inv(game, 5, 'playtime')
    p, rng = w.p, w.rng

    def setup(w, level, scen):
        set_pn(w)
        # lit lights into the light slots (5..8), by the proven AddToInventory
        for slot in (5, 6, 7, 8):
            if rng.random() < 0.6 and not w.slots()[slot]:
                o = w.new_obj(rng.choice([0x94, 0x95, 0x96, 0x97, 0x93, 0x92]), quality=rng.choice([0, 1, 2, 5, 20, 63]))
                if o:
                    r = p.call('AddToInventory', [('f', o[0]), ('w', slot)], 'b')
                    p.same('AddToInventory', r, w.ctx)
        p.same('FixPlayerEquips', p.call('FixPlayerEquips', [], None), w.ctx)

    def op(w, level, scen, k, ctx):
        kind = rng.choice(['dur', 'dur', 'dur', 'dur', 'degrade', 'degrade', 'sink', 'cur', 'cur', 'dispel', 'roll'])
        if kind in ('dur', 'dispel', 'cur'):
            set_spells(w, shrooms=rng.choice([0, 0, 1, 2]))
        # the player stays alive: his death (crit_die on him) reads the critter variables the AI leaves (meptr, mycst: null
        # here, which DOS reads through low memory and the host does not), and is SKILLS.C's player_is_dead's business
        p.w('L', 0x4000 + 27 + 8, bytes([rng.randint(150, 250)]))
        p.w('Creature', 63 * 48 + 4, bytes([255]))
        w.setw('plyregen', rng.choice([0, 0, 1, 2, 3, 22, 23, 47, 200]), '<B', 1)  # plyregen[1], the slow clock
        w.setw('plyregen', rng.choice([0, 0, 1, 2, 3]), '<B', 0)
        p.w('PlayerDat', 0xb9, bytes([rng.choice([0, 0, 0x30, 0x51, 0x60, 0x79, 0x8c])]))  # swim_count
        p.w('PlayerDat', 0x4a, struct.pack('<H', rng.randint(0, 900)))
        p.w('PlayerDat', 0x4c, struct.pack('<H', rng.choice([0, rng.randint(1, 1200)])))
        if w.trace:
            print(ctx, kind, flush=True)
        if kind == 'dur':
            w.check('duration_check', p.call('duration_check', [], None), ctx)
        elif kind == 'degrade':
            amount = rng.choice([0, 1, 2, 5, 180, 900, 1000])
            counter = rng.randint(0, 255)
            w.check('DegradeLights', p.call('DegradeLights', [('w', amount), ('w', counter)], 'b'), ctx + ' amount %d counter %d' % (amount, counter))
        elif kind == 'sink':
            w.check('sink_sink_sink', p.call('sink_sink_sink', [], None), ctx)
        elif kind == 'cur':
            cls, sub = rng.choice([(0, 3), (1, 2), (2, 4), (3, 1), (11, 2), (11, 0), (0, 5)])
            stab = rng.choice([0, 1, 0x40, 0x80])
            w.check('set_curmagic', p.call('set_curmagic', [('w', cls), ('w', sub), ('w', stab)], 'b'), ctx + ' %d %d %d' % (cls, sub, stab))
        elif kind == 'dispel':
            n = struct.unpack('<H', bytes.fromhex(p.r('PlayerDat', 0x5f, 2)[0]))[0] >> 6 & 15
            if n:
                p.w('S', 0, struct.pack('<h', rng.randint(0, n - 1)))
                w.check('dispel_spell', p.call('dispel_spell', [('n', 'S+0')], 'b'), ctx)
                p.same('dispel index', p.r('S', 0, 2), ctx)
        else:
            dice, sides = rng.randint(0, 6), rng.randint(0, 25)
            w.check('rollem', p.call('rollem', [('w', dice), ('w', sides)], 'w'), ctx)

    scenarios(w, per, op, setup=setup)
    w.summary('playtime', ROUTINES)


if __name__ == '__main__':
    main_wrap('playtime', main)
