"""proof_worldev.py: WORLDEV.C (ovr107: world events: teleports, terrain changes, traps, plot deaths and item repair:
find_good_x_and_y, do_teleport, change_terrain, whack_thing, inanimate_spell, go_fish, work_bullfrog_tiles,
emerald_trap, player_did_bad, eight_pos_switch, ExplodingBook, talking_door_trap, do_arial_talking, remove_whoami,
TyballDeath, death_check, get_rep_diff, do_repair, repair_item, clear_loretry, clear_all_loretries) proved beside the
rules core, on the nine real levels from LEV.ARK. The player is put on an open tile by the proven player_setup (and,
for fishing, facing water), with a random record (skills, weight, game variables); the objects are the level's own
(critters to teleport, hit, kill; objects on the tiles that change) and new ones from the proven Obj_Alloc (weapons
and armour to repair, in the player's inventory or on the floor; the emeralds of the puzzle, the book that explodes,
the move triggers of Tyball's room). Every routine runs directly with sound arguments and through its callers. After
every call: the return, the outside calls (messages with their arguments), rand's state, the whole level block, the
player record, the physics records and the globals the events write (motion_common.World.check and magic_common).

Limits of the harness and of the core, reported with the proof: (1) coretest's call passes at most eight arguments and
change_terrain has nine, so it is run through its callers (UseTrap's terrain trap on a new trap object with random
fields, the eight position switch, the bullfrog: adjust 0, 1 and 3); (2) find_good_x_and_y (and do_teleport, which calls
it) clears only the first nine of the eighteen bytes of its 'visited' array (memset(visited, 0, 9)): the rest is the
stack's old contents, zero in the emulator's harness and the host's own on our side, so the search is run only where
it does not reach them (the object fits on the square itself, or the square is a wall; MWorld.ring0_or_solid); the
test counts the searches it leaves out.

Not run: talking_door_trap (it makes a creature of conversation 0x19 and calls TalkTo, which needs the conversation
front end: the emulator does not survive it) and death_check of conversation 0x16 with the killing blow about to land
(mode 0: TalkTo of a real conversation, the same). repair_item's question to the player (wyorn) is a stand-in on both
sides that answers 0 (the attempt is made).

    python tools/proof_worldev.py game build/coretest.exe [operations per level]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coreproof import Differ  # noqa: E402
from magic_common import MWorld, run, u16, Skip, obj_off, chain  # noqa: E402

ROUTINES = ['find_good_x_and_y', 'do_teleport', 'change_terrain', 'whack_thing', 'inanimate_spell', 'go_fish',
            'work_bullfrog_tiles', 'emerald_trap', 'player_did_bad', 'eight_pos_switch', 'ExplodingBook_ovr107_1259',
            'talking_door_trap', 'do_arial_talking', 'remove_whoami', 'TyballDeath_ovr107_13D1', 'death_check',
            'get_rep_diff', 'do_repair', 'repair_item', 'clear_loretry', 'clear_all_loretries']
TYBALL_ALLIES = [0xDE, 0xD1, 0xDB, 0xD2, 0xDC, 0xD5, 0xD8, 0xD4, 0xD3, 0xDD]


def main():
    game, coretest = sys.argv[1], sys.argv[2]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 200
    w = MWorld(game, coretest, 107, 'worldev', cover=True)
    p = w.p
    rng = w.rng
    counts = {}
    outcomes = {}

    def check(name, res, ctx):
        w.ctx = ctx
        w.check(ctx, res)
        counts[name] = counts.get(name, 0) + 1
        if res is not None:
            k = name + ' ' + res[0][0] + ' ' + (res[0][1].split(' ', 2)[2][:40] if res[0][1].count(' ') > 1 else '')
            outcomes[k] = outcomes.get(k, 0) + 1

    def spec(i):
        return 'L+%x' % obj_off(i)

    # the displacement of move_along at heading h, 0xB ahead (from the original's routine, for fishing)
    aim = {}
    for h in range(256):
        p.w('S', 0, struct.pack('<hh', 0x400, 0x400))
        r = p.call('move_along', [('w', h), ('w', 0xB), ('n', 'S+0'), ('n', 'S+2')], None)
        p.same('move_along', r)
        p.same_bytes('S', 0, 4)
        x, y = struct.unpack('<hh', bytes.fromhex(p.r('S', 0, 4)[0]))
        aim[h] = (x - 0x400, y - 0x400)

    def player_record():
        pdv = bytearray(bytes.fromhex(p.r('PlayerDat', 0, 0xd2)[0]))
        for i in range(0x21, 0x35):
            pdv[i] = rng.choice([0, 10, 30, 45, rng.randint(0, 63)])
        struct.pack_into('<HH', pdv, 0x4a, rng.choice([0, 100, 500, 900, rng.randint(0, 1200)]), rng.choice([0, 300, 800, 1200]))
        for i in range(0x70, 0xb0):
            pdv[i] = rng.choice([0, 0, rng.getrandbits(6), 0x3f])
        pdv[0x70 + 0x18] &= 7   # the bullfrog puzzle's square (game variables 24, 25) is 0..7
        pdv[0x70 + 0x19] &= 7
        pdv[0x61] &= ~0x0c
        w16 = struct.unpack_from('<H', pdv, 0x5f)[0] & ~0x3c
        struct.pack_into('<H', pdv, 0x5f, w16)
        pdv[0x5e] &= 0xf0
        struct.pack_into('<I', pdv, 0xce, rng.getrandbits(24))
        pdv[0xc2:0xca] = bytes(rng.randint(0, 5) for _ in range(8))
        p.w('PlayerDat', 0, bytes(pdv))

    def critters():
        return [(i, t) for i, t in w.all_objects(mobile=True) if 0x40 <= (w.obj_word(i, 0) & 0x1ff) < 0x7f and i >= 2]

    def talkers_off():
        b = w.block
        for i in range(2, 256):
            if b[obj_off(i) + 0x1a]:
                p.w('L', obj_off(i) + 0x1a, bytes(1))

    def alloc(item, quality=None, link=0, quant=False):
        r = p.call('Obj_Alloc', [('w', 0)], 'f')
        p.same('Obj_Alloc', r, w.ctx)
        sp = r[0][0].split()[1]
        if sp == '0':
            return None
        o = int(sp[2:], 16)
        p.w('L', o, struct.pack('<HHHH', item | (0x8000 if quant else 0), rng.getrandbits(7) | (3 << 10 | 3 << 13),
                                (rng.randint(1, 63) if quality is None else quality), link << 6))
        return sp, 256 + (o - 0x5b00) // 8

    def on_tile(item, t, quality=None, end=False):
        r = alloc(item, quality)
        if not r:
            return None
        sp, idx = r
        fn = 'Obj_AddEnd' if end else 'Obj_Add'
        c = p.call(fn, [('f', 'L+%x' % (t * 4 + 2)), ('f', sp)], None)
        p.same(fn, c, w.ctx)
        return idx

    def in_inventory(item, slot=None):
        r = alloc(item)
        if not r:
            return None
        sp, idx = r
        c = p.call('Obj_AddEnd', [('f', 'L+%x' % (0x401b + 6)), ('f', sp)], None)
        p.same('Obj_AddEnd', c, w.ctx)
        if slot is None:
            slot = rng.randint(0, 7)
        p.w('Inventory', slot * 2, struct.pack('<H', idx << 6))
        return idx

    water = {}
    TILE_WALLS = [30, 0, 19, 21, 11, 13, 32, 32, 32, 32, 0, 0, 0, 0, 0, 30]

    def ring0_or_solid(idx, x, y):
        """find_good_x_and_y's search reads its 'visited' array, of which the original clears only the first nine of
        eighteen bytes (memset(visited, 0, 9)): the rest is the stack's old contents, zero in the emulator's harness and
        whatever the host's stack held on our side (a core difference, reported: tools/core.py should zero it). The
        search is run only where it does not reach them: where the object fits on the square itself or the square is a
        wall (no square is queued). Returns whether it is such a case (can_place is proved elsewhere and run here on
        both sides as the check)."""
        t = max(0, min(63, y)) * 64 + max(0, min(63, x))
        if x < 1 or x > 62 or y < 1 or y > 62:
            return False
        typ, hgt, _ = w.tiles[t]
        if typ == 0:
            return True
        item = w.obj_word(idx, 0) & 0x1ff
        z = (hgt << 3) + (4 if TILE_WALLS[typ] & 0x20 else 0)
        r = p.call('can_place', [('w', item), ('w', idx), ('w', x * 8 + 3), ('w', y * 8 + 3), ('w', z), ('w', 0), ('w', 8)], 'b')
        check('can_place', r, w.ctx)
        return r[0][0] == 'ret 1'

    for level in range(1, 10):
        w.level(level)
        done = 0
        episodes = 0
        while done < per:
            episodes += 1
            if episodes > 1:
                w.level(level)
            w.refresh()
            talkers_off()
            p.w('Inventory', 0, bytes(0x30))
            tx = bytes.fromhex(p.r('TxmTerr', 0, 20)[0])
            wfloors = [f for f in range(10) if (struct.unpack_from('<H', tx, f * 2)[0] >> 4) == 1]
            for k in range(26):
                if done >= per:
                    break
                done += 1
                op = rng.choice(['fgx'] * 6 + ['tele'] * 8 + ['terrain'] * 10 + ['whack'] * 6 + ['inan'] * 4 + ['fish'] * 8
                                + ['frog'] * 4 + ['emerald'] * 3 + ['hack'] * 6 + ['tyball'] * 2 + ['death'] * 8 + ['repdiff'] * 4
                                + ['repair'] * 8 + ['repitem'] * 8 + ['lore'] * 5 + ['alllore'] * 1 + ['remwho'] * 2 + ['arial'] * 1)
                ctx = 'level %d op %d %s' % (level, done, op)
                w.ctx = ctx
                if os.environ.get('PROOF_TRACE'):
                    print(ctx, flush=True)
                try:
                    cs = critters()
                    # the player: on an open tile, near a critter or facing water (fishing)
                    t = None
                    facing = None
                    if op == 'fish' and wfloors and rng.random() < 0.8:
                        wt = [tt for tt in range(4096) if (tt & 63) in range(2, 62) and (tt >> 6) in range(2, 62)
                              and w.tiles[tt][0] != 0 and ((struct.unpack_from('<H', w.block, tt * 4)[0] >> 10) & 15) in wfloors]
                        if wt:
                            wtile = rng.choice(wt)
                            wx, wy = wtile & 63, wtile >> 6
                            cand = [o for o in w.open_tiles(3) if abs((o & 63) - wx) <= 2 and abs((o >> 6) - wy) <= 2 and w.tiles[o][1] >= w.tiles[wtile][1]]
                            if cand:
                                t = rng.choice(cand)
                                px, py = (t & 63) * 8 + 4, (t >> 6) * 8 + 4
                                hs = [h for h in range(256) if ((px + aim[h][0]) >> 3, (py + aim[h][1]) >> 3) == (wx, wy)]
                                facing = rng.choice(hs) if hs else None
                    if t is None:
                        t = w.put_player(near=rng.choice(cs)[1]) if cs else w.put_player()
                    else:
                        t = w.put_player(t=t)
                    px, py = t & 63, t >> 6
                    player_record()
                    w.set_time(rng.randint(0, 60000))
                    p.seed(rng.getrandbits(32))
                    p.w('L', obj_off(1) + 8, bytes([rng.choice([255, 200, 120, 60])]))
                    w.w8('DoAnimO', rng.choice([0, 1]))
                    w.w16('PlayerFacing', (facing if facing is not None else rng.randint(0, 255)) << 8)
                    plev = rng.choice([level, level, level, 4, 7, 9])
                    w.w16('PlayerLevel', plev)
                    w.w8('MapObj_X', px)
                    w.w8('MapObj_Y', py)
                    w.refresh()
                    c = rng.choice(cs)[0] if cs else None
                    ct = next((tt for ii, tt in cs if ii == c), None)
                    cx, cy = (ct & 63, ct >> 6) if ct is not None else (px, py)
                    statics = w.all_objects(mobile=False)
                    who = rng.choice(['L+401b', 'L+401b', spec(c) if c else 'L+401b'])
                    if op == 'fgx':
                        pool = [(1 if who == 'L+401b' else (c or 1))] + ([rng.choice(statics)[0]] if statics else [])
                        oi = rng.choice(pool)
                        ob = spec(oi)
                        x, y = rng.choice([(px, py), (cx, cy), (rng.randint(0, 63), rng.randint(0, 63)), (rng.randint(1, 62), rng.randint(1, 62))])
                        clear = rng.choice([0, 0, 0, 1])
                        if not ring0_or_solid(oi, x, y):
                            counts['fgx not run (search reaches uninitialised stack)'] = counts.get('fgx not run (search reaches uninitialised stack)', 0) + 1
                            continue
                        p.w('S', 0x10, bytes(4))
                        fargs = [('f', ob), ('w', x), ('w', y), ('n', 'S+10'), ('n', 'S+12'), ('w', clear)]
                        check('find_good_x_and_y', p.call('find_good_x_and_y', fargs, 'b'), ctx)
                        p.same('nx, ny', p.r('S', 0x10, 4), ctx)
                    elif op == 'tele':
                        x, y = rng.choice([(px, py), (cx, cy), (rng.randint(1, 62), rng.randint(1, 62)), (0x3f, rng.randint(0, 63)), (rng.randint(0, 63), 0x3f), (0x3f, 0x3f)])
                        lev = rng.choice([0, level, level, rng.randint(0, 9), 9])
                        wi = 1 if who == 'L+401b' else (c or 1)
                        if (lev == 0 or lev == plev) and x != 0x3f and y != 0x3f and not ring0_or_solid(wi, x, y):
                            counts['do_teleport not run (search reaches uninitialised stack)'] = counts.get('do_teleport not run (search reaches uninitialised stack)', 0) + 1
                            continue
                        check('do_teleport', p.call('do_teleport', [('f', who), ('w', x), ('w', y), ('w', lev)], 'w'), ctx)
                    elif op == 'terrain':
                        # change_terrain has nine arguments and coretest's call passes eight: it is run by its callers, the
                        # terrain trap (UseTrap's TRAP_CHANGE_TERRAIN: a new trap object with random fields, adjust 0), the
                        # eight position switch and the bullfrog (adjust 1 and 3)
                        x = rng.choice([cx, px, rng.randint(0, 56)])
                        y = rng.choice([cy, py, rng.randint(0, 56)])
                        x, y = min(x, 56), min(y, 56)
                        pos = rng.getrandbits(16)
                        r = alloc(0x185, rng.getrandbits(6), 0)
                        if r:
                            sp, idx = r
                            o = obj_off(idx)
                            p.w('L', o + 2, struct.pack('<H', pos))
                            p.w('L', o + 6, struct.pack('<H', rng.choice([rng.randint(0, 0x2f), 0x3f, rng.randint(0x30, 0x3f)])))
                            ctx += ' fields %x %x' % (pos, u16(w.block, o + 4))
                            w.refresh()
                            check('UseTrap', p.call('UseTrap', [('f', sp), ('w', x), ('w', y)], 'w'), ctx)
                    elif op == 'whack':
                        idx = rng.choice([1, c or 1, c or 1, (rng.choice(statics)[0] if statics else 1)])
                        # a static object is hit by a trap too (damage_item takes any object): only mobile ones here, as in the game
                        if idx >= 256:
                            idx = c or 1
                        dmg = rng.choice([0, 1, 5, 20, 60, 127, -1, -10, -60, -127, 100, 255])
                        check('whack_thing', p.call('whack_thing', [('w', idx), ('w', dmg), ('w', rng.choice([4, 4, 0, 3])), ('w', rng.choice([0, 2]))], 'w'), ctx)
                    elif op == 'inan':
                        traps = [i for i, tt in statics if (w.obj_word(i, 0) & 0x1ff) >> 6 == 6] or [i for i, tt in statics]
                        tr = rng.choice(traps)
                        mj = rng.choice([-1, -1, rng.randint(0, 14), 6, 5, 8, 4, 11])
                        ef = rng.choice([rng.randint(0, 52), rng.randint(1, 4), rng.getrandbits(8)])
                        if mj >= 0:
                            ef = rng.choice([ef & 0x3f, ef, 1, 2, 3, 4])
                            if mj in (6, 7):
                                ef = (ef & 0xc0) | rng.randint(1, 4)
                                if (ef & 0xc0) == 0x40 and (ef & 0x3f) not in (2, 4):
                                    ef ^= 0xc0
                            if mj == 7:
                                ef = (ef & 0xc0) | rng.randint(1, 5)
                                if (ef & 0xc0) == 0x40:
                                    ef ^= 0xc0
                            if mj == 8:
                                ef = rng.choice([1, 4])    # (a creating spell reads the caster's home square: a trap has none)
                                mj = 5
                            if mj == 11:
                                ef = rng.choice([1, 6, 9])
                            if mj == 5:
                                ef = rng.randint(1, 4)
                            if mj == 14 or mj == 13 or mj == 12:
                                ef = rng.choice([1, 2, 3])
                        else:
                            ef = rng.randint(0, 52)
                            if table_class(p, ef) == 8:
                                ef = 1
                        check('inanimate_spell', p.call('inanimate_spell', [('w', px), ('w', py), ('f', spec(tr)), ('f', who), ('w', mj), ('w', ef)], 'w'), ctx)
                    elif op == 'fish':
                        pdv = bytearray(bytes.fromhex(p.r('PlayerDat', 0, 0xd2)[0]))
                        struct.pack_into('<HH', pdv, 0x4a, rng.choice([0, 100, 900, 1190, 5]), rng.choice([200, 1200, 800, 1200, 1200]))
                        p.w('PlayerDat', 0x4a, bytes(pdv[0x4a:0x4e]))
                        check('go_fish', p.call('go_fish', [], 'b'), ctx)
                    elif op == 'frog':
                        mode = rng.choice([0, 1, 2, 3, 4, 4, 5])
                        w.w16('PlayerLevel', rng.choice([4, 4, level]))
                        vars_ = bytes([rng.randint(0, 7), rng.randint(0, 7), rng.choice([0, 1, 2, 5, 0x3f, rng.randint(0, 63)])])
                        p.w('PlayerDat', 0x70 + 0x18, vars_)
                        check('work_bullfrog_tiles', p.call('work_bullfrog_tiles', [('w', mode), ('w', rng.randint(0, 63)), ('w', rng.randint(0, 63))], None), ctx)
                    elif op == 'emerald':
                        ex, ey = rng.randint(4, 59), rng.randint(4, 59)
                        full = rng.random() < 0.6
                        for dx in (-4, 4):
                            for dy in (-4, 4):
                                if full or rng.random() < 0.5:
                                    on_tile(0xA7, (ey + dy) * 64 + ex + dx)
                        traps = [i for i, tt in statics if (w.obj_word(i, 0) & 0x1ff) >> 6 == 6] or [i for i, tt in statics]
                        w.refresh()
                        check('emerald_trap', p.call('emerald_trap', [('f', spec(rng.choice(traps))), ('w', ex), ('w', ey)], None), ctx)
                    elif op == 'hack':
                        traps = [i for i, tt in statics if (w.obj_word(i, 0) & 0x1ff) >> 6 == 6] or [i for i, tt in statics]
                        tr = rng.choice(traps)
                        fn = rng.choice(['player_did_bad', 'eight_pos_switch', 'eight_pos_switch', 'ExplodingBook_ovr107_1259', 'ExplodingBook_ovr107_1259'])
                        ctx += ' ' + fn
                        if os.environ.get('PROOF_TRACE'):
                            print(ctx, flush=True)
                        if fn == 'player_did_bad':
                            check(fn, p.call(fn, [('w', rng.choice([0, 1, 2, 9, 0xd, 0x1b, 0x20, 0x3f, rng.randint(0, 255)]))], None), ctx)
                        elif fn == 'eight_pos_switch':
                            q = rng.choice([3, 4])
                            wq = u16(w.block, obj_off(tr) + 4)
                            p.w('L', obj_off(tr) + 4, struct.pack('<H', (wq & ~0x3f) | q))
                            if q == 4:
                                ow = u16(w.block, obj_off(tr) + 6)
                                p.w('L', obj_off(tr) + 6, struct.pack('<H', (ow & 0x3f) | (rng.choice(statics)[0] << 6)))
                            x, y = rng.choice([(px, py), (cx, cy), (rng.randint(0, 63), rng.randint(0, 63))])
                            check(fn, p.call(fn, [('w', rng.randint(0, 7)), ('f', spec(tr)), ('w', x), ('w', y)], None), ctx)
                        else:
                            if rng.random() < 0.7:
                                # the exploding book (class 4, 1, 4) on the player's square, after him on the list: the book was
                                # there before he came (Obj_Punt(0, book) frees the rest of the list after it, the player too)
                                on_tile(0x114, t, end=True)
                            w.refresh()
                            check(fn, p.call(fn, [('f', spec(tr)), ('w', px), ('w', py)], None), ctx)
                    elif op == 'arial':
                        traps = [i for i, tt in statics]
                        check('do_arial_talking', p.call('do_arial_talking', [('f', spec(rng.choice(traps))), ('w', px), ('w', py)], None), ctx)
                    elif op == 'remwho':
                        if c:
                            check('remove_whoami', p.call('remove_whoami', [('f', spec(c))], 'b'), ctx)
                    elif op == 'tyball':
                        # the allies named by conversation, and move triggers on square 0x17, 0x38
                        for i2 in rng.sample([x for x, _ in cs], min(len(cs), rng.randint(0, 4))):
                            p.w('L', obj_off(i2) + 0x1a, bytes([rng.choice(TYBALL_ALLIES)]))
                        for _ in range(rng.randint(0, 3)):
                            on_tile(rng.choice([0x1a0, 0x1a0, 0x1a1, 0x80]), 0x38 * 64 + 0x17)
                        w.refresh()
                        check('TyballDeath_ovr107_13D1', p.call('TyballDeath_ovr107_13D1', [], None), ctx)
                        talkers_off()
                    elif op == 'death':
                        if not c:
                            continue
                        who_id = rng.choice([0x1b, 0xe7, 0x16, 0x0b, 0x6e, 0x8e, 0x18, 0, 5, 0xff])
                        mode = rng.choice([0, 1])
                        if who_id == 0x16 and mode == 0:
                            mode = 1   # (a killing blow's TalkTo of a real conversation: see above)
                        if who_id == 0xe7 and mode:
                            on_tile(0x1a0, 0x38 * 64 + 0x17)
                            w.refresh()
                        p.w('L', obj_off(c) + 0x1a, bytes([who_id]))
                        # hostile and not an ally: a talk answers 'no response' (a talk would open the conversation file)
                        aw = u16(w.block, obj_off(c) + 0x0d)
                        p.w('L', obj_off(c) + 0x0d, struct.pack('<H', aw & 0x3fff))
                        p.w('L', obj_off(c) + 0x19, bytes([w.block[obj_off(c) + 0x19] & ~0x40]))
                        gw = u16(w.block, obj_off(c) + 0x0b)
                        p.w('L', obj_off(c) + 0x0b, struct.pack('<H', (gw & 0xfff0) | rng.choice([1, 2, 3, 5, 6, 7])))
                        p.w('L', obj_off(c) + 0x12, bytes([rng.randint(0, 255)]))
                        check('death_check', p.call('death_check', [('f', spec(c)), ('w', mode)], 'b'), ctx)
                        p.w('L', obj_off(c) + 0x1a, bytes(1))
                    elif op in ('repdiff', 'repair', 'repitem'):
                        item = rng.choice([rng.randint(0, 0x3f)] * 4 + [rng.randint(0, 0x1ff)])
                        if 0x180 <= item < 0x1c0:
                            item = rng.randint(0, 0x3f)   # (a trap or trigger is never carried: freeing one reads its link's trap)
                        # the player's own repair is of something he carries; a conversation's (who 0) of something on the floor here
                        who_flag = rng.choice([1, 1, 0]) if op == 'repitem' else 0
                        place = 'inv' if who_flag else 'floor'   # (Obj_Punt frees from a tile's list only: a carried object would stay in the pack, freed)
                        idx = in_inventory(item) if place == 'inv' else on_tile(item, t)
                        if idx is None:
                            continue
                        ob = spec(idx)
                        w.refresh()
                        skill = rng.choice([0, 5, 15, 30, 45, 60, rng.randint(0, 63)])
                        if op == 'repdiff':
                            check('get_rep_diff', p.call('get_rep_diff', [('f', ob)], 'w'), ctx)
                        elif op == 'repair':
                            p.w('S', 0x20, bytes(2))
                            check('do_repair', p.call('do_repair', [('f', ob), ('w', skill), ('n', 'S+20')], 'w'), ctx)
                            p.same('time', p.r('S', 0x20, 2), ctx)
                        else:
                            if who_flag == 0 and place == 'floor':
                                tt = [tt for i2, tt in w.all_objects() if i2 == idx]
                                if tt:
                                    w.w8('MapObj_X', tt[0] & 63)
                                    w.w8('MapObj_Y', tt[0] >> 6)
                            check('repair_item', p.call('repair_item', [('f', ob), ('w', skill), ('w', who_flag)], None), ctx)
                    elif op == 'lore':
                        pool = statics + [x for x in w.all_objects(mobile=True)]
                        ii = rng.choice(pool)[0]
                        check('clear_loretry', p.call('clear_loretry', [('f', spec(ii))], 'b'), ctx)
                    elif op == 'alllore':
                        check('clear_all_loretries', p.call('clear_all_loretries', [], None), ctx)
                except Skip:
                    w.recover()
                    counts['skipped'] = counts.get('skipped', 0) + 1
    if os.environ.get('PROOF_COV'):
        for k in sorted(outcomes):
            if k.split()[0] in ('go_fish', 'do_teleport', 'death_check', 'do_repair', 'repair_item', 'get_rep_diff'):
                print('   ', k, outcomes[k])
    total = sum(v for k, v in counts.items() if k != 'skipped')
    print('worldev         %d operations on 9 levels  same (%s)' % (total, ', '.join('%s %d' % kv for kv in sorted(counts.items()))))
    ran, notrun, nosym = p.ran(ROUTINES)
    print('  routines run: %d of %d%s%s' % (len(ran), len(ROUTINES), ('; not run: ' + ', '.join(notrun)) if notrun else '',
          ('; no address: ' + ', '.join(nosym)) if nosym else ''))
    print('all the same')


def table_class(p, idx):
    seg, off = p.code['spells']
    return p.e.read(seg, off + idx * 4, 1)[0] >> 3


if __name__ == '__main__':
    run('worldev', main)
