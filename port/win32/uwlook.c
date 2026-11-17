/* uwlook.c: the name of what the crosshair (the 3D view's centre) is on, for our full-screen display's readout.
   Taken on the game's thread where uwgame.c takes the 3D frame (uwl_capture, called after the frame was copied to
   the screen), from the original's own pick (INTERACT.C's pick_3d: a pick frame, each object's face in a colour of its
   own, read at the pointer; here the view's centre) and its own words (GAMESTRN.C's get_string and get_name, the
   quality words of LOOK.C's LookAt, CritterLook's attitude and name, look_nothing's texture words, RectLook's decals).
   LookAt and look_nothing themselves print to the message scroll, so they are not called: only the lookups. The
   game's state is left as it was: pick_3d's globals (PickMap, releasePtr, releaseable, pTxtId, inplist's x and y) are
   put back; nothing is printed, no random number is drawn. Off (uwl_enable(0)) the game is not touched at all. */
#include "compat.h"
#include "gfx.h"
#include "inv.h"
#include "items.h"
#include "object.h"
#include "player.h"
#include "sys.h"
#include "ui.h"
#include "view3d.h"
#include <stdatomic.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "uwlook.h"

extern int16 pTxtId;
extern struct Tile far *PickMap;
extern union Link far *releasePtr;
extern char releaseable;
struct Object far * far pick_3d(int how);

unsigned char far InPickRange(int dist, struct Object far *obj, struct Tile far *tile);   /* INTERACT.C */
char far BlockingTerrain(int dist, struct Object far *obj);
char far IsMobElem(struct Object far *obj);
extern int16 pQatt;                                   /* COMBAT.C: < 0 a swing in progress */
extern int16 m3dx, m3dy, m3dw, m3dh;                  /* MOUSE.C: the 3D view (m3dy its bottom edge) */

static atomic_int enabled;
static atomic_flag lk = ATOMIC_FLAG_INIT;
static char text[96];
static UwlInfo info;
static unsigned frame_n;

static void lock(void) { while (atomic_flag_test_and_set_explicit(&lk, memory_order_acquire)) {} }
static void unlock(void) { atomic_flag_clear_explicit(&lk, memory_order_release); }

void uwl_enable(int on) { atomic_store(&enabled, on); }

int uwl_get(char *out, int n)
{
    int len;
    lock();
    len = (int)strlen(text);
    if (len >= n) len = n - 1;
    memcpy(out, text, (size_t)len);
    out[len] = 0;
    unlock();
    return len;
}

static void add(char *s, int n, const char *w)
{
    int len = (int)strlen(s);
    while (*w && len < n - 1) s[len++] = *w++;
    s[len] = 0;
}

/* a texture's words (look_nothing's: txt is pTxtId's 1-based number) */
static void texture_words(char *s, int n, int txt)
{
    int t;
    txt--;
    if (txt < 0) return;
    if (txt < 0x30) t = TxmID[txt];
    else if (txt < 0x3A) t = 0x1FE - floor_IDs[txt - 0x30];
    else t = 0x1FF;
    add(s, n, get_string(t | STR_TEXTURES));
}

/* a creature as CritterLook says it (attitude, kind, name), without 'You see' and the full stop */
static void critter_words(char *s, int n, struct Object far *obj)
{
    char far *desc, far *name, far *attitude;
    unsigned char who;
    char tmp[96];
    desc = get_string(OBJ_ITEM(obj) | STR_OBJNAMES);
    if (*desc == 0) desc = 0;
    who = obj->whoami;
    if (who < 0xF0 || who == 0xFF) {
        attitude = get_string((OBJ_ATTITUDE(obj) + 0x60) | STR_OBJLOOK);
        if (*attitude == 0) attitude = 0;
    } else attitude = 0;
    if (who > 0) {
        name = get_string((who + 0x10) | STR_CONV);
        if (*name == 0) name = 0;
    } else name = 0;
    if (desc != 0 && attitude != 0) {
        switch (*attitude) {
        case 'a': case 'e': case 'i': case 'o': case 'u': add(s, n, "an "); break;
        default: add(s, n, "a ");
        }
        add(s, n, attitude), add(s, n, " ");
    }
    if (desc != 0 && (name == 0 || isupper((unsigned char)*name))) {
        strncpy(tmp, desc, sizeof tmp - 1), tmp[sizeof tmp - 1] = 0;   /* fix_name_string works in place: on a copy */
        add(s, n, fix_name_string(tmp, attitude == 0, 0));
    }
    if (name != 0) {
        if (desc != 0 && isupper((unsigned char)*name)) add(s, n, " named ");
        strncpy(tmp, name, sizeof tmp - 1), tmp[sizeof tmp - 1] = 0;
        add(s, n, fix_name_string(tmp, desc == 0, 0));
    }
}

/* an object as LookAt names it at no lore: a number or an article, the quality word, the name */
static void object_words(char *s, int n, struct Object far *obj)
{
    struct ComObj *com = &ComObjData[OBJ_ITEM(obj)];
    char far *qstr;
    char qual[26], name[64];
    char first = 0, plural = 0, num[8];
    int q = 0, v;
    if (OBJ_MAJOR(obj) == MAJOR_CREATURE) { critter_words(s, n, obj); return; }
    if (!com->lookable) {
        if (OBJ_CLASS(obj) == CLASS_DECAL) {
            switch (OBJ_INCLASS(obj)) {
            case 14: case 15: texture_words(s, n, obj->ol.f.owner + 1); return;   /* RectLook: the texture behind */
            case 4: add(s, n, "a bridge"); return;
            }
        }
        name[0] = 0;
        if (get_name(name, obj, 1, 0)) add(s, n, name);
        return;
    }
    qual[0] = 0;
    if (obj->qn.f.quality != 0) q = com->qualclass != 3 ? (obj->qn.f.quality >> 4) + 1 : 5;
    qstr = get_string((com->qualtype * 6 + q) | STR_OBJLOOK);
    if (qstr != 0 && *qstr != 0) {
        strncpy(qual, qstr, sizeof qual - 1), qual[sizeof qual - 1] = 0;
        first = *qstr;
    }
    if (OBJ_ISQUANT(obj) && !(obj->ol.f.link & LINK_SPECIAL) && obj->ol.f.link > 1) {
        plural = 1, first = 'x';
        v = obj->ol.f.link;
        num[0] = 0;
        if (v > 99) { num[0] = (char)('0' + v / 100 % 10), num[1] = (char)('0' + v / 10 % 10), num[2] = (char)('0' + v % 10), num[3] = 0; }
        else if (v > 9) { num[0] = (char)('0' + v / 10), num[1] = (char)('0' + v % 10), num[2] = 0; }
        else num[0] = (char)('0' + v), num[1] = 0;
        add(s, n, num), add(s, n, " ");
    } else if (first != 0 && com->qualtype != 13) {
        switch (first) {
        case 'a': case 'e': case 'i': case 'o': case 'u': add(s, n, "an "); break;
        default: add(s, n, "a ");
        }
    }
    if (qual[0]) add(s, n, qual), add(s, n, " ");
    name[0] = 0;
    if (get_name(name, obj, first == 0, plural)) add(s, n, name);
}

void uwl_info(UwlInfo *out)
{
    lock();
    *out = info;
    unlock();
}

/* What our display shows at the point the game's pointer stands for (the crosshair, or our panel's pointer over the
   world): the level's things whose pictures our renderer draws there, nearest first (uwview.dll's uwv_pick: each the
   object's index | its tile << 10), given by the window's thread each frame; -1 none given (the original's own pick
   alone). The original's pick reads its own picture of the view, which at a steep look (wide-pitch) is not ours, and
   finds the nearest thing's colour only, so a bloodstain over a sack hides the sack: uwl_pick takes ours. */
static int targets[UWL_TARGETS], ntargets = -1;
int uwl_door_targets = 1;   /* the Esc menu's Easier door targeting (uwfront.c): a door by its drawn frame and leaf, last */
struct Object far *uwl_pick(void);
#ifndef UWPORT_PLAIN
extern struct Object far *(*port_pick_hook)(void);   /* uw-new's port/ui/keys.c: q, e and the left button's n take it */
#endif
void uwl_set_targets(const int *t, int n)
{
#ifndef UWPORT_PLAIN
    port_pick_hook = uwl_pick;
#endif
    lock();
    ntargets = n;
    if (n > 0) memcpy(targets, t, sizeof(int) * (size_t)(n < UWL_TARGETS ? n : UWL_TARGETS));
    unlock();
}

/* the object at index idx, when it is on tile t's list (else 0) */
static struct Object far *on_tile(int idx, int t)
{
    struct Tile far *tile = mlowptr + t;
    struct Object far *want = Obj_IntTMem(idx), far *o;
    int guard = 0;
    for (o = Obj_PtrTMem(&tile->objects); o != 0 && guard < 1024; o = Obj_PtrTMem(&o->qn.link), guard++)
        if (o == want) return o;
    return 0;
}

/* The game's thread: the object at the pointer, as INTERACT.C's pick_3d gives it (its PickMap, releasePtr,
   releaseable and pTxtId set the same way), chosen from ours: the nearest that can be taken or is a creature, before
   anything else there (a bloodstain, debris, a decoration) within a tile behind the nearest; else the nearest. With
   nothing of ours there (or none given), the original's own pick: a door, a switch, writing on a wall, a texture. */
struct Object far *uwl_pick(void)
{
    int t[UWL_TARGETS], n, k, best = -1;
    struct Object far *obj;
    lock();
    n = ntargets < UWL_TARGETS ? ntargets : UWL_TARGETS;
    if (n > 0) memcpy(t, targets, sizeof(int) * (size_t)n);
    unlock();
    for (k = 0; k < n; k++) {
        int idx = t[k] & 1023, tile = (t[k] >> 10) & 4095;
        const struct ComObj *co;
        if (t[k] < 0) continue;                    /* a door by its drawn frame and leaf: the last resort, below */
        if ((obj = on_tile(idx, tile)) == 0) continue;
        if (best >= 0 && (t[k] >> 22) - (t[best] >> 22) > 8) break;   /* (bits 22..: the distance, eighths of a tile) */
        co = &ComObjData[OBJ_ITEM(obj)];
        if (best < 0) best = k;
        if ((co->pickable && !IsMobElem(obj)) || (OBJ_MAJOR(obj) == MAJOR_CREATURE && IsMobElem(obj))) { best = k; break; }
    }
    if (best >= 0 && (obj = on_tile(t[best] & 1023, (t[best] >> 10) & 4095)) != 0) {
        PickMap = mlowptr + ((t[best] >> 10) & 4095);
        releasePtr = &PickMap->objects;
        releaseable = ComObjData[OBJ_ITEM(obj)].pickable && !IsMobElem(obj);
        pTxtId = 0;
        return obj;
    }
    obj = pick_3d(2);
    /* the original's pick of a thing ours draws (an object lying about, a creature) that ours did not find there: its
       picture is elsewhere than ours (the steep look); the texture behind it is not known either */
    if (obj != 0 && n >= 0 && OBJ_ITEM(obj) < 0x140) obj = 0;
    /* nothing of ours and no object of the original's there: the door whose frame or leaf our display draws at the
       point (uwv_pick's bit 31, nearest first), when the Esc menu's Easier door targeting is on. The original's pick
       frame has the leaf alone, in the object's colour, and the frame in the wall's: the frame, and an open leaf seen
       edge-on, could not be pointed at. What is done with the door (player_3duse, its reach) is the original's. */
    if (obj == 0 && uwl_door_targets)
        for (k = 0; k < n; k++) {
            int item;
            if (t[k] >= 0 || (obj = on_tile(t[k] & 1023, (t[k] >> 10) & 4095)) == 0) continue;
            item = OBJ_ITEM(obj);
            if ((item < 0x140 || item > 0x14F || (item & 7) == 7) && item != 0x1CF) { obj = 0; continue; }
            PickMap = mlowptr + ((t[k] >> 10) & 4095);
            releasePtr = &PickMap->objects;
            releaseable = 0;
            pTxtId = 0;
            break;
        }
    return obj;
}

void uwl_capture(void)
{
    char out[96];
    struct Object far *obj = 0;
    int16 sx, sy, spt, smx, smy;
    struct Tile far *spm;
    union Link far *srp;
    char srl;
    UwlInfo in;
    if (!atomic_load(&enabled)) return;
    if (++frame_n % 3 != 0) {   /* the blow's charge and the held object every frame */
        lock();
        info.charging = pQatt < 0, info.held = CursorObjPtr != 0, info.drawn = player && player->drawn;
        info.held_item = CursorObjPtr ? OBJ_ITEM(CursorObjPtr) : -1, info.carry = GameInputMode == GIM_CARRY;
        unlock();
        return;
    }
    out[0] = 0;
    memset(&in, 0, sizeof in);
    if (inplist && xwid > 8 && xhgt > 8) {
        sx = inplist->x, sy = inplist->y, spt = pTxtId, spm = PickMap, srp = releasePtr, srl = releaseable;
        smx = MapObj_X, smy = MapObj_Y;
        {   /* the point the port's q and e act at (port/ui/keys.c's port_key_interact: the game's pointer, which mouse-look
               keeps at the view's centre, relative to the view), so that the readout names what a click there does */
            int16 mx, my;
            mouse_getxy(&mx, &my);
            if (mx < m3dx || mx > m3dx + m3dw || my > m3dy || my < m3dy - m3dh) inplist->x = xwid / 2, inplist->y = xhgt / 2;
            else inplist->x = mx - m3dx, inplist->y = my - (m3dy - m3dh);
        }
        obj = uwl_pick();
        if (obj != 0) {
            int item = OBJ_ITEM(obj);
            /* the old version's: creatures, doors (not a closed secret door, which would give itself away) and
               objects are named; walls, floors and the texture map objects are not */
            if (item == 0x147 || (obj->id & 0x1FE) == ITEM_TMAP_C) obj = 0;
            else {
                object_words(out, sizeof out, obj);
                in.kind = OBJ_MAJOR(obj) == MAJOR_CREATURE && IsMobElem(obj) ? 1 : item >= 0x140 && item <= 0x14F ? 2 : 3;
                in.hostile = in.kind == 1 && OBJ_ATTITUDE(obj) == 0;
                in.take = releaseable != 0;
                /* a stack: player_3dget asks 'Move how many?' for it */
                in.stack = OBJ_ISQUANT(obj) && !(OBJ_LINK(obj) & LINK_SPECIAL) && OBJ_LINK(obj) != 1;
                in.open = item >= 0x148 && item <= 0x14F;
                in.sw = item >= 0x170 && item <= 0x17F;
                /* the reach as player_3duse and player_3dget test it (InPickRange sets MapObj_X, MapObj_Y: put back) */
                in.reach = InPickRange(PickDist, obj, PickMap) && !BlockingTerrain(PickDist, obj);
            }
        }
        MapObj_X = smx, MapObj_Y = smy;
        inplist->x = sx, inplist->y = sy, pTxtId = spt, PickMap = spm, releasePtr = srp, releaseable = srl;
    }
    if (!obj) out[0] = 0;
    if (getenv("UWPORT_LOOKFAKE")) snprintf(out, sizeof out, "%s", getenv("UWPORT_LOOKFAKE")), in.kind = 3;   /* a test: the readout's look */
    snprintf(in.name, sizeof in.name, "%s", out);
    in.charging = pQatt < 0, in.held = CursorObjPtr != 0, in.drawn = player && player->drawn;
    in.held_item = CursorObjPtr ? OBJ_ITEM(CursorObjPtr) : -1, in.carry = GameInputMode == GIM_CARRY;
    lock();
    info = in;
    unlock();
    if (frame_n % 150 == 0 && getenv("UWPORT_LOOKDBG")) fprintf(stderr, "uwlook: tick %u pick %p txt %d centre %d pickup %d\n", frame_n, (void *)obj, (int)pTxtId, stdat[(xhgt / 2) * (xwid + 2) + xwid / 2 + 2], (int)PickUp);
    if (strcmp(out, text) != 0 && getenv("UWPORT_LOOKDBG"))
        fprintf(stderr, "uwlook: \"%s\" kind %d hostile %d reach %d take %d open %d switch %d at %ld ms\n", out, in.kind, in.hostile,
                in.reach, in.take, in.open, in.sw, (long)(clock() * 1000L / CLOCKS_PER_SEC));
    lock();
    memcpy(text, out, sizeof text);
    unlock();
}

/* the game's own state, read (no change): the options panel's music and sound, the left panel's page (1 the options) */
extern int16 LeftPanel;
char far music_is_on(void);
char far fx_is_on(void);
int uwl_music_on(void) { return music_is_on() != 0; }
int uwl_fx_on(void) { return fx_is_on() != 0; }
int uwl_left_panel(void) { return LeftPanel; }

/* uwlook.h: the gargoyle's eyes and the charge, for our HUD (read only) */
extern unsigned char setting[9];                      /* PANELS.C: the screen pieces' frames, [7] the eyes */
extern unsigned char play_pow;                        /* COMBAT.C: the charge, 0..100 */
void uwl_combat(int *eyes, int *pow)
{
    *eyes = setting[7];
    *pow = pQatt < 0 && pQatt > -10 ? (play_pow > 100 ? 100 : play_pow) : -1;
}
