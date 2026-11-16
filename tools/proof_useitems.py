"""proof_useitems.py: USEITEMS.C (seg040_352B: using things: doors, locks and keys, lights, wands, food, books,
scrolls, levers and switches, containers, magic items, the unique objects) proved beside the rules core, on the
objects of the nine real levels. Data from the game's files on both sides (init_objects, init_strings); the player
record (PlayerDat, through player) with random skills and state; the clock, the input mode and the object in hand
random. UseObj with who the player on every kind of object, and the handlers it does not reach on their own: the
doors (OpenDoor, CloseDoor, ToggleDoor, checkLock), the "use on" handlers with a random tool, DumpTheBag, the spell
an object holds (decode_obj_spell, remove_spell, useNSpellCharges), place_new, using_punt. After every call: the
returns, the messages printed and every other outside call, rand's state, the level block, the player record and
the file's own data.

    python tools/proof_useitems.py game build/coretest.exe [uses per level]
"""
import os
import random
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coreproof import Differ, Pair  # noqa: E402

ROUTINES = ['UseObj', 'using_punt', 'place_new', 'UseLockpickOn', 'UseKeyOn', 'UseKey', 'UseThing',
            'SpikeDoor_seg040_662', 'SpikeDoor_seg040_352B_6F9', 'UseBonesOn', 'UsePoleOn', 'UseAnvilOn', 'UseUtil',
            'seg040_352B_9EA', 'TybalsOrb_seg040_A12', 'seg040_352B_AFF', 'UseUnique', 'UseLight', 'UseWand', 'UseFood',
            'UseRockHammerOn', 'UseOilOn', 'UseMagic', 'UseBook', 'UseRect', 'checkLock', 'checkSpell', 'checkTrap',
            'moveDoor', 'changeDoor', 'OpenDoor', 'CloseDoor', 'ToggleDoor', 'DumpTheBag', 'UseCont', 'BlastFunction',
            'decode_obj_spell', 'remove_spell', 'useNSpellCharges']
PLAYERDAT = 0xD2  # sizeof (struct Player): PlayerDat, PLAYER.DAT's record as the game holds it


def obj_off(i):
    return 0x4000 + i * 27 if i < 256 else 0x5b00 + (i - 256) * 8


def in_use(block):
    """(index, tile) of each object in use: on a tile's list or inside something that is"""
    out, seen = [], set()
    for t in range(4096):
        todo = [struct.unpack_from('<H', block, t * 4 + 2)[0] >> 6]
        while todo:
            i = todo.pop()
            while i and i not in seen and len(seen) < 1100:
                seen.add(i)
                out.append((i, t))
                w0, w3 = struct.unpack_from('<H', block, obj_off(i))[0], struct.unpack_from('<H', block, obj_off(i) + 6)[0]
                if not w0 & 0x8000 and w3 >> 6:
                    todo.append(w3 >> 6)
                i = struct.unpack_from('<H', block, obj_off(i) + 4)[0] >> 6
    return out


def main():
    game, coretest = sys.argv[1], sys.argv[2]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 150
    p = Pair(game, coretest)
    p.coverage()
    p.files()
    p.same('init_objects', p.call('init_objects', [], 'w'))
    p.same('init_strings', p.call('init_strings', [], 'b'))
    p.p('inplist', 0, 'S+0', near=True)
    p.p('player', 0, '@PlayerDat', near=True)
    rng = random.Random(352)
    counts = {}

    def check(fn, res, ctx):
        p.same(fn, res, ctx)
        p.same_bytes('L', 0, 0x7c08, ctx)
        p.same('rand', p.gseed(), ctx)
        p.same_bytes('PlayerDat', 0, PLAYERDAT, ctx)
        p.same_bytes('nextSpellTime', 0, 4, ctx)
        p.same_bytes('curvrad', 0, 4, ctx)  # the light the player carries
        p.same('CursorObjPtr', p.q('CursorObjPtr'), ctx)
        counts[fn] = counts.get(fn, 0) + 1

    for level in range(1, 10):
        block = p.level(level)
        p.w('PlayerLevel', 0, struct.pack('<h', level))
        p.p('playerdat', 0, '@Creature', near=True)
        for k in range(per):
            block = p.e.read(0xC000, 0, 0x7c08)
            objs = in_use(block)
            if not objs:
                break
            op = rng.choice(['use'] * 6 + ['door', 'door', 'lock', 'on', 'spell', 'bag', 'punt', 'new', 'direct'])
            doors = [(i, t) for i, t in objs if struct.unpack_from('<H', block, obj_off(i))[0] >> 4 & 0x1f == 0x14]
            # not the shrine (0x157): using it waits for a typed mantra (the input front end's, not a stub's)
            objs = [(i, t) for i, t in objs if struct.unpack_from('<H', block, obj_off(i))[0] & 0x1ff != 0x157]
            i, t = rng.choice(doors if op in ('door', 'lock') and doors else objs)
            o = 'L+%x' % obj_off(i)
            ctx = 'level %d use %d object %d (item %x)' % (level, k, i, struct.unpack_from('<H', block, obj_off(i))[0] & 0x1ff)
            p.seed(rng.getrandbits(32))
            p.time(rng.choice([0, rng.randint(0, 1 << 20), rng.getrandbits(32)]))
            # the player: random skills and state (PLAYER.DAT's first bytes: name, then the stats and skills)
            p.w('PlayerDat', 0x1e, bytes(rng.randint(0, 30) for _ in range(0x40)))
            p.w('S', 0, struct.pack('<5h', 0, 0, 0, 0, rng.choice([1, 1, 1, 2, 4])))  # inplist: mode
            p.w('MapObj_X', 0, struct.pack('<h', t % 64))
            p.w('MapObj_Y', 0, struct.pack('<h', t // 64))
            p.p('CursorObjPtr', 0, '0')
            p.mouse(rng.randint(0, 319), rng.randint(0, 199))
            how = rng.randint(0, 1)
            if struct.unpack_from('<H', block, obj_off(i))[0] >> 4 & 0x1f == 0x08:
                how = 0  # a container in the world is emptied (how 1 opens one in the inventory panel)
            ctx += ' ' + op
            if os.environ.get('PROOF_TRACE'):
                print(ctx, flush=True)
            major = struct.unpack_from('<H', block, obj_off(i))[0] >> 6 & 7
            if op == 'use':
                check('UseObj', p.call('UseObj', [('f', 'L+401b'), ('f', o), ('w', how)], 'f'), ctx)
            elif op == 'door' and major == 5:
                fn = rng.choice(['OpenDoor', 'ToggleDoor', 'CloseDoor', 'moveDoor', 'changeDoor'])
                args = [('f', o)] if fn in ('CloseDoor', 'moveDoor', 'changeDoor') else [('f', 'L+401b'), ('f', o)]
                check(fn, p.call(fn, args, None), ctx)
            elif op == 'lock' and major == 5:
                check('checkLock', p.call('checkLock', [('f', 'L+401b'), ('f', o), ('w', rng.randint(0, 0x3f))], 'w'), ctx)
            elif op == 'on':
                fn = rng.choice(['UseLockpickOn', 'UseKeyOn', 'UseBonesOn', 'UsePoleOn', 'UseAnvilOn', 'UseRockHammerOn',
                                 'UseOilOn', 'SpikeDoor_seg040_352B_6F9', 'TybalsOrb_seg040_A12'])
                args = [('f', o)] + ([] if fn == 'UsePoleOn' else [('w', how)])
                if fn in ('UseAnvilOn', 'UseRockHammerOn', 'UseOilOn', 'TybalsOrb_seg040_A12'):
                    args.append(('w', rng.randint(0, 1)))
                # the tool in hand (UseThing's ObjectActing): a random object of the level
                p.p('ObjectActing', 0, 'L+%x' % obj_off(rng.choice(objs)[0]))
                check(fn, p.call(fn, args, None), ctx)
                p.p('ObjectActing', 0, '0')
            elif op == 'spell':
                fn = rng.choice(['decode_obj_spell', 'remove_spell', 'useNSpellCharges', 'checkSpell'])
                if fn == 'decode_obj_spell':
                    p.w('S', 0x40, b'\0' * 8)
                    res = p.call(fn, [('f', o), ('n', 'S+40'), ('n', 'S+42'), ('n', 'S+44')], 'b')
                    check(fn, res, ctx)
                    p.same('decoded', p.r('S', 0x40, 6), ctx)
                elif fn == 'checkSpell':
                    check(fn, p.call(fn, [('w', t % 64), ('w', t // 64), ('f', 'L+401b'), ('f', o), ('w', how)], 'b'), ctx)
                else:
                    check(fn, p.call(fn, [('f', o)], None), ctx)
            elif op == 'bag':
                check('DumpTheBag', p.call('DumpTheBag', [('f', o), ('w', 0)], None), ctx)
            elif op == 'punt':
                check('using_punt', p.call('using_punt', [('f', o), ('w', 0), ('w', how)], 'w'), ctx)
            elif op == 'direct':
                # the handlers the "use on" cursor calls back (UseThing's fn, the missile aim's ObjectActor)
                fn = rng.choice(['SpikeDoor_seg040_662', 'seg040_352B_9EA', 'seg040_352B_AFF', 'BlastFunction'])
                ctx += ' ' + fn
                if fn == 'SpikeDoor_seg040_662':
                    d = [x for x in objs if struct.unpack_from('<H', block, obj_off(x[0]))[0] >> 4 & 0x1f == 0x14]
                    if d:
                        p.p('ObjectActing', 0, o)  # the spike, used up when it works
                        check(fn, p.call(fn, [('f', 'L+%x' % obj_off(rng.choice(d)[0]))], None), ctx)
                        p.p('ObjectActing', 0, '0')
                elif fn == 'seg040_352B_9EA':
                    c = [x for x in objs if 2 <= x[0] < 256]
                    if c:
                        check(fn, p.call(fn, [('f', 'L+%x' % obj_off(rng.choice(c)[0])), ('w', rng.randint(1, 8))], 'b'), ctx)
                elif fn == 'seg040_352B_AFF':
                    check(fn, p.call(fn, [('f', o), ('w', how)], None), ctx)
                else:
                    p.p('ObjectActing', 0, 'L+401b')
                    p.w('ObjectActorArg', 0, struct.pack('<h', rng.randint(1, 4)))  # missile spells 1..4
                    check(fn, p.call(fn, [], None), ctx)
                    p.p('ObjectActing', 0, '0')
            elif op == 'new':
                res = p.call('place_new', [('f', '0'), ('w', rng.randint(0, 0x1ff))], 'f')
                check('place_new', res, ctx)
    print('useitems        %d uses on 9 levels  same (%s)' % (sum(counts.values()), ', '.join('%s %d' % kv for kv in sorted(counts.items()))))
    ran, notrun, nosym = p.ran(ROUTINES)
    print('  routines run: %d of %d%s%s' % (len(ran), len(ROUTINES), ('; not run: ' + ', '.join(notrun)) if notrun else '',
          ('; no address: ' + ', '.join(nosym)) if nosym else ''))
    print('all the same')


if __name__ == '__main__':
    try:
        main()
    except Differ as ex:
        print('useitems DIFFER:', ex)
        sys.exit(1)
