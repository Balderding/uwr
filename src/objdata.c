#include "objdata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ObjInfo g_info[512];

/* --- the game's strings, kept from the start (malloc: the arena is reset between levels) ------------------ */
#define STR_BLOCKS 11
static struct {
    char **s;
    int n;
} g_str[STR_BLOCKS];

static void load_block(const char *data_dir, int block) {
    char path[700];
    snprintf(path, sizeof path, "%s\\strings\\block%04d.txt", data_dir, block);
    size_t mark = arena_mark();
    Bytes f = file_read(path);
    int max = -1;
    for (const char *p = (const char *)f.data; p && p < (const char *)f.data + f.size;) {
        int i;
        if (sscanf(p, "%d:", &i) == 1 && i > max) max = i;
        const char *nl = memchr(p, '\n', (size_t)((const char *)f.data + f.size - p));
        p = nl ? nl + 1 : NULL;
    }
    g_str[block].n = max + 1;
    g_str[block].s = max >= 0 ? calloc((size_t)max + 1, sizeof(char *)) : NULL;
    for (const char *p = (const char *)f.data; p && p < (const char *)f.data + f.size;) {
        const char *end = (const char *)f.data + f.size, *nl = memchr(p, '\n', (size_t)(end - p)), *colon = memchr(p, ':', (size_t)((nl ? nl : end) - p));
        int i;
        if (colon && sscanf(p, "%d:", &i) == 1 && i >= 0 && i <= max) {
            const char *s = colon + 1;
            if (*s == ' ') s++;
            const char *e = nl ? nl : end;
            if (e > s && e[-1] == '\r') e--;
            char *t = malloc((size_t)(e - s) + 1);
            size_t k = 0;
            for (const char *c = s; c < e; c++) {
                if (c[0] == '\\' && c + 1 < e && c[1] == 'n') t[k++] = '\n', c++;
                else t[k++] = *c;
            }
            t[k] = 0;
            g_str[block].s[i] = t;
        }
        p = nl ? nl + 1 : NULL;
    }
    arena_reset(mark);
}

const char *str_get(int block, int index) {
    if (block < 0 || block >= STR_BLOCKS || index < 0 || index >= g_str[block].n || !g_str[block].s[index]) return "";
    return g_str[block].s[index];
}

/* --- data\objects\objects.txt ------------------------------------------------------------------------------ */
bool objdata_load(const char *data_dir) {
    static const int BLOCKS[] = {1, 2, 3, 4, 5, 6, 8, 9, 10};
    for (int b = 0; b < COUNT(BLOCKS); b++)
        if (!g_str[BLOCKS[b]].s) load_block(data_dir, BLOCKS[b]);
    for (int i = 0; i < 512; i++) memset(&g_info[i], 0, sizeof g_info[i]), g_info[i].armour = -1, g_info[i].accepts = -1;
    char path[700];
    snprintf(path, sizeof path, "%s\\objects\\objects.txt", data_dir);
    size_t mark = arena_mark();
    Bytes f = file_read(path);
    int n = 0;
    for (const char *p = (const char *)f.data; p && p < (const char *)f.data + f.size;) {
        const char *end = (const char *)f.data + f.size, *nl = memchr(p, '\n', (size_t)(end - p));
        char line[400];
        size_t len = MIN((size_t)((nl ? nl : end) - p), sizeof line - 1);
        memcpy(line, p, len);
        line[len] = 0;
        char *hash = strchr(line, '#');
        if (hash) { /* the raw COMOBJ bytes after the hash: byte 8, the resistances */
            unsigned b[11];
            int item0;
            if (!strncmp(line, "obj", 3) && sscanf(line + 3, "%i", &item0) == 1 && item0 >= 0 && item0 < 512 &&
                sscanf(hash + 1, "%x %x %x %x %x %x %x %x %x %x %x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &b[6], &b[7], &b[8], &b[9], &b[10]) == 11)
                g_info[item0].resist = (u8)b[8];
            *hash = 0;
        }
        char kind[16];
        int item, used;
        if (sscanf(line, "%15s %i%n", kind, &item, &used) == 2 && item >= 0 && item < 512) {
            ObjInfo *o = &g_info[item];
            if (!strcmp(kind, "obj")) n++;
            if (!strcmp(kind, "container")) o->container = true;
            char key[24];
            int v, k;
            for (const char *q = line + used; sscanf(q, " %23s %i%n", key, &v, &k) == 2; q += k) {
                if (!strcmp(key, "pick")) o->pick = (u8)v;
                else if (!strcmp(key, "kind")) o->kind = (u8)v;
                else if (!strcmp(key, "mass")) o->mass = (u16)v;
                else if (!strcmp(key, "value")) o->value = (u16)v;
                else if (!strcmp(key, "height")) o->height = (u8)v;
                else if (!strcmp(key, "radius")) o->radius = (u8)v;
                else if (!strcmp(key, "qclass")) o->qclass = (u8)v;
                else if (!strcmp(key, "qtype")) o->qtype = (u8)v;
                else if (!strcmp(key, "look")) o->look = (u8)v;
                else if (!strcmp(key, "owner")) o->owner_ok = (u8)v;
                else if (!strcmp(key, "slot")) o->armour = (i8)v;
                else if (!strcmp(key, "protection")) o->protection = (u8)v;
                else if (!strcmp(key, "durability") && strcmp(kind, "weapon")) o->durability = (u8)v;
                else if (!strcmp(key, "capacity")) o->capacity = (u8)v;
                else if (!strcmp(key, "accepts")) o->accepts = (i16)v;
                else if (!strcmp(key, "nutrition")) o->nutrition = (i16)v;
                else if (!strcmp(key, "slash")) o->slash = (u8)v, o->weapon = true;
                else if (!strcmp(key, "bash")) o->bash = (u8)v;
                else if (!strcmp(key, "stab")) o->stab = (u8)v;
                else if (!strcmp(key, "skill")) o->skill = (u8)v;
                else if (!strcmp(key, "ammo")) o->ammo = (i16)v;
                else if (!strcmp(key, "power_min")) o->power_min = (u8)v;
                else if (!strcmp(key, "rate")) o->rate = (u8)v;
                else if (!strcmp(key, "power_max")) o->power_max = (u8)v;
                else if (!strcmp(key, "hp")) o->hp = (u8)v, o->critter = true;
                else if (!strcmp(key, "material")) o->material = (u8)v;
                else if (!strcmp(key, "death")) o->death = (u8)v;
                else if (!strcmp(key, "attack")) o->attack = (u8)v;
                else if (!strcmp(key, "defense")) o->defense = (u8)v;
                else if (!strcmp(key, "strength")) o->strength = (u8)v;
                else if (!strcmp(key, "xp")) o->xp = (u16)v;
                else if (!strcmp(key, "armour") || !strcmp(key, "a1") || !strcmp(key, "a2") || !strcmp(key, "a3")) {
                    /* several numbers: armour b a l h; aN bonus damage chance */
                    int w[4] = {v, 0, 0, 0}, m = 0, used2 = 0;
                    if (!strcmp(key, "armour")) m = sscanf(q + k, " %i %i %i%n", &w[1], &w[2], &w[3], &used2);
                    else m = sscanf(q + k, " %i %i%n", &w[1], &w[2], &used2);
                    if (!strcmp(key, "armour") && m == 3) for (int j = 0; j < 4; j++) o->carmour[j] = (u8)w[j];
                    else if (m == 2) {
                        int a = key[1] - '1';
                        o->a_bonus[a] = (i8)w[0], o->a_damage[a] = (u8)w[1], o->a_chance[a] = (u8)w[2];
                    }
                    k += used2;
                }
                else if (!strcmp(key, "durability") && !strcmp(kind, "weapon")) o->wdurability = (u8)v;
            }
        }
        p = nl ? nl + 1 : NULL;
    }
    arena_reset(mark);
    logf_("objects: %d kinds from objects.txt\n", n);
    return n > 0;
}

const ObjInfo *obj_info(int item) { return &g_info[item & 511]; }

/* is_quantity (bit 15 of the first word, our flags bit 6): link is a count (below 0x200) */
int obj_quantity(const Object *o) {
    if ((o->flags & 0x40) && o->link > 0 && o->link < 0x200) return o->link;
    return 1;
}

bool obj_stacks(int item) { return obj_info(item)->kind == 0 && strchr(str_get(4, item & 511), '&') != NULL; }

int obj_mass(const Object *o) { return obj_info(o->item)->mass * obj_quantity(o); }

/* block 4 names: "a_torch&torches" = article _ noun & plural */
static void name_parts(int item, char *article, char *noun, char *plural) {
    const char *s = str_get(4, item & 511);
    const char *u = strchr(s, '_'), *amp = strchr(s, '&');
    article[0] = noun[0] = plural[0] = 0;
    if (u && (!amp || u < amp)) snprintf(article, 16, "%.*s", (int)(u - s), s), s = u + 1;
    amp = strchr(s, '&');
    if (amp) snprintf(noun, 64, "%.*s", (int)(amp - s), s), snprintf(plural, 64, "%s", amp + 1);
    else snprintf(noun, 64, "%s", s), snprintf(plural, 64, "%s", s);
    if (!noun[0]) snprintf(noun, 64, "something"), snprintf(plural, 64, "things");
}

void obj_noun(const Object *o, char *out, size_t n) {
    char a[16], s[64], p[64];
    name_parts(o->item, a, s, p);
    snprintf(out, n, "%s", obj_quantity(o) > 1 ? p : s);
}

void obj_name(const Object *o, char *out, size_t n) {
    char a[16], s[64], p[64];
    name_parts(o->item, a, s, p);
    int q = obj_quantity(o);
    if (q > 1) snprintf(out, n, "%d %s", q, p);
    else snprintf(out, n, "%s%s%s", a, a[0] ? " " : "", s);
}

static bool vowel(char c) { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U'; }

/* The look, as UW.EXE builds it (file 0x7ba10): "You see " + (a count, or "a"/"an" by the next word's first
 * letter) + the quality word (block 5, qtype * 6 + 0 at quality 0, 5 for quality class 3, else quality / 16 + 1)
 * + the name (plural for a count) + " belonging to" + the owner (block 1, 370 + owner) when the kind can have
 * one and the owner is 1-27, + ".". Objects without the look flag: wall decorations with their own text
 * (file 0x7c0d1): writing (0x166) and gravestones (0x165): block 8, a lead-in (writing 0x170 + flags & 15,
 * gravestone 0x160 + flags & 15) then the text (the link when it is a count, else the quality); a bridge
 * (0x164) "You see a bridge." */
void obj_describe(const Object *o, char *out, size_t n) {
    const ObjInfo *in = obj_info(o->item);
    int item = o->item & 511;
    if (!in->look) {
        if (item == 0x165 || item == 0x166) {
            int text = (o->flags & 0x40) ? (o->link & 0x1ff) : o->quality;
            const char *lead = str_get(8, (item == 0x166 ? 0x170 : 0x160) + (o->flags & 15));
            snprintf(out, n, "%s%s", lead, str_get(8, text));
            return;
        }
        if (item == 0x164) {
            snprintf(out, n, "%s", str_get(1, 171));
            return;
        }
        char name[96];
        obj_name(o, name, sizeof name);
        snprintf(out, n, "You see %s.", name);
        return;
    }
    char a[16], s[64], p[64];
    name_parts(item, a, s, p);
    int q = obj_quantity(o), bucket = 0;
    if (o->quality) bucket = in->qclass == 3 ? 5 : (o->quality >> 4) + 1;
    const char *word = str_get(5, in->qtype * 6 + bucket);
    char buf[256];
    size_t k = (size_t)snprintf(buf, sizeof buf, "You see ");
    if (q > 1) k += (size_t)snprintf(buf + k, sizeof buf - k, "%d ", q);
    else if (in->qtype != 13) {
        char first = word[0] ? word[0] : a[0] ? s[0] : 0;
        if (word[0] || !strcmp(a, "a") || !strcmp(a, "an")) {
            if (first) k += (size_t)snprintf(buf + k, sizeof buf - k, "%s", vowel(first) ? "an " : "a ");
        } else if (a[0]) k += (size_t)snprintf(buf + k, sizeof buf - k, "%s ", a);
    }
    if (word[0]) k += (size_t)snprintf(buf + k, sizeof buf - k, "%s ", word);
    k += (size_t)snprintf(buf + k, sizeof buf - k, "%s", q > 1 ? p : s);
    int owner = o->owner & 0x1f;
    if (in->owner_ok && owner > 0 && owner <= 0x1b) k += (size_t)snprintf(buf + k, sizeof buf - k, " belonging to%s", str_get(1, 370 + owner));
    snprintf(buf + k, sizeof buf - k, ".");
    snprintf(out, n, "%s", buf);
}

/* the textures' descriptions: block 10, walls by their number, floors from 510 down */
void wall_describe(int tex, char *out, size_t n) {
    const char *s = str_get(10, tex);
    snprintf(out, n, "You see %s.", s[0] ? s : "a wall");
}
void floor_describe(int tex, char *out, size_t n) {
    const char *s = str_get(10, 510 - tex);
    snprintf(out, n, "You see %s.", s[0] ? s : "the floor");
}

/* Can o go into a container (UW.EXE file 0x7aaeb): the contents' weight with it must stay within the capacity
 * (0 = no limit) else "The sack is too full."; then by what the container accepts: anything (that can be
 * picked up), one kind of object, or a class: 0x200 runes (0xE8-0xFF: "You can only put runes in the rune
 * bag."), 0x201 ammunition (0x10-0x12), 0x202 maps and scrolls (0x138-0x13F), 0x203 food (0xB0-0xBF) and the
 * plants, candle, stew, leeches and dead rotworm; anything else "That item does not fit." */
bool obj_fits(int container, const Object *o, int held, char *msg, size_t n) {
    const ObjInfo *c = obj_info(container);
    int item = o->item & 511, major = item >> 6, minor = (item >> 4) & 3, index = item & 15;
    msg[0] = 0;
    if (c->capacity && held + obj_mass(o) > c->capacity) {
        char a[16], s[64], p[64];
        name_parts(container, a, s, p);
        snprintf(msg, n, "The %s is too full.", s);
        return false;
    }
    if (c->accepts < 0) {
        if (!obj_info(item)->pick) snprintf(msg, n, "%s", "That item does not fit.");
        return obj_info(item)->pick != 0;
    }
    bool ok;
    switch (c->accepts) {
    case 0x200: ok = major == 3 && (minor == 3 || (minor == 2 && index > 7)); break;
    case 0x201: ok = major == 0 && minor == 1 && index < 3; break;
    case 0x202: ok = major == 4 && minor == 3 && index >= 8; break;
    case 0x203:
        ok = (major == 2 && minor == 3) || item == 0xce || item == 0xcf || item == 0x92 || item == 0x125 || item == 0x11b || item == 0xd9;
        break;
    default: ok = item == c->accepts; break;
    }
    if (!ok) snprintf(msg, n, "%s", c->accepts == 0x200 ? "You can only put runes in the rune bag." : "That item does not fit.");
    return ok;
}
