/* smoothtext.c: see smoothtext.h. Each face and size is a set of 224 glyphs (characters 32-255 of the Windows
 * ANSI page), each drawn once by GDI in white on black into a small bitmap whose red channel is kept as the
 * letter's coverage; drawing is then a blend of the colour by that coverage. Up to 12 sets are kept (the menu
 * uses four); the oldest is given up for a new one. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "smoothtext.h"

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

static int CALLBACK font_seen(const LOGFONTA *lf, const TEXTMETRICA *tm, DWORD type, LPARAM p)
{
    (void)lf, (void)tm, (void)type;
    *(int *)p = 1;
    return 0;
}
static int face_there(const char *name)
{
    HDC dc = CreateCompatibleDC(NULL);
    LOGFONTA lf = {0};
    int found = 0;
    lf.lfCharSet = DEFAULT_CHARSET;
    strncpy(lf.lfFaceName, name, sizeof lf.lfFaceName - 1);
    if (dc) EnumFontFamiliesExA(dc, &lf, font_seen, (LPARAM)&found, 0), DeleteDC(dc);
    return found;
}
const char *st_font_name(void)
{
    if (!font_name[0]) strcpy(font_name, face_there("Segoe UI") ? "Segoe UI" : "Tahoma");
    return font_name;
}
/* the faces the Esc menu's Font offers (the user, 2026-11-14: "change the font to something that is easy to read ...
   options in the esc menu to change the font and font size", then "could we include other fonts?"): Windows' own
   readable faces that this PC has, then every font file in the fonts folder beside the game (st_load_font_dir: loaded
   for this program only, AddFontResourceEx FR_PRIVATE; nothing installed), by its family name. Choosing another drops
   every set made (they are made again in the new face as they are used). */
static const char *const WIN_FACES[] = { "Segoe UI", "Verdana", "Georgia", "Tahoma", "Palatino Linotype", "Calibri",
                                         "Cambria", "Candara", "Constantia", "Corbel", "Trebuchet MS", "Book Antiqua",
                                         "Garamond", "Consolas" };
#define MAXFACES 64
static char faces[MAXFACES][32];
static int nfaces = -1;
static void faces_add(const char *name)
{
    if (!name[0] || nfaces >= MAXFACES) return;
    for (int i = 0; i < nfaces; i++) if (!_stricmp(faces[i], name)) return;
    strncpy(faces[nfaces], name, 31), faces[nfaces][31] = 0, nfaces++;
}
static void faces_init(void)
{
    if (nfaces >= 0) return;
    nfaces = 0;
    for (unsigned i = 0; i < sizeof WIN_FACES / sizeof WIN_FACES[0]; i++)
        if (face_there(WIN_FACES[i])) faces_add(WIN_FACES[i]);
    if (!nfaces) faces_add("Tahoma");
}
static unsigned rd16be(const unsigned char *p) { return (unsigned)p[0] << 8 | p[1]; }
static unsigned rd32be(const unsigned char *p) { return (unsigned)p[0] << 24 | (unsigned)p[1] << 16 | (unsigned)p[2] << 8 | p[3]; }
/* a TrueType / OpenType file's family name (its name table's name 1, the Windows English one first), "" if none */
static void ttf_family(const unsigned char *b, size_t n, char *out, size_t cap)
{
    out[0] = 0;
    size_t base = 0;
    if (n >= 16 && !memcmp(b, "ttcf", 4)) base = rd32be(b + 12);   /* a collection: its first font */
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
    char pat[MAX_PATH];
    WIN32_FIND_DATAA fd;
    faces_init();
    for (int pass = 0; pass < 2; pass++) {
        snprintf(pat, sizeof pat, "%s\\*.%s", dir, pass ? "otf" : "ttf");
        HANDLE h = FindFirstFileA(pat, &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            char path[MAX_PATH], fam[64];
            snprintf(path, sizeof path, "%s\\%s", dir, fd.cFileName);
            FILE *f = fopen(path, "rb");
            if (!f) continue;
            fseek(f, 0, SEEK_END);
            long n = ftell(f);
            fseek(f, 0, SEEK_SET);
            unsigned char *b = n > 0 && n < (32L << 20) ? malloc((size_t)n) : NULL;
            size_t got = b ? fread(b, 1, (size_t)n, f) : 0;
            fclose(f);
            fam[0] = 0;
            if (b && got == (size_t)n) ttf_family(b, got, fam, sizeof fam);
            free(b);
            if (fam[0] && AddFontResourceExA(path, FR_PRIVATE, 0) > 0) faces_add(fam);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
}
int st_face_count(void) { faces_init(); return nfaces; }
const char *st_face_label(int i) { faces_init(); return i >= 0 && i < nfaces ? faces[i] : faces[0]; }
int st_face_find(const char *name)
{
    faces_init();
    for (int i = 0; i < nfaces; i++) if (!_stricmp(faces[i], name)) return i;
    return -1;
}
void st_set_face(int i)
{
    const char *want = st_face_label(i);
    if (!strcmp(want, st_font_name())) return;
    strncpy(font_name, want, sizeof font_name - 1), font_name[sizeof font_name - 1] = 0;
    for (int k = 0; k < NSETS; k++) free(sets[k].cov), sets[k].cov = NULL;
}
static int size_pct = 100;
void st_set_size_pct(int pct) { size_pct = pct < 20 ? 20 : pct > 300 ? 300 : pct; }
int st_size_pct(void) { return size_pct; }

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
    if (!s) s = old, free(s->cov), s->cov = NULL;
    s->face = face, s->em = em, s->pad = em / 3 + 2;
    s->cw = em * 2 + 2 * s->pad, s->ch = em * 2;
    HDC dc = CreateCompatibleDC(NULL);
    if (!dc) return NULL;
    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader, bi.bmiHeader.biWidth = s->cw, bi.bmiHeader.biHeight = -s->ch;
    bi.bmiHeader.biPlanes = 1, bi.bmiHeader.biBitCount = 32, bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    HBITMAP bm = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    HFONT f = CreateFontA(-em, 0, 0, 0, face == ST_SEMIBOLD ? FW_SEMIBOLD : FW_NORMAL, 0, 0, 0, ANSI_CHARSET, OUT_TT_PRECIS,
                          CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, st_font_name());
    s->cov = bm && f ? calloc(224, (size_t)s->cw * s->ch) : NULL;
    if (s->cov) {
        HGDIOBJ ob = SelectObject(dc, bm), of = SelectObject(dc, f);
        TEXTMETRICA tm;
        SetTextColor(dc, RGB(255, 255, 255)), SetBkColor(dc, RGB(0, 0, 0)), SetBkMode(dc, OPAQUE);
        GetTextMetricsA(dc, &tm);
        s->height = tm.tmHeight < s->ch ? tm.tmHeight : s->ch;
        for (int c = 32; c < 256; c++) {
            char ch = (char)c;
            int a = 0;
            unsigned char *cell = s->cov + (size_t)(c - 32) * s->cw * s->ch;
            memset(bits, 0, (size_t)s->cw * s->ch * 4);
            TextOutA(dc, s->pad, 0, &ch, 1);
            GdiFlush();
            for (int i = 0; i < s->cw * s->ch; i++) cell[i] = ((const unsigned char *)bits)[i * 4 + 2];
            if (!GetCharWidth32A(dc, (UINT)c, (UINT)c, &a)) a = em / 2;
            s->adv[c - 32] = (short)a;
        }
        SelectObject(dc, ob), SelectObject(dc, of);
    }
    if (f) DeleteObject(f);
    if (bm) DeleteObject(bm);
    DeleteDC(dc);
    if (!s->cov) return NULL;
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
    return e < 6 ? 6 : e > 60 ? 60 : e;   /* (6: the smallest a face is drawn at, set_get) */
}
