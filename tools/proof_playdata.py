"""proof_playdata.py: PLAYDATA.C (ovr133: what is worked out again from the player's equipment and active spells:
FixPlayerEquips, player_affected_by, parse_spells, parse_aspells, armor_val, init_spells, set_maze, set_drugged) proved
beside the rules core, on random sound player states of the nine real levels (a random record with real active spells and
poison; equipment in every slot by the proven AddToInventory, enchanted worn items carrying real spell objects; lefty or
not; lit lights; see proof_bags.py). After every call: the return, the outside calls, rand, the level block, the player
record and every global both sides have (the armour in the player's creature record, the resistances, stealth, motion bits,
protection, the light, the weapon's animation...).

Not proved: save_player_data and read_player_data (PLAYER.DAT through xorwrite and xorread: the core's loops there do not
end, see proof_invsave.py), swap_tmap (loads the floor textures into EMS buffers the proofs do not have; so the maze
spell is not cast on level 7) and set_drugged's mushroom effects past the first (random_light copies 4 KB into the
renderer's light table, which the proofs give 64 bytes).

    python tools/proof_playdata.py game build/coretest.exe [operations per level]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from inv_common import Inv, main_wrap, obj_off, scenarios, set_pn, set_spells  # noqa: E402

ROUTINES = ['init_spells', 'set_drugged', 'player_affected_by', 'parse_aspells',
            'set_maze', 'parse_spells', 'armor_val', 'FixPlayerEquips']
ARMOUR = {0: [0x2c, 0x2d, 0x2e, 0x2f], 1: [0x20, 0x21, 0x22, 0x23], 2: [0x26, 0x27, 0x28], 3: [0x23, 0x24, 0x25],
          4: [0x29, 0x2a, 0x2b], 9: [0x38, 0x39, 0x3a, 0x3b], 10: [0x38, 0x39, 0x3a]}
# CORE DIFFERENCE (reported): player_affected_by's class 12 (an enchanted item's own protection) with slot above 4 has
# slots[0] = 0, slots[1] = 1 and loops `for (; slots[i] != -1 && i < 2; i++)`: the read of slots[2], past the 2 element array,
# is undefined behaviour, so clang -O2 drops the i < 2 test and the loop runs on over the stack's garbage (cmbModTH and
# armour written at garbage indexes). Left out here (UW1_PROOF_UB=1 puts it back) until tools/core.py PATCHES the loop to
# `i < 2 && slots[i] != -1`.
UB = bool(os.environ.get('UW1_PROOF_UB'))
SPELL_SUBS = {0: range(1, 8), 1: range(1, 7), 2: range(1, 16), 3: range(1, 10), 11: (0, 1, 2, 3, 14, 15), 12: range(0, 16)}


def main():
    game = sys.argv[1]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 500
    w = Inv(game, 133, 'playdata')
    p, rng = w.p, w.rng
    state = {'shroom': 0}

    def equip(slot, item, spell=None):
        if w.slots()[slot]:
            return
        o = w.new_obj(item, quality=rng.choice([0, 5, 20, 40, 63, rng.getrandbits(6)]))
        if not o:
            return
        if spell is not None:
            sp = w.new_obj(0x120 + rng.randint(0, 15), quality=rng.randint(1, 63), qty=spell)  # is_quant: its link word is the spell, not a contents list
            if sp:
                w.add_end('L+%x' % (int(o[0][2:], 16) + 6), sp[0])
        r = p.call('AddToInventory', [('f', o[0]), ('w', slot)], 'b')
        p.same('AddToInventory', r, w.ctx)

    def setup(w, level, scen):
        set_pn(w)
        lefty = struct.unpack('<B', bytes.fromhex(p.r('PlayerDat', 0x64, 1)[0]))[0] & 1
        for slot, items in ARMOUR.items():
            if rng.random() < 0.6:
                spell = None
                if rng.random() < 0.4:
                    cls = rng.choice([0, 1, 2, 3, 11, 12, 12])
                    if cls == 12 and slot > 4 and not UB:
                        cls = 3
                    sub = rng.choice(list(SPELL_SUBS[cls]))
                    spell = (cls << 4 | sub) & 0x1ff
                equip(slot, rng.choice(items), spell)
        # weapons and shields in the hands, lights on the shoulders
        for slot in (8 - lefty, 7 + lefty):
            if rng.random() < 0.7:
                equip(slot, rng.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0x18, 0x19, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f]))
        for slot in (5, 6):
            if rng.random() < 0.4:
                equip(slot, rng.choice([0x94, 0x95, 0x96, 0x97, 0x93]))

    def op(w, level, scen, k, ctx):
        kind = rng.choice(['fix'] * 6 + ['affected'] * 3 + ['aspells', 'initspells', 'armor', 'armor', 'parse', 'maze', 'drug',
                                                              'reequip'])
        if rng.random() < 0.7:
            set_spells(w)
        p.w('PlayerDat', 0x63, bytes([rng.getrandbits(8)]))
        w.setw('PlayerLevel', level)
        if w.trace:
            print(ctx, kind, flush=True)
        if kind == 'fix':
            if rng.random() < 0.3:
                # the cursor holding a lit light
                o = w.new_obj(rng.choice([0x94, 0x95, 0x96, 0x97]))
                if o:
                    p.p('CursorObjPtr', 0, o[0])
                    w.setw('GameInputMode', 1)
            w.check('FixPlayerEquips', p.call('FixPlayerEquips', [], None), ctx)
            p.p('CursorObjPtr', 0, '0')
        elif kind == 'affected':
            major = rng.choice([0, 1, 2, 3, 11, 12, 13, 5, 6, 7, 8, 10])
            minor = rng.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 14, 15, rng.randint(0, 15)])
            if major == 13 and level == 7:
                major = 0
            if major == 1 and minor == 0:
                minor = 1  # motionbits |= 1 << (minor - 1)
            slot = rng.choice([-1, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10])
            if major == 12 and slot > 4 and not UB:
                slot = rng.choice([-1, 0, 1, 2, 3, 4])
            p.w('S', 0, struct.pack('<H', rng.choice([0, 0, 0x10, 0x30, 0x55, 0xf0, rng.getrandbits(16)])))
            w.check('player_affected_by', p.call('player_affected_by', [('w', major), ('w', minor), ('n', 'S+0'), ('w', slot)], 'b'),
                    ctx + ' major %d minor %d slot %d' % (major, minor, slot))
            p.same('bonuses', p.r('S', 0, 2), ctx)
        elif kind == 'aspells':
            p.w('S', 0, bytes(8))
            w.check('parse_aspells', p.call('parse_aspells', [('n', 'S+0')], None), ctx)
            p.same('icons', p.r('S', 0, 3), ctx)
        elif kind == 'initspells':
            w.check('init_spells', p.call('init_spells', [], None), ctx)
        elif kind == 'armor':
            cands = [i for i in w.carried() if (w.id_at(i) >> 6 & 7) == 0 and w.id_at(i) & 0x3f >= 0x20]
            if cands:
                i = rng.choice(cands)
                w.check('armor_val', p.call('armor_val', [('f', 'L+%x' % obj_off(i))], 'w'), ctx)
        elif kind == 'parse':
            bon = rng.choice([0, 1, 2, 4, 8, 0xf, 0x30, 0xf0, rng.getrandbits(16)])
            p.w('MazeSpell', 0, bytes([rng.choice([0, 0, 1]) if level != 7 else 0]))
            w.check('parse_spells', p.call('parse_spells', [('w', bon)], None), ctx + ' bonuses %x' % bon)
        elif kind == 'maze':
            if level != 7:
                on = rng.choice([0, 1])
                w.check('set_maze', p.call('set_maze', [('w', on)], None), ctx)
        elif kind == 'drug':
            if state['shroom'] == 0:
                state['shroom'] = 1
                w.check('set_drugged', p.call('set_drugged', [('w', 1)], None), ctx + ' on (the first time ever)')
            else:
                w.check('set_drugged', p.call('set_drugged', [('w', 0)], None), ctx + ' off')
        else:
            # take a worn piece off and put another on (the proven routines), then work it out
            slot = rng.choice(list(ARMOUR))
            inv = w.slots()
            if inv[slot]:
                r = p.call('InvRemoveObject', [('f', 'L+%x' % obj_off(inv[slot]))], 'b')
                p.same('InvRemoveObject', r, ctx)
            equip(slot, rng.choice(ARMOUR[slot]))
            w.check('FixPlayerEquips', p.call('FixPlayerEquips', [], None), ctx)

    scenarios(w, per, op, setup=setup)
    w.summary('playdata', ROUTINES)


if __name__ == '__main__':
    main_wrap('playdata', main)
