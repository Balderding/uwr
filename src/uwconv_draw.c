/* uwconv_draw.c: a conversation in our display, drawn (the backend's port\win32\uwconv.c lays it out). The interface
 * layer (uw by uh, RGBA) dims our 3D view's last frame everywhere and shows the pieces of the original's screen (src,
 * sw by sh, 0x00RRGGBB) given as n rows of six ints: the screen rectangle x, y, w, h and the layer point dx, dy, each
 * drawn scale times, opaque, with a dark edge round it. The screen's own black (0x000004: palette 0, what is round
 * its panels) is left dimmed, so that the panels keep their own outlines. dim: the dimming's alpha, 0-255. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define UWV_API __declspec(dllexport)
#else
#define UWV_API __attribute__((visibility("default")))
#endif

static unsigned *buf;
static int bw, bh;

UWV_API const unsigned char *uwv_conv(const unsigned *src, int sw, int sh, int uw, int uh, const int *pieces, int n,
                                      int scale, int dim)
{
    if (!src || uw <= 0 || uh <= 0 || scale < 1) return NULL;
    if (!buf || bw != uw || bh != uh) {
        free(buf);
        buf = malloc((size_t)uw * (size_t)uh * 4);
        bw = uw, bh = uh;
        if (!buf) return NULL;
    }
    unsigned shade = (unsigned)(dim & 0xFF) << 24;
    for (size_t i = 0, all = (size_t)uw * (size_t)uh; i < all; i++) buf[i] = shade;
    int edge = scale > 1 ? scale / 2 + 1 : 1;
    for (int p = 0; p < n; p++) {     /* the dark edges first, so that a piece next to another covers its edge */
        const int *q = pieces + p * 6;
        int x0 = q[4] - edge, y0 = q[5] - edge, x1 = q[4] + q[2] * scale + edge, y1 = q[5] + q[3] * scale + edge;
        for (int y = y0 < 0 ? 0 : y0; y < y1 && y < uh; y++)
            for (int x = x0 < 0 ? 0 : x0; x < x1 && x < uw; x++)
                if (x < q[4] || y < q[5] || x >= x1 - edge || y >= y1 - edge) buf[(size_t)y * uw + x] = 0xE0000000u;
    }
    for (int p = 0; p < n; p++) {
        const int *q = pieces + p * 6;
        for (int y = 0; y < q[3] * scale; y++) {
            int ty = q[5] + y, ry = q[1] + y / scale;
            if (ty < 0 || ty >= uh || ry < 0 || ry >= sh) continue;
            const unsigned *srow = src + (size_t)ry * sw;
            unsigned *drow = buf + (size_t)ty * uw;
            for (int x = 0; x < q[2] * scale; x++) {
                int tx = q[4] + x, rx = q[0] + x / scale;
                if (tx < 0 || tx >= uw || rx < 0 || rx >= sw) continue;
                unsigned c = srow[rx] & 0xFFFFFF;
                if (c == 0x000004) continue;            /* the screen's black round the panels: the edge shows */
                drow[tx] = 0xFF000000u | (c >> 16 & 0xFF) | (c & 0xFF00) | (c & 0xFF) << 16;
            }
        }
    }
    return (const unsigned char *)buf;
}

/* a cutscene in the 3D window (the death ...), alone: the screen's rectangle x, y, w, h drawn scale times at dx, dy over
 * our view's last frame dimmed (dim), only where the cutscene painted: not where the screen is the original's own art
 * there (data\ui\screens\MAIN.png: the window's frame and ornaments), nor its black (colour 0); no edge round it */
#include "ui.h"
UWV_API const unsigned char *uwv_cutscene(const unsigned *src, int sw, int sh, int uw, int uh, int x0, int y0, int w,
                                          int h, int dx, int dy, int scale, int dim)
{
    const Pic *art = ui_pic_file("ui\\screens\\MAIN.png");
    if (!src || uw <= 0 || uh <= 0 || scale < 1) return NULL;
    if (!buf || bw != uw || bh != uh) {
        free(buf);
        buf = malloc((size_t)uw * (size_t)uh * 4);
        bw = uw, bh = uh;
        if (!buf) return NULL;
    }
    unsigned shade = (unsigned)(dim & 0xFF) << 24;
    for (size_t i = 0, all = (size_t)uw * (size_t)uh; i < all; i++) buf[i] = shade;
    for (int y = 0; y < h * scale; y++) {
        int ty = dy + y, ry = y0 + y / scale;
        if (ty < 0 || ty >= uh || ry < 0 || ry >= sh) continue;
        for (int x = 0; x < w * scale; x++) {
            int tx = dx + x, rx = x0 + x / scale;
            if (tx < 0 || tx >= uw || rx < 0 || rx >= sw) continue;
            unsigned c = src[(size_t)ry * sw + rx] & 0xFFFFFF;
            if (c == 0x000004 || c == 0) continue;   /* palette colour 0 (the cutscene's empty): our view shows */
            if (art && rx < art->w && ry < art->h) {
                const unsigned char *a = art->rgba + ((size_t)ry * art->w + rx) * 4;
                int d = abs((int)(c >> 16) - a[0]) + abs((int)(c >> 8 & 0xFF) - a[1]) + abs((int)(c & 0xFF) - a[2]);
                if (d < 24) continue;                                    /* the original's own frame there */
            }
            buf[(size_t)ty * uw + tx] = 0xFF000000u | (c >> 16 & 0xFF) | (c & 0xFF00) | (c & 0xFF) << 16;
        }
    }
    return (const unsigned char *)buf;
}

/* ---- our conversation text (the plan's step A2) ------------------------------------------------------------------
 * The backend (port\win32\uwconv.c) hands each text area of the conversation screen: its rectangle on the original's
 * screen (x, y, w, h), where that point is in the layer (dx, dy), the scale, the colours the original's text is drawn
 * in there (erase: 0x00RRGGBB), and the text to draw, from the conversation's record (uwconvrec.c): the original's
 * escapes already turned into colour bytes 1..7 (\0..\6, whose colours are inks[0..6]) and '\n' between paragraphs.
 * The area's pixels in an erase colour take the colour of the pixel left of them (the paper, or a name plate's
 * leather), so the original's letters go and everything else (the pointer drawn into the screen, the rolled edges)
 * stays; then the text is wrapped to the area's width and drawn from its bottom up, `back` lines scrolled back
 * (top_down: from its top, no scrolling: a name).
 * conv_font (settings.txt, the Esc menu's "Conversation text"): 0 a smooth font (Tahoma, through GDI, at the window's
 * resolution), 1 the original's FONT5X6P scaled up. Returns the number of lines the text makes. */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
/* smoothtext_stb.c: a letter in Tahoma's stand-in, drawn as the cells below take it (Linux) */
int st_conv_glyph(int em, int c, unsigned char *cell, int cw, int ch, int pad, int *adv, int *ascent);
#endif

extern int uwv_conv_font_original;      /* uwview.c, settings.txt conv_font: 0 smooth (the default), 1 the original's */

typedef struct { unsigned char cov[64 * 64]; int adv; } SGlyph;   /* coverage 0-255 in a cell, the pen's advance */
static SGlyph *sg;              /* 256 characters at the size sg_em */
static int sg_em, sg_cw, sg_ch, sg_pad, sg_base;

static void smooth_build(int em)
{
    if (sg && sg_em == em) return;
    if (!sg) sg = calloc(256, sizeof *sg);
    if (!sg) return;
    sg_em = em, sg_pad = em / 4 + 1;
    sg_cw = em + 2 * sg_pad, sg_ch = em * 3 / 2 + 2;
    if (sg_cw > 64) sg_cw = 64;
    if (sg_ch > 64) sg_ch = 64;
#ifndef _WIN32
    memset(sg, 0, 256 * sizeof *sg);
    for (int c = 32; c < 256; c++) {
        static unsigned char cell[64 * 64];
        memset(cell, 0, sizeof cell);
        if (!st_conv_glyph(em, c, cell, sg_cw, sg_ch, sg_pad, &sg[c].adv, &sg_base)) break;
        memcpy(sg[c].cov, cell, (size_t)sg_cw * sg_ch);
    }
#else
    HDC dc = CreateCompatibleDC(NULL);
    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader, bi.bmiHeader.biWidth = sg_cw, bi.bmiHeader.biHeight = -sg_ch;
    bi.bmiHeader.biPlanes = 1, bi.bmiHeader.biBitCount = 32, bi.bmiHeader.biCompression = BI_RGB;
    void *bits = NULL;
    HBITMAP bm = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    HFONT f = CreateFontA(-em, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                          ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, "Tahoma");
    HGDIOBJ ob = SelectObject(dc, bm), of = SelectObject(dc, f);
    SetTextColor(dc, RGB(255, 255, 255)), SetBkColor(dc, RGB(0, 0, 0)), SetBkMode(dc, OPAQUE);
    TEXTMETRICA tm;
    GetTextMetricsA(dc, &tm);
    sg_base = tm.tmAscent;
    for (int c = 32; c < 256; c++) {
        char s = (char)c;
        SIZE sz;
        memset(bits, 0, (size_t)sg_cw * sg_ch * 4);
        TextOutA(dc, sg_pad, 0, &s, 1);
        GdiFlush();
        GetTextExtentPoint32A(dc, &s, 1, &sz);
        sg[c].adv = sz.cx;
        const unsigned *p = bits;
        for (int i = 0; i < sg_cw * sg_ch; i++) sg[c].cov[i] = (unsigned char)(p[i] >> 8 & 0xFF);
    }
    SelectObject(dc, of), SelectObject(dc, ob);
    DeleteObject(f), DeleteObject(bm), DeleteDC(dc);
#endif
}

/* the font in use: a character's advance, a line's height, and drawing one at (x, y) (its line's top) */
static int tf_original, tf_k, tf_line;
int uwv_ui_ready(void);         /* uwview.c: the game's fonts loaded */
static void tf_set(int scale, int line_rows)
{
    tf_original = uwv_conv_font_original && uwv_ui_ready(), tf_k = scale;
    tf_line = line_rows * scale;
    if (!tf_original) smooth_build(tf_line * 4 / 5);
}
static int tf_adv(int c)
{
    /* the original's pen moves by the glyph's own width (its string_width / string_to_screen: the width byte holds the
       gap after the letter), not w + 1 as our ui_text; so its lines wrap where the original's do */
    if (tf_original) { int w, h, row; ui_glyph(FONT_NORMAL, c, &w, &h, &row); return w * tf_k; }
    return sg ? sg[(unsigned char)c].adv : tf_k * 6;
}
static int clip_y0, clip_y1;
static void put(int x, int y, unsigned ink, int a)
{
    if (x < 0 || y < clip_y0 || x >= bw || y >= clip_y1 || y >= bh || a <= 0) return;
    unsigned *d = buf + (size_t)y * bw + x, o = *d;
    int r = ink >> 16 & 0xFF, g = ink >> 8 & 0xFF, b = ink & 0xFF;
    int or_ = o & 0xFF, og = o >> 8 & 0xFF, ob = o >> 16 & 0xFF;
    or_ += (r - or_) * a / 255, og += (g - og) * a / 255, ob += (b - ob) * a / 255;
    *d = 0xFF000000u | (unsigned)or_ | (unsigned)og << 8 | (unsigned)ob << 16;
}
static void tf_draw(int c, int x, int y, unsigned ink)
{
    if (tf_original) {
        int w, h, row;
        const u8 *gl = ui_glyph(FONT_NORMAL, c, &w, &h, &row);
        if (!gl) return;
        for (int py = 0; py < h; py++)
            for (int px = 0; px < w && px < row * 8; px++)
                if (gl[py * row + px / 8] & (0x80 >> (px % 8)))
                    for (int yy = 0; yy < tf_k; yy++)
                        for (int xx = 0; xx < tf_k; xx++) put(x + px * tf_k + xx, y + py * tf_k + yy, ink, 255);
        return;
    }
    if (!sg || c < 32) return;
    /* the glyph's baseline at four fifths of the line, as the original's letters sit (their descenders below it) */
    int top = y + tf_line * 4 / 5 - sg_base;
    const unsigned char *cv = sg[(unsigned char)c].cov;
    for (int py = 0; py < sg_ch; py++)
        for (int px = 0; px < sg_cw; px++)
            if (cv[py * sg_cw + px]) put(x - sg_pad + px, top + py, ink, cv[py * sg_cw + px]);
}

enum { TL_MAX = 4096 };
static struct { int a, b, col; } tl[TL_MAX];     /* the wrapped lines: text [a, b), the colour at a */

static int wrap(const char *s, int width)
{
    int n = 0, col = 1, i = 0;
    while (s[i] && n < TL_MAX) {
        int a = i, acol = col, x = 0, brk = -1, brkcol = col, c2 = col;
        for (;; i++) {
            unsigned char c = (unsigned char)s[i];
            if (!c || c == '\n') break;
            if (c >= 1 && c <= 7) { c2 = c; continue; }
            if (c == ' ') brk = i, brkcol = c2;
            int w = tf_adv(c);
            if (x + w > width && x > 0) {
                if (brk > a) i = brk, c2 = brkcol;
                break;
            }
            x += w;
        }
        tl[n].a = a, tl[n].b = i, tl[n].col = acol, n++;
        col = c2;
        if (s[i] == '\n') i++;
        else while (s[i] == ' ') i++;          /* a line broken at spaces: they go */
    }
    return n;
}

static void block(const unsigned *src, int sw, int x, int y, int rx, int ry, int dx, int dy, int scale, unsigned c)
{
    unsigned px = 0xFF000000u | (c >> 16 & 0xFF) | (c & 0xFF00) | (c & 0xFF) << 16;
    (void)src, (void)sw;
    for (int yy = 0; yy < scale; yy++) {
        int ty = dy + (ry - y) * scale + yy;
        if (ty < 0 || ty >= bh) continue;
        for (int xx = 0; xx < scale; xx++) {
            int tx = dx + (rx - x) * scale + xx;
            if (tx >= 0 && tx < bw) buf[(size_t)ty * bw + tx] = px;
        }
    }
}

/* paper: the area's plain colour; a pixel there neither paper nor an erase colour (the pointer the game drew into its
 * screen) is drawn again over our text (0xFFFFFFFF: none, a name plate's leather has many colours). The lines fill the
 * area from its top as the original's scroll does, and once they are more than it holds, the last ones (back lines
 * earlier) end at its bottom. rows (rows_n of them): for each line row of the area, the offset in text where the line
 * drawn there starts, -1 for none (the backend finds the answer under a click with it). */
UWV_API int uwv_conv_text(const unsigned *src, int sw, int sh, int x, int y, int w, int h, int dx, int dy, int scale,
                          const unsigned *erase, int nerase, const char *text, const unsigned *inks, int text_x,
                          int line_rows, int back, unsigned paper, int *rows, int rows_n)
{
    for (int r = 0; rows && r < rows_n; r++) rows[r] = -1;
    if (!buf || !src || scale < 1) return 0;
    /* the original's letters out */
    for (int ry = y; ry < y + h && ry < sh; ry++) {
        unsigned left = src[(size_t)ry * sw + (x > 0 ? x - 1 : 0)] & 0xFFFFFF;
        for (int rx = x; rx < x + w && rx < sw; rx++) {
            unsigned c = src[(size_t)ry * sw + rx] & 0xFFFFFF;
            int er = 0;
            for (int k = 0; k < nerase; k++) if (c == (erase[k] & 0xFFFFFF)) { er = 1; break; }
            if (!er) { left = c; continue; }
            if (left == 0x000004) continue;       /* (the screen's black: left as the layer has it) */
            unsigned px = 0xFF000000u | (left >> 16 & 0xFF) | (left & 0xFF00) | (left & 0xFF) << 16;
            for (int yy = 0; yy < scale; yy++) {
                int ty = dy + (ry - y) * scale + yy;
                if (ty < 0 || ty >= bh) continue;
                for (int xx = 0; xx < scale; xx++) {
                    int tx = dx + (rx - x) * scale + xx;
                    if (tx >= 0 && tx < bw) buf[(size_t)ty * bw + tx] = px;
                }
            }
        }
    }
    int n = 0;
    if (text && *text) {
        tf_set(scale, line_rows);
        int x0 = dx + (text_x - x) * scale, width = (x + w - text_x) * scale;
        n = wrap(text, width);
        while (n > 0 && tl[n - 1].a == tl[n - 1].b) n--;        /* no empty line at the end */
        int vis = h * scale / tf_line;
        if (vis < 1) vis = 1;
        if (back > n - vis) back = n - vis;
        if (back < 0) back = 0;
        int first = n > vis ? n - vis - back : 0, ty = dy;
        if (n > vis) ty = dy + h * scale - vis * tf_line;      /* full: the last line on the bottom */
        clip_y0 = dy < 0 ? 0 : dy, clip_y1 = dy + h * scale;
        for (int l = first, r = 0; l < n && l < first + vis; l++, r++, ty += tf_line) {
            int col = tl[l].col, px = x0;
            if (rows && r < rows_n) rows[r] = tl[l].a;
            for (int i = tl[l].a; i < tl[l].b; i++) {
                unsigned char c = (unsigned char)text[i];
                if (c >= 1 && c <= 7) { col = c; continue; }
                tf_draw(c, px, ty, inks[col - 1]);
                px += tf_adv(c);
            }
        }
        clip_y0 = 0, clip_y1 = bh;
    }
    /* the pointer over our text */
    if (paper != 0xFFFFFFFFu)
        for (int ry = y; ry < y + h && ry < sh; ry++)
            for (int rx = x; rx < x + w && rx < sw; rx++) {
                unsigned c = src[(size_t)ry * sw + rx] & 0xFFFFFF;
                if (c == (paper & 0xFFFFFF) || c == 0x000004) continue;
                int er = 0;
                for (int k = 0; k < nerase; k++) if (c == (erase[k] & 0xFFFFFF)) { er = 1; break; }
                if (!er) block(src, sw, x, y, rx, ry, dx, dy, scale, c);
            }
    return n;
}

/* ---- our map (the plan's Part B: the original's look, sharper; the user 2026-11-13) --------------------------------
 * The original (AUTOMAP.C) still draws its map into its screen (its rand() calls are the game's; nothing here calls
 * rand()). Over the map's part of the screen we draw it again at the window's resolution from the same data, by the
 * same rules, in palette indices as the original works: the parchment (DATA\BLNKMAP.BYT), each seen tile of PlayersMap
 * as DoTile (its pattern, the terrain's shade, the kind: dark, the 0xE9.. colours, a door as DoDoorTile), the walls as
 * ShadeSide and the corners as ShowDungeonMap's PixelDarken (its corner quirk too: east and south darken the top right).
 * Sharper: a diagonal is a line across the tile, not three steps; the "hand-drawn" darkening varies by our pixel
 * (a hash, fixed for a place) where the original's varied by its own. Then the notes (FONT4X5P, ink 0x2D), the note
 * being typed, the player's marker (BUTTONS.GR 0x3F) and the game's pointer (its CURSORS.GR picture), all the
 * original's own pictures scaled up whole. The rest of the screen (the buttons, the level's number) stays the
 * original's pixels (uwv_conv). Coordinates as the original's: y up, row = 199 - y. */
int uwv_game_file(const char *rel, unsigned char *out, int cap);   /* uwview.c */
static unsigned char *map_bg;           /* BLNKMAP.BYT: 320 x 200 palette indices, the top row first */
static int map_bg_tried;
static unsigned short *mapidx;          /* our map's palette indices (16 bits: the darkening adds up past 255) */
static int map_w, map_h;
enum { MR_X0 = 8, MR_X1 = 200, MR_Y0 = 5, MR_Y1 = 197 };   /* the region we draw (x, and y up), the map's and its edges */

static unsigned mhash(unsigned x, unsigned y, unsigned salt)
{
    unsigned h = x * 0x9E3779B1u ^ (y + 0x7F4A7C15u) * 0x85EBCA77u ^ salt * 0xC2B2AE3Du;
    h ^= h >> 15, h *= 0x2C1B3C6Du, h ^= h >> 12, h *= 0x297A2D39u, h ^= h >> 15;
    return h;
}

static int mk;                          /* the scale */
static int mgrain;                      /* our pixels per grain of the darkening */
/* one of our pixels (X, Y: from the region's left and its top row) */
#define MIDX(X, Y) mapidx[(size_t)(Y) * map_w + (X)]
/* PixelDarken over the original's pixel (x, y up), on our pixels inside it where keep(): base + a random 0..n-1 */
static void darken_px(int x, int y, int base, int n, unsigned salt)
{
    int X0 = (x - MR_X0) * mk, Y0 = (MR_Y1 - y) * mk;
    for (int v = 0; v < mk; v++)
        for (int u = 0; u < mk; u++) {
            int X = X0 + u, Y = Y0 + v;
            if (X < 0 || Y < 0 || X >= map_w || Y >= map_h) continue;
            MIDX(X, Y) += (unsigned short)(base + (int)(mhash((unsigned)(X / mgrain), (unsigned)(Y / mgrain), salt) % (unsigned)n));
        }
}

/* DoTile and the sides of one tile; t its type (1..9), b its PlayersMap byte, nb(dx, dy) a neighbour's type */
static void map_tile(const unsigned char *pm, int x, int y)
{
    int t = pm[y * 64 + x] & 0xF, b = pm[y * 64 + x] >> 4, shade = b & 3, kind = b & 0xC;
    int px = (x - 1) * 3 + 10, py = (y - 1) * 3 + 7;     /* the tile's bottom left, y up */
    int tt = t >= 6 ? 1 : t;
    unsigned salt = (unsigned)(y * 64 + x) * 16u;
    /* the pattern, at our resolution: a = across (x), c = up (y), 0..3 in the original's pixels */
    int X0 = (px - MR_X0) * mk, Ytop = (MR_Y1 - (py + 2)) * mk;
    for (int v = 0; v < 3 * mk; v++)
        for (int u = 0; u < 3 * mk; u++) {
            float a = (u + 0.5f) / mk, c = 3.0f - (v + 0.5f) / mk;
            int p = 1;
            float d = a - c, s = a + c - 3.0f;
            const float band = 0.5f;
            switch (tt) {
            case 2: p = d > -band && d < band ? 2 : d > 0 ? 1 : 0; break;
            case 3: p = s > -band && s < band ? 2 : s < 0 ? 1 : 0; break;
            case 4: p = s > -band && s < band ? 2 : s > 0 ? 1 : 0; break;
            case 5: p = d > -band && d < band ? 2 : d < 0 ? 1 : 0; break;
            }
            int X = X0 + u, Y = Ytop + v;
            if (X < 0 || Y < 0 || X >= map_w || Y >= map_h) continue;
            unsigned r = mhash((unsigned)(X / mgrain), (unsigned)(Y / mgrain), salt + 1);
            if (p == 1) {
                if (shade == 0) MIDX(X, Y) += (unsigned short)(2 + r % 3);
                else if (shade == 1) MIDX(X, Y) = (unsigned short)(0xB1 + r % 2);
                else if (shade == 2) MIDX(X, Y) = (unsigned short)(0xB5 + r % 2);
            } else if (p == 2)
                MIDX(X, Y) += (unsigned short)(6 + r % 2);
            unsigned r2 = mhash((unsigned)(X / mgrain), (unsigned)(Y / mgrain), salt + 2);
            if (kind == 0xC) MIDX(X, Y) += (unsigned short)(6 + r2 % 3);
            else if (kind == 8) MIDX(X, Y) = (unsigned short)(0xE9 + r2 % 3);
        }
    if (kind == 4) {   /* DoDoorTile: the middle, and the two across the passage (door_dx / door_dy) */
        static const signed char ddy[4] = { -1, 0, -1, 1 }, ddx[4] = { 0, -1, -1, -1 };
        darken_px(px + 1, py + 1, 6, 3, salt + 3);
        for (int dd = 0; dd < 4; dd++) {
            int ya = y + ddy[dd], xa = x + ddx[dd], yb = y - ddy[dd], xb = x - ddx[dd];
            int ta = ya >= 0 && ya < 64 && xa >= 0 && xa < 64 ? pm[ya * 64 + xa] & 0xF : 0;
            int tb = yb >= 0 && yb < 64 && xb >= 0 && xb < 64 ? pm[yb * 64 + xb] & 0xF : 0;
            if (ta == 1 || tb == 1) {
                darken_px(px + 1 + ddy[dd], py + 1 + ddx[dd], 6, 3, salt + 4);
                darken_px(px + 1 - ddy[dd], py + 1 - ddx[dd], 6, 3, salt + 5);
                break;
            }
        }
    }
}

/* ShadeSide: side 0 north (y + 1), 1 east, 2 south, 3 west; 1 when it drew */
static int map_side(const unsigned char *pm, int side, int x, int y)
{
    int px = (x - 1) * 3 + 10, py = (y - 1) * 3 + 7;
    int nx = x + (side == 1) - (side == 3), ny = y + (side == 0) - (side == 2);
    int t = nx >= 0 && nx < 64 && ny >= 0 && ny < 64 ? pm[ny * 64 + nx] & 0xF : 0;
    if (t > 0 && t < 10) return 0;
    int a = t == 11 ? 0 : 6, n = t == 11 ? 4 : 2;
    unsigned salt = (unsigned)(y * 64 + x) * 16u + 8u + (unsigned)side;
    for (int i = 0; i < 3; i++) {
        if (side == 0) darken_px(px + i, py + 3, a, n, salt);
        else if (side == 2) darken_px(px + i, py - 1, a, n, salt);
        else if (side == 1) darken_px(px + 3, py + i, a, n, salt);
        else darken_px(px - 1, py + i, a, n, salt);
    }
    return 1;
}

/* a picture's shape (its PNG's alpha; y up top) with the colours the original's screen has there: the original drew it
   in the palette now (the map's), which the PNG (made with the game's) does not have */
static void put_pic(const Pic *p, const unsigned *src, int x, int ytop, int dx, int dy, int k)
{
    if (!p) return;
    for (int r = 0; r < p->h; r++)
        for (int c = 0; c < p->w; c++) {
            const unsigned char *s = p->rgba + ((size_t)r * p->w + c) * 4;
            int sx = x + c, sy = 199 - ytop + r;
            if (s[3] < 128 || sx < 0 || sx >= 320 || sy < 0 || sy >= 200) continue;
            unsigned c2 = src[sy * 320 + sx] & 0xFFFFFF;
            unsigned px = 0xFF000000u | (c2 >> 16 & 0xFF) | (c2 & 0xFF00) | (c2 & 0xFF) << 16;
            for (int yy = 0; yy < k; yy++) {
                int ty = dy + (199 - ytop + r) * k + yy;
                if (ty < 0 || ty >= bh) continue;
                for (int xx = 0; xx < k; xx++) {
                    int tx = dx + (x + c) * k + xx;
                    if (tx >= 0 && tx < bw) buf[(size_t)ty * bw + tx] = px;
                }
            }
        }
}

static void map_text(const char *s, int x, int ytop, unsigned ink, int dx, int dy, int k)   /* FONT4X5P, as string_to_screen */
{
    unsigned px = 0xFF000000u | (ink >> 16 & 0xFF) | (ink & 0xFF00) | (ink & 0xFF) << 16;
    for (; *s; s++) {
        int w, h, row;
        const u8 *gl = ui_glyph(FONT_SMALL, (unsigned char)*s, &w, &h, &row);
        if (gl)
            for (int r = 0; r < h; r++)
                for (int c = 0; c < w && c < row * 8; c++)
                    if (gl[r * row + c / 8] & (0x80 >> (c % 8)))
                        for (int yy = 0; yy < k; yy++) {
                            int ty = dy + (199 - ytop + r) * k + yy;
                            if (ty < 0 || ty >= bh) continue;
                            for (int xx = 0; xx < k; xx++) {
                                int tx = dx + (x + c) * k + xx;
                                if (tx >= 0 && tx < bw) buf[(size_t)ty * bw + tx] = px;
                            }
                        }
        x += w;
    }
}

/* pm: PlayersMap (64 x 64, [y][x]); pal: the palette now (256, 0x00RRGGBB); notes: n of them, each 50 bytes of text and
 * nx/ny (x, y up; x < 0 erased); typing: the note being typed (NULL none) at tx, ty; marker mx, my (mx < 0 none);
 * cursor: on (its rectangle cx, cytop, its picture cpic) or not. dx, dy, k: where the screen's (0, 0) is in the layer
 * and its scale. 1: drawn. */
UWV_API int uwv_map(const unsigned *src, int sw, int sh, int dx, int dy, int k, const unsigned char *pm,
                    const unsigned *pal, const char *notes, const short *nx, const short *ny, int n, const char *typing,
                    int tx, int ty, int mx, int my, int cursor_on, int cx, int cytop, int cw, int ch, int cpic)
{
    if (!buf || !src || !pm || !pal || k < 1 || sw != 320 || sh != 200) return 0;
    if (!map_bg_tried) {
        map_bg_tried = 1;
        map_bg = malloc(64000);
        if (map_bg && uwv_game_file("DATA\\BLNKMAP.BYT", map_bg, 64000) != 64000) free(map_bg), map_bg = NULL;
    }
    if (!map_bg || !uwv_ui_ready()) return 0;
    mk = k, mgrain = k >= 4 ? k / 2 : 1;
    int w = (MR_X1 - MR_X0 + 1) * k, h = (MR_Y1 - MR_Y0 + 1) * k;
    if (w != map_w || h != map_h || !mapidx) {
        free(mapidx);
        mapidx = malloc((size_t)w * h * sizeof *mapidx);
        map_w = w, map_h = h;
        if (!mapidx) return 0;
    }
    for (int Y = 0; Y < h; Y++) {   /* the parchment */
        int row = 199 - MR_Y1 + Y / k;
        for (int X = 0; X < w; X++) MIDX(X, Y) = map_bg[row * 320 + MR_X0 + X / k];
    }
    /* ShowDungeonMap's order: each tile, its sides, then its corners */
    for (int y = 1; y < 0x3F; y++)
        for (int x = 1; x < 0x3F; x++) {
            int t = pm[y * 64 + x] & 0xF;
            if (t <= 0 || t >= 10) continue;
            map_tile(pm, x, y);
            int side[4] = {0, 0, 0, 0};
            if (t >= 2 && t <= 5) {   /* tile_walls' TW_DIAG: the two sides diag_side names */
                static const signed char diag_side[4] = { 1, 2, 0, 3 };
                int d = diag_side[t - 2];
                side[d] = map_side(pm, d, x, y);
                d = (d + 1) & 3;
                side[d] = map_side(pm, d, x, y);
            } else
                for (int i = 0; i < 4; i++) side[i] = map_side(pm, i, x, y);
            int px = x * 3 + 10, py = y * 3 + 7;
            for (int i = 0; i < 4; i++)
                if (side[i] && side[(i + 1) & 3])
                    darken_px(px - ((i & 2) << 1), py - ((i & 2) << 1), 3, 2, (unsigned)(y * 64 + x) * 16u + 13u + (unsigned)i);
        }
    for (int Y = 0; Y < h; Y++) {   /* to the layer */
        int ty = dy + (199 - MR_Y1) * k + Y;
        if (ty < 0 || ty >= bh) continue;
        for (int X = 0; X < w; X++) {
            int tx = dx + MR_X0 * k + X;
            if (tx < 0 || tx >= bw) continue;
            unsigned c = pal[MIDX(X, Y) & 0xFF];
            buf[(size_t)ty * bw + tx] = 0xFF000000u | (c >> 16 & 0xFF) | (c & 0xFF00) | (c & 0xFF) << 16;
        }
    }
    if (mx >= 0) put_pic(ui_pic("ui\\buttons", 0x3F), src, mx, my, dx, dy, k);
    unsigned ink = pal[0x2D];
    for (int i = 0; i < n; i++) {
        if (nx[i] < 0) continue;
        char t[51];
        memcpy(t, notes + i * 50, 50), t[50] = 0;
        map_text(t, nx[i], ny[i], ink, dx, dy, k);
    }
    if (typing) map_text(typing, tx, ty, ink, dx, dy, k);
    if (cursor_on) {
        if (cpic >= 0x106C && cpic < 0x106C + 19) put_pic(ui_pic("ui\\cursors", cpic - 0x106C), src, cx, cytop, dx, dy, k);
        else   /* (another picture on the pointer: the original's pixels there) */
            for (int r = 0; r < ch; r++)
                for (int c = 0; c < cw; c++) {
                    int sx = cx + c, sy = 199 - cytop + r;
                    if (sx < 0 || sx >= 320 || sy < 0 || sy >= 200) continue;
                    unsigned c2 = src[sy * 320 + sx] & 0xFFFFFF;
                    for (int yy = 0; yy < k; yy++)
                        for (int xx = 0; xx < k; xx++) {
                            int ox = dx + sx * k + xx, oy = dy + sy * k + yy;
                            if (ox >= 0 && ox < bw && oy >= 0 && oy < bh)
                                buf[(size_t)oy * bw + ox] = 0xFF000000u | (c2 >> 16 & 0xFF) | (c2 & 0xFF00) | (c2 & 0xFF) << 16;
                        }
                }
    }
    return 1;
}
