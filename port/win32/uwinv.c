/* uwinv.c: holds the player's inventory for our panel (src\uwinv_draw.c): a snapshot of what the original's own code
   holds, and the commands our panel sends back. The snapshot is taken on the game's thread where uwgame.c takes the 3D
   frame, reading only: INVPANEL.C's Inventory[] (the slots, each a link to an object of the player's contents),
   SlotToDisplay / DisplayToSlot and InvDisplay (the panel's rectangles; an open bag's eight objects are slots 20..27
   shown in positions 12..19), CursorObjPtr (the object on the cursor), the weights (INVDATA.C's ItemWeight, PlayerDat's
   weight and max_weight) and the names (UI's get_name). The commands (uwi_cmd) are run on the game's thread between
   the main loop's passes, using the original's own rules. The one other change to the game is uwi_want_panel: the
   original's own pull_chain, run at the next frame, when the right-hand panel shows the rune bag or the statistics
   instead of the inventory. */
#include "compat.h"
#include "combat.h"
#include "gfx.h"
#include "inv.h"
#include "items.h"
#include "object.h"
#include "player.h"
#include "sys.h"
#include "ui.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uwinv.h"
#include "rlog.h"

void far pull_chain(int how);   /* INTERACT.C */

static UwiSnap snap;
static atomic_flag lk = ATOMIC_FLAG_INIT;
static atomic_int have, want_panel, g_mode;
/* the game's input mode as last captured */
int uwi_input_mode(void) { return atomic_load(&g_mode); }
static void lock(void) { while (atomic_flag_test_and_set_explicit(&lk, memory_order_acquire)) {} }
static void unlock(void) { atomic_flag_clear_explicit(&lk, memory_order_release); }

/* asks for the right-hand panel to be switched back to the inventory at the next frame */
void uwi_want_panel(void) { atomic_store(&want_panel, 1); }

/* the body slot an object is worn or wielded in (INV_*), -1 none: armour (MAJOR_HACK, minor 2 and 3) by its Armor
   category as ItemFitsSlot matches it to the paperdoll, a ring in the ring hole that is free (else the first), a
   weapon (minor 0, not a stack, as ItemFitsSlot refuses one) or a sling or bow in the weapon hand, a shield in the
   other hand. get_class_data reads ActiveObj: put back. */
static int wear_slot(struct Object far *obj)
{
    int item = OBJ_ITEM(obj), minor = OBJ_MINOR(obj), slot = -1;
    if (OBJ_MAJOR(obj) != MAJOR_HACK) return -1;
    if (minor == MINOR_WEAPON || item == ITEM_SLING || item == ITEM_BOW || item == ITEM_CROSSBOW || item == ITEM_JEWELED_BOW) {
        if (OBJ_ISQUANT(obj) && !(obj->ol.f.link & LINK_SPECIAL) && obj->ol.f.link > 1) return -1;
        return INV_WEAPON_HAND - player->lefty;
    }
    if (minor >= MINOR_ARMOR) {
        struct Object far *was = ActiveObj;
        const struct Armour *cls;
        ActiveObj = obj;
        cls = (const struct Armour *)get_class_data();
        ActiveObj = was;
        switch (cls->category) {
        case ARMOUR_HAT: slot = INV_HEAD; break;
        case ARMOUR_BODY: slot = INV_TORSO; break;
        case ARMOUR_GLOVES: slot = INV_GLOVES; break;
        case ARMOUR_LEGGINGS: slot = INV_LEGS; break;
        case ARMOUR_BOOTS: slot = INV_BOOTS; break;
        case ARMOUR_RING: slot = Inventory[INV_RING].f.index && !Inventory[INV_RING + 1].f.index ? INV_RING + 1 : INV_RING; break;
        case ARMOUR_SHIELD: slot = INV_HAND + player->lefty; break;
        }
    }
    return slot;
}

/* fills t from an object: its name, quantity, weight, worth, condition, and its weapon or armour numbers */
static void thing(UwiThing *t, struct Object far *obj)
{
    memset(t, 0, sizeof *t);
    t->item = -1;
    t->wear = -1;
    if (!obj) return;
    {
        int ws = wear_slot(obj);
        t->wear = ws >= 0 ? SlotToDisplay[ws] : -1;
    }
    if (OBJ_MAJOR(obj) == MAJOR_HACK) {   /* the class record (get_class_data reads ActiveObj: put back) */
        struct Object far *was = ActiveObj;
        int minor = OBJ_MINOR(obj);
        ActiveObj = obj;
        if (minor == MINOR_WEAPON) {
            const struct Weapon *w = (const struct Weapon *)get_class_data();
            if (w) t->kind = 2, t->dmg[0] = w->damage[0], t->dmg[1] = w->damage[1], t->dmg[2] = w->damage[2];
        } else if (minor >= MINOR_ARMOR) {
            const struct Armour *a = (const struct Armour *)get_class_data();
            if (a) t->kind = 1, t->prot = a->protection;
        }
        ActiveObj = was;
    }
    {
        int item = OBJ_ITEM(obj), q = 1, quality, k;
        const struct ComObj *com = &ComObjData[item];
        char far *w;
        t->item = item;
        if (OBJ_ISQUANT(obj) && !(obj->ol.f.link & LINK_SPECIAL) && obj->ol.f.link > 1) q = obj->ol.f.link;
        t->qty = q;
        t->weight = ItemWeight(obj);
        t->container = OBJ_MAJOR(obj) == MAJOR_MISC && OBJ_MINOR(obj) == MINOR_CONTAINER;
        t->runebag = item == ITEM_RUNE_BAG;
        t->magic = (obj->id >> 12 & 1) != 0;
        t->worth = com->value * q;
        get_name(t->name, obj, 0, q > 1);
        t->name[sizeof t->name - 1] = 0;
        quality = obj->qn.f.quality ? (com->qualclass != 3 ? (obj->qn.f.quality >> 4) + 1 : 5) : 0;
        w = get_string((com->qualtype * 6 + quality) | STR_OBJLOOK);
        if (w && *w) {
            for (k = 0; k < (int)sizeof t->cond - 1 && w[k]; k++) t->cond[k] = w[k];
            t->cond[k] = 0;
        }
    }
}

static atomic_int active;
/* turns the capture on or off */
void uwi_set_active(int on) { atomic_store(&active, on); }

/* ---- the whole of what is carried, for our grid: the pack's eight (Inventory[11..18]) and each container's contents
   (its ol link, then each object's qn link: the original's own chains), bags in bags to 4 deep. The rune bag (inner
   class 0xF) is a thing, not a bag: OpenTheBag shows the rune panel for it. ---- */
/* whether the object is a bag that opens (the rune bag is not) */
static int is_bag(struct Object far *o)
{
    return o && OBJ_MAJOR(o) == MAJOR_MISC && OBJ_MINOR(o) == MINOR_CONTAINER && OBJ_INCLASS(o) != 0xF;
}
/* adds an object to the snapshot's tree of carried things */
static void tree_add(UwiSnap *s, struct Object far *o, int parent, int depth, int slot)
{
    int me;
    if (!o || s->nnodes >= UWI_NODES) return;
    me = s->nnodes++;
    thing(&s->node[me].t, o);
    s->node[me].obj = Obj_MemTPtr(o), s->node[me].parent = parent, s->node[me].depth = depth, s->node[me].slot = slot;
    s->node[me].bag = is_bag(o);
}
/* fills the snapshot's tree with the pack's objects and the contents of every bag */
static void tree_build(UwiSnap *s)
{
    int k, i;
    s->nnodes = 0;
    for (k = INV_PACK; k <= INV_PACK_LAST; k++)
        if (Inventory[k].f.index) tree_add(s, Obj_PtrTMem(&Inventory[k]), -1, 0, k);
    /* then each bag's contents, breadth first: a bag's own after everything before it */
    for (i = 0; i < s->nnodes && s->nnodes < UWI_NODES; i++) {
        struct Object far *bag, far *o;
        int guard = 0;
        if (!s->node[i].bag || s->node[i].depth >= 4) continue;
        bag = Obj_IntTMem(s->node[i].obj);
        if (!bag || !bag->ol.f.link) continue;
        for (o = Obj_PtrTMem(&bag->ol.link); o && guard < 256; o = o->qn.f.next ? Obj_PtrTMem(&o->qn.link) : 0, guard++)
            tree_add(s, o, i, s->node[i].depth + 1, -1);
    }
}

/* ---- show an object in the original's panel (uwi_reveal): its bags opened from the pack in, then the open bag scrolled
   to it, all by the original's own routines (BAGS.C: FixBagArea gives the pack back, OpenTheBag opens one on show,
   ScrollItemsUp shows the next row of four). Run on the game's thread before the snapshot. ---- */
/* a look at an object carried (our panel's left click: what it is), as the original's inv_look
   (INTERACT.C): the Lore roll kept in its heading, then LookAt; without inv_look's DoInventoryMouse(-1), which would act
   on the original's pointer */
static atomic_int want_look;
void uwi_look(int obj) { atomic_store(&want_look, obj + 1); }
static void port_look(int idx)
{
    struct Object far *o = Obj_IntTMem(idx);
    int lore, head, ident;
    if (!o) return;
    ident = OBJ_MAJOR(o) != MAJOR_RECT && OBJ_MAJOR(o) != MAJOR_TRAP && ComObjData[OBJ_ITEM(o)].render != 2;
    if (ident) {
        head = OBJ_HEADING(o);
        if (head & 4) lore = head & 3;
        else {
            lore = skill_check(player->skills[SKILL_LORE], 0xA) + 1;
            if (lore == 0) lore = 1;
            if ((head & 3) > lore) lore = head & 3;
            SET_HEADING(o, lore | 4);
        }
    } else lore = 1;
    seg024_24DC_D0A(o, lore);
}
static atomic_int want_reveal;
void uwi_reveal(int obj) { atomic_store(&want_reveal, obj + 1); }
static atomic_int want_pack;
void uwi_show_pack(void) { atomic_store(&want_pack, 1); }
static int shown_at(int obj)   /* the display position (1..19) it is on show in, -1 none */
{
    int d;
    for (d = 1; d <= 19; d++) {
        int slot = DisplayToSlot[d];
        if (slot >= 0 && slot < NUM_INV_SLOTS && Inventory[slot].f.index && Inventory[slot].f.index == obj) return d;
    }
    return -1;
}
static int scroll_to(int obj)  /* scroll the open bag until obj is on show: its position, -1 */
{
    int d, k;
    for (k = 0; k < 64; k++) {
        if ((d = shown_at(obj)) >= 0) return d;
        if (!OpenBag || !(char)InvUpArrow) return -1;
        ScrollItemsUp();
    }
    return -1;
}
/* opens the bags round an object and scrolls to it so that the original's panel shows it */
static void reveal(const UwiSnap *t, int obj)
{
    int path[8], n = 0, i, node = -1, d;
    if (shown_at(obj) >= 0) return;
    for (i = 0; i < t->nnodes; i++)
        if (t->node[i].obj == obj) { node = i; break; }
    if (node < 0) return;
    for (i = t->node[node].parent; i >= 0 && n < 8; i = t->node[i].parent) path[n++] = t->node[i].obj;
    FixBagArea();                                  /* the pack on show, every bag closed */
    while (n > 0) {                                /* the outermost bag first */
        int b = path[--n];
        if ((d = scroll_to(b)) < 0) return;
        OpenTheBag(DisplayToSlot[d]);
    }
    scroll_to(obj);
}

/* ---- the backend's commands (uwinv.h's UWI_OP_*): a ring written by the window's thread, run on the game's thread
   between the main loop's passes (MAINLOOP.C's port_inv_hook), in order. The inventory's data routines here are our
   own, written against the objects themselves: the slots Inventory[0..18] (the body and the pack, the player's own
   contents chain) and each bag's contents chain (ol.link, then qn.link: the original's lists), bags in bags. The rules
   are the original's, ported line by line (INVPANEL.C ItemFitsSlot, INVDATA.C takeFromSlot / AddToInventory,
   INVPANEL.C AddToOccupiedSlot, BAGS.C PutObjectInBag / SwapItemsInBag); what they leave out is the original's own
   panel state (OpenBag, the slots 19..27, DisplayToSlot, its page), which is never touched: nothing has to be put
   back. The game's side effects stay the original's (FixPlayerEquips: light, armour, weapon; UseObj; ReturnObject;
   add_rune). ---- */
#define CMDN 32
static struct { int op, a, b, c; } cmds[CMDN];
static atomic_int cmd_w, cmd_r;
/* queues a command for the game's thread; 0 when the queue is full */
int uwi_cmd(int op, int a, int b, int c)
{
    int w = atomic_load_explicit(&cmd_w, memory_order_relaxed);
    if (w - atomic_load_explicit(&cmd_r, memory_order_acquire) >= CMDN) return 0;
    cmds[w % CMDN].op = op, cmds[w % CMDN].a = a, cmds[w % CMDN].b = b, cmds[w % CMDN].c = c;
    atomic_store_explicit(&cmd_w, w + 1, memory_order_release);
    return 1;
}
/* how many commands are still waiting */
int uwi_cmd_pending(void) { return atomic_load(&cmd_w) - atomic_load(&cmd_r); }

extern void (*port_inv_hook)(void);               /* MAINLOOP.C, port only: called between the main loop's passes */
extern int port_stack_whole;                      /* INVPANEL.C, port only: AskHowMany takes a stack whole, unasked */
extern int port_grab_armed[3];                    /* port/ui/keys.c: the port's n, o, u, armed first */
extern int port_throw_x, port_throw_y, port_throw_speed;
void far port_key_grab(int how);
void far port_key_rune(int ch);                   /* port/ui/keys.c: a rune key (a..y), Backspace clears, Space casts */
void far port_target_at(int x, int y);            /* port/ui/keys.c: the target in the world at the screen point */
void far port_aim_at(int x, int y);               /* port/ui/keys.c: a missile spell fired at the screen point */
int port_rune_to_bag(struct Object far *obj);     /* INVPANEL.C, port only: a runestone into the rune bag when one is carried */
char far add_rune(struct Object far *obj);        /* RUNES.C */
char far AddTogether(struct Object far *obj, struct Object far *onto);   /* INVPANEL.C */
extern struct Object far *ActiveObj;

static UwiSnap ct;   /* the tree, for the bag an object is in */
static int slot_of(int idx)   /* the slot (0..18) an object is in, -1 */
{
    int i;
    for (i = 0; i <= INV_PACK_LAST; i++) if (Inventory[i].f.index && Inventory[i].f.index == idx) return i;
    return -1;
}
/* the bag whose contents chain holds the object (by the tree, bags in bags), 0: in a slot or not carried */
static struct Object far *bag_of(int idx)
{
    int i;
    tree_build(&ct);
    for (i = 0; i < ct.nnodes; i++)
        if (ct.node[i].obj == idx) return ct.node[i].parent >= 0 ? Obj_IntTMem(ct.node[ct.node[i].parent].obj) : 0;
    return 0;
}
static int ancestor(struct Object far *cont, struct Object far *obj)   /* obj is cont or a bag round it */
{
    int guard = 0;
    for (; cont && guard < 8; cont = bag_of(Obj_MemTPtr(cont)), guard++) if (cont == obj) return 1;
    return 0;
}
static int targeting(void) { return GameInputMode == GIM_TARGET || GameInputMode == GIM_AIM; }   /* the thing on the cursor is still in its place */
static void cursor_clear(void)   /* nothing on the pointer any more, as DoInventoryMouse ends */
{
    CursorObjPtr = 0;
    unforce_mouse_cursor(3);
    GameInputMode = GIM_NONE;
}
static void cursor_set(struct Object far *o)
{
    CursorObjPtr = o;
    unforce_mouse_cursor(0);
    force_mouse_cursor(OBJ_ITEM(o));
    GameInputMode = GIM_CARRY;
}
static void body_redraw(int slot)   /* the original's panel: a body hole redrawn (our panel's top copies it) */
{
    if (slot >= 0 && slot < INV_PACK && RightPanel == PANEL_INV) DisplayInvObject(SlotToDisplay[slot]);
}

/* ItemFitsSlot for a body or pack slot with nothing in it: the armour by its category, the rings, the weapon hand
   (no stack of weapons), a lit light as its unlit form outside the shoulders and hands (ValidLightSlots), else the
   item's pickup flag; -1: eaten (food on the head) */
static int fits_slot(struct Object far *obj, int slot)
{
    struct Armour *cls;
    int major = OBJ_MAJOR(obj), minor = OBJ_MINOR(obj), sub = OBJ_INCLASS(obj), id = OBJ_ITEM(obj), i;
    ActiveObj = obj;
    if (slot < INV_SHOULDER) {
        if (major != MAJOR_HACK) {
            if (slot == INV_HEAD && UseFood(ThePlayer, obj, 0) > 0) return -1;
            return 0;
        }
        if (minor < 2) return 0;
        cls = (struct Armour *)get_class_data();
        switch (slot) {
        case INV_TORSO: return cls->category == ARMOUR_BODY;
        case INV_LEGS: return cls->category == ARMOUR_LEGGINGS;
        case INV_GLOVES: return cls->category == ARMOUR_GLOVES;
        case INV_HEAD: return cls->category == ARMOUR_HAT;
        case INV_BOOTS: return cls->category == ARMOUR_BOOTS;
        }
        return 0;
    }
    if (slot == INV_RING || slot == INV_RING + 1) {
        if (major != MAJOR_HACK || minor < MINOR_ARMOR) return 0;
        cls = (struct Armour *)get_class_data();
        return cls->category == ARMOUR_RING;
    }
    if (INV_WEAPON_HAND - player->lefty == slot && major == MAJOR_HACK && minor == 0) {
        if (OBJ_ISQUANT(obj) && !(obj->ol.f.link & LINK_SPECIAL) && obj->ol.f.link > 1) return 0;
    } else if (major == MAJOR_MISC && minor == 1 && sub >= 4 && sub < 8) {
        for (i = 0; i < 4; i++) if (ValidLightSlots[i] == slot) return 1;
        SET_INCLASS(obj, sub - 4);
        if (!fits_slot(obj, slot)) { SET_INCLASS(obj, sub); return 0; }
        return 1;
    }
    return ComObjData[id].pickup;
}
/* ItemFitsSlot's container part: the weight into the bag and every bag round it ('The sack is too full.'), then the
   bag's mask (the rune bag runestones, the quiver missiles, the map case scrolls, the bowl food), else the pickup flag */
static int bag_takes(struct Object far *obj, struct Object far *cont)
{
    int id = OBJ_ITEM(obj), major = OBJ_MAJOR(obj), minor = OBJ_MINOR(obj), sub = OBJ_INCLASS(obj), cap, res;
    struct Object far *c;
    int guard = 0;
    for (c = cont; c && guard < 8; c = bag_of(Obj_MemTPtr(c)), guard++) {
        int16 w = (int16)ItemWeight(obj);
        char name[26];
        BagWeight(&c->ol.link, &w);
        cap = Containers[OBJ_INCLASS(c)].capacity;
        if (cap != 0 && w > cap) {
            if (!get_name(name, c, 0, 0)) strcpy(name, "UNNAMED");
            scroll_print("The "), scroll_print(name), scroll_print(" is too full.\n");
            return 0;
        }
    }
    cap = Containers[OBJ_INCLASS(cont)].mask;
    if (cap >= 0) {
        if (cap < CONT_RUNES) {
            if (id != cap) game_sprint(0xF8);   /* 'That item does not fit.' */
            return id == cap;
        }
        switch (cap) {
        case CONT_RUNES:
            res = major == MAJOR_STUFF && (minor == 3 || (minor == 2 && sub > 7));
            if (!res) { game_sprint(0xF7); return 0; }   /* 'You can only put runes in the rune bag.' */
            break;
        case CONT_MISSILES: res = major == MAJOR_HACK && minor == 1 && sub < 3; break;
        case CONT_SCROLLS: res = major == MAJOR_SPEC && minor == 3 && sub >= 8; break;
        case CONT_FOOD:
            res = (major == MAJOR_MISC && minor == 3) || id == ITEM_PLANT_CE || id == ITEM_PLANT_CF || id == ITEM_CANDLE
                || id == ITEM_LEECHES || id == ITEM_ROTWORM_STEW || id == ITEM_DEAD_ROTWORM;
            break;
        default: res = 0;
        }
        if (!res) game_sprint(0xF8);
        return res;
    }
    return ComObjData[id].pickup;
}

/* an object carried, anywhere, onto the pointer (takeFromSlot's bookkeeping): a stack split first (qty: 0 all, -1
   asked by the original's 'Move how many?', n that many), the rest staying where it was */
static void take(int idx, int qty)
{
    struct Object far *o = Obj_IntTMem(idx), far *copy = 0, far *cont = 0;
    int slot;
    if (!o || CursorObjPtr != 0 || targeting()) return;
    slot = slot_of(idx);
    if (slot < 0 && !(cont = bag_of(idx))) return;
    if (qty != 0 && OBJ_ISQUANT(o) && !(o->ol.f.link & LINK_SPECIAL) && o->ol.f.link > 1) {
        int have = o->ol.f.link;
        if (qty < 0) {
            int whole = port_stack_whole;
            port_stack_whole = 0;
            copy = AskHowMany(o);   /* o cut to the count, the rest a new object (not yet linked), or o itself, or 0 */
            port_stack_whole = whole;
            if (copy == 0) return;
            if (copy == o) copy = 0;
        } else if (qty < have) {
            copy = Obj_Alloc(0);
            if (copy == 0) return;
            *(struct StaticObj far *)copy = *(struct StaticObj far *)o;
            copy->ol.f.link = have - qty;
            o->ol.f.link = qty;
        }
        if (copy) Obj_Add(&o->qn.link, copy);   /* the rest after it in the chain: it stays when o is unlinked */
    }
    if (slot >= 0) {
        Inventory[slot].f.index = copy ? Obj_MemTPtr(copy) : 0;
        Obj_Rem(&ThePlayer->ol.link, o);
    } else Obj_Rem(&cont->ol.link, o);
    player->weight -= ItemWeight(o);
    cursor_set(o);
    FixPlayerEquips();
    body_redraw(slot);
}
/* the pointer's object into a bag (PutObjectInBag): a lit light goes out, the rune bag takes a runestone as a rune,
   a matching stack grows, else on the end of the bag's chain */
static void put_in_bag(struct Object far *cont)
{
    struct Object far *obj = CursorObjPtr, far *next;
    int sub, lit = 0;
    if (!obj || !cont || obj == cont || ancestor(cont, obj) || targeting()) return;
    sub = OBJ_INCLASS(obj);
    if (OBJ_MAJOR(obj) == MAJOR_MISC && OBJ_MINOR(obj) == 1 && sub >= 4 && sub < 8) SET_INCLASS(obj, sub - 4), lit = 1;
    if (!bag_takes(obj, cont)) { if (lit) SET_INCLASS(obj, sub); return; }
    if (OBJ_ITEM(cont) == ITEM_RUNE_BAG) {
        if (add_rune(obj)) cursor_clear();
        else game_sprint(0xF7);
        return;
    }
    player->weight += ItemWeight(obj);
    for (next = Obj_PtrTMem(&cont->ol.link); next != 0; next = Obj_PtrTMem(&next->qn.link))
        if (AddTogether(obj, next)) {
            int qty = OBJ_ISQUANT(obj) ? obj->ol.f.link : 1;
            if (!OBJ_ISQUANT(next)) SET_ISQUANT(next, 1), next->ol.f.link = 1;
            next->ol.f.link += qty;
            next->qn.f.quality = (next->qn.f.quality + obj->qn.f.quality) >> 1;
            Obj_Free(obj);
            break;
        }
    if (next == 0) Obj_AddEnd(&cont->ol.link, obj);
    cursor_clear();
    FixPlayerEquips();
}
/* the pointer's object into an empty slot (AddToInventory): -1 fits means it was eaten */
static void put_empty(int slot)
{
    struct Object far *obj = CursorObjPtr;
    int fits;
    if (!obj || slot < 0 || slot > INV_PACK_LAST || Inventory[slot].f.index != 0 || targeting()) return;
    fits = fits_slot(obj, slot);
    if (fits > 0) {
        Inventory[slot].f.index = Obj_MemTPtr(obj);
        Obj_AddEnd(&ThePlayer->ol.link, obj);
        player->weight += ItemWeight(obj);
        cursor_clear();
    } else if (fits == -1) cursor_clear();
    FixPlayerEquips();
    body_redraw(slot);
}
/* the pointer's object onto an object carried (AddToOccupiedSlot / SwapItemsInBag): into a bag that it is, merged
   with a matching stack, combined (COMBINE.C), else swapped: the other comes onto the pointer */
static void put_on(int idx)
{
    struct Object far *obj = CursorObjPtr, far *target = Obj_IntTMem(idx), far *cont = 0;
    int slot, combo;
    if (!obj || !target || obj == target || targeting()) return;
    slot = slot_of(idx);
    if (slot < 0 && !(cont = bag_of(idx))) return;
    if (OBJ_MAJOR(target) == MAJOR_MISC && OBJ_MINOR(target) == MINOR_CONTAINER) { put_in_bag(target); return; }
    if (AddTogether(obj, target)) {
        int qty = OBJ_ISQUANT(obj) ? obj->ol.f.link : 1;
        if (slot >= 0 ? fits_slot(obj, slot) != 1 : !bag_takes(obj, cont)) return;
        if (!OBJ_ISQUANT(target)) SET_ISQUANT(target, 1), target->ol.f.link = 1;
        player->weight += ComObjData[OBJ_ITEM(obj)].mass * qty;
        target->ol.f.link += qty;
        target->qn.f.quality = (target->qn.f.quality + obj->qn.f.quality) >> 1;
        Obj_Free(obj);
        cursor_clear();
        FixPlayerEquips();
        body_redraw(slot);
        return;
    }
    if ((combo = ObjsBeCombinable(obj, target)) >= 0) {
        struct Object far *made = CombineObjs(combo);
        char used;
        if (made == 0) return;
        if ((used = RemoveAfterCombine(obj, combo)) == 1) {
            Obj_Punt(0, obj, 1);
            cursor_set(made);
        }
        if (RemoveAfterCombine(target, combo)) {
            int tidx = Obj_MemTPtr(target);
            if (slot >= 0) {   /* the target out of its slot, the thing made in its place when the pointer's was used up */
                Inventory[slot].f.index = 0;
                Obj_Rem(&ThePlayer->ol.link, target);
            } else Obj_Rem(&cont->ol.link, target);
            player->weight -= ItemWeight(target);
            Obj_Punt(0, target, 1);
            (void)tidx;
            if (!used) {
                CursorObjPtr = made;
                if (slot >= 0) put_empty(slot); else put_in_bag(cont);
                if (CursorObjPtr == made) cursor_set(made);   /* it did not fit there: left on the pointer */
            }
        }
        FixPlayerEquips();
        body_redraw(slot);
        return;
    }
    {   /* swapped */
        if (slot >= 0) {
            Inventory[slot].f.index = 0;
            Obj_Rem(&ThePlayer->ol.link, target);
            player->weight -= ItemWeight(target);
            put_empty(slot);
            if (Inventory[slot].f.index != 0) cursor_set(target);   /* the pointer's in: the other onto the pointer */
            else {   /* the pointer's does not go there (or was eaten there: food on the head hole): the other back */
                Inventory[slot].f.index = Obj_MemTPtr(target);
                Obj_AddEnd(&ThePlayer->ol.link, target);
                player->weight += ItemWeight(target);
                FixPlayerEquips();
                body_redraw(slot);
            }
        } else {
            Obj_Rem(&cont->ol.link, target);
            player->weight -= ItemWeight(target);
            put_in_bag(cont);
            if (CursorObjPtr == 0) cursor_set(target);
            else {
                Obj_AddEnd(&cont->ol.link, target);
                player->weight += ItemWeight(target);
                FixPlayerEquips();
            }
        }
    }
}
static void put_pack(void)   /* the pointer's object into the first empty hole of the pack (a runestone into the rune bag) */
{
    int slot;
    if (!CursorObjPtr || targeting()) return;
    if (port_rune_to_bag(CursorObjPtr)) { cursor_clear(); return; }
    for (slot = INV_PACK; slot <= INV_PACK_LAST && CursorObjPtr; slot++)
        if (Inventory[slot].f.index == 0) put_empty(slot);
}
/* used, as DoSpecialActions does for a click on it: the weapon hand's weapon or launcher turns fight mode, else UseObj
   (a bag and the rune bag are ours: never used here) */
static void cmd_use(int idx)
{
    struct Object far *o = Obj_IntTMem(idx);
    int slot, id, cls;
    if (CursorObjPtr != 0 || !o) return;
    if (OBJ_MAJOR(o) == MAJOR_MISC && OBJ_MINOR(o) == MINOR_CONTAINER) return;
    slot = slot_of(idx);
    if (slot == INV_WEAPON_HAND - player->lefty) {
        id = OBJ_ITEM(o), cls = OBJ_CLASS(o);
        if (cls == CLASS_WEAPON || id == ITEM_SLING || id == ITEM_BOW || id == ITEM_CROSSBOW || id == ITEM_JEWELED_BOW) {
            toggle_fightmode();
            return;
        }
    }
    if (slot < 0 && !bag_of(idx)) return;
    UseObj(ThePlayer, o, 1);
}
static void rune_look(int rune)   /* mous_in_rune's right click: the rune looked at */
{
    struct StaticObj obj;
    if (rune < 0 || rune >= NUM_RUNES || !(player->runebag[rune >> 3] >> (7 - (rune & 7)) & 1)) return;
    memset(&obj, 0, sizeof obj);
    obj.ol.f.link = 0;
    SET_ITEM(&obj, rune + FIRST_RUNESTONE);
    obj.ol.f.owner = 0;
    LookAt((struct Object far *)&obj, 0);
}
static int inv_dbg(void)   /* UWPORT_INVDBG (the checks), read once */
{
    static int v = -1;
    if (v < 0) v = getenv("UWPORT_INVDBG") != 0;
    return v;
}
/* our save screen (plat_win32.c's menu pages): the description for the next save, and the game's own save and restore */
static char save_desc[40];
/* keeps the description typed for the next save */
void uwi_save_desc(const char *d) { snprintf(save_desc, sizeof save_desc, "%s", d ? d : ""); }
extern char *port_save_desc;               /* GAMEWRAP.C, uw-new: SaveGame's description given, no prompt */
void far DoSaveRest(int restore, int slot); /* GAMEWRAP.C: save or restore slot 1..4 and say how it went */
static int cmd_busy;   /* (a command's own prompt, AskHowMany, draws frames: no commands inside it) */
static void cmds_run(void)
{
    int dbg = inv_dbg();
    if (cmd_busy) return;
    cmd_busy = 1;
    for (;;) {
        int r = atomic_load_explicit(&cmd_r, memory_order_relaxed), op, a, b, c;
        if (r >= atomic_load_explicit(&cmd_w, memory_order_acquire)) break;
        op = cmds[r % CMDN].op, a = cmds[r % CMDN].a, b = cmds[r % CMDN].b, c = cmds[r % CMDN].c;
        atomic_store_explicit(&cmd_r, r + 1, memory_order_release);
        if (OpenBag != 0 && RightPanel == PANEL_INV) FixBagArea();   /* the original's panel kept on the pack (never used) */
        if (dbg) RLOG_I("inv", "cmd %d (%d, %d, %d) panel %d cursor %p", op, a, b, c, (int)RightPanel, (void *)CursorObjPtr);
        switch (op) {
        case UWI_OP_PICK: take(a, b); break;
        case UWI_OP_PUT_ON: put_on(a); break;
        case UWI_OP_PUT_SLOT: if (a >= INV_PACK && a <= INV_PACK_LAST) { if (Inventory[a].f.index) put_on(Inventory[a].f.index); else put_empty(a); } break;
        case UWI_OP_PUT_DISP:
            if (a >= 1 && a <= 11) {
                int slot = DisplayToSlot[a];
                if (slot >= 0 && slot < INV_PACK) { if (Inventory[slot].f.index) put_on(Inventory[slot].f.index); else put_empty(slot); }
            }
            break;
        case UWI_OP_USE: cmd_use(a); break;
        case UWI_OP_LOOK: port_look(a); break;
        case UWI_OP_DROP:
            if (CursorObjPtr == 0) break;
            port_throw_x = a, port_throw_y = b, port_throw_speed = c;
            port_grab_armed[1] = 1;
            port_key_grab(1);
            port_grab_armed[1] = 0;
            break;
        case UWI_OP_PACK: put_pack(); break;
        case UWI_OP_TAKE:
            mouse_putxy(a, b);
            port_grab_armed[0] = 1;
            port_key_grab(0);
            port_grab_armed[0] = 0;
            break;
        case UWI_OP_FIGHT: toggle_fightmode(); break;
        case UWI_OP_RUNE: if (GameInputMode == GIM_NONE) port_key_rune(a < 0 ? KEY_BACKSPACE : a == 23 ? 'y' : 'a' + a); break;   /* (mous_in_rune's own refusal in another mode) */
        case UWI_OP_RUNELOOK: rune_look(a); break;
        case UWI_OP_CAST: port_key_rune(' '); break;
        case UWI_OP_TARGET3D: port_target_at(a, b); break;
        case UWI_OP_AIM3D: port_aim_at(a, b); break;
        case UWI_OP_SAVE:
            if (a >= 1 && a <= 4) { port_save_desc = save_desc; DoSaveRest(0, a); port_save_desc = 0; }
            break;
        case UWI_OP_LOAD:
            if (a >= 1 && a <= 4) { DoSaveRest(1, a); punt_fightmode(); }   /* (as the autoload: no weapon drawn) */
            break;
        case UWI_OP_TARGET:
            if (GameInputMode == GIM_TARGET) {   /* mous_in_inv's GIM_TARGET: the thing clicked is the target, else cancelled */
                struct Object far *o = a >= 0 ? Obj_IntTMem(a) : 0;
                if (o && ObjectActor) (*ObjectActor)(o, 1, 1);
                if (CursorObjPtr) { unforce_mouse_cursor(3); CursorObjPtr = 0; GameInputMode = GIM_NONE; }
            }
            break;
        }
        if (dbg) RLOG_I("inv", "  -> cursor %p mode %d weight %d", (void *)CursorObjPtr, (int)GameInputMode, (int)player->weight);
    }
    cmd_busy = 0;
}

static void str2(char *out, int n, int idx)   /* string block 2 (the character's words) */
{
    char far *s = get_string(idx | STR_CHARGEN);
    int i;
    out[0] = 0;
    if (!s) return;
    for (i = 0; i < n - 1 && s[i]; i++) out[i] = s[i];
    out[i] = 0;
}

static void str1(char *out, int n, int idx)   /* string block 1 (the game's messages) */
{
    char far *s = get_string(idx | STR_GAME);
    int i;
    out[0] = 0;
    if (!s) return;
    for (i = 0; i < n - 1 && s[i]; i++) out[i] = s[i];
    out[i] = 0;
}

/* the full character sheet's numbers (uwinv.h), read only, each as the game's own code has it:
   - the armour by hit location and the defence: playerdat's, as FixPlayerEquips last set them (PLAYDATA.C);
   - the attack: DoPlayerWeapon's to-hit skill (COMBAT.C), worked out here without its stores (askill, damage,
     fromwho); the weapon as GetPlayerWeapon finds it in the weapon hand (slot 8 - lefty), without its check_ammo;
   - the words: print_info's (INTERACT.C, a click on the compass): hunger string 0x68 + hunger / 0x1E, fatigue
     0x76 - min(5, fatigue / 0x17), the level 0x19A + PlayerLevel, the time of day 0x47 + the period */
static void sheet_full(UwiSnap *s)
{
    struct Object far *weap = AskInventory(INV_WEAPON_HAND - player->lefty);
    struct Creature *pc = &Creature[ThePlayer->id & ID_INMAJOR];
    int item = weap ? OBJ_ITEM(weap) : -1, n, k;
    s->skill_points = player->skill_points;
    s->hunger = player->hunger, s->fatigue = player->fatigue, s->poison = player->poison;
    s->easy = player->easy != 0, s->drawn = player->drawn;
    for (k = 0; k < 4; k++) s->armour[k] = playerdat->armour[k];
    s->defence = playerdat->defence;
    s->w_weapon[0] = 0;
    if (weap) get_name(s->w_weapon, weap, 0, 0);
    if (item >= 0 && (item >> 4) == CLASS_MISSILE && Missile[item & ID_INCLASS].ammo >= 0 && Missile[item & ID_INCLASS].ammo < 0x10) {
        s->attack = -1, s->atk_skill = SKILL_MISSILE;
        s->dmg[0] = Missile[item & ID_INCLASS].damage, s->dmg[1] = s->dmg[2] = 0;
    } else {
        const struct Weapon *wd = item >= 0 && (item >> 4) == CLASS_WEAPON ? &Weapons[item & ID_INCLASS] : &Weapons[ITEM_FIST];
        int skill = wd->skill;
        if (skill >= SKILL_MISSILE || skill < SKILL_BAREHAND) skill = SKILL_BAREHAND;
        s->atk_skill = skill;
        s->attack = (player->skills[SKILL_ATTACK] >> 1) + player->skills[skill] + player->dexterity / 7 + (player->easy ? 7 : 0);
        for (k = 0; k < 3; k++)
            s->dmg[k] = skill == SKILL_BAREHAND ? player->skills[SKILL_BAREHAND] * 2 / 5 + pc->attr[ATTR_STR] / 6 + 4
                                                : wd->damage[k] + pc->attr[ATTR_STR] / 9;
        if (weap) {   /* an enchantment of major class 0xC: + (effect & 7) + 1 to damage (bit 3) or to hit */
            int16 major, effect;
            char flag;
            decode_obj_spell(weap, &major, &effect, &flag);
            if (!flag && major == 0xC) {
                if (effect & 8) for (k = 0; k < 3; k++) s->dmg[k] += (effect & 7) + 1;
                else s->attack += (effect & 7) + 1;
            }
        }
    }
    str1(s->w_hunger, sizeof s->w_hunger, player->hunger / 0x1E + 0x68);
    n = player->fatigue / 0x17;
    str1(s->w_fatigue, sizeof s->w_fatigue, 0x76 - (n > 5 ? 5 : n));
    s->dlevel = PlayerLevel;
    str1(s->w_level, sizeof s->w_level, PlayerLevel + 0x19A);
    n = (int)(player->game_clock / 0x1C2000L);
    s->day = n / 12 > 100 ? -1 : n / 12, s->period = n % 12;
    str1(s->w_period, sizeof s->w_period, 0x47 + s->period);
    s->w_day[0] = 0;
    if (s->day >= 0) str1(s->w_day, sizeof s->w_day, 0x19B + s->day);
}

static char wlab[6][20], wskn[20][24];
extern uint16 curZoom;       /* VIEW3D.C */
static int zoom_ours;        /* we set curZoom */
/* takes the snapshot of the inventory and runs the queued commands (game thread, once a frame) */
void uwi_capture(void)
{
    static UwiSnap s;
    int d;
    static int words;
    if (!player || !inplist) return;
    atomic_store(&g_mode, GameInputMode);
    port_inv_hook = cmds_run;   /* the commands run between the main loop's passes (MAINLOOP.C), not here: the panel's
                                   drawing (DisplayInvObject) inside the frame-shown callback never reached the page shown */
    /* the original's own pick (INTERACT.C's pick_3d) reads the 3D frame it drew, in its play lens (0x61A8: about 37 degrees
       either side of the middle); while our panel is open it draws with the widest (0x7ED2, VIEW3D.C's), so that more of
       our wider view has the original's picture behind it. Nothing shows it: ours draws the view. */
    if (!atomic_load(&active)) {
        if (zoom_ours && curZoom == 0x7ED2) curZoom = 0x61A8, cZoom(curZoom);
        zoom_ours = 0;
        return;
    }
    if (curZoom == 0x61A8) zoom_ours = 1, curZoom = 0x7ED2, cZoom(curZoom);
    if (RightPanel == PANEL_INV) atomic_store(&want_panel, 0);   /* (already there) */
    else if (RightPanel != PANEL_TURNING && atomic_exchange(&want_panel, 0)) pull_chain(0);   /* kept while the page turns */
    if (inv_dbg()) { static int was = -99; if (RightPanel != was) RLOG_I("inv", "RightPanel %d", RightPanel), was = RightPanel; }
    memset(&s, 0, sizeof s);
    tree_build(&s);
    if (inv_dbg()) {   /* the tree, whenever it changes (the checks) */
        static unsigned last;
        unsigned h = 2166136261u;
        for (d = 0; d < s.nnodes; d++) h = (h ^ (unsigned)(s.node[d].obj * 131 + s.node[d].parent * 7 + s.node[d].t.item + s.node[d].t.qty * 977)) * 16777619u;
        if (h != last) {
            last = h;
            char tb[1024];   /* the tree as a header line, then the nodes in lines of under 900 bytes (none is cut) */
            int tn = 0;
            RLOG_I("inv", "tree %d nodes, weight %d:", s.nnodes, (int)player->weight);
            for (d = 0; d < s.nnodes; d++) {
                if (tn > 850) RLOG_I("inv", "%s", tb), tn = 0;
                tn += snprintf(tb + tn, sizeof tb - tn, " [%d obj %d item %d x%d p%d s%d]", d, s.node[d].obj, s.node[d].t.item, s.node[d].t.qty, s.node[d].parent, s.node[d].slot);
            }
            for (d = 0; d < INV_PACK; d++) if (Inventory[d].f.index) {
                if (tn > 850) RLOG_I("inv", "%s", tb), tn = 0;
                tn += snprintf(tb + tn, sizeof tb - tn, " {slot %d obj %d}", d, (int)Inventory[d].f.index);
            }
            if (tn) RLOG_I("inv", "%s", tb);
        }
    }
    {   /* a thing asked to be shown (our grid's click on one in a bag): the original's panel brought to it first */
        int lk = atomic_exchange(&want_look, 0);
        if (lk > 0) port_look(lk - 1);
        int r = atomic_exchange(&want_reveal, 0);
        if (r > 0 && RightPanel == PANEL_INV) reveal(&s, r - 1);
        if (atomic_exchange(&want_pack, 0) && RightPanel == PANEL_INV && OpenBag) FixBagArea();
    }
    s.panel = RightPanel;
    s.mode = inplist->mode;
    s.input_mode = GameInputMode;
    s.lefty = player->lefty;
    for (d = 0; d < 3; d++) s.runebag[d] = player->runebag[d], s.shelf[d] = player->shelf[d];
    s.nrunes = player->nrunes;
    s.target = GameInputMode == GIM_TARGET || GameInputMode == GIM_AIM;
    for (d = 1; d <= DISP_BAG; d++) {
        int slot = DisplayToSlot[d], x, y;
        const struct InvRect *r = &InvDisplay[d];
        thing(&s.t[d], slot >= 0 && slot < NUM_INV_SLOTS && Inventory[slot].f.index ? Obj_PtrTMem(&Inventory[slot]) : 0);
        s.disp_obj[d] = slot >= 0 && slot < NUM_INV_SLOTS && Inventory[slot].f.index ? (int)Inventory[slot].f.index : -1;
        /* the point the original's own hit test gives this position */
        x = (r->left + r->right) / 2, y = (r->top + r->bottom) / 2;
        s.spot[d][0] = s.spot[d][1] = -1;
        s.rect[d][0] = r->left, s.rect[d][1] = 199 - r->top, s.rect[d][2] = r->right, s.rect[d][3] = 199 - r->bottom;
        if (FindInventoryHit(x, y) == d) s.spot[d][0] = x, s.spot[d][1] = 199 - y;
    }
    for (d = DISP_UP; d <= DISP_DOWN; d++) {
        const struct InvRect *r = &InvDisplay[d];
        int x = (r->left + r->right) / 2, y = (r->top + r->bottom) / 2;
        thing(&s.t[d], 0);
        s.spot[d][0] = s.spot[d][1] = -1;
        s.rect[d][0] = r->left, s.rect[d][1] = 199 - r->top, s.rect[d][2] = r->right, s.rect[d][3] = 199 - r->bottom;
        if (FindInventoryHit(x, y) == d) s.spot[d][0] = x, s.spot[d][1] = 199 - y;
    }
    s.held = CursorObjPtr != 0;
    thing(&s.h, CursorObjPtr);
    s.weight = player->weight, s.max_weight = player->max_weight;
    s.bag_open = OpenBag != 0;
    s.up = InvUpArrow, s.down = InvDownArrow;
    memcpy(s.name, player->name, sizeof s.name - 1);
    s.attr[0] = player->strength, s.attr[1] = player->dexterity, s.attr[2] = player->intelligence;
    for (d = 0; d < 20; d++) s.skill[d] = player->skills[d];
    s.health = player->health, s.maxhealth = player->maxhealth, s.mana = player->play_mana, s.maxmana = player->max_mana;
    s.level = player->level, s.exp = (int)(player->exp / 10);
    s.female = player->female, s.body = player->body;
    if (!words) {
        for (d = 0; d < 6; d++) str2(wlab[d], sizeof wlab[d], 17 + d);
        for (d = 0; d < 20; d++) str2(wskn[d], sizeof wskn[d], 31 + d);
        words = 1;
    }
    memcpy(s.lab, wlab, sizeof wlab), memcpy(s.skn, wskn, sizeof wskn);
    str2(s.cls, sizeof s.cls, 23 + (player->pclass & 7));
    sheet_full(&s);
    s.valid = 1;
    lock();
    snap = s;
    unlock();
    atomic_store(&have, 1);
}

/* copies the last snapshot out; 0 when none yet */
int uwi_snapshot(UwiSnap *out)
{
    if (!atomic_load(&have)) return 0;
    lock();
    *out = snap;
    unlock();
    return 1;
}

/* health, mana and poison for our HUD's beakers (read only) */
void uwi_vitals(int v[5])
{
    v[0] = player->health, v[1] = player->maxhealth, v[2] = player->play_mana, v[3] = player->max_mana;
    v[4] = player->poison;
}
