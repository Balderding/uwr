/* menu_draw.c: our Esc menu's drawing (uw.exe's options menu made again: the game dimmed, the leather, a button a row,
 * the values in gold on the right, the scrollbar, the foot), moved out of uwview.c unchanged (2026-11-15) so that a
 * backend without our renderer (the SDL3 backend's uwmenu.c, the Linux build) draws the same menu: ui.c's overlay
 * laid over a layer of the window's pixels (0xAABBGGRR) the caller gives. uwview.c's uwv_menu_rows2, uwv_menu_wheel and
 * uwv_menu_click call these. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "base.h"
#include "ui.h"
#include "smoothtext.h"
#include "menu_draw.h"

/* the menu's list scrolled (the user, 2026-11-13: more rows than fit, more to come): the first row shown, and the
 * scrollbar as last drawn (overlay pixels) for the backend's wheel and clicks */
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

/* a row's slider (the Fog and smoke page, 2026-11-17, the user: "esc menu sliders and inputs"): a track between the
 * row's label and its number, the part up to the value filled, a knob at the value. md_sliders gives each row's place
 * along its track before md_rows (0..1; below 0, or a row past n: no slider; n 0: none, as every other page). The
 * tracks as last drawn (overlay pixels) are kept for the pointer (md_slider). */
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

/* the tabs (2026-11-17, the user: "a tab for each section of the esc menu to make it easier to get to the option"): a
 * strip of names under the title, the open one marked in gold; the rows given to md_rows are the open tab's. md_tabs
 * gives the names before md_rows (n 0: no strip, as the save and load pages); the names' boxes as last drawn (overlay
 * pixels) are kept for the pointer (md_tab_at). Names that do not fit one line go on to a second. */
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
 * pixels; else the game's capitals, overlay pixels); a last line still too long ends in "...". Returns the lines.
 * (2026-11-16: the one line a hint had was cut in its middle for 29 of the rows.) */
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
static void md_blend(unsigned *d, int ui_w, int ui_h);
/* the panel's ground (the user, 2026-11-16: "a solid color so it is easier to see"): one dark brown under the rows
 * and the foot's text, with a gold line round it; 0 the leather as before (the row Menu background) */
static int md_solid = 1;
void md_set_solid(int on) { md_solid = on != 0; }

/* uw.exe's options menu drawn (its look: the game dimmed, the leather, a button a row, the values in gold on the
 * right) over the interface layer; returns the row under the pointer, or -1.
 * 2026-11-14 (the user: the font was hard to read and the language model's name was cut): the text is a smooth font
 * at the window's own resolution, in the letters' own case (smoothtext.c; settings.txt menu_font original brings the
 * game's small capitals back); a label beginning with '#' is a heading (gold, no button, never chosen); a value too
 * long for its row is drawn smaller, then with its middle left out; under the rows the foot for the row chosen or
 * under the pointer: its value in full, and what the row does (hints[r], NULL or "" none) on up to three lines.
 * UWVIEW_MENU_DUMP=FILE (the checks): what was drawn, a line a row. */
int md_rows(unsigned char *ui_buf, int ui_w, int ui_h, bool smooth, const char *title, const char *const *labels,
            const char *const *values, const char *const *hints, int n, int sel, int mx, int my)
{
    if (!ui_buf) return -1;   /* (the caller has started ui.c: ui_start) */
    ui_set_scale(3);
    Overlay *o = ui_overlay();
    ui_clear();
    const int ROW = 15, FOOT = 16 + FOOT_LINES * 10;   /* (27 until 2026-11-16: one line for the hint, now three) */
    const int s = o->scale;
    /* 300 wide (216 until 2026-11-14; never wider than the window); as many rows as fit (the rest scrolled: the
     * wheel, the scrollbar on the right, and the chosen row always in view) */
    /* the tabs' strip: each name's box as wide as its text, a second line begun when a line is full */
    int tab_w[MD_TABS], tab_line[MD_TABS], tab_lines = 0, tabh = 0;
    const int em_tab = 14 * s * 6 / 10 * 17 / 20;
    if (md_tab_n) {
        int room = (o->w - 8 < 300 ? o->w - 8 : 300) - 10, used = 0, all = 0, widest = 0;
        for (int i = 0; i < md_tab_n; i++) {
            char l[224];
            if (smooth) tab_w[i] = (st_width(ST_SEMIBOLD, em_tab, md_tab_name[i]) + s - 1) / s + 8;
            else menu_fit_orig(md_tab_name[i], 4096, l, sizeof l), tab_w[i] = ui_text_width(FONT_SMALL, l) + 8;
            all += tab_w[i] + 1, widest = tab_w[i] > widest ? tab_w[i] : widest;
        }
        if (all > room) {   /* more than a line: the lines made about as long as each other, not one full and one short */
            int even = all / ((all + room - 1) / room) + widest / 2;
            if (even < room) room = even;
        }
        tab_lines = 1;
        for (int i = 0; i < md_tab_n; i++) {
            if (used && used + tab_w[i] > room) tab_lines++, used = 0;
            tab_line[i] = tab_lines - 1, used += tab_w[i] + 1;
        }
        tabh = tab_lines * 13 + 3;
    }
    const int TOP = 22 + tabh;   /* the rows begin this far under the panel's top: the title, then the tabs */
    int vis = (o->h - 40 - 30 - FOOT - tabh) / ROW;
    if (vis < 3) vis = 3;
    const int panel_rows = md_tab_n || vis <= n ? vis : n;   /* (with tabs the panel keeps its full height: the strip stays where it is from tab to tab) */
    if (vis > n) vis = n;
    bool bar = n > vis;
    if (n != menu_last_n) menu_top = 0, menu_last_sel = -1, menu_last_n = n;   /* another page */
    if (sel != menu_last_sel) {        /* the chosen row (the keys) kept in view, a heading just above it too */
        if (sel >= 0 && sel < menu_top) menu_top = sel > 0 && labels[sel - 1][0] == '#' ? sel - 1 : sel;
        if (sel >= 0 && sel >= menu_top + vis) menu_top = sel - vis + 1;
        if (sel > 0 && sel == menu_top && labels[sel - 1][0] == '#') menu_top = sel - 1;   /* (its heading shown with it) */
        menu_last_sel = sel;
    }
    int w = 300;
    if (w > o->w - 8) w = o->w - 8;
    int h = TOP + panel_rows * ROW + 4 + FOOT, x = (o->w - w) / 2, y = (o->h - h) / 2, hit = -1;
    if (y < 0) y = 0;
    int px = mx / o->scale, py = my / o->scale;
    /* the scrollbar: its track right of the rows, the thumb its share of the list */
    int tx = x + w - 10, ty = y + TOP, th = vis * ROW - 1, tw = 5;
    if (bar && menu_drag) {            /* the thumb dragged: the list follows the pointer */
        int span = th - menu_thumb_h;
        menu_top = span > 0 ? (int)((float)(py - menu_drag_dy - ty) / (float)span * (float)(n - vis) + 0.5f) : 0;
    }
    if (menu_top > n - vis) menu_top = n - vis;
    if (menu_top < 0) menu_top = 0;
    menu_vis = vis, menu_rows_n = n;
    int thumb_h = bar ? th * vis / n : th, thumb_y = bar ? ty + (th - thumb_h) * menu_top / (n - vis) : ty;
    if (thumb_h < 8) thumb_h = 8;
    menu_track[0] = tx, menu_track[1] = ty, menu_track[2] = tw, menu_track[3] = th, menu_thumb_y = thumb_y, menu_thumb_h = thumb_h;
    menu_bar = bar;
    ui_fill(0, 0, o->w, o->h, 0, 0, 0, 140);
    if (md_solid) {
        ui_fill(x, y, w, h, 26, 16, 10, 255);
        ui_fill(x, y, w, 1, 120, 96, 40, 255), ui_fill(x, y + h - 1, w, 1, 120, 96, 40, 255);
        ui_fill(x, y, 1, h, 120, 96, 40, 255), ui_fill(x + w - 1, y, 1, h, 120, 96, 40, 255);
    } else ui_leather(x, y, w, h);
    for (int i = 0; i < md_tab_n; i++) {   /* the tabs: the open one lighter with a gold bar under it, the one under the pointer lit */
        int line_w = -1, cx = 0;
        for (int k = 0; k < md_tab_n; k++) if (tab_line[k] == tab_line[i]) { if (k < i) cx += tab_w[k] + 1; line_w += tab_w[k] + 1; }
        int bx = x + (w - line_w) / 2 + cx, by = y + 21 + tab_line[i] * 13, bw = tab_w[i], bh = 12;
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
    if (md_tab_n) ui_fill(x + 6, y + 21 + tab_lines * 13, w - 12, 1, 120, 96, 40, 160);   /* a rule under them */
    if (bar) {
        ui_fill(tx, ty, tw, th, 40, 28, 16, 200);                                           /* the track */
        bool on = menu_drag || (px >= tx - 2 && px < tx + tw + 2 && py >= thumb_y && py < thumb_y + thumb_h);
        ui_fill(tx, thumb_y, tw, thumb_h, on ? 255 : 210, on ? 220 : 170, on ? 60 : 40, 255); /* the thumb, gold */
    }
    char ttl[64];
    {
        int k = 0;
        for (; title[k] && k < 63; k++) ttl[k] = (char)(!smooth && title[k] >= 'a' && title[k] <= 'z' ? title[k] - 32 : title[k]);
        ttl[k] = 0;
        if (!smooth) ui_text(FONT_NORMAL, ttl, x + w / 2 - ui_text_width(FONT_NORMAL, ttl) / 2, y + 7, 232, 190, 40);
    }
    int rw = bar ? w - 8 - 10 : w - 8;   /* the rows' width: room for the scrollbar */
    /* what each row draws (every row, shown or not: the checks read them all): the value as drawn, the widths in
     * window pixels, the value's size (smooth) */
    static struct { char drawn[224]; int vw, room, lw, em; } M[160];   /* (160: uwfront.c's MENU_MAX; 96 until 2026-11-16, the sound's dials) */
    const int em = 14 * s * 6 / 10, em_foot = 22 * s / 3;
    for (int r = 0; r < n && r < 160; r++) {
        const char *lab = labels[r], *val = values && values[r] ? values[r] : "";
        bool head = lab[0] == '#';
        if (head) lab++;
        M[r].drawn[0] = 0, M[r].vw = 0, M[r].em = em;
        if (smooth) {
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
    for (int r = 0; r < 160; r++) md_sx0[r] = md_sx1[r] = 0;
    for (int r = menu_top; r < menu_top + vis && r < n && r < 160; r++) {
        int ry = y + TOP + (r - menu_top) * ROW;
        bool head = labels[r][0] == '#';
        if (head) {
            ui_fill(x + 6, ry + 13, rw - 4, 1, 120, 96, 40, 160);   /* a rule under the heading */
            if (!smooth) {
                char l[224];
                menu_fit_orig(labels[r] + 1, 4096, l, sizeof l);
                ui_text(FONT_SMALL, l, x + 7, ry + 5, 232, 190, 40);
            }
            continue;
        }
        if (!menu_drag && px >= x + 4 && px < x + 4 + rw && py >= ry && py < ry + 14) hit = r;
        ui_button(x + 4, ry, rw, r == sel || r == hit);
        if (!smooth) {
            char l[224];
            menu_fit_orig(labels[r], 4096, l, sizeof l);
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
                bool on = r == sel || r == hit;
                ui_fill(x0, ry + 5, x1 - x0, 4, 120, 96, 40, 255);                 /* the track: dark, a gold line round it */
                ui_fill(x0 + 1, ry + 6, x1 - x0 - 2, 2, 26, 16, 10, 255);
                ui_fill(x0 + 1, ry + 6, kx - x0, 2, 210, 170, 40, 255);            /* the part up to the value, gold */
                ui_fill(kx, ry + 2, 3, 10, on ? 255 : 232, on ? 220 : 190, on ? 60 : 40, 255);   /* the knob */
                md_sx0[r] = x0, md_sx1[r] = x1, md_sy[r] = ry;
            }
        }
    }
    /* the foot: the row chosen or under the pointer: its value in full, and what it does */
    int fr = hit >= 0 ? hit : sel, fy = y + TOP + panel_rows * ROW + 3;
    char foot1[224] = "", foot2[FOOT_LINES][224];
    int fe1 = em_foot, fe2 = em_foot * 10 / 11, nf2 = 0;
    ui_fill(x + 6, fy - 1, w - 12, 1, 120, 96, 40, 160);
    if (fr >= 0 && fr < n && labels[fr][0] != '#') {
        char t[224];
        const char *val = values && values[fr] ? values[fr] : "", *hint = hints && hints[fr] ? hints[fr] : "";
        if (*val) snprintf(t, sizeof t, "%s: %s", labels[fr], val);
        else snprintf(t, sizeof t, "%s", labels[fr]);
        if (smooth) {
            menu_fit(t, ST_REGULAR, em_foot, (w - 16) * s, foot1, sizeof foot1, &fe1);
            nf2 = menu_wrap(hint, true, fe2, (w - 16) * s, foot2, FOOT_LINES);
        } else {
            menu_fit_orig(t, w - 16, foot1, sizeof foot1), nf2 = menu_wrap(hint, false, 0, w - 16, foot2, FOOT_LINES);
            ui_text(FONT_SMALL, foot1, x + 8, fy + 4, 255, 220, 60);
            for (int i = 0; i < nf2; i++) ui_text(FONT_SMALL, foot2[i], x + 8, fy + 15 + i * 10, 207, 207, 223);
        }
    }
    unsigned *d = (unsigned *)ui_buf;
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
    if (smooth) {   /* the text, at the window's resolution over the art; each with a dark edge under it (the leather is lit) */
#define SMT(X, Y, FACE, EM, T, RGB) (st_draw(d, ui_w, ui_h, (X) + 2, (Y) + 2, FACE, EM, T, 0x000000), st_draw(d, ui_w, ui_h, X, Y, FACE, EM, T, RGB))
        int et = em * 5 / 4, hh = st_height(ST_REGULAR, em);
        SMT(x * s + w * s / 2 - st_width(ST_SEMIBOLD, et, ttl) / 2, y * s + (22 * s - st_height(ST_SEMIBOLD, et)) / 2, ST_SEMIBOLD, et, ttl, 0xE8BE28);
        for (int i = 0; i < md_tab_n; i++)   /* the tabs' names, each in the middle of its box */
            SMT(md_tab_box[i][0] * s + (md_tab_box[i][2] * s - st_width(ST_SEMIBOLD, em_tab, md_tab_name[i])) / 2,
                md_tab_box[i][1] * s + (10 * s - st_height(ST_SEMIBOLD, em_tab)) / 2, ST_SEMIBOLD, em_tab, md_tab_name[i],
                i == md_tab_open ? 0xFFDC3C : 0xD8D8E4);
        for (int r = menu_top; r < menu_top + vis && r < n && r < 160; r++) {
            int ry = (y + TOP + (r - menu_top) * ROW) * s, ty2 = ry + (14 * s - hh) / 2;
            if (labels[r][0] == '#') { SMT((x + 7) * s, ty2 - s, ST_SEMIBOLD, em, labels[r] + 1, 0xE8BE28); continue; }
            SMT((x + 9) * s, ty2, ST_SEMIBOLD, em, labels[r], 0xE4E4F0);
            if (M[r].drawn[0])
                SMT((x + 4 + rw - 6) * s - M[r].vw, ry + (14 * s - st_height(ST_REGULAR, M[r].em)) / 2, ST_REGULAR, M[r].em, M[r].drawn, 0xFFDC3C);
        }
        if (foot1[0]) SMT((x + 8) * s, (fy + 2) * s, ST_REGULAR, fe1, foot1, 0xFFDC3C);
        for (int i = 0; i < nf2; i++) SMT((x + 8) * s, (fy + 13 + i * 10) * s, ST_REGULAR, fe2, foot2[i], 0xD8D8E4);
#undef SMT
    }
    if (mx >= 0 && my >= 0) {   /* the game's cursor (the game is paused under the menu), over the text */
        ui_clear();
        ui_cursor(10, px, py);
        md_blend((unsigned *)ui_buf, ui_w, ui_h);
    }
    {   /* the checks: what was drawn, written when it changes */
        const char *dump = getenv("UWVIEW_MENU_DUMP");
        static unsigned last;
        if (dump && *dump) {
            unsigned hsh = 2166136261u ^ (unsigned)sel ^ (unsigned)menu_top << 8 ^ (unsigned)n << 16 ^ (unsigned)smooth << 30;
            hsh ^= (unsigned)md_tab_open << 24 ^ (unsigned)md_tab_n << 4;
            for (int r = 0; r < n && r < 160; r++) {
                for (const char *c = labels[r]; *c; c++) hsh = (hsh ^ (unsigned char)*c) * 16777619u;
                for (const char *c = values && values[r] ? values[r] : ""; *c; c++) hsh = (hsh ^ (unsigned char)*c) * 16777619u;
            }
            if (hsh != last) {
                FILE *f = fopen(dump, "w");
                last = hsh;
                if (md_tab_n && md_tab_open >= 0 && md_tab_open < md_tab_n) fprintf(stderr, "menu: tab %d of %d %s\n", md_tab_open, md_tab_n, md_tab_name[md_tab_open]);
                if (sel >= 0 && sel < n)
                    fprintf(stderr, "menu: sel %d head %d shown %d %s = %s\n", sel, labels[sel][0] == '#', sel >= menu_top && sel < menu_top + vis,
                            labels[sel], values && values[sel] ? values[sel] : "");
                if (f) {
                    fprintf(f, "panel\t%d\t%d\t%d\t%d\t%d\t%d\t%s\t%s\t%s\n", x * s, y * s, w * s, h * s, ui_w, ui_h,
                            smooth ? "smooth" : "original", smooth ? st_font_name() : "FONT_SMALL", title);
                    for (int i = 0; i < md_tab_n; i++)   /* a line a tab: its number, 1 the open one, its name, its box (window pixels) */
                        fprintf(f, "tab\t%d\t%d\t%s\t%d\t%d\t%d\t%d\n", i, i == md_tab_open, md_tab_name[i], md_tab_box[i][0] * s,
                                md_tab_box[i][1] * s, md_tab_box[i][2] * s, md_tab_box[i][3] * s);
                    for (int r = 0; r < n && r < 160; r++)
                        fprintf(f, "row\t%d\t%d\t%d\t%s\t%s\t%s\t%d\t%d\t%d\n", r, labels[r][0] == '#', r >= menu_top && r < menu_top + vis,
                                labels[r] + (labels[r][0] == '#'), values && values[r] ? values[r] : "", M[r].drawn, M[r].vw, M[r].room, M[r].lw);
                    fprintf(f, "sel\t%d\nfoot1\t%s\nfoot2\t", sel, foot1);
                    for (int i = 0; i < nf2; i++) fprintf(f, "%s%s", i ? " | " : "", foot2[i]);
                    fprintf(f, "\n");
                    fclose(f);
                }
            }
        }
    }
    ui_set_scale(0);   /* the window's own scale again for the HUD and the panels */
    return hit;
}
static void md_blend(unsigned *d, int ui_w, int ui_h)
{
    Overlay *o = ui_overlay();
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
            unsigned r = (p[0] * a + (c & 0xFF) * na) / 255, g = (p[1] * a + (c >> 8 & 0xFF) * na) / 255, b = (p[2] * a + (c >> 16 & 0xFF) * na) / 255;
            d[(size_t)yy * ui_w + xx] = 0xFF000000u | r | g << 8 | b << 16;
        }
    }
    ui_text_apply(d, ui_w, ui_h);   /* (the interface font's smooth text over the overlay: ui.c) */
}
