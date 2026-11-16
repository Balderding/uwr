/* smoothtext_none.c: smoothtext.h where there is no GDI (the Linux build's menu: menu_draw.c and ui.c over the SDL3
 * backend): no smooth face, so ui.c and the menu keep the game's own letters (ui_set_font is never given a face there,
 * and md_rows is asked for the game's letters). Every call answers "nothing drawn". */
#include "smoothtext.h"

int st_width(int face, int em, const char *s) { (void)face, (void)em, (void)s; return 0; }
int st_draw(unsigned *rgba, int w, int h, int x, int y, int face, int em, const char *s, unsigned rgb)
{
    (void)rgba, (void)w, (void)h, (void)y, (void)face, (void)em, (void)s, (void)rgb;
    return x;
}
int st_height(int face, int em) { (void)face; return em; }
const char *st_font_name(void) { return "none"; }
int st_game_em(int window_h) { int e = window_h * 19 / 1080; return e < 12 ? 12 : e; }
int st_face_count(void) { return 0; }
const char *st_face_label(int i) { (void)i; return ""; }
void st_set_face(int i) { (void)i; }
int st_face_find(const char *name) { (void)name; return -1; }
void st_load_font_dir(const char *dir) { (void)dir; }
void st_set_size_pct(int pct) { (void)pct; }
int st_size_pct(void) { return 100; }
