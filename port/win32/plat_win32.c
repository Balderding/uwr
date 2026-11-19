/* plat_win32.c: the Win32 backend of Exhume's platform API (runtime/port/platform/plat.h), in place of
   the SDL3 one (platform/sdl3/plat_sdl3.c, whose behaviour this follows point by point): the window
   and its scaling, the event loop, keys and pointer, lifecycle events, the high-resolution counter,
   threads, the audio stream, and the message box and folder picker before the window opens. The only
   file of the port that includes windows.h' window and sound parts. Nothing of the game is here: our front end
   (the menu, the panel, the input's routing, the renderer's table) is uwfront.c, through uwfront.h's seam.

   The game runs on a thread of its own (plat_run); this file's loop runs on the main thread. Each
   pass it reads the emulated VGA's picture through the scanout hook, converts it through the DAC's
   palette and presents it (GDI's StretchDIBits for now, paced by the compositor's DwmFlush). Keys:
   a window's key messages carry the PC's set-1 scan code (lParam bits 16-23, the E0 prefix as bit
   24), which go to the game as the keyboard sent them. A captured pointer's motion comes from raw
   input (WM_INPUT), as SDL's relative mode reads it.

   Log tag: win32 (rlog). Left as plain fprintf on purpose: log_start's own lines and the "==== uwr started ===="
   banner (they run while stderr is being sent to the log file), and the VEH: lines of a fault (written from inside
   the exception handler, with as little work as can be, never dropped by the log's level). */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <mmsystem.h>
#include <dwmapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "plat.h"
#include "ui/settings.h"
#include "uwfront.h"
#include "rlog.h"
void port_crash_handlers(void);         /* sys/crash.c */

static HWND g_win;                      /* the one window (NULL in a hidden run, live.hidden) */
static volatile LONG game_done;         /* 1 once the game's thread has ended: the loop then ends too */
static int game_status;                 /* the game's exit code, plat_run's answer */
static int focus_test_msg;             /* UWPORT_FOCUSTEST's own WM_KILLFOCUS / WM_SETFOCUS being handled (its log tag) */
static int cursor_hidden;              /* the host's cursor hidden (uwp_cursor_hide; ShowCursor's count is ours) */
static uint64_t ms_now(void) { return GetTickCount64(); }
static void set_capture(int on);
static void resize_client(int w, int h);

/* ---- keys: set-1 codes, 0x100 for the E0 prefix ---- */

/* Print Screen and Pause have sequences of their own (as plat_sdl3.c's key()) */
static int special_key(int vk, int down)
{
    int i;
    if (!hooks->key) return 1;
    if (vk == VK_SNAPSHOT) {
        static const uint8_t make[] = { 0xE0, 0x2A, 0xE0, 0x37 }, brk[] = { 0xE0, 0xB7, 0xE0, 0xAA };
        const uint8_t *s = down ? make : brk;
        for (i = 0; i < 4; i++) uwf_key_byte(s[i]);
        return 1;
    }
    if (vk == VK_PAUSE) {
        static const uint8_t pause[] = { 0xE1, 0x1D, 0x45, 0xE1, 0x9D, 0xC5 };
        if (down) for (i = 0; i < 6; i++) uwf_key_byte(pause[i]);
        return 1;
    }
    return 0;
}


/* ---- the game's thread, time, threads ---- */

static void *targ[2];                   /* the game's function and its argument, for game_thread */
/* the game's own thread: the crash handlers set up, the game run to its end, its exit code kept for plat_run */
static DWORD WINAPI game_thread(LPVOID p)
{
    void **a = p;
    int (*game)(void *) = (int (*)(void *))a[0];
    int st;
    port_crash_handlers();
    st = game(a[1]);
    game_status = st;
    InterlockedExchange(&game_done, 1);
    return (DWORD)st;
}

/* the game asks to end (on its own thread): the loop is told, and this thread waits for the process to close */
void plat_game_exit(int status)
{
    game_status = status;
    InterlockedExchange(&game_done, 1);
    for (;;) Sleep(1000);
}

void plat_game_park(void) { for (;;) Sleep(1000); }   /* the game's thread held for good (the process ends elsewhere) */

/* the high-resolution counter and its ticks a second */
uint64_t plat_counter(void) { LARGE_INTEGER c; QueryPerformanceCounter(&c); return (uint64_t)c.QuadPart; }
uint64_t plat_counter_hz(void) { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return (uint64_t)f.QuadPart; }

/* a precise sleep: a high-resolution waitable timer for all but the last half millisecond, then
   a spin to the counter (SDL_DelayPrecise does the same) */
void plat_sleep_ns(uint64_t ns)
{
    static __thread HANDLE t;
    uint64_t hz = plat_counter_hz(), end = plat_counter() + ns * hz / 1000000000u;
    if (ns > 1000000u) {
        if (!t) t = CreateWaitableTimerExW(NULL, NULL, 0x00000002 /* CREATE_WAITABLE_TIMER_HIGH_RESOLUTION */, TIMER_ALL_ACCESS);
        if (!t) t = CreateWaitableTimerW(NULL, TRUE, NULL);
        if (t) {
            LARGE_INTEGER due;
            due.QuadPart = -(LONGLONG)((ns - 500000u) / 100u);
            if (SetWaitableTimer(t, &due, 0, NULL, NULL, FALSE)) WaitForSingleObject(t, INFINITE);
        } else
            Sleep((DWORD)(ns / 1000000u) - 1);
    }
    while (plat_counter() < end) YieldProcessor();
}

struct thread_arg { int (*fn)(void *); void *arg; };
/* a runtime thread's start: its function and argument taken from the block plat_thread_start made, the block freed */
static DWORD WINAPI thread_start(LPVOID p)
{
    struct thread_arg a = *(struct thread_arg *)p;
    free(p);
    return (DWORD)a.fn(a.arg);
}
/* The music stream's room (the Esc menu's "Music buffer" and "Music thread first"; uwfront.c gives them every frame).
   ms: how much music is held ready before the device is given any, at the start and again after the stream ran
   dry. The runtime's ring starts playing at 2048 frames (46 ms) and that is its own; more is made here by giving
   the device silence, the ring left untouched, for the blocks that make up the difference. high: the timer thread
   that renders the music runs above the game's threads. -1: as it stands. */
static HANDLE pit_thread_h;
static volatile LONG music_room_ms = 150, music_high = 1, music_rehold;
void uwp_music_room(int ms, int high)
{
    if (ms >= 0 && ms != music_room_ms) InterlockedExchange(&music_room_ms, ms < 46 ? 46 : ms > 600 ? 600 : ms), InterlockedExchange(&music_rehold, 1);
    if (high >= 0) InterlockedExchange(&music_high, high != 0);
    if (pit_thread_h) SetThreadPriority(pit_thread_h, music_high ? THREAD_PRIORITY_HIGHEST : THREAD_PRIORITY_NORMAL);
}
/* one of the runtime's threads started (1 MB of stack reserved); -1 when it cannot be */
int plat_thread_start(const char *name, int (*fn)(void *), void *arg)
{
    struct thread_arg *a = malloc(sizeof *a);
    HANDLE h;
    (void)name;
    if (!a) return -1;
    a->fn = fn, a->arg = arg;
    h = CreateThread(NULL, 1 << 20, thread_start, a, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
    if (!h) { free(a); return -1; }
    /* the runtime's timer thread ("pit": it runs AIL's ticks and renders the music into the ring the device drains)
       is kept (2026-11-16, the user's log: "the music's stream ran dry 9 times in this run"): at normal priority any
       busy moment of the machine held it back longer than the ring's 46 ms. uwp_music_room raises it. */
    if (name && !strcmp(name, "pit")) { pit_thread_h = h; uwp_music_room(-1, -1); return 0; }
    CloseHandle(h);
    return 0;
}

/* ---- audio: waveOut, a thread of its own refilling blocks as the device gives them back ---- */

#define AUDIO_BLOCKS 4
#define AUDIO_FRAMES 512
static void (*audio_fill)(int16_t *, int);
static HWAVEOUT wo;
static HANDLE audio_ev;
static WAVEHDR audio_hdr[AUDIO_BLOCKS];
static int16_t audio_buf[AUDIO_BLOCKS][AUDIO_FRAMES * 2];

unsigned audio_underruns(void);
/* a block for the device: the ring's frames, or silence while more music is being held ready (uwp_music_room) */
static void audio_pull(int16_t *out, int frames)
{
    static unsigned seen;
    static int hold = -1;
    unsigned u = audio_underruns();
    if (hold < 0 || u != seen || InterlockedExchange(&music_rehold, 0)) {
        int extra = (int)music_room_ms - 46;     /* (the ring's own 2048 frames at 44100) */
        seen = u;
        hold = extra > 0 ? (extra * 441 / 10 + frames - 1) / frames : 0;
    }
    if (hold > 0) { hold--; memset(out, 0, (size_t)frames * 4); return; }
    audio_fill(out, frames);
}
/* the sound's own thread: each block the device has played is filled again and handed back */
static DWORD WINAPI audio_thread(LPVOID p)
{
    int i;
    (void)p;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    for (;;) {
        WaitForSingleObject(audio_ev, INFINITE);
        for (i = 0; i < AUDIO_BLOCKS; i++)
            if (audio_hdr[i].dwFlags & WHDR_DONE) {
                audio_pull(audio_buf[i], AUDIO_FRAMES);
                audio_hdr[i].dwFlags &= ~WHDR_DONE;
                waveOutWrite(wo, &audio_hdr[i], sizeof audio_hdr[i]);
            }
    }
    return 0;
}

/* the sound device opened: 16-bit stereo at rate, four blocks of 512 frames queued, the refill thread started;
   -1 when the device cannot be opened (the game then runs silent) */
int plat_audio_open(int rate, void (*fill)(int16_t *, int))
{
    WAVEFORMATEX f;
    HANDLE th;
    int i;
    if (wo) return 0;
    memset(&f, 0, sizeof f);
    f.wFormatTag = WAVE_FORMAT_PCM;
    f.nChannels = 2;
    f.nSamplesPerSec = (DWORD)rate;
    f.wBitsPerSample = 16;
    f.nBlockAlign = 4;
    f.nAvgBytesPerSec = (DWORD)rate * 4;
    audio_fill = fill;
    audio_ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!audio_ev || waveOutOpen(&wo, WAVE_MAPPER, &f, (DWORD_PTR)audio_ev, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
        wo = NULL;
        return -1;
    }
    for (i = 0; i < AUDIO_BLOCKS; i++) {
        memset(&audio_hdr[i], 0, sizeof audio_hdr[i]);
        audio_hdr[i].lpData = (LPSTR)audio_buf[i];
        audio_hdr[i].dwBufferLength = sizeof audio_buf[i];
        waveOutPrepareHeader(wo, &audio_hdr[i], sizeof audio_hdr[i]);
        audio_fill(audio_buf[i], AUDIO_FRAMES);
        waveOutWrite(wo, &audio_hdr[i], sizeof audio_hdr[i]);
    }
    th = CreateThread(NULL, 1 << 20, audio_thread, NULL, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
    if (!th) return -1;
    CloseHandle(th);
    return 0;
}

/* ---- the window ---- */

/* the window made as large as needed for a client area (the inside, without frame and title) of w x h */
static void resize_client(int w, int h)
{
    RECT r = { 0, 0, w, h };
    DWORD st = (DWORD)GetWindowLongW(g_win, GWL_STYLE), ex = (DWORD)GetWindowLongW(g_win, GWL_EXSTYLE);
    AdjustWindowRectEx(&r, st, FALSE, ex);
    SetWindowPos(g_win, NULL, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

/* the pointer captured (held inside the window, the cursor hidden) or let go; never in a hidden run */
static void set_capture(int on)
{
    if (!g_win || uwf_offscreen()) return;
    if (on) {
        RECT r;
        GetClientRect(g_win, &r);
        MapWindowPoints(g_win, NULL, (POINT *)&r, 2);
        ClipCursor(&r);
        if (!cursor_hidden) { ShowCursor(FALSE); cursor_hidden = 1; }
    } else {
        ClipCursor(NULL);
        if (cursor_hidden) { ShowCursor(TRUE); cursor_hidden = 0; }
    }
    if (getenv("UWPORT_LOOKDBG")) {
        /* (inputcheck visible's: the system's real clip, GetClipCursor, beside the client's rectangle on the screen and
           the virtual screen's, and whether the cursor shows, GetCursorInfo's CURSOR_SHOWING: 1 shown, 0 hidden. Read
           only: the probe used to call ShowCursor twice to read the display count, which it must not touch; the
           final review's finding 4) */
        RECT c, r;
        CURSORINFO ci;
        int n;
        memset(&ci, 0, sizeof ci);
        ci.cbSize = sizeof ci;
        n = GetCursorInfo(&ci) ? (ci.flags & CURSOR_SHOWING) != 0 : -1;
        GetClipCursor(&c);
        GetClientRect(g_win, &r);
        MapWindowPoints(g_win, NULL, (POINT *)&r, 2);
        RLOG_I("win32", "clip: set %d: clip %ld,%ld,%ld,%ld client %ld,%ld,%ld,%ld screen %d,%d,%d,%d cursor %d at %lu ms",
                on, c.left, c.top, c.right, c.bottom, r.left, r.top, r.right, r.bottom,
                GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
                GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN),
                GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN), n,
                (unsigned long)(ms_now() - L.start));
    }
}

/* a key message to the game as the keyboard's set-1 scan code (Ctrl+F10 lets a locked pointer go; Print Screen and
   Pause send their own sequences); 1: handled here */
static int key_msg(WPARAM vk, LPARAM lp, int down)
{
    if (uwf_no_input()) return 1;
    int c = (int)((lp >> 16) & 0xFF) | ((lp & (1 << 24)) ? 0x100 : 0);
    int repeat = down && (lp & (1 << 30));
    if (live.mouse_lock && vk == VK_F10 && (GetKeyState(VK_CONTROL) & 0x8000)) {
        if (down) uwf_mouse_unlock();
        return 1;
    }
    if (!settings_open() && special_key((int)vk, down)) return 1;
    if (vk == VK_SHIFT && c == 0x36) c = 0x36;  /* right shift: no E0 */
    if (vk == VK_NUMLOCK) c = 0x45;            /* Num Lock comes with the extended bit; the keyboard sends no E0 */
    uwf_key(c, down, repeat);
    return 1;
}

/* the window's messages: close, keys, pointer and wheel, raw mouse motion, focus, size, dropped files, painting */
static LRESULT CALLBACK wnd_proc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_CLOSE:
        if (hooks->lifecycle) hooks->lifecycle(PLAT_QUIT_REQUEST);
        L.quit = 1;
        return 0;
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        if (m == WM_SYSKEYDOWN && wp == VK_F4) break;      /* Alt+F4: the window's close */
        return key_msg(wp, lp, 1) ? 0 : DefWindowProcW(w, m, wp, lp);
    case WM_KEYUP: case WM_SYSKEYUP:
        return key_msg(wp, lp, 0) ? 0 : DefWindowProcW(w, m, wp, lp);
    case WM_SYSCHAR: case WM_CHAR:
        return 0;
    case WM_MOUSEMOVE:
        uwf_pointer(PLAT_POINTER_MOVE, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), 0);
        {   /* the game's cursor stands in for the host's over the picture (uwf_hover: -1 not now) */
            int over = uwf_hover(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            if (over < 0) return 0;
            if (over != cursor_hidden) { ShowCursor(over ? FALSE : TRUE); cursor_hidden = over; }
            TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, w, 0 };
            TrackMouseEvent(&t);
        }
        return 0;
    case WM_MOUSEWHEEL:   /* the message box, the menu's list, a conversation's scroll (uwf_wheel) */
        uwf_wheel(GET_WHEEL_DELTA_WPARAM(wp));
        return 0;
    case WM_MOUSELEAVE:
        if (cursor_hidden && uwf_pointer_free()) { ShowCursor(TRUE); cursor_hidden = 0; }
        return 0;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
        SetCapture(w);
        uwf_pointer(PLAT_POINTER_DOWN, GET_X_LPARAM(lp), GET_Y_LPARAM(lp),
                    m == WM_LBUTTONDOWN ? PLAT_BUTTON_LEFT : m == WM_RBUTTONDOWN ? PLAT_BUTTON_RIGHT : PLAT_BUTTON_MIDDLE);
        return 0;
    case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP:
        if (!(wp & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON))) ReleaseCapture();
        uwf_pointer(PLAT_POINTER_UP, GET_X_LPARAM(lp), GET_Y_LPARAM(lp),
                    m == WM_LBUTTONUP ? PLAT_BUTTON_LEFT : m == WM_RBUTTONUP ? PLAT_BUTTON_RIGHT : PLAT_BUTTON_MIDDLE);
        return 0;
    case WM_INPUT: {
        RAWINPUT ri;
        UINT sz = sizeof ri;
        if (GetRawInputData((HRAWINPUT)lp, RID_INPUT, &ri, &sz, sizeof(RAWINPUTHEADER)) != (UINT)-1
            && ri.header.dwType == RIM_TYPEMOUSE && !(ri.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
            uwf_raw_motion(ri.data.mouse.lLastX, ri.data.mouse.lLastY);
        break;
    }
    case WM_KILLFOCUS: {
        /* every press held let go through its owner (uwinput), mouse-look wanted off and the pointer let go at once
           (uwf_focus); on the focus's return the want is ours again */
        if (getenv("UWPORT_LOOKDBG")) {   /* (where the focus went: the window that takes it, its process, its class) */
            HWND to = (HWND)wp, fg = GetForegroundWindow();
            DWORD pid = 0;
            char cls[64] = "";
            if (fg) GetWindowThreadProcessId(fg, &pid), GetClassNameA(fg, cls, sizeof cls);
            RLOG_I("win32", "focus: lost to window %p; the foreground window %p (pid %lu, class %s) at %lu ms%s",
                    (void *)to, (void *)fg, (unsigned long)pid, cls, (unsigned long)(ms_now() - L.start),
                    focus_test_msg ? " (focus test)" : "");
        }
        uwf_focus(0);
        return 0;
    }
    case WM_SETFOCUS:
        uwf_focus(1);
        return 0;
    case WM_SIZE:
        if (wp == SIZE_MINIMIZED) {
            L.hidden_win = 1;
            if (hooks->lifecycle) hooks->lifecycle(PLAT_SUSPEND);
        } else if (L.hidden_win) {
            L.hidden_win = 0;
            if (hooks->lifecycle) hooks->lifecycle(PLAT_RESUME);
        }
        if (wp != SIZE_MINIMIZED) uwf_resized();    /* the pointer held in the window's new rectangle */
        return 0;
    case WM_DROPFILES: {
        char path[MAX_PATH];
        if (hooks->drop && DragQueryFileA((HDROP)wp, 0, path, sizeof path)) hooks->drop(path);
        DragFinish((HDROP)wp);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(w, &ps);
        EndPaint(w, &ps);
        return 0;
    }
    default:
        break;
    }
    return DefWindowProcW(w, m, wp, lp);
}

/* ---- the settings screen's live options ---- */

/* the largest whole scale (1 to 8) of the 320 x 200 picture (240 high with aspect) whose window fits the work area */
int plat_max_scale(int aspect)
{
    RECT wa;
    int s, fw, fh;
    RECT r = { 0, 0, 320, 200 };
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0)) return 8;
    AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, 0);
    fw = (r.right - r.left) - 320, fh = (r.bottom - r.top) - 200;
    for (s = 8; s > 1; s--)
        if (320 * s + fw <= wa.right - wa.left && (aspect ? 240 : 200) * s + fh <= wa.bottom - wa.top) break;
    return s;
}

static WINDOWPLACEMENT g_wpprev = { sizeof g_wpprev };   /* where the window was before full screen */
/* full screen: the frame taken off and the window over the whole monitor; off: the frame and the old place back */
static void set_fullscreen(int on)
{
    if (uwf_offscreen()) return;
    DWORD st = (DWORD)GetWindowLongW(g_win, GWL_STYLE);
    if (on && (st & WS_OVERLAPPEDWINDOW)) {
        MONITORINFO mi = { sizeof mi };
        if (GetWindowPlacement(g_win, &g_wpprev) && GetMonitorInfoW(MonitorFromWindow(g_win, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLongW(g_win, GWL_STYLE, (LONG)(st & ~WS_OVERLAPPEDWINDOW));
            SetWindowPos(g_win, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    } else if (!on && !(st & WS_OVERLAPPEDWINDOW)) {
        SetWindowLongW(g_win, GWL_STYLE, (LONG)(st | WS_OVERLAPPEDWINDOW));
        SetWindowPlacement(g_win, &g_wpprev);
        SetWindowPos(g_win, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

/* With our renderer the window has a size of its own (UWPORT_SIZE, default 1920x1080: the user's; the screen's work
   area at most), not 320x200 times a whole scale: the 4:3 picture fills its height, scaled freely, and the 3D view
   is drawn at the window's own pixels. 0 width: the original's scales. */
static int big_w, big_h;
static void big_window_size(void)
{
    char path[MAX_PATH];
    const char *e = getenv("UWPORT_SIZE"), *base = plat_base_dir(), *gdi = getenv("EXHUME_TEST_CHEATS") ? getenv("UWPORT_GDI") : NULL;   /* (a check's only, as uwfront.c's view_start) */
    RECT wa, r = {0, 0, 640, 480};
    int w = 1920, h = 1080, fw, fh;
    big_w = big_h = 0;
    if ((gdi && *gdi && *gdi != '0') || !base) return;
#ifdef UWPORT_PLAIN
    return;   /* uw-original.exe: the original's own picture (its 320 x 200 screen, drawn with GDI), no renderer */
#endif
    snprintf(path, sizeof path, "%suwview.dll", base);
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) return;   /* no renderer: the original's scales */
    if (e && *e && sscanf(e, "%dx%d", &w, &h) != 2) return;
    if (w < 320 || h < 200) return;
    if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0)) {
        AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, 0);
        fw = (r.right - r.left) - 640, fh = (r.bottom - r.top) - 480;
        if (w + fw > wa.right - wa.left) w = wa.right - wa.left - fw;
        if (h + fh > wa.bottom - wa.top) h = wa.bottom - wa.top - fh;
    }
    big_w = w, big_h = h;
}

/* the settings screen's display options taken: full screen, the scale (kept within what fits), 4:3, whole-number scaling */
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
        if (big_w) resize_client(big_w, big_h);   /* our renderer's window keeps its size */
        else resize_client(320 * live.scale, (aspect ? 240 : 200) * live.scale);
    }
}

/* a left click at a point of the window, sent as the window's own messages (release: the button let go too) */
void plat_window_click(float x, float y, int release)
{
    LPARAM lp = MAKELPARAM((int)x, (int)y);
    if (!g_win) return;
    PostMessageW(g_win, WM_LBUTTONDOWN, MK_LBUTTON, lp);
    if (release) PostMessageW(g_win, WM_LBUTTONUP, 0, lp);
}

/* ---- the settings screen's folder picker (from the loop: the answer goes back on the next pass) ---- */

static volatile LONG pick_state;       /* 0 none, 1 a folder in pick_path, 2 cancelled */
static char pick_path[1024];

/* Windows' folder picker: 0 and the chosen folder in out, -1 when none was chosen */
static int browse_folder(HWND owner, const char *title, char *out, size_t outsz)
{
    BROWSEINFOA bi;
    PIDLIST_ABSOLUTE id;
    char path[MAX_PATH];
    int ok = 0;
    memset(&bi, 0, sizeof bi);
    bi.hwndOwner = owner;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    id = SHBrowseForFolderA(&bi);
    if (id) {
        if (SHGetPathFromIDListA(id, path) && strlen(path) < outsz) { strcpy(out, path); ok = 1; }
        CoTaskMemFree(id);
    }
    return ok ? 0 : -1;
}

/* the settings screen's "choose a folder": the picker shown, its answer left in pick_state for the loop */
static int start_pick(const char *start)
{
    (void)start;
    if (!g_win) return -1;
    /* modal on the main thread: the loop waits while it is open, as the game's clock is paused by the
       settings screen anyway */
    InterlockedExchange(&pick_state, browse_folder(g_win, "Choose a folder", pick_path, sizeof pick_path) == 0 ? 1 : 2);
    return 0;
}

/* UWPV_VEH=1 (a check's; registered by plat_run before our renderer is loaded): a fault's address, its module and the offset in it, before anything else sees the fault */
static LONG CALLBACK veh_where(EXCEPTION_POINTERS *x)
{
    DWORD c = x->ExceptionRecord->ExceptionCode;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == EXCEPTION_STACK_OVERFLOW || c == EXCEPTION_ILLEGAL_INSTRUCTION) {
        HMODULE m = NULL;
        char name[MAX_PATH] = "?";
        void *a = x->ExceptionRecord->ExceptionAddress;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)a, &m);
        if (m) GetModuleFileNameA(m, name, sizeof name);
        fprintf(stderr, "VEH: code %08lx at %p = %s + 0x%llx, thread %lu, reading %p\n", (unsigned long)c, a, name,
                (unsigned long long)((char *)a - (char *)m), (unsigned long)GetCurrentThreadId(),
                x->ExceptionRecord->NumberParameters > 1 ? (void *)x->ExceptionRecord->ExceptionInformation[1] : NULL);
        {   /* the return addresses on the stack, near the fault (a rough backtrace) */
            void **sp = (void **)x->ContextRecord->Rsp;
            for (int i = 0, n = 0; i < 400 && n < 12; i++) {
                HMODULE mm = NULL;
                if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)sp[i], &mm) && mm == GetModuleHandleA(NULL))
                    fprintf(stderr, "VEH:   stack exe + 0x%llx\n", (unsigned long long)((char *)sp[i] - (char *)mm)), n++;
            }
        }
        fflush(stderr);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
/* the picture (uwf_frame: our renderer presented it, else its pixels) with GDI */
static void present(HDC dc)
{
    int i, cw, ch, sw, sh;
    BITMAPINFO bi;
    const uint32_t *src = uwf_frame(&sw, &sh);
    if (!src) return;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = sw;
    bi.bmiHeader.biHeight = -sh;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    uwf_client_size(&cw, &ch);
    /* the bars round the picture black */
    PatBlt(dc, 0, 0, cw, L.dst.top, BLACKNESS);
    PatBlt(dc, 0, L.dst.bottom, cw, ch - L.dst.bottom, BLACKNESS);
    PatBlt(dc, 0, L.dst.top, L.dst.left, L.dst.bottom - L.dst.top, BLACKNESS);
    PatBlt(dc, L.dst.right, L.dst.top, cw - L.dst.right, L.dst.bottom - L.dst.top, BLACKNESS);
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc, L.dst.left, L.dst.top, L.dst.right - L.dst.left, L.dst.bottom - L.dst.top, 0, 0, sw, sh,
                  src, &bi, DIB_RGB_COLORS, SRCCOPY);
    if (L.shot == 1 && live.window_shot_path) {
        /* the window's contents as presented */
        HDC mdc = CreateCompatibleDC(dc);
        HBITMAP bm = CreateCompatibleBitmap(dc, cw, ch);
        uint8_t *bgra = malloc((size_t)cw * (size_t)ch * 4), *rgbp = malloc((size_t)cw * (size_t)ch * 3);
        if (mdc && bm && bgra && rgbp) {
            BITMAPINFO gb;
            HGDIOBJ old = SelectObject(mdc, bm);
            BitBlt(mdc, 0, 0, cw, ch, dc, 0, 0, SRCCOPY);
            SelectObject(mdc, old);
            memset(&gb, 0, sizeof gb);
            gb.bmiHeader = bi.bmiHeader;
            gb.bmiHeader.biWidth = cw, gb.bmiHeader.biHeight = -ch;
            GetDIBits(mdc, bm, 0, (UINT)ch, bgra, &gb, DIB_RGB_COLORS);
            for (i = 0; i < cw * ch; i++)
                rgbp[i * 3] = bgra[i * 4 + 2], rgbp[i * 3 + 1] = bgra[i * 4 + 1], rgbp[i * 3 + 2] = bgra[i * 4];
            if (plat_write_png_rgb(live.window_shot_path, rgbp, cw, ch) == 0)
                RLOG_I("win32", "wrote %s (%dx%d, the window)", live.window_shot_path, cw, ch);
        }
        free(bgra), free(rgbp);
        if (bm) DeleteObject(bm);
        if (mdc) DeleteDC(mdc);
        L.shot = 2;
    }
}

/* (2026-11-18, the speed's research) UWPORT_TIMING=1: a step of the loop longer than 50 ms is named in the log with
   the time of each of its parts, to find what holds the window's thread */
static double loop_ms(void) { LARGE_INTEGER c, f; QueryPerformanceCounter(&c), QueryPerformanceFrequency(&f); return (double)c.QuadPart * 1000.0 / (double)f.QuadPart; }
/* one pass of the main thread's loop: the checks' timed events, the window's messages, the input, the game's
   picture, then the frame shown; 1 when the program is to end */
static int loop_step(void)
{
    MSG msg;
    static int timing = -1;
    if (timing < 0) { const char *e = getenv("UWPORT_TIMING"); timing = e && *e && *e != '0'; }
    double t[8];
    t[0] = loop_ms();
    uwf_tick_begin();   /* (the game's mouse-look turned on or off: the capture follows it now) */
    uwf_tick_tests();   /* (the checks' UWPORT_FOCUSTEST and UWPORT_MOUSETEST: uwfront.c) */
    t[1] = loop_ms();
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) L.quit = 1;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    t[2] = loop_ms();
    uwf_tick_input();   /* (the runtime's tick, the settings screen, the input's step, our taps' breaks) */
    {
        LONG ps = InterlockedExchange(&pick_state, 0);
        if (ps) settings_folder_chosen(ps == 1 ? pick_path : NULL);
    }
    t[3] = loop_ms();
    uwf_tick_scan();    /* (the game's picture, the screenshots, a timed run's end) */
    t[4] = loop_ms();
    if (game_done) L.quit = 1;
    if (!g_win || L.hidden_win) {
        Sleep(16);
        t[5] = t[6] = loop_ms();
    } else {
        HDC dc = GetDC(g_win);
        present(dc);
        ReleaseDC(g_win, dc);
        L.presents++;
        t[5] = loop_ms();
        if (!uwf_view_ok() && FAILED(DwmFlush())) Sleep(10);  /* the compositor's frame: the loop's pace (the renderer's
                                                       present waits for the display itself) */
        t[6] = loop_ms();
    }
    if (timing && t[6] - t[0] > 50)
        RLOG_I("win32", "a slow loop step at %lu ms: %.0f ms (tests %.0f, messages %.0f, input %.0f, the game's picture %.0f, "
                "the frame %.0f, the compositor %.0f)", (unsigned long)(ms_now() - L.start), t[6] - t[0], t[1] - t[0], t[2] - t[1],
                t[3] - t[2], t[4] - t[3], t[5] - t[4], t[6] - t[5]);
    return L.quit;
}

/* the exe started by itself (a console or none: not Play.cmd's redirection, not a check's pipe): everything printed goes
   to home\logs\uw-new.log as Play.cmd's does (rotated past 5 MB), with the mouse-look trace on (UWPORT_LOOKDBG), so a
   problem met in play leaves its evidence (the user, 2026-11-12: the mouse after a new character, an assertion at exit) */
/* Started from a terminal, the log's lines are shown there too, as they are written (the user, 2026-11-16: the
   few lines seen in "the terminal window where I start it" were all that came before the log took over; what a
   start is doing belongs there). The log stays the file, written straight (a crash loses nothing); a thread reads
   the file from this start's first line and copies what is added to the terminal: ours, the renderer's, the map
   maker's. At the program's end what is left is copied before it goes. */
static HANDLE term_out = INVALID_HANDLE_VALUE, term_file = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION term_lock;
/* what the log file has gained since the last time, copied to the terminal */
static void term_copy(void)
{
    char buf[4096];
    DWORD got, put;
    EnterCriticalSection(&term_lock);
    while (ReadFile(term_file, buf, sizeof buf, &got, NULL) && got) WriteFile(term_out, buf, got, &put, NULL);
    LeaveCriticalSection(&term_lock);
}
static DWORD WINAPI term_thread(LPVOID arg)
{
    (void)arg;
    for (;;) term_copy(), Sleep(50);
}
static void term_last(void) { fflush(stdout), fflush(stderr), term_copy(); }
/* the terminal copy started: the log file read from byte from on, by a thread every 50 ms, and once more at exit */
static void term_start(const char *path, HANDLE out, LONGLONG from)
{
    LARGE_INTEGER at;
    HANDLE th;
    term_file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (term_file == INVALID_HANDLE_VALUE) { CloseHandle(out); return; }
    term_out = out;
    at.QuadPart = from;
    SetFilePointerEx(term_file, at, NULL, FILE_BEGIN);
    InitializeCriticalSection(&term_lock);
    atexit(term_last);
    if ((th = CreateThread(NULL, 0, term_thread, NULL, 0, NULL)) != NULL) CloseHandle(th);
}
/* stderr and stdout sent to home\logs\uwr.log (the old log moved to uwr.1.log past 5 MB), unless they already go to
   a file or a pipe, or the run is hidden. Its lines stay plain fprintf: stderr is being redirected while they run */
static void log_start(void)
{
    HANDLE e = GetStdHandle(STD_ERROR_HANDLE);
    DWORD t = e && e != INVALID_HANDLE_VALUE ? GetFileType(e) : FILE_TYPE_UNKNOWN;
    const char *home = plat_home();
    char dir[MAX_PATH], path[MAX_PATH], old[MAX_PATH];
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (t == FILE_TYPE_DISK || t == FILE_TYPE_PIPE || !home || getenv("UWPORT_OFFSCREEN")) return;
    snprintf(dir, sizeof dir, "%s\\logs", home);
    CreateDirectoryA(dir, NULL);
    snprintf(path, sizeof path, "%s\\uwr.log", dir), snprintf(old, sizeof old, "%s\\uwr.1.log", dir);
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &fa) && (fa.nFileSizeHigh || fa.nFileSizeLow > 5u << 20))
        MoveFileExA(path, old, MOVEFILE_REPLACE_EXISTING);
    fprintf(stderr, PLAT_NAME ": the log is %s\n", path);
    fflush(stdout), fflush(stderr);
    HANDLE term = INVALID_HANDLE_VALUE;   /* the terminal the game was started from, kept (freopen closes ours) */
    if (t == FILE_TYPE_CHAR && !DuplicateHandle(GetCurrentProcess(), e, GetCurrentProcess(), &term, 0, FALSE, DUPLICATE_SAME_ACCESS))
        term = INVALID_HANDLE_VALUE;
    if (!freopen(path, "a", stderr)) return;
    setvbuf(stderr, NULL, _IONBF, 0);
    if (term != INVALID_HANDLE_VALUE) {
        WIN32_FILE_ATTRIBUTE_DATA now_fa;
        term_start(path, term, GetFileAttributesExA(path, GetFileExInfoStandard, &now_fa) ? ((LONGLONG)now_fa.nFileSizeHigh << 32 | now_fa.nFileSizeLow) : 0);
    }
    if (freopen(path, "a", stdout)) setvbuf(stdout, NULL, _IONBF, 0);
    if (!getenv("UWPORT_LOOKDBG")) _putenv("UWPORT_LOOKDBG=1");
    {
        time_t now = time(NULL);
        char ts[64];
        strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(stderr, "\n==== uwr started %s ====\n", ts);
    }
}
/* 2026-11-17: the window while our renderer starts (three or four seconds; a first run makes the relief maps first,
   some seconds to a minute or two): black with a line of text, not the unpainted white it was. Drawn once with GDI
   before the renderer has the window; the renderer's first frame covers it. Not when the window is never shown. */
static void start_note(void)
{
    static const WCHAR l1[] = L"Starting\x2026";
    static const WCHAR l2[] = L"The first start prepares the game's pictures. That can take up to a minute or two.";
    RECT r, a;
    HDC dc;
    HFONT f1, f2, old;
    if (!g_win || uwf_offscreen() || !IsWindowVisible(g_win) || !GetClientRect(g_win, &r) || r.bottom < 64) return;
    if (!(dc = GetDC(g_win))) return;
    FillRect(dc, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
    SetBkMode(dc, TRANSPARENT);
    f1 = CreateFontW(-(r.bottom / 22), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    f2 = CreateFontW(-(r.bottom / 44), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    old = SelectObject(dc, f1);
    SetTextColor(dc, RGB(222, 190, 110));   /* gold */
    a = r, a.bottom = r.bottom / 2 - r.bottom / 40;
    DrawTextW(dc, l1, -1, &a, DT_CENTER | DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, f2);
    SetTextColor(dc, RGB(150, 150, 140));
    a = r, a.top = r.bottom / 2 + r.bottom / 40;
    DrawTextW(dc, l2, -1, &a, DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, old);
    DeleteObject(f1), DeleteObject(f2);
    ReleaseDC(g_win, dc);
    GdiFlush();
}

/* The backend's main: the log, the window (made, shown unless hidden, our renderer and sound started), the input,
   the game's thread started, then the loop till the end; the game's exit code */
int plat_run(const PlatConfig *cfg0, const PlatHooks *h, int (*game)(void *), void *arg)
{
    int scale = cfg0->scale > 0 ? cfg0->scale : 3;
    HANDLE gt;
    hooks = h;
    live = *cfg0;
    if (!live.hidden) log_start();
    RLOG_I("win32", "mouse: %s", live.mouse_lock ? "lock" : "follow");
    settings_pick_folder = start_pick;
    SetProcessDPIAware();
    timeBeginPeriod(1);
    if (!live.hidden) {
        WNDCLASSEXW wc;
        HINSTANCE inst = GetModuleHandleW(NULL);
        RECT r;
        wchar_t title[256];
        RAWINPUTDEVICE rid;
        if (scale > plat_max_scale(live.aspect)) scale = plat_max_scale(live.aspect);
        live.scale = scale;
        memset(&wc, 0, sizeof wc);
        wc.cbSize = sizeof wc;
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = wnd_proc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
        wc.lpszClassName = L"ExhumePort";
        RegisterClassExW(&wc);
        r.left = 0, r.top = 0, r.right = 320 * scale, r.bottom = (live.aspect ? 240 : 200) * scale;
        big_window_size();
        if (big_w) {
            r.right = big_w, r.bottom = big_h;
            live.integer_scale = 0;               /* the picture fills the height, scaled freely */
            RLOG_I("win32", "the window at %dx%d for our renderer", big_w, big_h);
        }
        AdjustWindowRectEx(&r, WS_OVERLAPPEDWINDOW, FALSE, 0);
        MultiByteToWideChar(CP_UTF8, 0, live.title ? live.title : PLAT_TITLE, -1, title, 256);
        g_win = CreateWindowExW(0, wc.lpszClassName, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
        if (!g_win) {
            RLOG_E("win32", "no window: error %lu", (unsigned long)GetLastError());
            return 1;
        }
        DragAcceptFiles(g_win, TRUE);
        rid.usUsagePage = 1, rid.usUsage = 2, rid.dwFlags = 0, rid.hwndTarget = g_win;
        RegisterRawInputDevices(&rid, 1, sizeof rid);
        {   /* UWPORT_OFFSCREEN=1 (the checks): the window made but never shown or activated, the renderer drawing
               into it unseen: nothing takes the user's focus */
            const char *off = getenv("UWPORT_OFFSCREEN");
            if (!(off && *off && *off != '0')) {
                ShowWindow(g_win, SW_SHOW);
                uwf_set_focused(GetForegroundWindow() == g_win);   /* started behind another window: no capture till it is chosen */
            }
        }
        if (live.fullscreen) set_fullscreen(1);
        if (live.mouse_lock) uwf_mouse_title(0);
        /* (2026-11-17: always, unless UWPV_VEH=0: a fault's place in the log; "host fault" alone told nothing) */
        { const char *e = getenv("UWPV_VEH"); if (!e || *e != '0') AddVectoredExceptionHandler(1, veh_where); }
        uwf_view_start();
        uwf_fx_audio_start();   /* fx_audio 1: OpenAL Soft (bin\soft_oal.dll); else nothing */
        RLOG_I("win32", "Win32 window, %s", uwf_view_loaded() ? "our renderer (Vulkan or D3D12: the line below)" : "GDI, paced by the compositor");
    }
    uwf_input_init();                  /* the window's input (uwinput.c) */
    targ[0] = (void *)game;
    targ[1] = arg;
    start_note();                      /* (said in the window: the renderer's start shows nothing by itself) */
    uwf_view_warm();                   /* the renderer up before the game's thread starts: nothing heard before a picture */
    L.start = ms_now();
    /* the game's thread gets a large stack: host frames are bigger than DOS's */
    gt = CreateThread(NULL, 16 << 20, game_thread, targ, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
    if (!gt) {
        RLOG_E("win32", "no game thread: error %lu", (unsigned long)GetLastError());
        return 1;
    }
    CloseHandle(gt);
    while (!loop_step()) {}
    uwf_shutdown();   /* (the language model, the capture, the renderer, our audio let go) */
    if (g_win) DestroyWindow(g_win);
    if (wo) waveOutReset(wo);
    timeEndPeriod(1);
    return game_status;
}

/* ---- dialogs, before plat_run ---- */

/* a message box (an error's, or for information), its text also in the log */
void plat_message(int error, const char *title, const char *text)
{
    rlog(error ? RLOG_ERROR : RLOG_INFO, "win32", "%s\n%s", title, text);
    MessageBoxA(NULL, text, title, MB_OK | (error ? MB_ICONERROR : MB_ICONINFORMATION));
}

/* the text asked first (OK or Cancel), then the folder picker: 0 and the folder in out, -1 none chosen */
int plat_choose_folder(const char *title, const char *text, char *out, size_t outsz)
{
    RLOG_I("win32", "%s\n%s", title, text);
    if (MessageBoxA(NULL, text, title, MB_OKCANCEL | MB_ICONINFORMATION) != IDOK) return -1;
    return browse_folder(NULL, title, out, outsz);
}

/* the exe's own folder, ending in '\'; NULL when Windows cannot say */
const char *plat_base_dir(void)
{
    static char dir[MAX_PATH];
    char *cut;
    if (!dir[0]) {
        if (!GetModuleFileNameA(NULL, dir, sizeof dir)) return NULL;
        cut = strrchr(dir, '\\');
        if (cut) cut[1] = 0;
    }
    return dir;
}

/* ---- the services our front end asks of the window (uwfront.h) ---- */

uint64_t uwp_ms(void) { return GetTickCount64(); }
void uwp_sleep_ms(unsigned ms) { Sleep(ms); }
long uwp_xchg(volatile long *p, long v) { return InterlockedExchange((volatile LONG *)p, v); }
long uwp_inc(volatile long *p) { return InterlockedIncrement((volatile LONG *)p); }
void *uwp_window(void) { return g_win; }
int uwp_window_size(int *w, int *h)
{
    RECT r = { 0, 0, 0, 0 };
    BOOL ok = GetClientRect(g_win, &r);
    *w = r.right - r.left, *h = r.bottom - r.top;
    return ok != 0;
}
void uwp_resize_client(int w, int h) { resize_client(w, h); }
void uwp_title(const char *t) { SetWindowTextA(g_win, t); }
void uwp_capture(int on) { set_capture(on); }
int uwp_cursor_hidden(void) { return cursor_hidden; }
void uwp_cursor_hide(int hide)
{
    if (hide) { if (!cursor_hidden) { ShowCursor(FALSE); cursor_hidden = 1; } }
    else if (cursor_hidden) { ShowCursor(TRUE); cursor_hidden = 0; }
}
int uwp_pointer_pos(int *x, int *y)
{
    POINT pt;
    if (!GetCursorPos(&pt) || !ScreenToClient(g_win, &pt)) return 0;
    *x = pt.x, *y = pt.y;
    return 1;
}
/* (capture_sync's hold on the original's options panel: the client rectangle on the screen, clipped again only when
   that moved; the first time the pointer put in its middle) */
int uwp_clip_hold(int x0, int y0, int x1, int y1, int first, int *mx, int *my)
{
    static RECT last;
    RECT pr;
    int moved = 0;
    pr.left = x0, pr.top = y0, pr.right = x1, pr.bottom = y1;
    MapWindowPoints(g_win, NULL, (POINT *)&pr, 2);
    if (first || !EqualRect(&pr, &last)) ClipCursor(&pr);
    if (first) {
        POINT m = { (pr.left + pr.right) / 2, (pr.top + pr.bottom) / 2 };
        SetCursorPos(m.x, m.y);
        ScreenToClient(g_win, &m);
        *mx = m.x, *my = m.y, moved = 1;
    }
    last = pr;
    return moved;
}
void uwp_clip_free(void) { ClipCursor(NULL); }
int uwp_ctrl_down(void) { return GetKeyState(VK_CONTROL) < 0; }
void uwp_close(void) { PostMessageW(g_win, WM_CLOSE, 0, 0); }
void uwp_new_game(void)
{
    static WCHAR cmd[4096];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si), si.cb = sizeof si;
    _snwprintf(cmd, 4096, L"%s --menu", GetCommandLineW());
    cmd[4095] = 0;
    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread), CloseHandle(pi.hProcess);
        PostMessageW(g_win, WM_CLOSE, 0, 0);
    }
}
void uwp_repaint(void) { InvalidateRect(g_win, NULL, FALSE); }
/* UWPORT_FOCUSTEST's focus lost (away) or back, as Alt-Tab gives the window: its own message, tagged for the log */
void uwp_test_focus(int away)
{
    focus_test_msg = 1;
    SendMessageW(g_win, away ? WM_KILLFOCUS : WM_SETFOCUS, 0, 0);
    focus_test_msg = 0;
}
void *uwp_lib_open(const char *path) { return (void *)LoadLibraryA(path); }
void *uwp_lib_sym(void *lib, const char *name) { return (void *)GetProcAddress((HMODULE)lib, name); }
void uwp_lib_close(void *lib) { FreeLibrary((HMODULE)lib); }
