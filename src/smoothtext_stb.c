/* smoothtext_stb.c: smoothtext.h on Linux (libuwview.so, tools\buildview_linux.py), with stb_truetype in place of
 * GDI. The same sets as smoothtext.c (224 glyphs, characters 32-255 of the Windows ANSI page, each a coverage cell
 * cw x ch with the letter at pad, the line's top at the cell's top), the same blending, the same sizes: em is the
 * letter height as GDI's CreateFont(-em) takes it (the font's em square in pixels), the line's height its Windows
 * ascent + descent (OS/2 usWinAscent + usWinDescent, as GDI's tmHeight), each advance rounded to whole pixels (as
 * GetCharWidth32).
 *
 * The faces (the user, 2026-11-15, through fable uwr: the free fonts for the Windows ones) keep the Windows names in
 * the Esc menu and settings.txt, so one home\ serves both systems; each is drawn from its stand-in, offered when its
 * file is found: freefonts\ beside this library (or up to four folders up: uo-new's), else the system's fonts.
 *   Segoe UI -> Selawik    Verdana, Tahoma -> DejaVu Sans    Georgia -> Gelasio    Calibri -> Carlito
 *   Cambria -> Caladea     Consolas -> Inconsolata
 * Windows faces without an approved stand-in are not offered (as Windows leaves out a face the PC lacks). The fonts
 * folder's own files (st_load_font_dir) are offered by their family names, as on Windows. */
#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "smoothtext.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

typedef struct {
    int face, em, cw, ch, pad, height;
    unsigned char *cov;         /* 224 cells of cw x ch */
    short adv[224];
    unsigned used;
} Set;
#define NSETS 12
static Set sets[NSETS];
static unsigned set_clock;
static char font_name[64];

/* the Windows ANSI page's 0x80-0x9F as Unicode (the rest of 32-255 is Latin-1, its own code) */
static const unsigned short CP1252[32] = {0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
                                          0x2039, 0x0152, 0, 0x017D, 0, 0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013,
                                          0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178};
static int code_of(int c) { return c >= 0x80 && c < 0xA0 ? CP1252[c - 0x80] : c; }

/* the Windows faces and their stand-ins: the regular file, the semibold's (NULL: the regular) */
static const struct { const char *name, *file, *bold; } STANDIN[] = {
    {"Segoe UI", "selawk.ttf", "selawksb.ttf"},           {"Verdana", "dejavusans.ttf", "dejavusans-bold.ttf"},
    {"Georgia", "gelasio.ttf", NULL},                     {"Tahoma", "dejavusans.ttf", "dejavusans-bold.ttf"},
    {"Calibri", "carlito-regular.ttf", "carlito-bold.ttf"}, {"Cambria", "caladea-regular.ttf", "caladea-bold.ttf"},
    {"Consolas", "inconsolata.ttf", NULL},
};
/* where a stand-in's file may also be, by the system's packages (Linux Mint has DejaVu and Carlito) */
static const struct { const char *file, *path; } SYSTEM[] = {
    {"dejavusans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"},
    {"dejavusans-bold.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"},
    {"carlito-regular.ttf", "/usr/share/fonts/truetype/crosextra/Carlito-Regular.ttf"},
    {"carlito-bold.ttf", "/usr/share/fonts/truetype/crosextra/Carlito-Bold.ttf"},
    {"caladea-regular.ttf", "/usr/share/fonts/truetype/crosextra/Caladea-Regular.ttf"},
    {"caladea-bold.ttf", "/usr/share/fonts/truetype/crosextra/Caladea-Bold.ttf"},
};

#define MAXFACES 64
static struct { char name[32], path[512], bold[512]; } faces[MAXFACES];
static int nfaces = -1;
static char ff_dir[512];   /* freefonts\ found (or "") */

static int file_there(const char *p) { struct stat st; return p && *p && !stat(p, &st) && S_ISREG(st.st_mode); }

static void find_freefonts(void)
{
    char p[512];
    ff_dir[0] = 0;
    if (!uwos_self_dir((const void *)find_freefonts, p, sizeof p)) return;
    for (int up = 0; up < 5; up++) {
        if (up) {
            char *cut = strrchr(p, '/');
            if (!cut) return;
            *cut = 0;
        }
        char probe[600];
        snprintf(probe, sizeof probe, "%s/freefonts/selawk.ttf", p);
        if (file_there(probe)) {
            snprintf(ff_dir, sizeof ff_dir, "%s/freefonts", p);
            return;
        }
    }
}
/* a stand-in's file: freefonts\ first, then the system's; "" when neither */
static void standin_path(const char *file, char *out, size_t n)
{
    out[0] = 0;
    if (!file) return;
    if (ff_dir[0]) {
        snprintf(out, n, "%s/%s", ff_dir, file);
        if (file_there(out)) return;
    }
    for (unsigned i = 0; i < sizeof SYSTEM / sizeof SYSTEM[0]; i++)
        if (!strcmp(SYSTEM[i].file, file) && file_there(SYSTEM[i].path)) {
            snprintf(out, n, "%s", SYSTEM[i].path);
            return;
        }
    out[0] = 0;
}
static void faces_add(const char *name, const char *path, const char *bold)
{
    if (!name[0] || !path[0] || nfaces >= MAXFACES) return;
    for (int i = 0; i < nfaces; i++) if (!strcasecmp(faces[i].name, name)) return;
    snprintf(faces[nfaces].name, sizeof faces[nfaces].name, "%s", name);
    snprintf(faces[nfaces].path, sizeof faces[nfaces].path, "%s", path);
    snprintf(faces[nfaces].bold, sizeof faces[nfaces].bold, "%s", bold && *bold ? bold : path);
    nfaces++;
}
static void faces_init(void)
{
    if (nfaces >= 0) return;
    nfaces = 0;
    find_freefonts();
    for (unsigned i = 0; i < sizeof STANDIN / sizeof STANDIN[0]; i++) {
        char p[512], b[512];
        standin_path(STANDIN[i].file, p, sizeof p);
        standin_path(STANDIN[i].bold, b, sizeof b);
        if (p[0]) faces_add(STANDIN[i].name, p, b);
    }
    if (!nfaces) fprintf(stderr, "smoothtext: no font found (freefonts\\ beside the program, or the system's DejaVu Sans)\n");
}
static int face_index(const char *name)
{
    faces_init();
    for (int i = 0; i < nfaces; i++) if (!strcasecmp(faces[i].name, name)) return i;
    return -1;
}
const char *st_font_name(void)
{
    if (!font_name[0]) {
        faces_init();
        const char *n = face_index("Segoe UI") >= 0 ? "Segoe UI" : face_index("Tahoma") >= 0 ? "Tahoma" : nfaces > 0 ? faces[0].name : "Tahoma";
        snprintf(font_name, sizeof font_name, "%s", n);
    }
    return font_name;
}

static unsigned char *read_all(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *b = len > 0 && len < (32L << 20) ? malloc((size_t)len) : NULL;
    size_t got = b ? fread(b, 1, (size_t)len, f) : 0;
    fclose(f);
    if (b && got != (size_t)len) free(b), b = NULL;
    if (b) *n = got;
    return b;
}

static unsigned rd16be(const unsigned char *p) { return (unsigned)p[0] << 8 | p[1]; }
static unsigned rd32be(const unsigned char *p) { return (unsigned)p[0] << 24 | (unsigned)p[1] << 16 | (unsigned)p[2] << 8 | p[3]; }
/* a TrueType / OpenType file's family name (its name table's name 1, the Windows English one first), "" if none
   (smoothtext.c's) */
static void ttf_family(const unsigned char *b, size_t n, char *out, size_t cap)
{
    out[0] = 0;
    size_t base = 0;
    if (n >= 16 && !memcmp(b, "ttcf", 4)) base = rd32be(b + 12);
    if (base + 12 > n) return;
    unsigned nt = rd16be(b + base + 4);
    for (unsigned t = 0; t < nt && base + 12 + t * 16 + 16 <= n; t++) {
        const unsigned char *r = b + base + 12 + t * 16;
        if (memcmp(r, "name", 4)) continue;
        size_t off = rd32be(r + 8);
        if (off + 6 > n) return;
        unsigned cnt = rd16be(b + off + 2), so = rd16be(b + off + 4);
        int best = -1;
        for (unsigned k = 0; k < cnt && off + 6 + k * 12 + 12 <= n; k++) {
            const unsigned char *e = b + off + 6 + k * 12;
            unsigned plat = rd16be(e), lang = rd16be(e + 4), id = rd16be(e + 6), len = rd16be(e + 8), at = rd16be(e + 10);
            if (id != 1) continue;
            int score = plat == 3 ? (lang == 0x409 ? 3 : 2) : plat == 1 ? 1 : 0;
            if (score <= best) continue;
            size_t s = off + so + at, o = 0;
            if (s + len > n) continue;
            for (unsigned c = 0; c < len && o + 1 < cap; c += plat == 3 || plat == 0 ? 2 : 1) {
                unsigned ch = plat == 3 || plat == 0 ? rd16be(b + s + c) : b[s + c];
                out[o++] = ch >= 32 && ch < 127 ? (char)ch : '?';
            }
            out[o] = 0, best = score;
        }
        return;
    }
}
void st_load_font_dir(const char *dir)
{
    faces_init();
    char d[600];
    snprintf(d, sizeof d, "%s", dir ? dir : "");
    for (char *c = d; *c; c++) if (*c == '\\') *c = '/';
    DIR *h = opendir(d);
    if (!h) return;
    for (struct dirent *e; (e = readdir(h));) {
        size_t l = strlen(e->d_name);
        if (l < 5 || (strcasecmp(e->d_name + l - 4, ".ttf") && strcasecmp(e->d_name + l - 4, ".otf"))) continue;
        char path[1200], fam[64];
        snprintf(path, sizeof path, "%s/%s", d, e->d_name);
        size_t n = 0;
        unsigned char *b = read_all(path, &n);
        fam[0] = 0;
        if (b) ttf_family(b, n, fam, sizeof fam);
        free(b);
        if (fam[0]) faces_add(fam, path, path);
    }
    closedir(h);
}
int st_face_count(void) { faces_init(); return nfaces > 0 ? nfaces : 1; }
const char *st_face_label(int i) { faces_init(); return nfaces <= 0 ? "Tahoma" : i >= 0 && i < nfaces ? faces[i].name : faces[0].name; }
int st_face_find(const char *name) { return face_index(name); }
void st_set_face(int i)
{
    const char *want = st_face_label(i);
    if (!strcmp(want, st_font_name())) return;
    snprintf(font_name, sizeof font_name, "%s", want);
    for (int k = 0; k < NSETS; k++) free(sets[k].cov), sets[k].cov = NULL;
}
static int size_pct = 100;
void st_set_size_pct(int pct) { size_pct = pct < 20 ? 20 : pct > 300 ? 300 : pct; }
int st_size_pct(void) { return size_pct; }

/* the font files read, kept (a set is made from them whenever one is wanted) */
static struct { char path[512]; unsigned char *data; stbtt_fontinfo info; int ok; } files[16];
static stbtt_fontinfo *font_of(const char *path)
{
    int free_slot = -1;
    for (int i = 0; i < 16; i++) {
        if (files[i].data && !strcmp(files[i].path, path)) return files[i].ok ? &files[i].info : NULL;
        if (!files[i].data && free_slot < 0) free_slot = i;
    }
    if (free_slot < 0) free_slot = 15, free(files[15].data), files[15].data = NULL;
    size_t n = 0;
    unsigned char *b = read_all(path, &n);
    if (!b) return NULL;
    snprintf(files[free_slot].path, sizeof files[free_slot].path, "%s", path);
    files[free_slot].data = b;
    files[free_slot].ok = stbtt_InitFont(&files[free_slot].info, b, stbtt_GetFontOffsetForIndex(b, 0));
    if (!files[free_slot].ok) fprintf(stderr, "smoothtext: %s is not a font stb_truetype reads\n", path);
    return files[free_slot].ok ? &files[free_slot].info : NULL;
}

/* a font's scale for letter height em (GDI's CreateFont(-em)), its baseline below the line's top and the line's
   height (the Windows ascent + descent: OS/2 usWinAscent + usWinDescent, as GDI's tmAscent and tmHeight) */
static void font_metrics(stbtt_fontinfo *fi, int em, float *sc, int *base, int *height)
{
    int asc, desc, gap, wa = 0, wd = 0;
    stbtt_uint32 os2 = stbtt__find_table(fi->data, (stbtt_uint32)fi->fontstart, "OS/2");
    *sc = stbtt_ScaleForMappingEmToPixels(fi, (float)em);
    if (os2) wa = ttUSHORT(fi->data + os2 + 74), wd = ttUSHORT(fi->data + os2 + 76);
    if (wa + wd <= 0) stbtt_GetFontVMetrics(fi, &asc, &desc, &gap), wa = asc, wd = -desc;
    *base = (int)(wa * *sc + 0.5f), *height = *base + (int)(wd * *sc + 0.5f);
}
/* character c (the ANSI page's) drawn as coverage into cell (cw x ch, cleared by the caller) with its pen at pad and
   the line's top at the cell's top; returns its advance in whole pixels (as GetCharWidth32) */
static int glyph_cell(stbtt_fontinfo *fi, float sc, int base, int c, unsigned char *cell, int cw, int ch, int pad)
{
    int cp = code_of(c), g = cp ? stbtt_FindGlyphIndex(fi, cp) : 0, adv = 0, lsb = 0;
    if (!cp) g = stbtt_FindGlyphIndex(fi, '?');   /* (the page's unused codes: GDI shows its default) */
    stbtt_GetGlyphHMetrics(fi, g, &adv, &lsb);
    int x0, y0, x1, y1;
    stbtt_GetGlyphBitmapBox(fi, g, sc, sc, &x0, &y0, &x1, &y1);
    int gw = x1 - x0, gh = y1 - y0;
    unsigned char *bm = gw > 0 && gh > 0 ? malloc((size_t)gw * gh) : NULL;
    if (bm) {
        stbtt_MakeGlyphBitmap(fi, bm, gw, gh, gw, sc, sc, g);
        for (int y = 0; y < gh; y++) {
            int cy = base + y0 + y;
            if (cy < 0 || cy >= ch) continue;
            for (int x = 0; x < gw; x++) {
                int cx = pad + x0 + x;
                if (cx >= 0 && cx < cw) cell[(size_t)cy * cw + cx] = bm[(size_t)y * gw + x];
            }
        }
        free(bm);
    }
    return (int)(adv * sc + 0.5f);
}

static Set *set_get(int face, int em)
{
    Set *s = NULL, *old = &sets[0];
    if (em < 6) em = 6;
    if (em > 96) em = 96;
    for (int i = 0; i < NSETS; i++) {
        if (sets[i].cov && sets[i].face == face && sets[i].em == em) { sets[i].used = ++set_clock; return &sets[i]; }
        if (!sets[i].cov && !s) s = &sets[i];
        if (sets[i].used < old->used) old = &sets[i];
    }
    int k = face_index(st_font_name());
    if (k < 0) return NULL;
    stbtt_fontinfo *fi = font_of(face == ST_SEMIBOLD ? faces[k].bold : faces[k].path);
    if (!fi) return NULL;
    if (!s) s = old, free(s->cov), s->cov = NULL;
    s->face = face, s->em = em, s->pad = em / 3 + 2;
    s->cw = em * 2 + 2 * s->pad, s->ch = em * 2;
    s->cov = calloc(224, (size_t)s->cw * s->ch);
    if (!s->cov) return NULL;
    float sc;
    int base, height;
    font_metrics(fi, em, &sc, &base, &height);
    s->height = height < s->ch ? height : s->ch;
    for (int c = 32; c < 256; c++)
        s->adv[c - 32] = (short)glyph_cell(fi, sc, base, c, s->cov + (size_t)(c - 32) * s->cw * s->ch, s->cw, s->ch, s->pad);
    s->used = ++set_clock;
    return s;
}

int st_height(int face, int em)
{
    Set *s = set_get(face, em);
    return s ? s->height : em;
}

int st_width(int face, int em, const char *t)
{
    Set *s = set_get(face, em);
    int w = 0;
    if (!s || !t) return 0;
    for (; *t; t++) {
        int c = (unsigned char)*t;
        if (c >= 32) w += s->adv[c - 32];
    }
    return w;
}

int st_draw(unsigned *rgba, int w, int h, int x, int y, int face, int em, const char *t, unsigned rgb)
{
    Set *s = set_get(face, em);
    unsigned r = rgb >> 16 & 255, g = rgb >> 8 & 255, b = rgb & 255;
    if (!s || !t || !rgba) return x;
    for (; *t; t++) {
        int c = (unsigned char)*t;
        if (c < 32) continue;
        const unsigned char *cell = s->cov + (size_t)(c - 32) * s->cw * s->ch;
        int x0 = x - s->pad;
        for (int gy = 0; gy < s->ch; gy++) {
            int py = y + gy;
            if (py < 0 || py >= h) continue;
            unsigned *row = rgba + (size_t)py * w;
            const unsigned char *crow = cell + (size_t)gy * s->cw;
            for (int gx = 0; gx < s->cw; gx++) {
                unsigned a = crow[gx];
                int px = x0 + gx;
                if (!a || px < 0 || px >= w) continue;
                unsigned d = row[px], na = 255 - a, da = d >> 24;
                unsigned nr = (r * a + (d & 255) * na) / 255, ng = (g * a + (d >> 8 & 255) * na) / 255,
                         nb = (b * a + (d >> 16 & 255) * na) / 255;
                if (a > da) da = a;
                row[px] = da << 24 | nr | ng << 8 | nb << 16;
            }
        }
        x += s->adv[c - 32];
    }
    return x;
}

int st_game_em(int window_h)
{
    int e = (window_h * 19 * size_pct / 100 + 540) / 1080;
    return e < 6 ? 6 : e > 60 ? 60 : e;
}

/* for uwconv_draw.c's smooth conversation letters (Tahoma through GDI on Windows; here Tahoma's stand-in, else the
   interface's face): character c into cell as glyph_cell draws it; its advance, and the font's ascent (GDI's
   tmAscent) into *ascent. 0 when there is no font. */
int st_conv_glyph(int em, int c, unsigned char *cell, int cw, int ch, int pad, int *adv, int *ascent)
{
    int k = face_index("Tahoma");
    if (k < 0) k = face_index(st_font_name());
    stbtt_fontinfo *fi = k < 0 ? NULL : font_of(faces[k].path);
    if (!fi) return 0;
    float sc;
    int base, height;
    font_metrics(fi, em, &sc, &base, &height);
    *adv = glyph_cell(fi, sc, base, c, cell, cw, ch, pad), *ascent = base;
    return 1;
}
#endif
