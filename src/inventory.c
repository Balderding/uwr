#include "inventory.h"
#include "core_world.h"
#include "sound.h"
#include "orig.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "game.h"
#include "objdata.h"
#include "ui.h"

/* light sources: 144 lantern, 145 torch, 146 candle, 147 taper; +4 = lit */
enum { ITEM_LANTERN = 144, ITEM_TORCH = 145, ITEM_CANDLE = 146, ITEM_TAPER = 147, LIT = 4 };
static bool is_light(int item) { return item >= ITEM_LANTERN && item <= ITEM_TAPER + LIT; }
static bool is_lit(int item) { return item >= ITEM_LANTERN + LIT && item <= ITEM_TAPER + LIT; }

#define POOL 512
static struct {
    char data[600];
    Object pool[POOL]; /* the player's objects (0 = none); contents chained from a container's link by next */
    bool used[POOL];
    int slot[INV_SLOTS];
    int open[8], nopen; /* the containers opened into the pack's holes, innermost last */
    int scroll;         /* the first content shown when a container is open */
    int held;           /* on the mouse pointer */
    int hover;          /* the hole under the mouse: 0-18 a slot, 19-26 a hole of the open container, -1 */
    bool shift;
    bool hidden[POOL];  /* a bag whose part of the grid is hidden */
    int mouse_x, mouse_y;
    float clock;
    int tick_secs;
    u8 tick;            /* the player tick's counter: 1-24, back to 0 at 24 (UW.EXE [0x3f9]) */
    int hunger;         /* 0-255, a new character 192 (player byte 0x39) */
    int fatigue;        /* 0-255 (player byte 0x3a), up 1 every 24 ticks */
    int drunk, poison;  /* player 0x61 bits 4-9; 0x5f bits 2-5 */
    u32 rng;
    u8 light_dur[16], light_bright[16];
} I;

static int alloc(void) {
    for (int i = 1; i < POOL; i++)
        if (!I.used[i]) {
            I.used[i] = true;
            memset(&I.pool[i], 0, sizeof I.pool[i]);
            return i;
        }
    return 0;
}

static bool holds(const Object *o) { return obj_info(o->item)->container && !(o->flags & 0x40); }
/* what travels with a thing when it is taken or dropped: a container's contents, an enchanted thing's spell object
 * (0x120-0x12F, its link: wands, rings, weapons) */
static bool spell_linked(int item) { return (item & 511) >= 0x120 && (item & 511) <= 0x12f; }
static bool lv_carries(const Level *lv, int i) {
    const Object *o = &lv->obj[i];
    if (o->flags & 0x40 || !o->link || o->link >= 1024 || spell_linked(o->item)) return false; /* a spell object's link is its number */
    return obj_info(o->item)->container || spell_linked(lv->obj[o->link].item);
}
static bool pool_carries(int p) {
    const Object *o = &I.pool[p];
    if (o->flags & 0x40 || !o->link || o->link >= POOL || !I.used[o->link] || spell_linked(o->item)) return false;
    return obj_info(o->item)->container || spell_linked(I.pool[o->link].item);
}

static void free_tree(int p) {
    if (!p) return;
    if (pool_carries(p))
        for (int c = I.pool[p].link, guard = 0; c && guard < POOL; guard++) {
            int nx = I.pool[c].next;
            free_tree(c);
            c = nx;
        }
    I.used[p] = false;
}

/* the weight of a thing with all it holds, tenths of a stone */
static int tree_mass(int p) {
    int m = obj_mass(&I.pool[p]);
    if (holds(&I.pool[p]))
        for (int c = I.pool[p].link, guard = 0; c && guard < POOL; c = I.pool[c].next, guard++) m += tree_mass(c);
    return m;
}
static int contents_mass(int p) {
    int m = 0;
    for (int c = I.pool[p].link, guard = 0; c && guard < POOL; c = I.pool[c].next, guard++) m += tree_mass(c);
    return m;
}

/* --- the light table and the burning ------------------------------------------------------------------- */
static void load_light_table(void) {
    static const u8 DUR[16] = {0, 0, 0, 0, 10, 3, 12, 0}, BRIGHT[16] = {0, 0, 0, 0, 4, 2, 1, 3};
    memcpy(I.light_dur, DUR, 16), memcpy(I.light_bright, BRIGHT, 16);
    char path[700];
    snprintf(path, sizeof path, "%s\\objects\\lights.txt", I.data);
    size_t mark = arena_mark();
    Bytes f = file_read(path);
    for (const char *p = (const char *)f.data; p && p < (const char *)f.data + f.size;) {
        int item, dur, bright;
        if (sscanf(p, "light %d %d %d", &item, &dur, &bright) == 3 && item >= 0x90 && item <= 0x9F)
            I.light_dur[item - 0x90] = (u8)MIN(MAX(dur, 0), 255), I.light_bright[item - 0x90] = (u8)MIN(MAX(bright, 0), 15);
        const char *nl = memchr(p, '\n', (size_t)((const char *)f.data + f.size - p));
        p = nl ? nl + 1 : NULL;
    }
    arena_reset(mark);
}

/* UW.EXE (file 0x2acbd, 0x2af36): every time more than 20 whole seconds have gone by (the clock is real time),
 * a tick; each lit light in the 4 light slots loses a point of quality when the tick counter (a byte) is a
 * multiple of its duration; at the last point it goes out (quality 0, "burned out") and stays where it is */
static void burn_tick(void) {
    for (int s = 5; s <= 8; s++) {
        Object *o = I.slot[s] ? &I.pool[I.slot[s]] : NULL;
        if (!o || !is_lit(o->item)) continue;
        int d = I.light_dur[o->item & 15];
        if (!d || I.tick % d) continue;
        if (o->quality > 1) {
            o->quality--;
            continue;
        }
        o->quality = 0, o->item -= LIT;
        if (o->item == ITEM_LANTERN) game_message("Thy lantern has run out of oil.");
        else {
            char noun[64];
            obj_noun(o, noun, sizeof noun);
            game_message("Thy %s has burned out.", noun);
        }
    }
}
static int rnd(int n) {
    return uw_rand_mod(n); /* the original's rand() % n (orig.c) */
}

/* The player tick (UW.EXE file 0x2acbd), every 21 seconds: the counter goes up; the lights burn; every 3rd
 * tick a point of poison wears off; every 24th hunger falls 3-6, drunkenness 1, fatigue rises 1, and the
 * counter starts again from 0 (so a lantern, duration 10, burns at 10 and 20 of each 24: about a point every
 * 12 ticks). */
static void player_tick(void) {
    I.tick++;
    burn_tick();
    game_player_tick(I.tick, I.poison); /* healing, mana, poison's harm (the game has the numbers) */
    if (I.tick % 3 == 0 && I.poison > 0) I.poison--;
    if (I.tick % 24 == 0) {
        I.hunger = MAX(0, I.hunger - (3 + rnd(4)));
        if (I.drunk > 0) I.drunk--;
        I.fatigue = MIN(255, I.fatigue + 1);
        I.tick = 0;
    }
}

/* "You are well fed and wide awake." (file 0x26122: block 1, 104 + hunger / 30, 118 - fatigue / 23 up to 5) */
void inv_body(int *hunger, int *fatigue, int *drunk, int *poison) { *hunger = I.hunger, *fatigue = I.fatigue, *drunk = I.drunk, *poison = I.poison; }
void inv_set_body(int hunger, int fatigue, int drunk, int poison) {
    I.hunger = MAX(0, MIN(255, hunger)), I.fatigue = MAX(0, MIN(255, fatigue)), I.drunk = MAX(0, drunk), I.poison = MAX(0, poison);
}
void inv_status(char *out, size_t n) {
    snprintf(out, n, "You are %s and %s.", str_get(1, 104 + MIN(8, I.hunger / 30)), str_get(1, 118 - MIN(5, I.fatigue / 23)));
}

void inv_core_sync(void);
void inv_tick(float dt) {
    if (cw_running()) { /* the original keeps the things, the lights and the body's needs */
        inv_core_sync();
        return;
    }
    I.clock += dt;
    int secs = (int)I.clock;
    I.clock -= (float)secs;
    I.tick_secs += secs;
    if (I.tick_secs > 20) I.tick_secs = 0, player_tick();
}

bool inv_light(float *intensity, float *range, float color[3]) {
    static const float C[4][3] = {{1.0f, 0.84f, 0.58f}, {1.0f, 0.70f, 0.40f}, {1.0f, 0.78f, 0.50f}, {1.0f, 0.80f, 0.55f}};
    float best = 0;
    for (int s = 5; s <= 8; s++) {
        const Object *o = I.slot[s] ? &I.pool[I.slot[s]] : NULL;
        if (!o || !is_lit(o->item)) continue;
        /* brightness 0-4 from the table is the light level (0 darkness, 1 burning match, 2 candlelight, 3 light,
         * 4 magic lantern); how far it reaches is the original's view distance for that level: SHADES.DAT's 4th
         * word (UW.EXE 0x810f8): 3 3 4 4 5 6 7 7 tiles (a torch 4, a lantern 5, a candle 3). Intensity 0.4 a level
         * more; a light low on fuel is dimmer */
        static const float REACH[8] = {3, 3, 4, 4, 5, 6, 7, 7};
        float b = (float)I.light_bright[o->item & 15];
        if (b <= best) continue;
        best = b;
        float low = o->quality < 8 ? 0.55f + 0.45f * o->quality / 8.0f : 1.0f;
        *intensity = (0.5f + 0.4f * b) * low, *range = REACH[(int)b & 7];
        memcpy(color, C[(o->item - ITEM_LANTERN) & 3], 12);
    }
    int sl = game_spell_light(); /* a light spell, when brighter */
    if (sl > best) {
        static const float REACH[8] = {3, 3, 4, 4, 5, 6, 7, 7};
        best = (float)sl, *intensity = 0.5f + 0.4f * best, *range = REACH[sl & 7];
        color[0] = 0.85f, color[1] = 0.9f, color[2] = 1.0f;
    }
    return best > 0;
}

/* an object's enchantment (UW.EXE 0x38b5c). Never a trap or trigger (0x180-0x1BF). Its source: when the object has a
 * link (no quantity), the spell object (0x120-0x12F) in the chain the link leads to -- one with no charges left is
 * missed 4 times in 10 unless only looked at (DS 0x9f8); when it has a quantity and is enchanted (word 0 bit 12) and
 * is not furniture (0x140-0x17F), the object itself. The source's 9 bits l: major (word 0 bit 11): group l >> 6 --
 * 0 the spell numbered l & 63 (category -1), else category 12 + group, value l & 63; normal: category l >> 4, value
 * l & 15 (12 on a weapon: accuracy 0-7, damage 8-15, each + (v & 7) + 1, 0x254ed). Block 6 names it: index l, + 256
 * when major (and for a weapon's or armour's 12 and 13, whose names are there). */
static const Object *enchant_src(const Object *o, bool look) {
    int cls = (o->item & 0x1c0) >> 6;
    if (cls == 6) return NULL;
    if (!(o->flags & 0x40)) {
        for (int q = o->link, n = 0; q > 0 && q < POOL && I.used[q] && n < 64; q = I.pool[q].next, n++) {
            int it = I.pool[q].item & 511;
            if (it >= 0x120 && it <= 0x12f) return (I.pool[q].quality & 63) == 0 && !look && rnd(10) < 4 ? NULL : &I.pool[q];
        }
        return NULL;
    }
    return (o->flags & 0x08) && cls != 5 ? o : NULL;
}
static bool enchant_read(const Object *o, bool look, int *cat, int *val, bool *major) {
    const Object *src = enchant_src(o, look);
    if (!src) return false;
    int l = src->link & 0x1ff;
    *major = (src->flags & 0x04) != 0;
    if (*major) *cat = (l >> 6) ? 12 + (l >> 6) : -1, *val = l & 63;
    else *cat = l >> 4, *val = l & 15;
    return true;
}
static bool enchant_of(const Object *o, int *cat, int *val) {
    bool major;
    if (!enchant_read(o, false, cat, val, &major)) return false;
    if (*cat == -1) *cat = 16; /* "the spell numbered val": game_enchant_effect's 16 */
    return true;
}
const char *inv_enchant_name(int p) {
    if (p <= 0 || p >= POOL || !I.used[p]) return "";
    const Object *o = &I.pool[p], *src = enchant_src(o, true);
    if (!src) return "";
    int l = src->link & 0x1ff, item = o->item & 511;
    bool major = (src->flags & 0x04) != 0;
    if (!major && item >= 0x20 && item < 0x40 && (l >> 4) == 12) return str_get(6, 464 + (l & 15)); /* armour: Protection, Toughness */
    return str_get(6, major || (item < 0x20 && (l >> 4) == 12) ? 256 + l : l);
}
/* is the thing in slot s worn, so that its enchantment counts (UW.EXE 0x79c12): the armour slots 0-4 and the
 * rings 9-10 always; the off hand (8) only for a shield (0x2B-0x2F, 0x3B-0x3F); never a weapon in hand */
static bool worn_counts(int s) {
    if (s <= 4 || s == 9 || s == 10) return true;
    if (s != 8 || !I.slot[s]) return false;
    int item = I.pool[I.slot[s]].item & 511;
    return item >= 0x20 && item < 0x40 && (item & 15) >= 0xb;
}
int inv_worn_enchants(int *cat, int *val, int cap) { /* UW.EXE 0x7e6e5: the worn slots, the normal form only */
    int n = 0;
    for (int s = 0; s < INV_BODY && n < cap; s++) {
        int c, v;
        bool major;
        if (!I.slot[s] || !worn_counts(s) || !enchant_read(&I.pool[I.slot[s]], true, &c, &v, &major) || major) continue;
        if (c <= 3 || c == 11) cat[n] = c, val[n] = v, n++;
    }
    return n;
}
void inv_enchant_armour(int prot[4], int tough[4]) {
    static const int PART[5] = {3, 0, 1, 2, 2}; /* DS 0x1af8: helm head, chest body, gloves arms, leggings and boots legs */
    for (int p = 0; p < 4; p++) prot[p] = tough[p] = 0;
    for (int s = 0; s < INV_BODY; s++) {
        int c, v;
        bool major;
        if (!I.slot[s] || !worn_counts(s) || !enchant_read(&I.pool[I.slot[s]], true, &c, &v, &major) || major || c != 12) continue;
        int parts[2] = {s <= 4 ? PART[s] : 0, s <= 4 ? -1 : 1}; /* a ring or shield: body and arms */
        for (int k = 0; k < 2 && parts[k] >= 0; k++) {
            if (v & 8) tough[parts[k]] += (v & 7) + 1;
            else prot[parts[k]] += (v & 7) + 1;
        }
    }
}
void inv_weapon_enchant(int *accuracy, int *damage) {
    int c, v;
    bool major;
    *accuracy = *damage = 0;
    if (I.slot[7] && enchant_read(&I.pool[I.slot[7]], true, &c, &v, &major) && !major && c == 12) {
        if (v & 8) *damage = (v & 7) + 1;
        else *accuracy = (v & 7) + 1;
    }
}

/* --- start ---------------------------------------------------------------------------------------------- */
void inv_start(const char *data_dir, bool give_torch) {
    memset(&I, 0, sizeof I);
    str_copy(I.data, sizeof I.data, data_dir);
    I.hover = -1, I.hunger = 192, I.rng = 0x1234567;
    load_light_table();
    if (give_torch) { /* a lit torch in the right hand: the dungeon is dark */
        int p = alloc();
        I.pool[p].item = ITEM_TORCH + LIT, I.pool[p].quality = 40;
        I.slot[7] = p;
    }
}

bool inv_holding(void) { return I.held != 0; }
int inv_held_index(void) { return I.held; }
static bool place_new(int p);
static bool place_new(int p);
static bool drop_held(Level *lv, const float eye[3], const float fwd[3]);
static void free_tree(int p);
int inv_slot_item(int s) { return s >= 0 && s < INV_SLOTS && I.slot[s] ? I.pool[I.slot[s]].item & 511 : -1; }
static int find_kind(int p, int item, int depth) {
    for (int guard = 0; p && guard < POOL && depth < 8; guard++, p = I.pool[p].next) {
        if ((I.pool[p].item & 511) == item) return p;
        if (holds(&I.pool[p])) {
            int q = find_kind(I.pool[p].link, item, depth + 1);
            if (q) return q;
        }
    }
    return 0;
}
static void consume(int p);
bool inv_consume_one(int item) {
    for (int s = 0; s < INV_SLOTS; s++) {
        int p = I.slot[s];
        if (!p) continue;
        if ((I.pool[p].item & 511) == item) return consume(p), true;
        if (holds(&I.pool[p])) {
            int q = find_kind(I.pool[p].link, item, 0);
            if (q) return consume(q), true;
        }
    }
    return false;
}
int inv_held_item(int *owner) {
    if (!I.held) return -1;
    if (owner) *owner = I.pool[I.held].owner & 0x3f;
    return I.pool[I.held].item & 511;
}
void inv_held_consume(void) {
    if (!I.held) return;
    Object *o = &I.pool[I.held];
    if (obj_quantity(o) > 1) {
        o->link--;
        inv_put_back();
        return;
    }
    free_tree(I.held);
    I.held = 0;
}
bool inv_drop_held(Level *lv, const float eye[3], const float dir[3]) {
    if (cw_running()) { /* the original's DoSpecialActions(3D view): dropped in front */
        cw_drop(0);
        return true;
    }
    return drop_held(lv, eye, dir);
}
bool inv_give(int item, int quality) {
    int p = alloc();
    if (!p) return false;
    I.pool[p].item = (u16)item, I.pool[p].quality = (u8)quality;
    if (place_new(p)) return true;
    I.used[p] = false;
    return false;
}
void inv_wield(int item) {
    int p = alloc();
    if (!p) return;
    I.pool[p].item = (u16)item, I.pool[p].quality = 40;
    if (I.slot[7] && !place_new(I.slot[7])) return;
    I.slot[7] = p;
}
static int tree_mass(int p);
static int carried(void) {
    int m = 0;
    for (int s = 0; s < INV_SLOTS; s++)
        if (I.slot[s]) m += tree_mass(I.slot[s]);
    return m + (I.held ? tree_mass(I.held) : 0);
}
int inv_carried(void) { return carried(); }

bool inv_save(const char *dir) {
    char path[700];
    snprintf(path, sizeof path, "%s\\inventory.txt", dir);
    size_t cap = 128 * 1024, n = 0, mark = arena_mark();
    char *out = arena_alloc(cap);
    n += (size_t)snprintf(out + n, cap - n, "# what the player carries and how the body is (uo-new save)\r\n");
    n += (size_t)snprintf(out + n, cap - n, "state %d %d %d %d %d %d %.3f %u\r\n", I.hunger, I.fatigue, I.drunk, I.poison, I.tick, I.tick_secs,
                          (double)I.clock, (unsigned)I.rng);
    for (int s = 0; s < INV_SLOTS; s++)
        if (I.slot[s]) n += (size_t)snprintf(out + n, cap - n, "slot %d %d\r\n", s, I.slot[s]);
    if (I.held) n += (size_t)snprintf(out + n, cap - n, "held %d\r\n", I.held);
    for (int i = 1; i < POOL; i++) {
        if (!I.used[i]) continue;
        const Object *o = &I.pool[i];
        n += (size_t)snprintf(out + n, cap - n, "object %d %d %d %d %d %d %d %d %d %d %d%s\r\n", i, o->item, o->flags, o->z, o->heading, o->y,
                              o->x, o->quality, o->next, o->owner, o->link, I.hidden[i] ? " hidden" : "");
    }
    bool ok = file_write(path, out, n);
    arena_reset(mark);
    return ok;
}

bool inv_load(const char *dir) {
    char path[700];
    snprintf(path, sizeof path, "%s\\inventory.txt", dir);
    size_t mark = arena_mark();
    Bytes f = file_read(path);
    if (!f.size) return arena_reset(mark), false;
    memset(I.pool, 0, sizeof I.pool), memset(I.used, 0, sizeof I.used), memset(I.slot, 0, sizeof I.slot), memset(I.hidden, 0, sizeof I.hidden);
    I.held = 0, I.scroll = 0, I.hover = -1;
    for (const char *p = (const char *)f.data; p && p < (const char *)f.data + f.size;) {
        const char *nl = memchr(p, '\n', (size_t)((const char *)f.data + f.size - p));
        int a[11];
        double clk;
        unsigned rng;
        if (sscanf(p, "state %d %d %d %d %d %d %lf %u", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &clk, &rng) == 8)
            I.hunger = a[0], I.fatigue = a[1], I.drunk = a[2], I.poison = a[3], I.tick = (u8)a[4], I.tick_secs = a[5], I.clock = (float)clk,
            I.rng = rng;
        else if (sscanf(p, "slot %d %d", &a[0], &a[1]) == 2 && a[0] >= 0 && a[0] < INV_SLOTS && a[1] > 0 && a[1] < POOL) I.slot[a[0]] = a[1];
        else if (sscanf(p, "held %d", &a[0]) == 1 && a[0] > 0 && a[0] < POOL) I.held = a[0];
        else if (sscanf(p, "object %d %d %d %d %d %d %d %d %d %d %d", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6], &a[7], &a[8], &a[9],
                        &a[10]) == 11 && a[0] > 0 && a[0] < POOL) {
            Object *o = &I.pool[a[0]];
            I.used[a[0]] = true;
            o->item = (u16)a[1], o->flags = (u8)a[2], o->z = (u8)a[3], o->heading = (u8)a[4], o->y = (u8)a[5], o->x = (u8)a[6];
            o->quality = (u8)a[7], o->next = (u16)a[8], o->owner = (u8)a[9], o->link = (u16)a[10];
            const char *end = nl ? nl : (const char *)f.data + f.size;
            for (const char *q = p; q + 6 <= end; q++)
                if (!memcmp(q, "hidden", 6)) I.hidden[a[0]] = true;
        }
        p = nl ? nl + 1 : NULL;
    }
    arena_reset(mark);
    return true;
}
static bool place_new(int p);
void inv_put_back(void) {
    if (I.held && place_new(I.held)) I.held = 0;
}

static bool tree_has_key(int p, int id, int depth) {
    for (int guard = 0; p && guard < POOL && depth < 8; guard++, p = I.pool[p].next) {
        int item = I.pool[p].item & 511;
        if (item >= 0x100 && item <= 0x10e && item != 0x101 && (I.pool[p].owner & 0x3f) == id) return true;
        if (holds(&I.pool[p]) && tree_has_key(I.pool[p].link, id, depth + 1)) return true;
    }
    return false;
}
static int g_capacity = 1 << 30;
void inv_set_capacity(int tenths) { g_capacity = tenths; }
int inv_capacity(void) { return g_capacity; }
static int carried(void);

static int count_in(int p, int item, int depth) {
    int n = 0;
    for (int guard = 0; p && guard < POOL && depth < 8; guard++, p = I.pool[p].next) {
        if ((I.pool[p].item & 511) == item) n += obj_quantity(&I.pool[p]);
        if (holds(&I.pool[p])) n += count_in(I.pool[p].link, item, depth + 1);
    }
    return n;
}
int inv_count(int item) {
    int n = 0;
    for (int s = 0; s < INV_SLOTS; s++) {
        int p = I.slot[s];
        if (!p) continue;
        if ((I.pool[p].item & 511) == item) n += obj_quantity(&I.pool[p]);
        if (holds(&I.pool[p])) n += count_in(I.pool[p].link, item, 0);
    }
    return n;
}

int inv_weapon(void) {
    int p = I.slot[7];
    if (!p) return -1;
    int item = I.pool[p].item & 511;
    return obj_info(item)->weapon ? item : -1;
}
/* what the thing worn in slot s is worth as armour (UW.EXE 0x7e3ed): nothing for a weapon (below 0x20), else its
 * protection x its condition (quality, 0-63) / 64 + 1. Slot 8 counts only a shield (0x3B-0x3F, 0x7e4cc). */
int inv_armour(int s) {
    int p = s >= 0 && s < INV_BODY ? I.slot[s] : 0;
    if (!p) return 0;
    int item = I.pool[p].item & 511;
    if (item < 0x20 || (s == 8 && (item < 0x3b || item > 0x3f))) return 0;
    return obj_info(item)->protection * (I.pool[p].quality & 63) / 64 + 1;
}
int inv_protection(int s) {
    int p = s >= 0 && s < INV_BODY ? I.slot[s] : 0;
    return p ? obj_info(I.pool[p].item)->protection : 0;
}

bool inv_has_key(int id) {
    for (int s = 0; s < INV_SLOTS; s++) {
        int p = I.slot[s];
        if (!p) continue;
        int item = I.pool[p].item & 511;
        if (item >= 0x100 && item <= 0x10e && item != 0x101 && (I.pool[p].owner & 0x3f) == id) return true;
        if (holds(&I.pool[p]) && tree_has_key(I.pool[p].link, id, 0)) return true;
    }
    return false;
}

/* --- the level's objects: which slots are in use ----------------------------------------------------------- */
static void mark(const Level *lv, int i, u8 *used, int depth) {
    for (int guard = 0; i > 0 && i < 1024 && !used[i] && guard < 1024 && depth < 16; guard++) {
        used[i] = 1;
        const Object *o = &lv->obj[i];
        /* the link: contents, a lock, a trigger's trap... (followed even when it is a count: marking an unused
         * slot as used costs nothing, freeing a trap would break the level) */
        if (o->link > 0 && o->link < 1024) mark(lv, o->link, used, depth + 1);
        i = o->next;
    }
}
static u8 g_used[1024];
static void level_used(const Level *lv) {
    memset(g_used, 0, sizeof g_used);
    for (int t = 0; t < 64 * 64; t++) mark(lv, lv->tile[t].first_object, g_used, 0);
}
bool inv_level_free(const Level *lv, int i) {
    level_used(lv);
    return i > 0 && i < 1024 && !g_used[i];
}
int inv_level_alloc(const Level *lv) {
    level_used(lv);
    for (int i = 256; i < 1024; i++)
        if (!g_used[i]) return i;
    return -1;
}
/* a free mobile slot (2-255: creatures; 1 is the player) */
int inv_level_alloc_mobile(const Level *lv) {
    level_used(lv);
    for (int i = 2; i < 256; i++)
        if (!g_used[i] && !(lv->obj[i].item & 511)) return i;
    return -1;
}

/* the level -> the pool: object i with what it holds (the level's copies are cleared) */
static int copy_in(Level *lv, int i, int depth) {
    int p = alloc();
    if (!p) return 0;
    I.pool[p] = lv->obj[i];
    I.pool[p].next = 0;
    if (lv_carries(lv, i) && depth < 8) {
        int last = 0;
        I.pool[p].link = 0;
        for (int c = lv->obj[i].link, guard = 0; c > 0 && c < 1024 && guard < 256; guard++) {
            int nx = lv->obj[c].next, q = copy_in(lv, c, depth + 1);
            if (q) {
                if (last) I.pool[last].next = (u16)q;
                else I.pool[p].link = (u16)q;
                last = q;
            }
            c = nx;
        }
    }
    memset(&lv->obj[i], 0, sizeof lv->obj[i]);
    return p;
}
static int count_tree(const Level *lv, int i, int depth) {
    int n = 1;
    if (lv_carries(lv, i) && depth < 8)
        for (int c = lv->obj[i].link, guard = 0; c > 0 && c < 1024 && guard < 256; c = lv->obj[c].next, guard++) n += count_tree(lv, c, depth + 1);
    return n;
}

/* the pool -> the level: thing p with what it holds into free static slots (g_used: worked out once by the
 * caller; each slot taken is marked at once, so the contents never get their container's); its level
 * number, or -1 */
static int copy_out_rec(Level *lv, int p, int depth) {
    int i = -1;
    for (int k = 256; k < 1024 && i < 0; k++)
        if (!g_used[k]) i = k;
    if (i < 0) return -1;
    g_used[i] = 1;
    lv->obj[i] = I.pool[p];
    lv->obj[i].next = 0;
    if (pool_carries(p) && depth < 8) {
        int last = 0;
        lv->obj[i].link = 0;
        for (int c = I.pool[p].link, guard = 0; c && guard < POOL; c = I.pool[c].next, guard++) {
            int q = copy_out_rec(lv, c, depth + 1);
            if (q < 0) break;
            if (last) lv->obj[last].next = (u16)q;
            else lv->obj[i].link = (u16)q;
            last = q;
        }
    }
    return i;
}
static int copy_out(Level *lv, int p) {
    level_used(lv);
    return copy_out_rec(lv, p, 0);
}

/* --- where a thing can go -------------------------------------------------------------------------------- */
/* may thing p sit in slot s (body slots by the armour table; the shoulders and hands take anything) */
static bool slot_ok(int s, int p) {
    if (s >= INV_BODY || (s >= 5 && s <= 8)) return true;
    int a = obj_info(I.pool[p].item)->armour;
    if (s == 9 || s == 10) return a == 9;
    return a == s;
}

/* into container c (at the front of its contents) when it fits: else false, with the message */
static bool put_in(int c, int p, bool say) {
    char msg[128];
    if (c == p) return false;
    if (!obj_fits(I.pool[c].item, &I.pool[p], contents_mass(c), msg, sizeof msg)) {
        if (say && msg[0]) game_message("%s", msg);
        return false;
    }
    I.pool[p].next = I.pool[c].link;
    I.pool[c].link = (u16)p;
    return true;
}

/* takes p out of the container it is in (if any) */
static bool unlink_from(int c, int p) {
    u16 *at = &I.pool[c].link;
    for (int guard = 0; *at && guard < POOL; guard++) {
        if (*at == p) {
            *at = I.pool[p].next;
            I.pool[p].next = 0;
            return true;
        }
        if (holds(&I.pool[*at]) && unlink_from(*at, p)) return true;
        at = &I.pool[*at].next;
    }
    return false;
}

/* a new thing p: a free pack hole, else into the pack's containers (in order) */
static bool place_new(int p) {
    for (int s = INV_BODY; s < INV_SLOTS; s++)
        if (!I.slot[s]) return I.slot[s] = p, true;
    for (int s = INV_BODY; s < INV_SLOTS; s++)
        if (I.slot[s] && obj_info(I.pool[I.slot[s]].item)->container && put_in(I.slot[s], p, false)) return true;
    return false;
}

bool inv_take(Level *lv, int tx, int tz, int i) {
    if (cw_running()) return cw_take(i, tz * 64 + tx); /* the original's player_3dget */
    const Object *o = &lv->obj[i];
    char name[96];
    obj_name(o, name, sizeof name);
    if (!obj_info(o->item)->pick) {
        game_message("You cannot pick that up.");
        return false;
    }
    int free = 0;
    for (int k = 1; k < POOL; k++) free += !I.used[k];
    if (count_tree(lv, i, 0) > free) return game_message("%s", "That is too heavy to take."), false;
    /* the carry limit (strength x 2 stones): "That is too heavy for you to pick up." (strings 1:95) */
    {
        int m = obj_mass(o);
        for (int c = (o->flags & 0x40) ? 0 : o->link, guard = 0; obj_info(o->item)->container && c > 0 && c < 1024 && guard < 256; c = lv->obj[c].next, guard++)
            m += obj_mass(&lv->obj[c]);
        if (carried() + m > g_capacity) return game_message("%s", str_get(1, 95)), false;
    }
    /* is there a place: a free pack hole or a container it fits in */
    bool room = false;
    for (int s = INV_BODY; s < INV_SLOTS && !room; s++) room = !I.slot[s];
    if (!room) {
        char msg[128];
        for (int s = INV_BODY; s < INV_SLOTS && !room; s++)
            if (I.slot[s] && obj_info(I.pool[I.slot[s]].item)->container) room = obj_fits(I.pool[I.slot[s]].item, o, contents_mass(I.slot[s]), msg, sizeof msg);
    }
    if (!room) return game_message("There is no place to put that."), false;
    /* out of the tile's chain */
    Tile *t = &lv->tile[tz * 64 + tx];
    if (t->first_object == i) t->first_object = lv->obj[i].next;
    else
        for (int j = t->first_object, guard = 0; j > 0 && j < 1024 && guard < 1024; j = lv->obj[j].next, guard++)
            if (lv->obj[j].next == i) {
                lv->obj[j].next = lv->obj[i].next;
                break;
            }
    int p = copy_in(lv, i, 0);
    place_new(p);
    game_message("You pick up %s.", name);
    return true;
}

/* the held thing put down in the world, a little ahead of the player */
static bool drop_held(Level *lv, const float eye[3], const float fwd[3]) {
    if (!I.held) return false;
    float x = eye[0] + fwd[0] * 0.5f, z = eye[2] + fwd[2] * 0.5f;
    if (level_solid_at(lv, x, z)) x = eye[0], z = eye[2];
    int tx = (int)x, tz = (int)z;
    int i = copy_out(lv, I.held);
    if (i < 0) return game_message("There is no space to drop that."), sound_effect(0xf, 0x40, -10), true; /* (MISSILE.C) */
    Object *o = &lv->obj[i];
    o->x = (u8)MIN(7, (int)((x - tx) * 8)), o->y = (u8)MIN(7, (int)((z - tz) * 8));
    o->z = (u8)MIN(127, (int)(level_floor_y(lv, x, z) * 32 + 0.5f));
    Tile *t = &lv->tile[tz * 64 + tx];
    o->next = t->first_object;
    t->first_object = (u16)i;
    char name[96];
    obj_name(o, name, sizeof name);
    game_message("You drop %s.", name);
    free_tree(I.held);
    I.held = 0;
    return true;
}

/* --- using things ---------------------------------------------------------------------------------------- */
static int slot_of(int p) {
    for (int s = 0; s < INV_SLOTS; s++)
        if (I.slot[s] == p) return s;
    return -1;
}

/* one of thing p is gone (eaten, drunk): a count goes down, else it leaves the inventory */
static void consume(int p) {
    Object *o = &I.pool[p];
    if (obj_quantity(o) > 1) {
        o->link--;
        return;
    }
    for (int s = 0; s < INV_SLOTS; s++)
        if (I.slot[s] == p) I.slot[s] = 0;
    for (int s = 0; s < INV_SLOTS; s++)
        if (I.slot[s] && holds(&I.pool[I.slot[s]])) unlink_from(I.slot[s], p);
    if (I.held == p) I.held = 0;
    free_tree(p);
}

/* Eating and drinking (UW.EXE file 0x374ee): -1 not food, 0 not eaten, 1 eaten. Food (0xB0-0xBF) adds its
 * nutrition to hunger (refused past 255: "You are too full to eat that now.") and says how it tasted (block 1,
 * 172 + (0-19 + quality) / 16, up to 4); the special ones say their own: leeches 229, candle 230, toadstool
 * 231 (poisons), mushroom 232, plants 233/236 (nutrition 23/4), rotworm 234 (4), stew 235 (64), water 237,
 * port 238, ale 239, potions 240. A drink below -1 (ale, port) makes drunk; wine: "You are unable to open the
 * wine bottle." */
static int eat(int p) {
    Object *o = &I.pool[p];
    int item = o->item & 511, say = 0, food = 255;
    if ((item >> 4) == 0xb) food = obj_info(item)->nutrition;
    switch (item) {
    case 0x125: say = 229; break;
    case 0x92: say = 230; break;
    case 0xb9: say = 231; break;
    case 0xb8: say = 232; break;
    case 0xcf: say = 233, food = 23; break;
    case 0xd9: say = 234, food = 4; break;
    case 0x11b: say = 235, food = 64; break;
    case 0xce: say = 236, food = 4; break;
    case 0xbd: say = 237; break;
    case 0xbe: say = 238; break;
    case 0xba: say = 239; break;
    case 0xbb: case 0xbc: say = 240; break;
    case 0xbf: return game_message("%s", str_get(1, 127)), 0;
    default:
        if (food == 255) return -1;
    }
    if (food > 0 && food != 255) {
        if (I.hunger + food > 255) return game_message("%s", str_get(1, 126)), 0;
        I.hunger += food;
    }
    if (!say) {
        char noun[64];
        obj_noun(o, noun, sizeof noun);
        game_message("The %s%s", noun, str_get(1, 172 + MIN(4, (rnd(20) + o->quality) >> 4)));
    } else game_message("%s", str_get(1, say));
    if (item == 0xbb || item == 0xbc) { /* a potion: its enchantment */
        int c, v;
        if (enchant_of(o, &c, &v)) game_enchant_effect(c, v);
    }
    if (item == 0xb9) I.poison = I.poison < 4 ? 4 : MIN(15, I.poison + 2);
    if (food < -1 && food > -127) I.drunk = MIN(63, I.drunk - food);
    return 1;
}

/* Reading (UW.EXE file 0x37fa0): books and scrolls 0x130-0x13F; the map (0x13B) opens the map; an enchanted
 * one (flags bit 3) casts its spell and one with flags bit 1 has its own text (both: later); else "You read
 * the scroll..." and strings block 3 [link & 0x1ff] (when below 0x100) */
static bool read_thing(int p) {
    const Object *o = &I.pool[p];
    int item = o->item & 511;
    if (item < 0x130 || item > 0x13f) return false;
    if (item == 0x13b) return game_message("The map will come later."), true;
    if (o->flags & 0x08) { /* an enchanted scroll: its spell, once */
        int c, v;
        if (enchant_of(o, &c, &v) && game_enchant_effect(c, v)) consume(p);
        else game_message("%s", str_get(1, 213));
        return true;
    }
    int n = o->link & 0x1ff;
    char noun[64], title[96];
    obj_noun(o, noun, sizeof noun);
    snprintf(title, sizeof title, "You read the %s...", noun);
    if ((o->flags & 0x02) || n >= 0x100 || !str_get(3, n)[0]) {
        game_read(title, str_get(3, 0)); /* "The pages are blank." */
        return true;
    }
    game_message("%s", title);
    game_read(title, str_get(3, n));
    return true;
}

static void use_thing(int p) {
    Object *o = &I.pool[p];
    char name[96];
    obj_name(o, name, sizeof name);
    if (is_light(o->item)) {
        int s = slot_of(p);
        if (s < 5 || s > 8) return game_message("Lights may only be used if equipped.");
        if (is_lit(o->item)) {
            o->item -= LIT;
            obj_name(o, name, sizeof name);
            return game_message("You put out %s.", name);
        }
        if (o->quality == 0) return game_message("That light is already used up.");
        o->item += LIT;
        obj_name(o, name, sizeof name);
        return game_message("You light %s.", name);
    }
    if (obj_info(o->item)->container) {
        I.hidden[p] = !I.hidden[p]; /* shown in the grid, or not */
        return;
    }
    if (read_thing(p)) return;
    if ((o->item & 511) == 0x121) return game_sleep(1); /* a bedroll (UW.EXE 0x37cd2) */
    if ((o->item & 511) == 0x98) { /* a wand (0x38d20): its spell object's charges */
        int c, v;
        game_message("%s", str_get(1, 215)); /* "You attempt to use the wand." */
        Object *sp = (Object *)enchant_src(o, true);
        if (!sp || !enchant_of(o, &c, &v)) return game_message("%s", str_get(1, 132));
        if ((sp->quality & 63) == 0) {
            if (rnd(10) < 4) game_message("%s", str_get(1, 125)), consume(p); /* "With a loud <SNAP!>, the wand cracks." */
            else game_message("%s", str_get(1, 132));
            return;
        }
        sp->quality = (u8)((sp->quality & ~63) | ((sp->quality & 63) - 1));
        game_enchant_effect(c, v);
        return;
    }
    if ((o->item & 511) == 0x12b) return game_fish();                      /* a fishing pole (0x37d8e) */
    if ((o->item & 511) == 0x123 || (o->item & 511) == 0x124) return game_play(); /* a mandolin, a flute (0x37ce9) */
    if ((o->item & 511) == 0x125) { /* leeches (0x37cfe): the poison out, with some skin and blood */
        if (game_leeches()) consume(p);
        return;
    }
    if ((o->item & 511) == 0x122) { /* the silver seed (0x37d45): planted where the player stands */
        if (game_plant_seed()) consume(p);
        return;
    }
    int eaten = eat(p);
    if (eaten > 0) consume(p);
    if (eaten >= 0) return;
    game_message("You cannot use %s yet.", name);
}

void inv_toggle_light(void) {
    static const int ORDER[4] = {7, 8, 5, 6};
    for (int k = 0; k < 4; k++) {
        int p = I.slot[ORDER[k]];
        if (p && is_light(I.pool[p].item) && (is_lit(I.pool[p].item) || I.pool[p].quality)) return use_thing(p);
    }
    /* none in the hands or shoulders: a light from the pack goes to a free one */
    for (int s = INV_BODY; s < INV_SLOTS; s++) {
        int p = I.slot[s];
        if (!p || !is_light(I.pool[p].item) || !I.pool[p].quality) continue;
        for (int k = 0; k < 4; k++)
            if (!I.slot[ORDER[k]]) {
                I.slot[ORDER[k]] = p, I.slot[s] = 0;
                return use_thing(p);
            }
        return game_message("Your hands are full.");
    }
    game_message("Thou hast no light.");
}

/* --- the panel ----------------------------------------------------------------------------------------------
 * A grey board at the right, the height of the screen. At the top the body: the figure, and round it the worn
 * things' holes, each with its name in the background and the thing over it. Below, one grid of all that is
 * carried, as in World of Warcraft's combined bags: first the pack's own holes (the 8 of the original, less the
 * bags), then the contents of each bag that is shown (each from a new row, with room to drop more in). At the
 * bottom the bag bar: the bags in the pack; a left click shows or hides a bag's part of the grid. */
#define PW 188
#define BODY 29  /* a body hole (the holes touch: no gaps) */
#define CELL 22  /* a grid hole */
#define PITCH 22 /* a grid hole's step: the holes touch */
#define COLS 7
enum { C_BODY, C_PACK, C_BAG, C_BAR, C_UP, C_DOWN, C_CHECK };
static bool g_with_sheet = true; /* the checkbox: the character panel opens with the pack (Tab) */
bool inv_with_sheet(void) { return g_with_sheet; }
void inv_set_with_sheet(bool on) { g_with_sheet = on; }
typedef struct {
    int x, y, w, h, kind, slot, bag, thing;
} Cell;
static Cell g_cell[512];
static int g_ncell, g_grid_rows, g_scroll_max;

static void panel_origin(int *x, int *y) {
    Overlay *o = ui_overlay();
    *x = o->w - PW - 4, *y = 4;
}
static int panel_h(void) { return ui_overlay()->h - 54; } /* above the flasks */
static void cell_add(int x, int y, int w, int h, int kind, int slot, int bag, int thing) {
    if (g_ncell >= COUNT(g_cell)) return;
    g_cell[g_ncell++] = (Cell){x, y, w, h, kind, slot, bag, thing};
}
static bool is_bag(int p) { return p && obj_info(I.pool[p].item)->container; }

/* the grid's rows: rows[r][c] = (kind, bag, thing); each section from a new row */
typedef struct {
    int kind, slot, bag, thing;
} GridItem;
static GridItem g_grid[64][COLS];
static int g_rows;
static void grid_section_bag(int b, int depth) {
    int c = 0;
    for (int q = I.pool[b].link, guard = 0; q && guard < POOL && g_rows < 64; q = I.pool[q].next, guard++) {
        g_grid[g_rows][c] = (GridItem){C_BAG, -1, b, q};
        if (++c == COLS) c = 0, g_rows++;
    }
    /* room to drop more in: the rest of the row (a whole row when the last one is full) */
    if (g_rows < 64) {
        do g_grid[g_rows][c] = (GridItem){C_BAG, -1, b, 0};
        while (++c < COLS);
        g_rows++;
    }
    /* bags in this bag that are shown: their own sections after */
    for (int q = I.pool[b].link, guard = 0; q && guard < POOL && depth < 4; q = I.pool[q].next, guard++)
        if (is_bag(q) && !I.hidden[q]) grid_section_bag(q, depth + 1);
}
static void grid_build(void) {
    g_rows = 0;
    memset(g_grid, 0, sizeof g_grid);
    int c = 0;
    for (int s = INV_BODY; s < INV_SLOTS; s++) {
        if (is_bag(I.slot[s])) continue; /* the bags are in the bar */
        g_grid[g_rows][c] = (GridItem){C_PACK, s, 0, I.slot[s]};
        if (++c == COLS) c = 0, g_rows++;
    }
    if (c) {
        while (c < COLS) g_grid[g_rows][c++] = (GridItem){-1, 0, 0, 0};
        g_rows++;
    }
    for (int s = INV_BODY; s < INV_SLOTS; s++)
        if (is_bag(I.slot[s]) && !I.hidden[I.slot[s]]) grid_section_bag(I.slot[s], 0);
}

/* a bag and the bags in it (to 4 deep), in order */
static void bags_in(int b, int *out, int *n, int depth) {
    if (*n < 32) out[(*n)++] = b;
    for (int q = I.pool[b].link, guard = 0; q && guard < POOL && depth < 4; q = I.pool[q].next, guard++)
        if (is_bag(q)) bags_in(q, out, n, depth + 1);
}

/* the cells of the panel this frame */
static void cells_build(void) {
    int px, py, ph = panel_h();
    panel_origin(&px, &py);
    g_ncell = 0;
    /* the body: outer columns shoulders, hands, rings; inner left helm, chest, hands; inner right legs, feet */
    static const int COL[INV_BODY] = {1, 1, 1, 3, 3, 0, 4, 0, 4, 0, 4};
    static const int ROW[INV_BODY] = {0, 1, 2, 1, 2, 0, 0, 1, 1, 2, 2};
    static const int CX[5] = {4, 4 + BODY, 0, 116, 116 + BODY};
    for (int s = 0; s < INV_BODY; s++)
        cell_add(px + CX[COL[s]], py + 6 + ROW[s] * BODY, BODY, BODY, C_BODY, s, 0, I.slot[s]);
    /* the bag bar at the bottom, the grid between; the bar has every bag, those in bags too (each can be shown,
     * hidden and filled) */
    int bar_y = py + ph - 44, grid_y = py + 6 + 3 * BODY + 6;
    int bags[32], nbags = 0;
    for (int s = INV_BODY; s < INV_SLOTS; s++)
        if (is_bag(I.slot[s])) bags_in(I.slot[s], bags, &nbags, 0);
    int bx = px + 4;
    for (int k = 0; k < nbags && bx + CELL <= px + PW - 36; k++) cell_add(bx, bar_y, CELL, CELL, C_BAR, -1, bags[k], bags[k]), bx += PITCH;
    cell_add(px + PW - 74, py + ph - 16, 70, 12, C_CHECK, 0, 0, 0); /* "[x] Character" on the weight's line */
    grid_build();
    g_grid_rows = MAX(1, (bar_y - 14 - grid_y) / PITCH); /* (above the bags line) */
    g_scroll_max = MAX(0, g_rows - g_grid_rows);
    I.scroll = MIN(I.scroll, g_scroll_max);
    for (int r = 0; r < g_grid_rows && r + I.scroll < g_rows; r++)
        for (int c = 0; c < COLS; c++) {
            const GridItem *g = &g_grid[r + I.scroll][c];
            if (g->kind < 0) continue;
            cell_add(px + 4 + c * PITCH, grid_y + r * PITCH, CELL, CELL, g->kind, g->slot, g->bag, g->thing);
        }
    if (g_scroll_max) { /* scroll arrows at the right of the bag bar */
        cell_add(px + PW - 34, bar_y + 4, 14, 16, C_UP, 0, 0, 0);
        cell_add(px + PW - 18, bar_y + 4, 14, 16, C_DOWN, 0, 0, 0);
    }
}
static int cell_at(int mx, int my) {
    for (int k = 0; k < g_ncell; k++)
        if (mx >= g_cell[k].x && mx < g_cell[k].x + g_cell[k].w && my >= g_cell[k].y && my < g_cell[k].y + g_cell[k].h) return k;
    return -1;
}

bool inv_mouse_over(int x, int y) {
    int px, py;
    panel_origin(&px, &py);
    return x >= px && x < px + PW && y >= py && y < py + panel_h();
}

/* where a thing goes on the body: armour by the armour table, rings a ring (the free one), weapons the right
 * hand (0x00-0x0F hand weapons, 0x18-0x1F bows and slings), shields the left, lights a free shoulder; -1 none */
static int body_slot_for(int p) {
    int item = I.pool[p].item & 511, a = obj_info(item)->armour;
    if (a == 9) return !I.slot[9] ? 9 : !I.slot[10] ? 10 : 9;
    if (a >= 0) return a;
    if (item < 0x10 || (item >= 0x18 && item < 0x20)) return 7;
    if (item >= 0x37 && item <= 0x3f && obj_info(item)->armour < 0) return 8; /* shields */
    if (is_light(item)) return !I.slot[5] ? 5 : !I.slot[6] ? 6 : 5;
    return -1;
}

static const char *const WHERE[INV_BODY] = {"on thy head", "on thy body", "on thy hands", "on thy legs", "on thy feet",
                                            "on thy right shoulder", "on thy left shoulder", "in thy right hand",
                                            "in thy left hand", "on thy right hand", "on thy left hand"};

/* the held thing onto the body; what was in that slot comes onto the pointer */
static void equip_held(void) {
    int s = body_slot_for(I.held);
    char name[96];
    obj_name(&I.pool[I.held], name, sizeof name);
    if (s < 0) return game_message("You cannot wear %s.", name);
    int old = I.slot[s];
    I.slot[s] = I.held, I.held = old;
    game_message("You put %s %s.", name, WHERE[s]);
}

/* the link that holds p inside container c (its contents, and bags in it), or NULL */
static u16 *link_to(int c, int p, int depth) {
    u16 *at = &I.pool[c].link;
    for (int guard = 0; *at && guard < POOL && depth < 8; guard++) {
        if (*at == p) return at;
        if (holds(&I.pool[*at])) {
            u16 *in = link_to(*at, p, depth + 1);
            if (in) return in;
        }
        at = &I.pool[*at].next;
    }
    return NULL;
}

/* a wearable thing in the pack or in a bag (armour, a ring, a weapon, a shield) onto the body; what it
 * replaces goes exactly where it was (the same hole, the same place in the bag) */
static bool wear_from_pack(int p) {
    int s = body_slot_for(p);
    if (s < 0) return false;
    int old = I.slot[s], k = slot_of(p);
    if (k >= INV_BODY) I.slot[k] = old;
    else {
        u16 *at = NULL;
        for (int b = INV_BODY; b < INV_SLOTS && !at; b++)
            if (I.slot[b] && holds(&I.pool[I.slot[b]])) at = link_to(I.slot[b], p, 0);
        if (!at) return false;
        if (old) I.pool[old].next = I.pool[p].next, *at = (u16)old;
        else *at = I.pool[p].next;
        I.pool[p].next = 0;
    }
    I.slot[s] = p;
    char name[96];
    obj_name(&I.pool[p], name, sizeof name);
    game_message("You put %s %s.", name, WHERE[s]);
    return true;
}

/* takes p out of wherever it is (a slot or a container) */
static void take_out(int p) {
    for (int s = 0; s < INV_SLOTS; s++)
        if (I.slot[s] == p) I.slot[s] = 0;
    for (int s = 0; s < INV_SLOTS; s++)
        if (I.slot[s] && holds(&I.pool[I.slot[s]])) unlink_from(I.slot[s], p);
}

/* Right click never drops. On something worn or held (the body): off to the pack (a free hole, else a bag it
 * fits in). On a bag: shows or hides it in the grid. On something usable (a light, food, drink, a book, a
 * scroll): used. On something wearable (armour, a ring, a weapon, a shield): worn, and what it replaces takes
 * its place. On anything else: into the first bag in the pack that takes it (not the one it is in). */
static void right_click(int p) {
    int s = slot_of(p);
    const Object *o = &I.pool[p];
    char name[96];
    obj_name(o, name, sizeof name);
    if (s >= 0 && s < INV_BODY) {
        if (is_light(o->item) && s >= 5 && s <= 8) return use_thing(p); /* a light in a hand: light / put out */
        take_out(p);
        if (place_new(p)) return game_message("You put %s in thy pack.", name);
        I.slot[s] = p;
        return game_message("There is no place to put that.");
    }
    int item = o->item & 511;
    if (obj_info(item)->container) {
        I.hidden[p] = !I.hidden[p];
        return;
    }
    if (is_light(item) || (item >> 4) == 0xb || (item >= 0x130 && item <= 0x13f) || item == 0x92 || item == 0xce || item == 0xcf ||
        item == 0xd9 || item == 0x11b || item == 0x125 || item == 0x121 || item == 0x122 || item == 0x12b || item == 0x123 || item == 0x124 ||
        item == 0x98)
        return use_thing(p);
    if (wear_from_pack(p)) return;
    for (int k = INV_BODY; k < INV_SLOTS; k++) {
        int c = I.slot[k];
        if (!is_bag(c) || c == p) continue;
        bool inside = false; /* already in this one */
        for (int q = I.pool[c].link, guard = 0; q && guard < POOL && !inside; q = I.pool[q].next, guard++) inside = q == p;
        if (inside) continue;
        char msg[128];
        if (!obj_fits(I.pool[c].item, o, contents_mass(c), msg, sizeof msg)) continue;
        take_out(p);
        put_in(c, p, false);
        char bag[64];
        obj_noun(&I.pool[c], bag, sizeof bag);
        return game_message("You put %s in the %s.", name, bag);
    }
}

/* the held thing onto thing `under` of the same kind: one stack (true) */
static bool stack_onto(int under) {
    Object *a = &I.pool[under], *b = &I.pool[I.held];
    if (a->item != b->item || !obj_stacks(a->item) || a->quality != b->quality) return false;
    int n = obj_quantity(a) + obj_quantity(b);
    if (n >= 0x200) return false;
    a->flags |= 0x40, a->link = (u16)n;
    free_tree(I.held);
    I.held = 0;
    return true;
}

bool inv_update_core(const InvInput *in);
bool inv_update(Level *lv, const InvInput *in, const float eye[3], const float fwd[3]) {
    if (cw_running()) return inv_update_core(in); /* the original's panel (the rules core) */
    I.mouse_x = in->mouse_x, I.mouse_y = in->mouse_y, I.shift = in->shift;
    cells_build();
    int k = cell_at(in->mouse_x, in->mouse_y);
    I.hover = k;
    const Cell *c = k >= 0 ? &g_cell[k] : NULL;
    int under = c ? c->thing : 0;
    if (in->look && under) {
        char line[300];
        obj_describe(&I.pool[under], line, sizeof line);
        game_message("%s", line);
    }
    if (!inv_mouse_over(in->mouse_x, in->mouse_y)) return false;
    /* the figure: how the player is; a thing dropped on it is worn where it belongs */
    int px, py;
    panel_origin(&px, &py);
    int fx = px + (PW - 36) / 2, fy = py + 6;
    bool on_figure = in->mouse_x >= fx && in->mouse_x < fx + 36 && in->mouse_y >= fy && in->mouse_y < fy + 69;
    if (on_figure && in->click_left && I.held) return equip_held(), true;
    if (on_figure && (in->click_left || in->click_right) && !I.held) {
        game_status();
        return true;
    }
    if (!c) return true;
    if (c->kind == C_UP && in->click_left) return I.scroll = MAX(0, I.scroll - 1), true;
    if (c->kind == C_DOWN && in->click_left) return I.scroll = MIN(g_scroll_max, I.scroll + 1), true;
    if (in->click_right && under) {
        if (in->shift) { /* shift + right click: put it down in the world */
            take_out(under);
            I.held = under;
            drop_held(lv, eye, fwd);
            return true;
        }
        right_click(under);
        return true;
    }
    if (!in->click_left) return true;
    if (c->kind == C_CHECK) return g_with_sheet = !g_with_sheet, true;
    if (c->kind == C_BAR) { /* a bag in the bar: something held goes in; shift picks the bag up; else show / hide */
        if (I.held) {
            if (put_in(c->bag, I.held, true)) I.held = 0;
        } else if (in->shift) take_out(c->bag), I.held = c->bag;
        else I.hidden[c->bag] = !I.hidden[c->bag];
        return true;
    }
    if (!I.held) { /* pick up what is in the hole */
        if (!under) return true;
        take_out(under);
        I.held = under;
        return true;
    }
    /* the held thing used on the thing here (oil on a lantern, the anvil on a sword: UW.EXE's "use ... on") */
    if (under && under != I.held) {
        int r = game_use_on_item(&I.pool[I.held], &I.pool[under]);
        if (r == 2) {
            inv_held_consume();
            return true;
        }
        if (r == 1) {
            inv_put_back();
            return true;
        }
        if (r == 3) { /* the target is gone (destroyed) */
            take_out(under), free_tree(under);
            inv_put_back();
            return true;
        }
    }
    /* put the held thing down here */
    if (under && is_bag(under) && under != I.held) {
        if (put_in(under, I.held, true)) I.held = 0;
        return true;
    }
    if (under && stack_onto(under)) return true;
    if (c->kind == C_BAG) { /* into this bag */
        if (put_in(c->bag, I.held, true)) I.held = 0;
        return true;
    }
    if (c->kind == C_BODY && !slot_ok(c->slot, I.held)) return game_message("That does not go there."), true;
    if (under) take_out(under); /* swap */
    I.slot[c->slot] = I.held, I.held = under;
    return true;
}

/* a hole: sunk into the parchment (shadow above and left, light below and right) */
static void draw_hole(int x, int y, int w, int h, bool hot) { ui_inset(x, y, w, h, hot); }
/* a thing in a hole of size w (its picture twice the size when that fits), with its count */
static void draw_thing_in(int p, int x, int y, int w) {
    const Object *o = &I.pool[p];
    const Pic *pic = ui_pic("sprites\\objects", o->item & 511);
    if (pic) {
        int s = pic->w * 2 <= w - 2 && pic->h * 2 <= w - 2 ? 2 : 1;
        ui_draw(pic, x + (w - pic->w * s) / 2, y + (w - pic->h * s) / 2, s);
    }
    int q = obj_quantity(o);
    if (q > 1) {
        char t[8];
        snprintf(t, sizeof t, "%d", q);
        ui_text(FONT_NORMAL, t, x + w - 2 - ui_text_width(FONT_NORMAL, t), y + w - 8, 255, 240, 200);
    }
}
static void draw_thing(int p, int x, int y) { draw_thing_in(p, x, y, CELL); }

/* --- tooltips ------------------------------------------------------------------------------------------------
 * As in World of Warcraft: the name, its condition, where it is worn, what it does (damage, protection), its
 * durability, weight and value; for something that can be worn, what changes if it is put on in place of what is
 * there ("If you equip this:", gains green, losses red). With shift, the worn one's tooltip beside it, each
 * showing its differences from the other. Not while something is held. */
typedef struct {
    char text[64];
    u8 r, g, b;
    bool small;
} TipLine;
typedef struct {
    TipLine l[24];
    int n;
} Tip;
static void tip_add(Tip *t, bool small, u8 r, u8 g, u8 b, const char *fmt, ...) __attribute__((format(printf, 6, 7)));
static void tip_add(Tip *t, bool small, u8 r, u8 g, u8 b, const char *fmt, ...) {
    if (t->n >= 24) return;
    TipLine *l = &t->l[t->n++];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(l->text, sizeof l->text, fmt, ap);
    va_end(ap);
    l->r = r, l->g = g, l->b = b, l->small = small;
}

/* the numbers compared: slash, bash, stab, protection, durability */
typedef struct {
    int v[5];
} Stats;
static const char *const STAT_NAME[5] = {"Slash damage", "Bash damage", "Thrust damage", "Protection", "Durability"};
static Stats stats_of(int p) {
    Stats s = {{0}};
    const ObjInfo *in = obj_info(I.pool[p].item);
    if (in->weapon) s.v[0] = in->slash, s.v[1] = in->bash, s.v[2] = in->stab, s.v[4] = in->wdurability == 255 ? 0 : in->wdurability;
    if (in->armour >= 0 || (I.pool[p].item >= 0x37 && I.pool[p].item <= 0x3f)) s.v[3] = in->protection, s.v[4] = in->durability == 255 ? 0 : in->durability;
    return s;
}

static void tip_build(Tip *t, int p, int other, const char *header) {
    const Object *o = &I.pool[p];
    const ObjInfo *in = obj_info(o->item);
    int item = o->item & 511;
    char name[96];
    obj_name(o, name, sizeof name);
    t->n = 0;
    if (header) tip_add(t, false, 150, 150, 150, "%s", header);
    if (name[0] >= 'a' && name[0] <= 'z') name[0] = (char)(name[0] - 32);
    bool magic = (o->flags & 0x08) != 0; /* enchanted (item word bit 12) */
    tip_add(t, false, magic ? 190 : 255, magic ? 130 : 255, magic ? 255 : 255, "%s", name);
    const char *ench = inv_enchant_name(p);
    if (ench[0]) tip_add(t, false, 190, 130, 255, "of %s", ench);
    if (item >= 0x98 && item <= 0x9b && enchant_src(o, true) && enchant_src(o, true) != o) /* a wand: its spell object's charges */
        tip_add(t, false, 190, 130, 255, "%d charge%s", enchant_src(o, true)->quality & 63, (enchant_src(o, true)->quality & 63) == 1 ? "" : "s");
    /* its condition: the look's quality word */
    int bucket = o->quality ? (in->qclass == 3 ? 5 : (o->quality >> 4) + 1) : 0;
    const char *word = str_get(5, in->qtype * 6 + bucket);
    if (word[0] && in->qtype != 15) tip_add(t, false, 255, 210, 90, "%c%s", word[0] >= 'a' ? word[0] - 32 : word[0], word + 1);
    /* where it goes and what it does */
    static const char *const SLOT[11] = {"Head", "Chest", "Hands", "Legs", "Feet", "Shoulder", "Shoulder", "Weapon hand", "Off hand", "Finger", "Finger"};
    int s = slot_of(p) >= 0 && slot_of(p) < INV_BODY ? slot_of(p) : body_slot_for(p); /* worn: where it is */
    static const int SKILL[7] = {0, 0, 0, 34, 35, 36, 33}; /* block 2: Sword, Axe, Mace, Unarmed */
    if (in->weapon) {
        tip_add(t, false, 255, 255, 255, "%s   %s", SLOT[7], in->skill < 7 && SKILL[in->skill] ? str_get(2, SKILL[in->skill]) : "");
        tip_add(t, false, 255, 255, 255, "Slash %d   Bash %d   Thrust %d", in->slash, in->bash, in->stab);
    } else if (in->ammo) {
        char ammo[64];
        Object a = {0};
        a.item = (u16)in->ammo;
        obj_noun(&a, ammo, sizeof ammo);
        tip_add(t, false, 255, 255, 255, "Ranged weapon   uses %s", ammo);
    } else if (s >= 0) tip_add(t, false, 255, 255, 255, "%s", SLOT[s]);
    if (in->armour >= 0 || (item >= 0x37 && item <= 0x3f)) tip_add(t, false, 255, 255, 255, "%d Protection", in->protection);
    /* what it means for this character (the original's numbers: UW.EXE 0x25406, 0x7e3ed, 0x7e444) */
    if (in->weapon) {
        int k = in->skill >= 3 && in->skill <= 5 ? in->skill : 2; /* sword, axe, mace; else unarmed */
        int score = game_skill(0) / 2 + game_skill(k) + game_attr(1) / 7 + (game_easy() ? 7 : 0);
        tip_add(t, false, 120, 200, 255, "%s skill: yours %d", str_get(2, 31 + k), game_skill(k));
        tip_add(t, false, 120, 200, 255, "Attack score %d   +%d damage (strength)", score, game_attr(0) / 9);
        tip_add(t, false, 120, 200, 255, "Power %d-%d, charges %d a 1/16 s", in->power_min, in->power_max, in->rate);
    } else if (in->ammo) tip_add(t, false, 120, 200, 255, "%s skill: yours %d", str_get(2, 31 + 6), game_skill(6));
    else if (in->armour >= 0 || (item >= 0x3b && item <= 0x3f)) {
        static const char *const PART[5] = {"head", "body", "arms", "legs", "legs"};
        int a = item < 0x20 ? 0 : in->protection * (o->quality & 63) / 64 + 1;
        if (item >= 0x3b && item <= 0x3f) tip_add(t, false, 120, 200, 255, "Armour %d on body and arms (left hand)", a);
        else if (in->armour >= 0 && in->armour <= 4) tip_add(t, false, 120, 200, 255, "Armour %d on the %s", a, PART[in->armour]);
    }
    if (item == 0x101) tip_add(t, false, 120, 200, 255, "Use on a lock: %s skill, yours %d", str_get(2, 31 + 16), game_skill(16));
    if (item == 0xd7) tip_add(t, false, 120, 200, 255, "Use on a weapon or armour: %s skill, yours %d", str_get(2, 31 + 14), game_skill(14));
    if (in->weapon || in->armour >= 0 || (item >= 0x37 && item <= 0x3f)) {
        int dur = in->weapon ? in->wdurability : in->durability;
        if (dur == 255) tip_add(t, false, 255, 255, 255, "Never wears out");
        else tip_add(t, false, 255, 255, 255, "Durability %d / %d", o->quality * dur / 63, dur);
    }
    if (in->container) {
        int count = 0;
        for (int c = o->link, guard = 0; c && guard < POOL; c = I.pool[c].next, guard++) count++;
        if (in->capacity) tip_add(t, false, 255, 255, 255, "Holds %d.%d stones (%d.%d in it)", in->capacity / 10, in->capacity % 10, contents_mass(p) / 10, contents_mass(p) % 10);
        tip_add(t, false, 255, 255, 255, "%d thing%s inside", count, count == 1 ? "" : "s");
    }
    if (is_light(item)) {
        int d = I.light_dur[(item & 3) + 4], b = I.light_bright[(item & 3) + 4];
        tip_add(t, false, 255, 255, 255, "Brightness %d of 4", b);
        if (!d) tip_add(t, false, 0, 230, 0, "Never burns out");
        else { /* a point every d ticks of 21 s, within the 24-tick cycle */
            int per = 24 / d, secs = o->quality * 24 * 21 / MAX(1, per);
            tip_add(t, false, 0, 230, 0, "Burns about %d min more%s", secs / 60, is_lit(item) ? " (lit)" : "");
        }
    }
    if ((item >> 4) == 0xb && in->nutrition > 0) tip_add(t, false, 0, 230, 0, "Use: eat (nourishment %d)", in->nutrition);
    if (item >= 0x130 && item <= 0x13f) tip_add(t, false, 0, 230, 0, "Use: read");
    tip_add(t, false, 190, 190, 190, "Weight %d.%d stones", obj_mass(o) / 10, obj_mass(o) % 10);
    if (in->value) tip_add(t, false, 190, 190, 190, "Worth %d gold", in->value * obj_quantity(o));
    /* the differences from the other one */
    if (other) {
        Stats a = stats_of(p), b = stats_of(other);
        bool any = false;
        for (int k = 0; k < 5; k++) {
            int d = a.v[k] - b.v[k];
            if (!d) continue;
            if (!any) tip_add(t, false, 255, 255, 255, "%s", header ? "Compared with the other:" : "If you equip this:"), any = true;
            tip_add(t, false, d > 0 ? 0 : 255, d > 0 ? 230 : 60, d > 0 ? 0 : 60, "%+d %s", d, STAT_NAME[k]);
        }
    }
}

static int tip_size(const Tip *t, int *h) {
    int w = 0;
    *h = 6;
    for (int k = 0; k < t->n; k++) {
        int f = t->l[k].small ? FONT_SMALL : FONT_NORMAL;
        w = MAX(w, ui_text_width(f, t->l[k].text));
        *h += ui_font_height(f) + 2;
    }
    return w + 10;
}
static void tip_draw(const Tip *t, int x, int y) {
    int h, w = tip_size(t, &h);
    ui_parchment(x, y, w, h);
    int cy = y + 4;
    for (int k = 0; k < t->n; k++) {
        int f = t->l[k].small ? FONT_SMALL : FONT_NORMAL;
        u8 r = t->l[k].r, g = t->l[k].g, b = t->l[k].b;
        ui_ink(&r, &g, &b); /* the colours' meanings kept, as inks on the paper */
        ui_text(f, t->l[k].text, x + 5, cy, r, g, b);
        cy += ui_font_height(f) + 2;
    }
}

/* the tooltip of thing p by the mouse (to its left: the panel is at the right edge); compared with what is worn
 * where it would go (when that is something else); with shift, that one's tooltip too */
static void tooltips(int p, bool shift) {
    static Tip a, b;
    int s = body_slot_for(p), worn = s >= 0 && slot_of(p) != s ? I.slot[s] : 0;
    if (worn == p) worn = 0;
    tip_build(&a, p, worn, NULL);
    int ah, aw = tip_size(&a, &ah);
    int x = MAX(2, I.mouse_x - aw - 8), y = MAX(2, I.mouse_y - ah / 2);
    if (shift && worn) {
        tip_build(&b, worn, p, "Currently equipped");
        tip_build(&a, p, worn, "Looking at");
        aw = tip_size(&a, &ah);
        int bh, bw = tip_size(&b, &bh);
        x = MAX(2, I.mouse_x - aw - 8);
        tip_draw(&b, MAX(2, x - bw - 4), MAX(2, I.mouse_y - bh / 2));
    }
    tip_draw(&a, x, y);
}

void inv_draw(void) {
    int px, py, ph = panel_h();
    panel_origin(&px, &py);
    cells_build();
    /* the board: parchment, with a raised edge */
    ui_parchment(px, py, PW, ph);
    /* the figure */
    const Pic *body = ui_pic("ui\\bodies", 0);
    if (body) ui_draw(body, px + (PW - body->w) / 2, py + 6, 1);
    /* the slots' names, in the background of their holes (one or two lines) */
    static const char *const NAME1[INV_BODY] = {"Helm", "Chest", "Hands", "Legs", "Feet", "Right", "Left", "Right", "Left", "Right", "Left"};
    static const char *const NAME2[INV_BODY] = {"", "", "", "", "", "Shldr", "Shldr", "Hand", "Hand", "Ring", "Ring"};
    static const char *const FULL[INV_BODY] = {"Helm", "Chest", "Hands (gloves)", "Legs", "Feet (boots)", "Right shoulder", "Left shoulder",
                                               "Right hand (weapon)", "Left hand (shield)", "Right ring", "Left ring"};
    int last_bag = -1, grid_top = 1 << 30;
    for (int k = 0; k < g_ncell; k++) {
        const Cell *c = &g_cell[k];
        bool hot = k == I.hover;
        if (c->kind == C_CHECK) continue; /* drawn with the weight's line */
        if (c->kind == C_UP || c->kind == C_DOWN) {
            ui_inset(c->x, c->y, c->w, c->h, hot);
            ui_text(FONT_NORMAL, c->kind == C_UP ? "^" : "v", c->x + 5, c->y + 5, UI_INK);
            continue;
        }
        if (c->kind == C_BAG || c->kind == C_PACK) {
            grid_top = MIN(grid_top, c->y);
            /* a thin line where a bag's part of the grid begins */
            if (c->kind == C_BAG && c->bag != last_bag && c->x == px + 4) ui_fill(px + 4, c->y - 2, COLS * PITCH - 2, 1, UI_PAPER_DARK, 255);
            last_bag = c->kind == C_BAG ? c->bag : -1;
        }
        draw_hole(c->x, c->y, c->w, c->h, hot);
        if (c->kind == C_BODY) {
            int lh = ui_font_height(FONT_NORMAL) + 1, two = NAME2[c->slot][0] != 0;
            int ty = c->y + c->h / 2 - (two ? lh : lh / 2);
            u8 r = c->thing ? 108 : 70, g = c->thing ? 78 : 48, b = c->thing ? 60 : 38; /* fainter under a thing */
            ui_text(FONT_NORMAL, NAME1[c->slot], c->x + c->w / 2 - ui_text_width(FONT_NORMAL, NAME1[c->slot]) / 2, ty, r, g, b);
            if (two) ui_text(FONT_NORMAL, NAME2[c->slot], c->x + c->w / 2 - ui_text_width(FONT_NORMAL, NAME2[c->slot]) / 2, ty + lh, r, g, b);
        }
        if (c->kind == C_BAR && I.hidden[c->bag]) ui_fill(c->x + 1, c->y + 1, c->w - 2, c->h - 2, UI_PAPER_DARK, 255);
        if (c->thing) draw_thing_in(c->thing, c->x, c->y, c->w);
        if (c->kind == C_BAR && I.hidden[c->bag]) ui_fill(c->x + 1, c->y + 1, c->w - 2, c->h - 2, UI_PAPER_DARK, 150); /* hidden: dimmed */
    }
    /* the bag bar's line, and the weight carried */
    ui_text(FONT_NORMAL, "Bags (click: show / hide)", px + 6, py + ph - 54, UI_INK);
    int m = carried();   /* (with the thing on the pointer, as the limit counts it: 2026-11-16) */
    char w[48];
    snprintf(w, sizeof w, "Carrying %d.%d stones", m / 10, m % 10);
    ui_text(FONT_NORMAL, w, px + 6, py + ph - 14, UI_INK);
    { /* the checkbox: the character panel with Tab */
        int bx = px + PW - 74, by = py + ph - 15;
        ui_fill(bx, by, 9, 9, UI_INK, 255), ui_fill(bx + 1, by + 1, 7, 7, 176, 140, 116, 255);
        if (g_with_sheet) ui_fill(bx + 2, by + 2, 5, 5, UI_INK_GREEN, 255);
        ui_text(FONT_NORMAL, "Character", bx + 12, by + 1, UI_INK);
    }
    (void)grid_top;
    /* what the mouse is over: an empty body hole its full name; a thing its tooltip */
    const Cell *c = I.hover >= 0 ? &g_cell[I.hover] : NULL;
    if (c && !c->thing && c->kind == C_BODY && !I.held) {
        const char *t = FULL[c->slot];
        ui_parchment(c->x - ui_text_width(FONT_NORMAL, t) - 12, c->y + 7, ui_text_width(FONT_NORMAL, t) + 9, 13);
        ui_text(FONT_NORMAL, t, c->x - ui_text_width(FONT_NORMAL, t) - 8, c->y + 10, UI_INK);
    }
    if (c && c->thing && !I.held) tooltips(c->thing, I.shift);
    /* the pointer, or the thing held on it */
    if (I.held) draw_thing(I.held, I.mouse_x - CELL / 2, I.mouse_y - CELL / 2);
    else {
        ui_cursor(10, I.mouse_x, I.mouse_y);
    }
}

/* --- on the rules core: the player's things are the original's (Inventory[], the object in hand, the bags' contents,
 * all in the core's object store); our panel shows them and its clicks are the original's routines' ------------------- */
static int g_core_of[POOL];     /* a pool object's core object (0-1023) */
static u8 g_hidden_core[1024];  /* the bags hidden in our grid, by core object */

static int mirror_tree(const u8 *b, int ci, int depth) {
    if (ci <= 0 || ci >= 1024 || depth > 12) return 0;
    int p = alloc();
    if (!p) return 0;
    level_obj_from_block(b, ci, &I.pool[p]);
    g_core_of[p] = ci;
    I.pool[p].next = 0;
    I.hidden[p] = g_hidden_core[ci] != 0;
    if (!(I.pool[p].flags & 0x40)) { /* not a stack: its link is its contents */
        int first = 0, last = 0, guard = 0;
        for (int c = I.pool[p].link; c > 0 && c < 1024 && guard < 256; guard++) {
            Object t;
            level_obj_from_block(b, c, &t);
            int q = mirror_tree(b, c, depth + 1);
            if (q) {
                if (last) I.pool[last].next = (u16)q;
                else first = q;
                last = q;
            }
            c = t.next;
        }
        I.pool[p].link = (u16)first;
    }
    return p;
}

/* the panel's contents from the core (every frame while it runs) */
void inv_core_sync(void) {
    const u8 *b = cw_block();
    if (!b) return;
    for (int p = 1; p < POOL; p++)
        if (I.used[p] && g_core_of[p]) g_hidden_core[g_core_of[p]] = I.hidden[p];
    memset(I.used, 0, sizeof I.used);
    memset(g_core_of, 0, sizeof g_core_of);
    for (int k = 0; k < INV_SLOTS; k++) I.slot[k] = mirror_tree(b, cw_inv_slot(k), 0);
    I.held = mirror_tree(b, cw_held(), 0);
}
int inv_core_of(int p) { return p > 0 && p < POOL ? g_core_of[p] : 0; }

/* where a pool object is: its body or pack slot (0-18), else -1 (inside something) */
static int top_slot(int p) {
    for (int k = 0; k < INV_SLOTS; k++)
        if (I.slot[k] == p) return k;
    return -1;
}
/* the container holding pool object p, or 0 */
static int parent_of(int p) {
    for (int c = 1; c < POOL; c++)
        if (I.used[c] && !(I.pool[c].flags & 0x40) && obj_info(I.pool[c].item)->container)
            for (int q = I.pool[c].link, guard = 0; q && guard < 256; q = I.pool[q].next, guard++)
                if (q == p) return c;
    return 0;
}

/* a click on pool object p as the original's panel takes it: a top-level thing in its slot; a thing in a bag is
 * reached as the original reaches it, by opening the bags round it (BAGS.C's OpenTheBag) and scrolling to it */
static void core_click_thing(int p, int drag, int how) {
    int k = top_slot(p);
    if (k >= 0) {
        cw_inv_click(k, drag, how);
        return;
    }
    int chain[16], n = 0;
    for (int q = p; q && n < 16; q = parent_of(q)) chain[n++] = q;
    if (n < 2 || top_slot(chain[n - 1]) < 0) return;
    int cc[16];
    for (int i = 0; i < n; i++) cc[i] = g_core_of[chain[i]];
    int pos = cw_bag_reach(cc, n, top_slot(chain[n - 1]));
    if (pos >= 20) cw_inv_click(pos, drag, how);
}

bool inv_update_core(const InvInput *in) {
    I.mouse_x = in->mouse_x, I.mouse_y = in->mouse_y, I.shift = in->shift;
    cells_build();
    int k = cell_at(in->mouse_x, in->mouse_y);
    I.hover = k;
    const Cell *c = k >= 0 ? &g_cell[k] : NULL;
    int under = c ? c->thing : 0;
    if (in->look && under) core_click_thing(under, 0, -2); /* LOOK.C, through inv_look */
    if ((in->click_left || in->click_right) && getenv("UW_CRIT_DEBUG") && *getenv("UW_CRIT_DEBUG"))
        fprintf(stderr, "panel click at %d,%d: cell %d kind %d slot %d thing %d held %d\n", in->mouse_x, in->mouse_y, k, c ? c->kind : -1, c ? c->slot : -1, under, I.held);
    if ((in->click_left || in->click_right) && getenv("UW_CRIT_DEBUG") && *getenv("UW_CRIT_DEBUG"))
        for (int j = 0; j < g_ncell && j < 24; j++) fprintf(stderr, "  cell %d: %d,%d %dx%d kind %d slot %d\n", j, g_cell[j].x, g_cell[j].y, g_cell[j].w, g_cell[j].h, g_cell[j].kind, g_cell[j].slot);
    if (!inv_mouse_over(in->mouse_x, in->mouse_y)) return false;
    if (!c) return true;
    if (c->kind == C_UP && in->click_left) return I.scroll = MAX(0, I.scroll - 1), true;
    if (c->kind == C_DOWN && in->click_left) return I.scroll = MIN(g_scroll_max, I.scroll + 1), true;
    if (c->kind == C_CHECK && in->click_left) return g_with_sheet = !g_with_sheet, true;
    if (in->click_right && under && !I.held) { /* use it (a bag opens, food is eaten, a book read, ...) */
        if (in->shift) { /* shift: into the world, in front */
            core_click_thing(under, 1, 0);
            cw_drop(0);
        } else core_click_thing(under, 0, 0);
        return true;
    }
    if (!in->click_left) return true;
    if (c->kind == C_BAR) { /* a bag in the bar: something held goes in; shift picks it up; else show / hide */
        if (I.held || in->shift) core_click_thing(c->bag, 1, 0);
        else I.hidden[c->bag] = !I.hidden[c->bag];
        return true;
    }
    if (c->kind == C_BODY || (c->kind == C_PACK && c->slot >= 0)) { /* pick up from or put down in the slot (the original's rules: fits, stacks, swaps) */
        cw_inv_click(c->slot, 1, 0);
        return true;
    }
    if (under) { /* a thing in a bag: picked up, or the held one put on it (into it, stacked, combined, swapped) */
        core_click_thing(under, 1, 0);
        return true;
    }
    if (c->kind == C_BAG && I.held) core_click_thing(c->bag, 1, 0); /* an empty hole of a bag: into the bag */
    return true;
}
