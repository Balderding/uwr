"""proof_invpanel.py: INVPANEL.C (ovr121: the inventory panel: slots and their screen rectangles, picking objects up and
putting them down, stacking and combining, the weight display, the hit test) proved beside the rules core, on random
sound inventories of the nine real levels (see proof_bags.py). Every routine is called on both sides: the clicks
(DoInventoryMouse with the pointer over every display position, an object on the cursor or not, in input modes 1 and 4,
the second hit from the mouse's own position), the drags in (DoInventoryDrag), ItemFitsSlot for every slot, AddTogether,
AddToEmptySlot, AddToOccupiedSlot (into a bag, merged, combined, swapped), RearrangeInventory, SetCursorObj, the
drawing routines (their outside calls compared), displayEnc, FindInventoryHit. After every call: the return, the
outside calls, rand, the level block, the player record, the slots, the cursor, the open bags and every global.

Not reached: the pick-up branch of DoInventoryMouse (it needs mouse_dragged(1), a stub that answers 0 on both sides), the
asking in AskHowMany (wdialog, a stub, answers 0: the typed number is the zeroed buffer on both sides), and the barter
area (mode 4 over the trade slots: BARTER.C's conv_inv_special needs the conversation).

    python tools/proof_invpanel.py game build/coretest.exe [operations per level]
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from inv_common import DISPLAY_TO_SLOT, Inv, SLOT_TO_DISPLAY, main_wrap, obj_off, sound_cursor, ScenarioDone  # noqa: E402

ROUTINES = ['BeginInventory', 'EndInventory', 'ClearInventory', 'DoInventoryMouse', 'DoInventoryDrag',
            'load_inv_pic', 'SetCursorObj', 'AskHowMany', 'ItemFitsSlot',
            'RearrangeInventory', 'AddToEmptySlot', 'AddTogether', 'AddToOccupiedSlot', 'displayEnc', 'FindInventoryHit']
# not proved: load_inventory_pix, DisplayInvSpecial, DisplayInvObject, DisplayInventory, displayInventoryArray (and BAGS.C's
# DisplayOpenBag): replaced by the front end's panel in the core (tools/core.py PATCHES), they only draw
# InvDisplay's rectangles (left, top, right, bottom), the mouse's hit boxes
RECT = [(0x0, 0x0, 0x0, 0x0), (0x10D, 0x90, 0x11D, 0x80), (0x10D, 0xBF, 0x11E, 0xAF), (0x107, 0xAE, 0x123, 0x9D),
        (0x107, 0x9C, 0x123, 0x90), (0x107, 0x80, 0x123, 0x77), (0xF4, 0xBB, 0x105, 0xAA), (0x125, 0xBB, 0x136, 0xAA),
        (0xF1, 0xA5, 0x102, 0x92), (0x127, 0xA5, 0x138, 0x92), (0xF1, 0x93, 0x10C, 0x88), (0x11E, 0x93, 0x138, 0x82),
        (0xF0, 0x76, 0x101, 0x65), (0x103, 0x76, 0x114, 0x65), (0x116, 0x76, 0x127, 0x65), (0x129, 0x76, 0x13A, 0x65),
        (0xF0, 0x64, 0x101, 0x53), (0x103, 0x64, 0x114, 0x53), (0x116, 0x64, 0x127, 0x53), (0x129, 0x64, 0x13A, 0x53),
        (0xF0, 0x87, 0x101, 0x76), (0x127, 0x81, 0x130, 0x78), (0x131, 0x81, 0x13A, 0x78)]


def main():
    game = sys.argv[1]
    per = int(sys.argv[3]) if len(sys.argv) > 3 else 500
    w = Inv(game, 121, 'invpanel')
    p, rng = w.p, w.rng
    based = False
    L = lambda i: 'L+%x' % obj_off(i)
    begun = False
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
                    for nm, v in (('PLeft', 0x0C), ('PBot', 0x37), ('PWid', 0xDC), ('PHgt', 0x80)):
                        w.setw(nm, v + rng.choice([0, 0, 1]))
                    w.setw('RightPanel', rng.choice([0, 0, 0, 0, 1]), '<B')
                    w.setw('scrmode', rng.choice([1, 1, 4, 2]))
                    w.setw('inv_refresh', rng.choice([0, 1]), '<B')
                    mode = rng.choice([1, 1, 4])
                    inv = w.slots()
                    bagopen = w.bag_open()
                    if not bagopen and any(inv[19:]):  # (see proof_bags.py: CloseAllBags' left over)
                        p.w('Inventory', 19 * 2, bytes(9 * 2))
                        p.w('SlotToDisplay', 0, bytes(SLOT_TO_DISPLAY + DISPLAY_TO_SLOT))
                        inv = w.slots()
                    carried = w.carried()
                    sound_cursor(w, carried)
                    slots_all = list(range(0, 19)) + (list(range(19, 28)) if bagopen else [])
                    op = rng.choice(['mouse'] * 8 + ['drag', 'fits', 'fits', 'fits', 'fits', 'together', 'together', 'empty', 'empty',
                                     'occupied', 'occupied', 'occupied', 'occupied', 'rearrange', 'rearrange', 'setcursor',
                                     'enc', 'enc', 'hit', 'hit',
                                     'clear', 'pic', 'misc', 'bagopen', 'bagclose', 'askhow'])
                    ctx = 'level %d scenario %d op %d %s' % (level, scen, k, op)
                    w.ctx = ctx
                    if w.trace:
                        print(ctx, flush=True)

                    def pos(i=None):
                        """a pointer position (screen x, y) over display position i (any when None), kept out of the barter
                        area in input mode 4"""
                        for _ in range(50):
                            j = rng.randint(1, 22) if i is None else i
                            if j == 23:
                                x, y = rng.randint(0x10, 0xE0), rng.randint(0x38, 0xB0)
                            else:
                                l, t, r, b = RECT[j]
                                x, y = rng.randint(l, r), rng.randint(b, t)
                            if rng.random() < 0.1:
                                x, y = rng.randint(0, 0x140), rng.randint(0, 0xC8)
                            if mode == 4 and 0x8B < x < 0xC1 and 0x98 < y < 0xBE:
                                continue
                            return x, y
                        return 0x120, 0x90

                    def setptr(x, y):
                        p.w('S', 0, struct.pack('<5h', x - 0xF0, y - 0x52, 0, 0, mode))

                    def freshobj(kind=None):
                        o = w.random_obj(kind or rng.choice([None, None, 'bag', 'armour', 'light', 'food', 'missile', 'rune']))
                        return o

                    if op == 'mouse':
                        held = rng.random() < 0.45
                        if held:
                            o = freshobj()
                            if o:
                                p.p('CursorObjPtr', 0, o[0])
                                w.setw('GameInputMode', rng.choice([0, 1, 1, 2]))
                        else:
                            w.setw('GameInputMode', rng.choice([0, 0, 1]))
                        x, y = pos()
                        setptr(x, y)
                        x2, y2 = pos()
                        p.mouse(x2, y2)
                        # not -2 (a look): inv_look calls checkTrap with a null object, which DOS reads as the interrupt vector table at
                        # 0000:0000; the emulator has UW.EXE's code there and the port reads a zero record
                        how = rng.choice([0, 1, -1, 2, 3])
                        p.p('newPlObj', 0, '0')  # the object looked at (set by the caller's click in the 3D view): none, so the panel's own
                        if w.trace:
                            print('   at', x, y, 'then', x2, y2, 'how', how, 'held', held, 'mode', mode, 'inv', inv, 'bagopen', bagopen, flush=True)
                        w.check('DoInventoryMouse', p.call('DoInventoryMouse', [('w', how)], None),
                                ctx + ' at %d,%d then %d,%d how %d held %d mode %d' % (x, y, x2, y2, how, held, mode))
                    elif op == 'drag':
                        o = freshobj()
                        if not o:
                            continue
                        x, y = pos()
                        p.mouse(x, y)
                        setptr(x, y)
                        w.check('DoInventoryDrag', p.call('DoInventoryDrag', [('f', o[0])], None), ctx)
                    elif op == 'fits':
                        setptr(0, 0)
                        c = rng.random()
                        if carried and c < 0.4:
                            i = rng.choice(carried)
                            spec = L(i)
                        else:
                            o = freshobj(rng.choice([None, 'armour', 'bag', 'light', 'food', 'missile', 'rune', 'scroll', 'weapon']))
                            if not o:
                                continue
                            spec = o[0]
                        slot = rng.choice(slots_all)
                        if spec != 'x' and carried and slot == 0 and spec in [L(i) for i in carried]:
                            slot = rng.choice([s for s in slots_all if s]) if len(slots_all) > 1 else 1  # food on the head slot is eaten (UseFood frees it): only the cursor's object goes there
                        w.check('ItemFitsSlot', p.call('ItemFitsSlot', [('f', spec), ('w', slot)], 'w'), ctx + ' slot %d' % slot)
                    elif op == 'together':
                        a = rng.choice(carried) if carried and rng.random() < 0.5 else None
                        b = rng.choice(carried) if carried and rng.random() < 0.5 else None
                        if a is None:
                            o = freshobj()
                            a = o[1] if o else None
                        if b is None:
                            # a stack of what a is, so the merge path is reached
                            if rng.random() < 0.5:
                                it = w.item_at(a) if a else 0
                                o = w.random_obj(item=it) if a else None
                                b = o[1] if o else None
                            else:
                                o = freshobj()
                                b = o[1] if o else None
                        if a and b:
                            w.check('AddTogether', p.call('AddTogether', [('f', L(a)), ('f', L(b))], 'b'), ctx)
                    elif op == 'empty':
                        cands = [s for s in slots_all if not inv[s] and s != 19]
                        if not cands:
                            continue
                        o = freshobj()
                        if not o:
                            continue
                        slot = rng.choice(cands + ([19] if rng.random() < 0.1 else []))
                        w.check('AddToEmptySlot', p.call('AddToEmptySlot', [('f', o[0]), ('w', slot)], 'b'), ctx + ' slot %d' % slot)
                    elif op in ('occupied', 'rearrange'):
                        cands = [s for s in slots_all if inv[s]]
                        if op == 'rearrange':
                            cands = slots_all
                        if not cands:
                            continue
                        slot = rng.choice(cands)
                        # a held object that likes the slot: the same item as the slot's now and then (the merge)
                        it = w.item_at(inv[slot]) if inv[slot] and rng.random() < 0.4 else None
                        o = w.random_obj(item=it) if it is not None else freshobj()
                        if not o:
                            continue
                        p.p('CursorObjPtr', 0, o[0])
                        w.setw('GameInputMode', 1)
                        if op == 'occupied':
                            w.check('AddToOccupiedSlot', p.call('AddToOccupiedSlot', [('f', o[0]), ('w', slot)], 'b'), ctx + ' slot %d' % slot)
                        else:
                            w.check('RearrangeInventory', p.call('RearrangeInventory', [('w', slot)], None), ctx + ' slot %d' % slot)
                    elif op == 'setcursor':
                        cands = [s for s in slots_all if inv[s] and s != 19]
                        if not cands:
                            continue
                        slot = rng.choice(cands)
                        if rng.random() < 0.3:
                            o = freshobj()
                            if o:
                                p.p('CursorObjPtr', 0, o[0])
                        keep = rng.choice([0, 0, 1])
                        w.check('SetCursorObj', p.call('SetCursorObj', [('w', slot), ('w', keep)], None), ctx + ' slot %d keep %d' % (slot, keep))
                    elif op == 'disp':
                        a = rng.randint(0, 22)
                        b = rng.randint(a, 22)
                        if rng.random() < 0.5:
                            b = a
                        w.check('displayInventoryArray', p.call('displayInventoryArray', [('w', a), ('w', b)], None), ctx + ' %d..%d' % (a, b))
                    elif op == 'dispobj':
                        d = rng.randint(0, 22)
                        w.check('DisplayInvObject', p.call('DisplayInvObject', [('w', d)], None), ctx + ' %d' % d)
                    elif op == 'dispinv':
                        w.check('DisplayInventory', p.call('DisplayInventory', [], None), ctx)
                    elif op == 'dispspecial':
                        w.check('DisplayInvSpecial', p.call('DisplayInvSpecial', [], None), ctx)
                    elif op == 'enc':
                        show = rng.choice([0, 1])
                        w.check('displayEnc', p.call('displayEnc', [('w', show)], 'b'), ctx)
                    elif op == 'hit':
                        x, y = (pos() if rng.random() < 0.8 else (rng.randint(-5, 0x150), rng.randint(-5, 0xD0)))
                        setptr(0, 0)
                        p.w('S', 8, struct.pack('<h', mode))
                        w.check('FindInventoryHit', p.call('FindInventoryHit', [('w', x), ('w', y)], 'w'), ctx + ' %d,%d mode %d' % (x, y, mode))
                    elif op == 'clear':
                        if bagopen:  # (a start-up / load routine: the bags were closed before, FixBagArea; here the display map would stay)
                            continue
                        w.check('ClearInventory', p.call('ClearInventory', [], None), ctx)
                        # every slot empty and no bags: carried objects stay in the contents list (their slots forgotten)
                    elif op == 'pix':
                        w.check('load_inventory_pix', p.call('load_inventory_pix', [], None), ctx)
                    elif op == 'pic':
                        w.check('load_inv_pic', p.call('load_inv_pic', [('w', rng.randint(1, 5)), ('w', rng.randint(0, 59))], 'b'), ctx)
                    elif op == 'misc':
                        if not begun and rng.random() < 0.5:
                            begun = True
                            w.check('BeginInventory', p.call('BeginInventory', [], None), ctx)
                        else:
                            w.check('EndInventory', p.call('EndInventory', [], None), ctx)
                    elif op == 'bagopen':
                        cands = [s for s in range(28) if inv[s] and w.item_at(inv[s]) >> 4 == 8]
                        if cands:
                            w.check('OpenTheBag', p.call('OpenTheBag', [('w', rng.choice(cands))], None), ctx)
                    elif op == 'bagclose':
                        w.check('CloseTheBag', p.call('CloseTheBag', [], None), ctx)
                    elif op == 'askhow':
                        cands = [i for i in carried if w.id_at(i) & 0x8000]
                        if not cands:
                            continue
                        i = rng.choice(cands)
                        w.check('AskHowMany', p.call('AskHowMany', [('f', L(i))], 'f'), ctx)
            except ScenarioDone as ex:
                if w.trace:
                    print('   scenario ends:', ex, flush=True)
    w.summary('invpanel', ROUTINES)


if __name__ == '__main__':
    main_wrap('invpanel', main)
