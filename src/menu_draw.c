/* menu_draw.c: draws our Esc menu (the game's options menu made again: the game dimmed, the leather, a button a row,
 * the values in gold on the right, the scrollbar, the foot) so that a backend without our renderer (the SDL3
 * backend's uwmenu.c, the Linux build) draws the same menu as uwview.c does: ui.c's overlay laid over a layer of the
 * window's pixels (0xAABBGGRR) the caller gives. Also keeps the menu's scroll position, sliders and tab boxes for the
 * pointer. uwview.c's uwv_menu_rows2, uwv_menu_wheel and uwv_menu_click call these. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "base.h"
#include "ui.h"
#include "smoothtext.h"
#include "menu_draw.h"
/* the logger: the renderer builds find it by -I port\win32, the Esc menu build one folder up */
#if __has_include("rlog.h")
#include "rlog.h"
#else
#include "../rlog.h"
#endif
#define LOG_TAG "menu"
#include <time.h>

/* UWVIEW_MENU_TIMING=1 (the checks, 2026-11-22: the menu's slow frames): where md_rows' time goes, every 120 calls */
static double md_ms(void) { struct timespec t; timespec_get(&t, TIME_UTC); return (double)t.tv_sec * 1000.0 + t.tv_nsec / 1e6; }
static int md_timing_on(void) { static int on = -1; if (on < 0) { const char *e = getenv("UWVIEW_MENU_TIMING"); on = e && *e == '1'; } return on; }
static double md_t[8];
static int md_tn;
static void md_timing(const double *t, int ui_w, int ui_h)
{
    for (int i = 0; i < 7; i++) md_t[i] += t[i + 1] - t[i];
    if (++md_tn < 120) return;
    RLOG_I(LOG_TAG, "md_rows at %dx%d, ms a call: clear %.2f, panel+rows+foot %.2f, composite %.2f, smooth text %.2f, "
           "cursor clear %.2f, cursor blend %.2f, dump %.2f", ui_w, ui_h, md_t[0] / md_tn, md_t[1] / md_tn, md_t[2] / md_tn,
           md_t[3] / md_tn, md_t[4] / md_tn, md_t[5] / md_tn, md_t[6] / md_tn);
    memset(md_t, 0, sizeof md_t), md_tn = 0;
}

/* the menu's list scrolled: the first row shown, and the scrollbar as last drawn (overlay pixels) for the backend's
 * wheel and clicks */
static int menu_top, menu_last_sel = -1, menu_last_n = -1, menu_vis, menu_rows_n;
static int menu_bar, menu_track[4], menu_thumb_y, menu_thumb_h, menu_drag, menu_drag_dy;
/* the wheel: notches up (positive) show earlier rows, down later ones, three rows a notch */
void md_wheel(int notches)
{
    menu_top -= notches * 3;
    if (menu_top > menu_rows_n - menu_vis) menu_top = menu_rows_n - menu_vis;
    if (menu_top < 0) menu_top = 0;
}
/* a left press (down 1) or release (0) at window pixel mx, my: on the thumb it starts a drag, on the track above or
 * below the thumb it pages; 1 when the scrollbar took it (the backend then does not choose a row) */
int md_click(int ui_ok, int mx, int my, int down)
{
    Overlay *o = ui_ok ? ui_overlay() : NULL;
    if (!down) { int was = menu_drag; menu_drag = 0; return was; }
    if (!o || !menu_bar) return 0;
    int s = 3, px = mx / s, py = my / s;   /* (the menu is drawn at 3x: uwv_menu_rows's ui_set_scale) */
    if (px < menu_track[0] - 2 || px >= menu_track[0] + menu_track[2] + 2 || py < menu_track[1] || py >= menu_track[1] + menu_track[3])
        return 0;
    if (py >= menu_thumb_y && py < menu_thumb_y + menu_thumb_h) menu_drag = 1, menu_drag_dy = py - menu_thumb_y;
    else md_wheel(py < menu_thumb_y ? menu_vis / 3 : -(menu_vis / 3));   /* a page: a third of the rows shown, x3 */
    return 1;
}

/* a row's slider: a track between the row's label and its number, the part up to the value filled, a knob at the
 * value. md_sliders gives each row's place along its track before md_rows (0..1; below 0, or a row past n: no
 * slider; n 0: none, as every other page). The tracks as last drawn (overlay pixels) are kept for the pointer
 * (md_slider). */
static float md_frac[160];
static int md_nfrac, md_sx0[160], md_sx1[160], md_sy[160];   /* (sx1 <= sx0: the row's slider is not on the screen) */
void md_sliders(const float *frac, int n)
{
    md_nfrac = frac && n > 0 ? (n > 160 ? 160 : n) : 0;
    for (int i = 0; i < md_nfrac; i++) md_frac[i] = frac[i];
}
/* the pointer (window pixels) and the sliders. row < 0: the row whose track is under the pointer, or -1; row >= 0 (a
 * drag under way): that row, or -1 when its slider is not shown. *frac: the place along the track at mx (0..1) */
int md_slider(int mx, int my, int row, float *frac)
{
    const int s = 3;   /* (the menu is drawn at 3x) */
    int px = mx / s, py = my / s;
    if (row < 0)
        for (int r = 0; r < md_nfrac && row < 0; r++)
            if (md_sx1[r] > md_sx0[r] && px >= md_sx0[r] - 3 && px < md_sx1[r] + 3 && py >= md_sy[r] && py < md_sy[r] + 14) row = r;
    if (row < 0 || row >= md_nfrac || md_sx1[row] <= md_sx0[row]) return -1;
    if (frac) {
        float f = (float)(mx - md_sx0[row] * s - 3 * s / 2) / (float)((md_sx1[row] - md_sx0[row] - 3) * s);
        *frac = f < 0 ? 0 : f > 1 ? 1 : f;
    }
    return row;
}

/* the tabs: a strip of names under the title, the open one marked in gold; the rows given to md_rows are the open
 * tab's. md_tabs gives the names before md_rows (n 0: no strip, as the save and load pages); the names' boxes as last
 * drawn (overlay pixels) are kept for the pointer (md_tab_at). Names that do not fit one line go on to a second. */
#define MD_TABS 12
static char md_tab_name[MD_TABS][32];
static int md_tab_n, md_tab_open, md_tab_box[MD_TABS][4];   /* x, y, w, h (w 0: not drawn) */
void md_tabs(const char *const *names, int n, int open)
{
    int was_n = md_tab_n, was = md_tab_open;
    md_tab_n = names && n > 0 ? (n > MD_TABS ? MD_TABS : n) : 0;
    for (int i = 0; i < md_tab_n; i++) snprintf(md_tab_name[i], sizeof md_tab_name[i], "%s", names[i] ? names[i] : "");
    for (int i = md_tab_n; i < MD_TABS; i++) md_tab_box[i][2] = 0;
    md_tab_open = open;
    if (md_tab_n != was_n || open != was) menu_last_n = -1;   /* another tab: its list from the top */
}
/* the tab whose name is under the pointer (window pixels), or -1 */
int md_tab_at(int mx, int my)
{
    int px = mx / 3, py = my / 3;   /* (the menu is drawn at 3x) */
    for (int i = 0; i < md_tab_n; i++)
        if (md_tab_box[i][2] > 0 && px >= md_tab_box[i][0] && px < md_tab_box[i][0] + md_tab_box[i][2] && py >= md_tab_box[i][1] &&
            py < md_tab_box[i][1] + md_tab_box[i][3]) return i;
    return -1;
}

/* a text fitted into room pixels (smooth text): drawn smaller first (to three quarters), then its middle left out
 * ("..."), the start and the end kept (a model's quantisation is at its end); out the text to draw, *em_out its size */
static void menu_fit(const char *v, int face, int em, int room, char *out, int n, int *em_out)
{
    int e = em, len = (int)strlen(v);
    snprintf(out, (size_t)n, "%s", v);
    if (st_width(face, e, out) > room) e = em * 3 / 4;
    for (int keep = len - 1; st_width(face, e, out) > room && keep >= 4; keep--)
        snprintf(out, (size_t)n, "%.*s...%s", (keep + 1) / 2, v, v + len - keep / 2);
    *em_out = e;
}
/* the same for the game's own letters (capitals, one size): the middle left out until it fits room overlay pixels */
static void menu_fit_orig(const char *v, int room, char *out, int n)
{
    char u[224];
    int k = 0, len;
    for (; v[k] && k < 223; k++) u[k] = (char)(v[k] >= 'a' && v[k] <= 'z' ? v[k] - 32 : v[k]);
    u[k] = 0, len = k;
    snprintf(out, (size_t)n, "%s", u);
    for (int keep = len - 1; ui_text_width(FONT_SMALL, out) > room && keep >= 4; keep--)
        snprintf(out, (size_t)n, "%.*s..%s", (keep + 1) / 2, u, u + len - keep / 2);
}
/* what a row does, broken at its spaces into up to max lines of room pixels (smooth: the face at em, window
 * pixels; else the game's capitals, overlay pixels); a last line still too long ends in "...". Returns the lines. */
#define FOOT_LINES 3
static int menu_wrap(const char *v, bool smooth, int em, int room, char lines[][224], int max)
{
    char u[224], t[224];
    int n = 0, k = 0;
    for (; v[k] && k < 223; k++) u[k] = (char)(!smooth && v[k] >= 'a' && v[k] <= 'z' ? v[k] - 32 : v[k]);
    u[k] = 0;
#define WRAP_W(T) (smooth ? st_width(ST_REGULAR, em, T) : ui_text_width(FONT_SMALL, T))
    for (const char *p = u; n < max;) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (n == max - 1) {   /* the last line takes the rest, its end left out if it must be */
            int len = (int)strlen(p);
            snprintf(lines[n], 224, "%s", p);
            while (WRAP_W(lines[n]) > room && len > 4) snprintf(lines[n], 224, "%.*s...", --len, p);
            n++;
            break;
        }
        int good = 0;
        for (int at = 0;;) {
            int e = at;
            while (p[e] && p[e] != ' ') e++;
            snprintf(t, sizeof t, "%.*s", e, p);
            if (good && WRAP_W(t) > room) break;
            good = e;
            if (!p[e]) break;
            for (at = e; p[at] == ' ';) at++;
            if (!p[at]) break;
        }
        snprintf(lines[n++], 224, "%.*s", good, p);
        p += good;
    }
#undef WRAP_W
    return n;
}
static void md_blend(unsigned *d, int ui_w, int ui_h, int bx0, int by0, int bx1, int by1);
/* the panel's ground: one dark brown under the rows and the foot's text, with a gold line round it; 0 the leather
 * (the row Menu background) */
static int md_solid = 1;
void md_set_solid(int on) { md_solid = on != 0; }
/* the row Quick menu drawing (2026-11-22, the menu's slow frames: 18.7 ms a frame at 1920x1080, the mouse jerky): 1
 * the menu's finished picture kept and laid down again while nothing in it changed (its overlay, its text, the
 * picture under it all the same as last frame), and the pointer laid over its own small box, not the whole window;
 * 0 everything drawn again every frame (as before) */
static int md_fast = 1;
/* the GPU's panel (render_set_panel, set by uwview.c: the fast-UI plan B): the menu's overlay and text laid over the
   picture there, while Quick menu drawing is on and the pointer is the system's (a drawn one would be under it) */
static bool (*md_gpu)(const u8 *rgba, int w, int h, int scale, const unsigned *text, int stride, int x0, int y0, int x1, int y1);
void md_set_gpu(bool (*fn)(const u8 *, int, int, int, const unsigned *, int, int, int, int, int)) { md_gpu = fn; }
static bool (*md_gpu_keep)(void);
static unsigned (*md_gpu_gen)(void);
void md_set_gpu_keep(bool (*keep)(void), unsigned (*gen)(void)) { md_gpu_keep = keep, md_gpu_gen = gen; }
void md_set_fast(int on) { md_fast = on != 0; }
int md_get_fast(void) { return md_fast; }

/* ---- the menu drawn in steps: md_rows fills a MenuFrame on its stack and passes it from step to step ---- */
#define MD_ROW 15                          /* one row's height, overlay pixels */
#define MD_FOOT (16 + FOOT_LINES * 10)     /* the foot's height: the value line and up to three lines of the hint */
#define MD_MAXROWS 160                     /* (uwfront.c's MENU_MAX) */

/* what one row drew: the value as drawn, its width, the room it had, the label's width (window pixels), the value's
 * size (smooth) */
typedef struct { char drawn[224]; int vw, room, lw, em; } RowInfo;

typedef struct {
    /* what the caller gave */
    unsigned char *ui_buf;
    int ui_w, ui_h;
    bool smooth;
    const char *title;
    const char *const *labels, *const *values, *const *hints;
    int n, sel, mx, my;
    /* the overlay and the sizes taken from it */
    Overlay *o;
    int s, em, em_foot, em_tab;
    /* the tab strip */
    int tab_w[MD_TABS], tab_line[MD_TABS], tab_lines, tabh;
    /* the panel and its rows */
    int TOP, vis, panel_rows;
    bool bar;
    int w, h, x, y, px, py, rw;
    int tx, ty, th, tw, thumb_h, thumb_y;   /* the scrollbar */
    int hit;                                /* the row under the pointer, or -1 */
    char ttl[64];
    RowInfo *M;
    /* the foot */
    int fy, fe1, fe2, nf2;
    char foot1[224], foot2[FOOT_LINES][224];
} MenuFrame;

/* the tabs' strip: each name's box as wide as its text, a second line begun when a line is full */
static void md_tabs_measure(MenuFrame *m)
{
    const Overlay *o = m->o;
    const int s = m->s;
    m->tab_lines = 0, m->tabh = 0;
    if (!md_tab_n) return;
    int room = (o->w - 8 < 300 ? o->w - 8 : 300) - 10, used = 0, all = 0, widest = 0;
    for (int i = 0; i < md_tab_n; i++) {
        char l[224];
        if (m->smooth) m->tab_w[i] = (st_width(ST_SEMIBOLD, m->em_tab, md_tab_name[i]) + s - 1) / s + 8;
        else menu_fit_orig(md_tab_name[i], 4096, l, sizeof l), m->tab_w[i] = ui_text_width(FONT_SMALL, l) + 8;
        all += m->tab_w[i] + 1, widest = m->tab_w[i] > widest ? m->tab_w[i] : widest;
    }
    if (all > room) {   /* more than a line: the lines made about as long as each other, not one full and one short */
        int even = all / ((all + room - 1) / room) + widest / 2;
        if (even < room) room = even;
    }
    m->tab_lines = 1;
    for (int i = 0; i < md_tab_n; i++) {
        if (used && used + m->tab_w[i] > room) m->tab_lines++, used = 0;
        m->tab_line[i] = m->tab_lines - 1, used += m->tab_w[i] + 1;
    }
    m->tabh = m->tab_lines * 13 + 3;
}

/* the panel's size and place, how many rows show, the scroll position (the chosen row kept in view, a heading just
 * above it too; a thumb drag followed) and the scrollbar's track and thumb, kept for the wheel and clicks */
static void md_layout(MenuFrame *m)
{
    const Overlay *o = m->o;
    const int n = m->n, sel = m->sel;
    const char *const *labels = m->labels;
    m->TOP = 22 + m->tabh;   /* the rows begin this far under the panel's top: the title, then the tabs */
    int vis = (o->h - 40 - 30 - MD_FOOT - m->tabh) / MD_ROW;
    if (vis < 3) vis = 3;
    m->panel_rows = md_tab_n || vis <= n ? vis : n;   /* (with tabs the panel keeps its full height: the strip stays where it is from tab to tab) */
    if (vis > n) vis = n;
    m->bar = n > vis;
    if (n != menu_last_n) menu_top = 0, menu_last_sel = -1, menu_last_n = n;   /* another page */
    if (sel != menu_last_sel) {        /* the chosen row (the keys) kept in view, a heading just above it too */
        if (sel >= 0 && sel < menu_top) menu_top = sel > 0 && labels[sel - 1][0] == '#' ? sel - 1 : sel;
        if (sel >= 0 && sel >= menu_top + vis) menu_top = sel - vis + 1;
        if (sel > 0 && sel == menu_top && labels[sel - 1][0] == '#') menu_top = sel - 1;   /* (its heading shown with it) */
        menu_last_sel = sel;
    }
    int w = 300;   /* never wider than the window */
    if (w > o->w - 8) w = o->w - 8;
    int h = m->TOP + m->panel_rows * MD_ROW + 4 + MD_FOOT, x = (o->w - w) / 2, y = (o->h - h) / 2;
    if (y < 0) y = 0;
    m->px = m->mx / o->scale, m->py = m->my / o->scale;
    /* the scrollbar: its track right of the rows, the thumb its share of the list */
    m->tx = x + w - 10, m->ty = y + m->TOP, m->th = vis * MD_ROW - 1, m->tw = 5;
    if (m->bar && menu_drag) {            /* the thumb dragged: the list follows the pointer */
        int span = m->th - menu_thumb_h;
        menu_top = span > 0 ? (int)((float)(m->py - menu_drag_dy - m->ty) / (float)span * (float)(n - vis) + 0.5f) : 0;
    }
    if (menu_top > n - vis) menu_top = n - vis;
    if (menu_top < 0) menu_top = 0;
    menu_vis = vis, menu_rows_n = n;
    int thumb_h = m->bar ? m->th * vis / n : m->th, thumb_y = m->bar ? m->ty + (m->th - thumb_h) * menu_top / (n - vis) : m->ty;
    if (thumb_h < 8) thumb_h = 8;
    menu_track[0] = m->tx, menu_track[1] = m->ty, menu_track[2] = m->tw, menu_track[3] = m->th, menu_thumb_y = thumb_y, menu_thumb_h = thumb_h;
    menu_bar = m->bar;
    m->vis = vis, m->w = w, m->h = h, m->x = x, m->y = y, m->thumb_h = thumb_h, m->thumb_y = thumb_y;
    m->rw = m->bar ? w - 8 - 10 : w - 8;   /* the rows' width: room for the scrollbar */
}

/* the dimmed game, the panel's ground, the tabs (their boxes kept for the pointer), the scrollbar and the title */
static void md_draw_panel(MenuFrame *m)
{
    const Overlay *o = m->o;
    const int x = m->x, y = m->y, w = m->w, h = m->h, px = m->px, py = m->py;
    const bool smooth = m->smooth;
    if (md_fast) ui_fill_empty(0, 0, 0, 140);   /* (the game dimmed: md_rows has just emptied the overlay) */
    else ui_fill(0, 0, o->w, o->h, 0, 0, 0, 140);
    if (md_solid) {
        ui_fill(x, y, w, h, 26, 16, 10, 255);
        ui_fill(x, y, w, 1, 120, 96, 40, 255), ui_fill(x, y + h - 1, w, 1, 120, 96, 40, 255);
        ui_fill(x, y, 1, h, 120, 96, 40, 255), ui_fill(x + w - 1, y, 1, h, 120, 96, 40, 255);
    } else ui_leather(x, y, w, h);
    for (int i = 0; i < md_tab_n; i++) {   /* the tabs: the open one lighter with a gold bar under it, the one under the pointer lit */
        int line_w = -1, cx = 0;
        for (int k = 0; k < md_tab_n; k++) if (m->tab_line[k] == m->tab_line[i]) { if (k < i) cx += m->tab_w[k] + 1; line_w += m->tab_w[k] + 1; }
        int bx = x + (w - line_w) / 2 + cx, by = y + 21 + m->tab_line[i] * 13, bw = m->tab_w[i], bh = 12;
        bool open = i == md_tab_open, hot = px >= bx && px < bx + bw && py >= by && py < by + bh;
        ui_fill(bx, by, bw, bh, open ? 96 : hot ? 72 : 44, open ? 74 : hot ? 54 : 30, open ? 26 : hot ? 28 : 18, 255);
        if (open) ui_fill(bx, by + bh - 2, bw, 2, 255, 220, 60, 255);
        if (!smooth) {
            char l[224];
            menu_fit_orig(md_tab_name[i], 4096, l, sizeof l);
            ui_text(FONT_SMALL, l, bx + 4, by + 3, open ? 255 : 207, open ? 220 : 207, open ? 60 : 223);
        }
        md_tab_box[i][0] = bx, md_tab_box[i][1] = by, md_tab_box[i][2] = bw, md_tab_box[i][3] = bh;
    }
    if (md_tab_n) ui_fill(x + 6, y + 21 + m->tab_lines * 13, w - 12, 1, 120, 96, 40, 160);   /* a rule under them */
    if (m->bar) {
        ui_fill(m->tx, m->ty, m->tw, m->th, 40, 28, 16, 200);                                           /* the track */
        bool on = menu_drag || (px >= m->tx - 2 && px < m->tx + m->tw + 2 && py >= m->thumb_y && py < m->thumb_y + m->thumb_h);
        ui_fill(m->tx, m->thumb_y, m->tw, m->thumb_h, on ? 255 : 210, on ? 220 : 170, on ? 60 : 40, 255); /* the thumb, gold */
    }
    {
        int k = 0;
        for (; m->title[k] && k < 63; k++) m->ttl[k] = (char)(!smooth && m->title[k] >= 'a' && m->title[k] <= 'z' ? m->title[k] - 32 : m->title[k]);
        m->ttl[k] = 0;
        if (!smooth) ui_text(FONT_NORMAL, m->ttl, x + w / 2 - ui_text_width(FONT_NORMAL, m->ttl) / 2, y + 7, 232, 190, 40);
    }
}

/* what each row will draw (every row, shown or not: the checks read them all): the value as drawn, the widths in
 * window pixels, the value's size (smooth) */
static void md_measure_rows(MenuFrame *m)
{
    const int s = m->s, em = m->em, rw = m->rw;
    RowInfo *M = m->M;
    for (int r = 0; r < m->n && r < MD_MAXROWS; r++) {
        const char *lab = m->labels[r], *val = m->values && m->values[r] ? m->values[r] : "";
        bool head = lab[0] == '#';
        if (head) lab++;
        M[r].drawn[0] = 0, M[r].vw = 0, M[r].em = em;
        if (m->smooth) {
            M[r].lw = st_width(ST_SEMIBOLD, em, lab);
            M[r].room = rw * s - 9 * s - M[r].lw - 6 * s - 5 * s;
            if (*val && !head) menu_fit(val, ST_REGULAR, em, M[r].room, M[r].drawn, sizeof M[r].drawn, &M[r].em),
                               M[r].vw = st_width(ST_REGULAR, M[r].em, M[r].drawn);
        } else {
            char l[224];
            menu_fit_orig(lab, 4096, l, sizeof l);
            M[r].lw = ui_text_width(FONT_SMALL, l) * s;
            M[r].room = rw * s - 9 * s - M[r].lw - 6 * s - 5 * s;
            if (*val && !head) menu_fit_orig(val, M[r].room / s, M[r].drawn, sizeof M[r].drawn),
                               M[r].vw = ui_text_width(FONT_SMALL, M[r].drawn) * s;
        }
    }
}

/* the shown rows' buttons, headings and sliders on the overlay (the game's letters here; the smooth text comes
 * later, md_smooth_text); finds the row under the pointer (m->hit) and keeps each slider's track for the pointer */
static void md_draw_rows(MenuFrame *m)
{
    const int x = m->x, y = m->y, rw = m->rw, s = m->s, px = m->px, py = m->py, n = m->n, sel = m->sel;
    const bool smooth = m->smooth;
    const RowInfo *M = m->M;
    for (int r = 0; r < MD_MAXROWS; r++) md_sx0[r] = md_sx1[r] = 0;
    for (int r = menu_top; r < menu_top + m->vis && r < n && r < MD_MAXROWS; r++) {
        int ry = y + m->TOP + (r - menu_top) * MD_ROW;
        bool head = m->labels[r][0] == '#';
        if (head) {
            ui_fill(x + 6, ry + 13, rw - 4, 1, 120, 96, 40, 160);   /* a rule under the heading */
            if (!smooth) {
                char l[224];
                menu_fit_orig(m->labels[r] + 1, 4096, l, sizeof l);
                ui_text(FONT_SMALL, l, x + 7, ry + 5, 232, 190, 40);
            }
            continue;
        }
        if (!menu_drag && px >= x + 4 && px < x + 4 + rw && py >= ry && py < ry + 14) m->hit = r;
        ui_button(x + 4, ry, rw, r == sel || r == m->hit);
        if (!smooth) {
            char l[224];
            menu_fit_orig(m->labels[r], 4096, l, sizeof l);
            ui_text(FONT_SMALL, l, x + 9, ry + 5, 207, 207, 223);
            if (M[r].drawn[0]) ui_text(FONT_SMALL, M[r].drawn, x + 4 + rw - 6 - M[r].vw / s, ry + 5, 255, 220, 60);
        }
        if (r < md_nfrac && md_frac[r] >= 0) {   /* its slider: the same columns on every row, less where the label or the number is wider */
            int x0 = x + 4 + rw * 48 / 100, x1 = x + 4 + rw - 6 - 40;
            int l1 = x + 9 + (M[r].lw + s - 1) / s + 5, v0 = x + 4 + rw - 6 - (M[r].vw + s - 1) / s - 5;
            if (x0 < l1) x0 = l1;
            if (x1 > v0) x1 = v0;
            if (x1 - x0 >= 24) {
                float f = md_frac[r] > 1 ? 1 : md_frac[r];
                int kx = x0 + (int)(f * (float)(x1 - x0 - 3) + 0.5f);
                bool on = r == sel || r == m->hit;
                ui_fill(x0, ry + 5, x1 - x0, 4, 120, 96, 40, 255);                 /* the track: dark, a gold line round it */
                ui_fill(x0 + 1, ry + 6, x1 - x0 - 2, 2, 26, 16, 10, 255);
                ui_fill(x0 + 1, ry + 6, kx - x0, 2, 210, 170, 40, 255);            /* the part up to the value, gold */
                ui_fill(kx, ry + 2, 3, 10, on ? 255 : 232, on ? 220 : 190, on ? 60 : 40, 255);   /* the knob */
                md_sx0[r] = x0, md_sx1[r] = x1, md_sy[r] = ry;
            }
        }
    }
}

/* the foot: the row chosen or under the pointer: its value in full, and what it does (the game's letters drawn here) */
static void md_foot(MenuFrame *m)
{
    const int x = m->x, y = m->y, w = m->w, s = m->s, n = m->n;
    const char *const *labels = m->labels, *const *values = m->values, *const *hints = m->hints;
    int fr = m->hit >= 0 ? m->hit : m->sel, fy = y + m->TOP + m->panel_rows * MD_ROW + 3;
    m->fy = fy;
    m->foot1[0] = 0, m->nf2 = 0;
    m->fe1 = m->em_foot, m->fe2 = m->em_foot * 10 / 11;
    ui_fill(x + 6, fy - 1, w - 12, 1, 120, 96, 40, 160);
    if (fr >= 0 && fr < n && labels[fr][0] != '#') {
        char t[224];
        const char *val = values && values[fr] ? values[fr] : "", *hint = hints && hints[fr] ? hints[fr] : "";
        if (*val) snprintf(t, sizeof t, "%s: %s", labels[fr], val);
        else snprintf(t, sizeof t, "%s", labels[fr]);
        if (m->smooth) {
            menu_fit(t, ST_REGULAR, m->em_foot, (w - 16) * s, m->foot1, sizeof m->foot1, &m->fe1);
            m->nf2 = menu_wrap(hint, true, m->fe2, (w - 16) * s, m->foot2, FOOT_LINES);
        } else {
            menu_fit_orig(t, w - 16, m->foot1, sizeof m->foot1), m->nf2 = menu_wrap(hint, false, 0, w - 16, m->foot2, FOOT_LINES);
            ui_text(FONT_SMALL, m->foot1, x + 8, fy + 4, 255, 220, 60);
            for (int i = 0; i < m->nf2; i++) ui_text(FONT_SMALL, m->foot2[i], x + 8, fy + 15 + i * 10, 207, 207, 223);
        }
    }
}

/* the overlay laid over the caller's window pixels; where the 3D view shows through (a clear hole) the dimming and
 * the paper go over it, not opaque black; then the interface font's smooth text over it (ui.c) */
static void md_composite(MenuFrame *m)
{
    const Overlay *o = m->o;
    const int ui_w = m->ui_w, ui_h = m->ui_h;
    unsigned *d = (unsigned *)m->ui_buf;
    for (int yy = 0; yy < ui_h; yy++) {
        int oy = yy / o->scale;
        if (oy >= o->h) break;
        const u8 *orow = o->px + (size_t)oy * o->w * 4;
        for (int xx = 0; xx < ui_w; xx++) {
            int ox = xx / o->scale;
            if (ox >= o->w) break;
            const u8 *p = orow + ox * 4;
            if (p[3] == 0) continue;
            unsigned a = p[3], na = 255 - a, c = d[(size_t)yy * ui_w + xx];
            if (!(c >> 24)) {   /* the 3D view shows here (a clear hole): the dimming and the paper over it, not opaque black */
                d[(size_t)yy * ui_w + xx] = a << 24 | p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16;
                continue;
            }
            unsigned r = (p[0] * a + (c & 0xFF) * na) / 255, g = (p[1] * a + (c >> 8 & 0xFF) * na) / 255, b = (p[2] * a + (c >> 16 & 0xFF) * na) / 255;
            d[(size_t)yy * ui_w + xx] = 0xFF000000u | r | g << 8 | b << 16;
        }
    }
    ui_text_apply(d, ui_w, ui_h);   /* (the interface font's smooth text over the overlay: ui.c) */
}

/* the menu's own text at the window's resolution over the art, each with a dark edge under it (the leather is lit):
 * title, tab names, labels, values, the foot */
static void md_smooth_text(MenuFrame *m)
{
    const int x = m->x, y = m->y, w = m->w, s = m->s, em = m->em, em_tab = m->em_tab, rw = m->rw, n = m->n;
    const int ui_w = m->ui_w, ui_h = m->ui_h, vis = m->vis, TOP = m->TOP, fy = m->fy;
    const char *const *labels = m->labels;
    const RowInfo *M = m->M;
    unsigned *d = (unsigned *)m->ui_buf;
#define SMT(X, Y, FACE, EM, T, RGB) (st_draw(d, ui_w, ui_h, (X) + 2, (Y) + 2, FACE, EM, T, 0x000000), st_draw(d, ui_w, ui_h, X, Y, FACE, EM, T, RGB))
    int et = em * 5 / 4, hh = st_height(ST_REGULAR, em);
    SMT(x * s + w * s / 2 - st_width(ST_SEMIBOLD, et, m->ttl) / 2, y * s + (22 * s - st_height(ST_SEMIBOLD, et)) / 2, ST_SEMIBOLD, et, m->ttl, 0xE8BE28);
    for (int i = 0; i < md_tab_n; i++)   /* the tabs' names, each in the middle of its box */
        SMT(md_tab_box[i][0] * s + (md_tab_box[i][2] * s - st_width(ST_SEMIBOLD, em_tab, md_tab_name[i])) / 2,
            md_tab_box[i][1] * s + (10 * s - st_height(ST_SEMIBOLD, em_tab)) / 2, ST_SEMIBOLD, em_tab, md_tab_name[i],
            i == md_tab_open ? 0xFFDC3C : 0xD8D8E4);
    for (int r = menu_top; r < menu_top + vis && r < n && r < MD_MAXROWS; r++) {
        int ry = (y + TOP + (r - menu_top) * MD_ROW) * s, ty2 = ry + (14 * s - hh) / 2;
        if (labels[r][0] == '#') { SMT((x + 7) * s, ty2 - s, ST_SEMIBOLD, em, labels[r] + 1, 0xE8BE28); continue; }
        SMT((x + 9) * s, ty2, ST_SEMIBOLD, em, labels[r], 0xE4E4F0);
        if (M[r].drawn[0])
            SMT((x + 4 + rw - 6) * s - M[r].vw, ry + (14 * s - st_height(ST_REGULAR, M[r].em)) / 2, ST_REGULAR, M[r].em, M[r].drawn, 0xFFDC3C);
    }
    if (m->foot1[0]) SMT((x + 8) * s, (fy + 2) * s, ST_REGULAR, m->fe1, m->foot1, 0xFFDC3C);
    for (int i = 0; i < m->nf2; i++) SMT((x + 8) * s, (fy + 13 + i * 10) * s, ST_REGULAR, m->fe2, m->foot2[i], 0xD8D8E4);
#undef SMT
}

/* Quick menu drawing's kept picture: the layer under the menu as it came (under), the finished picture (done), the
 * overlay (opx) and the interface font's text layer (ttx, its drawn rectangle) the picture was made from, and what the
 * smooth text drew (sig) */
static struct {
    unsigned *under, *done, *ttx;
    u8 *opx;
    size_t cap, ocap, tcap;
    int w, h, ow, oh, tx0, ty0, tx1, ty1, valid;
    unsigned sig;
} K;
static unsigned fnv_s(unsigned h, const char *t) { for (; t && *t; t++) h = (h ^ (unsigned char)*t) * 16777619u; return (h ^ 0xFF) * 16777619u; }
static unsigned fnv_i(unsigned h, int v) { return (h ^ (unsigned)v) * 16777619u; }
/* everything md_smooth_text draws from */
static unsigned md_text_sig(const MenuFrame *m)
{
    unsigned h = 2166136261u;
    h = fnv_i(h, m->smooth), h = fnv_i(h, m->x), h = fnv_i(h, m->y), h = fnv_i(h, m->w), h = fnv_i(h, m->s);
    h = fnv_i(h, m->em), h = fnv_i(h, m->em_tab), h = fnv_i(h, m->rw), h = fnv_i(h, m->n), h = fnv_i(h, m->vis);
    h = fnv_i(h, m->TOP), h = fnv_i(h, m->fy), h = fnv_i(h, m->ui_w), h = fnv_i(h, m->ui_h), h = fnv_i(h, menu_top);
    h = fnv_s(h, st_font_name()), h = fnv_i(h, st_size_pct()), h = fnv_s(h, m->ttl);
    h = fnv_i(h, md_tab_n), h = fnv_i(h, md_tab_open);
    for (int i = 0; i < md_tab_n; i++)
        h = fnv_s(h, md_tab_name[i]), h = fnv_i(h, md_tab_box[i][0]), h = fnv_i(h, md_tab_box[i][1]), h = fnv_i(h, md_tab_box[i][2]);
    for (int r = menu_top; r < menu_top + m->vis && r < m->n && r < MD_MAXROWS; r++)
        h = fnv_s(h, m->labels[r]), h = fnv_s(h, m->M[r].drawn), h = fnv_i(h, m->M[r].vw), h = fnv_i(h, m->M[r].em);
    h = fnv_s(h, m->foot1), h = fnv_i(h, m->fe1), h = fnv_i(h, m->fe2), h = fnv_i(h, m->nf2);
    for (int i = 0; i < m->nf2; i++) h = fnv_s(h, m->foot2[i]);
    return h;
}
static int grow(void **p, size_t *cap, size_t want)
{
    if (*cap >= want) return 1;
    void *n = realloc(*p, want);
    if (!n) { RLOG_E(LOG_TAG, "out of memory (%zu bytes) for the kept menu picture: drawn every frame", want); return 0; }
    *p = n, *cap = want;
    return 1;
}
/* 1 when last frame's finished picture is this frame's too: then it is laid down (d) */
static int md_kept(const MenuFrame *m, unsigned sig)
{
    const Overlay *o = m->o;
    const size_t all = (size_t)m->ui_w * (size_t)m->ui_h;
    unsigned *d = (unsigned *)m->ui_buf;
    int x0, y0, x1, y1, ts;
    const unsigned *t = ui_text_rect(&x0, &y0, &x1, &y1, &ts);
    if (!K.valid || sig != K.sig || K.w != m->ui_w || K.h != m->ui_h || K.ow != o->w || K.oh != o->h) return 0;
    if (x0 != K.tx0 || y0 != K.ty0 || x1 != K.tx1 || y1 != K.ty1) return 0;
    if (memcmp(K.opx, o->px, (size_t)o->w * o->h * 4)) return 0;
    if (t && x1 >= x0)
        for (int y = y0; y <= y1; y++)
            if (memcmp(K.ttx + (size_t)(y - y0) * (x1 - x0 + 1), t + (size_t)y * ts + x0, (size_t)(x1 - x0 + 1) * 4)) return 0;
    /* the layer under it the same as last frame's: laid over by the finished picture in the same pass (one read of
       the window's pixels, not two); at the first pixel that differs, what was laid down so far is put back */
    {   /* (in blocks of 16 KB: the library's memcmp and memcpy, each over a block still in the cache) */
        const size_t bytes = all * 4, B = 16384;
        unsigned char *dd = (unsigned char *)d;
        const unsigned char *uu = (const unsigned char *)K.under, *kk = (const unsigned char *)K.done;
        for (size_t at = 0; at < bytes; at += B) {
            size_t len = bytes - at < B ? bytes - at : B;
            if (memcmp(dd + at, uu + at, len)) { memcpy(dd, uu, at); return 0; }
            memcpy(dd + at, kk + at, len);
        }
    }
    return 1;
}
/* before the drawing: what it is made from, kept (the layer under it, the overlay, the text layer); 0 no room */
static int md_keep_from(const MenuFrame *m, unsigned sig)
{
    const Overlay *o = m->o;
    const size_t all = (size_t)m->ui_w * (size_t)m->ui_h;
    int x0, y0, x1, y1, ts;
    const unsigned *t = ui_text_rect(&x0, &y0, &x1, &y1, &ts);
    size_t tn = t && x1 >= x0 ? (size_t)(x1 - x0 + 1) * (size_t)(y1 - y0 + 1) : 0;
    K.valid = 0;
    if (!grow((void **)&K.under, &K.cap, all * 4) || !grow((void **)&K.opx, &K.ocap, (size_t)o->w * o->h * 4)) return 0;
    if (tn && !grow((void **)&K.ttx, &K.tcap, tn * 4)) return 0;
    {   /* (done: the same size as under) */
        static size_t dcap;
        if (!grow((void **)&K.done, &dcap, all * 4)) return 0;
    }
    memcpy(K.under, m->ui_buf, all * 4);
    memcpy(K.opx, o->px, (size_t)o->w * o->h * 4);
    for (int y = y0; tn && y <= y1; y++) memcpy(K.ttx + (size_t)(y - y0) * (x1 - x0 + 1), t + (size_t)y * ts + x0, (size_t)(x1 - x0 + 1) * 4);
    K.w = m->ui_w, K.h = m->ui_h, K.ow = o->w, K.oh = o->h, K.tx0 = x0, K.ty0 = y0, K.tx1 = x1, K.ty1 = y1, K.sig = sig;
    return 1;
}

/* UWVIEW_MENU_DUMP=FILE (the checks): what was drawn, a line a row, written when it changes; tools\menucheck.py reads
 * the "menu: tab" and "menu: sel" lines from stderr as they are, so they stay plain lines, not logger lines */
static void md_dump(const MenuFrame *m)
{
    const char *dump = getenv("UWVIEW_MENU_DUMP");
    static unsigned last;
    if (!(dump && *dump)) return;
    const int n = m->n, sel = m->sel, s = m->s, vis = m->vis;
    const char *const *labels = m->labels, *const *values = m->values;
    const RowInfo *M = m->M;
    unsigned hsh = 2166136261u ^ (unsigned)sel ^ (unsigned)menu_top << 8 ^ (unsigned)n << 16 ^ (unsigned)m->smooth << 30;
    hsh ^= (unsigned)md_tab_open << 24 ^ (unsigned)md_tab_n << 4;
    for (int r = 0; r < n && r < MD_MAXROWS; r++) {
        for (const char *c = labels[r]; *c; c++) hsh = (hsh ^ (unsigned char)*c) * 16777619u;
        for (const char *c = values && values[r] ? values[r] : ""; *c; c++) hsh = (hsh ^ (unsigned char)*c) * 16777619u;
    }
    if (hsh == last) return;
    FILE *f = fopen(dump, "w");
    last = hsh;
    if (!f) RLOG_E(LOG_TAG, "cannot open %s: %s", dump, rlog_errno());
    if (md_tab_n && md_tab_open >= 0 && md_tab_open < md_tab_n) fprintf(stderr, "menu: tab %d of %d %s\n", md_tab_open, md_tab_n, md_tab_name[md_tab_open]);
    if (sel >= 0 && sel < n)
        fprintf(stderr, "menu: sel %d head %d shown %d %s = %s\n", sel, labels[sel][0] == '#', sel >= menu_top && sel < menu_top + vis,
                labels[sel], values && values[sel] ? values[sel] : "");
    if (f) {
        fprintf(f, "panel\t%d\t%d\t%d\t%d\t%d\t%d\t%s\t%s\t%s\n", m->x * s, m->y * s, m->w * s, m->h * s, m->ui_w, m->ui_h,
                m->smooth ? "smooth" : "original", m->smooth ? st_font_name() : "FONT_SMALL", m->title);
        for (int i = 0; i < md_tab_n; i++)   /* a line a tab: its number, 1 the open one, its name, its box (window pixels) */
            fprintf(f, "tab\t%d\t%d\t%s\t%d\t%d\t%d\t%d\n", i, i == md_tab_open, md_tab_name[i], md_tab_box[i][0] * s,
                    md_tab_box[i][1] * s, md_tab_box[i][2] * s, md_tab_box[i][3] * s);
        for (int r = 0; r < n && r < MD_MAXROWS; r++)
            fprintf(f, "row\t%d\t%d\t%d\t%s\t%s\t%s\t%d\t%d\t%d\n", r, labels[r][0] == '#', r >= menu_top && r < menu_top + vis,
                    labels[r] + (labels[r][0] == '#'), values && values[r] ? values[r] : "", M[r].drawn, M[r].vw, M[r].room, M[r].lw);
        fprintf(f, "sel\t%d\nfoot1\t%s\nfoot2\t", sel, m->foot1);
        for (int i = 0; i < m->nf2; i++) fprintf(f, "%s%s", i ? " | " : "", m->foot2[i]);
        fprintf(f, "\n");
        fclose(f);
    }
}

/* The Esc menu drawn (the game's options menu look: the game dimmed, the leather, a button a row, the values in gold
 * on the right) over the interface layer ui_buf; returns the row under the pointer, or -1. The text is a smooth font
 * at the window's own resolution in the letters' own case (smoothtext.c; settings.txt menu_font original brings the
 * game's small capitals back); a label beginning with '#' is a heading (gold, no button, never chosen); a value too
 * long for its row is drawn smaller, then with its middle left out; under the rows the foot for the row chosen or
 * under the pointer: its value in full, and what the row does (hints[r], NULL or "" none) on up to three lines. */
int md_rows(unsigned char *ui_buf, int ui_w, int ui_h, bool smooth, const char *title, const char *const *labels,
            const char *const *values, const char *const *hints, int n, int sel, int mx, int my)
{
    if (!ui_buf) return -1;   /* (the caller has started ui.c: ui_start) */
    double tt[8];
    const int timed = md_timing_on();
    if (timed) tt[0] = md_ms();
    ui_set_scale(3);
    static RowInfo M[MD_MAXROWS];
    MenuFrame m = {0};
    m.ui_buf = ui_buf, m.ui_w = ui_w, m.ui_h = ui_h, m.smooth = smooth, m.title = title;
    m.labels = labels, m.values = values, m.hints = hints, m.n = n, m.sel = sel, m.mx = mx, m.my = my;
    m.o = ui_overlay();
    ui_clear();
    m.s = m.o->scale;
    m.em_tab = 14 * m.s * 6 / 10 * 17 / 20;
    m.em = 14 * m.s * 6 / 10, m.em_foot = 22 * m.s / 3;
    m.hit = -1;
    m.M = M;
    if (timed) tt[1] = md_ms();
    md_tabs_measure(&m);
    md_layout(&m);
    md_draw_panel(&m);
    md_measure_rows(&m);
    md_draw_rows(&m);
    md_foot(&m);
    if (timed) tt[2] = md_ms();
    if (md_fast && md_gpu && ui_sys_pointer_on()) {   /* on the GPU: the overlay and the text go up, nothing into the layer */
        int x0, y0, x1, y1, stride;
        unsigned *T = (unsigned *)ui_text_rect(&x0, &y0, &x1, &y1, &stride);
        /* what the GPU has is this menu still (the same text, overlay and text rectangle, no other panel since): kept */
        static unsigned last_sig, last_gen;
        static u8 *last_ov;
        static size_t last_cap;
        static int last_w, last_h, last_s;
        const Overlay *o = m.o;
        const size_t ob = (size_t)o->w * o->h * 4;
        unsigned sig = md_text_sig(&m) * 16777619u ^ (unsigned)smooth;
        int rr[4] = {x0, y0, x1, y1};
        for (int k = 0; k < 4; k++) sig = (sig ^ (unsigned)rr[k]) * 16777619u;
        if (T && stride == ui_w && md_gpu_keep && md_gpu_gen && last_ov && sig == last_sig && o->w == last_w && o->h == last_h &&
            o->scale == last_s && md_gpu_gen() == last_gen && !memcmp(last_ov, o->px, ob) && md_gpu_keep()) {
            K.valid = 0;
            if (mx >= 0 && my >= 0) ui_pointer_take(-1, 0);
            md_dump(&m);
            if (timed) tt[3] = tt[4] = tt[5] = tt[6] = tt[7] = md_ms(), md_timing(tt, ui_w, ui_h);
            ui_set_scale(0);
            return m.hit;
        }
        if (T && stride == ui_w) {
            if (smooth) {   /* the menu's own text into the text layer (premultiplied, as ui.c's), over the panel's rectangle */
                MenuFrame t = m;
                t.ui_buf = (unsigned char *)T;
                md_smooth_text(&t);
                ui_text_mark(m.x * m.s, m.y * m.s, (m.x + m.w) * m.s + m.s, (m.y + m.h) * m.s + m.s);
                ui_text_rect(&x0, &y0, &x1, &y1, &stride);
            }
            if (timed) tt[3] = md_ms();
            if (md_gpu(m.o->px, m.o->w, m.o->h, m.o->scale, T, stride, x0, y0, x1, y1)) {
                if (grow((void **)&last_ov, &last_cap, ob)) {   /* (what the GPU has now: the next frame's test) */
                    memcpy(last_ov, o->px, ob);
                    last_sig = sig, last_w = o->w, last_h = o->h, last_s = o->scale, last_gen = md_gpu_gen ? md_gpu_gen() : 0;
                }
                K.valid = 0;   /* (the layer holds no menu: the kept picture is not this one) */
                if (mx >= 0 && my >= 0) ui_pointer_take(-1, 0);   /* (the system's cursor shows it) */
                md_dump(&m);
                if (timed) tt[4] = tt[5] = tt[6] = tt[7] = md_ms(), md_timing(tt, ui_w, ui_h);
                ui_set_scale(0);
                return m.hit;
            }
        }
    }
    {   /* Quick menu drawing: last frame's picture again when nothing in it changed */
        unsigned sig = md_fast ? md_text_sig(&m) : 0;
        int kept = md_fast && md_kept(&m, sig);
        static int verify = -1;   /* UWVIEW_MENU_VERIFY=1 (the checks): each kept picture against the menu drawn again */
        if (verify < 0) { const char *e = getenv("UWVIEW_MENU_VERIFY"); verify = e && *e == '1'; }
        if (kept && verify) {
            static unsigned *tmp;
            static size_t tcap;
            static int hits, bad;
            const size_t all = (size_t)ui_w * ui_h;
            if (grow((void **)&tmp, &tcap, all * 4)) {
                memcpy(tmp, ui_buf, all * 4);
                memcpy(ui_buf, K.under, all * 4);
                md_composite(&m);
                if (smooth) md_smooth_text(&m);
                size_t diff = 0;
                for (size_t i = 0; i < all; i++) diff += tmp[i] != ((unsigned *)ui_buf)[i];
                hits++, bad += diff != 0;
                if (diff || hits % 240 == 1) RLOG_I(LOG_TAG, "menu verify: kept picture %d: %zu pixels differ from the menu drawn again (%d of %d differed)", hits, diff, bad, hits);
            }
        }
        if (!kept) {
            int keep = md_fast && md_keep_from(&m, sig);
            md_composite(&m);
            if (timed) tt[3] = md_ms();
            if (smooth) md_smooth_text(&m);
            if (keep) memcpy(K.done, ui_buf, (size_t)ui_w * ui_h * 4), K.valid = 1;
            if (!md_fast) K.valid = 0;
        } else if (timed) tt[3] = md_ms();
    }
    if (timed) tt[4] = tt[5] = tt[6] = md_ms();
    if (mx >= 0 && my >= 0 && !ui_pointer_take(-1, 0)) {   /* (else the system's cursor shows it) the game's cursor (the game is paused under the menu), over the text */
        ui_clear();
        ui_cursor(10, m.px, m.py);
        if (timed) tt[5] = md_ms();
        if (md_fast) md_blend((unsigned *)ui_buf, ui_w, ui_h, m.px - 16, m.py - 16, m.px + 17, m.py + 17);   /* (the small cross: 12 by 11) */
        else md_blend((unsigned *)ui_buf, ui_w, ui_h, 0, 0, 1 << 20, 1 << 20);
        if (timed) tt[6] = md_ms();
    }
    md_dump(&m);
    if (timed) tt[7] = md_ms(), md_timing(tt, ui_w, ui_h);
    ui_set_scale(0);   /* the window's own scale again for the HUD and the panels */
    return m.hit;
}
/* the overlay (only the cursor now) laid over the window pixels d, then the smooth text layer; only the overlay's
 * pixels bx0..bx1-1, by0..by1-1 (the cursor's box; the whole overlay when Quick menu drawing is off) */
static void md_blend(unsigned *d, int ui_w, int ui_h, int bx0, int by0, int bx1, int by1)
{
    Overlay *o = ui_overlay();
    if (bx0 < 0) bx0 = 0;
    if (by0 < 0) by0 = 0;
    if (bx1 > o->w) bx1 = o->w;
    if (by1 > o->h) by1 = o->h;
    for (int yy = by0 * o->scale; yy < by1 * o->scale && yy < ui_h; yy++) {
        int oy = yy / o->scale;
        if (oy >= o->h) break;
        const u8 *orow = o->px + (size_t)oy * o->w * 4;
        for (int xx = bx0 * o->scale; xx < bx1 * o->scale && xx < ui_w; xx++) {
            int ox = xx / o->scale;
            if (ox >= o->w) break;
            const u8 *p = orow + ox * 4;
            if (p[3] == 0) continue;
            unsigned a = p[3], na = 255 - a, c = d[(size_t)yy * ui_w + xx];
            unsigned r = (p[0] * a + (c & 0xFF) * na) / 255, g = (p[1] * a + (c >> 8 & 0xFF) * na) / 255, b = (p[2] * a + (c >> 16 & 0xFF) * na) / 255;
            d[(size_t)yy * ui_w + xx] = 0xFF000000u | r | g << 8 | b << 16;
        }
    }
    ui_text_apply(d, ui_w, ui_h);   /* (the interface font's smooth text over the overlay: ui.c) */
}
