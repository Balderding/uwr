#include "ui.h"
#include "smoothtext.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Overlay O;
static int g_scale0, g_ww, g_wh;   /* the window's own scale and size (ui_start) */

/* ---- the interface's font (the user, 2026-11-14: "change the font to something that is easy to read and apply it to
   everything in the game. choose the font size based on the resolution of the game. also put options in the esc menu
   to change the font and font size"). g_font 0: the game's own letters on the overlay as before; 1..: a smooth face
   (smoothtext.c's st_face_label(g_font - 1)), its size st_game_em of the window's height (FONT_SMALL 85 %, FONT_BIG
   130 %), drawn at the window's own pixels into the text layer T, which is laid over the overlay wherever the overlay
   is laid on the screen (ui_text_apply). Anything opaque drawn on the overlay afterwards clears the text under it
   (blend), so what is drawn later still covers it (a tooltip over the sheet). Widths and heights are the face's,
   in overlay pixels, so the layouts measure and wrap by it. Presentation only. ---- */
static int g_font = 0;   /* (the port's renderer sets it from settings.txt ui_font, 1 by default: uwview.c) */
static unsigned *T;                                      /* window pixels, st_draw's 0xAABBGGRR (premultiplied) */
static int tx0 = 1 << 30, ty0 = 1 << 30, tx1 = -1, ty1 = -1;   /* what of T is drawn on */
void ui_set_font(int f) { g_font = f < 0 ? 0 : f; if (g_font) st_set_face(g_font - 1); }
int ui_font(void) { return g_font; }
static void t_clear(void) {
    if (T && tx1 >= tx0 && ty1 >= ty0)
        for (int y = ty0; y <= ty1; y++) memset(T + (size_t)y * g_ww + tx0, 0, (size_t)(tx1 - tx0 + 1) * 4);
    tx0 = ty0 = 1 << 30, tx1 = ty1 = -1;
}
static int sm_em(int f) {
    int e = st_game_em(g_wh);
    return f == FONT_SMALL ? e * 85 / 100 : f == FONT_BIG ? e * 13 / 10 : e;
}
void ui_text_apply(unsigned *d, int w, int h) { ui_text_apply_a(d, w, h, 256); }
void ui_text_apply_a(unsigned *d, int w, int h, unsigned A) {   /* A: its opacity, 0..256 (a fading box) */
    if (!T || tx1 < tx0) return;
    if (A > 256) A = 256;
    for (int y = ty0; y <= ty1 && y < h && y < g_wh; y++)
        for (int x = tx0; x <= tx1 && x < w && x < g_ww; x++) {
            unsigned t = T[(size_t)y * g_ww + x], a = t >> 24;
            if (!a) continue;
            if (A < 256) {   /* premultiplied: every channel by the fade */
                a = a * A >> 8;
                if (!a) continue;
                t = a << 24 | ((t & 255) * A >> 8) | ((t >> 8 & 255) * A >> 8) << 8 | ((t >> 16 & 255) * A >> 8) << 16;
            }
            unsigned c = d[(size_t)y * w + x], da = c >> 24, k = da * (255 - a) / 255, oa = a + k;
            unsigned r = ((t & 255) * 255 + (c & 255) * k) / oa, g = ((t >> 8 & 255) * 255 + (c >> 8 & 255) * k) / oa,
                     b = ((t >> 16 & 255) * 255 + (c >> 16 & 255) * k) / oa;
            d[(size_t)y * w + x] = oa << 24 | (r > 255 ? 255 : r) | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b) << 16;
        }
}
/* draw at another scale for a while (the Esc menu, smaller); 0: the window's own again. Clears the overlay. */
void ui_set_scale(int s) {
    if (!g_ww) return;
    O.scale = s > 0 ? s : g_scale0;
    O.w = g_ww / O.scale, O.h = g_wh / O.scale;
    memset(O.px, 0, (size_t)O.w * O.h * 4);
    t_clear();
}
static char g_data[600];

/* the fonts (.SYS): u16 1, u16 bytes a character, u16 space width, u16 height, u16 bytes a row, u16 widest;
 * then each character: its rows (most significant bit leftmost), then a byte: its width. Character 0 first. */
typedef struct {
    Bytes d;
    int size, space, height, row, count;
} Font;
static Font F[FONTS];
static char g_game[600];   /* the game's folder (ui_game_dir), where the fonts are copied from when missing */
void ui_game_dir(const char *game_dir) { str_copy(g_game, sizeof g_game, game_dir ? game_dir : ""); }

bool ui_start(const char *data_dir, int ww, int wh) {
    str_copy(g_data, sizeof g_data, data_dir);
    O.scale = g_scale0 = MAX(1, (wh + 150) / 300);
    g_ww = ww, g_wh = wh;
    O.w = ww / O.scale, O.h = wh / O.scale;
    O.px = arena_alloc((size_t)ww * wh * 4);   /* room for any scale (ui_set_scale), down to 1 */
    T = calloc((size_t)ww * wh, 4);            /* the smooth text layer */
    static const char *names[FONTS] = {"font4x5p", "font5x6p", "fontbig"};   /* (lower case: the same files on Windows) */
    char path[700];
    for (int f = 0; f < FONTS; f++) {
        /* the fonts are copied loose from game\DATA when missing (they are not archives: one font a file) */
        snprintf(path, sizeof path, "%s\\ui\\fonts\\%s.sys", data_dir, names[f]);
        if (!file_exists(path)) {
            char src[700];
            snprintf(src, sizeof src, "%s\\..\\game\\data\\%s.sys", data_dir, names[f]);
            if (!file_exists(src) && g_game[0]) snprintf(src, sizeof src, "%s\\data\\%s.sys", g_game, names[f]);
            Bytes b = file_read(src);
            if (b.size) make_dirs_for(path), file_write(path, b.data, b.size);
        }
        Bytes b = file_read(path);
        if (b.size < 12) continue;
        Font *t = &F[f];
        t->d = b, t->size = (int)rd16(b.data + 2), t->space = (int)rd16(b.data + 4), t->height = (int)rd16(b.data + 6), t->row = (int)rd16(b.data + 8);
        t->count = (int)((b.size - 12) / (size_t)(t->size + 1));
    }
    return true;
}

Overlay *ui_overlay(void) { return &O; }
void ui_clear(void) { memset(O.px, 0, (size_t)O.w * O.h * 4); t_clear(); }

/* pictures, kept once loaded */
typedef struct {
    char dir[48];
    int k;
    Pic pic;
    bool tried;
} PicSlot;
static PicSlot g_pics[512];
static int g_npics;

/* a picture by its file name under the data folder ("ui\\screens\\MAIN.png"), kept once loaded */
const Pic *ui_pic_file(const char *rel) {
    static struct { char rel[64]; Pic pic; bool tried; } f[8];
    int i;
    if (!g_data[0]) return NULL;   /* before ui_start: not tried (a failure then would be kept for good) */
    for (i = 0; i < 8 && f[i].tried; i++)
        if (!strcmp(f[i].rel, rel)) return f[i].pic.rgba ? &f[i].pic : NULL;
    if (i == 8) return NULL;
    str_copy(f[i].rel, sizeof f[i].rel, rel), f[i].tried = true;
    char path[700];
    snprintf(path, sizeof path, "%s\\%s", g_data, rel);
    Image img;
    size_t mark = arena_mark();
    if (png_read(path, &img)) {
        f[i].pic.rgba = malloc((size_t)img.w * img.h * 4);
        if (f[i].pic.rgba) memcpy(f[i].pic.rgba, img.rgba, (size_t)img.w * img.h * 4), f[i].pic.w = img.w, f[i].pic.h = img.h;
    }
    arena_reset(mark);
    return f[i].pic.rgba ? &f[i].pic : NULL;
}

const Pic *ui_pic(const char *dir, int k) {
    if (!g_data[0]) return NULL;   /* before ui_start: not tried */
    for (int i = 0; i < g_npics; i++)
        if (g_pics[i].k == k && !strcmp(g_pics[i].dir, dir)) return g_pics[i].pic.rgba ? &g_pics[i].pic : NULL;
    if (g_npics >= COUNT(g_pics)) return NULL;
    PicSlot *s = &g_pics[g_npics++];
    str_copy(s->dir, sizeof s->dir, dir), s->k = k, s->tried = true;
    char path[700];
    snprintf(path, sizeof path, "%s\\%s\\%03d.png", g_data, dir, k);
    Image img;
    size_t mark = arena_mark();
    if (png_read(path, &img)) { /* kept outside the arena: levels come and go, the interface stays */
        s->pic.rgba = malloc((size_t)img.w * img.h * 4);
        if (s->pic.rgba) memcpy(s->pic.rgba, img.rgba, (size_t)img.w * img.h * 4), s->pic.w = img.w, s->pic.h = img.h;
    }
    arena_reset(mark);
    return s->pic.rgba ? &s->pic : NULL;
}

static void blend(int x, int y, const u8 *c, int a) {
    if (x < 0 || y < 0 || x >= O.w || y >= O.h || a <= 0) return;
    if (a >= 200 && tx1 >= tx0) {   /* something opaque over the smooth text drawn before: it covers it */
        int sc = O.scale, X = x * sc, Y = y * sc;
        if (X <= tx1 && X + sc - 1 >= tx0 && Y <= ty1 && Y + sc - 1 >= ty0)
            for (int yy = Y; yy < Y + sc && yy < g_wh; yy++) memset(T + (size_t)yy * g_ww + X, 0, (size_t)(X + sc <= g_ww ? sc : g_ww - X) * 4);
    }
    u8 *d = O.px + ((size_t)y * O.w + x) * 4;
    if (a >= 255) {
        d[0] = c[0], d[1] = c[1], d[2] = c[2], d[3] = 255;
        return;
    }
    int da = d[3], oa = a + da * (255 - a) / 255;
    for (int k = 0; k < 3; k++) d[k] = (u8)(oa ? (c[k] * a + d[k] * da * (255 - a) / 255) / oa : 0);
    d[3] = (u8)oa;
}

void ui_draw(const Pic *p, int x, int y, int s) {
    if (!p) return;
    s = MAX(1, s);
    for (int py = 0; py < p->h * s; py++)
        for (int px = 0; px < p->w * s; px++) {
            const u8 *c = p->rgba + ((size_t)(py / s) * p->w + px / s) * 4;
            blend(x + px, y + py, c, c[3]);
        }
}

u8 ui_new_sprite[512];
const Pic *ui_item_pic(int item, int *div) {
    if (div) *div = 1;
    if (item >= 0 && item < 512 && ui_new_sprite[item]) {
        const Pic *p = ui_pic("newsprites\\objects", item);
        if (p && p->w <= 32 && p->h <= 32) {
            if (div) *div = p->w > 16 || p->h > 16 ? 2 : 1;
            return p;
        }
    }
    return ui_pic("sprites\\objects", item);
}

void ui_draw_fine(const Pic *p, int x, int y, int div) {
    if (!p) return;
    if (div <= 1) { ui_draw(p, x, y, 1); return; }
    int sc = O.scale, w = p->w / div, h = p->h / div;   /* its size on the overlay */
    if (!T || sc < 2) {   /* no finer pixels to draw on: every div-th pixel on the overlay */
        for (int py = 0; py < h; py++)
            for (int px = 0; px < w; px++) {
                const u8 *c = p->rgba + ((size_t)(py * div) * p->w + (size_t)(px * div)) * 4;
                blend(x + px, y + py, c, c[3]);
            }
        return;
    }
    int X0 = x * sc, Y0 = y * sc, W = w * sc, H = h * sc;
    for (int wy = 0; wy < H; wy++) {
        int Y = Y0 + wy;
        if (Y < 0 || Y >= g_wh) continue;
        for (int wx = 0; wx < W; wx++) {
            int X = X0 + wx;
            if (X < 0 || X >= g_ww) continue;
            const u8 *c = p->rgba + ((size_t)(wy * p->h / H) * p->w + (size_t)(wx * p->w / W)) * 4;
            if (c[3] < 128) continue;
            T[(size_t)Y * g_ww + X] = 0xFF000000u | c[0] | (unsigned)c[1] << 8 | (unsigned)c[2] << 16;
            if (X < tx0) tx0 = X;
            if (Y < ty0) ty0 = Y;
            if (X > tx1) tx1 = X;
            if (Y > ty1) ty1 = Y;
        }
    }
}

void ui_fill(int x, int y, int w, int h, u8 r, u8 g, u8 b, u8 a) {
    u8 c[3] = {r, g, b};
    for (int py = y; py < y + h; py++)
        for (int px = x; px < x + w; px++) blend(px, py, c, a);
}

int ui_font_height(int f) {
    if (g_font && g_wh) return (st_height(ST_REGULAR, sm_em(f)) + O.scale - 1) / O.scale;
    return F[f].height ? F[f].height : 6;
}

/* a character of the game's font f: its rows (row bytes each, most significant bit leftmost, height rows), NULL for a
 * space or a missing one; *w its width (the pen moves w + 1), *h the font's height, *row its bytes a row */
const u8 *ui_glyph(int f, int ch, int *w, int *h, int *row) {
    const Font *t = &F[f];
    int i = (u8)ch;
    *h = t->height ? t->height : 6, *row = t->row;
    if (ch == ' ' || !t->d.size || i >= t->count) { *w = t->space; return NULL; }
    *w = t->d.data[12 + (size_t)i * (t->size + 1) + t->size];
    return t->d.data + 12 + (size_t)i * (t->size + 1);
}

static int glyph_width(const Font *t, int ch) {
    if (ch == ' ') return t->space;
    int i = ch;
    if (!t->d.size || i < 0 || i >= t->count) return t->space;
    return t->d.data[12 + (size_t)i * (t->size + 1) + t->size];
}

/* text wrapped at word breaks to width w (and at new lines); returns the height used; draw false: measure only */
static int g_clip0 = -(1 << 30), g_clip1 = 1 << 30;
void ui_clip(int y0, int y1) {
    if (y0 < 0 && y1 < 0) g_clip0 = -(1 << 30), g_clip1 = 1 << 30;
    else g_clip0 = y0, g_clip1 = y1;
}

/* a wrapped line spread to the width w (overlay pixels at x): its words placed at the window's pixels with the spare room
   shared between the gaps (the smooth face), or at overlay pixels (the game's letters); a line with one word, or one
   that would need gaps over three spaces wide, as it is */
static int t_draw(int f, const char *s, int X, int y, u8 r, u8 g, u8 b);
static void just_line(int f, const char *line, int x, int y, int w, u8 r, u8 g, u8 b) {
    char words[64][96];
    int n = 0, sc = O.scale, smooth = g_font && T && g_wh;
    for (const char *p = line; *p && n < 64;) {
        while (*p == ' ') p++;
        if (!*p) break;
        int k = 0;
        while (*p && *p != ' ' && k < 95) words[n][k++] = *p++;
        while (*p && *p != ' ') p++;
        words[n][k] = 0, n++;
    }
    int total = 0, space = smooth ? st_width(ST_REGULAR, sm_em(f), " ") : (glyph_width(&F[f], ' ') + 1) * sc;
    for (int i = 0; i < n; i++) total += smooth ? st_width(ST_REGULAR, sm_em(f), words[i]) : ui_text_width(f, words[i]) * sc;
    float gap = n > 1 ? (float)(w * sc - total) / (n - 1) : 0;
    if (n < 2 || gap > 3.0f * space || gap < space * 0.5f) { ui_text(f, line, x, y, r, g, b); return; }
    float X = (float)(x * sc);
    for (int i = 0; i < n; i++) {
        int wpx = smooth ? st_width(ST_REGULAR, sm_em(f), words[i]) : ui_text_width(f, words[i]) * sc;
        if (smooth) t_draw(f, words[i], (int)(X + 0.5f), y, r, g, b);
        else ui_text(f, words[i], (int)(X + 0.5f) / sc, y, r, g, b);
        X += wpx + gap;
    }
}
static bool g_justify;   /* ui_text_wrap spreads its wrapped lines (the tooltips: the user, 2026-11-14) */
int ui_text_wrap(int f, const char *s, int x, int y, int w, u8 r, u8 g, u8 b, bool draw) {
    int lh = ui_font_height(f) + 1, cy = y;   /* (one overlay pixel between lines: less wasted room) */
    char line[512];
    while (*s) {
        int n = 0, last_space = -1;
        line[0] = 0;
        while (s[n] && s[n] != '\n' && n < 510) {
            line[n] = s[n];
            line[n + 1] = 0;
            if (s[n] == ' ') last_space = n;
            if (ui_text_width(f, line) > w && last_space > 0) {
                n = last_space;
                break;
            }
            n++;
        }
        line[n] = 0;
        bool wrapped = s[n] == ' ';   /* (broken at a space within its paragraph: not the paragraph's last line) */
        if (draw && cy >= g_clip0 && cy + lh <= g_clip1) {
            if (g_justify && wrapped) just_line(f, line, x, cy, w, r, g, b);
            else ui_text(f, line, x, cy, r, g, b);
        }
        cy += lh;
        s += n;
        while (*s == ' ' || *s == '\n') s++;
    }
    return cy - y;
}

int ui_text_width(int f, const char *s) {
    if (g_font && g_wh) return (st_width(ST_REGULAR, sm_em(f), s) + O.scale - 1) / O.scale;
    int w = 0;
    for (; *s; s++) w += glyph_width(&F[f], (u8)*s) + 1;
    return w;
}

/* the smooth face's text at window column X, on the overlay row y's line, into the text layer; its width in overlay
   pixels (rounded up) */
static int t_draw(int f, const char *s, int X, int y, u8 r, u8 g, u8 b) {
    {
        int sc = O.scale, em = sm_em(f), sh = sc / 3 > 1 ? sc / 3 : 1, w = st_width(ST_REGULAR, em, s);
        int Y = y * sc + (ui_font_height(f) * sc - st_height(ST_REGULAR, em)) / 2;
        if ((r * 3 + g * 6 + b) / 10 >= 160) st_draw(T, g_ww, g_wh, X + sh, Y + sh, ST_REGULAR, em, s, 0x000000);
        st_draw(T, g_ww, g_wh, X, Y, ST_REGULAR, em, s, (unsigned)r << 16 | (unsigned)g << 8 | b);
        int a0 = X - em, a1 = X + w + em + sh, b0 = Y, b1 = Y + 2 * em + sh;
        if (a0 < 0) a0 = 0;
        if (b0 < 0) b0 = 0;
        if (a1 >= g_ww) a1 = g_ww - 1;
        if (b1 >= g_wh) b1 = g_wh - 1;
        if (a1 >= a0 && b1 >= b0) {
            if (a0 < tx0) tx0 = a0;
            if (b0 < ty0) ty0 = b0;
            if (a1 > tx1) tx1 = a1;
            if (b1 > ty1) ty1 = b1;
        }
        return (w + sc - 1) / sc;
    }
}
int ui_text(int f, const char *s, int x, int y, u8 r, u8 g, u8 b) {
    if (g_font && T && g_wh) return t_draw(f, s, x * O.scale, y, r, g, b);
    const Font *t = &F[f];
    u8 c[3] = {r, g, b}, sh[3] = {0, 0, 0};
    int x0 = x;
    bool shadow = (r * 3 + g * 6 + b) / 10 >= 160; /* a shadow only under bright text (dark text on a panel reads clean) */
    for (; *s; s++) {
        int ch = (u8)*s, i = ch, w = glyph_width(t, ch);
        if (t->d.size && i >= 0 && i < t->count && ch != ' ') {
            const u8 *gl = t->d.data + 12 + (size_t)i * (t->size + 1);
            for (int py = 0; py < t->height; py++)
                for (int px = 0; px < w && px < t->row * 8; px++)
                    if (gl[py * t->row + px / 8] & (0x80 >> (px % 8))) {
                        if (shadow) blend(x + px + 1, y + py + 1, sh, 200); /* a shadow, for bright text over the world */
                        blend(x + px, y + py, c, 255);
                    }
        }
        x += w + 1;
    }
    return x - x0;
}

void ui_cursor(int n, int x, int y) {
    const Pic *c = ui_pic("ui\\cursors", n);
    if (!c) return;
    int hx = c->w / 2, hy = c->h / 2;
    if (n == 0) hx = 7, hy = 7;       /* the cross (16 x 15): its centre pixel */
    else if (n == 10) hx = 5, hy = 5; /* the small cross (12 x 11) */
    ui_draw(c, x - hx, y - hy, 1);
}

/* --- parchment ------------------------------------------------------------------------------------------- */
/* a rectangle of the window's own pixels into the text layer, opaque (the outlines: thinner than an overlay pixel) */
static void t_rect(int X, int Y, int W, int H, u8 r, u8 g, u8 b) {
    if (!T) return;
    unsigned c = 0xFF000000u | r | (unsigned)g << 8 | (unsigned)b << 16;
    int x0 = X < 0 ? 0 : X, y0 = Y < 0 ? 0 : Y, x1 = X + W > g_ww ? g_ww : X + W, y1 = Y + H > g_wh ? g_wh : Y + H;
    if (x1 <= x0 || y1 <= y0) return;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) T[(size_t)y * g_ww + x] = c;
    if (x0 < tx0) tx0 = x0;
    if (y0 < ty0) ty0 = y0;
    if (x1 - 1 > tx1) tx1 = x1 - 1;
    if (y1 - 1 > ty1) ty1 = y1 - 1;
}
/* a frame round the overlay rectangle (x, y, w, h), one overlay pixel thick in all (the user, 2026-11-14: "reduce the
   thickness of the outline by half": it was two overlay pixels, the ink and the bevel): at the window's pixels, the
   dark edge half of it and the bevel (lit top-left, shaded bottom-right: ul and dr) the rest */
static void frame_half(int x, int y, int w, int h, const u8 ink[3], const u8 ul[3], const u8 dr[3]) {
    int sc = O.scale, X = x * sc, Y = y * sc, W = w * sc, H = h * sc, e = (sc + 1) / 2, v = sc - e;
    if (!T) return;
    t_rect(X, Y, W, e, ink[0], ink[1], ink[2]), t_rect(X, Y + H - e, W, e, ink[0], ink[1], ink[2]);
    t_rect(X, Y, e, H, ink[0], ink[1], ink[2]), t_rect(X + W - e, Y, e, H, ink[0], ink[1], ink[2]);
    if (v > 0) {
        t_rect(X + e, Y + e, W - 2 * e, v, ul[0], ul[1], ul[2]), t_rect(X + e, Y + e, v, H - 2 * e, ul[0], ul[1], ul[2]);
        t_rect(X + e, Y + H - e - v, W - 2 * e, v, dr[0], dr[1], dr[2]), t_rect(X + W - e - v, Y + e, v, H - 2 * e, dr[0], dr[1], dr[2]);
    }
}
void ui_parchment(int x, int y, int w, int h) {
    static const u8 ink[3] = {UI_INK}, lit[3] = {168, 130, 106}, dark[3] = {UI_PAPER_DARK};
    ui_fill(x, y, w, h, UI_PAPER, 250);
    if (T) { frame_half(x, y, w, h, ink, lit, dark); return; }
    ui_fill(x, y, w, 1, UI_INK, 255), ui_fill(x, y + h - 1, w, 1, UI_INK, 255); /* the outline */
    ui_fill(x, y, 1, h, UI_INK, 255), ui_fill(x + w - 1, y, 1, h, UI_INK, 255);
    ui_fill(x + 1, y + 1, w - 2, 1, 168, 130, 106, 255), ui_fill(x + 1, y + 1, 1, h - 2, 168, 130, 106, 255); /* raised: lit */
    ui_fill(x + 1, y + h - 2, w - 2, 1, UI_PAPER_DARK, 255), ui_fill(x + w - 2, y + 1, 1, h - 2, UI_PAPER_DARK, 255);
}

/* a picture's rows sy..sy+h and columns sx..sx+w onto the overlay at (dx, dy), opaque where the picture is */
void ui_blit(const Pic *p, int sx, int sy, int w, int h, int dx, int dy) {
    if (!p) return;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int px = sx + x, py = sy + y;
            if (px < 0 || py < 0 || px >= p->w || py >= p->h) continue;
            const u8 *c = p->rgba + ((size_t)py * p->w + px) * 4;
            if (c[3] >= 128) blend(dx + x, dy + y, c, 255);
        }
}

/* the original's leather (OPTBTNS.GR picture 3's plain rows, 62..103), w x h, its dark frame round it */
void ui_leather(int x, int y, int w, int h) {
    const Pic *p = ui_pic("ui\\options", 3);
    if (!p || p->h < 104 || p->w < 30) { ui_parchment(x, y, w, h); return; }
    for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < w; xx++) {
            const u8 *c = p->rgba + ((size_t)(62 + yy % 42) * p->w + (size_t)(4 + xx % 26)) * 4;
            blend(x + xx, y + yy, c, 255);
        }
    if (T) {   /* its frame at half the thickness (the user, 2026-11-14), at the window's pixels */
        static const u8 ink[3] = {52, 24, 12}, lit[3] = {89, 48, 12}, dark[3] = {52, 24, 12};
        frame_half(x, y, w, h, ink, lit, dark);
        return;
    }
    ui_fill(x, y, w, 1, 52, 24, 12, 255), ui_fill(x, y + h - 1, w, 1, 52, 24, 12, 255);
    ui_fill(x, y + 1, w, 1, 89, 48, 12, 255);
    ui_fill(x, y, 1, h, 52, 24, 12, 255), ui_fill(x + w - 1, y, 1, h, 52, 24, 12, 255);
}

/* one of the original's buttons (OPTBTNS.GR's "Done": 24 plain, 25 lit), w wide (its 14 rows), its lettering cleared:
   the face's plain row and column stretched between its ends */
void ui_button(int x, int y, int w, bool lit) {
    const Pic *p = ui_pic("ui\\options", lit ? 25 : 24);
    if (!p || p->w < 31 || p->h < 14) { ui_inset(x, y, w, 14, lit); return; }
    for (int yy = 0; yy < p->h; yy++) {
        int sy = yy < 5 || yy > 9 ? yy : 4;
        for (int xx = 0; xx < w; xx++) {
            int sx = xx < 3 ? xx : xx >= w - 7 ? p->w - (w - xx) : 15;
            const u8 *c = p->rgba + ((size_t)sy * p->w + (size_t)sx) * 4;
            if (c[3] >= 128) blend(x + xx, y + yy, c, 255);
        }
    }
}

void ui_inset(int x, int y, int w, int h, bool hot) {
    if (hot) ui_fill(x + 1, y + 1, w - 2, h - 2, 156, 120, 98, 255);
    else ui_fill(x + 1, y + 1, w - 2, h - 2, UI_PAPER_MID, 255);
    ui_fill(x, y, w, 1, UI_INK, 255), ui_fill(x, y, 1, h, UI_INK, 255); /* sunk: shadow above and left */
    ui_fill(x, y + h - 1, w, 1, 172, 136, 112, 255), ui_fill(x + w - 1, y, 1, h, 172, 136, 112, 255);
}

void ui_ink(u8 *r, u8 *g, u8 *b) {
    int R = *r, G = *g, B = *b, l = (R * 3 + G * 6 + B) / 10;
    int hi = MAX(R, MAX(G, B)), lo = MIN(R, MIN(G, B));
    if (l < 100) return; /* already dark */
    if (hi - lo < 40) { /* white or grey: the ink, a lighter grey fainter */
        if (l >= 200) *r = 60, *g = 40, *b = 32;
        else *r = 88, *g = 62, *b = 48;
        return;
    }
    int k = 55 * 256 / l; /* the same hue at the ink's darkness */
    *r = (u8)(R * k / 256), *g = (u8)(G * k / 256), *b = (u8)(B * k / 256);
}

void ui_tip(int mx, int my, const char *title, const char *body, int w) {
    const Overlay *o = ui_overlay();
    int lh = ui_font_height(FONT_NORMAL) + 2, tw = title && title[0] ? ui_text_width(FONT_NORMAL, title) : 0;
    { /* as wide as its longest line needs, up to w */
        char line[512];
        int n = 0, widest = 0;
        for (const char *s = body;; s++) {
            if (*s == '\n' || !*s) {
                line[n] = 0, widest = MAX(widest, ui_text_width(FONT_NORMAL, line)), n = 0;
                if (!*s) break;
            } else if (n < 510) line[n++] = *s;
        }
        w = MIN(w, MAX(tw, widest));
    }
    /* (the user, 2026-11-14: margins of 1-5 pixels, the text justified: the frame is one overlay pixel, the text one
       more inside it; the body's wrapped lines spread to the full width) */
    lh = ui_font_height(FONT_NORMAL) + 1;
    int bh = ui_text_wrap(FONT_NORMAL, body, 0, 0, w, 0, 0, 0, false);
    int bw = w + 4, h = bh + (tw ? lh + 2 : 0) + 4;
    int x = mx + 14, y = my + 6;
    if (x + bw > o->w - 2) x = MAX(2, mx - bw - 8);
    if (y + h > o->h - 2) y = MAX(2, o->h - 2 - h);
    ui_parchment(x, y, bw, h);
    int cy = y + 2;
    if (tw) ui_text(FONT_NORMAL, title, x + 2, cy, UI_INK_HEAD), cy += lh + 2, ui_fill(x + 2, cy - 2, w, 1, UI_PAPER_DARK, 255);
    g_justify = true;
    ui_text_wrap(FONT_NORMAL, body, x + 2, cy, w, UI_INK, true);
    g_justify = false;
}
