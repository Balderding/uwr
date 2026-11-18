/* uwfront.h: the seam between our front end (uwfront.c: the Esc menu, the inventory panel, the clicks on things, the
   window's input through uwinput.c, mouse-look's want and capture, the hotkeys, our renderer's function table and the
   picture it is given) and a platform backend (plat_win32.c now; an SDL one later). uwfront.c holds no window-system
   code: everything it needs of the window comes through the services below (uwp_*), and everything the window gets
   (keys, the pointer, the wheel, the focus, a resize, each pass of the loop) comes in through the events (uwf_*).

   Threads: every uwf_* event and every uwp_* service is called on the window's thread (the backend's loop), but
   uwp_xchg / uwp_inc (the flags the game's thread sets: plat_screen, plat_death_fade_start, plat_pointer_capture)
   and uwp_ms, which both threads use. */
#ifndef UWFRONT_H
#define UWFRONT_H
#include <stdint.h>
#include "plat.h"
#include "ui/settings.h"

/* ---- the state both sides keep ---- */

typedef struct { long left, top, right, bottom; } UwfRect;   /* (Win32's RECT: LONG fields) */
extern struct loop {
    uint8_t pix[640 * 480];
    uint32_t rgb[640 * 480];            /* the picture, or with the settings screen the 640x400 composite */
    uint32_t layer[SET_W * SET_H];
    uint8_t pal[768];
    uint32_t lut[256];
    int w, hgt, quit, shot;
    unsigned swallow;
    int hidden_win;
    uint64_t start;
    UwfRect dst;                        /* the picture's place in the client area */
    int have_dst;
    unsigned presents;
} L;
extern const PlatHooks *hooks;
extern PlatConfig live;                /* the options as they stand: plat_set_display and plat_set_mouse_lock change it */

/* our own files' folder (settings.txt, our maps): <program's folder>data, or <program's folder>uwr in a flat folder
   (the program beside the game's own UW.EXE, no game\ folder: the user, 2026-11-16) */
void uwf_our_data(char *out, size_t n);

/* ---- events: the backend's window thread calls these ---- */

void uwf_key(int c, int down, int repeat);   /* a key (set-1 code, 0x100 for E0; down 1, a repeat with repeat 1) */
void uwf_key_byte(uint8_t b);                /* one byte straight to the game (Print Screen's and Pause's sequences) */
void uwf_pointer(int type, int cx, int cy, unsigned button);   /* PLAT_POINTER_*: client pixels, the button changed */
void uwf_raw_motion(long dx, long dy);       /* a captured pointer's motion, the mouse's counts */
int uwf_hover(int x, int y);                 /* after a move: -1 the host's cursor left as it is, else 1 hidden (the
                                                game's cursor stands in for it there), 0 shown */
int uwf_pointer_free(void);                  /* the pointer neither locked nor captured (the pointer left the window) */
void uwf_wheel(int delta);                   /* the wheel: 120 a notch, up positive */
void uwf_focus(int on);                      /* the keyboard's focus gained (1) or lost (0) */
void uwf_set_focused(int on);                /* the focus as the window opens (no event made) */
void uwf_resized(void);                      /* the client area changed (not minimised) */
void uwf_mouse_unlock(void);                 /* Ctrl+F10 with the pointer lock option */
void uwf_mouse_title(int is_locked);         /* the window's title for the pointer lock option */
/* each pass of the loop: before the window's messages; after them (the runtime's tick, the input's step); then
   (after the backend's own folder picker answer) the game's picture taken and the screenshots */
void uwf_tick_begin(void);
void uwf_tick_tests(void);                   /* (after uwf_tick_begin) the checks' UWPORT_FOCUSTEST, UWPORT_MOUSETEST */
void uwf_tick_input(void);
void uwf_tick_scan(void);
/* the picture to show: NULL when there is none or our renderer presented it; else its pixels (0x00RRGGBB, *sw by
   *sh) for the backend to put at L.dst */
const uint32_t *uwf_frame(int *sw, int *sh);
void uwf_client_size(int *w, int *h);        /* the renderer's pixels while it runs, else the client area's */
int uwf_offscreen(void);                     /* UWPORT_OFFSCREEN: never shown, never full screen, the mouse untouched */
int uwf_no_input(void);                      /* UWPORT_NO_INPUT: the window's own keys and pointer reach nothing */
void uwf_view_warm(void);                    /* our renderer started now: before the game's thread, so nothing is heard before a picture */
void uwf_view_start(void);                   /* our renderer loaded (uwview.dll), when it is there */
int uwf_view_loaded(void), uwf_view_ok(void);   /* loaded; started */
void uwf_fx_audio_start(void);               /* our positional audio (uwaudio.c) */
void uwf_input_init(void);                   /* the window's input (uwinput.c) */
void uwf_shutdown(void);                     /* the loop ended: the model, the capture, the renderer, the audio let go */
/* our Esc menu drawn by the backend when our renderer is not up (plat_sdl3.c's uwmenu.c over src\menu_draw.c; the
   Linux build): rows draws the rows (as uwview.dll's uwv_menu_rows2) over the picture uwf_frame gives, the pointer in
   window pixels, and returns the row under it or -1; wheel scrolls the list; click a left press (1) or release (0) on
   its scrollbar, 1 taken. Registered once (uwf_soft_menu), before the loop; without it there is no menu then */
typedef struct {
    int (*rows)(const char *title, const char *const *labels, const char *const *values, const char *const *hints,
                int n, int sel, int mx, int my);
    void (*wheel)(int notches);
    int (*click)(int mx, int my, int down);
    /* the menu's tabs (2026-11-17; NULL: a backend without them, the tabs then by the keys alone): their names and the
       open one before rows, and the tab under the pointer or -1 (menu_draw.c's md_tabs, md_tab_at) */
    void (*tabs)(const char *const *names, int n, int open);
    int (*tab_at)(int mx, int my);
} UwfMenuFns;
void uwf_soft_menu(const UwfMenuFns *f);

/* ---- services: the backend provides these ---- */

uint64_t uwp_ms(void);                       /* a monotonic clock, milliseconds */
void uwp_sleep_ms(unsigned ms);
void uwp_music_room(int ms, int high);       /* the music stream: ms held ready before the device plays; its thread above the game's (-1: as it stands) */
long uwp_xchg(volatile long *p, long v);     /* an atomic exchange (a full barrier): the old value */
long uwp_inc(volatile long *p);              /* an atomic increment: the new value */
void *uwp_window(void);                      /* the window (its native handle, for the renderer), NULL none */
int uwp_window_size(int *w, int *h);         /* the client area's own pixels: 1, or 0 with none */
void uwp_resize_client(int w, int h);        /* the client area made this size (the window not moved) */
void uwp_title(const char *t);               /* the window's title */
void uwp_capture(int on);                    /* the pointer held in the window and the host's cursor hidden, or freed */
int uwp_cursor_hidden(void);                 /* the host's cursor hidden by us */
void uwp_cursor_hide(int hide);              /* the host's cursor hidden (1) or shown (0), if not so already */
int uwp_pointer_pos(int *x, int *y);         /* the pointer's place in client pixels: 1, or 0 not known */
/* the pointer held in the client rectangle x0,y0-x1,y1 (held again only if it moved on the screen, or first); first:
   put in its middle too, *mx, *my that point in client pixels, and 1 */
int uwp_clip_hold(int x0, int y0, int x1, int y1, int first, int *mx, int *my);
void uwp_clip_free(void);                    /* the pointer's hold let go (the cursor's own state left alone) */
int uwp_ctrl_down(void);                     /* the live keyboard's Ctrl held */
void uwp_close(void);                        /* the window's close asked for (as its close button: a quit request) */
void uwp_new_game(void);                     /* the program started again with --menu, and this one closed */
void uwp_repaint(void);                      /* the window drawn again (the renderer given up) */
void uwp_test_focus(int away);               /* UWPORT_FOCUSTEST: the focus lost (1) or back (0), as the window gets it */
void *uwp_lib_open(const char *path);        /* a library loaded, NULL none */
void *uwp_lib_sym(void *lib, const char *name);
void uwp_lib_close(void *lib);
#endif
