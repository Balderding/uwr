"""proof_invdata.py: INVDATA.C (ovr120: the inventory as data: what is in each slot, adding objects, taking them out whole
or in part, searching by class, damage to worn gear, weights) proved beside the rules core, on random sound inventories
of the nine real levels (see proof_bags.py for how they are made). Every routine of the file is called on both sides with
random but valid arguments: slots (the open bag's only with a bag open), objects the player carries (in a slot, in a
container, in none), fresh ones, quantities, major/minor/class filters. After every call: the return, the outside calls
(the redraws, the messages), rand, the level block, the player record, the slots, the cursor, the open bags and every
global both sides have.

    python tools/proof_invdata.py game build/coretest.exe [operations per level]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from inv_common import DISPLAY_TO_SLOT, Inv, SLOT_TO_DISPLAY, main_wrap, obj_off, ScenarioDone  # noqa: E402

ROUTINES = ['RedisplayInvSlot', 'AskInventory', 'FindEmptySlot', 'AddToInventory', 'FindSlot', 'FindObj', 'find_obj',
            'pick_inv', 'WhatsInSlot', 'InvRemoveObject', 'InvRemoveOneObject', 'invRemoveObject', 'RemoveAllFromSlot',
            'RemoveOneFromSlot', 'removeFromSlot', 'takeFromSlot', 'ObjWorn', 'DamageInventory', 'ItemWeight', 'EncumCheck']


def main():
    game = sys.argv[1]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 600
    w = Inv(game, 120, 'invdata')
    p, rng = w.p, w.rng
    based = False
    L = lambda i: 'L+%x' % obj_off(i)
    for level in range(1, 10):
        done = 0
        scen = 0
        while done < per:
            scen += 1
            w.level(level)
            w.ctx = 'level %d scenario %d' % (level, scen)
            w.reset_inventory()
            w.build_inventory(rng.randint(4, 16))
            if not based:
                w.baseline()
                based = True
            try:
                for k in range(rng.randint(15, 40)):
                    done += 1
                    p.seed(rng.getrandbits(32))
                    p.time(rng.getrandbits(32))
                    p.w('PlayerDat', 0x1e, bytes(rng.randint(0, 30) for _ in range(0x40)))
                    inv = w.slots()
                    bagopen = w.bag_open()
                    if not bagopen and any(inv[19:]):  # (see proof_bags.py: CloseAllBags' left over)
                        p.w('Inventory', 19 * 2, bytes(9 * 2))
                        p.w('SlotToDisplay', 0, bytes(SLOT_TO_DISPLAY + DISPLAY_TO_SLOT))
                        inv = w.slots()
                    carried = w.carried()
                    # slots to use: 0..18, and the open bag's 20..27 with a bag open
                    slots_all = list(range(0, 19)) + (list(range(19, 28)) if bagopen else [])
                    filled = [s for s in slots_all if inv[s]]
                    empty = [s for s in slots_all if not inv[s] and s != 19]
                    op = rng.choice(['redisp', 'ask', 'findempty', 'add', 'add', 'add', 'findslot', 'findobj', 'findobj', 'find_obj',
                                     'pick', 'whats', 'remove', 'removeone', 'invremove', 'invremove', 'removeall', 'removeone2',
                                     'removefs', 'takefs', 'takefs', 'worn', 'damage', 'damage', 'weight', 'encum',
                                     'bagopen', 'bagclose'])
                    ctx = 'level %d scenario %d op %d %s' % (level, scen, k, op)
                    w.ctx = ctx
                    if w.trace:
                        print(ctx, flush=True)

                    def anyobj(allow_fresh=True):
                        c = rng.random()
                        if carried and c < 0.7:
                            return rng.choice(carried)
                        o = w.random_obj() if allow_fresh else None
                        return o[1] if o else None
                    if op == 'bagopen':
                        cands = [s for s in range(28) if inv[s] and w.item_at(inv[s]) >> 4 == 8]
                        if cands:
                            w.check('OpenTheBag', p.call('OpenTheBag', [('w', rng.choice(cands))], None), ctx)
                        continue
                    if op == 'bagclose':
                        if rng.random() < 0.5:
                            w.check('CloseTheBag', p.call('CloseTheBag', [], None), ctx)
                        else:
                            w.check('FixBagArea', p.call('FixBagArea', [], None), ctx)
                        continue
                    if op == 'redisp':
                        w.check('RedisplayInvSlot', p.call('RedisplayInvSlot', [('w', rng.choice(slots_all))], None), ctx)
                    elif op == 'ask' or op == 'whats':
                        fn = 'AskInventory' if op == 'ask' else 'WhatsInSlot'
                        w.check(fn, p.call(fn, [('w', rng.choice(slots_all))], 'f'), ctx)
                    elif op == 'findempty':
                        w.check('FindEmptySlot', p.call('FindEmptySlot', [], 'w'), ctx)
                    elif op == 'add':
                        o = w.random_obj(rng.choice([None, None, 'armour', 'bag', 'light', 'food', 'missile']))
                        if not o:
                            continue
                        slot = rng.choice(empty + [-1] if empty else [-1])
                        if slot == -1 or rng.random() < 0.9:
                            pass
                        w.check('AddToInventory', p.call('AddToInventory', [('f', o[0]), ('w', slot)], 'b'), ctx + ' slot %d' % slot)
                    elif op == 'findslot':
                        i = anyobj()
                        if i:
                            w.check('FindSlot', p.call('FindSlot', [('f', L(i))], 'w'), ctx)
                    elif op == 'findobj':
                        mj = rng.choice([-1, -1, 0, 1, 2, 3, 4, 5])
                        mn = rng.choice([-1, -1, 0, 1, 2, 3])
                        cl = rng.choice([-1, -1, rng.randint(0, 15)])
                        how = rng.choice([1, 2, 3, 4, 5, 0])
                        p.w('S', 0, struct.pack('<h', rng.randint(-5, 5)))
                        w.check('FindObj', p.call('FindObj', [('w', mj), ('w', mn), ('w', cl), ('w', how), ('n', 'S+0')], 'f'),
                                ctx + ' major %d minor %d class %d how %d' % (mj, mn, cl, how))
                        p.same('FindObj where', p.r('S', 0, 2), ctx)
                    elif op == 'find_obj':
                        mj = rng.choice([-1, -1, 0, 1, 2, 3, 4, 5])
                        mn = rng.choice([-1, -1, 0, 1, 2, 3])
                        cl = rng.choice([-1, -1, rng.randint(0, 15)])
                        start = u16s(w, 0x4000 + 27 + 6) if rng.random() < 0.6 else (rng.choice(carried) if carried else 0)
                        p.p('ActiveObj', 0, L(start) if start else '0')  # the list variable (its address goes in)
                        w.check('find_obj', p.call('find_obj', [('w', mj), ('w', mn), ('w', cl), ('f', '@ActiveObj')], 'f'),
                                ctx + ' major %d minor %d class %d' % (mj, mn, cl))
                    elif op == 'pick':
                        x, y = rng.randint(-0x10, 0x60), rng.randint(-0x10, 0x70)
                        p.w('S', 0, struct.pack('<5h', x, y, 0, 0, rng.choice([1, 1, 4])))
                        how = rng.choice([2, 2, 0, 1])
                        w.check('pick_inv', p.call('pick_inv', [('w', how)], 'f'), ctx + ' x %d y %d how %d' % (x, y, how))
                    elif op in ('remove', 'removeone', 'invremove'):
                        i = anyobj(False)
                        if i and i not in w.open_objs():  # the panel refuses to take an open bag out: not a state the game has
                            fn = {'remove': 'InvRemoveObject', 'removeone': 'InvRemoveOneObject', 'invremove': 'invRemoveObject'}[op]
                            args = [('f', L(i))] + ([('w', rng.choice([-1, 1, 2, 3, 5, 0, 12]))] if op == 'invremove' else [])
                            w.check(fn, p.call(fn, args, 'b'), ctx)
                    elif op in ('removeall', 'removeone2', 'removefs', 'takefs'):
                        slot = rng.choice(filled) if filled and rng.random() < 0.85 else rng.choice(slots_all)
                        if inv[slot] in w.open_objs():  # (an open bag is not taken out: the panel refuses)
                            continue
                        qty = rng.choice([0, 1, 2, 5, -1, 3, 12])
                        # the filters (major, minor, class) come from an object the player carries, so a match exists: the
                        # search (find_obj) starts at the owner itself and, with no match, goes on to the owner's next objects
                        # (for the player: the objects on his tile) and a direct match there leaves the owner variable 0, and
                        # Obj_Rem(&owner->ol.link, ...) goes through a null pointer (DOS: low memory, the host: a crash). The game
                        # filters on ammunition it has found (MISSILE.C's RemoveOneFromSlot(0, 1, ammo, slot)), and only on the
                        # player's own slots
                        mj = mn = cl = -1
                        if slot <= 18 and carried and rng.random() < 0.6 and not w.open_objs():
                            cid = w.id_at(rng.choice(carried))
                            mj, mn, cl = cid >> 6 & 7, cid >> 4 & 3, cid & 15
                            mj, mn, cl = rng.choice([(mj, -1, -1), (-1, mn, -1), (mj, mn, -1), (mj, mn, cl), (-1, -1, cl), (mj, -1, cl)])
                            if (mj < 0 or mj == 1) and (mn < 0 or mn == 3) and (cl < 0 or cl == 15):  # the player (item 0x7F) matches itself
                                mj = mn = cl = -1
                        fn = {'removeall': 'RemoveAllFromSlot', 'removeone2': 'RemoveOneFromSlot', 'removefs': 'removeFromSlot',
                              'takefs': 'takeFromSlot'}[op]
                        args = [('w', mj), ('w', mn), ('w', cl), ('w', slot)] + ([('w', qty)] if op in ('removefs', 'takefs') else [])
                        w.check(fn, p.call(fn, args, 'f'), ctx + ' slot %d major %d minor %d class %d qty %d' % (slot, mj, mn, cl, qty))
                    elif op == 'worn':
                        iid = rng.choice([rng.randint(0, 0x1ff), rng.randint(0x20, 0x3f), rng.randint(0x30, 0x3f)])
                        slot = rng.randint(-2, 30)
                        w.check('ObjWorn', p.call('ObjWorn', [('w', iid), ('w', slot)], 'b'), ctx)
                    elif op == 'damage':
                        slot = rng.choice(filled) if filled else 0
                        if inv[slot] in w.open_objs():
                            continue
                        dmg = rng.choice([0, 1, 5, 20, 70, 255])
                        typ = rng.choice([0, 1, 2, 4, 8, 0x10, 0x20, 0xff])
                        how = rng.choice([0, 1, 2])
                        # debris (a destroyed object leaves some by the player: near_mob_put_at) needs the player on the map
                        w.check('DamageInventory', p.call('DamageInventory', [('w', slot), ('w', dmg), ('w', typ), ('w', how), ('w', 0)], 'w'),
                                ctx + ' slot %d dmg %d type %d how %d' % (slot, dmg, typ, how))
                    elif op == 'weight' or op == 'encum':
                        i = anyobj()
                        if i:
                            fn = 'ItemWeight' if op == 'weight' else 'EncumCheck'
                            w.check(fn, p.call(fn, [('f', L(i))], 'w' if op == 'weight' else 'b'), ctx)
            except ScenarioDone as ex:
                if w.trace:
                    print('   scenario ends:', ex, flush=True)
    w.summary('invdata', ROUTINES)


def u16s(w, off):
    return struct.unpack_from('<H', w.p.e.read(0xC000, off, 2), 0)[0] >> 6


if __name__ == '__main__':
    main_wrap('invdata', main)
