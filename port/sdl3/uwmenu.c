/* uwmenu.c: our Esc menu where our renderer (uwview.dll) is not up: the Linux build, or Windows without it. The same
   drawing as the renderer's (src\menu_draw.c over src\ui.c: the leather, the buttons, the game's own letters), into a
   layer of the window's pixels (0xAABBGGRR, transparent where the menu is not), which plat_sdl3.c lays over the picture.
   uwfront.c's menu logic drives it through UwfMenuFns (uwf_soft_menu). The interface's pictures and the game's fonts
   come from data\ui as the renderer's (data\ui\fonts: copied from the game's data when missing). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "menu/base.h"        /* (src's own, copied into menu beside this file by the build: apart from the game's base.h) */
#include "menu/ui.h"
#include "menu/menu_draw.h"
#include "plat.h"
#include "uwfront.h"
#include "uwmenu.h"

static unsigned char *layer;
static int lw, lh, started, fresh;

/* ui.c started at the window's size (again when that changed: the arena's earlier start given back first) */
static int menu_start(int w, int h)
{
    char data[1024], game[1024];
    const char *base = plat_base_dir();
    static int arena;
    if (!arena) arena_init((size_t)256 << 20), arena = 1;
    else arena_reset(0);
    (void)base;
    uwf_our_data(data, sizeof data);   /* data\, or uwr\ in a flat folder (uwfront.c) */
    file_memory_root(data);            /* the game's fonts ui_start copies are kept in memory, not written */
    snprintf(game, sizeof game, "%s", plat_data_root() ? plat_data_root() : "");
    ui_game_dir(game);
    started = 0;
    if (!ui_start(data, w, h)) return 0;
    free(layer);
    if (!(layer = malloc((size_t)w * (size_t)h * 4))) return 0;
    lw = w, lh = h, started = 1;
    fprintf(stderr, PLAT_NAME ": our menu without the renderer: %dx%d, the game's letters (%s)\n", w, h, data);
    return 1;
}

static int menu_rows(const char *title, const char *const *labels, const char *const *values, const char *const *hints,
                     int n, int sel, int mx, int my)
{
    int w = 0, h = 0;
    uwp_window_size(&w, &h);
    if (w < 64 || h < 64) return -1;
    if ((!started || w != lw || h != lh) && !menu_start(w, h)) return -1;
    memset(layer, 0, (size_t)lw * (size_t)lh * 4);
    fresh = 1;
    return md_rows(layer, lw, lh, false, title, labels, values, hints, n, sel, mx, my);
}
static void menu_wheel(int notches) { md_wheel(notches); }
static int menu_click(int mx, int my, int down) { return md_click(started, mx, my, down); }

static void menu_tabs(const char *const *names, int n, int open) { md_tabs(names, n, open); }
static int menu_tab_at(int mx, int my) { return md_tab_at(mx, my); }

static const UwfMenuFns fns = { menu_rows, menu_wheel, menu_click, menu_tabs, menu_tab_at };
void uwmenu_register(void) { uwf_soft_menu(&fns); }

const unsigned char *uwmenu_layer(int *w, int *h)
{
    if (!fresh || !layer) return NULL;
    fresh = 0;
    *w = lw, *h = lh;
    return layer;
}
