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
static void md_blend(unsigned *d, int ui_w, int ui_h);

/* uw.exe's options menu drawn (its look: the game dimmed, the leather, a button a row, the values in gold on the
 * right) over the interface layer; returns the row under the pointer, or -1.
 * 2026-11-14 (the user: the font was hard to read and the language model's name was cut): the text is a smooth font
 * at the window's own resolution, in the letters' own case (smoothtext.c; settings.txt menu_font original brings the
 * game's small capitals back); a label beginning with '#' is a heading (gold, no button, never chosen); a value too
 * long for its row is drawn smaller, then with its middle left out; under the rows two lines for the row chosen or
 * under the pointer: its value in full and what the row does (hints[r], NULL or "" none).
 * UWVIEW_MENU_DUMP=FILE (the checks): what was drawn, a line a row. */
int md_rows(unsigned char *ui_buf, int ui_w, int ui_h, bool smooth, const char *title, const char *const *labels,
            const char *const *values, const char *const *hints, int n, int sel, int mx, int my)
{
    if (!ui_buf) return -1;   /* (the caller has started ui.c: ui_start) */
    ui_set_scale(3);
    Overlay *o = ui_overlay();
    ui_clear();
    const int ROW = 15, FOOT = 27;
    const int s = o->scale;
    /* 300 wide (216 until 2026-11-14; never wider than the window); as many rows as fit (the rest scrolled: the
     * wheel, the scrollbar on the right, and the chosen row always in view) */
    int vis = (o->h - 40 - 30 - FOOT) / ROW;
    if (vis < 3) vis = 3;
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
    int h = 22 + vis * ROW + 4 + FOOT, x = (o->w - w) / 2, y = (o->h - h) / 2, hit = -1;
    if (y < 0) y = 0;
    int px = mx / o->scale, py = my / o->scale;
    /* the scrollbar: its track right of the rows, the thumb its share of the list */
    int tx = x + w - 10, ty = y + 22, th = vis * ROW - 1, tw = 5;
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
    ui_leather(x, y, w, h);
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
    static struct { char drawn[224]; int vw, room, lw, em; } M[96];
    const int em = 14 * s * 6 / 10, em_foot = 22 * s / 3;
    for (int r = 0; r < n && r < 96; r++) {
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
    for (int r = menu_top; r < menu_top + vis && r < n && r < 96; r++) {
        int ry = y + 22 + (r - menu_top) * ROW;
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
    }
    /* the foot: the row chosen or under the pointer: its value in full, and what it does */
    int fr = hit >= 0 ? hit : sel, fy = y + 22 + vis * ROW + 3;
    char foot1[224] = "", foot2[224] = "";
    int fe1 = em_foot, fe2 = em_foot;
    ui_fill(x + 6, fy - 1, w - 12, 1, 120, 96, 40, 160);
    if (fr >= 0 && fr < n && labels[fr][0] != '#') {
        char t[224];
        const char *val = values && values[fr] ? values[fr] : "", *hint = hints && hints[fr] ? hints[fr] : "";
        if (*val) snprintf(t, sizeof t, "%s: %s", labels[fr], val);
        else snprintf(t, sizeof t, "%s", labels[fr]);
        if (smooth) {
            menu_fit(t, ST_REGULAR, em_foot, (w - 16) * s, foot1, sizeof foot1, &fe1);
            menu_fit(hint, ST_REGULAR, em_foot, (w - 16) * s, foot2, sizeof foot2, &fe2);
        } else {
            menu_fit_orig(t, w - 16, foot1, sizeof foot1), menu_fit_orig(hint, w - 16, foot2, sizeof foot2);
            ui_text(FONT_SMALL, foot1, x + 8, fy + 4, 255, 220, 60);
            ui_text(FONT_SMALL, foot2, x + 8, fy + 15, 207, 207, 223);
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
        for (int r = menu_top; r < menu_top + vis && r < n && r < 96; r++) {
            int ry = (y + 22 + (r - menu_top) * ROW) * s, ty2 = ry + (14 * s - hh) / 2;
            if (labels[r][0] == '#') { SMT((x + 7) * s, ty2 - s, ST_SEMIBOLD, em, labels[r] + 1, 0xE8BE28); continue; }
            SMT((x + 9) * s, ty2, ST_SEMIBOLD, em, labels[r], 0xE4E4F0);
            if (M[r].drawn[0])
                SMT((x + 4 + rw - 6) * s - M[r].vw, ry + (14 * s - st_height(ST_REGULAR, M[r].em)) / 2, ST_REGULAR, M[r].em, M[r].drawn, 0xFFDC3C);
        }
        if (foot1[0]) SMT((x + 8) * s, (fy + 2) * s, ST_REGULAR, fe1, foot1, 0xFFDC3C);
        if (foot2[0]) SMT((x + 8) * s, (fy + 13) * s, ST_REGULAR, fe2, foot2, 0xD8D8E4);
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
            for (int r = 0; r < n && r < 96; r++) {
                for (const char *c = labels[r]; *c; c++) hsh = (hsh ^ (unsigned char)*c) * 16777619u;
                for (const char *c = values && values[r] ? values[r] : ""; *c; c++) hsh = (hsh ^ (unsigned char)*c) * 16777619u;
            }
            if (hsh != last) {
                FILE *f = fopen(dump, "w");
                last = hsh;
                if (sel >= 0 && sel < n)
                    fprintf(stderr, "menu: sel %d head %d shown %d %s = %s\n", sel, labels[sel][0] == '#', sel >= menu_top && sel < menu_top + vis,
                            labels[sel], values && values[sel] ? values[sel] : "");
                if (f) {
                    fprintf(f, "panel\t%d\t%d\t%d\t%d\t%d\t%d\t%s\t%s\t%s\n", x * s, y * s, w * s, h * s, ui_w, ui_h,
                            smooth ? "smooth" : "original", smooth ? st_font_name() : "FONT_SMALL", title);
                    for (int r = 0; r < n && r < 96; r++)
                        fprintf(f, "row\t%d\t%d\t%d\t%s\t%s\t%s\t%d\t%d\t%d\n", r, labels[r][0] == '#', r >= menu_top && r < menu_top + vis,
                                labels[r] + (labels[r][0] == '#'), values && values[r] ? values[r] : "", M[r].drawn, M[r].vw, M[r].room, M[r].lw);
                    fprintf(f, "sel\t%d\nfoot1\t%s\nfoot2\t%s\n", sel, foot1, foot2);
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
