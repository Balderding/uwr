#include "extract.h"

#include "gr.h"
#include "audio.h"
#include "conv.h"
#ifdef _WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <string.h>

#define N_WALLS 210
#define N_FLOORS 52
#define LEVELS 9
#define LEVEL_BLOCK 31752

/* every loose file, in a fixed order */
int extract_list(char names[][48], int cap) {
    int n = 0;
    for (int i = 0; i < N_WALLS && n < cap; i++) snprintf(names[n++], 48, "textures\\walls\\%03d.png", i);
    for (int i = 0; i < N_FLOORS && n < cap; i++) snprintf(names[n++], 48, "textures\\floors\\%03d.png", i);
    for (int i = 1; i <= LEVELS && n < cap; i++) snprintf(names[n++], 48, "levels\\level%d.txt", i);
    return n;
}

/* --- the originals ------------------------------------------------------------------------------ */
typedef struct {
    Bytes pals, w64, f32, lev;
} Originals;

static bool load_originals(const char *game_dir, Originals *o) {
    char p[600];
    snprintf(p, sizeof p, "%s\\DATA\\PALS.DAT", game_dir);
    o->pals = file_read(p);
    snprintf(p, sizeof p, "%s\\DATA\\W64.TR", game_dir);
    o->w64 = file_read(p);
    snprintf(p, sizeof p, "%s\\DATA\\F32.TR", game_dir);
    o->f32 = file_read(p);
    snprintf(p, sizeof p, "%s\\DATA\\LEV.ARK", game_dir);
    o->lev = file_read(p);
    if (o->pals.size != 6144 || o->w64.size < 4 || o->f32.size < 4 || o->lev.size < 2) {
        logf_("extract: the original files are missing or wrong in %s\\DATA (PALS.DAT, W64.TR, F32.TR, LEV.ARK)\n", game_dir);
        return false;
    }
    return true;
}

/* palette 0 as 8-bit RGB (VGA 6-bit c -> c << 2 | c >> 4, as tools\assets.py) */
static void palette0(const Originals *o, u8 pal[768]) {
    for (int i = 0; i < 768; i++) {
        u8 c = o->pals.data[i];
        pal[i] = (u8)(c << 2 | c >> 4);
    }
}

/* texture i of a .TR file, or NULL */
static const u8 *tr_texture(Bytes b, int i, int *size) {
    if (b.size < 4 || b.data[0] != 2) return NULL;
    int count = (int)rd16(b.data + 2);
    *size = b.data[1];
    if (i >= count || 4 + 4 * (size_t)count > b.size) return NULL;
    u32 off = rd32(b.data + 4 + 4 * i);
    if (off + (size_t)*size * *size > b.size) return NULL;
    return b.data + off;
}

/* block i of LEV.ARK: its bytes and size (the gap to the next stored block), or NULL when empty */
static const u8 *ark_block(Bytes b, int i, size_t *size) {
    int n = (int)rd16(b.data);
    if (i >= n || 2 + 4 * (size_t)n > b.size) return NULL;
    u32 off = rd32(b.data + 2 + 4 * i);
    if (!off || off >= b.size) return NULL;
    u32 end = (u32)b.size;
    for (int k = 0; k < n; k++) {
        u32 o2 = rd32(b.data + 2 + 4 * k);
        if (o2 > off && o2 < end) end = o2;
    }
    *size = end - off;
    return b.data + off;
}

/* --- the level text (exactly as tools\loose.py level_text) ------------------------------------- */
typedef struct {
    char *p;
    size_t n, cap;
} Out;

static void outf(Out *o, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#include <stdarg.h>
static void outf(Out *o, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int k = vsnprintf(o->p + o->n, o->cap - o->n, fmt, ap);
    va_end(ap);
    if (k < 0 || o->n + (size_t)k >= o->cap) fatal("extract: level text too long");
    o->n += (size_t)k;
}
static void hex(Out *o, const u8 *b, size_t n) {
    for (size_t i = 0; i < n; i++) outf(o, "%02x", b[i]);
}

static bool write_level(const char *path, int n, const u8 *lev, const u8 *ovl, size_t ovl_n, const u8 *tm) {
    size_t mark = arena_mark();
    Out o = {arena_alloc(1 << 20), 0, 1 << 20};
    outf(&o, "# Ultima Underworld level %d (LEV.ARK blocks %d, %d, %d), written by uo-new\r\n", n, n - 1, n + 8, n + 17);
    outf(&o, "# Edit freely: the engine reads this file. Delete it to have it extracted again from game\\DATA\\LEV.ARK.\r\n");
    outf(&o, "walls");
    for (int i = 0; i < 48; i++) outf(&o, " %u", rd16(tm + 2 * i));
    outf(&o, "\r\nfloors");
    for (int i = 0; i < 10; i++) outf(&o, " %u", rd16(tm + 96 + 2 * i));
    outf(&o, "\r\ndoors");
    for (int i = 0; i < 6; i++) outf(&o, " %u", tm[116 + i]);
    outf(&o, "\r\n# tile x y type height floor wall door nomagic unknown8 first_object  (y 0 = south; type 0 solid 1 open"
             " 2-5 diagonal open SE SW NE NW 6-9 slope N S E W)\r\n");
    for (int i = 0; i < 4096; i++) {
        u32 w0 = rd16(lev + 4 * i), w1 = rd16(lev + 4 * i + 2);
        outf(&o, "tile %d %d %u %u %u %u %u %u %u %u\r\n", i % 64, i / 64, w0 & 15, (w0 >> 4) & 15, (w0 >> 10) & 15, w1 & 63,
             w0 >> 15, (w0 >> 14) & 1, (w0 >> 8) & 3, w1 >> 6);
    }
    outf(&o, "# object index item flags z heading y x quality next owner link [19 more bytes for 0-255, hex]\r\n");
    for (int i = 0; i < 1024; i++) {
        size_t off = 0x4000 + (i < 256 ? (size_t)i * 27 : 256 * 27 + (size_t)(i - 256) * 8);
        u32 w0 = rd16(lev + off), w1 = rd16(lev + off + 2), w2 = rd16(lev + off + 4), w3 = rd16(lev + off + 6);
        outf(&o, "object %d %u %u %u %u %u %u %u %u %u %u", i, w0 & 511, w0 >> 9, w1 & 127, (w1 >> 7) & 7, (w1 >> 10) & 7, w1 >> 13,
             w2 & 63, w2 >> 6, w3 & 63, w3 >> 6);
        if (i < 256) {
            outf(&o, " ");
            hex(&o, lev + off + 8, 19);
        }
        outf(&o, "\r\n");
    }
    outf(&o, "# the rest of the level block (not decoded yet): offset, then bytes in hex\r\n");
    for (size_t off = 0x7300; off < LEVEL_BLOCK; off += 32) {
        outf(&o, "rest %zu ", off);
        hex(&o, lev + off, MIN(32, LEVEL_BLOCK - off));
        outf(&o, "\r\n");
    }
    outf(&o, "# LEV.ARK block %d (not decoded yet; 384 bytes when present)\r\n", n + 8);
    for (size_t off = 0; ovl && off < ovl_n; off += 32) {
        outf(&o, "overlay %zu ", off);
        hex(&o, ovl + off, MIN(32, ovl_n - off));
        outf(&o, "\r\n");
    }
    make_dirs_for(path);
    bool ok = file_write(path, o.p, o.n);
    arena_reset(mark);
    return ok;
}

/* --- the text: data\strings\blockNNNN.txt, "index: text" a line (\n new line, \\ a backslash) ------------- */
typedef struct {
    const char *dir;
    int block, written, failed;
    char *buf;
    size_t n, cap;
    bool want;
} StrOut;

static void str_flush(StrOut *o) {
    if (o->block >= 0 && o->want && o->n) {
        char path[700];
        snprintf(path, sizeof path, "%s\\strings\\block%04d.txt", o->dir, o->block);
        make_dirs_for(path);
        if (file_write(path, o->buf, o->n)) o->written++;
        else o->failed++;
    }
    o->n = 0;
}
static void str_each(void *ctx, int block, int index, const char *s, int len) {
    StrOut *o = ctx;
    if (block != o->block) {
        str_flush(o);
        o->block = block;
        char path[700];
        snprintf(path, sizeof path, "%s\\strings\\block%04d.txt", o->dir, block);
        o->want = !file_exists(path);
        if (o->want)
            o->n += (size_t)snprintf(o->buf + o->n, o->cap - o->n,
                                     "# STRINGS.PAK block %d, one string a line: \"index: text\" (\\n = a new line). Edit freely; delete to "
                                     "extract it again.\r\n",
                                     block);
    }
    if (!o->want || o->n + (size_t)len * 2 + 32 > o->cap) return;
    o->n += (size_t)snprintf(o->buf + o->n, o->cap - o->n, "%d: ", index);
    for (int i = 0; i < len; i++) {
        char c = s[i];
        if (c == '\n') o->buf[o->n++] = '\\', o->buf[o->n++] = 'n';
        else if (c == '\\') o->buf[o->n++] = '\\', o->buf[o->n++] = '\\';
        else if (c == '\r') continue;
        else o->buf[o->n++] = c;
    }
    o->buf[o->n++] = '\r', o->buf[o->n++] = '\n';
}

static int extract_strings(const char *data_dir, const char *game_dir) {
    char path[700];
    /* data\strings\blocks.txt lists the blocks: when it and every block are there, nothing to do */
    snprintf(path, sizeof path, "%s\\strings\\blocks.txt", data_dir);
    size_t mark = arena_mark();
    Bytes list = file_read(path);
    bool all = list.size > 0;
    for (const char *p = (const char *)list.data; all && p && p < (const char *)list.data + list.size;) {
        int b;
        if (sscanf(p, "%d", &b) == 1) {
            char bp[700];
            snprintf(bp, sizeof bp, "%s\\strings\\block%04d.txt", data_dir, b);
            all = file_exists(bp);
        }
        const char *nl = memchr(p, '\n', (size_t)((const char *)list.data + list.size - p));
        p = nl ? nl + 1 : NULL;
    }
    arena_reset(mark);
    if (all) return 0;
    snprintf(path, sizeof path, "%s\\DATA\\STRINGS.PAK", game_dir);
    Bytes pak = file_read(path);
    if (!pak.size) return arena_reset(mark), logf_("extract: %s is missing\n", path), -1;
    StrOut o = {data_dir, -1, 0, 0, arena_alloc(1 << 20), 0, 1 << 20, false};
    if (!strings_each(pak, str_each, &o)) return arena_reset(mark), logf_("extract: STRINGS.PAK is not as expected\n"), -1;
    str_flush(&o);
    /* the list of blocks */
    char *t = arena_alloc(1 << 16);
    size_t tn = 0;
    int nn = (int)rd16(pak.data);
    size_t at = 2 + 4 * (size_t)nn;
    int nb = (int)rd16(pak.data + at);
    for (int b = 0; b < nb; b++) tn += (size_t)snprintf(t + tn, (1 << 16) - tn, "%d\r\n", (int)rd16(pak.data + at + 2 + 6 * b));
    snprintf(path, sizeof path, "%s\\strings\\blocks.txt", data_dir);
    file_write(path, t, tn);
    int w = o.written;
    arena_reset(mark);
    return o.failed ? -1 : w;
}

/* --- pictures: a .GR file into data\<dir>\NNN.png (RGBA, palette 0, index 0 transparent) ------------- */
static int extract_gr(const char *data_dir, const char *game_dir, const char *gr_name, const char *dir, const u8 pal[768]) {
    char path[700];
    size_t mark = arena_mark();
    snprintf(path, sizeof path, "%s\\DATA\\%s", game_dir, gr_name);
    Bytes gr = file_read(path);
    snprintf(path, sizeof path, "%s\\DATA\\ALLPALS.DAT", game_dir);
    Bytes aux = file_read(path);
    if (!gr.size || aux.size < 512) return arena_reset(mark), logf_("extract: %s or ALLPALS.DAT is missing\n", gr_name), -1;
    int n = gr_count(gr), written = 0;
    u8 *idx = arena_alloc(256 * 256), *rgba = arena_alloc(256 * 256 * 4);
    if (!strcmp(gr_name, "PANELS.GR") && gr.size > 3 + 4 * 4) {
        /* UW1's PANELS.GR: its pictures have no headers, each 83 x 114 bytes of palette 0 between one offset and the
           next (the right-hand panel's pages: inventory, rune bag, statistics); the last, shorter, is not a page */
        int count = gr.data[1] | gr.data[2] << 8;
        for (int k = 0; k < count && 3 + 4 * (k + 1) <= (int)gr.size; k++) {
            const u8 *o = gr.data + 3 + 4 * k;
            size_t a = (size_t)(o[0] | o[1] << 8 | o[2] << 16 | (unsigned)o[3] << 24), b;
            if (k + 1 < count) {
                const u8 *q = o + 4;
                b = (size_t)(q[0] | q[1] << 8 | q[2] << 16 | (unsigned)q[3] << 24);
            } else b = gr.size;
            if (b <= a || b - a != 83 * 114 || b > gr.size) continue;
            snprintf(path, sizeof path, "%s\\%s\\%03d.png", data_dir, dir, k);
            if (file_exists(path)) continue;
            for (int i = 0; i < 83 * 114; i++) memcpy(rgba + i * 4, pal + gr.data[a + i] * 3, 3), rgba[i * 4 + 3] = 255;
            make_dirs_for(path);
            if (!png_write(path, rgba, 83, 114, 4)) return arena_reset(mark), logf_("extract: cannot write %s\n", path), -1;
            written++;
        }
        arena_reset(mark);
        return written;
    }
    for (int k = 0; k < n; k++) {
        snprintf(path, sizeof path, "%s\\%s\\%03d.png", data_dir, dir, k);
        if (file_exists(path)) continue;
        int w = 0, h = 0;
        if (!gr_picture(gr, k, aux.data, idx, &w, &h) || w <= 0 || h <= 0) continue;
        for (int i = 0; i < w * h; i++) {
            u8 *o = rgba + i * 4;
            memcpy(o, pal + idx[i] * 3, 3);
            o[3] = idx[i] ? 255 : 0;
        }
        if (!png_write(path, rgba, w, h, 4)) return arena_reset(mark), logf_("extract: cannot write %s\n", path), -1;
        written++;
    }
    arena_reset(mark);
    return written;
}

/* --- driver ------------------------------------------------------------------------------------- */
/* is the file the engine's own of this version (its first line) */
static bool file_starts_with(const char *path, const char *head) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char line[64] = {0};
    size_t n = fread(line, 1, sizeof line - 1, f);
    fclose(f);
    line[n] = 0;
    return !strncmp(line, head, strlen(head));
}

static int extract_more(const char *data_dir, const char *game_dir) {
    int total = 0, r;
    if ((r = extract_strings(data_dir, game_dir)) < 0) return -1;
    total += r;
    /* palette 0 as 8-bit RGB */
    char path[700];
    snprintf(path, sizeof path, "%s\\DATA\\PALS.DAT", game_dir);
    size_t mark = arena_mark();
    Bytes pals = file_read(path);
    if (pals.size != 6144) return arena_reset(mark), -1;
    u8 pal[768];
    for (int i = 0; i < 768; i++) pal[i] = (u8)(pals.data[i] << 2 | pals.data[i] >> 4);
    arena_reset(mark);
    /* the object pictures (items in the world and in the inventory) and the pictures the interface is made of */
    static const char *const GR[][2] = {{"OBJECTS.GR", "sprites\\objects"}, {"TMOBJ.GR", "sprites\\wall_objects"},
                                        {"DOORS.GR", "sprites\\doors"},     {"PANELS.GR", "ui\\panels"},
                                        {"INV.GR", "ui\\inventory"},       {"COMPASS.GR", "ui\\compass"},
                                        {"FLASKS.GR", "ui\\flasks"},        {"BUTTONS.GR", "ui\\buttons"},
                                        {"CURSORS.GR", "ui\\cursors"},      {"OPTBTNS.GR", "ui\\options"},
                                        {"CHAINS.GR", "ui\\chains"},        {"DRAGONS.GR", "ui\\dragons"},
                                        {"SCRLEDGE.GR", "ui\\scroll_edge"}, {"SPELLS.GR", "ui\\spells"},
                                        {"EYES.GR", "ui\\eyes"},            {"HEADS.GR", "ui\\heads"},
                                        {"CHARHEAD.GR", "ui\\character_heads"}, {"ARMOR_M.GR", "ui\\armour_male"},
                                        {"ARMOR_F.GR", "ui\\armour_female"}, {"BODIES.GR", "ui\\bodies"},
                                        {"WEAPONS.GR", "ui\\weapons"},      {"POWER.GR", "ui\\power"},
                                        {"CONVERSE.GR", "ui\\converse"},    {"LFTI.GR", "ui\\lfti"},
                                        {"ANIMO.GR", "ui\\animo"},          {"VIEWS.GR", "ui\\views"},
                                        {"3DWIN.GR", "ui\\3dwin"},          {"QUESTION.GR", "ui\\question"},
                                        {"GENHEAD.GR", "ui\\generated_heads"}, {"OPBTN.GR", "ui\\opbtn"},
                                        {"OPTB.GR", "ui\\optb"},            {"CHRBTNS.GR", "ui\\character_buttons"},
                                        {"TMFLAT.GR", "sprites\\flat_objects"}};
    for (int i = 0; i < COUNT(GR); i++) {
        if ((r = extract_gr(data_dir, game_dir, GR[i][0], GR[i][1], pal)) < 0) return -1;
        total += r;
    }
    /* the spells (UW.EXE: 48 entries of 4 bytes in the segment of overlay-table entry 73, read by the caster 0x78ec8:
     * byte 0 the effect category << 3 | how it is aimed, a word of three 5-bit runes (0 An .. 23 Ylem, 24 none), byte 3
     * the strength; spell n is of circle n / 6 + 1) */
    snprintf(path, sizeof path, "%s\\magic\\spells.txt", data_dir);
    if (!file_exists(path)) {
        char src[700];
        snprintf(src, sizeof src, "%s\\UW.EXE", game_dir);
        mark = arena_mark();
        Bytes ex = file_read(src);
        size_t fbov = 0;
        for (size_t i = 0; i + 4 < ex.size && !fbov; i++)
            if (!memcmp(ex.data + i, "FBOV", 4)) fbov = i;
        if (fbov) {
            u32 tab = ex.data[fbov + 8] | ex.data[fbov + 9] << 8 | (u32)ex.data[fbov + 10] << 16 | (u32)ex.data[fbov + 11] << 24;
            size_t e = tab + 73 * 8;
            u32 seg = e + 2 <= ex.size ? (u32)(ex.data[e] | ex.data[e + 1] << 8) : 0, base = 0x3200 + seg * 16;
            static const char *const RUNE[25] = {"An", "Bet", "Corp", "Des", "Ex", "Flam", "Grav", "Hur", "In", "Jux", "Kal", "Lor", "Mani",
                                                 "Nox", "Ort", "Por", "Quas", "Rel", "Sanct", "Tym", "Uus", "Vas", "Wis", "Ylem", "-"};
            /* the spells' names (the game's manual; the runes, circles and effects are the executable's) */
            static const char *const NAME[48] = {
                "Light", "Resist Blows", "Magic Arrow", "Create Food", "Stealth", "Leap",
                "Protection", "Slow Fall", "Lesser Heal", "Detect Monster", "Cause Fear", "Rune of Warding",
                "Speed", "Conceal", "Night Vision", "Electric Bolt", "Strengthen Door", "Thick Skin",
                "Water Walk", "Heal", "Levitate", "Poison", "Flameproof", "Remove Trap",
                "Fireball", "Smite Undead", "Name Enchantment", "Missile Protection", "Open", "Cure Poison",
                "Greater Heal", "Sheet Lightning", "Gate Travel", "Paralyze", "Daylight", "Telekinesis",
                "Fly", "Ally", "Summon Monster", "Invisibility", "Confusion", "Reveal",
                "Iron Flesh", "Tremor", "Roaming Sight", "Flame Wind", "Freeze Time", "Armageddon"};
            size_t cap = 8192, n = 0;
            char *out = arena_alloc(cap);
            n += (size_t)snprintf(out + n, cap - n, "# the spells (UW.EXE's table): spell circle runes(3, - none) category aim strength name\r\n");
            for (int k = 0; k < 48 && base + k * 4 + 4 <= ex.size; k++) {
                const u8 *b = ex.data + base + k * 4;
                int code = b[1] | b[2] << 8, r0 = MIN(24, code >> 10 & 31), r1 = MIN(24, code >> 5 & 31), r2 = MIN(24, code & 31);
                n += (size_t)snprintf(out + n, cap - n, "spell %d %d %s %s %s %d %d %d %s\r\n", k, k / 6 + 1, RUNE[r0], RUNE[r1], RUNE[r2], b[0] >> 3, b[0] & 7,
                                      b[3], NAME[k]);
            }
            make_dirs_for(path);
            file_write(path, out, n);
            total++;
        }
        arena_reset(mark);
    }
    /* what each texture is underfoot (TERRAIN.DAT: 256 wall + 256 floor words; UW.EXE keeps it at DS 0x7192 and
     * reads type >> 4: 1 water, 2 lava): the floor textures that are not plain floor */
    snprintf(path, sizeof path, "%s\\levels\\terrain.txt", data_dir);
    if (!file_exists(path)) {
        char src[700];
        snprintf(src, sizeof src, "%s\\DATA\\TERRAIN.DAT", game_dir);
        mark = arena_mark();
        Bytes td = file_read(src);
        if (td.size >= 1024) {
            size_t cap = 8192, n = 0;
            char *out = arena_alloc(cap);
            n += (size_t)snprintf(out + n, cap - n, "# floor textures that are not plain floor (TERRAIN.DAT): floor <texture> water | lava\r\n");
            for (int t = 0; t < 256; t++) {
                int v = td.data[512 + t * 2] | td.data[513 + t * 2] << 8;
                if ((v >> 4) == 1 || (v >> 4) == 2) n += (size_t)snprintf(out + n, cap - n, "floor %d %s\r\n", t, (v >> 4) == 1 ? "water" : "lava");
            }
            make_dirs_for(path);
            file_write(path, out, n);
            total++;
        }
        arena_reset(mark);
    }
    /* the classes for character creation (SKILLS.DAT, read by UW.EXE 0x6dd3e): 8 x (strength, dexterity,
     * intelligence, bonus points), then for each class 5 sets of skills: a count, then that many skills (one: given;
     * more: the player picks one) */
    snprintf(path, sizeof path, "%s\\character\\classes.txt", data_dir);
    if (!file_exists(path)) {
        char src[700];
        snprintf(src, sizeof src, "%s\\DATA\\SKILLS.DAT", game_dir);
        mark = arena_mark();
        Bytes sd = file_read(src);
        if (sd.size >= 40) {
            size_t cap = 4096, n = 0, at = 32;
            char *out = arena_alloc(cap);
            n += (size_t)snprintf(out + n, cap - n, "# the classes (SKILLS.DAT): base strength, dexterity, intelligence; bonus points dealt out at random\r\n"
                                                     "# (1-4 at a time, an attribute at most 30); 5 skill sets ('/'): one skill is given, more: one is picked.\r\n"
                                                     "# Skills: 0 attack 1 defense 2 unarmed 3 sword 4 axe 5 mace 6 missile 7 mana 8 lore 9 casting 10 traps\r\n"
                                                     "# 11 search 12 track 13 sneak 14 repair 15 charm 16 picklock 17 acrobat 18 appraise 19 swimming\r\n");
            for (int c = 0; c < 8; c++) {
                n += (size_t)snprintf(out + n, cap - n, "class %d str %d dex %d int %d bonus %d sets", c, sd.data[c * 4], sd.data[c * 4 + 1],
                                      sd.data[c * 4 + 2], sd.data[c * 4 + 3]);
                for (int s = 0; s < 5 && at < sd.size; s++) {
                    int cnt = sd.data[at++];
                    n += (size_t)snprintf(out + n, cap - n, "%s", s ? " /" : "");
                    for (int k = 0; k < cnt && at < sd.size; k++) n += (size_t)snprintf(out + n, cap - n, " %d", sd.data[at++]);
                }
                n += (size_t)snprintf(out + n, cap - n, "\r\n");
            }
            make_dirs_for(path);
            file_write(path, out, n);
            total++;
        }
        arena_reset(mark);
    }
    /* where each weapon picture goes in the original's 176 x 112 view (WEAPONS.DAT, read by UW.EXE 0x32fb1: per hand
     * 224 bytes, per kind (sword, axe, mace, fist) 28 x then 28 y; y counts up from the view's bottom row) */
    snprintf(path, sizeof path, "%s\\ui\\weapons\\offsets.txt", data_dir);
    if (!file_exists(path)) {
        char src[700];
        snprintf(src, sizeof src, "%s\\DATA\\WEAPONS.DAT", game_dir);
        mark = arena_mark();
        Bytes wd = file_read(src);
        if (wd.size == 448) {
            size_t cap = 224 * 24 + 512, n = 0;
            char *out = arena_alloc(cap);
            n += (size_t)snprintf(out + n, cap - n, "# weapon pictures (ui\\weapons\\NNN.png) in the original's 176 x 112 view: "
                                                     "picture, left, top. 0-111 right hand, 112-223 left; 28 a kind: sword, axe, mace, fist\r\n");
            for (int k = 0; k < 224; k++) {
                int hand = k / 112, kind = k % 112 / 28, f = k % 28, at = hand * 224 + kind * 56;
                n += (size_t)snprintf(out + n, cap - n, "%d %d %d\r\n", k, wd.data[at + f], 111 - wd.data[at + 28 + f]);
            }
            file_write(path, out, n);
            total++;
        }
        arena_reset(mark);
    }
    /* the creatures (CRIT): each of the 64 kinds (object 0x40 + k) in its own colours: data\sprites\creatures\NN\
     * pP_FFF.png (page P, frame F) and anim.txt (slots, segments, frames with their hotspot = the feet) */
    {
        snprintf(path, sizeof path, "%s\\CRIT\\ASSOC.ANM", game_dir);
        mark = arena_mark();
        Bytes assoc = file_read(path);
        for (int k = 0; assoc.size >= 384 && k < 64; k++) {
            char txt[700];
            snprintf(txt, sizeof txt, "%s\\sprites\\creatures\\%02d\\anim.txt", data_dir, k);
            /* there and current ("v2": transparency = the game palette's colour 0), or someone's own: kept; the
             * engine's first version (transparent where the small index was 0: black boxes) is made again */
            bool remake = false;
            {
                FILE *f = fopen(txt, "rb");
                if (f) {
                    char head[64] = {0};
                    size_t got = fread(head, 1, sizeof head - 1, f);
                    fclose(f);
                    head[got] = 0;
                    if (!strncmp(head, "# creature", 10)) remake = true;
                    else continue;
                }
            }
            int anim = assoc.data[256 + 2 * k], palno = assoc.data[257 + 2 * k];
            size_t tcap = 1 << 16, tn = 0;
            char *t = arena_alloc(tcap);
            tn += (size_t)snprintf(t + tn, tcap - tn, "# uo-new creature v2: %d (object 0x%X): animation %.8s (CRIT\\CR%02oPAGE), its palette %d\r\n"
                                                       "# page P slots ... | page P segment G frames... | page P frame F width height hotspot_x hotspot_y\r\n",
                                   k, 0x40 + k, (const char *)assoc.data + anim * 8, anim, palno);
            for (int pg = 0; pg < 2; pg++) {
                char src[700];
                snprintf(src, sizeof src, "%s\\CRIT\\CR%02oPAGE.N%02d", game_dir, anim, pg);
                Bytes pd = file_read(src);
                CritPage cp;
                if (!pd.size || !crit_page(pd, &cp)) continue;
                tn += (size_t)snprintf(t + tn, tcap - tn, "page %d slots", pg);
                for (int i = 0; i < cp.nslots; i++) tn += (size_t)snprintf(t + tn, tcap - tn, " %d", cp.slots[i]);
                tn += (size_t)snprintf(t + tn, tcap - tn, "\r\n");
                for (int g = 0; g < cp.nsegs; g++) {
                    tn += (size_t)snprintf(t + tn, tcap - tn, "page %d segment %d", pg, g);
                    for (int i = 0; i < 8; i++) tn += (size_t)snprintf(t + tn, tcap - tn, " %d", cp.segs[g][i]);
                    tn += (size_t)snprintf(t + tn, tcap - tn, "\r\n");
                }
                const u8 *aux = cp.pals[palno < cp.npals ? palno : 0];
                u8 *idx = arena_alloc(256 * 256), *rgba = arena_alloc(256 * 256 * 4);
                for (int f = 0; f < cp.nframes; f++) {
                    int w, h, hx, hy;
                    if (!crit_frame(pd, &cp, f, idx, &w, &h, &hx, &hy)) continue;
                    for (int i = 0; i < w * h; i++) {
                        u8 *o = rgba + i * 4;
                        int gi = aux[idx[i] & 31];
                        memcpy(o, pal + gi * 3, 3), o[3] = gi ? 255 : 0; /* the game palette's colour 0 is see-through */
                    }
                    char dst[700];
                    snprintf(dst, sizeof dst, "%s\\sprites\\creatures\\%02d\\p%d_%03d.png", data_dir, k, pg, f);
                    if ((remake || !file_exists(dst)) && png_write(dst, rgba, w, h, 4)) total++;
                    tn += (size_t)snprintf(t + tn, tcap - tn, "page %d frame %d %d %d %d %d\r\n", pg, f, w, h, hx, hy);
                }
            }
            make_dirs_for(txt);
            if (file_write(txt, t, tn)) total++;
        }
        arena_reset(mark);
    }
    /* the music (.XMI -> .mid) and the speech (.VOC -> .wav) */
    {
        char pat[700];
        snprintf(pat, sizeof pat, "%s\\SOUND\\*", game_dir);
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pat, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                const char *nm = fd.cFileName, *dot = strrchr(nm, '.');
                if (!dot) continue;
                bool xmi = str_ieq(dot, ".XMI"), voc = str_ieq(dot, ".VOC");
                if (!xmi && !voc) continue;
                char dst[700], src[700];
                snprintf(dst, sizeof dst, "%s\\%s\\%.*s.%s", data_dir, xmi ? "music" : "sound\\speech", (int)(dot - nm), nm, xmi ? "mid" : "wav");
                if (file_exists(dst)) continue;
                snprintf(src, sizeof src, "%s\\SOUND\\%s", game_dir, nm);
                mark = arena_mark();
                Bytes b = file_read(src);
                size_t cap = b.size * 4 + 4096, n = 0;
                u8 *out = arena_alloc(cap);
                if (b.size && (xmi ? xmi_to_mid(b, out, cap, &n) : voc_to_wav(b, out, cap, &n))) {
                    make_dirs_for(dst);
                    if (file_write(dst, out, n)) total++;
                }
                arena_reset(mark);
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        }
    }
    /* the original sound files as they are, for UW's own sound driver (xmidi.c): the songs (data\music\NAME.xmi),
     * the Roland timbres (UW.MT), the AdLib timbres (UW.AD), the effects' table (SOUNDS.DAT) */
    {
        static const char *const RAW[][2] = {{"UW.MT", "sound\\uw.mt"}, {"UW.AD", "sound\\uw.ad"}, {"SOUNDS.DAT", "sound\\sounds.dat"}};
        char pat[700];
        snprintf(pat, sizeof pat, "%s\\SOUND\\*.XMI", game_dir);
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pat, &fd);
        char names[64][32];
        int nn = 0;
        if (h != INVALID_HANDLE_VALUE) {
            do
                if (nn < 64) str_copy(names[nn++], 32, fd.cFileName);
            while (FindNextFileA(h, &fd));
            FindClose(h);
        }
        for (int i = 0; i < nn + COUNT(RAW); i++) {
            char src[700], dst[700];
            if (i < nn) {
                const char *dot = strrchr(names[i], '.');
                snprintf(src, sizeof src, "%s\\SOUND\\%s", game_dir, names[i]);
                snprintf(dst, sizeof dst, "%s\\music\\%.*s.xmi", data_dir, (int)(dot ? dot - names[i] : (long long)strlen(names[i])), names[i]);
            } else {
                snprintf(src, sizeof src, "%s\\SOUND\\%s", game_dir, RAW[i - nn][0]);
                snprintf(dst, sizeof dst, "%s\\%s", data_dir, RAW[i - nn][1]);
            }
            if (file_exists(dst)) continue;
            mark = arena_mark();
            Bytes b = file_read(src);
            if (b.size) {
                make_dirs_for(dst);
                if (file_write(dst, b.data, b.size)) total++;
            }
            arena_reset(mark);
        }
    }
    /* the conversations (CNV.ARK -> data\conversations\NNN.txt) */
    total += conv_extract(data_dir, game_dir);
    /* the full-screen pictures (.BYT: 320 x 200, palette 0) */
    static const char *const BYT[] = {"MAIN", "OPSCR", "CHARGEN", "CONV", "PRES1", "PRES2", "WIN1", "WIN2", "BLNKMAP"};
    for (int i = 0; i < COUNT(BYT); i++) {
        char dst[700];
        snprintf(dst, sizeof dst, "%s\\ui\\screens\\%s.png", data_dir, BYT[i]);
        if (file_exists(dst)) continue;
        snprintf(path, sizeof path, "%s\\DATA\\%s.BYT", game_dir, BYT[i]);
        mark = arena_mark();
        Bytes b = file_read(path);
        if (b.size >= 64000) {
            u8 *rgba = arena_alloc(64000 * 4);
            for (int p = 0; p < 64000; p++) memcpy(rgba + p * 4, pal + b.data[p] * 3, 3), rgba[p * 4 + 3] = 255;
            if (png_write(dst, rgba, 320, 200, 4)) total++;
        }
        arena_reset(mark);
    }
    /* the light sources' table of OBJECTS.DAT (at 0xd62: 16 entries of duration, brightness for objects
     * 0x90-0x9F) -> data\objects\lights.txt */
    snprintf(path, sizeof path, "%s\\objects\\lights.txt", data_dir);
    if (!file_exists(path)) {
        char src[700];
        snprintf(src, sizeof src, "%s\\DATA\\OBJECTS.DAT", game_dir);
        mark = arena_mark();
        Bytes b = file_read(src);
        if (b.size >= 0xd62 + 32) {
            char text[2048];
            size_t n = (size_t)snprintf(text, sizeof text,
                                        "# uo-new light sources (from OBJECTS.DAT at 0xd62), one line per object 144-159:\r\n"
                                        "#   light OBJECT DURATION BRIGHTNESS\r\n"
                                        "# DURATION: a lit light loses one point of quality (its fuel, up to 63) every DURATION\r\n"
                                        "# ticks of 21 seconds (0 = never burns out). BRIGHTNESS 0-4 (0 = no light: the unlit ones).\r\n"
                                        "# Edit freely; delete to extract it again.\r\n");
            for (int k = 0; k < 16; k++)
                n += (size_t)snprintf(text + n, sizeof text - n, "light %d %d %d\r\n", 0x90 + k, b.data[0xd62 + k * 2],
                                      b.data[0xd62 + k * 2 + 1]);
            make_dirs_for(path);
            if (file_write(path, text, n)) total++;
        }
        arena_reset(mark);
    }
    /* every kind of object: COMOBJ.DAT (11 bytes each from offset 2) and OBJECTS.DAT's armour (0xb2: 32 x 4
     * for 0x20-0x3F) and container (0xd32: 16 x 3 for 0x80-0x8F) tables -> data\objects\objects.txt */
    snprintf(path, sizeof path, "%s\\objects\\objects.txt", data_dir);
    if (!file_exists(path) || !file_starts_with(path, "# uo-new objects v7")) {
        char src[700];
        snprintf(src, sizeof src, "%s\\DATA\\COMOBJ.DAT", game_dir);
        mark = arena_mark();
        Bytes c = file_read(src);
        snprintf(src, sizeof src, "%s\\DATA\\OBJECTS.DAT", game_dir);
        Bytes od = file_read(src);
        snprintf(src, sizeof src, "%s\\strings\\block0004.txt", data_dir);
        Bytes names = file_read(src);
        if (c.size >= 2 + 512 * 11 && od.size >= 0xd32 + 48) {
            size_t cap = 1 << 18, n = 0;
            char *text = arena_alloc(cap);
            n += (size_t)snprintf(text + n, cap - n,
                "# uo-new objects v7: what the original knows about each kind (from COMOBJ.DAT and OBJECTS.DAT). The engine\r\n"
                "# reads this file; edit freely; delete to extract it again.\r\n"
                "#   obj N  pick (can be picked up) kind (0 a thing: those with a plural stack, 1 a weapon or armour,\r\n"
                "#          3 a container) mass (tenths of a stone) value (gold)\r\n"
                "#          height radius  qclass (3: always the last quality word) qtype (quality words: strings block 5\r\n"
                "#          from qtype * 6)  look (1: \"You see ...\", 0: a decoration with its own text) owner (can belong\r\n"
                "#          to someone)   # raw COMOBJ bytes, name\r\n"
                "#   armour N  slot (0 helm, 1 chest, 2 gloves, 3 leggings, 4 boots, 9 ring, -1 a shield: a hand)\r\n"
                "#          protection durability\r\n"
                "#   container N  capacity (tenths of a stone, 0 no limit) accepts (-1 anything, an object, 512 runes,\r\n"
                "#          513 ammunition, 514 maps and scrolls, 515 food)\r\n"
                "#   food N  nutrition (what eating it adds to hunger, 0-255; below 0: a drink)\r\n"
                "#   weapon N  slash bash stab (damage of each attack) skill (3 sword, 4 axe, 5 mace, 6 unarmed)\r\n"
                "#          durability (255 never breaks) power_min rate power_max (the swing: power from min to max\r\n"
                "#          as the charge goes 0-100, the charge rising by rate every 1/16 s; damage x power / 128)\r\n"
                "#   critter N  hp armour (body arms legs head; 255 none) attack (skill) defense strength\r\n"
                "#          material (its record's byte 0x10: bits 6-7 its weapon, 4-5 its hide (1 metal: UW.EXE 0x25039's clang),\r\n"
                "#          0-3 its walking sound) death (byte 8 bits 0-2: 1 its dying sound, effect 6)\r\n"
                "#          a1 a2 a3 (each: bonus damage chance%%) xp (experience value: a kill gives 5-6 x it)\r\n"
                "#   launcher N  ammo (the object it shoots: 16 sling stones, 17 crossbow bolts, 18 arrows)\r\n");
            for (int k = 0; k < 512; k++) {
                const u8 *e = c.data + 2 + k * 11;
                u32 w = rd16(e + 1);
                char name[80] = "";
                /* the name: "k: text" in block 4 */
                char key[12];
                int kl = snprintf(key, sizeof key, "\n%d: ", k);
                for (size_t i = 0; i + (size_t)kl <= names.size; i++)
                    if (!memcmp(names.data + i, key, (size_t)kl)) {
                        size_t j = i + (size_t)kl, m = 0;
                        while (j < names.size && names.data[j] != '\r' && names.data[j] != '\n' && m < sizeof name - 1) name[m++] = (char)names.data[j++];
                        name[m] = 0;
                        break;
                    }
                n += (size_t)snprintf(text + n, cap - n,
                                      "obj %3d  pick %d kind %d mass %4u value %4u height %3d radius %d qclass %d qtype %2d look %d owner %d"
                                      "   # %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x  %s\r\n",
                                      k, (e[3] >> 5) & 1, (e[3] >> 6) & 3, (w >> 4) & 0xFFF, rd16(e + 4), e[0], w & 7, (e[6] >> 2) & 3,
                                      e[10] & 15, (e[10] >> 4) & 1, (e[7] >> 7) & 1, e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7],
                                      e[8], e[9], e[10], name);
            }
            static const int SLOT[10] = {-1, 1, -1, 3, 2, 4, -1, -1, 0, 9}; /* OBJECTS.DAT's body part -> our slot */
            for (int k = 0; k < 32; k++) {
                const u8 *e = od.data + 0xb2 + k * 4;
                if (!e[0] && !e[3] && e[1] == 0xff) continue;
                n += (size_t)snprintf(text + n, cap - n, "armour %3d  slot %d protection %d durability %d   # raw %02x %02x %02x %02x\r\n",
                                      0x20 + k, e[3] < 10 ? SLOT[e[3]] : -1, e[0], e[1], e[0], e[1], e[2], e[3]);
            }
            for (int k = 0; k < 16; k++) {
                const u8 *e = od.data + 0xd32 + k * 3;
                n += (size_t)snprintf(text + n, cap - n, "container %3d  capacity %d accepts %d\r\n", 0x80 + k, e[0], rd16s(e + 1));
            }
            for (int k = 0; k < 16; k++) { /* OBJECTS.DAT 2: 16 hand weapons x 8 bytes */
                const u8 *e = od.data + 2 + k * 8;
                n += (size_t)snprintf(text + n, cap - n, "weapon %3d  slash %d bash %d stab %d skill %d durability %d power_min %d rate %d power_max %d\r\n",
                                      k, e[0], e[1], e[2], e[6], e[7], e[3], e[4], e[5]);
            }
            for (int k = 0; k < 64; k++) { /* OBJECTS.DAT 0x132: 64 critters x 48 bytes (0x40-0x7F) */
                const u8 *e = od.data + 0x132 + k * 48;
                if (!e[4]) continue;
                n += (size_t)snprintf(text + n, cap - n,
                                      "critter %3d  hp %d armour %d %d %d %d attack %d defense %d strength %d a1 %d %d %d a2 %d %d %d a3 %d %d %d xp %d material %d death %d\r\n",
                                      0x40 + k, e[4], e[0], e[1], e[2], e[3], e[0x11], e[0x12], e[5], (int)(i8)e[0x13], e[0x14], e[0x15],
                                      (int)(i8)e[0x16], e[0x17], e[0x18], (int)(i8)e[0x19], e[0x1a], e[0x1b], (int)rd16(e + 0x28), e[0x10], e[8] & 7);
            }
            for (int k = 8; k < 16; k++) { /* OBJECTS.DAT 0x82: 0x18-0x1F launchers, byte 2 = the ammunition */
                const u8 *e = od.data + 0x82 + k * 3;
                if (e[2] <= 2) n += (size_t)snprintf(text + n, cap - n, "launcher %3d  ammo %d\r\n", 0x10 + k, 0x10 + e[2]);
            }
            for (int k = 0; k < 16; k++) /* OBJECTS.DAT 0xd82: a signed byte for 0xB0-0xBF */
                n += (size_t)snprintf(text + n, cap - n, "food %3d  nutrition %d\r\n", 0xb0 + k, (int)(i8)od.data[0xd82 + k]);
            make_dirs_for(path);
            if (file_write(path, text, n)) total++;
        }
        arena_reset(mark);
    }
    return total;
}

int extract_missing(const char *data_dir, const char *game_dir) {
    int more = extract_more(data_dir, game_dir);
    if (more < 0) return -1;
    int base = extract_core(data_dir, game_dir);
    return base < 0 ? -1 : base + more;
}

int extract_core(const char *data_dir, const char *game_dir) {
    static char names[400][48];
    int n = extract_list(names, 400), written = 0;
    char path[700];
    bool any = false;
    for (int i = 0; i < n && !any; i++) {
        snprintf(path, sizeof path, "%s\\%s", data_dir, names[i]);
        any = !file_exists(path);
    }
    if (!any) return 0;
    size_t mark = arena_mark();
    Originals o;
    if (!load_originals(game_dir, &o)) {
        arena_reset(mark);
        return -1;
    }
    u8 pal[768];
    palette0(&o, pal);
    for (int i = 0; i < N_WALLS + N_FLOORS; i++) {
        bool wall = i < N_WALLS;
        int k = wall ? i : i - N_WALLS;
        snprintf(path, sizeof path, "%s\\%s", data_dir, names[i]);
        if (file_exists(path)) continue;
        int size = 0;
        const u8 *t = tr_texture(wall ? o.w64 : o.f32, k, &size);
        if (!t) return arena_reset(mark), logf_("extract: %s texture %d missing\n", wall ? "W64.TR" : "F32.TR", k), -1;
        if (!png_write_indexed(path, t, size, size, pal)) return arena_reset(mark), logf_("extract: cannot write %s\n", path), -1;
        written++;
    }
    for (int lv = 1; lv <= LEVELS; lv++) {
        snprintf(path, sizeof path, "%s\\levels\\level%d.txt", data_dir, lv);
        if (file_exists(path)) continue;
        size_t ls = 0, os = 0, ts = 0;
        const u8 *lev = ark_block(o.lev, lv - 1, &ls), *ovl = ark_block(o.lev, lv + 8, &os), *tm = ark_block(o.lev, lv + 17, &ts);
        if (!lev || ls != LEVEL_BLOCK || !tm || ts < 122)
            return arena_reset(mark), logf_("extract: LEV.ARK level %d is not as expected\n", lv), -1;
        if (!write_level(path, lv, lev, ovl, os, tm)) return arena_reset(mark), logf_("extract: cannot write %s\n", path), -1;
        written++;
    }
    arena_reset(mark);
    return written;
}
