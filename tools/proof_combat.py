"""proof_combat.py: COMBAT.C (seg022_230E, where a blow lands, whether it hits, the damage, the player's swing, missiles
striking, critters attacking, the experience for a kill) proved: UW.EXE's routines in the emulator beside the rules
core's, on the nine real levels from LEV.ARK. The class tables (Weapons, Missile, Armor, Creature) are OBJECTS.DAT's, the
critters are the level's own (their AI fields scrambled soundly), the player is put on an open tile near them with a
random record (skills, attributes, easy mode, left hand, poison ...) and real inventory objects (a weapon of the weapon
hand, armour, a shield, the ammunition of a missile weapon), allocated and chained by the proven object routines. Every
routine runs directly with the attack globals set as its callers do, and through its callers (do_attack, the player's
swing as a sequence of player_attack calls driven by the weapon frame and the held key, critter_attack, missile_thwack).
After every call the return, the outside calls (with the printed text and the effects' arguments), Borland rand's state,
the whole level block, the player record, the attack globals and the swing state must be the same.

    python -I tools/proof_combat.py game build/coretest.exe [per level]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coreproof import Differ  # noqa: E402
from cproof_lib import World, main_wrap  # noqa: E402

ROUTINES = ['pickloc', 'set_hitobj', 'find_wall_coll', 'resolve_attack', 'frp_check', 'do_damage', 'do_miss',
            'compute_hitangle', 'do_attack', 'check_ammo', 'GetPlayerWeapon', 'DoPlayerWeapon', 'missile_finish',
            'fin_attack', 'clear_fight_state', 'player_attack', 'missile_thwack', 'critter_attack',
            'seg022_230E_16F5', 'player_killed_a']
OPS = (['pickloc'] * 3 + ['sethit'] * 4 + ['wall'] * 3 + ['resolve'] * 5 + ['frp'] * 6 + ['damage'] * 8 + ['miss'] * 4
       + ['angle'] * 2 + ['attack'] * 10 + ['ammo'] * 3 + ['getweap'] * 3 + ['doweap'] * 4 + ['finish'] + ['fin'] + ['clear']
       + ['swing'] * 10 + ['thwack'] * 6 + ['cattack'] * 10 + ['f16f5'] + ['killed'] * 4)
WEAPON_ITEMS = list(range(0, 15)) + [0x18, 0x19, 0x1a, 0x1b]   # melee weapons, bows and slings
AMMO = list(range(0x10, 0x18))
ARMOUR = {0: [0x2c, 0x2d, 0x2e], 1: [0x20, 0x21, 0x22, 0x23], 2: [0x26, 0x27, 0x28], 3: [0x23, 0x24, 0x25],
          4: [0x29, 0x2a, 0x2b]}
OUT = 'editor_dispatch'  # 192 bytes of DS only a stub uses: GetPlayerWeapon's two out pointers (at 0 and 16)


def main():
    game, coretest = sys.argv[1], sys.argv[2]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 450
    w = World(game, coretest, 71)
    p = w.p
    p.files()
    p.call('init_strings', [], 'b')
    rng = w.rng
    counts, total, active = {}, 0, {}
    trace = os.environ.get('PROOF_TRACE')
    extra = ['weap_frame', ('Inventory', 0x38), 'MapObj_X', 'MapObj_Y', 'PlayerPitch', 'GameInputMode',
             ]
    extra = [e for e in extra if (e[0] if isinstance(e, tuple) else e) in p.data]
    L = lambda o: 'L+%x' % w.obj_off(o)

    def equip(slot, item, ctx):
        """a new static object of item in the player's contents and in Inventory[slot], by the proven routines"""
        r = p.call('Obj_Alloc', [('w', 0)], 'f')
        p.same('Obj_Alloc', r, ctx)
        spec = r[0][0].split()[1]
        if spec == '0':
            return False
        off = int(spec[2:], 16)
        idx = 256 + (off - 0x5b00) // 8
        p.w('L', off, struct.pack('<HHHH', item, 0, rng.randint(8, 63), 0))
        r = p.call('Obj_AddEnd', [('f', 'L+%x' % (0x401b + 6)), ('f', spec)], None)
        p.same('Obj_AddEnd', r, ctx)
        p.w('Inventory', slot * 2, struct.pack('<H', idx << 6))
        return True

    def inv_word(slot):
        return struct.unpack('<H', bytes.fromhex(p.r('Inventory', slot * 2, 2)[0]))[0]

    def setup_equipment(ctx):
        lefty = int(p.r('PlayerDat', 0x64, 1)[0], 16) & 1
        ws = 8 - lefty
        weapon = None
        if not inv_word(ws) and rng.random() < 0.85:
            weapon = rng.choice(WEAPON_ITEMS + [0, 1, 2, 3, 4, 5, 6, 7])
            if equip(ws, weapon, ctx) and weapon >= 0x18 and rng.random() < 0.8:
                if not inv_word(11):
                    equip(11, rng.choice(AMMO) | 0, ctx)
        for slot in range(5):
            if not inv_word(slot) and rng.random() < 0.5:
                equip(slot, rng.choice(ARMOUR[slot]), ctx)
        if not inv_word(7 + lefty) and rng.random() < 0.4:
            equip(7 + lefty, rng.choice([0x3b, 0x3c, 0x3d, 0x3e]), ctx)

    for level in range(1, 10):
        w.level(level)
        blk = w.block()
        for i in range(2, 256):
            o = 0x4000 + i * 27
            idv, = struct.unpack_from('<H', blk, o)
            if idv & 0x1ff in (0x4f, 0x7b, 0x7c, 0x7d, 0x7e):
                p.w('L', o, struct.pack('<H', (idv & 0xfe00) | 0x4e))
        p.w('Inventory', 0, bytes(0x30))
        for n in range(per):
            b = w.block()
            crits = w.critters(b)
            if not crits:
                continue
            op = rng.choice(OPS)
            ci = rng.choice(crits)
            p.seed(rng.getrandbits(32))
            ctx = 'level %d op %d %s crit %d' % (level, n, op, ci)
            if trace:
                print(ctx, flush=True)
            if rng.random() < 0.5:
                w.fill_player()
                w.relevel_state()
            p.w('L', 0x4000 + 27 + 8, bytes([rng.randint(1, 60)]))
            if rng.random() < 0.85:
                w.scramble_critter(ci, hostile=rng.choice([None, True]))
            if not w.player_near(ci, radius=rng.choice([0, 0, 1, 1, 2, 3])):
                continue
            b = w.block()
            setup_equipment(ctx)
            w.set_vars(ci)   # mycst (crit_die reads it), meptr: the last critter the AI worked on
            p.p(OUT, 0, '0', near=True)
            p.p(OUT, 16, '0')
            p.p('UsPtr', 0, 'L+%x' % 0x401b)
            # the attack globals as a caller leaves them
            attacker = rng.choice([1, ci, ci])
            defender = ci if attacker == 1 else rng.choice([1, 1, rng.choice(crits)])
            if attacker == defender:
                defender = 1 if attacker != 1 else ci
            glob = dict(askill=rng.randint(-5, 60), wsize=rng.randint(0, 4), towhere=rng.randint(0, 9), targx=0, targy=0,
                        hitobj=defender, hitloc=rng.randint(0, 3), damage=rng.choice([0, 1, 2, 5, 6, 7, 12, 18, 30, 60]),
                        fromwho=attacker, hitangle=rng.randint(0, 4), criti=rng.randint(0, 1), power=rng.choice([0x80, 0xFF, 0x40, 0, rng.getrandbits(8)]),
                        hitz=rng.randint(0, 100), play_pow=rng.randint(0, 100))
            dx, dy = w.crit_xy(b, defender)
            glob['targx'], glob['targy'] = dx, dy
            for nm, v in glob.items():
                if nm in ('hitangle', 'criti', 'power', 'hitz', 'play_pow'):
                    p.w(nm, 0, bytes([v & 0xff]))
                else:
                    p.w(nm, 0, struct.pack('<h', v))
            p.w('cmbModTH', 0, bytes(rng.randint(0, 255) if rng.random() < 0.3 else rng.randint(0, 6) for _ in range(4)))
            p.w('PlayerPitch', 0, struct.pack('<h', rng.choice([0, 0, rng.randint(-0x2000, 0x2000)])))
            p.w('pQatt', 0, struct.pack('<h', rng.choice([0, -10, 0, 0, 3, -1])))
            p.w('attackKey', 0, struct.pack('<h', rng.choice([-1, -1, 0x34, 0x27, 0x19])))
            p.w('GameInputMode', 0, struct.pack('<h', rng.choice([0, 0, 4, 8])))
            p.w('MapObj_X', 0, struct.pack('<h', dx))
            p.w('MapObj_Y', 0, struct.pack('<h', dy))
            res = None
            if op == 'pickloc':
                res = p.call('pickloc', [('w', rng.randint(0, 127)), ('w', rng.randint(0, 127)), ('w', rng.randint(0, 127)), ('w', rng.randint(0, 127))], 'w')
            elif op in ('sethit', 'wall'):
                w.rand_ppd(ci)
                cx_, cy_ = w.crit_xy(b, attacker)
                ppd = bytearray(bytes.fromhex(p.r('Ppd', 0, 23)[0]))
                struct.pack_into('<hhh', ppd, 0, (cx_ << 3) + rng.randint(0, 7), (cy_ << 3) + rng.randint(0, 7), rng.randint(0, 100))
                ppd[8], ppd[9] = rng.randint(1, 5), (rng.randint(0, 4) * 2 + 1) * 4   # radius and height as resolve_attack makes them
                ppd[0x15] = rng.randint(0, 5)    # count
                ppd[0x16] = rng.randint(0, 3)    # first
                p.w('Ppd', 0, bytes(ppd))
                if op == 'sethit':
                    res = p.call('set_hitobj', [('n', '@Ppd+0')], 'w')
                else:
                    res = p.call('find_wall_coll', [('w', rng.getrandbits(8)), ('w', rng.randint(0, 6)), ('n', '@Ppd+0')], None)
            elif op == 'resolve':
                res = p.call('resolve_attack', [], 'b')
            elif op == 'frp':
                res = p.call('frp_check', [('w', attacker), ('w', defender)], 'w')
            elif op == 'damage':
                res = p.call('do_damage', [('w', rng.choice([4, 4, 4, 1, 2, 0x10, 0x20, 0]))], None)
            elif op == 'miss':
                res = p.call('do_miss', [('w', rng.randint(0, 2))], 'b')
            elif op == 'angle':
                res = p.call('compute_hitangle', [], None)
            elif op == 'attack':
                res = p.call('do_attack', [], 'b')
            elif op == 'ammo':
                res = p.call('check_ammo', [('w', rng.randint(0, 15))], 'w')
            elif op == 'getweap':
                res = p.call('GetPlayerWeapon', [('n', '@' + OUT + '+0'), ('n', '@' + OUT + '+10')], 'w')
                p.same('GetPlayerWeapon', res, ctx)
                p.same('wd', w.q(OUT, 0, True), ctx)
                p.same('weap', w.q(OUT, 16, False, 16), ctx)
            elif op == 'doweap':
                wk = rng.randint(0, 15)
                wo = 0
                lefty = int(p.r('PlayerDat', 0x64, 1)[0], 16) & 1
                iw = inv_word(8 - lefty)
                if iw and rng.random() < 0.8:
                    wo = iw >> 6
                res = p.call('DoPlayerWeapon', [('n', '@Weapons+%x' % (wk * 8)), ('f', L(wo) if wo else '0'), ('w', rng.randint(1, 9))], None)
            elif op == 'finish':
                res = p.call('missile_finish', [], None)
            elif op == 'fin':
                res = p.call('fin_attack', [], None)
            elif op == 'clear':
                res = p.call('clear_fight_state', [], None)
            elif op == 'f16f5':
                res = p.call('seg022_230E_16F5', [], None)
            elif op == 'swing':
                # the player's swing, frame by frame: weap_frame and the held key driven from outside
                p.w('pQatt', 0, struct.pack('<h', 0))
                # held is read before it is set when there is no attack key (STACK_JUNK, the stack's leftovers): a key
                # is always set, and the swing is left when it clears the key
                p.w('attackKey', 0, struct.pack('<h', rng.choice([0x34, 0x27, 0x19])))
                p.w('weap_frame', 0, struct.pack('<h', -1))
                p.w('PlayerDat', 0x5F, bytes([int(p.r('PlayerDat', 0x5F, 1)[0], 16) | 2]))  # drawn
                swing = rng.randint(1, 9)
                steps = [('start', swing)]
                # a missile weapon (bow, sling: fire mode) is never at the strike frame 6: DoPlayerWeapon would take the swing
                # state -1 for an index into swing_kind (before its start), which the original and the host read differently
                lefty_ = int(p.r('PlayerDat', 0x64, 1)[0], 16) & 1
                wobj = inv_word(8 - lefty_) >> 6
                witem = (struct.unpack('<H', bytes.fromhex(p.r('L', w.obj_off(wobj), 2)[0]))[0] & 0x1ff) if wobj else 0
                frames = [0, 1, 2, 3, 3, 3, 4, 5, 7, -1] + ([] if witem >> 4 == 1 else [6, 6])
                steps += [('frame', rng.choice(frames)) for _ in range(rng.randint(2, 14))]
                for k, (kind, val) in enumerate(steps):
                    if kind == 'frame':
                        p.w('weap_frame', 0, struct.pack('<h', val))
                        if val == 6 and rng.random() < 0.7:
                            pass
                    held = rng.random() < 0.6
                    ak = struct.unpack('<h', bytes.fromhex(p.r('attackKey', 0, 2)[0]))[0]
                    if 0 < ak < 0x80:
                        p.p('key_on', 0, 'L+9100')    # the keyboard table (a far pointer): 128 bytes of the free level block
                        p.w('L', 0x9100 + ak, bytes([1 if held else 0]))
                    p.w('PlayerDat', 0xCE, struct.pack('<I', rng.randint(0, 0x30000) if kind == 'start' else
                                                      int.from_bytes(bytes.fromhex(p.r('PlayerDat', 0xCE, 4)[0]), 'little') + rng.choice([0, 3, 9, 20, 40])))
                    r = p.call('player_attack', [('w', val if kind == 'start' else rng.choice([0, 1, 5, 9]))], None)
                    c2 = '%s swing step %d %s %s' % (ctx, k, kind, val)
                    p.same('player_attack', r, c2)
                    w.compare(c2, extra=extra)
                    w.rand_state(c2)
                    counts['swing step'] = counts.get('swing step', 0) + 1
                    if r[0][1] != 'calls 0':
                        active['swing step'] = active.get('swing step', 0) + 1
                    pq = struct.unpack('<h', bytes.fromhex(p.r('pQatt', 0, 2)[0]))[0]
                    ak2 = struct.unpack('<h', bytes.fromhex(p.r('attackKey', 0, 2)[0]))[0]
                    if (kind == 'start' and pq == 0) or ak2 <= 0:
                        break
                res = None
                total += 1
                counts[op] = counts.get(op, 0) + 1
                continue
            elif op == 'thwack':
                mobs = [m for m in w.mobiles(b) if m != 1 and m != defender]
                missile = rng.choice(mobs) if mobs else ci
                x, y = w.crit_xy(b, defender)
                res = p.call('missile_thwack', [('w', attacker), ('f', L(missile)), ('f', L(defender)), ('w', x), ('w', y),
                                                 ('w', rng.choice([1, 3, 8, 15, 40])), ('w', rng.choice([4, 4, 1, 2, 0x10, 0x20]))], None)
            elif op == 'cattack':
                res = p.call('critter_attack', [('f', L(ci)), ('w', rng.randint(0, 8)), ('w', rng.choice([50, 100, 255, rng.getrandbits(8)])),
                                                  ('w', rng.randint(0, 2)), ('w', rng.randint(0, 15))], 'b')
            elif op == 'killed':
                mobs = w.mobiles(b)
                res = p.call('player_killed_a', [('f', L(rng.choice(mobs + crits)))], None)
            if res is None:
                continue
            p.same(op, res, ctx)
            w.compare(ctx, extra=extra)
            w.rand_state(ctx)
            counts[op] = counts.get(op, 0) + 1
            if res[0][0] not in ('ret 0', 'ret -') or res[0][1] != 'calls 0':
                active[op] = active.get(op, 0) + 1
            total += 1
    print('combat          %d operations on 9 levels  same (%s; with a result or outside calls: %s)' % (
        total, ', '.join('%s %d' % kv for kv in sorted(counts.items())), ', '.join('%s %d' % kv for kv in sorted(active.items()))))
    print('all the same')


if __name__ == '__main__':
    main_wrap('combat', main)
