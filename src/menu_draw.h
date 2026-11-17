/* menu_draw.h: our Esc menu's drawing (menu_draw.c), for uwview.c and the backends without our renderer */
#pragma once
#include <stdbool.h>
/* the rows drawn over the layer (ui_w x ui_h, 0xAABBGGRR) with ui.c's overlay (ui_start'ed by the caller); smooth: the
 * smooth font (smoothtext.c) else the game's letters; mx, my the pointer in layer pixels; the row under it, or -1 */
int md_rows(unsigned char *ui_buf, int ui_w, int ui_h, bool smooth, const char *title, const char *const *labels,
            const char *const *values, const char *const *hints, int n, int sel, int mx, int my);
void md_set_solid(int on);                         /* the panel's ground: 1 one dark colour (the default), 0 the leather */
void md_wheel(int notches);                        /* the list scrolled: up positive */
int md_click(int ui_ok, int mx, int my, int down); /* a left press / release on the scrollbar: 1 taken */
