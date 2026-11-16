"""proof_bags.py: BAGS.C (ovr117: opening and closing containers in the inventory panel, scrolling their contents,
putting objects into them and swapping, the weight of what a container holds, DoSpecialActions) proved beside the
rules core. Each scenario reloads one of the nine real levels, makes a random sound inventory on both sides (new
objects from the proven Obj_Alloc, placed by the proven AddToInventory and Obj_AddEnd, bags filled and nested), and
runs random operations on it: open, close, scroll, put in, swap, the panel's special clicks, BagWeight. After every
call: the return, the outside calls, rand, the level block, the player record, the slots, the cursor object, the open
bags (walked on both sides: object, weight, links) and every global both sides have.

Not proved: DisplayOpenBag (replaced by the front end's panel in the core: it only draws).

    python tools/proof_bags.py game build/coretest.exe [operations per level]     (the exe argument is not used: the
    proofs' own copy of the harness, build/coretest_inv.exe, is built from the current core)
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from inv_common import DISPLAY_TO_SLOT, Inv, SLOT_TO_DISPLAY, main_wrap, obj_off, ScenarioDone  # noqa: E402

ROUTINES = ['DoSpecialActions', 'MakeBagClose', 'CloseAllBags', 'FixBagArea', 'CloseTheBag', 'FixOpenBag',
            'OpenTheBag', 'ScrollItemsUp', 'ScrollItemsDown', 'PutObjectInBag', 'SwapItemsInBag', 'BagWeight']


def main():
    game = sys.argv[1]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 600
    w = Inv(game, 117, 'bags')
    p, rng = w.p, w.rng
    based = False
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
            w.check(None, None, w.ctx + ' built')
            try:
                for k in range(rng.randint(15, 40)):
                    done += 1
                    p.seed(rng.getrandbits(32))
                    p.time(rng.getrandbits(32))
                    p.w('PlayerDat', 0x1e, bytes(rng.randint(0, 30) for _ in range(0x40)))
                    inv = w.slots()
                    bagopen = w.bag_open()
                    if not bagopen and any(inv[19:]):
                        # CloseAllBags (UW1) leaves Inventory[19..27] and the display map as they were; with no bag open a
                        # later takeFromSlot (slot above 18) reads OpenBag->obj through a null pointer (DOS: low memory, the
                        # host: a crash). The game's callers redraw the panel after it; here the slots are cleared as
                        # ClearInventory does, and the display map restored, on both sides
                        p.w('Inventory', 19 * 2, bytes(9 * 2))
                        p.w('SlotToDisplay', 0, bytes(SLOT_TO_DISPLAY + DISPLAY_TO_SLOT))
                        inv = w.slots()
                    op = rng.choice(['open'] * 5 + ['close', 'fix', 'closeall', 'up', 'down', 'up', 'down', 'put', 'put', 'put',
                                     'swap', 'swap', 'special', 'special', 'weight', 'fixopen'])
                    ctx = 'level %d scenario %d op %d %s' % (level, scen, k, op)
                    w.ctx = ctx
                    if w.trace:
                        print(ctx, flush=True)
                    if op == 'open':
                        slot = rng.choice([s for s in range(28) if inv[s]] or [11])
                        w.check('OpenTheBag', p.call('OpenTheBag', [('w', slot)], None), ctx + ' slot %d' % slot)
                    elif op == 'close':
                        w.check('CloseTheBag', p.call('CloseTheBag', [], None), ctx)
                    elif op == 'fix':
                        w.check('FixBagArea', p.call('FixBagArea', [], None), ctx)
                    elif op == 'closeall':
                        w.check('CloseAllBags', p.call('CloseAllBags', [], None), ctx)
                    elif op == 'up':
                        w.check('ScrollItemsUp', p.call('ScrollItemsUp', [], None), ctx)
                    elif op == 'down':
                        w.check('ScrollItemsDown', p.call('ScrollItemsDown', [], None), ctx)
                    elif op == 'fixopen':
                        if bagopen:  # FixOpenBag reads Inventory[19]'s object
                            w.check('FixOpenBag', p.call('FixOpenBag', [], None), ctx)
                    elif op == 'dispopen':
                        if bagopen:
                            w.check('DisplayOpenBag', p.call('DisplayOpenBag', [], None), ctx)
                    elif op == 'put':
                        o = w.random_obj(rng.choice([None, None, 'bag', 'rune', 'missile', 'light', 'food', 'scroll']))
                        if not o:
                            continue
                        # the slot of a container (AddToOccupiedSlot calls it for a container at the slot), or 19 with a bag open
                        cands = [s for s in range(0, 28) if inv[s] and w.item_at(inv[s]) >> 4 == 8 and (s < 19 or bagopen)]
                        if bagopen:
                            cands += [19, 19]
                        if not cands:
                            continue
                        slot = rng.choice(cands)
                        w.check('PutObjectInBag', p.call('PutObjectInBag', [('f', o[0]), ('w', slot)], 'b'), ctx + ' slot %d' % slot)
                    elif op == 'swap':
                        slots = [s for s in range(20, 28) if inv[s]]
                        if not bagopen or not slots:
                            continue
                        o = w.random_obj(rng.choice([None, None, 'bag', 'missile', 'food']))
                        if not o:
                            continue
                        slot = rng.choice(slots)
                        w.check('SwapItemsInBag', p.call('SwapItemsInBag', [('f', o[0]), ('w', slot)], 'b'), ctx + ' slot %d' % slot)
                    elif op == 'weight':
                        # a contents head: not a stack's (its link word is a quantity)
                        cand = [s for s in range(28) if inv[s] and not w.id_at(inv[s]) & 0x8000]
                        if not cand:
                            continue
                        i = rng.choice(cand)
                        p.w('S', 0, struct.pack('<h', rng.randint(-100, 100)))
                        w.check('BagWeight', p.call('BagWeight', [('f', 'L+%x' % (obj_off(inv[i]) + 6)), ('f', 'S+0')], None), ctx)
                        p.same('weight total', p.r('S', 0, 2), ctx)
                    elif op == 'special':
                        held = rng.random() < 0.4
                        slot = rng.choice([0x14, 0x15, 0x16, 0x17, 0x15, 0x16, 0x14, 8, 9, 8, 9, 1, 2, 3, 4, 5, 6, 7, 10, 11,
                                           12, 13, 14, 15, 16, 17, 18, 19, 0])
                        if held:
                            o = w.random_obj(rng.choice([None, 'bag', 'missile']))
                            if o:
                                p.p('CursorObjPtr', 0, o[0])
                        w.check('DoSpecialActions', p.call('DoSpecialActions', [('w', slot)], None), ctx + ' slot %d held %d' % (slot, held))
            except ScenarioDone as ex:
                if w.trace:
                    print('   scenario ends:', ex, flush=True)
    w.summary('bags', ROUTINES)


if __name__ == '__main__':
    main_wrap('bags', main)
