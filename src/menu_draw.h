/* menu_draw.h: our Esc menu's drawing (menu_draw.c), for uwview.c and the backends without our renderer */
#pragma once
#include <stdbool.h>
/* the rows drawn over the layer (ui_w x ui_h, 0xAABBGGRR) with ui.c's overlay (ui_start'ed by the caller); smooth: the
 * smooth font (smoothtext.c) else the game's letters; mx, my the pointer in layer pixels; the row under it, or -1 */
int md_rows(unsigned char *ui_buf, int ui_w, int ui_h, bool smooth, const char *title, const char *const *labels,
            const char *const *values, const char *const *hints, int n, int sel, int mx, int my);
void md_set_solid(int on);                         /* the panel's ground: 1 one dark colour (the default), 0 the leather */
void md_set_fast(int on);                          /* Quick menu drawing: 1 the finished picture kept while unchanged (default) */
int md_get_fast(void);
/* the GPU's panel for the menu (uwview.c: render_set_panel; none: the menu drawn into the layer) */
void md_set_gpu(bool (*fn)(const unsigned char *, int, int, int, const unsigned *, int, int, int, int, int));
void md_set_gpu_keep(bool (*keep)(void), unsigned (*gen)(void));   /* (render_keep_panel, render_panel_gen) */
void md_wheel(int notches);                        /* the list scrolled: up positive */
int md_click(int ui_ok, int mx, int my, int down); /* a left press / release on the scrollbar: 1 taken */
/* the rows' sliders (the Fog and smoke page): each row's place along its track (0..1; below 0 none) for the next
 * md_rows, n 0 none; and the pointer on them: row < 0 the row whose track is under mx, my (or -1), row >= 0 that row
 * (or -1, not shown), *frac the place along the track at mx */
void md_sliders(const float *frac, int n);
int md_slider(int mx, int my, int row, float *frac);
/* the tabs' strip under the title: their names and the open one for the next md_rows (n 0: none); the tab whose name
 * is under the pointer, or -1 */
void md_tabs(const char *const *names, int n, int open);
int md_tab_at(int mx, int my);
