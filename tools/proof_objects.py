"""proof_objects.py: OBJECTS.C (seg027_2861, the object lists) proved: UW.EXE's routines in the emulator beside the
rules core's (underworld-exhumed obj/OBJECTS.C built by tools/core.py), on the nine real levels from LEV.ARK, with
random operations that keep the lists sound (allocating, adding at the head or the end, removing, freeing, punting,
culling, the searches, the active list, the consistency check). After every call the whole level block, the store's
pointers, the return, Borland rand's state and the outside calls must be the same.

    uv run --no-project --with unicorn==2.0.1.post1 python -I tools/proof_objects.py game build/coretest.exe [per level]
"""
import os
import random
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from coreproof import Differ, Pair  # noqa: E402
from emu import DGROUP  # noqa: E402

ROUTINES = ['Map_ObjFix', 'Obj_Check', 'chkTenacious', 'Obj_Elem_Fate', 'Obj_Fate', 'Obj_GarbageCollect', 'Obj_Alloc',
            'Obj_Free', 'Obj_Add', 'Obj_AddEnd', 'Obj_Rem', 'Obj_Punt', 'Obj_FreeChain', 'Obj_FreeLinkChain',
            'Obj_PtrTMem', 'Obj_MemTPtr', 'Obj_IntTMem', 'Obj_Find', 'IsMobElem', 'active_critter', 'free_critter',
            'Obj_InList', 'HasOrIsObj', 'Obj_FindInMap', 'flog_list', 'count_list', 'seg027_2861_EF9', 'Obj_ListOkay',
            'seg027_2861_10E4']
POINTERS = ['critptr', 'objptr', 'LastActiveMob', 'ActiveMob', 'critbot', 'objbot', 'crittop', 'objtop', 'Obj_Find_Head']


def obj_off(i):
    return 0x4000 + i * 27 if i < 256 else 0x5b00 + (i - 256) * 8


def chain(block, link, guard=1100):
    """the indices on a list from a link word's index"""
    out = []
    while link and guard:
        out.append(link)
        link = struct.unpack_from('<H', block, obj_off(link) + 4)[0] >> 6
        guard -= 1
    return out


def main():
    game, coretest = sys.argv[1], sys.argv[2]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 300
    p = Pair(game, coretest)
    p.coverage()
    # calloc and free for Obj_ListOkay: a zeroed block of DGROUP in the emulator (the core has the host's)
    heap = [0xD000]

    def calloc(e):
        n, k = e.arg(0), e.arg(1)
        e.write(DGROUP, heap[0], b'\0' * (n * k))
        return heap[0], 0
    for name, fn in (('calloc', calloc), ('free', lambda e: (0, 0))):
        if name in p.code:
            p.e.hook(*p.code[name], fn)
    rng = random.Random(27)
    counts = {}
    total = 0
    for level in range(1, 10):
        p.level(level)
        # the player somewhere on the map (Obj_GarbageCollect measures from its home tile)
        px, py = rng.randint(0, 63), rng.randint(0, 63)
        p.w('L', 0x4000 + 27 + 0x16, struct.pack('<H', (px << 10) | (py << 4)))
        for name, v in (('GrSq', rng.choice([0, -1])), ('GameInputMode', 0)):
            p.w(name, 0, struct.pack('<h', v))
        p.p('CursorObjPtr', 0, '0')
        p.p('UsPtr', 0, '0')
        p.p('Obj_Find_Head', 0, '0')
        for n in range(per):
            block = p.e.read(0xC000, 0, 0x7c08)
            tiles = [t for t in range(4096) if struct.unpack_from('<H', block, t * 4 + 2)[0] >> 6]
            p.seed(rng.getrandbits(32))
            op = rng.choice(['alloc', 'alloc', 'rem', 'rem', 'punt', 'find', 'inlist', 'fate', 'gc', 'has', 'findmap',
                             'mob', 'ptr', 'okay', 'active', 'freechain', 'fixall', 'tree'] if n else ['okay'])
            t = rng.choice(tiles) if tiles else 0
            head = 'L+%x' % (t * 4 + 2)
            objs = chain(block, struct.unpack_from('<H', block, t * 4 + 2)[0] >> 6) if tiles else []
            res = None
            if os.environ.get('PROOF_TRACE'):
                print(level, n, op, t, objs[:5], flush=True)
                if os.environ['PROOF_TRACE'] == '%d %d' % (level, n):
                    tf = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'work', 'itrace.txt'), 'w')
                    from unicorn.x86_const import UC_X86_REG_SP
                    p.e.uc.hook_add(1 << 2, lambda uc, addr, size, user: (tf.write('%x %x\n' % (addr, uc.reg_read(UC_X86_REG_SP))), tf.flush()))
            if op == 'alloc':
                res = p.call('Obj_Alloc', [('w', rng.randint(0, 1))], 'f')
                spec = res[0][0].split()[1]
                if spec != '0' and spec == res[1][0].split()[1]:
                    off = int(spec[2:], 16)
                    # a fresh kind and place, then into a tile's list at the head or the end
                    item = rng.choice([i for i in range(0x200) if i & 0x1c0 != 0x180]) | rng.choice([0, 0x8000, 0x2000])  # no traps: their links name other traps
                    pos = rng.getrandbits(16)
                    # a sound object: a stack (is_quant) holds a quantity or a special link, anything else nothing yet
                    link = rng.choice([1, 2, 7, 0x200, 0x205]) if item & 0x8000 else 0
                    p.w('L', off, struct.pack('<HHHH', item, pos, rng.getrandbits(6), rng.getrandbits(6) | link << 6))
                    nt = rng.randint(0, 4095)
                    fn = rng.choice(['Obj_Add', 'Obj_AddEnd'])
                    r2 = p.call(fn, [('f', 'L+%x' % (nt * 4 + 2)), ('f', spec)], None)
                    p.same(fn, r2, 'level %d op %d' % (level, n))
            elif op == 'rem' and objs:
                o = rng.choice(objs)
                res = p.call('Obj_Rem', [('f', head), ('f', 'L+%x' % obj_off(o))], None)
                p.same('Obj_Rem', res)
                if rng.random() < 0.5:
                    res = p.call('Obj_FreeLinkChain', [('f', '0'), ('f', 'L+%x' % obj_off(o))], None)
                else:
                    res = p.call('Obj_AddEnd', [('f', 'L+%x' % (rng.randint(0, 4095) * 4 + 2)), ('f', 'L+%x' % obj_off(o))], None)
            elif op == 'punt' and objs:
                o = rng.choice(objs)
                res = p.call('Obj_Punt', [('f', head), ('f', 'L+%x' % obj_off(o)), ('w', rng.randint(0, 1))], 'f')
            elif op == 'freechain' and objs:
                res = p.call('Obj_FreeChain', [('f', head)], None)
            elif op == 'find' and objs:
                idx = rng.choice(objs + [rng.randint(1, 1023)])
                res = p.call('Obj_Find', [('f', head), ('w', rng.randint(0, 1)), ('w', idx)], 'f')
                p.same('Obj_Find', res)
                res = p.q('Obj_Find_Head')
            elif op == 'inlist' and objs:
                p.p('Obj_Find_Head', 0, head)
                res = p.call('Obj_InList', [('n', '@Obj_Find_Head'), ('w', rng.randint(0, 1)), ('w', rng.choice([-1, rng.randint(0, 7)])),
                                             ('w', rng.choice([-1, rng.randint(0, 3)])), ('w', rng.choice([-1, rng.randint(0, 15)]))], 'f')
                p.same('Obj_InList', res)
                res = p.q('Obj_Find_Head')
            elif op == 'fate' and objs:
                o = rng.choice(objs)
                if rng.random() < 0.3:
                    res = p.call('chkTenacious', [('f', 'L+%x' % obj_off(o))], 'b')  # against the cull_range left
                else:
                    res = p.call('Obj_Elem_Fate', [('w', rng.choice([0, 1, 3, 10, rng.randint(0, 20)])), ('f', 'L+%x' % obj_off(o))], 'b')
                if rng.random() < 0.3:
                    p.same('fate', res)
                    res = p.call('Obj_Fate', [('w', rng.randint(0, 12)), ('f', head)], 'b')
            elif op == 'gc':
                res = p.call('Obj_GarbageCollect', [('w', rng.randint(0, 10)), ('w', rng.randint(1, 30))], None)
            elif op == 'has' and objs:
                o = rng.choice(objs)
                item = struct.unpack_from('<H', block, obj_off(rng.choice(objs)))[0] & 0x1ff
                res = p.call('HasOrIsObj', [('f', 'L+%x' % obj_off(o)), ('w', rng.choice([item, rng.randint(0, 511)]))], 'b')
            elif op == 'findmap':
                p.w('Ppd', 0, struct.pack('<hh', rng.randint(0, 64), rng.randint(0, 63)))
                res = p.call('Obj_FindInMap', [('w', rng.randint(-1, 7)), ('w', rng.randint(-1, 3)), ('w', rng.randint(-1, 15)),
                                               ('n', '@Ppd+0'), ('n', '@Ppd+2')], 'f')
                p.same('Obj_FindInMap', res)
                res = p.r('Ppd', 0, 4)
            elif op == 'mob':
                o = rng.choice(objs) if objs else rng.randint(1, 1023)
                res = p.call('IsMobElem', [('f', 'L+%x' % obj_off(o))], 'b')
            elif op == 'ptr':
                o = rng.randint(0, 1023)
                res = p.call('Obj_MemTPtr', [('f', 'L+%x' % obj_off(o) if o else '0')], 'w')
                p.same('Obj_MemTPtr', res)
                res = p.call('Obj_PtrTMem', [('f', head)], 'f')
            elif op == 'okay':
                cursor = None
                if rng.random() < 0.4:
                    # an object on the cursor (off every list), as the inventory has it: count_list counts it
                    r = p.call('Obj_Alloc', [('w', rng.randint(0, 1))], 'f')
                    p.same('Obj_Alloc', r, 'cursor')
                    if r[0][0] != 'ret 0':
                        cursor = r[0][0].split()[1]
                        p.w('L', int(cursor[2:], 16), struct.pack('<HHHH', rng.randint(0, 0x17f), 0, 0, 0))
                        p.p('CursorObjPtr', 0, cursor)
                fn = rng.choice(['Obj_ListOkay', 'seg027_2861_EF9'])
                res = p.call(fn, [('w', 0)], 'b' if fn == 'Obj_ListOkay' else None)
                if cursor:
                    p.same(fn, res, 'cursor')
                    p.p('CursorObjPtr', 0, '0')
                    res = p.call('Obj_Free', [('f', cursor)], None)
            elif op == 'tree' and objs:
                res = p.call('seg027_2861_10E4', [('f', head), ('w', rng.randint(0, 3))], None)
            elif op == 'active':
                idx = rng.randint(2, 255)
                res = p.call(rng.choice(['active_critter', 'free_critter']), [('w', idx)], None)
            elif op == 'fixall' and rng.random() < 0.1:
                # with no player (it would clear the player's inventory, the inventory's own proof)
                p.p('ThePlayer', 0, '0')
                res = p.call('Map_ObjFix', [], None)
                p.p('ThePlayer', 0, 'L+401b')
            if res is None:
                continue
            ctx = 'level %d op %d %s' % (level, n, op)
            p.same(op, res, ctx)
            p.same_bytes('L', 0, 0x7c08, ctx)
            for name in POINTERS:
                p.same(name, p.q(name), ctx)
            p.same('rand', p.gseed(), ctx)
            counts[op] = counts.get(op, 0) + 1
            total += 1
    print('objects         %d operations on 9 levels  same (%s)' % (total, ', '.join('%s %d' % kv for kv in sorted(counts.items()))))
    ran, notrun, nosym = p.ran(ROUTINES)
    print('  routines run: %d of %d%s%s' % (len(ran), len(ROUTINES), ('; not run: ' + ', '.join(notrun)) if notrun else '',
          ('; no address: ' + ', '.join(nosym)) if nosym else ''))
    print('all the same')


if __name__ == '__main__':
    try:
        main()
    except Differ as ex:
        print('objects DIFFER:', ex)
        sys.exit(1)
