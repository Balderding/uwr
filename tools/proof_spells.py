"""proof_spells.py: SPELLS.C (seg038_3307: casting: do_spell, the spell table, mana and health, missiles, area spells,
spells on a critter, summoning, detect monsters, the extra spells, the damage an area spell does to a tile) proved
beside the rules core, on the nine real levels from LEV.ARK. The spell table is the game's own (spells[], 53 entries),
the player is put on an open tile near a real critter by the proven player_setup and faces it (or not), with a random
record (level, mana, Casting and Search skills, active spells, runes, game variables); the casters are the player, a
real critter, or a trap (a static object, which casts at inanmMapX, inanmMapY); the targets the player, a critter, an
object or none; some tiles are made anti-magic. Every routine runs directly with sound arguments (every handler with
its real tile, every class and minor of do_spell, every spell number of cast) and through its callers (cast,
do_spell, nail_area, xt_spells ...). After every call: the return, the outside calls (messages and effects with their
arguments), rand's state, the whole level block, the player record, the physics records and every global the magic
writes (motion_common.World.check and magic_common).

Gate travel (Extra spell 10 with a moonstone) is not run: it puts the player on tile (0, 0) by player_setup, whose
terrain check reads the tiles before the map's start (another thing in DOS and on the host). It is reached only with
no moonstone (the message).

    python tools/proof_spells.py game build/coretest.exe [operations per level]
"""
import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coreproof import Differ  # noqa: E402
from magic_common import MWorld, run, u16, Skip, SPELLS_ROUTINES, obj_off, chain  # noqa: E402

ROUTINES = SPELLS_ROUTINES


def main():
    game, coretest = sys.argv[1], sys.argv[2]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 220
    w = MWorld(game, coretest, 38, 'spells')
    p = w.p
    p.coverage()
    rng = w.rng
    seg, off = p.code['spells']
    stab = p.e.read(seg, off, 53 * 4)
    table = [(stab[i * 4] >> 3, stab[i * 4 + 3], struct.unpack_from('<H', stab, i * 4 + 1)[0]) for i in range(53)]  # class, sub, runes
    counts = {}

    def pdw(o, data):
        p.w('PlayerDat', o, data)

    # the displacement of move_along at each heading (from the original's own routine; both sides must agree), for aiming
    aim = {}
    for h in range(256):
        p.w('S', 0, struct.pack('<hh', 0x400, 0x400))
        r = p.call('move_along', [('w', h), ('w', 4), ('n', 'S+0'), ('n', 'S+2')], None)
        p.same('move_along', r)
        p.same_bytes('S', 0, 4)
        x, y = struct.unpack('<hh', bytes.fromhex(p.r('S', 0, 4)[0]))
        aim[h] = (x - 0x400, y - 0x400)

    def check(name, res, ctx):
        w.ctx = ctx
        w.check(ctx, res)
        counts[name] = counts.get(name, 0) + 1

    def crits():
        out = []
        for i, t in w.all_objects(mobile=True):
            it = w.obj_word(i, 0) & 0x1ff
            if 0x40 <= it < 0x7f and i >= 2:
                out.append((i, t))
        return out

    def spec(i):
        return 'L+%x' % obj_off(i)

    def player_record():
        pdv = bytearray(bytes.fromhex(p.r('PlayerDat', 0, 0xd2)[0]))
        pdv[0x37] = rng.choice([0, 3, 6, 12, 30, 60, rng.randint(0, 255)])
        pdv[0x38] = max(pdv[0x37], rng.choice([0, 20, 60, 99, rng.randint(0, 255)]))
        pdv[0x3d] = rng.choice([1, 2, 5, 8, 15, rng.randint(0, 30)])
        pdv[0x21 + 9] = rng.choice([0, 5, 12, 20, 30, 45, rng.randint(0, 63)])
        pdv[0x21 + 11] = rng.choice([0, 10, 30, 45])
        pdv[0x21 + 8] = rng.randint(0, 30)
        n = rng.choice([0, 0, 1, 2, 3])
        for k in range(3):
            cls = rng.choice([0, 1, 1, 2, 3, 11, 0])
            sub = rng.choice([0, 1, 2, 3, 4, 5, 6, 7]) if cls != 11 else rng.choice([0, 1, 2, 3])
            struct.pack_into('<H', pdv, 0x3e + k * 2, cls | (sub << 4) | (rng.randint(1, 40) << 8))
        w16 = struct.unpack_from('<H', pdv, 0x5f)[0]
        w16 = (w16 & ~(0xf << 6)) | (n << 6)
        struct.pack_into('<H', pdv, 0x5f, w16 & ~(3 << 2) | (rng.randint(0, 3) << 2))
        pdv[0x61] &= ~0x0c
        pdv[0x5e] = (pdv[0x5e] & 0xf0) | 0                                           # no moonstone (gate travel: see above)
        pdv[0x44:0x47] = bytes(rng.getrandbits(8) for _ in range(3))
        pdv[0x47:0x4a] = bytes(rng.choice([rng.randint(0, 23), 0x18]) for _ in range(3))
        struct.pack_into('<I', pdv, 0xce, rng.getrandbits(20))
        for i in range(0x70, 0xb0):
            pdv[i] = rng.getrandbits(8) & 0x3f
        pdw(0, bytes(pdv))

    def player_obj(heading=None):
        """the player object's hit points and heading (pos bits 7-9, fine heading b18)"""
        b = w.block
        o = obj_off(1)
        pos = u16(b, o + 2)
        h = rng.randint(0, 255) if heading is None else heading
        pos = (pos & ~0x380) | ((h >> 5) << 7)
        p.w('L', o + 2, struct.pack('<H', pos))
        p.w('L', o + 0x18, bytes([(b[o + 0x18] & 0xe0) | (h & 0x1f)]))
        p.w('L', o + 8, bytes([rng.choice([5, 20, 60, 255, rng.randint(1, 255)])]))

    def scene():
        """the player near a real critter, facing it or not; returns (critter, critter tile) or (None, tile)"""
        cs = crits()
        c = rng.choice(cs) if cs else None
        if c:
            t = w.put_player(near=c[1])
        else:
            t = w.put_player()
        # aim at the critter: a heading whose point four squares ahead is nearest it
        px, py = t & 63, t >> 6
        h = None
        if c and rng.random() < 0.7:
            cx, cy = c[1] & 63, c[1] >> 6
            best = min(range(256), key=lambda k: (px + aim[k][0] / 1.0 - cx) ** 2 + (py + aim[k][1] / 1.0 - cy) ** 2)
            h = best
        player_obj(h)
        return c, t

    for level in range(1, 10):
        w.level(level)
        done = 0
        episodes = 0
        while done < per:
            episodes += 1
            if episodes > 1:
                w.level(level)
            # some anti-magic tiles (door bit 0: bit 14 of the tile's first word)
            for _ in range(rng.choice([0, 20, 400])):
                t = rng.randrange(4096)
                p.w('L', t * 4 + 1, bytes([w.block[t * 4 + 1] | 0x40]))
            w.refresh()
            for k in range(24):
                if done >= per:
                    break
                done += 1
                op = rng.choice(['dospell'] * 14 + ['cast'] * 8 + ['rdospell'] * 6 + ['heal'] * 6 + ['hp'] * 3
                                + ['missile'] * 3 + ['build'] * 2 + ['handler'] * 14 + ['area'] * 6 + ['nail'] * 6
                                + ['create'] * 5 + ['tremor'] * 2 + ['detect'] * 4 + ['objsp'] * 4 + ['xt'] * 8
                                + ['dsq'] * 4 + ['whoami'] * 2 + ['anti'] * 2)
                ctx = 'level %d op %d %s' % (level, done, op)
                w.ctx = ctx
                if os.environ.get('PROOF_TRACE'):
                    print(ctx, flush=True)
                try:
                    c, t = scene()
                    player_record()
                    w.set_time(rng.randint(0, 60000))
                    p.seed(rng.getrandbits(32))
                    w.w8('mspell_mused', rng.choice([0, 0, 3, 9]))
                    w.w16('GameInputMode', rng.choice([0, 0, 0, 0, 1, 4]))
                    px, py = t & 63, t >> 6
                    w.w8('inanmMapX', px if rng.random() < 0.5 else rng.randint(0, 63))
                    w.w8('inanmMapY', py if rng.random() < 0.5 else rng.randint(0, 63))
                    # who casts: the player, a critter, a trap (a static object)
                    statics = [(i, tt) for i, tt in w.all_objects(mobile=False)]
                    traps = [(i, tt) for i, tt in statics if (w.obj_word(i, 0) & 0x1ff) >> 6 == 6] or statics
                    kind = rng.choice(['player'] * 7 + ['critter'] * 2 + ['trap'] * 2)
                    if kind == 'critter' and not c:
                        kind = 'player'
                    if kind == 'trap' and not traps:
                        kind = 'player'
                    who = 'L+401b' if kind == 'player' else spec(c[0]) if kind == 'critter' else spec(rng.choice(traps)[0])
                    tk = rng.choice(['player', 'critter', 'none', 'object'])
                    if tk == 'critter' and not c:
                        tk = 'player'
                    target = {'player': 'L+401b', 'critter': spec(c[0]) if c else '0', 'none': '0',
                              'object': spec(rng.choice(statics)[0]) if statics else '0'}[tk]
                    cx, cy = (c[1] & 63, c[1] >> 6) if c else (px, py)
                    if kind == 'trap':
                        # a creating spell reads the caster's home square, which a static object has no field for
                        # (the next object's bytes: any square): a trap's spell is any other class, so the table's
                        # creating spells and class 8 are given to the player
                        who_c = who
                    src = 1 if kind == 'player' else (c[0] if kind == 'critter' else 0)
                    if op == 'dospell':
                        cls, sub, _ = rng.choice(table)
                        wcall = 'L+401b' if cls == 8 and kind == 'trap' else who
                        # the player's targeting modes (missiles, object spells) wait for a click: GameInputMode set by them
                        check('do_spell', p.call('do_spell', [('w', cls), ('w', sub), ('f', wcall), ('f', target)], 'b'), ctx)
                    elif op == 'rdospell':
                        cls = rng.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15])
                        sub = rng.choice([rng.getrandbits(8), rng.randint(0, 15), rng.randint(1, 6) | rng.choice([0, 0x40, 0x80])])
                        if cls in (6, 7):
                            sub = (sub & 0xc0) | rng.randint(1, 4 if cls == 6 else 5)
                            if (sub & 0xc0) == 0x40 and not (cls == 6 and (sub & 0x3f) in (2, 4)):
                                sub ^= 0xc0   # a random-square area hands the handler no object: only Sheet Lightning and Flame Wind do without
                        if cls == 8:
                            sub = rng.choice([1, 3, 4])
                        if cls == 11:
                            sub = (sub & 0xc0) | rng.choice([0, 1, 2, 3, 4, 5, 6, 8, 9, 11, 12]) if rng.random() < 0.9 else sub & 0xff
                            if (sub & 0x3f) == 10:
                                sub &= ~0x3f
                        if cls == 5:
                            sub = rng.randint(1, 4)
                        if cls == 13:
                            sub = rng.choice([3, 5, 1])
                        wcall = 'L+401b' if cls == 8 and kind == 'trap' else who
                        if cls == 14:
                            sub = rng.choice([1, 2, 3, 0x104 & 0xff])
                        if cls == 0 or cls in (1, 2, 3):
                            sub = (sub & 0xc0) | (sub & 0x3f) % 8
                        check('do_spell', p.call('do_spell', [('w', cls), ('w', sub), ('f', wcall), ('f', target)], 'b'), ctx)
                    elif op == 'cast':
                        s = rng.choice([rng.randint(0, 52)] * 5 + [rng.randint(53, 255)])
                        wcall = 'L+401b' if s < 53 and table[s][0] == 8 and kind == 'trap' else who
                        check('cast', p.call('cast', [('w', s), ('f', wcall), ('f', target)], None), ctx)
                    elif op == 'heal':
                        fn = rng.choice(['healing', 'backfire', 'restore_mana', 'restore_hp', 'get_hp_back'])
                        amount = rng.choice([0, 1, 3, 8, 15, 127, -1, -20, -128, 200])
                        wh = rng.choice([who, 'L+401b', spec(c[0]) if c else 'L+401b'])
                        if fn in ('healing', 'backfire') and wh != 'L+401b' and not c and kind != 'critter':
                            wh = 'L+401b'
                        if fn in ('healing', 'backfire'):
                            amount = rng.choice([0, 1, 3, 8, 15, 5])
                        if fn == 'get_hp_back':
                            amount &= 0xff
                        if fn == 'backfire' and kind == 'trap':
                            wh = 'L+401b'
                        check(fn, p.call(fn, [('f', wh), ('w', amount)], None), ctx)
                    elif op == 'hp':
                        wh = rng.choice(['L+401b', spec(c[0]) if c else 'L+401b'])
                        check('get_hp_back', p.call('get_hp_back', [('f', wh), ('w', rng.randint(0, 255))], None), ctx)
                    elif op == 'missile':
                        wh = rng.choice(['L+401b', spec(c[0]) if c else 'L+401b'])
                        check('release_missile', p.call('release_missile', [('f', wh), ('w', rng.randint(1, 4))], None), ctx)
                    elif op == 'build':
                        tt = rng.randrange(4096) if rng.random() < 0.3 else t
                        r = p.call('build_new_obj', [('w', rng.choice([0x1c5, 0x1c2, rng.randint(0, 0x1ff)])), ('f', 'L+%x' % (tt * 4))], 'f')
                        check('build_new_obj', r, ctx)
                    elif op == 'handler':
                        fn = rng.choice(['sp_true_sight', 'sp_sheet_light', 'sp_meteor', 'sp_ward_undead', 'sp_poison',
                                         'hit_critter_goal', 'sp_charm', 'sp_confusion', 'sp_fear', 'sp_hold'])
                        # a tile of the critter (the handlers act on what is there) or the player's
                        tx = cx if rng.random() < 0.8 else rng.randint(1, 62)
                        ty = cy if rng.random() < 0.8 else rng.randint(1, 62)
                        ctile = 'L+%x' % ((ty * 64 + tx) * 4)
                        tgt = spec(c[0]) if c else 'L+401b'
                        if fn == 'sp_true_sight':
                            check(fn, p.call(fn, [('f', who), ('f', spec(rng.choice(statics + w.all_objects(mobile=True))[0]))], 'b'), ctx)
                        elif fn == 'sp_charm':
                            check(fn, p.call(fn, [('w', tx), ('w', ty), ('f', tgt)], 'b'), ctx)
                        elif fn == 'hit_critter_goal':
                            check(fn, p.call(fn, [('w', rng.choice([1, 2, 3, 5, 6, 7, 8, 9, 0])), ('w', rng.choice([-1, 0, 1, 2, 3])),
                                                  ('f', tgt), ('w', tx), ('w', ty)], 'b'), ctx)
                        else:
                            if fn in ('sp_sheet_light', 'sp_meteor'):
                                tx, ty = max(1, min(62, tx)), max(1, min(62, ty))
                                ctile = 'L+%x' % ((ty * 64 + tx) * 4)
                            check(fn, p.call(fn, [('w', tx), ('w', ty), ('f', tgt), ('f', ctile), ('w', src)], 'b'), ctx)
                    elif op == 'area':
                        fn = rng.choice(['process_area', 'gronk_area'])
                        h = rng.choice(['sp_sheet_light', 'sp_confusion', 'sp_meteor', 'sp_fear', 'sp_ward_undead',
                                        'sp_charm', 'sp_poison', 'sp_hold', 'tremor_area'])
                        typ = rng.choice([0, 0x40, 0x80, 0xc0])
                        if typ == 0x40 and h not in ('sp_sheet_light', 'sp_meteor', 'tremor_area'):
                            typ = rng.choice([0, 0x80, 0xc0])   # a random-square area passes no object: only these handlers do without
                        if fn == 'process_area':
                            x0 = rng.choice([cx - 2, cx - 1, rng.randint(-3, 66)])
                            y0 = rng.choice([cy - 2, cy - 1, rng.randint(-3, 66)])
                            args = [('w', rng.randint(1, 12)), ('w', src), ('f', '@' + h), ('w', typ), ('w', x0), ('w', y0),
                                    ('w', rng.randint(0, 5)), ('w', rng.randint(0, 5))]
                            check('process_area', p.call('process_area', args, None), ctx)
                        else:
                            check('gronk_area', p.call('gronk_area', [('f', who), ('w', rng.randint(1, 8)), ('f', '@' + h), ('w', typ),
                                                                      ('w', rng.randint(0, 6)), ('w', rng.randint(0, 3))], None), ctx)
                    elif op == 'nail':
                        fn = rng.choice(['nail_area', 'nail_1area'])
                        sub = rng.randint(1, 4 if fn == 'nail_area' else 5)
                        fl = rng.choice([0, 0x40, 0x80, 0xc0])
                        if fl == 0x40 and not (fn == 'nail_area' and sub in (2, 4)):
                            fl = 0x80   # a random-square area hands the handler no object: only Sheet Lightning and Flame Wind do without
                        sub |= fl
                        check(fn, p.call(fn, [('f', who), ('w', sub)], None), ctx)
                    elif op == 'create':
                        wh = rng.choice([who if kind != 'trap' else 'L+401b', 'L+401b', 'L+401b'])
                        check('creat_spell', p.call('creat_spell', [('f', wh), ('w', rng.choice([1, 3, 4, 4]))], None), ctx)
                    elif op == 'tremor':
                        tx, ty = max(1, min(62, cx)), max(1, min(62, cy))
                        check('tremor_area', p.call('tremor_area', [('w', tx), ('w', ty), ('f', target), ('f', 'L+%x' % ((ty * 64 + tx) * 4)), ('w', src)], 'b'), ctx)
                    elif op == 'detect':
                        if rng.random() < 0.5:
                            check('print_monster', p.call('print_monster', [('w', rng.randint(0, 7)), ('w', rng.randint(0, 8))], None), ctx)
                        else:
                            check('mdetect', p.call('mdetect', [('w', rng.choice([3, 10, 20, 63])), ('w', rng.choice([0, 20, 45, 63]))], None), ctx)
                    elif op == 'objsp':
                        cs = [(i, tt) for i, tt in statics + w.all_objects(mobile=True)]
                        if cs:
                            w.w16('ObjectActorArg', rng.choice([3, 4, 5, 2, 0]))
                            i, tt = rng.choice(cs)
                            w.w8('MapObj_X', tt & 63)
                            w.w8('MapObj_Y', tt >> 6)
                            p.p('ObjectActing', 0, 'L+401b')
                            check('obj_spells', p.call('obj_spells', [('f', spec(i)), ('w', rng.randint(0, 1)), ('w', 0)], None), ctx)
                    elif op == 'xt':
                        sub = rng.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 12, 13])
                        if sub == 12 and rng.random() < 0.6:
                            sub = rng.choice([1, 6, 9, 11])   # Armageddon empties the level: not every time
                        wh = rng.choice(['L+401b'] * 4 + [who])
                        stabf = rng.choice([0, 0x40, 0x80, 1])
                        check('xt_spells', p.call('xt_spells', [('f', wh), ('w', stabf), ('w', sub)], None), ctx)
                    elif op == 'dsq':
                        tx, ty = max(1, min(62, cx)), max(1, min(62, cy))
                        check('damage_square', p.call('damage_square', [('w', tx if rng.random() < .8 else px), ('w', ty), ('w', rng.choice([0, 1, 2, 2, 3])), ('w', src)], None), ctx)
                    elif op == 'whoami':
                        who_id = rng.choice([0, 1, 5, 0x10, 0x18, 0x1b, 0x6e, 0xd8, rng.randint(0, 255)])
                        # a real critter's conversation number (whoami byte of the object)
                        if c:
                            who_id = rng.choice([who_id, w.block[obj_off(c[0]) + 0x1a]])
                        check('gronk_whoami', p.call('gronk_whoami', [('w', who_id), ('w', rng.randint(0, 1)), ('w', 0), ('f', '@remove_whoami')], None), ctx)
                    elif op == 'anti':
                        check('anti_magic_p', p.call('anti_magic_p', [('w', rng.randint(0, 63)), ('w', rng.randint(0, 63))], 'b'), ctx)
                except Skip:
                    w.recover()
                    counts['skipped'] = counts.get('skipped', 0) + 1
    total = sum(v for k, v in counts.items() if k != 'skipped')
    print('spells          %d operations on 9 levels  same (%s)' % (total, ', '.join('%s %d' % kv for kv in sorted(counts.items()))))
    ran, notrun, nosym = p.ran(ROUTINES)
    print('  routines run: %d of %d%s%s' % (len(ran), len(ROUTINES), ('; not run: ' + ', '.join(notrun)) if notrun else '',
          ('; no address: ' + ', '.join(nosym)) if nosym else ''))
    print('all the same')


if __name__ == '__main__':
    run('spells', main)
