#include "game.h"
#include "core_world.h"
#include <stdlib.h>

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "conv.h"
#include "inventory.h"
#include "objdata.h"
#include "orig.h"
#include "render.h"
#include "sound.h"
#include "ui.h"
#include "world.h"

static void door_sound(const Door *d); /* (the sounds: below) */
static void sfx_play_(int id, int pan, int vol);

#define REACH 2.2f /* tiles: how far away things can be picked up (and the look tag shows) */

static struct {
    char data[600];
    Level *lv;
    Mesh *mesh;
    const Light *lights;
    int nlights;
    bool open;             /* the inventory panel */
    bool fight;            /* fight mode (the original's Fight icon): only then does a click attack */
    bool sheet;            /* the character panel */
    u32 clock;             /* the game's clock, 1/256 s (player record 0xce; a new character: 0x010b3000, 19:00 day 1) */
    float eye[3];          /* where the player is (for sleep's look round) */
    float dark;            /* sleep's fade, seconds left */
    int silver_level;      /* where the silver tree grows (0 none; player byte 0x5e's high bits) */
    float silver_x, silver_z;
    bool dead;             /* died with no silver tree: the panel asks what next */
    bool playing;          /* an instrument: number keys play, Esc puts it down */
    bool chanting;         /* at a shrine: a mantra being typed */
    bool runes_open;       /* the rune panel */
    int shelf[3], nshelf;  /* the runes on the shelf (0 An .. 23 Ylem) */
    int nactive;           /* the lasting spells (at most three, UW.EXE 0x2b109) */
    u32 cast_time;         /* DS 0x16e3: the clock at the last cast; DS 0x16e2 the wait after it */
    u8 cast_wait;
    int act_cat[3], act_str[3], act_ticks[3];
    int wards[8][2], nwards; /* runes of warding: their tiles */
    char mantra[24];
    int swim;              /* the swimmer's tiring (player byte 0xb9): 96 on entering water, 0 out of it */
    int swim_slot;         /* the swimming sound's effect slot (DS 0x77e; 0 is "none" until started: see below) */
    bool swim_sound;       /* it plays */
    u32 swim_start;        /* when it began (DS 0x77f, the 256 Hz game clock) */
    u32 next_step;         /* the next footstep's time (nextstep) */
    bool step_foot;        /* which foot (step_foot: 1 the right, effect 1 panned 0x48; 0 the left, 2 at 0x38) */
    float step_x, step_z;  /* where the eye was last turn (the speed: ours, see the footsteps) */
    float lava_t;          /* time on lava since its last bite */
    float yaw;
    int death_choice;
    bool tag;              /* the crosshair is on something to look at: the left button looks */
    float charge;          /* the attack being charged, 0-1; -1 none */
    float anim;            /* seconds, for the full power gem's sparkle */
    /* what the crosshair is on */
    int hover_obj, hover_tx, hover_tz;
    /* messages */
    char msg[4][600];
    float msg_age[4];
    float hp, hp_max, mana, mana_max;
    int mouse_x, mouse_y;
    /* doors */
    Door doors[128];
    float door_target[128];
    int ndoors, hover_door, hover_creature;
    bool talking;
    bool reading;
    char read_title[96], read_text[8192];
    int read_scroll;
    int option_y[CONV_MAX_OPTIONS], option_h[CONV_MAX_OPTIONS], options_x;
    /* the move triggers: the tile the player was in; where a teleport sends them */
    int last_tx, last_tz;
    bool travel;
    int travel_level;
    float travel_x, travel_z;
    /* the cards of the items and torches (rebuilt when they change), the doors' quads added every frame */
    Vertex *cards;
    int ncards;
} G;

void game_message(const char *fmt, ...) {
    for (int i = 3; i > 0; i--) memcpy(G.msg[i], G.msg[i - 1], sizeof G.msg[0]), G.msg_age[i] = G.msg_age[i - 1];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(G.msg[0], sizeof G.msg[0], fmt, ap);
    va_end(ap);
    for (size_t k = strlen(G.msg[0]); k > 0 && (G.msg[0][k - 1] == '\n' || G.msg[0][k - 1] == ' '); k--) G.msg[0][k - 1] = 0;
    G.msg_age[0] = 0;
    logf_("%s\n", G.msg[0]);
}

void game_read(const char *title, const char *text) {
    str_copy(G.read_title, sizeof G.read_title, title);
    str_copy(G.read_text, sizeof G.read_text, text);
    G.reading = true, G.read_scroll = 0;
}

/* the reading panel: where it is and how much text fits */
static void read_rect(int *x, int *y, int *w, int *h) {
    Overlay *o = ui_overlay();
    *w = MIN(o->w - 40, 320), *h = MIN(o->h - 80, 200), *x = (o->w - *w) / 2, *y = 30;
}

static void character_load(void);
static int character_vitality(void);
static void character_derive(bool refill);
bool game_start(const char *data_dir, bool give_torch) {
    memset(&G, 0, sizeof G);
    str_copy(G.data, sizeof G.data, data_dir);
    objdata_load(data_dir);
    inv_start(data_dir, give_torch);
    character_load();
    G.hp = G.hp_max = (float)character_vitality(), G.mana = G.mana_max = 20;
    character_derive(true);
    G.hover_obj = -1, G.charge = -1;
    G.clock = 0x010b3000u;
    uw_srand((u16)time(NULL)); /* UW.EXE 0x6a947: srand(time()) */
    return true;
}

/* "a torch": the name the messages use */
static void item_name(const Object *o, char *out, size_t n) { obj_name(o, out, n); }

static Vertex g_cards[RENDER_MAX_SPRITE_VERTS];
bool game_blocked(float x, float z, float r);
static int g_ncr; /* (the creatures: below) */
static int creature_cards(Vertex *out, int cap);
static int missile_cards(Vertex *out, int cap);
static void creatures_load(void);
static void rebuild_sprites(void) { G.ncards = world_sprites(G.lv, G.mesh, G.lights, G.nlights, g_cards, RENDER_MAX_SPRITE_VERTS - 84 * 128); } /* room for 128 doors (84 vertices each) */
static void upload_sprites(void) {
    int n = G.ncards;
    for (int k = 0; k < G.ndoors; k++) n += world_door_quads(G.lv, G.mesh, &G.doors[k], g_cards + n, RENDER_MAX_SPRITE_VERTS - n);
    n += creature_cards(g_cards + n, RENDER_MAX_SPRITE_VERTS - n);
    n += missile_cards(g_cards + n, RENDER_MAX_SPRITE_VERTS - n);
    render_set_sprites(g_cards, n);
}

void game_remesh(Mesh *m, const Light *lights, int nlights) {
    G.mesh = m, G.lights = lights, G.nlights = nlights;
    rebuild_sprites();
    upload_sprites();
}

void game_level(Level *lv, Mesh *m, const Light *lights, int nlights) {
    G.lv = lv, G.mesh = m, G.lights = lights, G.nlights = nlights;
    /* the doors (0x140-0x147 shut, 0x148-0x14F open) */
    G.ndoors = 0, G.hover_door = -1, G.last_tx = G.last_tz = -1;
    for (int t = 0; t < 64 * 64 && G.ndoors < 128; t++)
        for (int i = lv->tile[t].first_object, guard = 0; i > 0 && i < 1024 && guard < 1024; i = lv->obj[i].next, guard++) {
            const Object *o = &lv->obj[i];
            if (o->item < 0x140 || o->item > 0x14F || G.ndoors >= 128) continue;
            Door *d = &G.doors[G.ndoors];
            d->tx = t % 64, d->tz = t / 64, d->heading = o->heading & 6, d->item = o->item, d->obj = i;
            d->open = o->item >= 0x148 ? 1.0f : 0.0f;
            G.door_target[G.ndoors++] = d->open;
        }
    creatures_load();
    {
        int kinds = 0, frames = 0;
        for (int k = 0; k < 64; k++)
            if (m->crit[k].loaded) kinds++, frames += m->crit[k].nframes;
        logf_("level %d: %d creatures, %d kinds with %d frames\n", lv->number, g_ncr, kinds, frames);
    }
    rebuild_sprites();
    upload_sprites();
}

/* --- doors ------------------------------------------------------------------------------------------- */
static float seg_dist2(float px, float pz, float ax, float az, float bx, float bz) {
    float dx = bx - ax, dz = bz - az, l = dx * dx + dz * dz, t = l > 0 ? ((px - ax) * dx + (pz - az) * dz) / l : 0;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    float qx = ax + dx * t - px, qz = az + dz * t - pz;
    return qx * qx + qz * qz;
}

bool game_blocked(float x, float z, float r) {
    for (int k = 0; k < G.ndoors; k++) {
        const Door *d = &G.doors[k];
        if (fabsf(x - (d->tx + 0.5f)) > 1.0f || fabsf(z - (d->tz + 0.5f)) > 1.0f) continue;
        bool sx = d->heading == 0 || d->heading == 4;
        float cx = d->tx + 0.5f, cz = d->tz + 0.5f, ax = sx ? 1.0f : 0.0f, az = sx ? 0.0f : 1.0f, hw = DOOR_WIDTH * 0.5f;
        /* the frames beside the door */
        if (seg_dist2(x, z, cx - ax * 0.5f, cz - az * 0.5f, cx - ax * hw, cz - az * hw) < r * r) return true;
        if (seg_dist2(x, z, cx + ax * hw, cz + az * hw, cx + ax * 0.5f, cz + az * 0.5f) < r * r) return true;
        /* the door while it is mostly shut */
        if (d->open < 0.6f && seg_dist2(x, z, cx - ax * hw, cz - az * hw, cx + ax * hw, cz + az * hw) < r * r) return true;
    }
    return false;
}

/* is the door's lock still locked: its link is a lock (0x10F) */
/* the door's lock: the lock object (0x10F) its link leads to, or 0. UW.EXE (file 0x3831b): locked when the
 * lock's item word bit 9 (our flags bit 0) is set; its number is its link & 0x1ff */
static int door_lock(const Door *d) {
    int l = G.lv->obj[d->obj].link;
    return l > 0 && l < 1024 && G.lv->obj[l].item == 0x10F ? l : 0;
}
static bool locked(const Door *d) {
    int l = door_lock(d);
    return l && (G.lv->obj[l].flags & 1);
}

/* the door the view ray meets within reach */
static void pick_door(const float eye[3], const float fwd[3], float *best) {
    G.hover_door = -1;
    for (int k = 0; k < G.ndoors; k++) {
        const Door *d = &G.doors[k];
        bool sx = d->heading == 0 || d->heading == 4;
        float cx = d->tx + 0.5f, cz = d->tz + 0.5f, y0, y1;
        world_door_span(G.lv, d, &y0, &y1);
        /* the doorway's plane: z = cz (spans x) or x = cx */
        float den = sx ? fwd[2] : fwd[0];
        if (fabsf(den) < 1e-4f) continue;
        float t = ((sx ? cz : cx) - (sx ? eye[2] : eye[0])) / den;
        if (t < 0.05f || t > *best) continue;
        float hx = eye[0] + fwd[0] * t, hy = eye[1] + fwd[1] * t, hz = eye[2] + fwd[2] * t;
        float across = sx ? hx - cx : hz - cz;
        if (fabsf(across) > DOOR_WIDTH * 0.5f + 0.05f || hy < y0 || hy > y1) continue;
        *best = t, G.hover_door = k;
    }
}

/* The lock (UW.EXE 0x3831b): key > 0 a key's number, < 0 a lockpick (minus the Picklock skill). 1 no lock; unlocked:
 * a lockpick 4, an open door 4, the right key locks it (2), else 0; locked: the right key, or a Picklock check
 * against z x 3 (z 15: never; z 14: only with 31), unlocks it (3), else 0 */
static int skill_check(int skill, int difficulty);
static int lock_try(Door *d, int key) {
    int l = door_lock(d);
    if (!l) return 1;
    Object *lock = &G.lv->obj[l];
    bool open = (d->item & 8) != 0 || G.door_target[d - G.doors] > 0.5f;
    if (!(lock->flags & 1)) {
        if (key <= 0 || open) return 4;
        if ((lock->link & 0x1ff) != key) return 0;
        lock->flags |= 1;
        return 2;
    }
    if (key < 0) {
        int z = lock->z & 0x7f;
        if (z == 15 || (z == 14 && -key < 31) || skill_check(-key, z * 3) <= 0) return 0;
    } else if (!(lock->link & 0x1ff) || (lock->link & 0x1ff) != key) return 0;
    lock->flags &= (u8)~1;
    return 3;
}
/* --- what the floor is (TERRAIN.DAT): water, lava ------------------------------------------------------------ */
static u8 g_terrain[256]; /* floor texture -> 0 floor, 1 water, 2 lava */
static bool g_terrain_loaded;
static int terrain_at(int tx, int tz) {
    if (!g_terrain_loaded) {
        g_terrain_loaded = true;
        char path[700];
        snprintf(path, sizeof path, "%s\\levels\\terrain.txt", G.data);
        size_t mark = arena_mark();
        Bytes f = file_read(path);
        for (const char *p = (const char *)f.data; p && p < (const char *)f.data + f.size;) {
            int t;
            char kind[16];
            if (sscanf(p, "floor %d %15s", &t, kind) == 2 && t >= 0 && t < 256) g_terrain[t] = !strcmp(kind, "water") ? 1 : !strcmp(kind, "lava") ? 2 : 0;
            const char *nl = memchr(p, '\n', (size_t)((const char *)f.data + f.size - p));
            p = nl ? nl + 1 : NULL;
        }
        arena_reset(mark);
    }
    if (!G.lv || tx < 0 || tx > 63 || tz < 0 || tz > 63) return 0;
    const Tile *t = &G.lv->tile[tz * 64 + tx];
    if (t->type == T_SOLID) return 0;
    return g_terrain[G.lv->floors[t->floor % 10] & 255];
}

/* a spiked door (UW.EXE 0x36c42 sets its quality to 63; looked at, a closed door with an odd quality "is spiked",
 * 0x7c6c5): not opened by hand (its quality / 2 is what it takes to break it); the portcullises, 63 from the start,
 * are the levers' */
static bool spiked(const Door *d) {
    const Object *o = &G.lv->obj[d->obj];
    return (o->item & 0x1ff) >= 0x140 && (o->item & 0x1ff) <= 0x145 && (o->quality & 1) && !(d->item & 8);
}

static const char *s1(int i);
static void use_switch(int i);
static int rnd(int n);
static int place_object(int item, int quality, float x, float z);
static const char *player_name(void);
/* the thing on the pointer used on what is under the pointer in the world (UW.EXE's "use ... on" targets, 0x36b9a):
 * keys and lockpicks on doors, spikes on closed doors. False: it has no use there (it is put down instead). */
static bool use_held_on_world(void) {
    int owner = 0, item = inv_held_item(&owner);
    if (item < 0) return false;
    if (G.hover_obj >= 0) {
        Object *t = &G.lv->obj[G.hover_obj];
        int ti = t->item & 0x1ff;
        if (item == 0xd8) { /* a pole (0x36e62): a switch out of reach */
            if (ti >= 0x170 && ti <= 0x17f) game_message("%s", s1(157)), use_switch(G.hover_obj), rebuild_sprites();
            else game_message("%s", s1(158));
            inv_put_back();
            return true;
        }
        if (item == 0x128) { /* a rock hammer (0x3797e): a boulder into smaller pieces, the smallest into sling stones */
            if (ti < 0x153 || ti > 0x156) return game_message("%s", s1(132)), inv_put_back(), true;
            float x = (float)G.hover_tx + (t->x + 0.5f) / 8.0f, z = (float)G.hover_tz + (t->y + 0.5f) / 8.0f;
            int pieces = rnd(2) + (0x155 - ti) + 1;
            u16 *at = &G.lv->tile[G.hover_tz * 64 + G.hover_tx].first_object;
            for (int guard = 0; *at && guard < 1024; guard++, at = &G.lv->obj[*at].next)
                if (*at == G.hover_obj) {
                    *at = t->next;
                    memset(t, 0, sizeof *t);
                    break;
                }
            for (int k = 0; k < pieces; k++) {
                int kind = ti + rnd(2) + 1;
                place_object(kind > 0x156 ? 0x10 : kind, 40, x + (float)(rnd(5) - 2) * 0.08f, z + (float)(rnd(5) - 2) * 0.08f);
            }
            game_message("%s", s1(135));
            inv_put_back();
            rebuild_sprites();
            return true;
        }
        if (item >= 0xc2 && item <= 0xc6) { /* bones on a gravestone (0x36cf9): Garamon's, on his grave, rest */
            if (ti != 0x165) return game_message("%s", s1(132)), inv_put_back(), true;
            if (owner == 62 && t->link == 0x221) {
                game_message("%s", s1(134));
                inv_held_consume();
                if (conv_start(G.data, 27, "Garamon", player_name())) G.talking = true;
            } else game_message("%s", s1(259)), inv_put_back();
            return true;
        }
    }
    if (G.hover_door >= 0) {
        Door *d = &G.doors[G.hover_door];
        if (item >= 0x100 && item <= 0x10e && item != 0x101) { /* a key (0x36afd): "The key does not fit." ... */
            int r = lock_try(d, owner);
            game_message("%s", str_get(1, 2 + r));
            inv_put_back();
            return true;
        }
        if (item == 0x101) { /* a lockpick (0x36a78) */
            int r = lock_try(d, -game_skill(16));
            static const int MSG[5] = {120, 3, 120, 121, 122};
            if (r == 2 || r == 3) sfx_play_(19, 0x40, 0); /* the lock picked (0x36ae2) */
            game_message("%s", str_get(1, MSG[r]));
            inv_put_back();
            return true;
        }
        if (item == 0x127) { /* a spike (0x36c42) */
            Object *o = &G.lv->obj[d->obj];
            if ((o->item & 0x1ff) < 0x140 || (o->item & 0x1ff) > 0x147 || (d->item & 8)) game_message("%s", str_get(1, 128)), inv_put_back();
            else o->quality = 63, game_message("%s", str_get(1, 129)), inv_held_consume();
            return true;
        }
    }
    return false;
}

static void use_door(int k) {
    Door *d = &G.doors[k];
    if ((d->item & 7) == 7 && G.door_target[k] < 0.5f) return game_message("There is nothing to open there.");
    if (G.door_target[k] < 0.5f && spiked(d)) return game_message("%s", str_get(1, 131)); /* "The door is spiked." */
    if (G.door_target[k] < 0.5f && locked(d)) {
        /* a key in the pack that fits turns and the door opens, one press (the original has the key used on the
         * lock, UW.EXE 0x36afd, then the door used; here E on the door finds the key carried) */
        Object *lock = &G.lv->obj[door_lock(d)];
        if (!inv_has_key(lock->link & 0x1ff)) {
            char noun[64];
            obj_noun(&G.lv->obj[d->obj], noun, sizeof noun);
            return game_message("The %s is locked.", noun); /* UW.EXE builds it: "The " + name + " is locked." (file 0x381b8) */
        }
        lock->flags &= (u8)~1;
        game_message("%s", str_get(1, 5)); /* "The key unlocks the lock." */
    }
    G.door_target[k] = G.door_target[k] < 0.5f ? 1.0f : 0.0f;
    d->item = (d->item & ~8) | (G.door_target[k] > 0.5f ? 8 : 0);
    G.lv->obj[d->obj].item = (u16)d->item;
    door_sound(d);
}


/* --- creatures ----------------------------------------------------------------------------------------
 * Each mobile object 0x40-0x7F of the level. Animations by the page's slots: 0 idle, 1 attack, 5 walk (when
 * there), 7 hit, 12 death (unconfirmed names, read from how the frames look). Hostile by its attitude bits (the object's
 * word at 0x0d, bits 14-15: 0 hostile; below), not by whether it has a conversation. A simple behaviour for now: one that sees the player
 * within 7 tiles comes and strikes; the rules from UW.EXE come later. */
enum { CS_IDLE, CS_WALK, CS_ATTACK, CS_HIT, CS_DYING, CS_DEAD };
typedef struct {
    int obj, kind, state, tile;
    float x, z, y, hp, t, cooldown;
    float frozen, fear;    /* spells: paralysed (goal 7: stands); afraid (goal 6: flees) */
    float confused;        /* Confusion (goal 2: wanders, attitude upset) */
    bool ally;             /* Ally, a summoned creature (mobile byte 0x19 bit 6): friendly */
    bool hostile, struck;
    int dir; /* the way it faces, in eighths (0 north) */
    int walk_i; /* the walk's frame last turn (its sounds on the odd ones) */
} Creature;
static void sfx_play_(int id, int pan, int vol);
static void sfx_at_(int id, float x, float z, int vol);
static void blow_sound(bool blocked, bool player_attacks, const Creature *c, int part);
static void creature_death_sound(const Creature *c);
static Creature g_cr[256];
static u32 g_quest;    /* (declared here for the saves; the trap section has their story) */
static u8 g_var[128];
static int g_ncr;

/* The weapon in the view (UW.EXE 0x33ffd draws, 0x330c9 steps it): WEAPONS.GR, 28 pictures a kind (sword, axe,
 * mace, fist: the weapon's skill 3-5 less 3; nothing in the hand, or not a hand weapon: the fist; a bow, crossbow
 * or sling: none, 0x7e580). At rest picture 27 (its tip at the bottom). A swing (slash 0, bash 1, thrust 2) shows
 * picture 9 x type + f: f steps 0-3 drawing back (out of sight), stays at 3 while the attack charges, then 4-8
 * swings; the blow lands at f 6 (0x257a8) and at 9 it is over. A step every 1/16 s (the update is a timed event
 * whose rate was not found; the charge's own tick). */
static struct {
    int type, f;      /* type -1: at rest */
    float up;         /* 0 put away .. 1 raised: fight mode raises it (UW.EXE states 5 / 3, pictures 18-20) */
    float t;          /* time to the next step */
    float charge;     /* the charge when let go (-1: still held) */
    float eye[3], fwd[3];
    bool loaded;
    int ox[224], oy[224];
} W = {-1, -1};
static int weapon_kind(void) {
    int w = inv_weapon();
    if (w < 0 || w == 15) return 3;
    if (w < 0x10) return MAX(3, MIN(5, obj_info(w)->skill)) - 3;
    if (w >= 0x18 && w < 0x20) return -1;
    return 3;
}
static void weapon_offsets(void) {
    if (W.loaded) return;
    W.loaded = true;
    char path[700];
    snprintf(path, sizeof path, "%s\\ui\\weapons\\offsets.txt", G.data);
    size_t mark = arena_mark();
    Bytes f = file_read(path);
    for (const char *p = (const char *)f.data; p && p < (const char *)f.data + f.size;) {
        int k, x, y;
        if (*p != '#' && sscanf(p, "%d %d %d", &k, &x, &y) == 3 && k >= 0 && k < 224) W.ox[k] = x, W.oy[k] = y;
        p = memchr(p, '\n', (size_t)((const char *)f.data + f.size - p));
        if (p) p++;
    }
    arena_reset(mark);
}
static int attack_type(const float eye[3], const float fwd[3]);
static void player_attack(const float eye[3], const float fwd[3], float charge, int type);

/* --- missiles (UW.EXE 0x2b2bd): sling stones, bolts, arrows in flight -------------------------------------------
 * OBJECTS.DAT 0x82: 3 bytes a kind 0x10-0x1F: the ammunition's damage, ?, 0xC0 for what is shot by skill; a
 * launcher's third byte is its ammunition (sling 0x10, crossbow 0x11, bows 0x12). Shot by the player: speed =
 * Missile x 8 + 0xC0, a Missile check against 10: fumble - 0x80, critical + 0xC0; damage = ammunition damage x speed
 * / 256. Flying (our tiles a second: speed / 20), a hit on a creature is the melee check (attack / 2 + Missile + dex
 * / 7 against its defense: our estimate of the impact), a miss lies where it fell. */
typedef struct {
    float x, y, z, vx, vy, vz, life;
    int item, damage;
    bool player;
} Missile;
static Missile g_ms[32];
static int g_nms;
static const u8 LAUNCH[16][3] = {{5, 15, 192}, {10, 26, 192}, {8, 22, 192}, {8, 10, 192}, {28, 24, 245}, {16, 20, 221}, {8, 13, 240}, {8, 15, 189},
                                 {3, 15, 0},  {3, 15, 2},   {3, 15, 1},   {3, 15, 255}, {3, 15, 255}, {3, 15, 255}, {3, 15, 255}, {3, 15, 2}};
static int skill_check(int skill, int difficulty);
static int rnd(int n);
static void missile_fire(int item, int damage, const float from[3], const float dir[3], float speed, bool player) {
    if (g_nms >= 32) return;
    Missile *m = &g_ms[g_nms++];
    m->x = from[0], m->y = from[1], m->z = from[2];
    float v = speed / 20.0f;
    m->vx = dir[0] * v, m->vy = dir[1] * v, m->vz = dir[2] * v;
    m->item = item, m->damage = damage, m->player = player, m->life = 4.0f;
    sfx_at_(0xa, m->x, m->z, 0); /* every shot as it leaves, at it (UW.EXE missile_fire) */
}
/* the player shoots: the launcher in the right hand, its ammunition from the pack */
static bool player_shoot(const float eye[3], const float fwd[3]) {
    int l = inv_slot_item(7);
    if (l < 0x18 || l > 0x1f || LAUNCH[l - 0x10][2] > 2) return false;
    int ammo = 0x10 + LAUNCH[l - 0x10][2];
    float ahead[3] = {eye[0] + fwd[0] * 0.5f, eye[1], eye[2] + fwd[2] * 0.5f};
    if (level_solid_at(G.lv, ahead[0], ahead[2])) return game_message("%s", s1(254)), true; /* "You need more space to fire that weapon." */
    if (!inv_consume_one(ammo)) return game_message("Thou hast nothing to shoot."), true;
    int speed = game_skill(6) * 8 + 0xc0, r = skill_check(game_skill(6), 10);
    if (r < 0) speed -= 0x80;
    else if (r == 2) speed += 0xc0;
    missile_fire(ammo, LAUNCH[ammo - 0x10][0] * speed / 256, eye, fwd, (float)speed, true);
    if (l == 0x19 || l == 0x1a) sfx_play_(9, 0x40, 0); /* a bow or crossbow (not the sling or the jeweled bow): its
                                                        * twang (UW.EXE player_fire) */
    return true;
}
static void creature_remove(Creature *c);
static void gain_exp(int v);
static void kill_exp(const Creature *c);
static int dice(int n, int sides);
static int roll_damage(int base);
static void missiles_update(float dt, const float eye[3]) {
    for (int k = 0; k < g_nms; k++) {
        Missile *m = &g_ms[k];
        m->life -= dt, m->vy -= 1.2f * dt;
        float nx = m->x + m->vx * dt, ny = m->y + m->vy * dt, nz = m->z + m->vz * dt;
        bool stop = m->life <= 0 || level_solid_at(G.lv, nx, nz) || ny <= level_floor_y(G.lv, nx, nz) || ny >= level_ceil_y();
        /* a creature in its way (not the one who shot it) */
        for (int c = 0; c < g_ncr && !stop; c++) {
            Creature *cr = &g_cr[c];
            if (cr->state >= CS_DYING || !m->player) continue;
            float dx = cr->x - nx, dz = cr->z - nz;
            if (dx * dx + dz * dz > 0.3f * 0.3f || ny < cr->y || ny > cr->y + 0.75f) continue; /* CREATURE_HEIGHT */
            const ObjInfo *ci = obj_info(0x40 + cr->kind);
            int score = game_skill(0) / 2 + game_skill(6) + game_attr(1) / 7 + (game_easy() ? 7 : 0);
            cr->hostile = true;
            if (skill_check(score, ci->defense) > 0) {
                sfx_at_(4, cr->x, cr->z, 0); /* the creature struck (UW.EXE missile_thwack: effect 4 on it) */
                int dmg = MAX(0, roll_damage(MAX(2, m->damage)) - ci->carmour[0]);
                cr->hp -= (float)dmg;
                if (cr->hp > 0) sound_song_blow(G.clock, true, (int)cr->hp, ci->hp); /* (UW.EXE 0x1c44d) */
                else sound_song_set_next(9);                                         /* a kill (0x25b56) */
                logf_("missile: the player's %d hits creature %d for %d\n", m->item, c, dmg);
                if (cr->hp <= 0) {
                    cr->state = CS_DYING, cr->t = 0, creature_death_sound(cr);
                    creature_remove(cr);
                    kill_exp(cr);
                } else cr->state = CS_HIT, cr->t = 0;
            }
            stop = true, m->life = -1; /* spent on the creature */
        }
        /* a trap's shot at the player */
        if (!stop && !m->player) {
            float dx = eye[0] - nx, dz = eye[2] - nz;
            if (dx * dx + dz * dz < 0.3f * 0.3f && ny < eye[1] + 0.1f && ny > eye[1] - 0.6f) {
                sfx_play_(3, 0, 0); /* the player struck (missile_thwack: effect 3, panned 0) */
                G.hp -= (float)roll_damage(MAX(2, m->damage));
                stop = true, m->life = -1;
            }
        }
        if (stop) {
            if (m->life > -0.5f && !level_solid_at(G.lv, m->x, m->z)) place_object(m->item, 40, m->x, m->z); /* it lies there */
            g_ms[k--] = g_ms[--g_nms];
            continue;
        }
        m->x = nx, m->y = ny, m->z = nz;
    }
}
static int missile_cards(Vertex *out, int cap) {
    int n = 0;
    for (int k = 0; k < g_nms && n + 6 <= cap; k++) {
        float pos[3] = {g_ms[k].x, g_ms[k].y - 0.05f, g_ms[k].z};
        n += world_item_card(G.mesh, g_ms[k].item, pos, out + n);
    }
    return n;
}

/* one step of the swing each 1/16 s: held at f 3 while charging; the blow at f 6; over at 9 */
static void weapon_step(float dt, const float eye[3], const float fwd[3]) {
    if (G.charge >= 0 && G.open) W.charge = G.charge, G.charge = -1; /* the panel opened mid-charge: let go */
    /* raised over three steps (1/16 s each) as fight mode comes on, lowered as it goes off */
    W.up = G.fight ? MIN(1.0f, W.up + dt * 16.0f / 3) : MAX(0.0f, W.up - dt * 16.0f / 3);
    if (W.type < 0) return;
    W.t -= dt;
    while (W.t <= 0 && W.type >= 0) {
        if (W.f >= 3 && W.charge < 0) { /* still held: stays drawn back */
            W.t = 0;
            break;
        }
        W.t += 1.0f / 16;
        W.f++;
        if (W.f == 6) player_attack(eye, fwd, W.charge, W.type);
        if (W.f >= 9) W.type = W.f = -1;
    }
}

/* the weapon at the bottom right of the view, scaled as the original's 176 x 112 view is to the screen */
static void weapon_draw(const Overlay *o) {
    int kind = weapon_kind();
    if (kind < 0 || G.talking || G.reading || W.up <= 0) return; /* only in fight mode */
    weapon_offsets();
    int f = W.up < 1 ? 18 + MIN(2, (int)(W.up * 3)) : W.type >= 0 && W.f >= 0 ? W.type * 9 + W.f : 27, n = kind * 28 + f;
    const Pic *p = ui_pic("ui\\weapons", n);
    if (!p || p->w <= 1) return;
    int s = MAX(1, (o->h + 56) / 112);
    ui_draw(p, o->w - (176 - W.ox[n]) * s, o->h - (112 - W.oy[n]) * s, s);
}
/* rand() % n, the original's most common roll (UW.EXE's Borland rand, orig.c) */
static int rnd(int n) { return uw_rand_mod(n); }

/* object i (a creature, 0x40-0x7F) on tile t joins the living */
static void creature_add(int i, int t) {
    const Object *o = &G.lv->obj[i];
    if (o->item < 0x40 || o->item >= 0x80 || g_ncr >= 256) return;
    Creature *c = &g_cr[g_ncr++];
    memset(c, 0, sizeof *c);
    float p[3];
    world_object_pos(G.lv, t % 64, t / 64, o, p);
    c->obj = i, c->kind = o->item - 0x40, c->x = p[0], c->z = p[2], c->y = p[1], c->tile = t;
    /* its hit points: its own (mobile byte 0), else its kind's (the critter table) */
    c->hp = (i < 256 && (o->mobile[0] & 0x7F)) ? (float)(o->mobile[0] & 0x7F) : (float)MAX(1, obj_info(o->item)->hp);
    c->dir = o->heading & 7;
    /* its attitude (the object's word at 0x0d, bits 14-15, UW.EXE 0x7120c): 0 hostile, 1 upset, 2 mellow, 3 friendly;
     * only the hostile come for the player */
    c->hostile = i < 256 ? (o->mobile[6] >> 6) == 0 : true;
    c->t = (float)rnd(100) / 37.0f;
}

/* a creature dies: its object leaves the level's tile, what it carried (its link chain) lies there instead, so it
 * stays dead when the level is written and read again */
static void rebuild_sprites(void);
static void creature_remove(Creature *c) {
    if (c->obj <= 0 || c->obj >= 1024 || c->tile < 0 || c->tile >= 4096) return;
    Object *o = &G.lv->obj[c->obj];
    u16 *at = &G.lv->tile[c->tile].first_object;
    for (int guard = 0; *at && guard < 1024; guard++) {
        if (*at == c->obj) {
            *at = o->next;
            break;
        }
        at = &G.lv->obj[*at].next;
    }
    /* its things onto the tile (the end of the chain they make) */
    int first = o->link;
    if (first > 0 && first < 1024 && !(o->flags & 0x40)) {
        int last = first;
        for (int guard = 0; G.lv->obj[last].next && guard < 1024; guard++) last = G.lv->obj[last].next;
        G.lv->obj[last].next = G.lv->tile[c->tile].first_object;
        G.lv->tile[c->tile].first_object = (u16)first;
        rebuild_sprites();
    }
    memset(o, 0, sizeof *o);
    c->obj = 0;
}

/* hostile creatures within r tiles of the player (UW.EXE 0x711e1: the tiles round the player, r = 2 to sleep at all,
 * 8 for what may wake the sleeper) */
static int hostiles_within(int r) {
    int px = (int)floorf(G.eye[0]), pz = (int)floorf(G.eye[2]), n = 0;
    for (int k = 0; k < g_ncr; k++) {
        const Creature *c = &g_cr[k];
        if (c->state >= CS_DYING || !c->hostile) continue;
        if (abs((int)floorf(c->x) - px) <= r && abs((int)floorf(c->z) - pz) <= r) n++;
    }
    return n;
}
/* vitality and mana back by sixteenths of their most (UW.EXE 0x346c2, 0x34656): v > 0: (v + 0-3) x most / 16
 * (+1); v <= 0: -v points */
static void restore_hp(int v) {
    float add = v > 0 ? (float)(1 + (v + rnd(4)) * (int)G.hp_max / 16) : (float)-v;
    G.hp = MIN(G.hp_max, G.hp + add);
}
static void restore_mana(int v) {
    float add = v > 0 ? (float)((v + rnd(4)) * (int)G.mana_max / 16 + 1) : (float)-v;
    G.mana = MIN(G.mana_max, G.mana + add);
}

void game_sleep(int how) {
    int hunger, fatigue, drunk, poison;
    inv_body(&hunger, &fatigue, &drunk, &poison);
    if (how >= 0) {
        if (G.lv && G.lv->number == 9) return game_message("%s", str_get(1, 20)); /* "You can't go to sleep here!" */
        if (hostiles_within(2)) return game_message("%s", str_get(1, 14));  /* "There are hostile creatures near!" */
        game_message("%s", str_get(1, 15));                                  /* "You make camp." */
        sound_song_set_next(13), sound_songs_update(G.clock, G.fight);      /* the quiet song (UW.EXE 0x81fea) */
        game_message("%s", str_get(1, 16));                                  /* "You go to sleep." */
    }
    G.dark = 2.5f, G.open = G.sheet = false, G.charge = -1;
    int hours = rnd(5) + 2;
    G.clock += (u32)hours * 0xE1000u; /* an hour: 3600 x 256 */
    if (poison > 0) G.hp -= (float)((poison + 1) * poison / 2), poison = 0; /* the poison runs its course */
    if (G.hp <= 0) return inv_set_body(hunger, fatigue, drunk, poison);
    sound_song_random(); /* awake: a wandering song (0x820d8, 0x82326) */
    if (hostiles_within(8) && rnd(2)) { /* woken (0x71670: a hostile creature within 8 tiles, half the time) */
        fatigue = MAX(0, fatigue - 32);
        game_message("%s", str_get(1, 21)); /* "Your sleep is interrupted!" */
        hunger -= 12 + rnd(16), drunk = MAX(0, drunk - 16);
        return inv_set_body(hunger, fatigue, drunk, poison);
    }
    int more = rnd(4) + 7 - hours;
    if (G.hp < 10) more += rnd(2) + 1;
    G.clock += (u32)MAX(0, more) * 0xE1000u;
    int good = hunger > 64 && how > 0, f = MIN(5, fatigue / 2 + 2);
    fatigue = 0;
    if (hunger) {
        restore_hp(f * good + f - 1);
        restore_mana(-6);
        restore_mana((f + 1) * good + f - 1);
    } else {
        game_message("%s", str_get(1, 17)); /* "You are starving." */
        G.hp -= 2;
    }
    hunger -= 24 + rnd(32), drunk = MAX(0, drunk - 32);
    inv_set_body(hunger, fatigue, drunk, poison);
    game_message("%s", str_get(1, 19 - good)); /* "You feel rested." / "Your sleep is uneasy." */
}

/* a block 1 string without its line breaks (pieces of one sentence); four kept at a time */
static const char *s1(int i) {
    static char buf[4][160];
    static int at;
    char *o = buf[at++ & 3];
    const char *t = str_get(1, i);
    size_t n = 0;
    for (; *t && n < 159; t++)
        if (*t != '\n' && *t != '\r') o[n++] = *t;
    o[n] = 0;
    return o;
}
/* a new object of kind item lying at (x, z) on the floor of this level; -1: no room */
static void rebuild_sprites(void);
static int place_object(int item, int quality, float x, float z) {
    if (!G.lv || level_solid_at(G.lv, x, z)) return -1;
    int i = inv_level_alloc(G.lv);
    if (i <= 0) return -1;
    int tx = (int)x, tz = (int)z;
    Object *o = &G.lv->obj[i];
    memset(o, 0, sizeof *o);
    o->item = (u16)item, o->quality = (u8)quality;
    o->x = (u8)MIN(7, (int)((x - tx) * 8)), o->y = (u8)MIN(7, (int)((z - tz) * 8));
    o->z = (u8)MIN(127, (int)(level_floor_y(G.lv, x, z) * 32 + 0.5f));
    Tile *t = &G.lv->tile[tz * 64 + tx];
    o->next = t->first_object, t->first_object = (u16)i;
    rebuild_sprites();
    return i;
}

/* Death (UW.EXE 0x82880): an eighth of the experience is lost, what was on the pointer falls, a pile of bones
 * (0xC2-0xC6) is left where the player fell; with the silver tree planted (not in the Ethereal Void) the player
 * wakes beside it, healed; else the game is over (0x368:0x6b). */
static void character_lose_exp(void);
static int player_talismans(void);
static void player_dies(void) {
    if (player_talismans() == 0) { /* the last talisman destroyed: the player cannot die and keeps 4 (UW.EXE player_is_dead) */
        G.hp = 4;
        return;
    }
    sound_effect_stop_all(), sound_song_play(10); /* the effects stopped, the death song (UW.EXE 0x8289c) */
    character_lose_exp(); /* an eighth of the experience */
    inv_put_back();
    place_object(0xc2 + rnd(5), 63, G.eye[0], G.eye[2]);
    G.open = G.sheet = G.talking = G.reading = false, G.charge = -1;
    if (G.silver_level && G.lv && G.lv->number != 9) {
        G.hp = G.hp_max, G.mana = G.mana_max, G.dark = 3.0f;
        G.travel = true, G.travel_level = G.silver_level, G.travel_x = G.silver_x, G.travel_z = G.silver_z;
        game_message("Thou awakest beside the silver tree.");
        sound_song_set_next(4); /* (UW.EXE 0x82858) */
        return;
    }
    G.dead = true, G.death_choice = 0;
}
void game_hurt(float damage) { G.hp -= damage; }
int game_death_choice(void) {
    int c = G.death_choice;
    G.death_choice = 0;
    return c;
}

/* the silver seed (UW.EXE 0x37d45, strings 10-12): it takes root on open floor, where the player will come back to
 * life; one tree at a time (a new one: the old gone) */
bool game_plant_seed(void) {
    if (!G.lv || G.lv->number == 9 || level_solid_at(G.lv, G.eye[0], G.eye[2])) return game_message("%s", s1(10)), false;
    int i = place_object(0x1ca, 63, G.eye[0], G.eye[2]);
    if (i < 0) return game_message("%s", s1(10)), false;
    G.silver_level = G.lv->number, G.silver_x = G.eye[0], G.silver_z = G.eye[2];
    game_message("%s", s1(12)); /* "You plant the seed, and a small silver tree quickly grows." */
    return true;
}

/* Repair (UW.EXE 0x747ac, 0x748c4): durability (weapons' byte 7, armour's byte 1; else none: "You cannot repair
 * that."); how hard it looks: durability - Repair + 15 (below 0 trivial, above 30 very difficult, else / 10 + 1);
 * a Repair check against the durability: critical: whole; success: + Repair / 5 + 3; failure: no change; fumble:
 * destroyed when 0-63 beats condition + Repair, else - (4-11). The first click asks, a second on it tries. */
static Object *g_repair_ask;
static int repair_item(Object *o) {
    int item = o->item & 0x1ff, dur = -1;
    if (item < 0x10) dur = obj_info(item)->wdurability;
    else if (item >= 0x20 && item < 0x40) dur = obj_info(item)->durability;
    char noun[64];
    obj_noun(o, noun, sizeof noun);
    if (dur < 0 || dur == 255) return game_message("%s", s1(142)), 1;
    int r = game_skill(14), q = o->quality & 63; /* Repair */
    if (g_repair_ask != o) {
        int v = dur - r + 15, w = v < 0 ? 0 : v > 30 ? 4 : v / 10 + 1;
        game_message("%s%s%s%s. %s(click it again)", s1(216), s1(219 + w), s1(217), noun, s1(218));
        g_repair_ask = o;
        return 1;
    }
    g_repair_ask = NULL;
    int c = skill_check(r, dur), add = 0;
    if (c == 2) add = 64;
    else if (c == 1) add = r / 5 + 3;
    else if (c == 0) return game_message("%s%s.", s1(143), noun), 1;
    else {
        if (rnd(64) > q + r) return game_message("%s%s.", s1(140), noun), 3;
        add = -(4 + rnd(8));
    }
    if (q + add > 63) return o->quality = (u8)((o->quality & ~63) | 63), game_message("%s%s.", s1(145), noun), 1;
    if (q + add <= 0) return game_message("%s%s.", s1(140), noun), 3;
    o->quality = (u8)((o->quality & ~63) | (q + add));
    game_message("%s%s.", add > 0 ? s1(144) : s1(141), noun);
    return 1;
}

int game_use_on_item(Object *held, Object *target) {
    int item = held->item & 0x1ff, ti = target->item & 0x1ff;
    if (item == 0xd7) return repair_item(target); /* the anvil */
    if (item == 0x12d) { /* oil (0x37b60): the lantern refuelled, the torch refreshed, wood made a torch */
        if (ti == 0x94) return game_message("%s", s1(178)), 1;
        if (ti == 0x95) return game_message("%s", s1(182)), 1;
        if (ti == 0x90) {
            if ((target->quality & 63) >= 63) return game_message("%s", s1(180)), 1;
            target->quality = 63;
            return game_message("%s", s1(179)), 2;
        }
        if (ti == 0x91) {
            if ((target->quality & 63) >= 63) return game_message("%s", s1(184)), 1;
            target->quality = 63;
            return game_message("%s", s1(183)), 2;
        }
        if (ti == 0xcc || ti == 0xcd) {
            target->item = (u16)((target->item & ~0x1ff) | 0x91), target->quality = 63;
            return game_message("%s", s1(181)), 2;
        }
        return game_message("%s", s1(177)), 1;
    }
    return 0;
}

/* Fishing (UW.EXE 0x73f34): water ahead (the tile in front, lower than the player), one try in five, a fish (0xB6) */
void game_fish(void) {
    float fx = sinf(G.yaw), fz = cosf(G.yaw);
    int tx = (int)floorf(G.eye[0] + fx * 0.9f), tz = (int)floorf(G.eye[2] + fz * 0.9f);
    if (terrain_at(tx, tz) != 1) return game_message("%s", s1(101));
    if (rnd(5)) return game_message("%s", s1(rnd(2) ? 100 : 102));
    if (inv_give(0xb6, 63)) game_message("%s", s1(99));
}
/* Leeches (UW.EXE 0x37cfe): the poison gone, and some vitality with it */
bool game_leeches(void) {
    int hunger, fatigue, drunk, poison;
    inv_body(&hunger, &fatigue, &drunk, &poison);
    inv_set_body(hunger, fatigue, drunk, 0);
    G.hp = MAX(1.0f, G.hp - 2);
    game_message("%s", s1(224));
    return true;
}
/* An instrument (UW.EXE 0x37ce9): the player plays (0-9) until Esc */
void game_play(void) {
    game_message("%s", s1(250));
    G.playing = true;
}

/* Tracking (UW.EXE 0x356ee): the creatures within r tiles each give a check (skill against 15 - how hidden the kind
 * is); the successes counted by direction (eight); the busiest direction told: none "You detect no monster
 * activity.", one "a creature", 2-3 "a few creatures", 4+ "the activity of many creatures", then which way */
static void track(int r, int skill) {
    int bin[8] = {0}, best = 0, at = 0;
    for (int k = 0; k < g_ncr; k++) {
        const Creature *c = &g_cr[k];
        if (c->state >= CS_DYING) continue;
        float dx = c->x - G.eye[0], dz = c->z - G.eye[2];
        if (fabsf(dx) >= (float)r || fabsf(dz) >= (float)r) continue;
        if (skill_check(skill, 15) <= 0) continue;
        int b = ((int)floorf(atan2f(dx, dz) / 0.7853982f + 0.5f) % 8 + 8) % 8; /* 0 north, 2 east (strings 36-43) */
        bin[b]++;
    }
    for (int b = 0; b < 8; b++)
        if (bin[b] > best) best = bin[b], at = b;
    if (!best) return game_message("%s", s1(62));
    game_message("%s%s.", s1(best >= 4 ? 61 : best >= 2 ? 60 : 59), s1(36 + at));
}

/* --- magic ----------------------------------------------------------------------------------------------------
 * The spells (data\\magic\\spells.txt, UW.EXE's table) cast from the shelf (UW.EXE 0x78ec8): the runes looked up; the
 * circle n / 6 + 1 needs (level + 1) / 2 >= circle (else 210) and 3 x circle mana (else 211); a Casting check (Casting +
 * 5 against 2 x circle): failure "The incantation failed." (212), a fumble "The spell backfires." (214, a hurt of
 * circle / 2 d8); else the mana is spent and the effect follows (0x34418, by category) or "Casting was not
 * successful." (213). */
typedef struct {
    int rune[3], cat, aim, str;
    char name[32];
} Spell;
static Spell g_spell[48];
static bool g_spells_loaded;
static const char *const RUNE_NAME[24] = {"An", "Bet", "Corp", "Des", "Ex", "Flam", "Grav", "Hur", "In", "Jux", "Kal", "Lor",
                                          "Mani", "Nox", "Ort", "Por", "Quas", "Rel", "Sanct", "Tym", "Uus", "Vas", "Wis", "Ylem"};
static void spells_load(void) {
    if (g_spells_loaded) return;
    g_spells_loaded = true;
    char path[700];
    snprintf(path, sizeof path, "%s\\magic\\spells.txt", G.data);
    size_t mark = arena_mark();
    Bytes f = file_read(path);
    for (const char *p = (const char *)f.data; p && p < (const char *)f.data + f.size;) {
        int k, circle, cat, aim, str, used = 0;
        char r[3][8];
        if (sscanf(p, "spell %d %d %7s %7s %7s %d %d %d %n", &k, &circle, r[0], r[1], r[2], &cat, &aim, &str, &used) == 8 && k >= 0 && k < 48) {
            Spell *s = &g_spell[k];
            for (int i = 0; i < 3; i++) {
                s->rune[i] = 24;
                for (int j = 0; j < 24; j++)
                    if (!strcmp(r[i], RUNE_NAME[j])) s->rune[i] = j;
            }
            s->cat = cat, s->aim = aim, s->str = str;
            int n = 0;
            for (const char *q = p + used; *q && *q != '\r' && *q != '\n' && n < 31; q++) s->name[n++] = *q;
            s->name[n] = 0;
        }
        const char *nl = memchr(p, '\n', (size_t)((const char *)f.data + f.size - p));
        p = nl ? nl + 1 : NULL;
    }
    arena_reset(mark);
}

/* a lasting spell (UW.EXE 0x2b109): at most three; its time by the strength's top bits: 0 2d3, 0x40 2d8 + 6, 0x80
 * 3d20 + 24 player ticks (21 s each) */
static bool spell_last(int cat, int str) {
    if (G.nactive >= 3) return false;
    int d = (str & 0xc0) == 0x40 ? dice(2, 8) + 6 : (str & 0xc0) == 0x80 ? dice(3, 20) + 24 : dice(2, 3);
    G.act_cat[G.nactive] = cat, G.act_str[G.nactive] = str & 0x3f, G.act_ticks[G.nactive] = d, G.nactive++;
    return true;
}
/* the effects in force: the lasting spells and the worn things' enchantments (UW.EXE 0x7e6e5 applies those through
 * the same routine, 0x7e007) */
static int effects(int *cat, int *val) {
    int n = 0;
    for (int k = 0; k < G.nactive; k++) cat[n] = G.act_cat[k], val[n] = G.act_str[k], n++;
    return n + inv_worn_enchants(cat + n, val + n, 16);
}
static int spell_has(int cat, int v) {
    int c[24], s[24], n = effects(c, s);
    for (int k = 0; k < n; k++)
        if (c[k] == cat && (v < 0 || s[k] == v)) return 1;
    return 0;
}
int game_spell_light(void) {
    int c[24], s[24], n = effects(c, s), l = 0;
    for (int k = 0; k < n; k++)
        if (c[k] == 0) l = MAX(l, s[k] & 7);
    return l;
}
/* the movement spells as flags (UW.EXE 0x7e0a8: 1 << (v - 1)), speed (the special 0) as 32 */
int game_motion(void) {
    int c[24], s[24], n = effects(c, s), m = 0;
    for (int k = 0; k < n; k++) {
        if (c[k] == 1 && s[k] >= 1 && s[k] <= 5) m |= 1 << (s[k] - 1);
        if (c[k] == 11 && s[k] == 2) m |= 32; /* speed (DS 0x283) */
    }
    return m;
}
/* the protection level (Resist Blows 2, Thick Skin 3, Iron Flesh 5: the highest, UW.EXE 0x7e028), added to the
 * armour of every part (0x7e39f) */
static int spell_armour(void) {
    int c[24], s[24], n = effects(c, s), a = 0;
    for (int k = 0; k < n; k++)
        if (c[k] == 2) a = MAX(a, s[k]);
    return a;
}
/* what is taken off an attacker's score at a part (DS 0x266a): the Protection spell + 3 (0x7e04e), armour
 * Protection enchantments (0x7e15f) */
static int player_protect(int part) {
    int c[24], s[24], n = effects(c, s), a = 0, prot[4], tough[4];
    for (int k = 0; k < n; k++)
        if (c[k] == 3 && s[k] == 1) a += 3;
    inv_enchant_armour(prot, tough);
    return a + prot[part & 3];
}
/* the player's resistances (UW.EXE DS 0x60eb, rebuilt from the effects: 0x1b00 table: missile protection 0x40,
 * flameproof 8, poison resistance 0x10, magic protection 1, greater magic protection 2) */
static int player_resist(void) {
    static const u8 RES[10] = {0, 0, 0, 0xff, 1, 0x40, 8, 0x10, 1, 2};
    int c[24], s[24], n = effects(c, s), r = 0;
    for (int k = 0; k < n; k++)
        if (c[k] == 3 && s[k] >= 5 && s[k] <= 9) r |= RES[s[k]];
    return r;
}
/* harm of a kind against resistances (UW.EXE 0x25eaf): a magic kind (bits 0-1) against magic resistance: resisted
 * when d3 - 1 < the resistance, else that part of the kind dropped; any other bit in both: nothing. The harm left. */
static int resisted(int res, int dmg, int kind) { return uw_resist(res, dmg, kind); } /* 0x25eaf */
/* how noticeable the player is (UW.EXE 0x7dea4, 0x7e31d): noise 13 - Sneak / 3 (stealth - 16), visibility 15 -
 * Sneak / 5 (conceal - 5, invisibility - 16), not below 0 */
static int player_visibility(void) {
    int c[24], s[24], n = effects(c, s), v = 15 - game_skill(13) / 5;
    for (int k = 0; k < n; k++)
        if (c[k] == 3 && s[k] == 3) v -= MIN(v, 5);
    for (int k = 0; k < n; k++)
        if (c[k] == 3 && s[k] == 4) v -= MIN(v, 16);
    return MAX(0, v);
}
static int player_noise(void) {
    int c[24], s[24], n = effects(c, s), v = 13 - game_skill(13) / 3;
    for (int k = 0; k < n; k++)
        if (c[k] == 3 && s[k] == 2) v -= MIN(v, 16);
    return MAX(0, v);
}
static void spells_tick(void) { /* each player tick */
    for (int k = 0; k < G.nactive; k++)
        if (--G.act_ticks[k] <= 0) {
            for (int j = k; j < G.nactive - 1; j++) G.act_cat[j] = G.act_cat[j + 1], G.act_str[j] = G.act_str[j + 1], G.act_ticks[j] = G.act_ticks[j + 1];
            G.nactive--, k--;
        }
}

/* --- the spells' effects (UW.EXE 0x34418 and the routines it calls) ----------------------------------------------
 * The area spells and the creature spells (0x35017): a 5 x 5 tile square centred 4 tiles ahead of the caster (the
 * step 0x38e1d: 4 x the facing, rounded away from 0); the creatures in it taken column by column. Area spells may
 * touch 3d4 (0x3515a), creature spells 1 (0x351a0); a spell with strength bits 0x40 (Sheet Lightning, Flame Wind)
 * picks open tiles at random instead, each with chance count / 28, up to four passes. */
static int player_level(void);
static void spell_square(int *x0, int *z0) {
    float f[2] = {sinf(G.yaw), cosf(G.yaw)};
    int o[2];
    for (int i = 0; i < 2; i++) {
        int v = (int)(f[i] * 255.0f) * 4 / 256;
        o[i] = v > 0 ? v + 1 : v < 0 ? v - 1 : 0;
    }
    *x0 = (int)G.eye[0] + o[0] - 2, *z0 = (int)G.eye[2] + o[1] - 2;
}
static int square_creatures(int *out, int cap) {
    int x0, z0, n = 0;
    spell_square(&x0, &z0);
    for (int x = x0; x < x0 + 5; x++)
        for (int z = z0; z < z0 + 5; z++)
            for (int k = 0; k < g_ncr && n < cap; k++) {
                const Creature *c = &g_cr[k];
                if (c->state < CS_DYING && (int)c->x == x && (int)c->z == z) out[n++] = k;
            }
    return n;
}
static int random_tiles(int count, int (*out)[2], int cap) {
    int x0, z0, n = 0;
    spell_square(&x0, &z0);
    for (int pass = 0; pass < 4 && count > 0; pass++)
        for (int x = x0; x < x0 + 5 && count > 0; x++)
            for (int z = z0; z < z0 + 5 && count > 0; z++) {
                if (x < 0 || x > 63 || z < 0 || z > 63 || level_solid_at(G.lv, x + 0.5f, z + 0.5f)) continue;
                if (rnd(28) < count && n < cap) out[n][0] = x, out[n][1] = z, n++, count--;
            }
    return n;
}

static void creature_hurt(int k, int dmg) {
    Creature *c = &g_cr[k];
    const ObjInfo *ci = obj_info(0x40 + c->kind);
    if (dmg <= 0) return;
    c->hostile = true, c->ally = false, c->fear = 0, c->confused = 0;
    c->hp -= (float)dmg;
    if (c->hp > 0) sound_song_blow(G.clock, true, (int)c->hp, ci->hp); /* the player's spell: (UW.EXE 0x1c44d) */
    else sound_song_set_next(9);                                        /* a kill (0x25b56) */
    if (c->hp <= 0) {
        c->state = CS_DYING, c->t = 0, creature_death_sound(c);
        creature_remove(c);
        kill_exp(c);
    } else c->state = CS_HIT, c->t = 0;
}
/* harm of a kind to a creature, through its resistances (0x25f0a) */
static void creature_harm(int k, int dmg, int kind) { creature_hurt(k, resisted(obj_info(0x40 + g_cr[k].kind)->resist, dmg, kind)); }
/* harm to every creature on a tile (0x35bca; the kinds by DS 0x9d3 / 0x9d5 / 0x9d7: Flame Wind 10d6 fire, Sheet
 * Lightning 6d5) */
static void tile_harm(int x, int z, int n, int s, int kind) {
    for (int k = 0; k < g_ncr; k++)
        if (g_cr[k].state < CS_DYING && (int)g_cr[k].x == x && (int)g_cr[k].z == z) creature_harm(k, dice(n, s), kind);
}
/* a creature spell's goal (0x34c29): magic resistance first (a magic kind, 3); then the goal and attitude */
static bool spell_takes(int k) { return resisted(obj_info(0x40 + g_cr[k].kind)->resist, 1, 3) > 0; }
static void set_attitude(Creature *c, int a) {
    c->hostile = a == 0;
    if (c->obj > 0 && c->obj < 256) G.lv->obj[c->obj].mobile[6] = (u8)((G.lv->obj[c->obj].mobile[6] & 0x3f) | a << 6);
}

/* where a made thing goes (0x351e5): 9 / 8 of a tile ahead (a creature 12 / 8), turned a little at random */
static bool spot_free(float x, float z) { return !level_solid_at(G.lv, x, z) && !game_blocked(x, z, 0.3f); }
static void make_spot(float dist, float *x, float *z) {
    float a = G.yaw + (float)(rnd(27) - 13) * (6.2831853f / 256.0f);
    *x = G.eye[0] + sinf(a) * dist, *z = G.eye[2] + cosf(a) * dist;
}

/* the effect (UW.EXE 0x34418, by category; strength: bits 0-5 the value, 6-7 how long or how it spreads). False:
 * "Casting was not successful." */
static void use_switch(int i);
static bool spell_effect(int cat, int aim, int str, int circle) {
    int v = str & 0x3f, list[64], n;
    (void)aim, (void)circle;
    switch (cat) {
    case 0: case 1: case 2: case 3: return spell_last(cat, str); /* light, movement, protection, the rest: lasting */
    case 4: /* healing (0x347cc): strength d8, 15 all */
        G.hp = v == 15 ? G.hp_max : MIN(G.hp_max, G.hp + (float)dice(v, 8));
        return true;
    case 5: { /* a missile (DS 0x9ab: 1 magic arrow 0x17, 2 electric bolt 0x15, 3 fireball 0x14, 4 acid 0x16) */
        static const int KIND[5] = {0x17, 0x17, 0x15, 0x14, 0x16};
        int kind = KIND[MIN(4, v)];
        float fwd[3] = {sinf(G.yaw), 0, cosf(G.yaw)};
        missile_fire(kind, LAUNCH[kind - 0x10][0], G.eye, fwd, 0x180, true);
        return true;
    }
    case 6: { /* area (0x3515a, DS 0x9af): 1 Reveal, 2 Sheet Lightning, 3 Confusion, 4 Flame Wind; up to 3d4 */
        int count = dice(3, 4), tiles[32][2];
        if (v == 2 || v == 4) {
            n = random_tiles(count, tiles, 32);
            for (int i = 0; i < n; i++) {
                if (v == 2) tile_harm(tiles[i][0], tiles[i][1], 6, 5, 3); /* 0x34a0e: a spark, 6d5 magic */
                else {                                                    /* 0x34a9b: a flame, 10d6 fire on the tile and around */
                    static const int D[5][2] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                    for (int d = 0; d < 5; d++) tile_harm(tiles[i][0] + D[d][0], tiles[i][1] + D[d][1], 10, 6, 0x0b);
                }
            }
        } else if (v == 3) { /* 0x34d13: goal 2 (wander), attitude 1 */
            n = square_creatures(list, MIN(64, count));
            for (int i = 0; i < n; i++)
                if (spell_takes(list[i])) g_cr[list[i]].confused = 30.0f, set_attitude(&g_cr[list[i]], 1);
        } else if (v == 1) { /* 0x34963: the traps about found (a search at skill 45) */
            int x0, z0, found = 0;
            spell_square(&x0, &z0);
            for (int x = MAX(0, x0); x < MIN(64, x0 + 5); x++)
                for (int z = MAX(0, z0); z < MIN(64, z0 + 5); z++)
                    for (int i = G.lv->tile[z * 64 + x].first_object, g = 0; i > 0 && i < 1024 && g < 256; i = G.lv->obj[i].next, g++)
                        if ((G.lv->obj[i].item & 511) >= 0x180 && (G.lv->obj[i].item & 511) <= 0x18f) found++;
            if (found) game_message("You found a trap!");
        }
        return true;
    }
    case 7: /* one creature in the square (0x351a0, DS 0x9bb): 1 Cause Fear, 2 Smite Undead, 3 Ally, 4 Poison, 5 Paralyze */
        n = square_creatures(list, 64);
        for (int i = 0; i < n; i++) {
            Creature *c = &g_cr[list[i]];
            const ObjInfo *ci = obj_info(0x40 + c->kind);
            if (v == 2) { /* 0x34b8a: the undead only (0x80) take 255 */
                if (!(ci->resist & 0x80)) continue;
                creature_harm(list[i], 255, 3);
            } else if (v == 4) creature_harm(list[i], dice(5, 4), 0x13); /* 0x34bd4: 5d4 poison */
            else if (spell_takes(list[i])) {
                if (v == 1) c->fear = 30.0f;                              /* goal 6: flees */
                else if (v == 3) c->ally = true, set_attitude(c, 3);       /* friendly, an ally */
                else if (v == 5) c->frozen = 20.0f, set_attitude(c, 1);    /* goal 7: stands */
            }
            break; /* one creature */
        }
        return true;
    case 8: { /* making (0x351e5): 1 food, 3 a rune of warding, 4 a creature; else 277 "There is no room..." */
        float x, z;
        if (v == 1) {
            make_spot(9.0f / 8, &x, &z);
            if (!spot_free(x, z) || place_object(0xb0 + rnd(7), 63, x, z) < 0) return game_message("%s", s1(277)), true;
            return true;
        }
        if (v == 3) {
            make_spot(9.0f / 8, &x, &z);
            if (G.nwards >= 8 || !spot_free(x, z)) return game_message("%s", s1(277)), true;
            G.wards[G.nwards][0] = (int)x, G.wards[G.nwards][1] = (int)z, G.nwards++;
            game_message("%s", s1(276)); /* "The Rune of Warding is placed." */
            return true;
        }
        if (v == 4) { /* a kind between L and 2L - 1 (L: the caster's level, at least 2), an ally */
            /* (here: among the kinds whose pictures this level has loaded; the nearest to the range when none in it) */
            int L = MAX(2, player_level()), kind = -1, best = 999;
            for (int t = 0; t < 32 && kind < 0; t++) {
                int k = L + rnd(L);
                const ObjInfo *ci = obj_info(0x40 + k);
                if (k < 63 && ci->critter && ci->hp > 0 && k != 0x3b && k != 0x3c && G.mesh->crit[k].loaded) kind = k;
            }
            int nearest = -1;
            for (int k = 0; k < 63 && kind < 0; k++)
                if (G.mesh->crit[k].loaded && k != 0x3b && k != 0x3c && obj_info(0x40 + k)->hp > 0) {
                    int d = k < L ? L - k : k > 2 * L - 1 ? k - (2 * L - 1) : 0;
                    if (d < best) best = d, nearest = k;
                }
            if (kind < 0) kind = nearest;
            make_spot(12.0f / 8, &x, &z);
            int i = kind >= 0 ? inv_level_alloc_mobile(G.lv) : -1;
            if (i <= 0 || i >= 256 || !spot_free(x, z)) return game_message("%s", s1(277)), true;
            Object *o = &G.lv->obj[i];
            memset(o, 0, sizeof *o);
            o->item = (u16)(0x40 + kind), o->x = (u8)((int)(x * 8) & 7), o->y = (u8)((int)(z * 8) & 7);
            o->mobile[6] = 3 << 6;
            int t = (int)z * 64 + (int)x;
            o->next = G.lv->tile[t].first_object, G.lv->tile[t].first_object = (u16)i;
            creature_add(i, t);
            if (g_ncr > 0) {
                Creature *c = &g_cr[g_ncr - 1];
                c->ally = true, c->hostile = false, c->x = x, c->z = z, c->y = level_floor_y(G.lv, x, z);
                logf_("summoned: kind %d at %.1f,%.1f\n", c->kind, x, z);
            }
            return true;
        }
        return true;
    }
    case 9: /* harm (0x34813): strength d8, never below 3; magic protection resists */
        if (G.hp > 3) G.hp = MAX(3.0f, G.hp - (float)resisted(player_resist(), dice(v, 8), 3));
        return true;
    case 10: restore_mana(v); return true; /* (0x34656) */
    case 11: /* the specials (0x359e5, its table) */
        switch (v) {
        case 0: return spell_last(11, (str & 0xc0) | 2); /* speed: the world at half pace */
        case 1: track(10, 45); return true;              /* detect monster: tracking at skill 45, 10 tiles */
        case 2: game_message("%s", s1(271)); return true; /* strengthen door: UW.EXE does nothing with it */
        case 3:                                          /* remove trap (a disarm at skill 45) */
            if (G.hover_obj < 0 || (G.lv->obj[G.hover_obj].item & 511) < 0x180 || (G.lv->obj[G.hover_obj].item & 511) > 0x18f) return game_message("%s", s1(271)), true;
            G.lv->obj[G.hover_obj].item = 0;
            return true;
        case 4: {                                        /* name enchantment: the held thing, else the one pointed at */
            int hp = inv_held_index();
            const char *nm = hp > 0 ? inv_enchant_name(hp) : "";
            if (hp <= 0) return game_message("%s", s1(271)), true;
            if (nm[0]) game_message("It is enchanted with %s.", nm);
            else game_message("It has no enchantment.");
            return true;
        }
        case 5: { /* open (the lockpick at skill 45): 270 or 271 */
            int r = G.hover_door >= 0 ? lock_try(&G.doors[G.hover_door], -45) : 0;
            game_message("%s", s1(r == 3 ? 270 : 271));
            return true;
        }
        case 6: { int h, f, d, p; inv_body(&h, &f, &d, &p); inv_set_body(h, f, d, 0); return true; } /* cure poison */
        case 7: return spell_last(11, (str & 0xc0) | 1); /* roaming sight */
        case 8: return spell_last(11, (str & 0xc0) | 3); /* telekinesis: no reach limit (DS 0x286 = 0) */
        case 9: { /* tremor (0x355b1): 3d8 rocks fall about a 7 x 7 square 5 tiles ahead */
            int tiles[32][2], k = random_tiles(dice(3, 8), tiles, 32);
            sfx_at_(0x12, G.eye[0], G.eye[2], 0); /* the rumble, on the caster (UW.EXE SPELLS.C: effect 0x12) */
            for (int i = 0; i < k; i++) {
                tile_harm(tiles[i][0], tiles[i][1], 2, 8, 0); /* a falling rock's harm: our estimate */
                place_object(0x154 + rnd(3), 40, tiles[i][0] + 0.5f, tiles[i][1] + 0.5f);
            }
            return true;
        }
        case 10: game_message("%s", s1(273)); return true; /* gate travel: the moonstone (the quest's) */
        case 11: return spell_last(11, (str & 0xc0) | 0);  /* freeze time */
        case 12:                                           /* armageddon: all but the player gone */
            game_message("%s", s1(278));
            for (int k = 0; k < g_ncr; k++)
                if (g_cr[k].state < CS_DYING) g_cr[k].state = CS_DYING, g_cr[k].t = 0, creature_remove(&g_cr[k]);
            return true;
        }
        return true;
    case 13: /* the potions' oddities: 5 hallucination (228) */
        if (v == 5) game_message("%s", s1(228));
        return true;
    }
    return true;
}

static int player_level(void);
static void cast_shelf(void) {
    if (cw_running()) { /* the original's cast (RUNES.C's try_cast, player_cast; SPELLS.C) */
        cw_cast();
        return;
    }
    spells_load();
    if (!G.nshelf) return;
    int r[3] = {24, 24, 24};
    for (int i = 0; i < G.nshelf; i++) r[i] = G.shelf[i];
    /* too soon after the last cast (UW.EXE 0x78dd9): effect 21, nothing cast */
    if (G.clock < G.cast_time + G.cast_wait) return sfx_play_(21, 0x40, 0);
    int n = -1;
    for (int k = 0; k < 48 && n < 0; k++)
        if (g_spell[k].rune[0] == r[0] && g_spell[k].rune[1] == r[1] && g_spell[k].rune[2] == r[2]) n = k;
    if (n < 0) return game_message("Not a spell"); /* 0x78cf8: the executable's own text (DS 0x16e7) */
    /* a failure (0x78ea5): the fizzle (effect 22) and message 210 + n */
#define FIZZLE(k) (sfx_play_(22, 0x40, 0), game_message("%s", s1(210 + (k))))
    int circle = n / 6 + 1;
    if ((player_level() + 1) / 2 < circle) return FIZZLE(0);
    if ((int)G.mana < circle * 3) return FIZZLE(1);
    int r2 = skill_check(game_skill(9) + 5, circle * 2) /* Casting */;
    if (r2 == 0) return FIZZLE(2);
    int cat = g_spell[n].cat, aim = g_spell[n].aim, str = g_spell[n].str;
    if (r2 < 0) game_message("%s", s1(214)), cat = 9, aim = 0, str = circle / 2; /* the backfire: the harm instead */
    /* the wait before the next cast (0x78f9c): ((2 n - level) x 4 + 0x40) as a byte, from the clock now */
    G.cast_wait = (u8)(((u8)(n * 2) - (u8)player_level()) * 4 + 0x40);
    G.cast_time = G.clock;
    if (r2 > 0) G.mana -= (float)(circle * 3);
    if (!spell_effect(cat, aim, str, circle)) return FIZZLE(3);
    sfx_play_(16, 0x40, 0); /* it took (0x78ffe) */
#undef FIZZLE
    if (r2 > 0) logf_("cast: %s\n", g_spell[n].name);
}
/* a potion, a scroll, a wand: its enchantment's effect (circle 3 for the estimated ones) */
bool game_enchant_effect(int cat, int strength) {
    if (cat == 16) { /* the spell numbered strength (the major form's group 0) */
        spells_load();
        if (strength < 0 || strength >= 48) return false;
        const Spell *s = &g_spell[strength];
        return spell_effect(s->cat, s->aim, s->str, strength / 6 + 1);
    }
    return spell_effect(cat, 0, strength, 3);
}

int game_set_shelf(const char *runes) {
    G.nshelf = 0;
    char w[8];
    int used = 0;
    for (const char *p = runes; G.nshelf < 3 && sscanf(p, " %7[A-Za-z]%n", w, &used) == 1; p += used) {
        for (int j = 0; j < 24; j++)
            if (!strcasecmp(w, RUNE_NAME[j])) {
                if (cw_running()) cw_debug_give_rune(j), cw_rune_add(j), G.nshelf++; /* the checks: into the core's bag */
                else G.shelf[G.nshelf++] = j, inv_give(0xe8 + j, 40);
            }
        while (p[used] == ',' || p[used] == '+') used++;
    }
    return G.nshelf;
}

/* the runes the player has (in the pack, the rune bag included) */
static bool has_rune(int r) { return cw_running() ? cw_has_rune(r) != 0 : inv_count(0xe8 + r) > 0; } /* the core: its rune bag */
/* the rune panel: the 6 x 4 runes on the left, the shelf's three slots and the Cast button in a column on the right */
enum { RUNE_PANEL_W = 8 + 6 * 34 + 6 + 44 + 8 };
static void rune_panel_at(int *px, int *py, int *sx) {
    const Overlay *o = ui_overlay();
    *px = (o->w - RUNE_PANEL_W) / 2, *py = 24, *sx = *px + 8 + 6 * 34 + 6;
}

void game_status(void) {
    u32 secs = G.clock / 256, day = secs / 86400 + 1, hour = secs / 3600 % 24;
    char a[160], b[160], c[200];
    snprintf(a, sizeof a, "%s%s%s", s1(65), s1(410 + (G.lv ? G.lv->number : 1)), s1(66));
    if (day <= 100) snprintf(b, sizeof b, "%s%s%s", s1(67), s1(410 + day), s1(68));
    else snprintf(b, sizeof b, "%s", s1(69));
    snprintf(c, sizeof c, "%s%s.", s1(70), s1(71 + hour / 2));
    game_message("%s %s %s", a, b, c);
    int hunger, fatigue, drunk, poison;
    inv_body(&hunger, &fatigue, &drunk, &poison);
    inv_status(a, sizeof a);
    snprintf(b, sizeof b, "%s%d.  %s%d.", s1(89), (int)G.hp, s1(90), (int)G.mana);
    if (poison > 0) snprintf(c, sizeof c, "  %s%s%s", s1(91), s1(84 + MIN(4, (poison - 1) / 3)), s1(92));
    else c[0] = 0;
    game_message("%s %s%s", a, b, c);
}

int game_terrain(float x, float z) { return terrain_at((int)floorf(x), (int)floorf(z)); }

/* Falling (UW.EXE 0x1d0c3): d from the fall's speed; an Acrobat check against 2d takes off a share (30 - Acrobat)
 * / 30 of it; above 3 it hurts that much. Our speed to the original's d: v^2 / 6 (a fall of two tiles about 6:
 * estimated) */
void game_landed(float v) {
    int d = (int)(v * v / 6.0f);
    if (d <= 0) return;
    int acro = game_skill(17);
    if (skill_check(acro, 2 * d) > 0) d = d * (30 - acro) / 30;
    if (d > 3) G.hp -= (float)d;
    if (d > 1) sfx_play_(0xf, 0x40, (d << 2) - 60); /* the thud (UW.EXE PHYSICS.C: effect 15 at (harm x 4) - 60) */
}

void game_store_level(void) {
    for (int k = 0; k < g_ncr; k++) {
        const Creature *c = &g_cr[k];
        if (c->obj <= 0 || c->obj >= 256 || c->state >= CS_DYING) continue;
        Object *o = &G.lv->obj[c->obj];
        o->mobile[0] = (u8)((o->mobile[0] & 0x80) | MAX(1, MIN(127, (int)c->hp)));
    }
}

static void creatures_load(void) {
    g_ncr = 0;
    for (int t = 0; t < 64 * 64; t++)
        for (int i = G.lv->tile[t].first_object, guard = 0; i > 0 && i < 1024 && guard < 1024; i = G.lv->obj[i].next, guard++) creature_add(i, t);
}

static int seg_len(const u8 *seg) {
    int n = 0;
    while (n < 8 && seg[n] != 255) n++;
    return n;
}

/* the frame to show: the slot's segment, at 6 frames a second (held at its end for a dying one) */
static int creature_frame(const Creature *c) {
    const Mesh *m = G.mesh;
    if (!m->crit[c->kind].loaded) return -1;
    static const int SLOT[6] = {0, 5, 1, 7, 12, 12};
    int slot = SLOT[c->state], s = m->crit[c->kind].slots[slot];
    if (s == 255 || s >= m->crit[c->kind].nsegs) s = m->crit[c->kind].slots[0];
    if (s == 255 || s >= m->crit[c->kind].nsegs) return -1;
    const u8 *seg = m->crit[c->kind].segs[s];
    int n = seg_len(seg);
    if (!n) return -1;
    int i = (int)(c->t * 6);
    if (c->state >= CS_DYING) i = MIN(i, n - 1);
    else i %= n;
    return seg[i] < m->crit[c->kind].nframes ? seg[i] : -1;
}

/* the position in its walk (the original's OBJ_FRAME in SEQ_WALK), or -1 */
static int creature_walk_pos(const Creature *c) {
    const Mesh *m = G.mesh;
    if (c->state != CS_WALK || !m->crit[c->kind].loaded) return -1;
    int s = m->crit[c->kind].slots[5];
    if (s == 255 || s >= m->crit[c->kind].nsegs) s = m->crit[c->kind].slots[0];
    if (s == 255 || s >= m->crit[c->kind].nsegs) return -1;
    int n = seg_len(m->crit[c->kind].segs[s]);
    return n ? (int)(c->t * 6) % n : -1;
}
/* A creature's walking sound (UW.EXE critter_mv): on its walk's odd frames, by its record's byte 0x10 bits 0-3: 1
 * feet (frame 1 effect 1, frame 3 effect 2), 3 effect 5, 2 effect 0x17, 5 effect 0xD, 4 effect 0xE; at it. The
 * original plays it each AI turn spent on that frame; ours once as the frame comes (our AI and animation until the
 * AI's port). */
static void creature_walk_sound(Creature *c) {
    int i = creature_walk_pos(c);
    if (i == c->walk_i) return;
    c->walk_i = i;
    if (i < 0 || !(i & 1)) return;
    int id = -1;
    switch (obj_info(0x40 + c->kind)->material & 15) {
    case 1: id = i == 1 ? 1 : i == 3 ? 2 : -1; break;
    case 3: id = 5; break;
    case 2: id = 0x17; break;
    case 5: id = 0xd; break;
    case 4: id = 0xe; break;
    }
    if (id >= 0) sfx_at_(id, c->x, c->z, 0);
}
/* A creature starts dying (UW.EXE crit_die): its record's death kind (byte 8 bits 0-2) 1 sounds effect 6. (UW1's
 * crit_die reads the kind and the place of the creature whose AI turn it is, not always the victim's: ours the
 * victim's, as UW2 does, until the AI's port.) */
static void creature_death_sound(const Creature *c) {
    if (obj_info(0x40 + c->kind)->death == 1) sfx_at_(6, c->x, c->z, 0);
}

static bool walkable(float x, float z, float y) {
    static const float OX[4] = {0.2f, -0.2f, 0, 0}, OZ[4] = {0, 0, 0.2f, -0.2f};
    for (int k = 0; k < 4; k++) {
        if (level_solid_at(G.lv, x + OX[k], z + OZ[k]) || game_blocked(x + OX[k], z + OZ[k], 0.02f)) return false;
        if (level_floor_y(G.lv, x + OX[k], z + OZ[k]) > y + 0.3f) return false;
    }
    return true;
}

static bool sees(float ax, float az, float bx, float bz) {
    float dx = bx - ax, dz = bz - az, d = sqrtf(dx * dx + dz * dz);
    for (float s = 0.2f; s < d; s += 0.2f)
        if (level_solid_at(G.lv, ax + dx * s / d, az + dz * s / d)) return false;
    return true;
}

static void creature_name(const Creature *c, char *out, size_t n) { item_name(&G.lv->obj[c->obj], out, n); }
/* --- combat, as UW.EXE does it ---------------------------------------------------------------------------------
 * The skill check (file 0x3419c): r = skill - difficulty + a roll of 0-30; above 28 a critical (2), above 15 a
 * success (1), above 2 a failure (0), else a fumble (-1). Dice (0x38eee): n dice of 1-sides each. */
static int search_skill(void);
static int skill_check(int skill, int difficulty) { return uw_skill_check(skill, difficulty); } /* 0x3419c */
static int dice(int n, int sides) { return uw_dice(n, sides); }                                 /* 0x38eee */
/* the damage roll (0x24cb5): at least 2; (base / 6) six-sided dice and one with (base % 6) sides */
static int roll_damage(int base) {
    base = MAX(base, 2);
    return dice(base / 6, 6) + (base % 6 ? dice(1, base % 6) : 0);
}
/* where a blow lands (0x2441a), by its height against the target's: 0 body, 1 arms, 2 legs, 3 head */
static int hit_part(float bottom, float top, float h) {
    float mid = (bottom + top) * 0.5f;
    if (h < bottom + 1.0f / 32) return 2;
    if (h > top - 1.0f / 32) return 3;
    if (h < mid) return rnd(2) ? 2 : (rnd(3) ? 0 : 1);
    return !rnd(3) ? 3 : (rnd(3) ? 0 : 1);
}
/* the angle bonus (0x25160): 0 face to face .. 4 from behind; headings in eighths */
static int angle_bonus(int target_dir, int attacker_dir) {
    int a = (target_dir + 12 - attacker_dir) & 7;
    return a > 4 ? 8 - a : a;
}
static int dir8(float dx, float dz) { return ((int)floorf(atan2f(dx, dz) / 0.785398f + 0.5f) % 8 + 8) % 8; }
#define CREATURE_HEIGHT 0.75f
#define EYE_HEIGHT 0.55f /* as main.c EYE */

/* --- the character (data\character.txt) ----------------------------------------------------------------------
 * The numbers the rules use: attributes, the skills (in UW.EXE's order: the player record from 0x1e: strength,
 * dexterity, intelligence, then attack, defense, unarmed, sword, axe, mace, missile, mana, lore, casting, traps,
 * search, track, sneak, repair, charm, picklock, acrobat, appraise, swimming), vitality, experience and the easy
 * setting (player byte 0xb4: +7 to attack, half the damage taken). Stand-ins until character creation is done. */
enum { SK_ATTACK, SK_DEFENSE, SK_UNARMED, SK_SWORD, SK_AXE, SK_MACE, SK_MISSILE, SK_MANA, SK_LORE, SK_CASTING, SK_TRAPS,
       SK_SEARCH, SK_TRACK, SK_SNEAK, SK_REPAIR, SK_CHARM, SK_PICKLOCK, SK_ACROBAT, SK_APPRAISE, SK_SWIMMING, SKILLS };
static const char *const SKILL_KEY[SKILLS] = {"attack", "defense", "unarmed", "sword", "axe", "mace", "missile", "mana", "lore", "casting",
                                              "traps", "search", "track", "sneak", "repair", "charm", "picklock", "acrobat", "appraise", "swimming"};
static struct {
    int str, dex, intl, skill[SKILLS], vitality, level, skill_points;
    int skill_points_earned; /* player byte 0x53: the points the experience has given (one every 3000 tenths) */
    int talismans;           /* the talismans left to destroy (8 at the start; with none, death keeps the player at 4) */
    u32 exp;                 /* player 0x4e: in tenths (shown / 10) */
    bool easy;
    int cls, sex, hand, head; /* class 0-7 (block 2: 23 +), sex 0 male 1 female, hand 0 left 1 right, portrait 0-4 */
    bool created;             /* made by character creation (else the stand-in numbers) */
    char name[24];
} P;
static int character_vitality(void) { return P.vitality; }
int game_skill(int k) { return k >= 0 && k < SKILLS ? P.skill[k] : 0; }
static int player_level(void) { return P.level; }
void game_set_char_level(int level) {
    P.level = MAX(1, MIN(16, level)), P.skill[SK_MANA] = MAX(P.skill[SK_MANA], 20), P.skill[SK_CASTING] = MAX(P.skill[SK_CASTING], 25);
    P.intl = MAX(P.intl, 25);
    character_derive(true);
}
static void gain_exp(int v);
static void character_lose_exp(void) { gain_exp(-(int)(P.exp >> 3)); } /* death: an eighth (SKILLS.C:832) */
static const char *player_name(void) { return P.name; }
static int player_talismans(void) { return P.talismans; }
int game_attr(int k) { return k == 0 ? P.str : k == 1 ? P.dex : P.intl; }
bool game_easy(void) { return P.easy; }

/* UW.EXE's player tick (0x2adee, 0x2af03): every 3rd tick mana + the result of a mana-skill check against 10 (when
 * it succeeds: 1 or 2) and poison hurts by its level; every 24th a strength check against 15 heals 1 hit point.
 * Resting and sleep heal more (to come). */
/* Swimming (UW.EXE 0x2b02e, each player tick while the tiring is past 80): a Swimming check against the load
 * (weight x 32 / what can be carried); short of success the tiring grows by (3 - result)d4 (to 140); past 120 a
 * second check, short of a critical: drowning, 2d(2 - result + 2) */
static int skill_check(int skill, int difficulty);
static int dice(int n, int sides);
static void swim_tick(void) {
    int load = inv_capacity() > 0 ? inv_carried() * 32 / inv_capacity() : 0;
    int r = skill_check(game_skill(19), load);
    if (r < 1 && G.swim < 140) G.swim += dice(3 - r, 4);
    if (G.swim > 120) {
        int h = 2 - skill_check(game_skill(19), load);
        if (h > 0) G.hp -= (float)dice(2, h + 2);
    }
}

static void spells_tick(void);
void game_player_tick(int tick, int poison) {
    if (G.swim > 80) swim_tick();
    spells_tick();
    if (spell_has(11, 14)) G.hp = MIN(G.hp_max, G.hp + 1.0f);     /* regeneration (estimate: a point a tick) */
    if (spell_has(11, 15)) G.mana = MIN(G.mana_max, G.mana + 1.0f); /* mana regeneration (the same) */
    if (tick % 3 == 0) {
        int r = skill_check(P.skill[SK_MANA], 10);
        if (r > 0) G.mana = MIN(G.mana_max, G.mana + (float)r);
        if (poison > 0) G.hp = MAX(1.0f, G.hp - (float)poison);
    }
    if (tick % 24 == 0 && skill_check(P.str, 15) > 0) G.hp = MIN(G.hp_max, G.hp + 1);
}
static int search_skill(void) { return P.skill[SK_SEARCH]; }
static void character_read(const char *path);
static void character_load(void) {
    static const int DEF[SKILLS] = {6, 6, 4, 6, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
    P.str = 20, P.dex = 15, P.intl = 13, P.vitality = 30, P.exp = 0, P.easy = false, P.level = 1, P.skill_points = 0, P.skill_points_earned = 0;
    P.cls = 0, P.sex = 0, P.hand = 1, P.head = 0, P.created = false;
    str_copy(P.name, sizeof P.name, "Avatar");
    memcpy(P.skill, DEF, sizeof DEF);
    char path[700];
    snprintf(path, sizeof path, "%s\\character.txt", G.data);
    if (!file_exists(path)) {
        char text[2048];
        size_t n = (size_t)snprintf(text, sizeof text,
                                    "# uo-new character: the numbers the original's rules use (stand-ins until character creation is\r\n"
                                    "# done). Edit freely; delete to get these back.\r\n"
                                    "strength %d\r\ndexterity %d\r\nintelligence %d\r\nvitality %d\r\n", P.str, P.dex, P.intl, P.vitality);
        for (int k = 0; k < SKILLS; k++) n += (size_t)snprintf(text + n, sizeof text - n, "%s %d\r\n", SKILL_KEY[k], P.skill[k]);
        n += (size_t)snprintf(text + n, sizeof text - n, "experience 0\r\nlevel 1\r\nskill_points 0\r\neasy 0                # 1: +7 to attack, half the damage taken (UW.EXE)\r\n");
        make_dirs_for(path);
        file_write(path, text, n);
    }
    character_read(path);
}
static void character_read(const char *path) {
    size_t mark = arena_mark();
    Bytes f = file_read(path);
    for (const char *p = (const char *)f.data; p && p < (const char *)f.data + f.size;) {
        char key[32];
        int v;
        if (!strncmp(p, "name ", 5)) { /* the rest of the line */
            int k = 0;
            for (const char *q = p + 5; *q && *q != '\r' && *q != '\n' && k < (int)sizeof P.name - 1; q++) P.name[k++] = *q;
            P.name[k] = 0;
        } else if (sscanf(p, "%31s %d", key, &v) == 2) {
            if (!strcmp(key, "strength")) P.str = v;
            else if (!strcmp(key, "dexterity")) P.dex = v;
            else if (!strcmp(key, "intelligence")) P.intl = v;
            else if (!strcmp(key, "vitality")) P.vitality = MAX(1, v);
            else if (!strcmp(key, "experience")) P.exp = (u32)MAX(0, v);
            else if (!strcmp(key, "level")) P.level = MAX(1, MIN(16, v));
            else if (!strcmp(key, "skill_points")) P.skill_points = MAX(0, v);
            else if (!strcmp(key, "skill_points_earned")) P.skill_points_earned = MAX(0, v);
            else if (!strcmp(key, "talismans")) P.talismans = MAX(0, v);
            else if (!strcmp(key, "easy")) P.easy = v != 0;
            else if (!strcmp(key, "class")) P.cls = MAX(0, MIN(7, v));
            else if (!strcmp(key, "sex")) P.sex = v != 0;
            else if (!strcmp(key, "hand")) P.hand = v != 0;
            else if (!strcmp(key, "portrait")) P.head = MAX(0, MIN(4, v));
            else if (!strcmp(key, "created")) P.created = v != 0;
            else
                for (int k = 0; k < SKILLS; k++)
                    if (!strcmp(key, SKILL_KEY[k])) P.skill[k] = v;
        }
        const char *nl = memchr(p, '\n', (size_t)((const char *)f.data + f.size - p));
        p = nl ? nl + 1 : NULL;
    }
    arena_reset(mark);
}

/* Derived numbers (UW.EXE 0x81305): max hit points = 30 + level x strength / 5; max mana = (mana skill + 1) x
 * intelligence / 8; carry limit = strength x 20 tenths of a stone. */
static void character_derive(bool refill) {
    G.hp_max = (float)(30 + P.level * P.str / 5);
    G.mana_max = (float)((P.skill[SK_MANA] + 1) * P.intl / 8);
    if (refill) G.hp = G.hp_max, G.mana = G.mana_max;
    G.hp = MIN(G.hp, G.hp_max), G.mana = MIN(G.mana, G.mana_max);
    inv_set_capacity(P.str * 20);
}
/* the character as it is, into a character.txt (data\\character.txt: the one a new game starts with; a save has its
 * own) */
static void character_save_to(const char *path);
static void character_save(void) {
    char path[700];
    snprintf(path, sizeof path, "%s\\character.txt", G.data);
    character_save_to(path);
}
static void character_save_to(const char *path) {
    char text[2048];
    size_t n = (size_t)snprintf(text, sizeof text,
                                "# uo-new character (made by character creation; edit freely, delete to make a new one)\r\n"
                                "created %d\r\nname %s\r\nclass %d\r\nsex %d\r\nhand %d\r\nportrait %d\r\n"
                                "strength %d\r\ndexterity %d\r\nintelligence %d\r\nvitality %d\r\n",
                                P.created, P.name, P.cls, P.sex, P.hand, P.head, P.str, P.dex, P.intl, P.vitality);
    for (int k = 0; k < SKILLS; k++) n += (size_t)snprintf(text + n, sizeof text - n, "%s %d\r\n", SKILL_KEY[k], P.skill[k]);
    n += (size_t)snprintf(text + n, sizeof text - n, "experience %u\r\nlevel %d\r\nskill_points %d\r\nskill_points_earned %d\r\ntalismans %d\r\neasy %d                # 1: +7 to attack, half the damage taken (UW.EXE)\r\n",
                          (unsigned)P.exp, P.level, P.skill_points, P.skill_points_earned, P.talismans, P.easy);
    make_dirs_for(path);
    file_write(path, text, n);
}

/* Skill advancement (UW.EXE 0x8141f; character creation's picks and the shrines' mantras): a skill at 0 gains 3 +
 * attribute / 9 + 0-2 and then three attribute checks against 20; above 0, 1 + attribute / 13 + 0-1 and two
 * checks; at most 30. The attribute (0x81402): strength for attack to missile, intelligence for mana, lore and
 * casting, dexterity for the rest. */
static int skill_check(int skill, int difficulty);
static int skill_advance(int k) {
    int s = P.skill[k], add = s ? 1 : 3, div = s ? 13 : 9, rounds = s ? 2 : 3;
    int a = k < 7 ? P.str : k < 10 ? P.intl : P.dex, before = s;
    s += add + a / div + rnd(rounds);
    for (int r = 0; r < rounds; r++) s += skill_check(a, 20);
    P.skill[k] = MAX(0, MIN(30, s));
    return P.skill[k] - before;
}

/* A shrine's advance (UW.EXE 0x814e1): not past 30 nor past twice the governing attribute; +1; a dexterity or
 * intelligence skill +1 more while below half the attribute; +1 more on rnd % (25 str, 40 dex, 10 int) < attribute -
 * skill. True: it went up. */
static bool shrine_advance(int k) {
    int a = k < 7 ? P.str : k < 10 ? P.intl : P.dex, ai = k < 7 ? 0 : k < 10 ? 2 : 1;
    static const int DIV[3] = {25, 40, 10};
    if (2 * a < P.skill[k] || P.skill[k] >= 30) return false;
    P.skill[k]++;
    if (ai && a / 2 > P.skill[k]) P.skill[k]++;
    if (P.skill[k] < a && rnd(32768) % DIV[ai] < a - P.skill[k]) P.skill[k]++;
    P.skill[k] = MIN(30, P.skill[k]);
    return true;
}
/* A mantra chanted at a shrine (UW.EXE 0x816fd): strings block 2, 51-76. The 20 skill mantras (RA attack .. ONO
 * swimming) cost a skill point (none: "You are not ready to advance."), two advances; SUMM RA (combat skills, 3 tries),
 * MU AHM (magic, 2), OM CAH (the other ten, 4) a random skill each try; INSAHN, FANLO: the quest's (to come). */
static void chant(const char *m) {
    int id = -1;
    for (int k = 51; k <= 76 && id < 0; k++)
        if (str_get(2, k)[0] && !strcasecmp(m, str_get(2, k))) id = k - 51;
    if (id < 0) return game_message("%s", str_get(1, 25)); /* "That is not a mantra." */
    if (id >= 20 && id <= 22) return; /* INSAHN, FANLO, NO: the quest's */
    if (P.skill_points <= 0) return game_message("%s", str_get(1, 24)); /* "You are not ready to advance." */
    if (id < 20) {
        bool a = shrine_advance(id), b = shrine_advance(id);
        if (a || b) {
            P.skill_points--;
            game_message("%s", str_get(1, 26)); /* "Knowledge and Understanding fill you." */
            game_message("%s%s.", str_get(1, 28), str_get(2, 31 + id));
        } else game_message("%s", str_get(1, 27));
        return;
    }
    static const int FIRST[3] = {0, 7, 10}, COUNT[3] = {7, 3, 10}, TRIES[3] = {3, 2, 4};
    int g = id - 23, up[4], n = 0;
    for (int t = 0; t < TRIES[g]; t++) {
        int k = FIRST[g] + rnd(COUNT[g]);
        if (FIRST[g] == 7 && P.skill[7] < 8 && rnd(2)) k = 7;
        if (shrine_advance(k) && n < 4) up[n++] = k;
    }
    if (!n) return game_message("%s", str_get(1, 30)); /* "None of your skills improved." */
    P.skill_points--;
    char line[200];
    int len = snprintf(line, sizeof line, "%s", str_get(1, 29));
    for (int i = 0; i < n; i++) len += snprintf(line + len, sizeof line - (size_t)len, "%s%s", i == 0 ? "" : i == n - 1 ? " and " : ", ", str_get(2, 31 + up[i]));
    game_message("%s.", line);
}

/* --- character creation (UW.EXE overlay 5991: 0x6c830 the new player, 0x6cdb2 the class, 0x6ca88 the skill
 * sets, 0x6cd85 the picks) ----------------------------------------------------------------------------------- */
typedef struct {
    int base[3], bonus, nsets, n[6], set[6][12];
} ClassDef;
static ClassDef CLS[8];
static bool g_cls_loaded;
static void classes_load(void) {
    if (g_cls_loaded) return;
    g_cls_loaded = true;
    char path[700];
    snprintf(path, sizeof path, "%s\\character\\classes.txt", G.data);
    size_t mark = arena_mark();
    Bytes f = file_read(path);
    for (const char *p = (const char *)f.data; p && p < (const char *)f.data + f.size;) {
        int c, st, dx, in, bo, used = 0;
        if (sscanf(p, "class %d str %d dex %d int %d bonus %d sets%n", &c, &st, &dx, &in, &bo, &used) == 5 && c >= 0 && c < 8) {
            CLS[c].base[0] = st, CLS[c].base[1] = dx, CLS[c].base[2] = in, CLS[c].bonus = bo, CLS[c].nsets = 0;
            const char *q = p + used;
            int s = 0;
            CLS[c].n[0] = 0;
            while (*q && *q != '\r' && *q != '\n' && s < 6) {
                int v, u;
                if (*q == '/') s++, CLS[c].n[s < 6 ? s : 5] = 0, q++;
                else if (sscanf(q, "%d%n", &v, &u) == 1) {
                    if (CLS[c].n[s] < 12) CLS[c].set[s][CLS[c].n[s]++] = v;
                    q += u;
                } else q++;
            }
            CLS[c].nsets = MIN(6, s + 1);
        }
        const char *nl = memchr(p, '\n', (size_t)((const char *)f.data + f.size - p));
        p = nl ? nl + 1 : NULL;
    }
    arena_reset(mark);
}

enum { CG_SEX, CG_HAND, CG_CLASS, CG_SKILL, CG_HEAD, CG_DIFF, CG_NAME, CG_KEEP };
static struct {
    bool on;
    int step, set, picks[6], npicks;
    char name[24];
    int ox[12], oy[12], ow[12], oh[12], nopt; /* the options as drawn (clicks) */
} CG;

bool game_needs_character(void) { return !P.created; }

bool game_save(const char *dir) {
    char path[700];
    snprintf(path, sizeof path, "%s\\character.txt", dir);
    make_dirs_for(path);
    character_save_to(path);
    snprintf(path, sizeof path, "%s\\game.txt", dir);
    char text[4096];
    size_t n = (size_t)snprintf(text, sizeof text, "# the game's state (uo-new save)\r\nhp %.2f\r\nmana %.2f\r\nfight %d\r\nclock %u\r\nsilver %d %.3f %.3f\r\nquest %u\r\nvars",
                                (double)G.hp, (double)G.mana, G.fight, (unsigned)G.clock, G.silver_level, (double)G.silver_x, (double)G.silver_z,
                                (unsigned)g_quest);
    for (int i = 0; i < 128; i++) n += (size_t)snprintf(text + n, sizeof text - n, " %d", g_var[i]);
    n += (size_t)snprintf(text + n, sizeof text - n, "\r\nshelf %d %d %d\r\n", G.nshelf > 0 ? G.shelf[0] : 24, G.nshelf > 1 ? G.shelf[1] : 24,
                          G.nshelf > 2 ? G.shelf[2] : 24);
    for (int k = 0; k < G.nactive; k++) n += (size_t)snprintf(text + n, sizeof text - n, "spell %d %d %d\r\n", G.act_cat[k], G.act_str[k], G.act_ticks[k]);
    bool ok = file_write(path, text, n);
    return ok && inv_save(dir) && conv_save(dir);
}

bool game_load(const char *dir) {
    char path[700];
    snprintf(path, sizeof path, "%s\\character.txt", dir);
    if (!file_exists(path)) return false;
    character_read(path);
    character_save(); /* the character playing now */
    character_derive(false);
    snprintf(path, sizeof path, "%s\\game.txt", dir);
    size_t mark = arena_mark();
    Bytes f = file_read(path);
    for (const char *p = (const char *)f.data; p && p < (const char *)f.data + f.size;) {
        const char *nl = memchr(p, '\n', (size_t)((const char *)f.data + f.size - p));
        double v, v2;
        unsigned u;
        int used;
        if (sscanf(p, "hp %lf", &v) == 1) G.hp = (float)v;
        else if (sscanf(p, "mana %lf", &v) == 1) G.mana = (float)v;
        else if (sscanf(p, "fight %u", &u) == 1) G.fight = u != 0;
        else if (sscanf(p, "clock %u", &u) == 1) G.clock = u;
        else if (!strncmp(p, "shelf", 5)) {
            int a[3] = {24, 24, 24};
            G.nshelf = 0;
            if (sscanf(p, "shelf %d %d %d", &a[0], &a[1], &a[2]) >= 1)
                for (int i = 0; i < 3; i++)
                    if (a[i] >= 0 && a[i] < 24) G.shelf[G.nshelf++] = a[i];
        } else if (!strncmp(p, "spell ", 6) && G.nactive < 3) {
            int c, s, t;
            if (sscanf(p, "spell %d %d %d", &c, &s, &t) == 3) G.act_cat[G.nactive] = c, G.act_str[G.nactive] = s, G.act_ticks[G.nactive] = t, G.nactive++;
        }
        else if (sscanf(p, "silver %d %lf %lf", &used, &v, &v2) == 3) G.silver_level = used, G.silver_x = (float)v, G.silver_z = (float)v2;
        else if (sscanf(p, "quest %u", &u) == 1) g_quest = u;
        else if (!strncmp(p, "vars", 4)) {
            const char *q = p + 4;
            for (int i = 0; i < 128 && sscanf(q, "%u%n", &u, &used) == 1; i++) g_var[i] = (u8)u, q += used;
        }
        p = nl ? nl + 1 : NULL;
    }
    arena_reset(mark);
    G.nactive = 0;
    inv_load(dir);
    conv_load(dir);
    G.open = G.sheet = G.talking = G.reading = G.dead = false, G.charge = -1, G.death_choice = 0;
    return true;
}

void game_new(void) {
    G.clock = 0x010b3000u;
    g_quest = 0;
    memset(g_var, 0, sizeof g_var);
    conv_forget();
}
/* The starting record of a new character (UW.EXE init_char, underworld-exhumed CHARGEN.C:88): level 1, no
 * experience, one skill point, the clock at 0x10B3000, no silver tree, no poison, no lasting spells, no runes on the
 * shelf, the quests and game variables zero but variable 0x1A = 0x35, eight talismans, hunger 0xC0, fatigue 0x40,
 * nothing drunk; the portrait rand() % 5 and the sex rand() & 1; unless blank (the creation screens follow) every
 * skill 3d4 and every attribute 2d10 + 10 (in that order: the same rolls as the original); the maxima (player_compute,
 * mana filled), and hit points the maximum less 6 + rand() % 6. */
static void init_char(bool blank) {
    P.hand = 1;
    P.exp = 0, P.skill_points = 1, P.skill_points_earned = 0, P.level = 1;
    G.clock = 0x10b3000u;
    G.silver_level = 0;
    G.nactive = 0, G.nshelf = 0;
    P.talismans = 8;
    g_quest = 0;
    memset(g_var, 0, sizeof g_var);
    g_var[0x1a] = 0x35;
    inv_set_body(0xc0, 0x40, 0, 0);
    P.head = uw_rand() % 5;
    P.sex = uw_rand() & 1;
    for (int i = 0; i < SKILLS; i++) P.skill[i] = blank ? 0 : uw_dice(3, 4);
    P.str = blank ? 0 : uw_dice(2, 10) + 10;
    P.dex = blank ? 0 : uw_dice(2, 10) + 10;
    P.intl = blank ? 0 : uw_dice(2, 10) + 10;
    character_derive(false);
    G.mana = G.mana_max;
    G.hp = (float)((u8)(30 + P.level * P.str / 5) - 6 - uw_rand() % 6);
}
/* a new game's character: random (blank false: the original's roll, played at once) or blank for the creation screens */
void game_init_char(bool blank) {
    init_char(blank);
    P.created = !blank, P.cls = 0, P.easy = false;
    str_copy(P.name, sizeof P.name, "Avatar");
}

bool game_chargen_on(void) { return CG.on; }
void game_chargen(bool on) {
    CG.on = on, CG.step = CG_SEX, CG.npicks = 0, CG.name[0] = 0;
    if (on) classes_load();
}

/* the class: its attributes, the bonus points dealt out (0x6cdb2), the skills from nothing */
static void cg_class(int c) {
    P.cls = c;
    P.str = CLS[c].base[0], P.dex = CLS[c].base[1], P.intl = CLS[c].base[2];
    for (int left = CLS[c].bonus; left > 0;) {
        int n = MIN(rnd(4) + 1, left), k = rnd(3);
        int *a = k == 0 ? &P.str : k == 1 ? &P.dex : &P.intl;
        if (*a + n > 30) n = 30 - *a;
        *a += n, left -= n;
        if (P.str >= 30 && P.dex >= 30 && P.intl >= 30) break;
    }
    memset(P.skill, 0, sizeof P.skill);
    CG.set = 0, CG.npicks = 0;
}
/* the next skill set: one skill is given, more are asked; after the last, the picks advance their skills */
static void cg_next_set(void) {
    const ClassDef *c = &CLS[P.cls];
    while (CG.set < c->nsets && c->n[CG.set] <= 1) {
        if (c->n[CG.set] == 1) CG.picks[CG.npicks++] = c->set[CG.set][0];
        CG.set++;
    }
    if (CG.set < c->nsets) return (void)(CG.step = CG_SKILL);
    for (int k = 0; k < CG.npicks; k++) skill_advance(CG.picks[k]);
    P.level = 1, P.exp = 0, P.skill_points = 1, P.skill_points_earned = 0; /* the new player (0x6c830) */
    character_derive(true);
    CG.step = CG_HEAD;
}
static void cg_choose(int i) {
    switch (CG.step) {
    case CG_SEX: P.sex = i, CG.step = CG_HAND; break;
    case CG_HAND: P.hand = i, CG.step = CG_CLASS; break;
    case CG_CLASS: cg_class(i), cg_next_set(); break;
    case CG_SKILL:
        CG.picks[CG.npicks++] = CLS[P.cls].set[CG.set][i], CG.set++;
        cg_next_set();
        break;
    case CG_HEAD: P.head = i, CG.step = CG_DIFF; break;
    case CG_DIFF: P.easy = i == 1, CG.step = CG_NAME; break;
    case CG_KEEP:
        if (i == 0) {
            P.created = true;
            str_copy(P.name, sizeof P.name, CG.name[0] ? CG.name : "Avatar");
            character_derive(true);
            character_save();
            CG.on = false;
            sound_song_restart(); /* (UW.EXE 0x6d5c7) */
            game_message("%s", str_get(1, 13)); /* "Welcome to the Underworld." */
        } else game_chargen(true);
        break;
    }
}
/* the step's question and its answers */
static int cg_options(const char **opt, int *prompt) {
    int n = 0;
    switch (CG.step) {
    case CG_SEX: *prompt = 1, opt[n++] = str_get(2, 9), opt[n++] = str_get(2, 10); break;
    case CG_HAND: *prompt = 2, opt[n++] = str_get(2, 11), opt[n++] = str_get(2, 12); break;
    case CG_CLASS:
        *prompt = 3;
        for (int c = 0; c < 8; c++) opt[n++] = str_get(2, 23 + c);
        break;
    case CG_SKILL:
        *prompt = 4;
        for (int k = 0; k < CLS[P.cls].n[CG.set]; k++) opt[n++] = str_get(2, 31 + CLS[P.cls].set[CG.set][k]);
        break;
    case CG_HEAD: *prompt = -1, n = 5; break;
    case CG_DIFF: *prompt = 6, opt[n++] = str_get(2, 13), opt[n++] = str_get(2, 14); break;
    case CG_NAME: *prompt = 7; break;
    case CG_KEEP: *prompt = 8, opt[n++] = str_get(2, 15), opt[n++] = str_get(2, 16); break;
    }
    return n;
}
static bool chargen_update(const GameInput *in) {
    G.mouse_x = in->mouse_x, G.mouse_y = in->mouse_y;
    if (CG.step == CG_NAME) {
        size_t len = strlen(CG.name);
        if (in->text == 8 && len) CG.name[len - 1] = 0;
        else if (in->text == 13) CG.step = CG_KEEP;
        else if (in->text >= 32 && in->text < 127 && len < sizeof CG.name - 1) CG.name[len] = (char)in->text, CG.name[len + 1] = 0;
        return true;
    }
    const char *opt[12];
    int prompt, n = cg_options(opt, &prompt), pick = in->number - 1;
    if (in->click_left || in->attack)
        for (int k = 0; k < CG.nopt && k < n; k++)
            if (in->mouse_x >= CG.ox[k] && in->mouse_x < CG.ox[k] + CG.ow[k] && in->mouse_y >= CG.oy[k] && in->mouse_y < CG.oy[k] + CG.oh[k]) pick = k;
    if (pick >= 0 && pick < n) cg_choose(pick);
    return true;
}
static void sheet_draw(const Overlay *o);
static void sheet_tip(void);
static void chargen_draw(const Overlay *o) {
    sheet_draw(o); /* the character as it is so far, on the left */
    const char *opt[12];
    int prompt, n = cg_options(opt, &prompt), lh = ui_font_height(FONT_NORMAL) + 3;
    int pw = 200, px = o->w - pw - 10, py = 10, ph = MIN(o->h - 70, 40 + MAX(n, 2) * lh + 40);
    ui_parchment(px, py, pw, ph);
    int y = py + 6;
    ui_text(FONT_NORMAL, prompt >= 0 ? str_get(2, prompt) : "Choose thy face:", px + 8, y, UI_INK_HEAD), y += lh + 4;
    CG.nopt = 0;
    if (CG.step == CG_HEAD) { /* the five faces (HEADS.GR: 0-4 male, 5-9 female) */
        for (int k = 0; k < 5; k++) {
            const Pic *h = ui_pic("ui\\heads", P.sex * 5 + k);
            int hx = px + 8 + k * 37, hy = y;
            if (h) ui_draw(h, hx, hy, 1);
            CG.ox[k] = hx, CG.oy[k] = hy, CG.ow[k] = h ? h->w : 34, CG.oh[k] = h ? h->h : 34;
            bool hov = G.mouse_x >= hx && G.mouse_x < hx + CG.ow[k] && G.mouse_y >= hy && G.mouse_y < hy + CG.oh[k];
            if (hov) ui_fill(hx, hy + CG.oh[k] + 1, CG.ow[k], 2, UI_INK_HEAD, 255);
            CG.nopt++;
        }
    } else if (CG.step == CG_NAME) {
        char line[40];
        snprintf(line, sizeof line, "%s_", CG.name);
        ui_text(FONT_NORMAL, line, px + 8, y, UI_INK), y += lh;
        ui_text(FONT_SMALL, "TYPE A NAME, ENTER WHEN DONE", px + 8, y + 4, UI_INK_FAINT);
    } else
        for (int k = 0; k < n; k++) {
            char num[8];
            snprintf(num, sizeof num, "%d.", k + 1);
            bool hov = G.mouse_y >= y && G.mouse_y < y + lh && G.mouse_x >= px && G.mouse_x < px + pw;
            if (hov) ui_fill(px + 3, y - 1, pw - 6, lh, UI_PAPER_MID, 255);
            ui_text(FONT_NORMAL, num, px + 8, y, UI_INK_GOLD);
            ui_text(FONT_NORMAL, opt[k], px + 24, y, hov ? 112 : 60, hov ? 36 : 40, hov ? 12 : 32);
            CG.ox[k] = px, CG.oy[k] = y, CG.ow[k] = pw, CG.oh[k] = lh;
            CG.nopt++, y += lh;
        }
    sheet_tip(); /* the character panel's explanations */
    ui_cursor(0, G.mouse_x, G.mouse_y);
}

/* Experience, in tenths (the panel shows exp / 10): player_get_exp (UW.EXE 0x341d8, proved: orig.c), then advance
 * (0x8138a, SKILLS.C:119): "You have attained experience level N" and the maxima again (player_compute) */
static void gain_exp(int v) {
    UwExp e = {P.exp, (u8)P.level, (u8)P.skill_points, (u8)P.skill_points_earned};
    int up = uw_player_get_exp(&e, v, G.lv ? G.lv->number : 1);
    P.exp = e.exp, P.level = e.level, P.skill_points = e.skill_points, P.skill_points_earned = e.earned;
    if (up) {
        game_message("%s%2d", str_get(1, 147), P.level); /* (" 0\n": a space before a single digit) */
        character_derive(false);
    }
}
/* a creature killed by the player (player_killed_a, 0x25b35, proved): its experience, x 1.5-3 if powerful (its
 * attitude word's bit 10: mobile byte 6 bit 2) */
static void kill_exp(const Creature *c) {
    bool powerful = c->obj > 0 && c->obj < 256 && (G.lv->obj[c->obj].mobile[6] & 4);
    gain_exp(uw_kill_exp(obj_info(0x40 + c->kind)->xp, powerful));
}

/* the player's armour on a body part (the player's critter record, parts 0-3; UW.EXE 0x7e444 and 0x7e39f): helm ->
 * head (3), chest -> body (0), gloves -> arms (1), leggings and boots -> legs (2), each protection x condition / 64
 * + 1; a shield in the off hand counts on the body and the arms; + the protection level of the spells; + Toughness
 * enchantments. It is taken off a blow's damage (0x24dc1). */
static int player_armour(int part) {
    static const int SLOTS[4][2] = {{1, -1}, {2, -1}, {3, 4}, {0, -1}};
    int a = 0, prot[4], tough[4];
    for (int k = 0; k < 2; k++)
        if (SLOTS[part][k] >= 0) a += inv_armour(SLOTS[part][k]);
    if (part <= 1) a += inv_armour(8);
    inv_enchant_armour(prot, tough);
    return a + spell_armour() + tough[part & 3];
}

/* a creature's blow at the player (0x259ee and 0x24b68): one of its three attacks by their chances; attack score =
 * the attack's bonus + the creature's attack skill / 2, less the player's protection where it lands (DS 0x266a); the
 * check against the player's defense; damage = the attack's damage + strength / 5, x1.5-2.5 on a critical, rolled
 * as dice, + the angle bonus, less the player's armour on that part (0x24dc1, not below 0); half on the easy
 * setting (0x24e43) */
static void creature_strikes(Creature *c, const float eye[3], float yaw) {
    const ObjInfo *ci = obj_info(0x40 + c->kind);
    int sum = ci->a_chance[0] + ci->a_chance[1] + ci->a_chance[2], pick = 0;
    if (sum > 0) {
        int r = rnd(sum);
        while (pick < 2 && r >= ci->a_chance[pick]) r -= ci->a_chance[pick], pick++;
    }
    int player_dir = dir8(sinf(yaw), cosf(yaw)), angle = angle_bonus(player_dir, c->dir);
    float feet = eye[1] - EYE_HEIGHT;
    int part = hit_part(feet, eye[1] + 0.1f, c->y + CREATURE_HEIGHT * 0.6f);
    int score = ci->a_bonus[pick] + ci->attack / 2 - player_protect(part);
    int r = skill_check(score + angle, P.skill[SK_DEFENSE]);
    blow_sound(r <= 0, false, c, part); /* blocked: the clang (do_miss) */
    if (r <= 0) return;
    int base = ci->a_damage[pick] + ci->strength / 5;
    if (r == 2) base = base * (48 + rnd(32)) / 32;
    int raw = roll_damage(base) + angle;
    sfx_play_(3, 0x40, (i8)(u8)(raw << 2)); /* the player hurt (0x24d48: the damage before armour x 4, a byte) */
    int dmg = MAX(0, raw - player_armour(part));
    if (P.easy) dmg /= 2;
    G.hp -= (float)dmg;
    if (G.hp > 0) sound_song_blow(G.clock, false, (int)G.hp, (int)G.hp_max); /* its fight's song (UW.EXE 0x1c48d) */
    logf_("combat: a creature (kind %d) hits the player for %d (check %d, part %d)\n", c->kind, dmg, r, part);
    char name[96];
    creature_name(c, name, sizeof name);
    const char *noun = strchr(name, ' ');
    (void)noun;
}


/* a sound effect (UW.EXE 0x1fa9b), and one at a place (0x1f82e: in eighths of a tile, heard from the player: their
 * place and heading, 0-255 clockwise from north) */
static void sfx_play_(int id, int pan, int vol) { sound_effect(id, pan, vol); }
/* a door opens or closes (UW.EXE 0x387c6 / 0x388dd): 20 for a portcullis (item & 7 == 6), else 11, at the door
 * (its tile x 8 + its fine place) */
static void sfx_at_(int id, float x, float z, int vol);
static void door_sound(const Door *d) {
    const Object *o = &G.lv->obj[d->obj];
    sfx_at_((d->item & 7) == 6 ? 20 : 11, d->tx + o->x / 8.0f, d->tz + o->y / 8.0f, 0);
}
static void sfx_at_(int id, float x, float z, int vol) {
    int h = (int)floorf(G.yaw / 6.2831853f * 256.0f + 0.5f) & 255;
    sound_effect_at(id, (int)(x * 8), (int)(z * 8), vol, (int)(G.eye[0] * 8), (int)(G.eye[2] * 8), h);
}
/* The sound of a blow that did not hurt (UW.EXE 0x25039, do_miss; the hurt ones sound 3 / 4 in the damage): a swing
 * that met nothing is the whoosh, 10, from the attacker (swing_whoosh); a blocked blow (the to-hit roll failed or
 * fumbled) is 7, the clang, when the weapon and what it strikes are both metal, else 8, from the target. The weapon
 * kind: a creature's record byte 0x10 bits 6-7; the player's from the player's own record, critter 0x3F (the
 * original takes the item id of the player's object, 0x7F). The struck: the player's armour where the blow lands
 * (the slot (part + 1) & 3), metal unless none or leather (0x20, 0x23, 0x26, 0x29, 0x2c); a creature: the
 * ATTACKER's record's armour kind, bits 4-5 (the original's slip: it reads the attacker's, not the target's). Before
 * 2026-11-09 ours had hits and misses the wrong way round (the decompilation, underworld-exhumed, showed it). */
static void swing_whoosh(bool player_attacks, const Creature *c) {
    if (player_attacks) sfx_at_(10, G.eye[0], G.eye[2], 0);
    else sfx_at_(10, c->x, c->z, 0);
}
static void blow_sound(bool blocked, bool player_attacks, const Creature *c, int part) {
    if (!blocked) return;
    int attacker = player_attacks ? 0x7f : 0x40 + c->kind;
    int weapon = (obj_info(attacker)->material >> 6) & 3, target;
    if (player_attacks) target = (obj_info(attacker)->material >> 4) & 3;
    else {
        int item = inv_slot_item((part + 1) & 3);
        target = item < 0 || item == 0x20 || item == 0x23 || item == 0x26 || item == 0x29 || item == 0x2c ? 0 : 1;
    }
    int id = weapon == 1 && target == 1 ? 7 : 8;
    if (player_attacks) sfx_at_(id, c->x, c->z, 0);
    else sfx_at_(id, G.eye[0], G.eye[2], 0);
}

static void creature_strikes(Creature *c, const float eye[3], float yaw);
static void creatures_update(float dt, const float eye[3], float yaw) {
    if (cw_running()) { /* the original's creatures move by its AI in the rules core; ours stand down, and our list is
                          * the core's creatures as they are (for pointing at them: look, talk, a blow's row) */
        const u8 *b = cw_block();
        g_ncr = 0;
        for (int t = 0; b && t < 4096; t++)
            for (int i = rd16(b + t * 4 + 2) >> 6, guard = 0; i > 0 && guard < 1024; guard++) {
                size_t off = 0x4000 + (i < 256 ? (size_t)i * 27 : 256 * 27 + (size_t)(i - 256) * 8);
                int next = rd16(b + off + 4) >> 6, item = rd16(b + off) & 511;
                if (i > 1 && i < 256 && item >= 0x40 && item < 0x80 && g_ncr < 256) {
                    Creature *c = &g_cr[g_ncr++];
                    memset(c, 0, sizeof *c);
                    float p[3];
                    world_object_pos(G.lv, t % 64, t / 64, &G.lv->obj[i], p);
                    c->obj = i, c->kind = item - 0x40, c->tile = t, c->x = p[0], c->y = p[1], c->z = p[2];
                    c->hp = b[off + 8], c->hostile = (b[off + 0x0e] >> 6) == 0;
                    c->state = (b[off + 0x15] & 0x3f) == 0xC ? CS_DYING : CS_IDLE;
                }
                i = next;
            }
        return;
    }
    for (int k = 0; k < g_ncr; k++) {
        Creature *c = &g_cr[k];
        c->t += dt;
        if (c->state == CS_DEAD) continue;
        creature_walk_sound(c);
        if ((c->frozen > 0 || spell_has(11, 0)) && c->state < CS_DYING) continue; /* paralysed, or time frozen (DS 0x282) */
        if (c->state == CS_DYING) {
            if (c->t > 1.2f) c->state = CS_DEAD;
            continue;
        }
        c->cooldown -= dt;
        if ((c->state == CS_ATTACK && c->t < 0.7f) || (c->state == CS_HIT && c->t < 0.35f)) {
            if (c->state == CS_ATTACK && !c->struck && c->t > 0.35f) {
                c->struck = true;
                float dx = eye[0] - c->x, dz = eye[2] - c->z;
                if (dx * dx + dz * dz < 1.2f * 1.2f) creature_strikes(c, eye, yaw);
                else swing_whoosh(false, c); /* its blow met nothing (do_miss(0)) */
            }
            continue;
        }
        float dx = eye[0] - c->x, dz = eye[2] - c->z, d = sqrtf(dx * dx + dz * dz);
        if (c->fear > 0 || c->confused > 0) { /* fleeing (goal 6) or wandering (goal 2) */
            float ax = c->fear > 0 ? -dx / MAX(d, 0.01f) : sinf(c->t * 0.7f + (float)k), az = c->fear > 0 ? -dz / MAX(d, 0.01f) : cosf(c->t * 0.9f + (float)k);
            float sp = 0.9f * dt, nx = c->x + ax * sp, nz = c->z + az * sp;
            if (walkable(nx, c->z, c->y)) c->x = nx;
            if (walkable(c->x, nz, c->y)) c->z = nz;
            c->y = level_floor_y(G.lv, c->x, c->z);
            c->fear = MAX(0.0f, c->fear - dt), c->confused = MAX(0.0f, c->confused - dt);
            if (c->state != CS_WALK) c->state = CS_WALK, c->t = 0;
            continue;
        }
        /* how far it notices the player: seen within the visibility (15 - Sneak / 5, conceal - 5, invisible - 16)
         * of 15 x 7 tiles, heard within the noise (13 - Sneak / 3, stealth - 16) of 13 x 4 tiles, walls or not (the
         * scales: our estimate; UW.EXE's use of DS 0x1afe / 0x1aff is at 0x3145a / 0x314e6) */
        float see = 1.0f + 6.0f * (float)player_visibility() / 15.0f, hear = 4.0f * (float)player_noise() / 13.0f;
        bool noticed = (d <= see && sees(c->x, c->z, eye[0], eye[2])) || d <= hear;
        if (!c->hostile || c->ally || !noticed) {
            if (c->state != CS_IDLE) c->state = CS_IDLE, c->t = 0;
            continue;
        }
        c->dir = dir8(dx, dz); /* it faces the player it comes for */
        sound_song_engaged(G.clock); /* the fight's song (UW.EXE 0x1b652; until the AI's port: ours, hostile and noticed) */
        if (d > 0.85f) {
            float sp = 0.9f * dt, nx = c->x + dx / d * sp, nz = c->z + dz / d * sp;
            if (walkable(nx, c->z, c->y)) c->x = nx;
            if (walkable(c->x, nz, c->y)) c->z = nz;
            c->y = level_floor_y(G.lv, c->x, c->z);
            if (c->state != CS_WALK) c->state = CS_WALK, c->t = 0;
        } else if (c->cooldown <= 0) {
            c->state = CS_ATTACK, c->t = 0, c->struck = false, c->cooldown = 1.6f;
        } else if (c->state != CS_IDLE) c->state = CS_IDLE, c->t = 0;
    }
}

/* The player's blow (UW.EXE 0x25406, 0x24b68, 0x24cb5): charge 0-100. The weapon in the right hand (else the
 * fist, weapon 15) gives the power (power_min + (power_max - power_min) x charge / 100) and its skill (sword, axe,
 * mace; else unarmed). Attack score = attack / 2 + the weapon skill + dexterity / 7 (+ 7 easy); damage base =
 * strength / 9 + the weapon's damage for the attack (unarmed: unarmed x 2 / 5 + strength / 6 + 4). The check
 * (score + the angle bonus) against the creature's defense; a critical x1.5-2.5; dice; x power / 128 + the angle
 * bonus; less the creature's armour where it lands. A kill gives experience: 4 x its value + value d2. */
/* the swing's type when it starts: where the crosshair meets the creature in reach (top third thrust 2, bottom
 * third bash 1, else slash 0; UW.EXE's view rows); nothing in reach: a slash */
static int attack_type(const float eye[3], const float fwd[3]) {
    int best = -1;
    float bt = 1.7f;
    for (int k = 0; k < g_ncr; k++) {
        Creature *c = &g_cr[k];
        if (c->state >= CS_DYING) continue;
        float p[3] = {c->x - eye[0], c->y + 0.35f - eye[1], c->z - eye[2]};
        float t = p[0] * fwd[0] + p[1] * fwd[1] + p[2] * fwd[2];
        if (t < 0.1f || t > bt) continue;
        float q[3] = {p[0] - fwd[0] * t, p[1] - fwd[1] * t, p[2] - fwd[2] * t};
        if (q[0] * q[0] + q[1] * q[1] + q[2] * q[2] > 0.4f * 0.4f) continue;
        bt = t, best = k;
    }
    if (best < 0) return 0;
    float frac = (eye[1] + fwd[1] * bt - g_cr[best].y) / CREATURE_HEIGHT;
    return frac > 2.0f / 3 ? 2 : frac < 1.0f / 3 ? 1 : 0;
}

static void player_attack(const float eye[3], const float fwd[3], float charge, int type) {
    int witem = inv_weapon();
    const ObjInfo *wi = obj_info(witem >= 0 ? witem : 15);
    int power = wi->power_min + (wi->power_max - wi->power_min) * (int)(charge * 100) / 100;
    int best = -1;
    float bt = 1.7f;
    for (int k = 0; k < g_ncr; k++) {
        Creature *c = &g_cr[k];
        if (c->state >= CS_DYING) continue;
        float p[3] = {c->x - eye[0], c->y + 0.35f - eye[1], c->z - eye[2]};
        float t = p[0] * fwd[0] + p[1] * fwd[1] + p[2] * fwd[2];
        if (t < 0.1f || t > bt) continue;
        float q[3] = {p[0] - fwd[0] * t, p[1] - fwd[1] * t, p[2] - fwd[2] * t};
        if (q[0] * q[0] + q[1] * q[1] + q[2] * q[2] > 0.4f * 0.4f) continue;
        bt = t, best = k;
    }
    if (best < 0) { /* it met nothing (UW.EXE resolve_attack): when the probe (radius r + 1 eighths, r + 3 eighths
                     * ahead; r the weapon's COMOBJ radius) touches a wall, find_wall_coll steps from the player an
                     * eighth at a time, r + 4 steps, and the first point within an eighth of the wall clangs (7,
                     * the spark); then the whoosh (do_miss(0): 10) */
        int ws = wi->radius;
        float hx = fwd[0], hz = fwd[2], hl = sqrtf(hx * hx + hz * hz);
        if (hl > 1e-4f) {
            hx /= hl, hz /= hl;
            float reach = (float)(2 * ws + 4) / 8.0f;
            if (level_solid_at(G.lv, eye[0] + hx * reach, eye[2] + hz * reach))
                for (int s = 0; s < ws + 4; s++) {
                    float d = (float)s / 8.0f, wx = eye[0] + hx * d, wz = eye[2] + hz * d;
                    if (level_solid_at(G.lv, wx + hx * 0.125f, wz + hz * 0.125f)) {
                        sfx_at_(7, wx, wz, 0);
                        break;
                    }
                }
        }
        swing_whoosh(true, NULL);
        return;
    }
    Creature *c = &g_cr[best];
    char name[96];
    creature_name(c, name, sizeof name);
    const char *noun = strchr(name, ' ');
    noun = noun ? noun + 1 : name;
    const ObjInfo *ci = obj_info(0x40 + c->kind);
    int skill = wi->skill >= 3 && wi->skill <= 5 ? wi->skill : 2; /* 3 sword 4 axe 5 mace, else unarmed */
    int acc, dmg_bonus;
    inv_weapon_enchant(&acc, &dmg_bonus); /* the weapon's enchantment (UW.EXE 0x254ed) */
    int score = P.skill[SK_ATTACK] / 2 + P.skill[SK_UNARMED + skill - 2] + P.dex / 7 + (P.easy ? 7 : 0) + acc;
    /* The attack (UW.EXE 0x26fa9, DS 0x235): the original splits its view into 3 x 3 and the row the pointer is in
     * picks it: the top row thrusts, the middle slashes, the bottom bashes. Here the crosshair is always central,
     * so the row is where it met the creature when the swing began (attack_type). */
    float hit_h = eye[1] + fwd[1] * bt; /* where the crosshair meets it */
    int dmg_type = type == 2 ? wi->stab : type == 1 ? wi->bash : wi->slash;
    int base = (skill == 2 ? P.skill[SK_UNARMED] * 2 / 5 + P.str / 6 + 4 : P.str / 9 + dmg_type) + dmg_bonus;
    int part = hit_part(c->y, c->y + CREATURE_HEIGHT, hit_h);
    int player_dir = dir8(fwd[0], fwd[2]), angle = angle_bonus(c->dir, player_dir);
    int r = skill_check(score + angle, ci->defense);
    c->hostile = true;
    blow_sound(r <= 0, true, c, part); /* blocked: the clang (do_miss) */
    if (r <= 0) return;
    if (r == 2) base = base * (48 + rnd(32)) / 32;
    int dmg = roll_damage(base) * power / 128 + angle;
    sfx_at_(4, c->x, c->z, (i8)(u8)(dmg << 2)); /* the creature hurt (0x24d64: at it, the damage before armour x 4) */
    int armour = ci->carmour[part] == 255 ? ci->carmour[0] : ci->carmour[part];
    dmg = MAX(0, dmg - armour);
    c->hp -= (float)dmg;
    logf_("combat: the player hits the %s for %d (check %d, part %d, power %d), %.0f hit points left\n", noun, dmg, r, part, power, c->hp);
    if (c->hp > 0) sound_song_blow(G.clock, true, (int)c->hp, ci->hp); /* its fight's song (UW.EXE 0x1c44d) */
    else sound_song_set_next(9);                                        /* a kill: its song (0x25b56) */
    if (c->hp <= 0) {
        c->state = CS_DYING, c->t = 0, creature_death_sound(c);
        creature_remove(c);
        int v = ci->xp;
        (void)v, kill_exp(c);

    } else {
        c->state = CS_HIT, c->t = 0;

    }
}


/* the creature the view ray passes within reach (living) */
static int creature_at(const float eye[3], const float fwd[3], float reach) {
    int best = -1;
    float bt = reach;
    for (int k = 0; k < g_ncr; k++) {
        Creature *c = &g_cr[k];
        if (c->state >= CS_DYING) continue;
        float p[3] = {c->x - eye[0], c->y + 0.35f - eye[1], c->z - eye[2]};
        float t = p[0] * fwd[0] + p[1] * fwd[1] + p[2] * fwd[2];
        if (t < 0.1f || t > bt) continue;
        float q[3] = {p[0] - fwd[0] * t, p[1] - fwd[1] * t, p[2] - fwd[2] * t};
        if (q[0] * q[0] + q[1] * q[1] + q[2] * q[2] > 0.4f * 0.4f) continue;
        bt = t, best = k;
    }
    return best;
}

/* talk: an NPC with a number (its last mobile byte) has its own conversation, the others the one for their
 * kind (256 + kind) */
static int g_talk_cr = -1; /* the creature spoken to */
/* the conversation run by the rules core (CONVERSE.C on the core's fiber): what it has said (scroll_print to the
 * NPC's scroll), its menu (read from the core), shown on our conversation screen as our own conversations are */
static Conv g_ccv;
static bool g_core_talk;
static int g_ccv_serial = -1;
static void ccv_add(const char *line) {
    if (g_ccv.nlines >= CONV_MAX_LINES) {
        memmove(g_ccv.lines[0], g_ccv.lines[1], sizeof g_ccv.lines[0] * (CONV_MAX_LINES - 1));
        g_ccv.nlines--;
    }
    snprintf(g_ccv.lines[g_ccv.nlines++], sizeof g_ccv.lines[0], "%s", line);
}
static const Conv *talk_state(void) {
    if (!g_core_talk) return conv_state();
    g_ccv.running = cw_conv_active();
    g_ccv.waiting = cw_conv_waiting();
    g_ccv.noptions = cw_conv_options(g_ccv.options, CONV_MAX_OPTIONS);
    if (g_ccv.running) cw_conv_name(g_ccv.who, sizeof g_ccv.who);
    return &g_ccv;
}
static void talk_choose(int i) {
    if (g_core_talk) cw_conv_choose(i);
    else conv_choose(i);
}
/* the core has begun a conversation (the player's talk, a creature's, a trigger's): our screen for it */
static void core_talk_open(void) {
    G.talking = g_core_talk = true;
    sound_song_set_next(13), sound_songs_update(G.clock, G.fight); /* the quiet song (UW.EXE 0x6e68b) */
}
bool game_talking(void) { return G.talking; }
/* the conversation over: the speaker keeps the attitude it ended with (set_attitude), hostile when it is 0 */
static void talk_end(void) {
    if (g_core_talk) { /* the original's conversation ends by itself (the script's end); the screen closes after */
        if (cw_conv_active()) return;
        g_core_talk = G.talking = false;
        sound_song_random(), sound_song_restart();
        return;
    }
    conv_end();
    G.talking = false;
    sound_song_random(), sound_song_restart(); /* a wandering song next (0x6ea81); this one again if it ended (0x6ee17) */
    if (g_talk_cr < 0 || g_talk_cr >= g_ncr) return;
    Creature *c = &g_cr[g_talk_cr];
    int a = conv_attitude() & 3;
    if (c->obj > 0 && c->obj < 256) G.lv->obj[c->obj].mobile[6] = (u8)((G.lv->obj[c->obj].mobile[6] & 0x3f) | a << 6);
    c->hostile = a == 0;
    g_talk_cr = -1;
}
static void talk(int k) {
    Creature *c = &g_cr[k];
    g_talk_cr = k;
    const Object *o = &G.lv->obj[c->obj];
    int whoami = c->obj < 256 ? o->mobile[18] : 0, n = whoami ? whoami : 256 + c->kind;
    char name[96];
    creature_name(c, name, sizeof name);
    const char *noun = strchr(name, ' ');
    if (c->obj > 0 && c->obj < 256) conv_set_attitude(G.lv->obj[c->obj].mobile[6] >> 6);
    if (!conv_start(G.data, n, noun ? noun + 1 : name, player_name())) {
        game_message("The %s does not answer.", noun ? noun + 1 : name);
        return;
    }
    G.talking = true;
    sound_song_set_next(13), sound_songs_update(G.clock, G.fight); /* the quiet song (UW.EXE 0x6e68b) */
}
/* the checks (--converse=N): conversation N, as if its speaker had been spoken to */
void game_converse(int n) {
    if (conv_start(G.data, n, "stranger", "Avatar")) G.talking = true;
}

/* The creatures as the rules core has them (its level block), each drawn as UW.EXE's DRAWOBJ.C chooses the frame:
 * the sequence (byte 0x15, bits 0-5) and the frame in it (the word at 0x0b, bits 12-15); standing and walking
 * (sequence 0x20 and up) by the creature's heading against the viewer's (dirtab, eight views: page 1 from slot
 * 0x20 + ((sequence - 0x20) << 3) + view), the other sequences but dying from the side or behind as the standing
 * view, and the rest (combat, attacks, firing, backing off, dying, casting) from page 0's slot. */
static const u8 DIRTAB[32] = {0, 0, 0, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 0, 0};
static int core_creature_frame(int k, int seq, int fr, int view) {
    const Mesh *m = G.mesh;
    if (!m->crit[k].loaded) return -1;
    int slot1 = -1;
    if (seq >= 0x20) slot1 = ((seq - 0x20) << 3) + view;
    else if (seq != 0xC && ((view + 5) & 7) >= 3) slot1 = view;
    /* the entry as the original reads it (uwview.c's creature_frame: segment * 8 + the frame 0-15, an empty entry the
     * page's frame 0, a missing slot nothing) */
    if (slot1 >= 0) {
        if (slot1 >= 128 || m->crit[k].base1 < 0) return -1;
        int sg = m->crit[k].slots1[slot1];
        if (sg == 255 || sg >= m->crit[k].nsegs1) return -1;
        int idx = sg * 8 + (fr & 15);
        int f = idx < m->crit[k].nsegs1 * 8 ? ((const u8 *)m->crit[k].segs1)[idx] : 255;
        if (f == 255) f = 0;
        return m->crit[k].base1 + f < m->crit[k].nframes ? m->crit[k].base1 + f : -1;
    }
    int sg = seq < 32 ? m->crit[k].slots[seq] : 255;
    if (sg == 255 || sg >= m->crit[k].nsegs) return -1;
    int idx = sg * 8 + (fr & 15);
    int f = idx < m->crit[k].nsegs * 8 ? ((const u8 *)m->crit[k].segs)[idx] : 255;
    if (f == 255) f = 0;
    return f >= m->crit[k].nframes ? -1 : f;
}
static int core_creature_cards(Vertex *out, int cap) {
    const u8 *b = cw_block();
    if (getenv("UW_CRIT_DEBUG") && *getenv("UW_CRIT_DEBUG")) fprintf(stderr, "core cards: block %p\n", (const void *)b);
    if (!b) return 0;
    unsigned vh = (unsigned)(int)(G.yaw * (65536.0f / 6.2831853f)) & 0xffff;
    int n = 0;
    for (int t = 0; t < 4096; t++)
        for (int i = rd16(b + t * 4 + 2) >> 6, guard = 0; i > 0 && guard < 1024; guard++) {
            size_t off = 0x4000 + (i < 256 ? (size_t)i * 27 : 256 * 27 + (size_t)(i - 256) * 8);
            int next = rd16(b + off + 4) >> 6, item = rd16(b + off) & 511;
            if (i > 1 && i < 256 && item >= 0x40 && item < 0x80 && n + 6 <= cap) { /* (1 is the player) */
                u32 pw = rd16(b + off + 2), home = rd16(b + off + 0x16);
                int seq = b[off + 0x15] & 0x3f, fr = rd16(b + off + 0x0b) >> 12, heading = (pw >> 7) & 7;
                int view = DIRTAB[(unsigned)((heading << 2) + 0x20 - (vh >> 11)) % 0x20];
                int f = core_creature_frame(item - 0x40, seq, fr, view);
                if (getenv("UW_CRIT_DEBUG") && *getenv("UW_CRIT_DEBUG")) fprintf(stderr, "crit %d item 0x%x seq 0x%x frame %d view %d -> %d (loaded %d base1 %d slots1 %d nsegs1 %d)\n", i, item, seq, fr, view, f, G.mesh->crit[item - 0x40].loaded, G.mesh->crit[item - 0x40].base1, G.mesh->crit[item - 0x40].slots1[view], G.mesh->crit[item - 0x40].nsegs1);
                if (f >= 0) {
                    Object o = {0};
                    o.item = (u16)item, o.z = (u8)(pw & 127), o.heading = (u8)heading, o.y = (u8)((pw >> 10) & 7), o.x = (u8)(pw >> 13);
                    float p[3];
                    world_object_pos(G.lv, home >> 10, (home >> 4) & 63, &o, p);
                    n += world_creature_card(G.mesh, item - 0x40, f, p, out + n);
                }
            }
            i = next;
        }
    return n;
}

static int creature_cards(Vertex *out, int cap) {
    if (cw_running()) return core_creature_cards(out, cap); /* the original's creatures (the rules core) */
    int n = 0;
    for (int k = 0; k < g_ncr && n + 6 <= cap; k++) {
        const Creature *c = &g_cr[k];
        int fr = creature_frame(c);
        if (fr < 0) continue;
        float pos[3] = {c->x, c->y, c->z};
        n += world_creature_card(G.mesh, c->kind, fr, pos, out + n);
    }
    return n;
}

/* --- move triggers ---------------------------------------------------------------------------------- */

/* the game's variables (UW.EXE: the player record's quest bits at 0x65 (32) and variables from 0x70 (6 bits each)) */
static u32 g_quest;
static u8 g_var[128];
static bool g_rebuild; /* the level's tiles changed: its geometry is to be made again */
u32 game_quests(void) { return g_quest; } /* the 32 quest bits (conversations read them) */
bool game_rebuild_wanted(void) {
    bool r = g_rebuild;
    g_rebuild = false;
    return r;
}
static void run_trap(int i, int tx, int tz, int depth);
static void fire_trigger(int t);
/* what a trap's chain goes on to (0x84803): the object its link names when that is a trap (it runs for the same
 * tile) or a trigger (it fires) */
static void chain_on(int link, int tx, int tz, int depth) {
    if (link <= 0 || link >= 1024 || depth > 16) return;
    int item = G.lv->obj[link].item;
    if (item >= 0x180 && item < 0x1A0) run_trap(link, tx, tz, depth + 1);
    else if (item >= 0x1A0 && item < 0x1C0) fire_trigger(link);
}
/* the value a variable trap carries: ((quality << 5 | owner) << 3) | y */
static int trap_value(const Object *o) { return ((o->quality << 5 | o->owner) << 3) | (o->y & 7); }

/* the door on tile (tx, tz), or -1 */
static int door_on(int tx, int tz) {
    for (int k = 0; k < G.ndoors; k++)
        if (G.doors[k].tx == tx && G.doors[k].tz == tz) return k;
    return -1;
}

/* A trap set off, for the target tile (tx, tz) its trigger names (UW.EXE's dispatcher, file 0x83d96, by item &
 * 0x3f through a table of 17). Done: damage, teleport, door, text string; the rest are logged. */
static void run_trap(int i, int tx, int tz, int depth) {
    if (i <= 0 || i >= 1024 || depth > 16) return;
    Object *o = &G.lv->obj[i];
    bool chain = true; /* the door, create and delete traps use their link themselves */
    switch (o->item) {
    case 0x18d: { /* set variable (0x83ea4): variable z; 0 = the quest bits: heading 1 clears, 5 toggles, else sets
                   * bit value; else variable op by heading: 0 add 1 sub 2 set 3 and 4 or 5 xor 6 shift left; 6 bits */
        int v = trap_value(o), var = o->z & 0x7f, op = o->heading & 7;
        if (!var) {
            u32 bit = v < 32 ? 1u << v : 0;
            if (op == 1) g_quest &= ~bit;
            else if (op == 5) g_quest ^= bit;
            else g_quest |= bit;
        } else {
            u8 *x = &g_var[var & 127];
            switch (op) {
            case 0: *x = (u8)(*x + v); break;
            case 1: *x = (u8)(*x - v); break;
            case 2: *x = (u8)v; break;
            case 3: *x &= (u8)v; break;
            case 4: *x |= (u8)v; break;
            case 5: *x ^= (u8)v; break;
            case 6: *x = (u8)(*x << (v & 7)); break;
            }
            *x &= 0x3f;
        }
        break;
    }
    case 0x18e: { /* check variable (0x83fc5): variables z .. z + heading, summed (x set) or packed 3 bits each;
                   * equal to the value: the chain goes on to the link; else to what follows the link */
        int first = o->z & 0x7f, last = first + (o->heading & 7), acc = 0;
        for (int k = first; k <= last && k < 128; k++) acc = o->x ? acc + g_var[k] : (acc << 3) | (g_var[k] & 7);
        if (acc != trap_value(o)) {
            chain = false;
            if (o->link > 0 && o->link < 1024) chain_on(G.lv->obj[o->link].next, tx, tz, depth);
        }
        break;
    }
    case 0x18c: { /* inventory (0x8463b): the chain goes on only when the player carries object (quality << 5 | owner),
                   * at least z of them when z is set */
        int item = o->quality << 5 | o->owner;
        if (inv_count(item) <= 0 || ((o->z & 0x7f) && inv_count(item) < (o->z & 0x7f))) return;
        break;
    }
    case 0x185: { /* change terrain (0x83e17, 0x73b68): the tiles from the target, x + 1 wide and y + 1 deep: floor
                   * height z / 8 (if <= 13; objects lifted with it), floor texture quality / 2 (if < 11), wall texture
                   * owner (if < 48), type heading x 2 + quality & 1 (if < 10; 15 -> 10) */
        int h = (o->z & 0x7f) >> 3, ftex = o->quality >> 1, wtex = o->owner, type = (o->heading & 7) * 2 + (o->quality & 1);
        if (type == 15) type = 10;
        for (int zz = tz; zz <= tz + (o->y & 7) && zz < 64; zz++)
            for (int xx = tx; xx <= tx + (o->x & 7) && xx < 64; xx++) {
                Tile *t = &G.lv->tile[zz * 64 + xx];
                if (h <= 13) {
                    for (int j = t->first_object, guard = 0; j > 0 && j < 1024 && guard < 1024; j = G.lv->obj[j].next, guard++)
                        if (G.lv->obj[j].z < h * 8) G.lv->obj[j].z = (u8)(h * 8);
                    t->height = (u8)h;
                }
                if (ftex < 11) t->floor = (u8)ftex;
                if (wtex < 48) t->wall = (u8)wtex;
                if (type < 10) t->type = (u8)type;
            }
        g_rebuild = true;
        logf_("change terrain trap %d: tiles from %d,%d (%dx%d)\n", i, tx, tz, (o->x & 7) + 1, (o->y & 7) + 1);
        break;
    }
    case 0x188: { /* a door trap (file 0x843dd): the door on the target tile loses its lock; a copy of the lock
                   * the trap links to (if any) goes on instead; then quality 1 opens, 2 closes, 3 toggles */
        int k = door_on(tx, tz);
        if (k < 0) break;
        Object *door = &G.lv->obj[G.doors[k].obj];
        if (door->link > 0 && door->link < 1024 && G.lv->obj[door->link].item == 0x10F) {
            int old = door->link;
            door->link = G.lv->obj[old].next; /* the lock out of the door's chain */
            memset(&G.lv->obj[old], 0, sizeof G.lv->obj[old]);
        }
        if (!(o->flags & 0x40) && o->link > 0 && o->link < 1024 && G.lv->obj[o->link].item == 0x10F) {
            int n = inv_level_alloc(G.lv);
            if (n > 0) {
                G.lv->obj[n] = G.lv->obj[o->link];
                G.lv->obj[n].next = door->link;
                door->link = (u16)n;
            }
        }
        float target = o->quality == 1 ? 1.0f : o->quality == 2 ? 0.0f : o->quality == 3 ? (G.door_target[k] < 0.5f ? 1.0f : 0.0f) : G.door_target[k];
        bool moved = (target > 0.5f) != (G.door_target[k] > 0.5f);
        G.door_target[k] = target;
        Door *d = &G.doors[k];
        if (moved) door_sound(d); /* (0x387c6 / 0x388dd: only a door that moves) */
        d->item = (d->item & ~8) | (target > 0.5f ? 8 : 0);
        door->item = (u16)d->item;
        logf_("door trap %d: the door at %d,%d %s\n", i, tx, tz, target > 0.5f ? "opens" : "closes");
        chain = false;
        break;
    }
    case 0x187: { /* a create object trap (file 0x8418f): a roll of 0-62 below the quality does nothing; else a
                   * copy of the object the trap links to (a creature with its mobile bytes) goes on the target
                   * tile, at the template's place in the tile and its height */
        chain = false;
        if (rnd(63) < o->quality || (o->flags & 0x40) || o->link <= 0 || o->link >= 1024) break;
        const Object *tpl = &G.lv->obj[o->link];
        bool creature = tpl->item >= 0x40 && tpl->item < 0x80;
        int n = -1;
        if (creature) { /* a mobile slot (1-255) nothing uses */
            for (int k = 1; k < 256 && n < 0; k++)
                if (inv_level_free(G.lv, k) && G.lv->obj[k].item == 0) n = k;
        } else n = inv_level_alloc(G.lv);
        if (n <= 0 || tx < 0 || tx > 63 || tz < 0 || tz > 63) break;
        G.lv->obj[n] = *tpl;
        G.lv->obj[n].link = 0;
        Tile *t = &G.lv->tile[tz * 64 + tx];
        G.lv->obj[n].next = t->first_object;
        t->first_object = (u16)n;
        if (creature) creature_add(n, tz * 64 + tx);
        rebuild_sprites();
        logf_("create object trap %d: object %d (0x%03x) on %d,%d\n", i, n, tpl->item, tx, tz);
        chain = false;
        break;
    }
    case 0x18b: { /* a delete object trap (file 0x845dc): the object it links to leaves the tile the trap's own
                   * quality and owner name */
        chain = false;
        int dx = o->quality, dz = o->owner, v = o->link;
        if (dx > 63 || dz > 63 || v <= 0 || v >= 1024) break;
        Tile *t = &G.lv->tile[dz * 64 + dx];
        u16 *at = &t->first_object;
        for (int guard = 0; *at && guard < 1024; guard++, at = &G.lv->obj[*at].next)
            if (*at == v) {
                *at = G.lv->obj[v].next;
                for (int k = 0; k < g_ncr; k++)
                    if (g_cr[k].obj == v) g_cr[k] = g_cr[--g_ncr];
                memset(&G.lv->obj[v], 0, sizeof G.lv->obj[v]);
                rebuild_sprites();
                logf_("delete object trap %d: object %d off %d,%d\n", i, v, dx, dz);
                break;
            }
        break;
    }
    case 0x190: /* a text string trap (file 0x847bb): strings block 9 [(quality << 5) | owner] */
        game_message("%s", str_get(9, (o->quality << 5) | o->owner));
        break;
    case 0x182: { /* an arrow trap (0x83dc3 -> 0xe0:0x73c): a missile of kind (quality << 5) | owner from the trap at
                    * the player (our aim) */
        int kind = (o->quality << 5) | o->owner;
        if (kind < 0x10 || kind > 0x17) kind = 0x12;
        float from[3] = {(float)tx + (o->x + 0.5f) / 8.0f, level_floor_y(G.lv, (float)tx + 0.5f, (float)tz + 0.5f) + (o->z & 0x7f) / 32.0f,
                         (float)tz + (o->y + 0.5f) / 8.0f};
        float d[3] = {G.eye[0] - from[0], G.eye[1] - 0.2f - from[1], G.eye[2] - from[2]};
        float len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (len > 0.01f) {
            for (int k = 0; k < 3; k++) d[k] /= len;
            missile_fire(kind, LAUNCH[kind - 0x10][0], from, d, 0x120, false);
        }
        break;
    }
    case 0x181: /* a teleport trap: the tile (quality, owner), on level z when that is not 0 */
        G.travel = true, G.travel_level = o->z ? o->z : G.lv->number;
        G.travel_x = o->quality + 0.5f, G.travel_z = o->owner + 0.5f;
        break;
    case 0x180: /* a damage trap (0x840bd): quality hit points; with an owner set it heals them instead */
        if (o->owner) G.hp = MIN(G.hp_max, G.hp + o->quality);
        else {
            G.hp -= o->quality, game_message("Ouch!");
            if (G.hp < 1) G.hp = 1;
        }
        break;
    default: logf_("trap 0x%03x (object %d) not done yet\n", o->item, i); break;
    }
    if (chain) chain_on(o->link, tx, tz, depth);
}

/* a trigger (0x1A0 move, 0x1A2 use, ...): its quality and owner name the target tile; its link the trap */
static void fire_trigger(int t);
static void fire_trigger(int t) {
    if (t <= 0 || t >= 1024) return;
    const Object *tr = &G.lv->obj[t];
    if (tr->item < 0x1A0 || tr->item > 0x1AF) return run_trap(t, tr->quality, tr->owner, 0); /* a trap linked directly */
    run_trap(tr->link, tr->quality, tr->owner, 0);
}


/* Triggers on events (UW.EXE 0x83b3b). Each trigger kind answers one event (OBJECTS.DAT 0xd92: 0x1A0 move 0,
 * 0x1A1 pick up 2, 0x1A2 use 4, 0x1A3 look 5, 0x1A4 step on 1, 0x1A5 open 7, 0x1A6 unlock 6). The player sets it
 * off only when its word-0 bit 11 (our flags bit 2) is set; a look trigger with a z needs a search check against
 * z. It runs the trap it links to, for the tile its quality and owner name. */
enum { EV_MOVE = 0, EV_STEP = 1, EV_PICKUP = 2, EV_USE = 4, EV_LOOK = 5, EV_UNLOCK = 6, EV_OPEN = 7 };
static const u8 TRIGGER_EVENT[16] = {0, 2, 4, 5, 1, 7, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int skill_check(int skill, int difficulty);
static int search_skill(void);
static bool trigger_event(int t, int event) {
    if (t <= 0 || t >= 1024) return false;
    const Object *tr = &G.lv->obj[t];
    if (tr->item < 0x1A0 || tr->item > 0x1AF || TRIGGER_EVENT[tr->item & 15] != event || !(tr->flags & 0x04)) return false;
    if (event == EV_LOOK && (tr->z & 0x7f) && skill_check(search_skill(), tr->z & 0x7f) <= 0) return false;
    logf_("trigger %d (0x%03x) fires: trap %d for %d,%d\n", t, tr->item, tr->link, tr->quality, tr->owner);
    run_trap(tr->link, tr->quality, tr->owner, 0);
    return true;
}

static void enter_tile(int tx, int tz) {
    for (int i = G.lv->tile[tz * 64 + tx].first_object, guard = 0; i > 0 && i < 1024 && guard < 1024; i = G.lv->obj[i].next, guard++)
        if (G.lv->obj[i].item == 0x1A0) run_trap(G.lv->obj[i].link, G.lv->obj[i].quality, G.lv->obj[i].owner, 0);
}

/* a switch, lever, button or pull chain used: it flips (0x170-0x177 <-> 0x178-0x17F) and fires what it links */
static void use_switch(int i) {
    Object *o = &G.lv->obj[i];
    o->item ^= 8;
    /* its click: effect 0x13 at its tile + 3/8 (UW.EXE UseRect, class 3) */
    for (int t = 0; t < 64 * 64; t++)
        for (int j = G.lv->tile[t].first_object, guard = 0; j > 0 && j < 1024 && guard < 1024; j = G.lv->obj[j].next, guard++)
            if (j == i) {
                sfx_at_(0x13, (float)(t % 64) + 0.375f, (float)(t / 64) + 0.375f, 0);
                t = 64 * 64, guard = 1024;
                break;
            }
    fire_trigger(o->link);
}

bool game_travel(int *level, float *x, float *z) {
    if (!G.travel) return false;
    G.travel = false;
    *level = G.travel_level, *x = G.travel_x, *z = G.travel_z;
    return true;
}

/* --- the world's objects -------------------------------------------------------------------------- */
/* the item the view ray meets first within reach (a card it passes through), with nothing solid between */
static void pick(const float eye[3], const float fwd[3]) {
    G.hover_obj = -1;
    float best = spell_has(11, 3) ? 8.0f : REACH; /* telekinesis: no reach limit (DS 0x286 = 0; 8 tiles here) */
    int ex = (int)eye[0], ez = (int)eye[2], w = (int)best + 1;
    for (int tz = MAX(0, ez - w); tz <= MIN(63, ez + w); tz++)
        for (int tx = MAX(0, ex - w); tx <= MIN(63, ex + w); tx++)
            for (int i = G.lv->tile[tz * 64 + tx].first_object, guard = 0; i > 0 && i < 1024 && guard < 1024; i = G.lv->obj[i].next, guard++) {
                const Object *o = &G.lv->obj[i];
                int decal = world_decal_cell(o);
                float p[3], r;
                if (decal >= 0) { /* a wall decoration: round its middle on the wall */
                    float nrm[3];
                    world_decal_place(G.lv, tx, tz, o, p, nrm);
                    r = 0.16f;
                } else {
                    if (!world_item_shown(o) || o->item >= MAX_SPRITES || !G.mesh->sprite_w[o->item]) continue;
                    world_object_pos(G.lv, tx, tz, o, p);
                    r = MAX(G.mesh->sprite_w[o->item], G.mesh->sprite_h[o->item]) * SPRITE_SCALE * 0.5f + 0.06f;
                    p[1] += G.mesh->sprite_h[o->item] * SPRITE_SCALE * 0.5f;
                }
                float d[3] = {p[0] - eye[0], p[1] - eye[1], p[2] - eye[2]};
                float t = d[0] * fwd[0] + d[1] * fwd[1] + d[2] * fwd[2];
                if (t < 0.05f || t > best) continue;
                float q[3] = {d[0] - fwd[0] * t, d[1] - fwd[1] * t, d[2] - fwd[2] * t};
                if (q[0] * q[0] + q[1] * q[1] + q[2] * q[2] > r * r) continue;
                bool clear = true;
                for (float s = 0.1f; s < t - 0.1f && clear; s += 0.1f) clear = !level_solid_at(G.lv, eye[0] + fwd[0] * s, eye[2] + fwd[2] * s);
                if (!clear) continue;
                best = t, G.hover_obj = i, G.hover_tx = tx, G.hover_tz = tz;
            }
    if (getenv("UW_CRIT_DEBUG") && *getenv("UW_CRIT_DEBUG")) {
        int n = 0;
        for (int t2 = 0; t2 < 4096; t2++) if (G.lv->tile[t2].first_object) n++;
        fprintf(stderr, "pick: eye %.2f %.2f %.2f fwd %.2f %.2f %.2f, tiles with objects %d, hover %d\n", eye[0], eye[1], eye[2], fwd[0], fwd[1], fwd[2], n, G.hover_obj);
    }
}

bool game_light(float *intensity, float *range, float color[3]) {
    if (cw_running()) {
        /* the original's light level (the player record's, set by PLAYDATA.C's FixPlayerEquips from the lights carried
         * and the light spells: 0 dark to 7, SHADES.DAT's rows): ours carries it as our torch's reach and strength,
         * scaled so that level 4 is the settings' torch */
        int level = cw_light_level();
        if (level <= 0) { /* the darkest level still shows what is next to the player (SHADES.DAT's row 0) */
            *range = 1.2f, *intensity = 0.12f;
            return true;
        }
        float lit_in, lit_range, lit_col[3];
        bool item = inv_light(&lit_in, &lit_range, lit_col);
        *range = item ? lit_range * (float)level / 4.0f : 1.5f * (float)level;
        *intensity = item ? lit_in * (0.5f + 0.125f * (float)level) : 0.25f * (float)level;
        if (item) color[0] = lit_col[0], color[1] = lit_col[1], color[2] = lit_col[2];
        return true;
    }
    return inv_light(intensity, range, color);
}

/* --- Look ---------------------------------------------------------------------------------------------------
 * What the crosshair is on, put into words as the original does: a creature, a door, an object (its quality
 * and owner, a writing's text), else the wall, floor or ceiling the view ray meets (strings block 10). */
static void look_world(const float eye[3], const float fwd[3]) {
    char line[400];
    if (cw_running() && G.hover_creature >= 0) { /* the original's look at a creature (LookAt, CritterLook) */
        const Creature *c = &g_cr[G.hover_creature];
        cw_look_world(c->obj, c->tile);
        return;
    }
    if (cw_running() && (G.hover_obj >= 0 || G.hover_door >= 0)) { /* the original's look (player_3dlook, LookAt) */
        if (G.hover_obj >= 0) cw_look_world(G.hover_obj, G.hover_tz * 64 + G.hover_tx);
        else cw_look_world(G.doors[G.hover_door].obj, G.doors[G.hover_door].tz * 64 + G.doors[G.hover_door].tx);
        return;
    }
    if (G.hover_creature >= 0) {
        char name[96];
        item_name(&G.lv->obj[g_cr[G.hover_creature].obj], name, sizeof name);
        return game_message("You see %s.", name);
    }
    if (G.hover_door >= 0) {
        const Door *d = &G.doors[G.hover_door];
        if ((d->item & 7) != 7 || d->open > 0.5f) return game_message("You see %s.", d->open > 0.5f ? "an open door" : "a door");
    }
    if (G.hover_obj >= 0) {
        obj_describe(&G.lv->obj[G.hover_obj], line, sizeof line);
        game_message("%s", line);
        trigger_event(G.lv->obj[G.hover_obj].link, EV_LOOK);
        return;
    }
    /* the ray through the level: the first wall, floor or ceiling, up to 8 tiles away */
    int last = -1;
    for (float t = 0.02f; t < 8.0f; t += 0.02f) {
        float x = eye[0] + fwd[0] * t, y = eye[1] + fwd[1] * t, z = eye[2] + fwd[2] * t;
        if (x < 0 || z < 0 || x >= 64 || z >= 64) break;
        int ti = (int)z * 64 + (int)x;
        if (level_solid_at(G.lv, x, z)) {
            const Tile *o = last >= 0 ? &G.lv->tile[last] : &G.lv->tile[ti];
            wall_describe(G.lv->walls[o->wall % 48], line, sizeof line);
            game_message("%s", line);
            /* special tmap objects (0x16E) on this wall of the open tile: their look triggers (hidden things) */
            if (last >= 0) {
                int lx = last % 64, lz = last / 64;
                for (int j = G.lv->tile[last].first_object, guard = 0; j > 0 && j < 1024 && guard < 1024; j = G.lv->obj[j].next, guard++) {
                    const Object *d = &G.lv->obj[j];
                    if (d->item != 0x16E && d->item != 0x16F) continue;
                    bool on = (d->x == 0 && x < lx) || (d->x == 7 && x >= lx + 1) || (d->y == 0 && z < lz) || (d->y == 7 && z >= lz + 1);
                    if (on) trigger_event(d->link, EV_LOOK);
                }
            }
            return;
        }
        last = ti;
        if (y < level_floor_y(G.lv, x, z)) {
            floor_describe(G.lv->floors[G.lv->tile[ti].floor % 10], line, sizeof line);
            return game_message("%s", line);
        }
        if (y > level_ceil_y()) {
            floor_describe(G.lv->floors[9], line, sizeof line);
            return game_message("%s", line);
        }
    }
    game_message("You see nothing of note.");
}

/* what the ray from eye along dir meets within reach: an item or decoration, a door, a creature */
static void hover_world(const float eye[3], const float dir[3]) {
    pick(eye, dir);
    float best = REACH;
    if (G.hover_obj >= 0) {
        float p[3];
        world_object_pos(G.lv, G.hover_tx, G.hover_tz, &G.lv->obj[G.hover_obj], p);
        best = sqrtf((p[0] - eye[0]) * (p[0] - eye[0]) + (p[2] - eye[2]) * (p[2] - eye[2]));
    }
    pick_door(eye, dir, &best);
    if (G.hover_door >= 0) G.hover_obj = -1;
    G.hover_creature = creature_at(eye, dir, MIN(best + 0.05f, REACH));
    if (G.hover_creature >= 0) G.hover_obj = -1, G.hover_door = -1;
}

/* E or a right click on what is hovered: talk, flip a switch, take, look at what cannot be taken, open a door */
static void use_hovered(const float eye[3], const float dir[3]) {
    if (cw_running() && getenv("UW_CRIT_DEBUG") && *getenv("UW_CRIT_DEBUG"))
        fprintf(stderr, "core use: object %d (item 0x%x) on %d,%d, door %d\n", G.hover_obj, G.hover_obj >= 0 ? G.lv->obj[G.hover_obj].item : 0, G.hover_tx, G.hover_tz, G.hover_door);
    if (cw_running() && (G.hover_obj >= 0 || G.hover_door >= 0)) { /* the original's take (player_3dget) or use (UseObj) */
        if (G.hover_obj >= 0) {
            if (obj_info(G.lv->obj[G.hover_obj].item)->pick) inv_take(G.lv, G.hover_tx, G.hover_tz, G.hover_obj);
            else cw_use_world(G.hover_obj, G.hover_tz * 64 + G.hover_tx);
        } else cw_use_world(G.doors[G.hover_door].obj, G.doors[G.hover_door].tz * 64 + G.doors[G.hover_door].tx);
        return;
    }
    if (G.hover_creature >= 0 && cw_running() && getenv("UW_CORE_TALK")) { /* the original's talk (player_3dtalk: TalkTo decides) */
        const Creature *c = &g_cr[G.hover_creature];
        cw_talk_world(c->obj, c->tile);
        if (cw_conv_active()) core_talk_open();
        return;
    }
    if (G.hover_creature >= 0) {
        if (!g_cr[G.hover_creature].hostile) talk(G.hover_creature);
        else game_message("It is in no mood to talk.");
        return;
    }
    if (G.hover_obj >= 0) {
        int it = G.lv->obj[G.hover_obj].item;
        if (it >= 0x170 && it <= 0x17F) {
            use_switch(G.hover_obj);
            rebuild_sprites();
        } else if (it == 0x157) { /* a shrine: a mantra is chanted (typed) */
            G.chanting = true, G.mantra[0] = 0;
        } else if (it == 0x1ca) { /* the silver tree withers back into its seed (string 9) */
            int t = G.hover_tz * 64 + G.hover_tx;
            u16 *at = &G.lv->tile[t].first_object;
            for (int guard = 0; *at && guard < 1024; guard++, at = &G.lv->obj[*at].next)
                if (*at == G.hover_obj) {
                    *at = G.lv->obj[G.hover_obj].next;
                    memset(&G.lv->obj[G.hover_obj], 0, sizeof G.lv->obj[0]);
                    break;
                }
            inv_give(0x122, 63);
            G.silver_level = 0;
            game_message("%s", s1(9));
            rebuild_sprites();
        } else if (obj_info(it)->pick) {
            int link = G.lv->obj[G.hover_obj].link; /* a pick-up trigger goes off as it is taken */
            if (inv_take(G.lv, G.hover_tx, G.hover_tz, G.hover_obj)) {
                trigger_event(link, EV_PICKUP);
                rebuild_sprites();
            }
        } else {
            int link = G.lv->obj[G.hover_obj].link;
            if (!trigger_event(link, EV_USE) && !trigger_event(link, EV_OPEN)) look_world(eye, dir);
        }
    } else if (G.hover_door >= 0) use_door(G.hover_door);
}

/* Esc: one step back out of what is open (reading, a conversation, a thing on the pointer, the inventory);
 * false when nothing was (then the options open) */
bool game_escape(void) {
    if (G.runes_open) return G.runes_open = false, true;
    if (G.chanting) return G.chanting = false, true;
    if (G.playing) return G.playing = false, game_message("%s", s1(251)), true;
    if (G.sheet) return G.sheet = false, true;
    if (G.reading) return G.reading = false, true;
    if (G.talking) return talk_end(), true;
    if (G.open) {
        if (inv_holding()) return inv_put_back(), true;
        return G.open = false, true;
    }
    return false;
}

/* The world from the rules core: our level's tiles and objects are its block's whenever the block changes (things
 * taken, dropped, moved, made, destroyed; doors opened and shut: their items 0x140-0x147 shut, 0x148-0x14F open); a
 * change to a tile's shape asks for the geometry again */
static u8 g_core_last[0x7c08];
static void core_world_sync(void) {
    const u8 *b = cw_block();
    if (!b || !G.lv || !memcmp(b, g_core_last, sizeof g_core_last)) return;
    bool shape = false;
    for (int t = 0; t < 4096 && !shape; t++)
        shape = rd16(b + t * 4) != rd16(g_core_last + t * 4) || (rd16(b + t * 4 + 2) & 63) != (rd16(g_core_last + t * 4 + 2) & 63);
    bool first = !g_core_last[0] && !g_core_last[1] && !g_core_last[4];
    memcpy(g_core_last, b, sizeof g_core_last);
    level_from_block(G.lv, b);
    for (int k = 0; k < G.ndoors; k++) {
        int item = G.lv->obj[G.doors[k].obj].item;
        if (item >= 0x140 && item <= 0x14F) G.doors[k].item = item, G.door_target[k] = item >= 0x148 ? 1.0f : 0.0f;
    }
    if (shape && !first) g_rebuild = true;
    rebuild_sprites();
}

bool game_update(float dt, const float eye[3], float yaw, const float fwd[3], const GameInput *in) {
    if (CG.on) return chargen_update(in);
    for (int i = 0; i < 4; i++) G.msg_age[i] += dt;
    G.anim += dt;
    if (!G.lv) return G.open;
    inv_tick(dt);
    if (G.reading) { /* reading: a click or E turns the page, past the last one closes; Esc closes */
        G.mouse_x = in->mouse_x, G.mouse_y = in->mouse_y;
        int x, y, w, h;
        read_rect(&x, &y, &w, &h);
        int total = ui_text_wrap(FONT_NORMAL, G.read_text, 0, 0, w - 16, 0, 0, 0, false), page = h - 34;
        if (in->escape) G.reading = false;
        else if (in->click_left || in->use || in->click_right) {
            if (G.read_scroll + page < total) G.read_scroll += page - ui_font_height(FONT_NORMAL) - 2;
            else G.reading = false;
        }
        return true;
    }
    if (G.talking) { /* a conversation: replies by number or click, Esc / E leaves when it is over */
        const Conv *cv = talk_state();
        G.mouse_x = in->mouse_x, G.mouse_y = in->mouse_y;
        int pick = in->number - 1;
        if (in->click_left)
            for (int i = 0; i < cv->noptions; i++)
                if (in->mouse_y >= G.option_y[i] && in->mouse_y < G.option_y[i] + G.option_h[i]) pick = i;
        if (cv->waiting && pick >= 0 && pick < cv->noptions) talk_choose(pick);
        else if (g_core_talk && cv->running && (in->click_left || in->use || in->number > 0)) cw_conv_skip(); /* a pause */
        else if (in->escape || (!cv->running && (in->use || in->click_left))) talk_end();
        return true;
    }
    if (in->inventory) G.open = !G.open;
    creatures_update(dt, eye, yaw);
    /* at nothing, the player wakes at the entrance of the Abyss (the original's death: to decode) */
    if (G.dead) { /* the game is over: load the last save (1) or begin anew (2) */
        G.mouse_x = in->mouse_x, G.mouse_y = in->mouse_y;
        int pick = in->number;
        if (in->click_left || in->attack)
            for (int k = 0; k < 2; k++)
                if (in->mouse_y >= G.option_y[k] && in->mouse_y < G.option_y[k] + G.option_h[k]) pick = k + 1;
        if (pick == 1 || pick == 2) G.death_choice = pick;
        return true;
    }
    if (G.hp <= 0) player_dies();
    if (in->torch) inv_toggle_light();
    if (in->character) G.sheet = !G.sheet;
    {
        u32 was = G.clock;
        G.clock += (u32)(dt * 256.0f);
        if (G.clock != was) sound_songs_update(G.clock, G.fight); /* the music's turn (UW.EXE 0x265c7) */
    }
    G.yaw = yaw;
    { /* what the feet are in: water (the swim's tiring starts, 0x1c5f3) or lava (it burns: our estimate) */
        bool down = G.lv && eye[1] - level_floor_y(G.lv, eye[0], eye[2]) < EYE_HEIGHT + 0.08f;
        int under = down ? game_terrain(eye[0], eye[2]) : 0;
        if (under == 1 && G.swim == 0) G.swim = 96;
        else if (under != 1) G.swim = 0;
        /* The player's sounds (UW.EXE 0x312d3, make_noise): swimming, effect 0, begun again once 0x1800 clock ticks
         * have passed (24 s, its 25 s length); out of the water it stops, and on the ground (not in the air, not
         * levitating or flying) the footsteps: effect 1 panned 0x48 and 2 at 0x38 by turns, at volume speed / 32 - 16,
         * the next one 6000 / (speed / 4 + 1) + 64 ticks later (at most 200). The speed is the original's PN.speed
         * (full run 0x3AC); until the movement's port it is ours measured and scaled to it (our run: 4.5 tiles a
         * second). */
        float mdx = eye[0] - G.step_x, mdz = eye[2] - G.step_z;
        G.step_x = eye[0], G.step_z = eye[2];
        int pspeed = dt > 0 ? (int)(sqrtf(mdx * mdx + mdz * mdz) / dt / 4.5f * 0x3AC) : 0;
        if (pspeed > 0x3AC || pspeed < 0) pspeed = mdx * mdx + mdz * mdz > 1.0f ? 0 : 0x3AC; /* a teleport: no step */
        if (cw_running()) {
            /* the rules core plays the player's sounds (PLAYMOVE.C's make_noise and set_sound) */
        } else if (under == 1 && !(game_motion() & 8)) {
            if (G.swim_sound && G.swim_start + 0x1800 <= G.clock) sound_effect_stop(G.swim_slot), G.swim_sound = false;
            if (!G.swim_sound) {
                G.swim_start = G.clock;
                G.swim_slot = sound_effect(0, 0x40, 0);
                G.swim_sound = G.swim_slot != 0xff;
            }
        } else {
            if (G.swim_sound) sound_effect_stop(G.swim_slot), G.swim_sound = false;
            if (down && !(game_motion() & 20) && pspeed != 0 && G.clock > G.next_step) {
                sfx_play_(G.step_foot ? 1 : 2, G.step_foot ? 0x48 : 0x38, (pspeed >> 5) - 0x10);
                G.step_foot = !G.step_foot;
                u32 delay = 6000u / (u32)((pspeed >> 2) + 1) + 0x40;
                G.next_step = G.clock + MIN(delay, 200u);
            }
        }
        if (under == 2) {
            G.lava_t += dt;
            if (G.lava_t >= 0.5f) G.lava_t -= 0.5f, G.hp -= (float)resisted(player_resist(), dice(1, 4), 8); /* flameproof: none; else 1d4 a half second (estimate) */
        } else G.lava_t = 0;
    }
    if (G.chanting) { /* the mantra typed at a shrine, Enter chants it, Esc leaves */
        size_t len = strlen(G.mantra);
        if (in->escape) G.chanting = false;
        else if (in->text == 8 && len) G.mantra[len - 1] = 0;
        else if (in->text == 13) G.chanting = false, chant(G.mantra);
        else if (in->text >= 32 && in->text < 127 && len < sizeof G.mantra - 1) G.mantra[len] = (char)toupper(in->text), G.mantra[len + 1] = 0;
        return true;
    }
    if (G.playing) { /* the instrument: 0-9 play (the sound effects are to come), Esc puts it down */
        if (in->escape) G.playing = false, game_message("%s", s1(251));
        return true;
    }
    memcpy(G.eye, eye, sizeof G.eye);
    G.dark = MAX(0.0f, G.dark - dt);
    if (in->sleep) game_sleep(1);
    if (in->track) track(8, game_skill(12));
    if (in->runes) G.runes_open = !G.runes_open;
    if (in->cast) cast_shelf();
    if (G.runes_open) { /* the rune panel: a click on a rune puts it on the shelf (three at most), on the shelf takes it
                         * off; Enter casts */
        G.mouse_x = in->mouse_x, G.mouse_y = in->mouse_y;
        int px, py, sx;
        rune_panel_at(&px, &py, &sx);
        if (in->click_left) {
            int mx = in->mouse_x, my = in->mouse_y, cx = (mx - px - 8) / 34, cy = (my - py - 22) / 34;
            if (mx >= px + 8 && my >= py + 22 && cx >= 0 && cx < 6 && cy >= 0 && cy < 4) {
                int r = cy * 6 + cx;
                if (cw_running()) cw_rune_add(r); /* the original's: a fourth pushes the oldest off */
                else if (has_rune(r) && G.nshelf < 3) G.shelf[G.nshelf++] = r;
            } else if (mx >= sx && mx < sx + 44 && my >= py + 22 && cy >= 0 && cy < 3) { /* a shelf slot: off the shelf */
                if (cw_running()) cw_shelf_clear(); /* the original clears the whole shelf */
                else if (cy < G.nshelf) {
                    for (int i = cy; i < G.nshelf - 1; i++) G.shelf[i] = G.shelf[i + 1];
                    G.nshelf--;
                }
            } else if (mx >= sx && mx < sx + 44 && my >= py + 22 && cy == 3) cast_shelf(); /* the Cast button */
        }
        if (in->text == 13) cast_shelf();
        if (in->text == 8 && G.nshelf) G.nshelf--;
        return true;
    }
    for (int k = 0; k < g_ncr; k++) g_cr[k].frozen = MAX(0.0f, g_cr[k].frozen - dt), g_cr[k].fear = MAX(0.0f, g_cr[k].fear - dt);
    for (int w = 0; w < G.nwards; w++) /* a rune of warding: the first hostile creature on its tile sets it off */
        for (int k = 0; k < g_ncr; k++) {
            Creature *c = &g_cr[k];
            if (c->state >= CS_DYING || c->ally || (int)c->x != G.wards[w][0] || (int)c->z != G.wards[w][1]) continue;
            game_message("%s", s1(245));
            creature_hurt(k, uw_rand_scaled(player_level()) + 3); /* UW.EXE 0x846e3: rand() x level / 0x8000 + 3 */
            G.wards[w][0] = G.wards[G.nwards - 1][0], G.wards[w][1] = G.wards[G.nwards - 1][1], G.nwards--, w--;
            break;
        }
    if (in->fight) {
        G.fight = !G.fight;
        sound_song_fight_mode(G.fight); /* the player record's 0x5f bit 1 and its song (UW.EXE 0x27621, 0x274c5) */
        game_message("%s", G.fight ? "Fight mode: a click attacks (hold to charge)." : "Fight mode off: a click looks.");
    }
    if (cw_running()) {
        /* the blow is the original's (COMBAT.C's player_attack in the rules core): fight mode draws its weapon, an
         * attack starts a swing in the row the crosshair picks (thrust the top, slash the middle, bash the bottom:
         * the ninths of its view, the middle column), and it charges while the attack is held; our weapon in the view
         * is the picture */
        if (cw_aiming() && (in->attack || in->click_left) && !G.open) { /* a missile spell: released where you look */
            cw_release_spell();
            goto core_fight_done;
        }
        cw_fight(G.fight);
        int swing = 0;
        if (G.fight && in->attack && !G.open) {
            int kind = attack_type(eye, fwd); /* 0 slash, 1 bash, 2 thrust */
            swing = kind == 2 ? 2 : kind == 1 ? 8 : 5;
        }
        cw_attack(swing, G.fight && (in->attack || in->attack_held));
    core_fight_done:;
    }
    weapon_step(dt, eye, fwd);
    missiles_update(dt, eye);
    if (G.open) {
        /* the panel; the world under the mouse pointer when it is off the panel (look: a left click or L; use,
         * take, open, talk: a right click or E). A left click in the world with a thing held drops it. */
        G.mouse_x = in->mouse_x, G.mouse_y = in->mouse_y;
        bool was_holding = inv_holding(), over = inv_mouse_over(in->mouse_x, in->mouse_y);
        InvInput ii = {in->click_left, in->click_right, in->look && over, in->shift, in->mouse_x, in->mouse_y};
        if (inv_update(G.lv, &ii, eye, fwd) || in->click_left) rebuild_sprites();
        G.hover_obj = G.hover_door = G.hover_creature = -1, G.tag = false;
        if (!over && in->mouse_dir_ok) {
            hover_world(eye, in->mouse_dir);
            if (was_holding && in->click_left && !use_held_on_world()) {
                inv_drop_held(G.lv, eye, in->mouse_dir);
                rebuild_sprites();
            }
            if (!was_holding && (in->click_left || in->look)) look_world(eye, in->mouse_dir);
            if (!was_holding && (in->click_right || in->use)) {
                use_hovered(eye, in->mouse_dir);
                if (G.talking) return true;
            }
        }
    } else {
        if (G.sheet) G.mouse_x = in->mouse_x, G.mouse_y = in->mouse_y; /* the pointer is free on the character panel */
        hover_world(eye, fwd);
        if (in->use && G.hover_creature >= 0) {
            use_hovered(eye, fwd);
            return G.talking;
        }
        /* The left button, as the original's command icons have it (UW.EXE 0x26fef, the table at DS 0x28f): with
         * no icon chosen a click in the view looks; only in Fight mode (C here) does it attack, starting a swing
         * that charges while held and strikes when let go. The look tag shows what a click would look at. */
        G.tag = !G.fight && G.charge < 0 && (G.hover_obj >= 0 || G.hover_door >= 0 || G.hover_creature >= 0);
        if (in->attack) {
            if (!G.fight) look_world(eye, fwd);
            else if (player_shoot(eye, fwd)) {
            } else if (W.type < 0 && weapon_kind() >= 0) { /* a swing starts (drawn back, then held while charging) */
                G.charge = 0;
                W.type = attack_type(eye, fwd), W.f = -1, W.t = 0, W.charge = -1;
            }
        }
        if (G.charge >= 0) {
            if (in->attack_held) { /* the weapon's rate every 1/16 s, up to 100 (UW.EXE 0x2574b) */
                int w = inv_weapon();
                G.charge = MIN(1.0f, G.charge + dt * 16.0f * obj_info(w >= 0 ? w : 15)->rate / 100.0f);
            } else W.charge = G.charge, G.charge = -1; /* let go: the swing goes on and lands at f 6 */
        }
        if (in->look) look_world(eye, fwd);
        if (in->use) use_hovered(eye, fwd);
    }
    /* the doors swing (half a second) */
    for (int k = 0; k < G.ndoors; k++) {
        float *o = &G.doors[k].open, t = G.door_target[k];
        if (*o < t) *o = MIN(t, *o + dt * 2);
        else if (*o > t) {
            *o = MAX(t, *o - dt * 2);
            if (*o <= 0.0f) { /* shut: its thud, effect 0xC at the door (UW.EXE toast_animobj) */
                const Door *d = &G.doors[k];
                sfx_at_(0xc, d->tx + 0.5f, d->tz + 0.5f, 0);
            }
        }
    }
    upload_sprites();
    /* the move triggers of the tile the player has just walked into */
    int tx = (int)eye[0], tz = (int)eye[2];
    if (cw_running()) {
        core_world_sync();
        if (cw_conv_active() && !G.talking) core_talk_open(); /* a creature or a trigger has begun one */
        int sh[3];
        G.nshelf = cw_shelf(sh); /* the shelf is the original's */
        for (int i = 0; i < G.nshelf; i++) G.shelf[i] = sh[i];
    }
    if (cw_running()) { /* the rules core sets its own off (its player_sqhandler), and has the player's vitals */
        int hp, hpmax, mana, manamax;
        cw_vitals(&hp, &hpmax, &mana, &manamax);
        G.hp = (float)hp, G.hp_max = (float)MAX(1, hpmax), G.mana = (float)mana, G.mana_max = (float)MAX(1, manamax);
        if (cw_dead()) G.hp = 0; /* the original ended it (real_death): our death screen */
    } else if ((tx != G.last_tx || tz != G.last_tz) && tx >= 0 && tx < 64 && tz >= 0 && tz < 64) {
        if (G.last_tx >= 0) enter_tile(tx, tz);
        G.last_tx = tx, G.last_tz = tz;
    }
    return G.open || G.sheet;
}

/* --- the HUD ------------------------------------------------------------------------------------------ */
/* a flask: the empty flask (pictures 75 blue, 76 red) with the liquid's slices stacked to the level */
static void flask(int x, int y, float frac, int first_slice, int glass) {
    const Pic *g = ui_pic("ui\\flasks", glass);
    if (!g) return;
    int n = (int)(frac * 13 + 0.5f);
    for (int k = 0; k < n; k++) {
        const Pic *s = ui_pic("ui\\flasks", first_slice + 12 - k);
        if (s) ui_draw(s, x, y + g->h - 4 - k * 2 - s->h, 1);
    }
    ui_draw(g, x, y, 1);
}

/* the character panel (,): the numbers the rules use, all of them, in the game's own words (strings block 2). With
 * the pointer on one, a tooltip says what it is and what it does, by the original's rules (MECHANICS.md). */
static int player_armour(int part);
enum { SH_NONE = -1, /* 0-19: the skills */ SH_NAME = 100, SH_LEVEL, SH_EXP, SH_POINTS, SH_STR, SH_DEX, SH_INT, SH_VIT, SH_MANA,
       SH_BODY, SH_CARRY, SH_ARMOUR, SH_ATTACK, SH_DEFENSE, SH_FIGHT };
static int g_sheet_hot = SH_NONE; /* what the pointer is on (drawn last, over everything) */
static bool g_sheet_live;         /* the pointer is free (the panel can be pointed at) */

/* a piece of the panel's text; under the pointer, a darker paper behind it and its key kept */
static bool sheet_hot(int x, int y, int w, int h) {
    return g_sheet_live && G.mouse_x >= x && G.mouse_x < x + w && G.mouse_y >= y && G.mouse_y < y + h;
}
static void sheet_seg(int *x, int y, const char *s, u8 r, u8 g, u8 b, int key) {
    int w = ui_text_width(FONT_NORMAL, s), lh = ui_font_height(FONT_NORMAL) + 2;
    if (sheet_hot(*x - 2, y - 1, w + 4, lh)) g_sheet_hot = key, ui_fill(*x - 2, y - 1, w + 4, lh, UI_PAPER_MID, 255);
    ui_text(FONT_NORMAL, s, *x, y, r, g, b);
    *x += w + 9;
}

static int attack_skill_now(void) {
    int wi = inv_weapon();
    return wi >= 0 && obj_info(wi)->skill >= 3 && obj_info(wi)->skill <= 5 ? obj_info(wi)->skill : SK_UNARMED;
}
static int attack_score_now(void) { return P.skill[SK_ATTACK] / 2 + P.skill[attack_skill_now()] + P.dex / 7 + (P.easy ? 7 : 0); }

static void sheet_draw(const Overlay *o) {
    int w = 214, x = 6, y = 6, lh = ui_font_height(FONT_NORMAL) + 2;
    int h = 10 * lh + 11 * lh + 12;
    g_sheet_hot = SH_NONE, g_sheet_live = G.open || G.sheet || CG.on;
    ui_parchment(x, y, w, h);
    int cx = x + 6, cy = y + 5, sx;
    char a[96], b[64], c[64];
    if (P.created || (CG.on && CG.step > CG_HEAD)) { /* the face chosen (HEADS.GR: 0-4 male, 5-9 female) */
        const Pic *face = ui_pic("ui\\heads", P.sex * 5 + P.head);
        if (face) ui_draw(face, x + w - face->w - 4, y + 4, 1);
    }
    snprintf(a, sizeof a, "%s the %s", P.name, str_get(2, 23 + MAX(0, MIN(7, P.cls))));
    snprintf(b, sizeof b, "level %d", P.level);
    sx = cx, sheet_seg(&sx, cy, a, UI_INK_HEAD, SH_NAME), sheet_seg(&sx, cy, b, UI_INK_HEAD, SH_LEVEL), cy += lh;
    snprintf(a, sizeof a, "%s %u", str_get(2, 22), (unsigned)(P.exp / 10)); /* (sp_xp: tenths, shown / 10) */
    snprintf(b, sizeof b, "skill points %d", P.skill_points);
    sx = cx, sheet_seg(&sx, cy, a, UI_INK, SH_EXP), sheet_seg(&sx, cy, b, UI_INK, SH_POINTS), cy += lh;
    snprintf(a, sizeof a, "%s %d", str_get(2, 17), P.str);
    snprintf(b, sizeof b, "%s %d", str_get(2, 18), P.dex);
    snprintf(c, sizeof c, "%s %d", str_get(2, 19), P.intl);
    sx = cx, sheet_seg(&sx, cy, a, UI_INK, SH_STR), sheet_seg(&sx, cy, b, UI_INK, SH_DEX), sheet_seg(&sx, cy, c, UI_INK, SH_INT), cy += lh;
    snprintf(a, sizeof a, "%s %d/%d", str_get(2, 20), (int)G.hp, (int)G.hp_max);
    snprintf(b, sizeof b, "%s %d/%d", str_get(2, 21), (int)G.mana, (int)G.mana_max);
    sx = cx, sheet_seg(&sx, cy, a, UI_INK_RED, SH_VIT), sheet_seg(&sx, cy, b, UI_INK_BLUE, SH_MANA), cy += lh;
    inv_status(a, sizeof a);
    sx = cx, sheet_seg(&sx, cy, a, UI_INK_FAINT, SH_BODY), cy += lh;
    snprintf(a, sizeof a, "Carrying %d.%d of %d.%d stones", inv_carried() / 10, inv_carried() % 10, inv_capacity() / 10, inv_capacity() % 10);
    sx = cx, sheet_seg(&sx, cy, a, UI_INK_FAINT, SH_CARRY), cy += lh;
    snprintf(a, sizeof a, "Armour: body %d  arms %d  legs %d  head %d", player_armour(0), player_armour(1), player_armour(2), player_armour(3));
    sx = cx, sheet_seg(&sx, cy, a, UI_INK_BLUE, SH_ARMOUR), cy += lh;
    snprintf(a, sizeof a, "Attack score %d (%s)", attack_score_now(), str_get(2, 31 + attack_skill_now()));
    snprintf(b, sizeof b, "Defense %d", P.skill[SK_DEFENSE]);
    sx = cx, sheet_seg(&sx, cy, a, UI_INK_BLUE, SH_ATTACK), sheet_seg(&sx, cy, b, UI_INK_BLUE, SH_DEFENSE), cy += lh;
    sx = cx, sheet_seg(&sx, cy, G.fight ? "Fight mode (C)" : "Fight mode off (C)", UI_INK_GOLD, SH_FIGHT), cy += lh + 2;
    ui_fill(cx, cy - 2, w - 12, 1, UI_PAPER_DARK, 255);
    cy += 2;
    for (int k = 0; k < SKILLS; k++) { /* two columns of ten; a whole row is the skill's */
        int col = k / 10, row = k % 10, kx = cx + col * (w / 2), ky = cy + row * lh, kw = w / 2 - 18;
        if (sheet_hot(kx - 2, ky - 1, kw + 4, lh)) g_sheet_hot = k, ui_fill(kx - 2, ky - 1, kw + 4, lh, UI_PAPER_MID, 255);
        ui_text(FONT_NORMAL, str_get(2, 31 + k), kx, ky, UI_INK);
        snprintf(a, sizeof a, "%d", P.skill[k]);
        ui_text(FONT_NORMAL, a, kx + kw - ui_text_width(FONT_NORMAL, a), ky, P.skill[k] ? 112 : 88, P.skill[k] ? 36 : 62, P.skill[k] ? 12 : 48);
    }
    (void)o;
}

/* the tooltip of what the pointer is on in the character panel: what it is, what it does by the original's rules
 * (with this character's numbers), and how it grows. Rules not decoded yet are said to be so. */
static void sheet_tip(void) {
    int k = g_sheet_hot;
    if (k == SH_NONE) return;
    static char t[64], s[1200];
    int n = 0;
#define ADD(...) (n += snprintf(s + n, sizeof s - (size_t)MIN((int)sizeof s, n), __VA_ARGS__), n = MIN(n, (int)sizeof s - 1))
    s[0] = 0;
    static const u8 NEXT[16] = {0, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192}; /* exp / 500 for each level */
    if (k >= 0 && k < SKILLS) {
        static const char *const WHAT[SKILLS] = {
            "Fighting with any weapon: half of it is added to every attack score.",
            "A creature's blow is a skill check of its attack score against this: the higher, the more it misses.",
            "Fists: attack score + Unarmed. Damage Unarmed x 2/5 + Strength / 6 + 4.",
            "Attack score with a sword: + Sword.",
            "Attack score with an axe: + Axe.",
            "Attack score with a mace or club: + Mace.",
            "Bows, crossbows and slings: attack score + Missile. A missile flies at Missile x 8 + 192; a Missile check against "
            "10 makes it faster (critical) or slower (fumble), and its damage grows with its speed.",
            "Mana: thy most is (Mana + 1) x Intelligence / 8. Every 3 ticks (about a minute) a Mana check against 10 gives "
            "back 1 or 2 points.",
            "Lore: the original's use of it is not decoded yet.",
            "Each spell is a check of Casting + 5 against twice its circle: a failure fizzles, a fumble backfires and "
            "harms thee.",
            "Traps: the original's use of it (finding and disarming traps) is not decoded yet.",
            "A hidden thing set to be found by looking: a Search check against its difficulty finds it.",
            "Track (K): every creature within 8 tiles gets a Track check; thou art told the direction where the most are.",
            "Creatures hear thee by 13 - Sneak / 3 and see thee by 15 - Sneak / 5: the higher, the less they notice.",
            "Use an anvil on a weapon or armour: a Repair check against its durability; success mends Repair / 5 + 3, a "
            "critical mends it fully.",
            "Charm: the original's use of it (bartering) is not decoded yet.",
            "Use a lockpick on a lock: a Picklock check against 3 x the lock's difficulty. Some locks cannot be picked.",
            "A fall: an Acrobat check against twice its harm; on success only (30 - Acrobat) / 30 of it is taken. Its use "
            "for jumping is not decoded yet.",
            "Appraise: the original's use of it (bartering) is not decoded yet.",
            "In water, every tick a Swimming check against thy load (carried / most x 32): failures tire thee; too tired and "
            "thou dost drown.",
        };
        static const char *const ATTR[3] = {"Strength", "Intelligence", "Dexterity"};
        int ai = k < 7 ? 0 : k < 10 ? 1 : 2, av = ai == 0 ? P.str : ai == 1 ? P.intl : P.dex;
        snprintf(t, sizeof t, "%s %d", str_get(2, 31 + k), P.skill[k]);
        ADD("%s\n", WHAT[k]);
        if (k == SK_ATTACK) ADD("Thy attack score now: %d.\n", attack_score_now());
        if (k >= SK_UNARMED && k <= SK_MACE) ADD("Thy attack score with it: %d.\n", P.skill[SK_ATTACK] / 2 + P.skill[k] + P.dex / 7 + (P.easy ? 7 : 0));
        if (k == SK_MANA) ADD("Thy most mana now: %d.\n", (P.skill[SK_MANA] + 1) * P.intl / 8);
        ADD("Raised at a shrine: chant %s (a skill point): two tries, each +1, +1 more ", str_get(2, 51 + k));
        if (ai) ADD("while below half thy %s, +1 more ", ATTR[ai]);
        ADD("by chance (likelier the further below %s it is). It stops at 30, or once above twice thy %s (%d).", ATTR[ai], ATTR[ai], 2 * av);
    } else
        switch (k) {
        case SH_NAME:
            snprintf(t, sizeof t, "%s", P.name);
            ADD("Thy class gave thy starting attributes and the skills chosen at the start.");
            break;
        case SH_LEVEL:
            snprintf(t, sizeof t, "Level %d", P.level);
            ADD("Each level adds a skill point and raises thy most vitality (30 + level x Strength / 5).\n");
            ADD("Spells up to circle (level + 1) / 2 can be cast: circle %d now.\n", (P.level + 1) / 2);
            if (P.level < 16) ADD("Next level at %d experience.", NEXT[P.level] * 50);
            else ADD("The highest level.");
            break;
        case SH_EXP:
            snprintf(t, sizeof t, "%s %u", str_get(2, 22), (unsigned)(P.exp / 10));
            ADD("Gained by slaying creatures: 0.4 x the creature's worth + 2 dice of it, tenths; a powerful one 1.5 to 3 "
                "times that. Death takes an eighth.\n");
            ADD("Halved when thy level is above twice the dungeon level + 2. No more once past 9600.\n");
            ADD("Every 300 gives a skill point.");
            if (P.level < 16) ADD(" Next level at %d.", NEXT[P.level] * 50);
            break;
        case SH_POINTS:
            snprintf(t, sizeof t, "Skill points %d", P.skill_points);
            ADD("Spent at a shrine: use the shrine and chant a skill's mantra to raise that skill (the mantras are in each "
                "skill's tooltip).\nOne for every level and for every 300 experience.");
            break;
        case SH_STR:
            snprintf(t, sizeof t, "%s %d", str_get(2, 17), P.str);
            ADD("Most vitality: 30 + level x Strength / 5 (%d).\n", 30 + P.level * P.str / 5);
            ADD("Damage: + Strength / 9 to weapon blows (+%d); fists + Strength / 6.\n", P.str / 9);
            ADD("Carry limit: Strength x 2 stones (%d).\n", P.str * 2);
            ADD("Every 24 ticks (about 8 minutes) a Strength check against 15 heals a point.\n");
            ADD("Governs Attack, Defense, Unarmed, Sword, Axe, Mace and Missile.");
            break;
        case SH_DEX:
            snprintf(t, sizeof t, "%s %d", str_get(2, 18), P.dex);
            ADD("Attack score: + Dexterity / 7 (+%d).\n", P.dex / 7);
            ADD("Governs Traps, Search, Track, Sneak, Repair, Charm, Picklock, Acrobat, Appraise and Swimming.");
            break;
        case SH_INT:
            snprintf(t, sizeof t, "%s %d", str_get(2, 19), P.intl);
            ADD("Most mana: (Mana skill + 1) x Intelligence / 8 (%d).\n", (P.skill[SK_MANA] + 1) * P.intl / 8);
            ADD("Governs Mana, Lore and Casting.");
            break;
        case SH_VIT:
            snprintf(t, sizeof t, "%s %d of %d", str_get(2, 20), (int)G.hp, (int)G.hp_max);
            ADD("Thy life: at nothing thou art dead. Most: 30 + level x Strength / 5.\n");
            ADD("Heals a point on a Strength check against 15 every 24 ticks (about 8 minutes), and by sixteenths of the most "
                "while sleeping. Poison takes its strength every 3 ticks.");
            break;
        case SH_MANA:
            snprintf(t, sizeof t, "%s %d of %d", str_get(2, 21), (int)G.mana, (int)G.mana_max);
            ADD("Spent casting: 3 x the spell's circle.\nMost: (Mana skill + 1) x Intelligence / 8.\n");
            ADD("Every 3 ticks (about a minute) a Mana skill check against 10 gives back 1 or 2; sleeping gives back "
                "sixteenths of the most.");
            break;
        case SH_BODY:
            snprintf(t, sizeof t, "Hunger and fatigue");
            ADD("Every 24 ticks (about 8 minutes) hunger grows by 3 to 6 and fatigue by 1. Eat to be fed (each food has "
                "its nourishment); sleep to rest.");
            break;
        case SH_CARRY:
            snprintf(t, sizeof t, "Carrying");
            ADD("What thou canst carry: Strength x 2 stones. In water, the load counts against Swimming.");
            break;
        case SH_ARMOUR:
            snprintf(t, sizeof t, "Armour");
            ADD("Each part's armour is what is worn there: its protection x its condition / 64 + 1 (helm: head; chest: body; "
                "gloves: arms; leggings and boots: legs). A shield in the off hand counts on body and arms. Spells of "
                "protection add to every part.\nIt is taken off the damage of a blow that lands there.");
            break;
        case SH_ATTACK:
            snprintf(t, sizeof t, "Attack score %d", attack_score_now());
            ADD("Attack / 2 + the weapon's skill (%s) + Dexterity / 7%s.\n", str_get(2, 31 + attack_skill_now()), P.easy ? " + 7 (easy)" : "");
            ADD("A blow is a skill check of score against the target's defense: 0-30 + score - defense; above 15 it hits, "
                "above 28 a critical hit.\nDamage: the weapon's slash, bash or thrust (by where thou aimest: top thrust, "
                "middle slash, bottom bash) + Strength / 9, more the longer it was charged.");
            break;
        case SH_DEFENSE:
            snprintf(t, sizeof t, "Defense %d", P.skill[SK_DEFENSE]);
            ADD("The Defense skill: a creature's blow is a check of its attack score against it.");
            break;
        case SH_FIGHT:
            snprintf(t, sizeof t, "Fight mode");
            ADD("C turns it on and off. On: a click attacks (hold to charge the blow). Off: a click looks.");
            break;
        }
#undef ADD
    ui_tip(G.mouse_x, G.mouse_y, t, s, 230);
}

void game_draw_ui(float yaw) {
    Overlay *o = ui_overlay();
    ui_clear();
    if (CG.on) {
        chargen_draw(o);
        return;
    }
    if (G.dark > 0) ui_fill(0, 0, o->w, o->h, 0, 0, 0, (u8)(255 * MIN(1.0f, G.dark < 0.6f ? G.dark / 0.6f : 1.0f)));
    if (G.runes_open) { /* the rune panel: the 24 runes (lit: owned), the shelf under them */
        int px, py, sx, pw = RUNE_PANEL_W, ph = 22 + 4 * 34 + 14;
        rune_panel_at(&px, &py, &sx);
        ui_parchment(px, py, pw, ph);
        ui_text(FONT_NORMAL, "Runes", px + 8, py + 6, UI_INK_HEAD);
        ui_text(FONT_NORMAL, "Shelf", sx + 2, py + 6, UI_INK_HEAD);
        for (int r = 0; r < 24; r++) {
            int x = px + 8 + r % 6 * 34, y = py + 22 + r / 6 * 34;
            bool own = has_rune(r);
            if (own) ui_inset(x, y, 32, 32, false);
            else ui_fill(x, y, 32, 32, UI_PAPER_DARK, 255);
            const Pic *pic = ui_pic("sprites\\objects", 0xe8 + r);
            if (pic && own) ui_draw(pic, x + 16 - pic->w / 2, y + 2, 1);
            ui_text(FONT_NORMAL, RUNE_NAME[r], x + 16 - ui_text_width(FONT_NORMAL, RUNE_NAME[r]) / 2, y + 22, own ? 60 : 128, own ? 40 : 96,
                    own ? 32 : 78);
        }
        for (int i = 0; i < 3; i++) {
            int y = py + 22 + i * 34;
            ui_inset(sx, y, 44, 32, false);
            if (i < G.nshelf) {
                const Pic *pic = ui_pic("sprites\\objects", 0xe8 + G.shelf[i]);
                if (pic) ui_draw(pic, sx + 22 - pic->w / 2, y + 2, 1);
                ui_text(FONT_NORMAL, RUNE_NAME[G.shelf[i]], sx + 22 - ui_text_width(FONT_NORMAL, RUNE_NAME[G.shelf[i]]) / 2, y + 22, UI_INK);
            }
        }
        int by = py + 22 + 3 * 34;
        if (G.nshelf) ui_fill(sx, by, 44, 32, UI_INK_HEAD, 255);
        else ui_fill(sx, by, 44, 32, UI_PAPER_DARK, 255);
        ui_text(FONT_NORMAL, "Cast", sx + 22 - ui_text_width(FONT_NORMAL, "Cast") / 2, by + 12, 255, 240, 200);
        ui_text(FONT_SMALL, "CLICK: ON OR OFF THE SHELF   ENTER: CAST", px + 8, py + ph - 11, UI_INK_FAINT);
        ui_cursor(0, G.mouse_x, G.mouse_y);
    } else if (G.nshelf) { /* the shelf beside the compass */
        char line[32] = "";
        for (int i = 0; i < G.nshelf; i++) strcat(line, RUNE_NAME[G.shelf[i]]), strcat(line, i < G.nshelf - 1 ? " " : "");
        ui_text(FONT_NORMAL, line, o->w / 2 - 70 - ui_text_width(FONT_NORMAL, line), o->h - 16, 230, 210, 150);
    }
    if (G.nactive) { /* the lasting spells, their names (strings block 6) at the top right */
        for (int k = 0; k < G.nactive; k++) {
            static const char *const SPECIAL[4] = {"Freeze Time", "Roaming Sight", "Speed", "Telekinesis"};
            const char *nm = G.act_cat[k] == 3 && G.act_str[k] == 1 ? "Protection"
                             : G.act_cat[k] < 4                     ? str_get(6, G.act_cat[k] * 16 + G.act_str[k])
                                                                    : SPECIAL[G.act_str[k] & 3];
            ui_text(FONT_NORMAL, nm, o->w - 6 - ui_text_width(FONT_NORMAL, nm), 6 + k * 11, 160, 200, 255);
        }
    }
    if (G.chanting) {
        int pw = 220, px = (o->w - pw) / 2, py = o->h / 3;
        ui_parchment(px, py, pw, 40);
        ui_text(FONT_NORMAL, "Chant a mantra:", px + 8, py + 6, UI_INK_HEAD);
        char line[40];
        snprintf(line, sizeof line, "%s_", G.mantra);
        ui_text(FONT_NORMAL, line, px + 8, py + 20, UI_INK);
    }
    if (G.dead) {
        ui_fill(0, 0, o->w, o->h, 30, 0, 0, 200);
        int pw = 200, px = (o->w - pw) / 2, py = o->h / 3, lh = ui_font_height(FONT_NORMAL) + 4;
        ui_text(FONT_BIG, "Thou art dead.", px + (pw - ui_text_width(FONT_BIG, "Thou art dead.")) / 2, py, 230, 60, 50);
        static const char *const OPT[2] = {"1. Load the last saved game", "2. Begin a new game"};
        for (int k = 0; k < 2; k++) {
            int y = py + 30 + k * lh;
            bool hov = G.mouse_y >= y && G.mouse_y < y + lh;
            ui_text(FONT_NORMAL, OPT[k], px + 20, y, hov ? 255 : 230, hov ? 240 : 200, hov ? 160 : 120);
            G.option_y[k] = y, G.option_h[k] = lh;
        }
        ui_cursor(0, G.mouse_x, G.mouse_y);
        return;
    }
    weapon_draw(o); /* under everything else */
    if (G.sheet || (G.open && inv_with_sheet())) sheet_draw(o); /* , alone, or with the pack (the checkbox) */
    if (G.reading) {
        int x, y, w, h;
        read_rect(&x, &y, &w, &h);
        /* parchment */
        ui_parchment(x, y, w, h);
        ui_text(FONT_NORMAL, G.read_title, x + 8, y + 5, UI_INK_HEAD);
        int top = y + 18, page = h - 34;
        ui_clip(top, top + page);
        int total = ui_text_wrap(FONT_NORMAL, G.read_text, x + 8, top - G.read_scroll, w - 16, UI_INK, true);
        ui_clip(-1, -1);
        const char *hint = G.read_scroll + page < total ? "CLICK FOR MORE" : "CLICK TO CLOSE";
        ui_text(FONT_SMALL, hint, x + w - 8 - ui_text_width(FONT_SMALL, hint), y + h - 11, UI_INK_FAINT);
        ui_cursor(10, G.mouse_x, G.mouse_y);
        return;
    }
    if (G.talking) {
        const Conv *cv = talk_state();
        int pw = MIN(o->w - 20, 360), px = (o->w - pw) / 2, py = o->h / 2 - 10, ph = o->h - py - 50;
        ui_parchment(px - 6, py - 16, pw + 12, ph + 22);
        ui_text(FONT_NORMAL, cv->who, px, py - 12, UI_INK_HEAD);
        /* the newest words that fit above the replies */
        int opts_h = 0;
        for (int i = 0; i < cv->noptions; i++) opts_h += ui_text_wrap(FONT_NORMAL, cv->options[i], 0, 0, pw - 14, 0, 0, 0, false) + 2;
        int room = ph - opts_h - 8, first = cv->nlines, used = 0;
        while (first > 0) {
            int h = ui_text_wrap(FONT_NORMAL, cv->lines[first - 1], 0, 0, pw, 0, 0, 0, false) + 3;
            if (used + h > room) break;
            used += h, first--;
        }
        int y = py;
        for (int i = first; i < cv->nlines; i++) {
            bool mine = cv->lines[i][0] == '>';
            y += ui_text_wrap(FONT_NORMAL, cv->lines[i], px, y, pw, mine ? 88 : 60, mine ? 62 : 40, mine ? 48 : 32, true) + 3;
        }
        y = py + ph - opts_h;
        for (int i = 0; i < cv->noptions; i++) {
            char num[8];
            snprintf(num, sizeof num, "%d.", i + 1);
            bool hov = G.mouse_y >= y && G.mouse_y < y + ui_font_height(FONT_NORMAL) + 2;
            ui_text(FONT_NORMAL, num, px, y, UI_INK_GOLD);
            int h = ui_text_wrap(FONT_NORMAL, cv->options[i], px + 14, y, pw - 14, hov ? 112 : 60, hov ? 36 : 40, hov ? 12 : 32, true);
            G.option_y[i] = y, G.option_h[i] = h + 2;
            y += h + 2;
        }
        if (!cv->running) ui_text(FONT_SMALL, "E OR ESC TO LEAVE", px + pw - ui_text_width(FONT_SMALL, "E OR ESC TO LEAVE"), py + ph, UI_INK_FAINT);
        ui_cursor(10, G.mouse_x, G.mouse_y);
        return;
    }
    if (G.open && !inv_holding()) {
        char name[96] = "";
        if (G.hover_creature >= 0) creature_name(&g_cr[G.hover_creature], name, sizeof name);
        else if (G.hover_door >= 0) snprintf(name, sizeof name, "%s", G.doors[G.hover_door].open > 0.5f ? "an open door" : "a door");
        else if (G.hover_obj >= 0) item_name(&G.lv->obj[G.hover_obj], name, sizeof name);
        if (name[0]) {
            int w = ui_text_width(FONT_NORMAL, name);
            ui_parchment(G.mouse_x + 8, G.mouse_y + 7, w + 9, 13);
            ui_text(FONT_NORMAL, name, G.mouse_x + 12, G.mouse_y + 10, UI_INK);
        }
    }
    if (!G.open) {
        /* The crosshair: gone while an attack charges (as the original hides its pointer); the blue pointer on
         * something to look at (the look tag: a click looks); the red ring on a hostile creature in reach */
        bool foe = G.hover_creature >= 0 && g_cr[G.hover_creature].hostile;
        int cur = G.tag ? 10 : foe ? 9 : 0;
        const Pic *c = ui_pic("ui\\cursors", cur);
        if (c && G.charge < 0) ui_draw(c, o->w / 2 - c->w / 2, o->h / 2 - c->h / 2, 1);
        /* what it is and what can be done: at the bottom right, out of the way */
        char name[96] = "", hint[64] = "";
        if (G.hover_creature >= 0) {
            creature_name(&g_cr[G.hover_creature], name, sizeof name);
            snprintf(hint, sizeof hint, "%s", foe ? (G.fight ? "CLICK - ATTACK (HOLD TO CHARGE)" : "C - FIGHT   CLICK - LOOK")
                                              : G.fight ? "E - TALK   L - LOOK" : "E - TALK   CLICK - LOOK");
        } else if (G.hover_door >= 0) {
            const Door *d = &G.doors[G.hover_door];
            if ((d->item & 7) != 7 || d->open > 0.5f)
                snprintf(name, sizeof name, "%s", d->open > 0.5f ? "an open door" : "a door"),
                    snprintf(hint, sizeof hint, "%s", d->open > 0.5f ? (G.fight ? "E - CLOSE   L - LOOK" : "E - CLOSE   CLICK - LOOK")
                                                                     : (G.fight ? "E - OPEN   L - LOOK" : "E - OPEN   CLICK - LOOK"));
        } else if (G.hover_obj >= 0) {
            item_name(&G.lv->obj[G.hover_obj], name, sizeof name);
            int hit = G.lv->obj[G.hover_obj].item;
            const char *lk = G.fight ? "L - LOOK" : "CLICK - LOOK";
            snprintf(hint, sizeof hint, "%s%s", hit >= 0x170 && hit <= 0x17F ? "E - USE   " : obj_info(hit)->pick ? "E - TAKE   " : "", lk);
        }
        int rx = o->w - 36;
        if (name[0]) ui_text(FONT_NORMAL, name, rx - ui_text_width(FONT_NORMAL, name), o->h - 34, 255, 230, 160);
        if (hint[0]) ui_text(FONT_SMALL, hint, rx - ui_text_width(FONT_SMALL, hint), o->h - 23, 200, 200, 200);
    }
    /* the compass: four turns of the star by where the player faces */
    float deg = yaw * 57.2957795f;
    int quarter = ((int)floorf((deg + 45.0f) / 90.0f) % 4 + 4) % 4;
    const Pic *cp = ui_pic("ui\\compass", quarter);
    if (cp) ui_draw(cp, o->w / 2 - cp->w / 2, o->h - cp->h - 4, 1);
    /* the power gem above the compass (POWER.GR): grey at rest, red to gold as an attack charges, green at full */
    {
        int g = G.charge < 0 ? 0 : G.charge >= 1.0f ? 9 + (int)(G.anim * 10) % 5 : 1 + (int)(G.charge * 100) / 12; /* UW.EXE */
        const Pic *gem = ui_pic("ui\\power", g);
        if (gem && cp) ui_draw(gem, o->w / 2 - gem->w / 2, o->h - cp->h - 4 - gem->h, 1);
    }
    /* fight mode: the original's lit Fight icon (LFTI.GR 9) beside the compass */
    if (G.fight && cp) {
        const Pic *fi = ui_pic("ui\\lfti", 9);
        if (fi) ui_draw(fi, o->w / 2 + cp->w / 2 + 4, o->h - fi->h - 6, 1);
    }
    /* health and mana */
    flask(6, o->h - 40, G.hp / G.hp_max, 0, 76);
    flask(o->w - 30, o->h - 40, G.mana / G.mana_max, 25, 76); /* 75 is a filled flask: the empty glass for both */
    /* the messages, newest at the bottom, wrapped, staying longer the longer they are, then fading */
    int y = o->h - 46, mw = MIN(o->w - 80, 420) - (G.open ? 150 : 0);
    for (int i = 0; i < 4; i++) {
        float stay = 6.0f + (float)strlen(G.msg[i]) / 25.0f;
        if (!G.msg[i][0] || G.msg_age[i] > stay + 2) continue;
        int a = G.msg_age[i] < stay ? 255 : (int)(255 * (stay + 2 - G.msg_age[i]) / 2);
        int h = ui_text_wrap(FONT_NORMAL, G.msg[i], 0, 0, mw, 0, 0, 0, false);
        y -= h + 3;
        if (y < 20) break;
        ui_text_wrap(FONT_NORMAL, G.msg[i], 36, y, mw, (u8)(240 * a / 255), (u8)(220 * a / 255), (u8)(180 * a / 255), true);
    }
    if (G.open) inv_draw();
    if (!inv_holding() && g_sheet_hot != SH_NONE && (G.sheet || (G.open && inv_with_sheet()))) {
        sheet_tip();
        ui_cursor(10, G.mouse_x, G.mouse_y);
    } else if (G.sheet && !G.open) ui_cursor(10, G.mouse_x, G.mouse_y);
}

/* --- the rules core's outside calls that the game carries out (they replace the weak stubs of
 * build\gen\core_stubs.c): the message scroll and the sound effects -------------------------------------------- */
/* SCROLLIO.C's scroll_print: the original prints a line in pieces; ours gathers them to the new line, then shows the
 * line among the messages (the colour escapes, a backslash and a digit, and other control bytes left out) */
static void scroll_line(const char *line, int colour) {
    int to = cw_running() ? cw_scroll_target() : 0;
    if (!to) { game_message("%s", line); return; }
    /* the conversation's scrolls: the NPC's (its words, the player's chosen line in colour 1, narration in 2) and
     * the player's (the menu, which ours shows from the core, and respond: the player's words) */
    if (to == 2 && cw_conv_menu()) return;
    if (cw_conv_serial() != g_ccv_serial) memset(&g_ccv, 0, sizeof g_ccv), g_ccv_serial = cw_conv_serial();
    char buf[700];
    snprintf(buf, sizeof buf, to == 2 || colour == 1 ? "> %s" : "%s", line);
    ccv_add(buf);
}
int scroll_print(const char *s) {
    static char line[640];
    static size_t n;
    static int colour;
    for (; s && *s; s++) {
        if (*s == '\\' && s[1] >= '0' && s[1] <= '9') { if (!n && s[1] != '0') colour = s[1] - '0'; s++; continue; }
        if (*s == '\\' && s[1] == 'P') { s++; continue; } /* npc_say's mark */
        if (*s == '\n' || n + 1 >= sizeof line) {
            line[n] = 0;
            if (n) scroll_line(line, colour);
            n = 0, colour = 0;
            if (*s == '\n') continue;
        }
        if ((unsigned char)*s >= 32) line[n++] = *s;
    }
    return 0;
}
/* SOUND.C's play_effect (proved in our sfx.c): effect fx at (x, y) in eighths of a tile, heard from the player */
unsigned char play_effect(unsigned char fx, int x, int y, char vol) {
    if (getenv("UW_CRIT_DEBUG") && *getenv("UW_CRIT_DEBUG")) fprintf(stderr, "effect %d at %d,%d vol %d\n", fx, x, y, vol);
    int h = (int)floorf(G.yaw / 6.2831853f * 256.0f + 0.5f) & 255;
    return (unsigned char)sound_effect_at(fx, x, y, vol, (int)(G.eye[0] * 8), (int)(G.eye[2] * 8), h);
}
unsigned char play_effect_here(unsigned char fx, unsigned char pan, char vol) { return (unsigned char)sound_effect(fx, pan, vol); }
/* at a mobile object: its home tile and its place in it */
unsigned char play_effect_on_mobile(unsigned char fx, const u8 *obj, char vol) {
    if (!obj) return 0xff;
    u32 pw = rd16(obj + 2), home = rd16(obj + 0x16);
    return play_effect(fx, (int)(home >> 10) * 8 + (int)(pw >> 13), (int)((home >> 4) & 63) * 8 + (int)((pw >> 10) & 7), vol);
}
void kill_effect(unsigned char n) { sound_effect_stop(n); }
/* SCROLLIO.C's wdialog, as AskHowMany asks it ("Move how many?"): the count the game chose (cw_set_howmany), else
 * the whole stack (the original clamps the number to the stack); Enter */
int wdialog(const char *prompt, const char *initial, char *result, char anychar, int maxlen) {
    (void)prompt, (void)initial, (void)anychar;
    if (cw_conv_active()) { result[0] = 0; return 0; } /* babl_ask, a typed answer: to do (an empty one for now) */
    int n = cw_howmany();
    snprintf(result, (size_t)(maxlen > 0 ? maxlen + 1 : 4), "%d", n > 0 ? n : 999);
    return 0;
}
/* the checks: what the core's conversation has said */
const Conv *game_core_conv(void) { return &g_ccv; }
