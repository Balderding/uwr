/* plat_sdl3.c: the SDL3 backend of the platform API (plat.h): window, keyboard and mouse, the folder picker, the
   message boxes, the sound stream and the game thread. It does for Linux what the Win32 backend (port\win32\plat_win32.c)
   does for Windows: SDL's events become the uwf_* events of our front end (port\win32\uwfront.c, through uwfront.h),
   and the uwp_* services the front end asks for are answered here.
   The picture: when our renderer (uwview.dll, or libuwview.so on Linux) is up it presents on this window itself and
   SDL never draws on it; otherwise SDL's renderer shows the picture uwf_frame gives.
   The game runs on its own thread (plat_run); this file's event loop runs on the main thread.
   Log lines go through rlog under the tag "sdl3". */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif
#include "plat.h"
#include "ui/settings.h"
#include "uwfront.h"
#include "uwmenu.h"
#include "rlog.h"
#if defined(PLAT_ICON) && !defined(__APPLE__)
#include PLAT_ICON
#endif
#include "uwos.h"   /* windows.h on Windows (uwp_new_game's command line); everywhere the shared files' clock, GetTickCount64 */

void port_crash_handlers(void);         /* sys/crash.c */

static SDL_Window *g_win;
static SDL_Renderer *ren;              /* SDL's renderer: made only when the picture is ours to show (no uwview.dll) */
static SDL_Texture *tex;
static int tw, th;
static float dens = 1;                 /* the window's pixels a point */
static SDL_AtomicInt game_done;
static int game_status;
static int focus_test_msg;             /* UWPORT_FOCUSTEST's own focus event being handled (its log tag) */
static int cursor_hidden;              /* the host's cursor hidden by us */
/* the shared files' clock (uwlog.c's messages, uwgame.c's frames), not SDL's ticks: one clock for every time that is
   compared (SDL_GetTicks counts from SDL's start, GetTickCount64 from the boot, and mixing them gave ages that underflowed) */
static uint64_t ms_now(void) { return GetTickCount64(); }

/* Logs a failed SDL call (ok == 0) with SDL's error text and returns ok, so a call whose result is otherwise ignored
   can be wrapped without changing the flow. Each label (the `what` text) logs its first 3 failures only:
   some of these run every frame or every mouse move. Used from the main and the game thread. */
static int sdl_ok(int ok, const char *what)
{
    static const char *seen[64];
    static int count[64], nseen, full;
    static SDL_SpinLock lock;
    int i, n = -1;
    if (ok) return 1;
    SDL_LockSpinlock(&lock);
    for (i = 0; i < nseen && seen[i] != what; i++) {}
    if (i == nseen && nseen < 64) seen[nseen++] = what, count[i] = 0;
    if (i < nseen) n = ++count[i];
    else if (!full) full = 1, n = 0;
    SDL_UnlockSpinlock(&lock);
    if (n == 0) RLOG_W("sdl3", "too many different failing SDL calls: further ones are not logged");
    if (n >= 1 && n <= 3)
        RLOG_W("sdl3", "%s failed: %s%s", what, SDL_GetError(), n == 3 ? " (not logged again)" : "");
    return 0;
}

/* SDL scan codes (USB HID usages) to PC set-1 make codes. 0 means no key; a value with bit 8
   set is sent after an E0 prefix. Print Screen and Pause have sequences of their own (special_key()). */
#define E0(c) (0x100 | (c))
static const uint16_t set1[SDL_SCANCODE_COUNT] = {
    [SDL_SCANCODE_ESCAPE] = 0x01,
    [SDL_SCANCODE_1] = 0x02, [SDL_SCANCODE_2] = 0x03, [SDL_SCANCODE_3] = 0x04, [SDL_SCANCODE_4] = 0x05,
    [SDL_SCANCODE_5] = 0x06, [SDL_SCANCODE_6] = 0x07, [SDL_SCANCODE_7] = 0x08, [SDL_SCANCODE_8] = 0x09,
    [SDL_SCANCODE_9] = 0x0A, [SDL_SCANCODE_0] = 0x0B, [SDL_SCANCODE_MINUS] = 0x0C,
    [SDL_SCANCODE_EQUALS] = 0x0D, [SDL_SCANCODE_BACKSPACE] = 0x0E, [SDL_SCANCODE_TAB] = 0x0F,
    [SDL_SCANCODE_Q] = 0x10, [SDL_SCANCODE_W] = 0x11, [SDL_SCANCODE_E] = 0x12, [SDL_SCANCODE_R] = 0x13,
    [SDL_SCANCODE_T] = 0x14, [SDL_SCANCODE_Y] = 0x15, [SDL_SCANCODE_U] = 0x16, [SDL_SCANCODE_I] = 0x17,
    [SDL_SCANCODE_O] = 0x18, [SDL_SCANCODE_P] = 0x19, [SDL_SCANCODE_LEFTBRACKET] = 0x1A,
    [SDL_SCANCODE_RIGHTBRACKET] = 0x1B, [SDL_SCANCODE_RETURN] = 0x1C, [SDL_SCANCODE_LCTRL] = 0x1D,
    [SDL_SCANCODE_A] = 0x1E, [SDL_SCANCODE_S] = 0x1F, [SDL_SCANCODE_D] = 0x20, [SDL_SCANCODE_F] = 0x21,
    [SDL_SCANCODE_G] = 0x22, [SDL_SCANCODE_H] = 0x23, [SDL_SCANCODE_J] = 0x24, [SDL_SCANCODE_K] = 0x25,
    [SDL_SCANCODE_L] = 0x26, [SDL_SCANCODE_SEMICOLON] = 0x27, [SDL_SCANCODE_APOSTROPHE] = 0x28,
    [SDL_SCANCODE_GRAVE] = 0x29, [SDL_SCANCODE_LSHIFT] = 0x2A, [SDL_SCANCODE_BACKSLASH] = 0x2B,
    [SDL_SCANCODE_NONUSHASH] = 0x2B,
    [SDL_SCANCODE_Z] = 0x2C, [SDL_SCANCODE_X] = 0x2D, [SDL_SCANCODE_C] = 0x2E, [SDL_SCANCODE_V] = 0x2F,
    [SDL_SCANCODE_B] = 0x30, [SDL_SCANCODE_N] = 0x31, [SDL_SCANCODE_M] = 0x32, [SDL_SCANCODE_COMMA] = 0x33,
    [SDL_SCANCODE_PERIOD] = 0x34, [SDL_SCANCODE_SLASH] = 0x35, [SDL_SCANCODE_RSHIFT] = 0x36,
    [SDL_SCANCODE_KP_MULTIPLY] = 0x37, [SDL_SCANCODE_LALT] = 0x38, [SDL_SCANCODE_SPACE] = 0x39,
    [SDL_SCANCODE_CAPSLOCK] = 0x3A,
    [SDL_SCANCODE_F1] = 0x3B, [SDL_SCANCODE_F2] = 0x3C, [SDL_SCANCODE_F3] = 0x3D, [SDL_SCANCODE_F4] = 0x3E,
    [SDL_SCANCODE_F5] = 0x3F, [SDL_SCANCODE_F6] = 0x40, [SDL_SCANCODE_F7] = 0x41, [SDL_SCANCODE_F8] = 0x42,
    [SDL_SCANCODE_F9] = 0x43, [SDL_SCANCODE_F10] = 0x44, [SDL_SCANCODE_NUMLOCKCLEAR] = 0x45,
    [SDL_SCANCODE_SCROLLLOCK] = 0x46,
    [SDL_SCANCODE_KP_7] = 0x47, [SDL_SCANCODE_KP_8] = 0x48, [SDL_SCANCODE_KP_9] = 0x49,
    [SDL_SCANCODE_KP_MINUS] = 0x4A, [SDL_SCANCODE_KP_4] = 0x4B, [SDL_SCANCODE_KP_5] = 0x4C,
    [SDL_SCANCODE_KP_6] = 0x4D, [SDL_SCANCODE_KP_PLUS] = 0x4E, [SDL_SCANCODE_KP_1] = 0x4F,
    [SDL_SCANCODE_KP_2] = 0x50, [SDL_SCANCODE_KP_3] = 0x51, [SDL_SCANCODE_KP_0] = 0x52,
    [SDL_SCANCODE_KP_PERIOD] = 0x53, [SDL_SCANCODE_NONUSBACKSLASH] = 0x56,
    [SDL_SCANCODE_F11] = 0x57, [SDL_SCANCODE_F12] = 0x58,
    [SDL_SCANCODE_KP_ENTER] = E0(0x1C), [SDL_SCANCODE_RCTRL] = E0(0x1D),
    [SDL_SCANCODE_KP_DIVIDE] = E0(0x35), [SDL_SCANCODE_RALT] = E0(0x38),
    [SDL_SCANCODE_HOME] = E0(0x47), [SDL_SCANCODE_UP] = E0(0x48), [SDL_SCANCODE_PAGEUP] = E0(0x49),
    [SDL_SCANCODE_LEFT] = E0(0x4B), [SDL_SCANCODE_RIGHT] = E0(0x4D), [SDL_SCANCODE_END] = E0(0x4F),
    [SDL_SCANCODE_DOWN] = E0(0x50), [SDL_SCANCODE_PAGEDOWN] = E0(0x51), [SDL_SCANCODE_INSERT] = E0(0x52),
    [SDL_SCANCODE_DELETE] = E0(0x53), [SDL_SCANCODE_LGUI] = E0(0x5B), [SDL_SCANCODE_RGUI] = E0(0x5C),
    [SDL_SCANCODE_APPLICATION] = E0(0x5D),
};

/* Print Screen and Pause send fixed byte sequences, not one code. 1 if the key was one of them (and handled). */
static int special_key(SDL_Scancode sc, int down)
{
    int i;
    if (!hooks->key) return 1;
    if (sc == SDL_SCANCODE_PRINTSCREEN) {
        static const uint8_t make[] = { 0xE0, 0x2A, 0xE0, 0x37 }, brk[] = { 0xE0, 0xB7, 0xE0, 0xAA };
        const uint8_t *s = down ? make : brk;
        for (i = 0; i < 4; i++) uwf_key_byte(s[i]);
        return 1;
    }
    if (sc == SDL_SCANCODE_PAUSE) {
        static const uint8_t pause[] = { 0xE1, 0x1D, 0x45, 0xE1, 0x9D, 0xC5 };
        if (down) for (i = 0; i < 6; i++) uwf_key_byte(pause[i]);
        return 1;
    }
    return 0;
}

/* a key of the window: the pointer lock's Ctrl+F10, the special keys, else its set-1 code to our front end */
static void key_event(const SDL_KeyboardEvent *k)
{
    int down = k->down, c;
    if (uwf_no_input()) return;
    if (live.mouse_lock && k->scancode == SDL_SCANCODE_F10 && (k->mod & SDL_KMOD_CTRL)) {
        if (down) uwf_mouse_unlock();
        return;
    }
    if (!settings_open() && special_key(k->scancode, down)) return;
    if ((unsigned)k->scancode >= SDL_SCANCODE_COUNT || !(c = set1[k->scancode])) return;
    uwf_key(c, down, down && k->repeat);
}

/* the game's thread body (plat_run starts it): runs the game to its end and tells the main loop it is done */
static int game_thread(void *p)
{
    void **a = p;
    int (*game)(void *) = (int (*)(void *))a[0];
    int st;
    /* the fault handlers on the thread the game runs on, as well as main's: the Windows C runtime
       keeps SIGSEGV's and SIGILL's per thread (crash.c) */
    port_crash_handlers();
    st = game(a[1]);
    game_status = st;
    SDL_SetAtomicInt(&game_done, 1);
    return st;
}

/* the game is over: keep its status for plat_run's return, tell the loop, and park this thread for good */
void plat_game_exit(int status)
{
    game_status = status;
    SDL_SetAtomicInt(&game_done, 1);
    for (;;) SDL_Delay(1000);
}

/* park the game's thread without ending the program */
void plat_game_park(void)
{
    for (;;) SDL_Delay(1000);
}

uint64_t plat_counter(void) { return SDL_GetPerformanceCounter(); }
uint64_t plat_counter_hz(void) { return SDL_GetPerformanceFrequency(); }
void plat_sleep_ns(uint64_t ns) { SDL_DelayPrecise(ns); }

/* a detached helper thread; 0 on success, -1 if SDL could not make it */
int plat_thread_start(const char *name, int (*fn)(void *), void *arg)
{
    SDL_Thread *t = SDL_CreateThread(fn, name, arg);
    if (!t) {
        RLOG_E("sdl3", "cannot start thread %s: %s", name ? name : "?", SDL_GetError());
        return -1;
    }
    SDL_DetachThread(t);
    return 0;
}

/* The settings screen's folder picker: the dialog's answer waits here for the loop to hand it on. */
static SDL_AtomicInt pick_state;       /* 0 none, 1 a folder in pick_path, 2 cancelled */
static char pick_path[1024];

/* the folder dialog's answer (SDL calls it from the dialog's thread): the chosen folder, or cancelled */
static void SDLCALL pick_cb(void *ud, const char *const *list, int filter)
{
    (void)ud; (void)filter;
    if (list && list[0] && strlen(list[0]) < sizeof pick_path) {
        strcpy(pick_path, list[0]);
        SDL_SetAtomicInt(&pick_state, 1);
    } else
        SDL_SetAtomicInt(&pick_state, 2);
}

/* opens the settings screen's folder dialog, starting in `start` (a file starts in the folder it is in); 0 if opened */
static int start_pick(const char *start)
{
    static char from[1024];             /* the dialog's start, kept while it is open */
    SDL_PathInfo info;
    if (!g_win) return -1;
    SDL_SetAtomicInt(&pick_state, 0);
    from[0] = 0;
    if (start && strlen(start) < sizeof from) {
        /* the row's folder; a file (a ROM given by itself) starts in the folder it is in */
        strcpy(from, start);
        if (!SDL_GetPathInfo(from, &info)) {
            RLOG_W("sdl3", "folder dialog start %s cannot be read: %s", from, SDL_GetError());
        } else if (info.type == SDL_PATHTYPE_FILE) {
            char *cut = strrchr(from, '/'), *bs = strrchr(from, '\\');
            if (bs > cut) cut = bs;
            if (cut) *cut = 0;
        }
    }
    SDL_ShowOpenFolderDialog(pick_cb, NULL, g_win, from[0] ? from : NULL, false);
    return 0;
}

/* puts a left click (and its release) into the window's event queue, as if the mouse had done it */
void plat_window_click(float x, float y, int release)
{
    SDL_Event ev;
    int i;
    if (!g_win) return;
    for (i = 0; i < (release ? 2 : 1); i++) {
        memset(&ev, 0, sizeof ev);
        ev.type = i ? SDL_EVENT_MOUSE_BUTTON_UP : SDL_EVENT_MOUSE_BUTTON_DOWN;
        ev.button.windowID = SDL_GetWindowID(g_win);
        ev.button.which = 0;
        ev.button.button = SDL_BUTTON_LEFT;
        ev.button.down = !i;
        ev.button.clicks = 1;
        ev.button.x = x;
        ev.button.y = y;
        sdl_ok(SDL_PushEvent(&ev), "SDL_PushEvent (click)");
    }
}

/* A title bar's height where the backend cannot read it (SDL_GetWindowBordersSize fails on macOS:
   measured there, a window asked for more than fits is given the usable height less 32) */
#define TITLE_ALLOW 40

/* the biggest whole scale (8 at most) at which a 320x200 (or 320x240) picture and the window's frame fit the screen */
int plat_max_scale(int aspect)
{
    SDL_DisplayID d;
    SDL_Rect r;
    int s, top = 0, left = 0, bottom = 0, right = 0;
    if (!SDL_WasInit(SDL_INIT_VIDEO)) return 8;
    d = g_win ? SDL_GetDisplayForWindow(g_win) : SDL_GetPrimaryDisplay();
    if (!d || !SDL_GetDisplayUsableBounds(d, &r)) {
        static int said;
        if (!said) said = 1, RLOG_W("sdl3", "the display's size is unknown, scale 8 allowed: %s", SDL_GetError());
        return 8;
    }
    {
        int got = g_win && SDL_GetWindowBordersSize(g_win, &top, &left, &bottom, &right);
        static int said;
        if (g_win && !got && !said) said = 1, RLOG_W("sdl3", "the window's frame size is unknown, title bar %d assumed: %s", TITLE_ALLOW, SDL_GetError());
        if (!got || top <= 0) top = TITLE_ALLOW;
    }
    for (s = 8; s > 1; s--)
        if (320 * s + left + right <= r.w && (aspect ? 240 : 200) * s + top + bottom <= r.h) break;
    return s;
}

/* full screen on or off (never in a hidden check run) */
static void set_fullscreen(int on)
{
    if (uwf_offscreen()) return;
    sdl_ok(SDL_SetWindowFullscreen(g_win, on != 0), "SDL_SetWindowFullscreen");
}

/* With our renderer the window has a size of its own (UWPORT_SIZE, default 1920x1080; the screen's usable area at
   most), not 320x200 times a whole scale. big_w stays 0 when there is no renderer: the original's scales then. */
static int big_w, big_h;
static void big_window_size(void)
{
    char path[1024];
    const char *e = getenv("UWPORT_SIZE"), *base = plat_base_dir(), *gdi = getenv("UWPORT_GDI");
    SDL_Rect wa;
    SDL_PathInfo info;
    int w = 1920, h = 1080, fw = 16, fh = TITLE_ALLOW - 1;   /* (Win32's overlapped window's frame at 100%: 16 x 39) */
    big_w = big_h = 0;
    if ((gdi && *gdi && *gdi != '0') || !base) return;
#ifdef UWPORT_PLAIN
    return;   /* uw-original: the original's own picture, no renderer */
#endif
#ifdef _WIN32
    snprintf(path, sizeof path, "%suwview.dll", base);
#else
    snprintf(path, sizeof path, "%slibuwview.so", base);   /* (Linux: the Vulkan renderer, uwfront.c's view_start) */
#endif
    if (!SDL_GetPathInfo(path, &info)) {   /* no renderer: the original's scales */
        RLOG_I("sdl3", "no renderer library at %s (%s): the original's window scales", path, SDL_GetError());
        return;
    }
    if (e && *e && sscanf(e, "%dx%d", &w, &h) != 2) {
        RLOG_W("sdl3", "UWPORT_SIZE=%s is not WIDTHxHEIGHT: the original's window scales", e);
        return;
    }
    if (w < 320 || h < 200) {
        RLOG_W("sdl3", "window size %dx%d is below 320x200: the original's window scales", w, h);
        return;
    }
    if (SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &wa)) {
        if (w + fw > wa.w) w = wa.w - fw;
        if (h + fh > wa.h) h = wa.h - fh;
    } else {
        RLOG_W("sdl3", "the display's size is unknown, the window is not cut to it: %s", SDL_GetError());
    }
    big_w = w, big_h = h;
}

/* applies the display settings: full screen, the scale (cut to what fits), the aspect; resizes the window to match */
void plat_set_display(int fullscreen, int scale, int aspect, int integer_scale)
{
    int most = plat_max_scale(aspect);
    live.fullscreen = fullscreen;
    live.scale = scale < 1 ? 1 : scale > most ? most : scale;
    live.aspect = aspect;
    live.integer_scale = integer_scale;
    if (!g_win) return;
    set_fullscreen(fullscreen != 0);
    if (big_w) live.integer_scale = 0;
    if (!fullscreen) {
        if (big_w) uwp_resize_client(big_w, big_h);   /* our renderer's window keeps its size */
        else uwp_resize_client(320 * live.scale, (aspect ? 240 : 200) * live.scale);
    }
}

static void (*audio_fill)(int16_t *, int);

/* SDL asks for more sound: fill it from the game's mixer (audio_fill) in blocks of at most 512 stereo frames */
static void SDLCALL audio_cb(void *ud, SDL_AudioStream *s, int additional, int total)
{
    int16_t buf[2048];
    (void)ud; (void)total;
    while (additional > 0) {
        int frames = additional / 4;
        if (frames > 512) frames = 512;
        if (frames <= 0) frames = 1;
        audio_fill(buf, frames);
        sdl_ok(SDL_PutAudioStreamData(s, buf, frames * 4), "SDL_PutAudioStreamData");
        additional -= frames * 4;
    }
}

/* opens the default sound device as 16-bit stereo at `rate`, fed by `fill`; 0 on success */
int plat_audio_open(int rate, void (*fill)(int16_t *, int))
{
    SDL_AudioSpec spec;
    SDL_AudioStream *s;
    if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        RLOG_E("sdl3", "cannot start SDL audio: %s", SDL_GetError());
        return -1;
    }
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq = rate;
    audio_fill = fill;
    s = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_cb, NULL);
    if (!s) {
        RLOG_E("sdl3", "cannot open the sound device at %d Hz: %s", rate, SDL_GetError());
        return -1;
    }
    sdl_ok(SDL_ResumeAudioStreamDevice(s), "SDL_ResumeAudioStreamDevice");
    return 0;
}

/* The window's icon, the project's PLAT_ICON (plat.h). Not on macOS, where the .app's own icon is the Dock's. */
static void set_icon(SDL_Window *w)
{
#if defined(PLAT_ICON) && !defined(__APPLE__)
    SDL_Surface *s = SDL_CreateSurfaceFrom(PLAT_ICON_W, PLAT_ICON_H, SDL_PIXELFORMAT_RGBA32,
                                           (void *)plat_icon_rgba, PLAT_ICON_W * 4);
    if (s) {
        sdl_ok(SDL_SetWindowIcon(w, s), "SDL_SetWindowIcon");
        SDL_DestroySurface(s);
    } else {
        RLOG_W("sdl3", "cannot make the window icon: %s", SDL_GetError());
    }
#else
    (void)w;
#endif
}

/* ---- the window's events to our front end (plat_win32.c's wnd_proc) ---- */

/* the focus lost or back (the window's, or UWPORT_FOCUSTEST's), to the front end; the loss is logged under UWPORT_LOOKDBG */
static void focus_event(int lost)
{
    if (!lost) {
        uwf_focus(1);
        return;
    }
    if (getenv("UWPORT_LOOKDBG"))   /* (SDL does not say which window takes the focus) */
        RLOG_I("sdl3", "focus: lost to window %p; the foreground window unknown (SDL) at %lu ms%s",
                (void *)NULL, (unsigned long)(ms_now() - L.start), focus_test_msg ? " (focus test)" : "");
    uwf_focus(0);
}

static float raw_rx, raw_ry, wheel_rest;   /* the motion's and the wheel's parts not yet whole */
/* one SDL event of the window to the front end: quit, files dropped, keys, mouse, wheel, focus, minimise, resize */
static void event(const SDL_Event *e)
{
    switch (e->type) {
    case SDL_EVENT_QUIT:
        if (hooks->lifecycle) hooks->lifecycle(PLAT_QUIT_REQUEST);
        L.quit = 1;
        break;
    case SDL_EVENT_DROP_FILE:
        if (hooks->drop && e->drop.data) hooks->drop(e->drop.data);
        break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        key_event(&e->key);
        break;
    case SDL_EVENT_MOUSE_MOTION: {
        int x = (int)(e->motion.x * dens), y = (int)(e->motion.y * dens), over;
        long dx, dy;
        if (e->motion.which == SDL_TOUCH_MOUSEID) break;
        /* the motion in the mouse's own counts (relative mode while captured: plat_win32.c's raw input), first, as the
           raw input's message comes before the move's there */
        raw_rx += e->motion.xrel * dens, raw_ry += e->motion.yrel * dens;
        dx = (long)raw_rx, dy = (long)raw_ry;
        raw_rx -= (float)dx, raw_ry -= (float)dy;
        if (dx || dy) uwf_raw_motion(dx, dy);
        uwf_pointer(PLAT_POINTER_MOVE, x, y, 0);
        over = uwf_hover(x, y);   /* the game's cursor stands in for the host's over the picture (-1 not now) */
        if (over >= 0 && over != cursor_hidden) {
            if (over) sdl_ok(SDL_HideCursor(), "SDL_HideCursor"); else sdl_ok(SDL_ShowCursor(), "SDL_ShowCursor");
            cursor_hidden = over;
        }
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        unsigned b = e->button.button == SDL_BUTTON_LEFT ? PLAT_BUTTON_LEFT : e->button.button == SDL_BUTTON_RIGHT ? PLAT_BUTTON_RIGHT
                   : e->button.button == SDL_BUTTON_MIDDLE ? PLAT_BUTTON_MIDDLE : 0;
        if (e->button.which == SDL_TOUCH_MOUSEID || !b) break;   /* (Win32's: no X buttons) */
        uwf_pointer(e->type == SDL_EVENT_MOUSE_BUTTON_DOWN ? PLAT_POINTER_DOWN : PLAT_POINTER_UP,
                    (int)(e->button.x * dens), (int)(e->button.y * dens), b);
        break;
    }
    case SDL_EVENT_MOUSE_WHEEL: {      /* 120 a notch, up positive (Win32's WHEEL_DELTA) */
        int d;
        wheel_rest += (e->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -e->wheel.y : e->wheel.y) * 120.0f;
        d = (int)wheel_rest;
        wheel_rest -= (float)d;
        if (d) uwf_wheel(d);
        break;
    }
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        if (cursor_hidden && uwf_pointer_free()) { sdl_ok(SDL_ShowCursor(), "SDL_ShowCursor"); cursor_hidden = 0; }
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        focus_event(1);
        break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        focus_event(0);
        break;
    case SDL_EVENT_WINDOW_MINIMIZED:
        L.hidden_win = 1;
        if (hooks->lifecycle) hooks->lifecycle(PLAT_SUSPEND);
        break;
    case SDL_EVENT_WINDOW_RESTORED:
        if (L.hidden_win) {
            L.hidden_win = 0;
            if (hooks->lifecycle) hooks->lifecycle(PLAT_RESUME);
        }
        uwf_resized();
        break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        dens = SDL_GetWindowPixelDensity(g_win);
        if (dens <= 0) dens = 1;
        uwf_resized();                  /* the pointer held in the window's new rectangle */
        break;
    default:
        break;
    }
}

/* shows the picture uwf_frame gives (none when our renderer presented it) through SDL's renderer, which is made at the
   first such picture and never while our renderer has the window; also lays the Esc menu's layer over it and takes the
   window screenshot when one is asked for. 1 if SDL presented */
static int present(void)
{
    int sw, sh;
    const uint32_t *src = uwf_frame(&sw, &sh);
    SDL_FRect d;
    if (!src) return 0;
    if (!ren) {
        if (!(ren = SDL_CreateRenderer(g_win, NULL))) {
            RLOG_E("sdl3", "no SDL renderer: %s", SDL_GetError());
            return 0;
        }
        sdl_ok(SDL_SetRenderVSync(ren, 1), "SDL_SetRenderVSync");
        RLOG_I("sdl3", "the picture through SDL's renderer (%s)", SDL_GetRendererName(ren));
    }
    if (sw != tw || sh != th) {
        if (tex) SDL_DestroyTexture(tex);
        tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, sw, sh);
        if (!tex) RLOG_E("sdl3", "cannot make the %dx%d picture texture: %s", sw, sh, SDL_GetError());
        sdl_ok(SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST), "SDL_SetTextureScaleMode");   /* (GDI's COLORONCOLOR) */
        tw = sw, th = sh;
    }
    sdl_ok(SDL_UpdateTexture(tex, NULL, src, sw * 4), "SDL_UpdateTexture (picture)");
    sdl_ok(SDL_SetRenderDrawColor(ren, 0, 0, 0, 255), "SDL_SetRenderDrawColor");   /* the bars round the picture black */
    sdl_ok(SDL_RenderClear(ren), "SDL_RenderClear");
    d.x = (float)L.dst.left, d.y = (float)L.dst.top;
    d.w = (float)(L.dst.right - L.dst.left), d.h = (float)(L.dst.bottom - L.dst.top);
    sdl_ok(SDL_RenderTexture(ren, tex, NULL, &d), "SDL_RenderTexture (picture)");
    {   /* our Esc menu without the renderer (uwmenu.c, drawn by uwf_frame this pass): its layer over the picture */
        static SDL_Texture *mtex;
        static int mw, mh;
        int ow = 0, oh = 0;
        const unsigned char *ov = uwmenu_layer(&ow, &oh);
        if (ov) {
            if (!mtex || ow != mw || oh != mh) {
                if (mtex) SDL_DestroyTexture(mtex);
                mtex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, ow, oh);   /* (0xAABBGGRR) */
                if (mtex) sdl_ok(SDL_SetTextureBlendMode(mtex, SDL_BLENDMODE_BLEND), "SDL_SetTextureBlendMode");
                else RLOG_E("sdl3", "cannot make the %dx%d menu texture: %s", ow, oh, SDL_GetError());
                mw = ow, mh = oh;
            }
            if (mtex)
                sdl_ok(SDL_UpdateTexture(mtex, NULL, ov, ow * 4), "SDL_UpdateTexture (menu)"),
                sdl_ok(SDL_RenderTexture(ren, mtex, NULL, NULL), "SDL_RenderTexture (menu)");
        }
    }
    if (L.shot == 1 && live.window_shot_path) {
        /* the window's contents as presented */
        SDL_Surface *s = SDL_RenderReadPixels(ren, NULL), *c = s ? SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGB24) : NULL;
        if (!s) RLOG_E("sdl3", "cannot read the window's pixels: %s", SDL_GetError());
        else if (!c) RLOG_E("sdl3", "cannot convert the window's pixels: %s", SDL_GetError());
        if (c) {
            uint8_t *rgbp = malloc((size_t)c->w * (size_t)c->h * 3);
            int yy;
            if (!rgbp) RLOG_E("sdl3", "out of memory for the %dx%d window shot", c->w, c->h);
            for (yy = 0; rgbp && yy < c->h; yy++)
                memcpy(rgbp + (size_t)yy * (size_t)c->w * 3, (uint8_t *)c->pixels + (size_t)yy * (size_t)c->pitch, (size_t)c->w * 3);
            if (rgbp && plat_write_png_rgb(live.window_shot_path, rgbp, c->w, c->h) == 0)
                RLOG_I("sdl3", "wrote %s (%dx%d, the window)", live.window_shot_path, c->w, c->h);
            else if (rgbp)
                RLOG_E("sdl3", "cannot write the window shot %s", live.window_shot_path);
            free(rgbp);
        }
        if (c) SDL_DestroySurface(c);
        if (s) SDL_DestroySurface(s);
        L.shot = 2;
    }
    sdl_ok(SDL_RenderPresent(ren), "SDL_RenderPresent");
    return 1;
}

/* One pass of the main loop: the front end's start, the checks' events, the window's events, the input's step, the
   folder picker's answer, the picture taken, then shown. 1 to stop. */
static int loop_step(void)
{
    SDL_Event e;
    uwf_tick_begin();   /* (the game's mouse-look turned on or off: the capture follows it now) */
    uwf_tick_tests();   /* (the checks' UWPORT_FOCUSTEST and UWPORT_MOUSETEST: uwfront.c) */
    while (SDL_PollEvent(&e)) event(&e);
    uwf_tick_input();   /* (the runtime's tick, the settings screen, the input's step, our taps' breaks) */
    {
        int ps = SDL_SetAtomicInt(&pick_state, 0);
        if (ps) settings_folder_chosen(ps == 1 ? pick_path : NULL);
    }
    uwf_tick_scan();    /* (the game's picture, the screenshots, a timed run's end) */
    if (SDL_GetAtomicInt(&game_done)) L.quit = 1;
    if (!g_win || L.hidden_win) {
        SDL_Delay(16);
    } else {
        int shown = present();
        L.presents++;
        if (!uwf_view_ok() && !shown) SDL_Delay(10);   /* (nothing paced this pass: SDL's vsync, or the renderer's present) */
    }
    return L.quit;
}

/* The log file: when the program is started by itself (not with its output redirected to a file or pipe, as a check
   run does), everything printed goes to home/logs/uwr.log, rotated past 5 MB, with the mouse-look trace on. */
#ifdef _WIN32
#define SEP "\\"
#else
#define SEP "/"
#endif
#ifndef _WIN32
/* Started from a terminal, the log's lines are shown there too, as they are written. The log stays the file, written
   straight; a thread reads the file from this start's first line and copies what is added to the terminal: ours, the
   renderer's, the map maker's. At the program's end what is left is copied before it goes. (A failed write to the
   terminal is not logged: the log is what is being copied, so the line would be copied back.) */
static int term_out = -1, term_file = -1;
static SDL_Mutex *term_lock;
/* copies what the log gained since the last copy to the terminal */
static void term_copy(void)
{
    char buf[4096];
    ssize_t got;
    SDL_LockMutex(term_lock);
    while ((got = read(term_file, buf, sizeof buf)) > 0)
        for (ssize_t at = 0, put; at < got; at += put)
            if ((put = write(term_out, buf + at, (size_t)(got - at))) <= 0) break;
    SDL_UnlockMutex(term_lock);
}
/* the thread that copies the log to the terminal twenty times a second */
static int SDLCALL term_thread(void *arg)
{
    (void)arg;
    for (;;) term_copy(), SDL_Delay(50);
    return 0;
}
/* at exit: flush our streams and copy the rest to the terminal */
static void term_last(void) { fflush(stdout), fflush(stderr), term_copy(); }
/* starts the copying to the terminal `out` (a dup of the original stderr) from the log's present end; on failure the
   terminal copy is simply off and the log goes on */
static void term_start(const char *path, int out)
{
    SDL_Thread *th;
    if ((term_file = open(path, O_RDONLY)) < 0) {
        RLOG_W("sdl3", "cannot follow the log %s on the terminal: %s", path, rlog_errno());
        close(out);
        return;
    }
    if (!(term_lock = SDL_CreateMutex())) {
        RLOG_W("sdl3", "cannot follow the log on the terminal: %s", SDL_GetError());
        close(out);
        return;
    }
    if (lseek(term_file, 0, SEEK_END) < 0) RLOG_W("sdl3", "cannot skip to the log's end: %s", rlog_errno());
    term_out = out;
    atexit(term_last);
    if ((th = SDL_CreateThread(term_thread, "terminal", NULL)) != NULL) SDL_DetachThread(th);
    else RLOG_W("sdl3", "cannot start the terminal copy thread: %s", SDL_GetError());
}
#endif
/* Moves stderr and stdout into home/logs/uwr.log (rotating a log past 5 MB to uwr.1.log) unless they already go to a
   file or pipe, or this is a hidden run. Lines before the move go to the terminal; if freopen itself fails nothing can
   be logged any more (the failed call closes stderr), so that failure is silent. */
static void log_start(void)
{
    struct stat es;
    const char *home = plat_home();
    char dir[1024], path[1100], old[1100];
    SDL_PathInfo info;
    if ((fstat(2, &es) == 0 && (S_ISREG(es.st_mode) || S_ISFIFO(es.st_mode))) || !home || getenv("UWPORT_OFFSCREEN")) return;
    snprintf(dir, sizeof dir, "%s" SEP "logs", home);
    if (!SDL_CreateDirectory(dir)) RLOG_E("sdl3", "cannot create the log folder %s: %s", dir, SDL_GetError());
    snprintf(path, sizeof path, "%s" SEP "uwr.log", dir), snprintf(old, sizeof old, "%s" SEP "uwr.1.log", dir);
    if (SDL_GetPathInfo(path, &info) && info.size > 5u << 20) {
        SDL_PathInfo oi;
        if (SDL_GetPathInfo(old, &oi)) sdl_ok(SDL_RemovePath(old), "SDL_RemovePath (old log)");   /* (none yet at the first rotation) */
        if (!SDL_RenamePath(path, old)) RLOG_E("sdl3", "cannot rotate the log %s: %s", path, SDL_GetError());
    }
    RLOG_I("sdl3", "the log is %s", path);
    fflush(stdout), fflush(stderr);
#ifndef _WIN32
    int term = isatty(2) ? dup(2) : -1;   /* the terminal the game was started from, kept (freopen closes ours) */
#endif
    if (!freopen(path, "a", stderr)) return;
    setvbuf(stderr, NULL, _IONBF, 0);
    if (freopen(path, "a", stdout)) setvbuf(stdout, NULL, _IONBF, 0);
    else RLOG_W("sdl3", "cannot send stdout to the log %s: %s", path, rlog_errno());
#ifndef _WIN32
    if (term >= 0) term_start(path, term);
#endif
#ifdef _WIN32
    if (!getenv("UWPORT_LOOKDBG")) _putenv("UWPORT_LOOKDBG=1");
#else
    if (!getenv("UWPORT_LOOKDBG")) setenv("UWPORT_LOOKDBG", "1", 1);
#endif
    {
        time_t now = time(NULL);
        char ts[64];
        strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(stderr, "\n==== uwr started %s ====\n", ts);   /* (a plain separator line, not an rlog line) */
    }
}

/* the backend's entry point: sets up the log, SDL and the window, starts the game on its own thread, then runs the
   event loop here until the game or the window ends; returns the game's status */
int plat_run(const PlatConfig *cfg0, const PlatHooks *h, int (*game)(void *), void *arg)
{
    int scale = cfg0->scale > 0 ? cfg0->scale : 3;
    static void *targ[2];
    SDL_Thread *gt;

    hooks = h;
    live = *cfg0;
    if (!live.hidden) log_start();
    RLOG_I("sdl3", "mouse: %s", live.mouse_lock ? "lock" : "follow");
    settings_pick_folder = start_pick;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        RLOG_E("sdl3", "SDL_Init: %s", SDL_GetError());
        return 1;
    }
    if (!live.hidden) {
        int w, hh;
        if (scale > plat_max_scale(live.aspect)) scale = plat_max_scale(live.aspect);
        live.scale = scale;
        w = 320 * scale, hh = (live.aspect ? 240 : 200) * scale;
        big_window_size();
        if (big_w) {
            w = big_w, hh = big_h;
            live.integer_scale = 0;               /* the picture fills the height, scaled freely */
            RLOG_I("sdl3", "the window at %dx%d for our renderer", big_w, big_h);
        }
        /* UWPORT_OFFSCREEN=1 (the checks): the window made but never shown, the renderer drawing into it unseen:
           nothing takes the user's focus. No SDL renderer here: our renderer may take the window (present) */
        g_win = SDL_CreateWindow(live.title ? live.title : PLAT_TITLE, w, hh,
                                 SDL_WINDOW_RESIZABLE | (uwf_offscreen() ? SDL_WINDOW_HIDDEN : 0));
        if (!g_win) {
            RLOG_E("sdl3", "no window: %s", SDL_GetError());
            return 1;
        }
        dens = SDL_GetWindowPixelDensity(g_win);
        if (dens <= 0) dens = 1;
        set_icon(g_win);
        if (!uwf_offscreen()) {
            sdl_ok(SDL_SyncWindow(g_win), "SDL_SyncWindow");
            uwf_set_focused((SDL_GetWindowFlags(g_win) & SDL_WINDOW_INPUT_FOCUS) != 0);   /* started behind another window: no capture till it is chosen */
        }
        if (live.fullscreen) set_fullscreen(1);
        if (live.mouse_lock) uwf_mouse_title(0);
        uwf_view_start();
        uwf_fx_audio_start();   /* fx_audio 1: OpenAL Soft (bin\soft_oal.dll); else nothing */
        RLOG_I("sdl3", "SDL3 window (%s), %s", SDL_GetCurrentVideoDriver(),
#ifdef _WIN32
                uwf_view_loaded() ? "our D3D12 renderer" : "SDL's renderer");
#else
                uwf_view_loaded() ? "our Vulkan renderer (libuwview.so)" : "SDL's renderer");
#endif
    }
    uwmenu_register();                 /* our Esc menu for when the renderer is not up (uwmenu.c) */
    uwf_input_init();                  /* the window's input (uwinput.c) */
    targ[0] = (void *)game;
    targ[1] = arg;
    if (g_win && !uwf_offscreen()) sdl_ok(SDL_SyncWindow(g_win), "SDL_SyncWindow (full screen)");   /* (full screen's size settled) */
    uwf_view_warm();                   /* the renderer up before the game's thread starts: nothing heard before a picture */
    L.start = ms_now();
    {
        /* the game's thread gets a large stack: host frames are bigger than DOS's */
        SDL_PropertiesID props = SDL_CreateProperties();
        if (!props) RLOG_E("sdl3", "cannot make the game thread's properties: %s", SDL_GetError());
        sdl_ok(SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_ENTRY_FUNCTION_POINTER, (void *)game_thread), "SDL_SetPointerProperty (entry)");
        sdl_ok(SDL_SetStringProperty(props, SDL_PROP_THREAD_CREATE_NAME_STRING, "game"), "SDL_SetStringProperty (name)");
        sdl_ok(SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_USERDATA_POINTER, targ), "SDL_SetPointerProperty (userdata)");
        sdl_ok(SDL_SetNumberProperty(props, SDL_PROP_THREAD_CREATE_STACKSIZE_NUMBER, 16 << 20), "SDL_SetNumberProperty (stack)");
        gt = SDL_CreateThreadWithProperties(props);
        SDL_DestroyProperties(props);
    }
    if (!gt) {
        RLOG_E("sdl3", "no game thread: %s", SDL_GetError());
        return 1;
    }
    SDL_DetachThread(gt);   /* (never joined: it ends with the process; SDL_Quit said "Leaked thread" otherwise) */
    while (!loop_step()) {}
    uwf_shutdown();   /* (the language model, the capture, the renderer, our audio let go) */
    if (tex) SDL_DestroyTexture(tex);
    if (ren) SDL_DestroyRenderer(ren);
    if (g_win) SDL_DestroyWindow(g_win);
    SDL_Quit();
    return game_status;
}

/* Dialogs (plat.h): SDL's message box and folder picker, usable before plat_run. */

/* a message to the log and in a message box (an error one when `error`). (rlog cuts a very long line in the log; the box has it all) */
void plat_message(int error, const char *title, const char *text)
{
    rlog(error ? RLOG_ERROR : RLOG_INFO, "sdl3", "%s\n%s", title, text);
    sdl_ok(SDL_ShowSimpleMessageBox(error ? SDL_MESSAGEBOX_ERROR : SDL_MESSAGEBOX_INFORMATION, title, text, NULL), "SDL_ShowSimpleMessageBox");
}

static struct { SDL_AtomicInt done; char path[1024]; int ok; } picked;

/* the answer of plat_choose_folder's dialog: the folder, or none (cancelled, or the dialog failed) */
static void SDLCALL folder_cb(void *ud, const char * const *list, int filter)
{
    (void)ud; (void)filter;
    picked.ok = 0;
    if (list && list[0] && strlen(list[0]) < sizeof picked.path) {
        strcpy(picked.path, list[0]);
        picked.ok = 1;
    } else if (!list) {
        RLOG_E("sdl3", "no folder dialog: %s", SDL_GetError());
    }
    SDL_SetAtomicInt(&picked.done, 1);
}

/* asks the user (a message box, then SDL's folder dialog) for a folder, into `out`; 0 if one was chosen, -1 to quit (rlog cuts a very long text in the log.) */
int plat_choose_folder(const char *title, const char *text, char *out, size_t outsz)
{
    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Quit" },
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Choose folder..." },
    };
    SDL_MessageBoxData box;
    int id = 0;
    RLOG_I("sdl3", "%s\n%s", title, text);
    memset(&box, 0, sizeof box);
    box.flags = SDL_MESSAGEBOX_INFORMATION;
    box.title = title;
    box.message = text;
    box.numbuttons = 2;
    box.buttons = buttons;
    if (!SDL_ShowMessageBox(&box, &id)) {
        RLOG_E("sdl3", "cannot show the message box: %s", SDL_GetError());
        return -1;
    }
    if (id != 1) return -1;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        RLOG_E("sdl3", "SDL_Init (folder dialog): %s", SDL_GetError());
        return -1;
    }
    SDL_SetAtomicInt(&picked.done, 0);
    SDL_ShowOpenFolderDialog(folder_cb, NULL, NULL, NULL, false);
    while (!SDL_GetAtomicInt(&picked.done)) {
        SDL_Event e;
        SDL_WaitEventTimeout(&e, 50);
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    if (!picked.ok || strlen(picked.path) >= outsz) return -1;
    strcpy(out, picked.path);
    return 0;
}

/* the folder the program is in, with a closing separator (SDL's own answer) */
const char *plat_base_dir(void)
{
    const char *b = SDL_GetBasePath();
    static int said;
    if (!b && !said) said = 1, RLOG_W("sdl3", "the program's folder is unknown: %s", SDL_GetError());
    return b;
}

/* ---- the services our front end asks of the window (uwfront.h) ---- */

/* small services: the clock, sleep, music room (nothing here), atomic exchange and increment */
uint64_t uwp_ms(void) { return ms_now(); }
void uwp_sleep_ms(unsigned ms) { SDL_Delay(ms); }
void uwp_music_room(int ms, int high) { (void)ms, (void)high; }   /* (Windows' waveOut path alone so far: SDL's stream is its own) */
long uwp_xchg(volatile long *p, long v) { return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST); }
long uwp_inc(volatile long *p) { return __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST); }
/* the window's native handle (our renderer's V.init takes the HWND); elsewhere the SDL window */
void *uwp_window(void)
{
    if (!g_win) return NULL;
#ifdef _WIN32
    return SDL_GetPointerProperty(SDL_GetWindowProperties(g_win), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
#else
    return g_win;
#endif
}
/* the window's size in pixels; 0 if there is no window */
int uwp_window_size(int *w, int *h)
{
    *w = *h = 0;
    return g_win && sdl_ok(SDL_GetWindowSizeInPixels(g_win, w, h), "SDL_GetWindowSizeInPixels");
}
/* sets the window's drawing area to w x h pixels */
void uwp_resize_client(int w, int h) { if (g_win) sdl_ok(SDL_SetWindowSize(g_win, (int)((float)w / dens), (int)((float)h / dens)), "SDL_SetWindowSize"); }
void uwp_title(const char *t) { if (g_win) sdl_ok(SDL_SetWindowTitle(g_win, t), "SDL_SetWindowTitle"); }
/* the pointer held in the window and the host's cursor hidden, through SDL's relative mouse mode (whose motion is the
   mouse's own counts); the change is logged under UWPORT_LOOKDBG */
void uwp_capture(int on)
{
    if (!g_win || uwf_offscreen()) return;
    sdl_ok(SDL_SetWindowRelativeMouseMode(g_win, on != 0), "SDL_SetWindowRelativeMouseMode");
    if (on) { if (!cursor_hidden) { sdl_ok(SDL_HideCursor(), "SDL_HideCursor"); cursor_hidden = 1; } }
    else if (cursor_hidden) { sdl_ok(SDL_ShowCursor(), "SDL_ShowCursor"); cursor_hidden = 0; }
    if (getenv("UWPORT_LOOKDBG")) {
        int w = 0, h = 0;
        uwp_window_size(&w, &h);
        RLOG_I("sdl3", "clip: set %d: clip 0,0,%d,%d client 0,0,%d,%d screen 0,0,0,0 cursor %d at %lu ms (SDL relative mode)",
                on, on ? w : 0, on ? h : 0, w, h, !SDL_CursorVisible() ? 0 : 1, (unsigned long)(ms_now() - L.start));
    }
}
int uwp_cursor_hidden(void) { return cursor_hidden; }
/* hides or shows the host's cursor (only when that changes it) */
void uwp_cursor_hide(int hide)
{
    if (hide) { if (!cursor_hidden) { sdl_ok(SDL_HideCursor(), "SDL_HideCursor"); cursor_hidden = 1; } }
    else if (cursor_hidden) { sdl_ok(SDL_ShowCursor(), "SDL_ShowCursor"); cursor_hidden = 0; }
}
/* our panels' pointer as the host's cursor (plat_win32.c's): an SDL colour cursor made from the picture (0xAARRGGBB,
   straight alpha: SDL's ARGB8888), set at once, the one before it destroyed; NULL: the system's default again */
void uwp_cursor_picture(const unsigned *argb, int w, int h, int hx, int hy)
{
    static SDL_Cursor *ours;
    SDL_Cursor *made = NULL;
    if (argb && w > 0 && h > 0) {
        SDL_Surface *sf = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ARGB8888, (void *)argb, w * 4);
        if (sf) made = SDL_CreateColorCursor(sf, hx, hy), SDL_DestroySurface(sf);
        if (!made) { RLOG_E("sdl3", "cannot make the pointer's cursor (%dx%d): %s", w, h, SDL_GetError()); return; }
        sdl_ok(SDL_SetCursor(made), "SDL_SetCursor");
    } else sdl_ok(SDL_SetCursor(SDL_GetDefaultCursor()), "SDL_SetCursor");
    if (ours) SDL_DestroyCursor(ours);
    ours = made;
}
/* the host's pointer in the window's pixels; 0 if it cannot be told */
int uwp_pointer_pos(int *x, int *y)
{
    float gx, gy;
    int wx, wy;
    if (!g_win || !sdl_ok(SDL_GetWindowPosition(g_win, &wx, &wy), "SDL_GetWindowPosition")) return 0;
    SDL_GetGlobalMouseState(&gx, &gy);
    *x = (int)((gx - (float)wx) * dens), *y = (int)((gy - (float)wy) * dens);
    return 1;
}
/* holds the pointer inside the rectangle (uwfront.c's capture_sync: the original's options panel), again when it
   changed; the first time the pointer is put in its middle. 1 if it was moved to *mx, *my */
int uwp_clip_hold(int x0, int y0, int x1, int y1, int first, int *mx, int *my)
{
    static SDL_Rect last;
    SDL_Rect r;
    r.x = (int)((float)x0 / dens), r.y = (int)((float)y0 / dens);
    r.w = (int)((float)(x1 - x0) / dens), r.h = (int)((float)(y1 - y0) / dens);
    if (!g_win) return 0;
    if (first || r.x != last.x || r.y != last.y || r.w != last.w || r.h != last.h) sdl_ok(SDL_SetWindowMouseRect(g_win, &r), "SDL_SetWindowMouseRect");
    last = r;
    if (!first) return 0;
    *mx = (x0 + x1) / 2, *my = (y0 + y1) / 2;
    SDL_WarpMouseInWindow(g_win, (float)*mx / dens, (float)*my / dens);
    return 1;
}
/* lets the pointer leave the window again */
void uwp_clip_free(void) { if (g_win) sdl_ok(SDL_SetWindowMouseRect(g_win, NULL), "SDL_SetWindowMouseRect (free)"); }
int uwp_ctrl_down(void) { return (SDL_GetModState() & SDL_KMOD_CTRL) != 0; }
/* closes the program, as the window's close button does */
void uwp_close(void)
{
    SDL_Event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = SDL_EVENT_QUIT;
    sdl_ok(SDL_PushEvent(&ev), "SDL_PushEvent (quit)");
}
/* the program started again with --menu (no autoload), as it was started, and this one closed */
void uwp_new_game(void)
{
#ifdef _WIN32
    static WCHAR cmd[4096];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si), si.cb = sizeof si;
    _snwprintf(cmd, 4096, L"%s --menu", GetCommandLineW());
    cmd[4095] = 0;
    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread), CloseHandle(pi.hProcess);
        uwp_close();
    } else {
        RLOG_E("sdl3", "cannot start the program again: error %lu", (unsigned long)GetLastError());
    }
#else
    static char buf[8192];
    const char *args[130];
    size_t n = 0, i;
    int argc = 0;
    FILE *f = fopen("/proc/self/cmdline", "rb");
    if (!f) {
        RLOG_E("sdl3", "cannot read /proc/self/cmdline to start the program again: %s", rlog_errno());
        return;
    }
    n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    for (i = 0; i < n && argc < 128; i += strlen(buf + i) + 1) args[argc++] = buf + i;
    args[argc++] = "--menu", args[argc] = NULL;
    {
        SDL_Process *p = SDL_CreateProcess(args, false);
        if (p) { SDL_DestroyProcess(p); uwp_close(); }
        else RLOG_E("sdl3", "cannot start the program again: %s", SDL_GetError());
    }
#endif
}
void uwp_repaint(void) {}   /* (the next pass presents through SDL's renderer) */
/* the checks' own focus loss or gain (UWPORT_FOCUSTEST), through the same path as the window's */
void uwp_test_focus(int away)
{
    focus_test_msg = 1;
    focus_event(away);
    focus_test_msg = 0;
}
/* loading a library (the renderer) and looking up its functions */
void *uwp_lib_open(const char *path)
{
    void *lib = SDL_LoadObject(path);
    if (!lib) RLOG_W("sdl3", "cannot load %s: %s", path, SDL_GetError());
    return lib;
}
void *uwp_lib_sym(void *lib, const char *name)
{
    void *f = (void *)SDL_LoadFunction((SDL_SharedObject *)lib, name);
    if (!f && lib) RLOG_D("sdl3", "no function %s in the library (optional ones are checked by the caller): %s", name, SDL_GetError());
    return f;
}
void uwp_lib_close(void *lib) { SDL_UnloadObject((SDL_SharedObject *)lib); }
