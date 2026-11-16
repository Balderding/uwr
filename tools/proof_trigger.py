"""proof_trigger.py: TRIGGER.C (ovr153: triggers and traps: UseTrigger, SetOffTrap, UseTrap and the per-type trap
actions, the hack traps, removing triggers and traps, the trap a spell lays, wandering monsters and closing doors) proved
beside the rules core, on the nine real levels from LEV.ARK. The triggers and traps are the levels' own (every trigger
with its real chain: damage, teleport, arrows, hacks, terrain changes, spells, object creation, doors, deletion,
inventory checks, wards, variables, text); who sets them off is the player (put on an open tile by the proven
player_setup, with a random record: skills, game variables, quests, hit points), a critter or another object; the
trigger's flags (enchanted, flag 10, flag 11) and the action's kind are random as well as real. Every routine runs
directly (the removals with their real heads, the hack traps with every quality they handle, the wandering monsters
and closing doors on the whole level) and through its callers. After every call: the return, the outside calls
(messages and effects with their arguments), rand's state, the whole level block, the player record, the physics
records and the globals of the traps.

Not run: the crystal ball (do_trap_hack quality 2: it sets the light and renders the camera; the emulator does not
survive it) and the talking door (quality 0x2A: TalkTo of a real conversation needs the conversation front end;
the emulator does not survive it either) and, in the same way, gronkify_talkto_player on a critter with a conversation: all
critters here have conversation 0 (TalkTo prints 'You get no response'), so quality 0x32 finds none and
gronkify_talkto_player runs only that way; the cutscene (0x39) only does what its stand-in records.
The static state of TRIGGER.C (the trigger that is running, the removal state) is not visible by name: it is
compared through what later calls do with it.

    python tools/proof_trigger.py game build/coretest.exe [operations per level]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coreproof import Differ  # noqa: E402
from magic_common import MWorld, run, u16, Skip, obj_off, chain  # noqa: E402

ROUTINES = ['trap_init', 'trap_class_data', 'UseTrigger', 'SetOffTrap', 'UseTrap', 'kill_triggers', 'delete_trap',
            'gronkify_talkto_player', 'do_trap_hack', 'cast_trap_spell', 'trigger_obj_del', 'trap_obj_del',
            'is_this_wandering', 'dont_create_wandering_monster_here', 'check_alert', 'DoWanderingMonsters',
            'DoClosingDoors']
TALK_HACKS = (0x2A, 2)


def all_with_heads(block):
    """(index, tile, head's offset in the block) of every object in use: on a tile's list or inside something"""
    out, seen = [], set()
    for t in range(4096):
        todo = [(u16(block, t * 4 + 2) >> 6, t * 4 + 2)]
        while todo:
            i, head = todo.pop()
            h = head
            while i and i not in seen and len(seen) < 1100:
                seen.add(i)
                out.append((i, t, h))
                o = obj_off(i)
                w0, w3 = u16(block, o), u16(block, o + 6)
                if not w0 & 0x8000 and w3 >> 6:
                    todo.append((w3 >> 6, o + 6))
                h = o + 4
                i = u16(block, o + 4) >> 6
    return out


def main():
    game, coretest = sys.argv[1], sys.argv[2]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 200
    w = MWorld(game, coretest, 153, 'trigger', cover=True)
    p = w.p
    p.code['dont_create_wandering_monster_here'] = p.code['dont_create_wandering_monster_he']   # (symbols.tsv truncates the name)
    rng = w.rng
    counts = {}

    def check(name, res, ctx):
        w.ctx = ctx
        w.check(ctx, res)
        counts[name] = counts.get(name, 0) + 1

    def spec(i):
        return 'L+%x' % obj_off(i)

    def player_record():
        pdv = bytearray(bytes.fromhex(p.r('PlayerDat', 0, 0xd2)[0]))
        for i in range(0x21, 0x35):
            pdv[i] = rng.choice([0, 10, 30, 45, rng.randint(0, 63)])
        struct.pack_into('<I', pdv, 0x65, rng.choice([0, 0xffffffff, rng.getrandbits(32), rng.getrandbits(32) & rng.getrandbits(32)]))
        for i in range(0x70, 0xb0):
            pdv[i] = rng.choice([0, 0, rng.getrandbits(6), 0x3f])
        pdv[0x70 + 0x18] &= 7   # the bullfrog puzzle's square (game variables 24, 25) is 0..7: it addresses an 8 by 8 area
        pdv[0x70 + 0x19] &= 7
        pdv[0x61] &= ~0x0c
        w16 = struct.unpack_from('<H', pdv, 0x5f)[0] & ~0x3c
        struct.pack_into('<H', pdv, 0x5f, w16)
        pdv[0x5e] &= 0xf0
        p.w('PlayerDat', 0, bytes(pdv))

    def setup_who():
        """the player on an open tile (hit points enough to survive a trap), and critter/object choices"""
        t = w.put_player()
        p.w('L', obj_off(1) + 8, bytes([rng.choice([255, 200, 120])]))
        return t

    def critters():
        return [(i, t) for i, t in w.all_objects(mobile=True) if 0x40 <= (w.obj_word(i, 0) & 0x1ff) < 0x7f and i >= 2]

    def pick(objs, by_link=False):
        """an object of a random kind (its item; for a trigger the item of the trap it names), so rare traps come up too"""
        groups = {}
        for i in objs:
            k = w.obj_word(i, 0) & 0x1ff
            if by_link:
                nx = u16(w.block, obj_off(i) + 6) >> 6
                k = (k, w.obj_word(nx, 0) & 0x1ff if nx else 0)
            groups.setdefault(k, []).append(i)
        return rng.choice(groups[rng.choice(sorted(groups))])

    def talkers_off():
        """critters keep no conversation (a death or a hit may start one: TalkTo waits for typed input)"""
        b = w.block
        for i in range(2, 256):
            if b[obj_off(i) + 0x1a]:
                p.w('L', obj_off(i) + 0x1a, bytes(1))

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
            # some creatures are temporary (the wandering ones)
            for i, t in critters():
                if rng.random() < 0.3:
                    o = obj_off(i) + 0x0d
                    p.w('L', o, struct.pack('<H', u16(w.block, o) | 0x100))
            w.refresh()
            for k in range(16):
                if done >= per:
                    break
                done += 1
                allo = all_with_heads(w.block)
                heads = dict((i, (t, h)) for i, t, h in allo)
                trigs = [i for i, t, h in allo if 0x1a0 <= (w.obj_word(i, 0) & 0x1ff) <= 0x1af]
                traps = [i for i, t, h in allo if 0x180 <= (w.obj_word(i, 0) & 0x1ff) < 0x1a0 and (w.obj_word(i, 0) & 0x1ff) != 0x19f]
                op = rng.choice(['usetrig'] * 14 + ['setoff'] * 10 + ['usetrap'] * 8 + ['remove'] * 8 + ['hack'] * 4 + ['castrap'] * 3
                                + ['wander'] * 3 + ['gronk'] * 2 + ['doors'] * 3 + ['alert'] * 2 + ['temp'] * 3 + ['class'] * 3)
                ctx = 'level %d op %d %s' % (level, done, op)
                w.ctx = ctx
                if os.environ.get('PROOF_TRACE'):
                    print(ctx, flush=True)
                try:
                    t = setup_who()
                    player_record()
                    w.set_time(rng.randint(0, 60000))
                    p.seed(rng.getrandbits(32))
                    w.w8('DoAnimO', rng.choice([0, 1]))
                    cs = critters()
                    statics = w.all_objects(mobile=False)
                    px, py = t & 63, t >> 6
                    if op in ('usetrig', 'setoff', 'usetrap') and trigs and rng.random() < 0.6:
                        pass
                    # who sets it off
                    kind = rng.choice(['player'] * 6 + ['critter'] * 3 + ['object'] * 2)
                    if kind == 'critter' and not cs:
                        kind = 'player'
                    # an object that moves onto a trigger's tile is a mobile one (a missile, a thrown thing)
                    mobs = [(i, t2) for i, t2 in w.all_objects(mobile=True) if i >= 2 and not 0x40 <= (w.obj_word(i, 0) & 0x1ff) < 0x80]
                    if kind == 'object' and not mobs:
                        kind = 'player'
                    whoi = 1 if kind == 'player' else rng.choice(cs)[0] if kind == 'critter' else rng.choice(mobs)[0]
                    who = spec(whoi) if whoi != 1 else 'L+401b'
                    if op == 'usetrig':
                        if not trigs:
                            continue
                        tr = pick(trigs, True)
                        # the trigger's flags: real, or random (enchanted 12, flag 10, flag 11)
                        if rng.random() < 0.5:
                            wd = u16(w.block, obj_off(tr))
                            wd = (wd & ~0x1c00) | rng.choice([0, 0x400, 0x800, 0x1000, 0x1400, 0x1800, 0x1c00])
                            p.w('L', obj_off(tr), struct.pack('<H', wd))
                        sub = (w.obj_word(tr, 0) & 0xf)
                        trig_tab = bytes.fromhex(p.r('Triggers', 0, 16)[0])
                        typ = rng.choice([trig_tab[sub]] * 6 + [rng.randint(0, 15), -1, 5])
                        start = rng.choice(['0', '0', spec(rng.choice(statics)[0]) if statics else '0', spec(tr)])
                        if typ >= 0 and kind == 'player' and rng.random() < 0.3:
                            pass
                        # a hack trap that starts a conversation is not run
                        if chain_has_talk(w, tr):
                            continue
                        if teleport_search(w, tr, whoi, level):
                            counts['not run (a teleport would search from an occupied square)'] = counts.get('not run (a teleport would search from an occupied square)', 0) + 1
                            continue
                        w.ctx = ctx = ctx + ' trigger %d chain %s' % (tr, describe(w, tr))
                        if os.environ.get('PROOF_TRACE'):
                            print(ctx, flush=True)
                        check('UseTrigger', p.call('UseTrigger', [('f', who), ('f', start), ('f', spec(tr)), ('w', typ)], 'w'), ctx)
                    elif op == 'setoff':
                        if not traps:
                            continue
                        tr = pick(traps)
                        if chain_has_talk(w, tr):
                            continue
                        if teleport_search(w, tr, whoi, level):
                            counts['not run (a teleport would search from an occupied square)'] = counts.get('not run (a teleport would search from an occupied square)', 0) + 1
                            continue
                        tt = heads[tr][0]
                        x, y = (tt & 63, tt >> 6) if rng.random() < 0.6 else (rng.randint(1, 62), rng.randint(1, 62))
                        if chain_has_talk(w, tr, quals=(0x28,)):
                            x, y = max(4, min(59, x)), max(4, min(59, y))   # the emerald puzzle looks four squares each way
                        start = rng.choice(['0', spec(rng.choice(statics)[0]) if statics else '0'])
                        w.ctx = ctx = ctx + ' trap %d chain %s' % (tr, describe(w, tr))
                        if os.environ.get('PROOF_TRACE'):
                            print(ctx, flush=True)
                        check('SetOffTrap', p.call('SetOffTrap', [('f', who), ('f', start), ('f', spec(tr)), ('w', x), ('w', y)], 'w'), ctx)
                    elif op == 'usetrap':
                        if not traps:
                            continue
                        tr = pick(traps)
                        inm = w.obj_word(tr, 0) & 0x3f
                        if inm in (0, 1, 3, 15) or chain_has_talk(w, tr) or chain_has_talk(w, tr, quals=(), types=(0, 1)):
                            # damage and teleport name who (the trigger that is running: none here), the hacks go through do_trap_hack
                            continue
                        tt = heads[tr][0]
                        x, y = (tt & 63, tt >> 6) if rng.random() < 0.6 else (rng.randint(1, 62), rng.randint(1, 62))
                        if chain_has_talk(w, tr, quals=(0x28,)):
                            x, y = max(4, min(59, x)), max(4, min(59, y))
                        w.ctx = ctx = ctx + ' trap %d chain %s at %d,%d' % (tr, describe(w, tr), x, y)
                        if os.environ.get('PROOF_TRACE'):
                            print(ctx, flush=True)
                        check('UseTrap', p.call('UseTrap', [('f', spec(tr)), ('w', x), ('w', y)], 'w'), ctx)
                    elif op == 'remove':
                        pool = trigs + traps
                        if not pool:
                            continue
                        i = rng.choice(pool)
                        tt, head = heads[i]
                        fn = rng.choice(['delete_trap', 'trap_obj_del', 'trigger_obj_del', 'kill_triggers'])
                        if fn == 'kill_triggers':
                            hs = [h for ii, t2, h in allo if h & 3 == 2 or True]
                            hd = rng.choice([head, tt * 4 + 2, rng.choice(hs)])
                            check(fn, p.call(fn, [('f', 'L+%x' % hd)], None), ctx)
                        elif fn == 'trigger_obj_del':
                            c = [x for x in trigs if u16(w.block, obj_off(x) + 6) >> 6]
                            if not c:
                                continue
                            i = rng.choice(c)
                            tt, head = heads[i]
                            check(fn, p.call(fn, [('f', 'L+%x' % head), ('f', spec(i))], None), ctx)
                        elif fn == 'trap_obj_del':
                            if i in trigs and not u16(w.block, obj_off(i) + 6) >> 6:
                                continue
                            check(fn, p.call(fn, [('f', 'L+%x' % head), ('f', spec(i))], None), ctx)
                        else:
                            if i in trigs:
                                continue
                            check(fn, p.call(fn, [('f', 'L+%x' % head), ('f', spec(i))], None), ctx)
                    elif op == 'hack':
                        hk = [i for i in traps if w.obj_word(i, 0) & 0x3f == 3]
                        if not hk:
                            continue
                        tr = rng.choice(hk)
                        q = rng.choice([w.obj_word(tr, 4) & 0x3f, 3, 4, 5, 0x18, 0x28, 0x29, 0x39, 0x3c, 0x3d, 0x3e, 0x3f, 0x17, 0, 0x32])
                        if q in TALK_HACKS:
                            q = 5
                        ctx += ' quality %x' % q
                        if os.environ.get('PROOF_TRACE'):
                            print(ctx, flush=True)
                        if q == 4 and statics and not u16(w.block, obj_off(tr) + 6) >> 6:
                            # the eight position switch sets the height of the object its link names: give it one
                            ow = u16(w.block, obj_off(tr) + 6)
                            p.w('L', obj_off(tr) + 6, struct.pack('<H', (ow & 0x3f) | (rng.choice(statics)[0] << 6)))
                        wq = u16(w.block, obj_off(tr) + 4)
                        p.w('L', obj_off(tr) + 4, struct.pack('<H', (wq & ~0x3f) | q))
                        tt = heads[tr][0]
                        x, y = (tt & 63, tt >> 6) if rng.random() < 0.7 else (rng.randint(5, 58), rng.randint(5, 58))
                        if q == 0x28:
                            x, y = max(4, min(59, x)), max(4, min(59, y))   # the emerald puzzle looks four squares each way
                        if rng.random() < 0.5:
                            check('do_trap_hack', p.call('do_trap_hack', [('f', spec(tr)), ('w', x), ('w', y)], 'w'), ctx)
                        elif teleport_search(w, tr, whoi, level):
                            counts['not run (a teleport would search from an occupied square)'] = counts.get('not run (a teleport would search from an occupied square)', 0) + 1
                        else:
                            check('SetOffTrap', p.call('SetOffTrap', [('f', who), ('f', spec(rng.choice(statics)[0]) if statics else '0'),
                                                                      ('f', spec(tr)), ('w', x), ('w', y)], 'w'), ctx)
                        p.w('L', obj_off(tr) + 4, struct.pack('<H', (u16(w.block, obj_off(tr) + 4) & ~0x3f) | (wq & 0x3f)))   # the mutation is for this call only (a chain would aim it badly)
                    elif op == 'castrap':
                        check('cast_trap_spell', p.call('cast_trap_spell', [('w', rng.randint(1, 62)), ('w', rng.randint(1, 62)), ('w', rng.choice([9, 9, 10]))], 'w'), ctx)   # the ward and tell traps (a spell lays only the ward)
                    elif op == 'wander':
                        if rng.random() < 0.5:
                            check('DoWanderingMonsters', p.call('DoWanderingMonsters', [('w', rng.choice([0, 1]))], None), ctx)
                        else:
                            ob = rng.choice(cs + statics)[0] if (cs or statics) else 1
                            check('dont_create_wandering_monster_here', p.call('dont_create_wandering_monster_here', [('f', spec(ob))], 'b'), ctx)
                    elif op == 'gronk':
                        if cs:
                            i = rng.choice(cs)[0]
                            gw = u16(w.block, obj_off(i) + 0x0b)
                            p.w('L', obj_off(i) + 0x0b, struct.pack('<H', (gw & 0xfff0) | rng.choice([7, 7, 1, 2, 5])))
                            # hostile and not an ally: TalkTo answers 'no response' (a talk would open the conversation file)
                            aw = u16(w.block, obj_off(i) + 0x0d)
                            p.w('L', obj_off(i) + 0x0d, struct.pack('<H', aw & 0x3fff))
                            p.w('L', obj_off(i) + 0x19, bytes([w.block[obj_off(i) + 0x19] & ~0x40]))
                            check('gronkify_talkto_player', p.call('gronkify_talkto_player', [('f', spec(i)), ('w', 0)], 'b'), ctx)
                    elif op == 'doors':
                        check('DoClosingDoors', p.call('DoClosingDoors', [('w', rng.choice([0, 1]))], None), ctx)
                    elif op == 'alert':
                        check('check_alert', p.call('check_alert', [('w', rng.choice([0, 1])), ('w', rng.randint(0, 63)), ('w', rng.choice([px, rng.randint(0, 63)]))], 'b'), ctx)
                    elif op == 'temp':
                        ob = rng.choice(cs + statics + [(1, 0)])[0] if (cs or statics) else 1
                        check('is_this_wandering', p.call('is_this_wandering', [('w', rng.randint(0, 63)), ('w', rng.randint(0, 63)), ('f', spec(ob))], 'b'), ctx)
                    elif op == 'class':
                        pool = trigs + traps + [x for x, _ in statics[:50]]
                        ob = rng.choice(pool) if pool else 1
                        p.p('ActiveObj', 0, spec(ob))
                        check('trap_class_data', p.call('trap_class_data', [], 'n'), ctx)
                        p.p('ActiveObj', 0, '0')
                except Skip:
                    w.recover()
                    counts['skipped'] = counts.get('skipped', 0) + 1
    total = sum(v for k, v in counts.items() if k != 'skipped')
    print('trigger         %d operations on 9 levels  same (%s)' % (total, ', '.join('%s %d' % kv for kv in sorted(counts.items()))))
    if os.environ.get('PROOF_COV'):
        reach = ['do_teleport', 'find_good_x_and_y', 'whack_thing', 'damage_item', 'trap_fire', 'change_terrain', 'inanimate_spell',
                 'do_spell', 'put_at', 'Obj_Alloc', 'OpenDoor', 'CloseDoor', 'ToggleDoor', 'Obj_FreeLinkChain', 'FindObj',
                 'print_path_to', 'player_did_bad', 'work_bullfrog_tiles', 'emerald_trap', 'eight_pos_switch',
                 'ExplodingBook_ovr107_1259', 'do_arial_talking', 'QuakeTrap_seg008_DE7', 'add_animobj', 'get_string',
                 'skill_check', 'player_grabbed', 'obj_deal', 'CreateObj', 'update_animobj']
        print('  (reached from here: %s)' % ', '.join(p.ran(reach)[0]))
    short = 'dont_create_wandering_monster_he'   # (symbols.tsv truncates the name: the coverage knows it so)
    ran, notrun, nosym = p.ran([short if r.startswith('dont_create') else r for r in ROUTINES])
    ran, notrun, nosym = [[r + 're' if r == short else r for r in l] for l in (ran, notrun, nosym)]
    print('  routines run: %d of %d%s%s' % (len(ran), len(ROUTINES), ('; not run: ' + ', '.join(notrun)) if notrun else '',
          ('; no address: ' + ', '.join(nosym)) if nosym else ''))
    print('all the same')


def describe(w, tr):
    out, cur, seen = [], tr, set()
    while cur and cur not in seen and len(out) < 12:
        seen.add(cur)
        out.append('%x/q%d/o%d' % (w.obj_word(cur, 0) & 0x1ff, w.obj_word(cur, 4) & 0x3f, w.obj_word(cur, 6) & 0x3f))
        cur = u16(w.block, obj_off(cur) + 6) >> 6
    return ' '.join(out)


def teleport_search(w, tr, who, plev):
    """whether the trap chain from tr has a teleport that would search for a free square (find_good_x_and_y) from a
    square where the object does not fit at once (the search reads uninitialised stack on our side: see
    MWorld.ring0_or_solid); who is the object index that sets it off. Runs can_place on both sides as the check."""
    cur, seen = tr, set()
    while cur and cur not in seen and len(seen) < 40:
        seen.add(cur)
        wd = w.obj_word(cur, 0)
        if (wd & 0x1ff) == 0x181:          # TRAP_TELEPORT
            x, y, dest = w.obj_word(cur, 4) & 0x3f, w.obj_word(cur, 6) & 0x3f, w.obj_word(cur, 2) & 0x7f
            if (dest == plev or dest == 0) and x != 0x3f and y != 0x3f and not w.ring0_or_solid(who, x, y):
                return True
        cur = u16(w.block, obj_off(cur) + 6) >> 6
        if cur and (w.obj_word(cur, 0) & 0x1ff) >> 6 != 6:
            break
    return False


def chain_has_talk(w, tr, depth=0, quals=TALK_HACKS, types=()):
    """a trap chain that reaches a hack trap which starts a conversation (do_trap_hack 0x2A, 0x32)"""
    seen = set()
    cur = tr
    while cur and cur not in seen and len(seen) < 40:
        seen.add(cur)
        wd = w.obj_word(cur, 0)
        if (wd & 0x1ff) == 0x183 and (w.obj_word(cur, 4) & 0x3f) in quals:
            return True
        if (wd & 0x1ff) >> 6 == 6 and (wd & 0x3f) in types and (wd & 0x30) == 0:
            return True
        if wd & 0x8000 and (wd & 0x1ff) != 0x183 and False:
            break
        cur = u16(w.block, obj_off(cur) + 6) >> 6
        if cur and (w.obj_word(cur, 0) & 0x1ff) >> 6 != 6:
            break
    return False


if __name__ == '__main__':
    run('trigger', main)
