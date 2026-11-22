/* uwfront.c: our front end over the platform backend (plat_win32.c or plat_sdl3.c; the seam between them is
   uwfront.h). Everything of ours that is not the window system: which screen the game shows, the routing of keys and
   pointer through uwinput.c (the settings screen, our Esc menu, our inventory panel, the world's clicks, a centred
   piece of the original's, the game), mouse-look's want and capture, the attack's flick and the throw's momentum,
   the hotkeys, the renderer DLL's function table (uwview.dll) and the picture it is given each pass, and our
   effects' audio. The window is reached only through the uwp_* services; no windows.h here. Log tag: front.
   Not through rlog, on purpose: the in: trace lines and the fx: lines (check scripts match them by their exact
   start), and the AI: lines on stdout (the AI's own log stream). */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include "plat.h"
#include "ui/settings.h"
#include "uwgame.h"
#include "uwinv.h"
#include "uwlook.h"
#include "uwlog.h"
#include "uwconv.h"
#include "uwaudio.h"
#include "sys/enhance.h"
#include "uwinput.h"
#include "uwfront.h"
#include "rlog.h"
#define UWF_PATH 260                   /* (Win32's MAX_PATH: the paths' buffers as they were) */
/* the MS C runtime's _stricmp for ASCII (the C locale's tolower): 0 when the two are the same but for case */
static int uwf_stricmp(const char *a, const char *b)
{
    unsigned char x, y;
    do {
        x = (unsigned char)*a++, y = (unsigned char)*b++;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
    } while (x && x == y);
    return x - y;
}
/* the runtime's (port.h, not included: its mouse_event is also the Windows API's) */
int port_config_get(const char *home, const char *key, char *out, size_t outsz);
int port_config_set(const char *home, const char *key, const char *value);
int port_hunter_setting(int v);   /* the game's hunter.c: llm_hunters read (v < 0) or set, taken at once */
int port_hunter_usable(void);    /* 0 smart hunting off, 1 ready, 2 no language model */
/* the creatures' AI levels (the game's cunning.c): 0 Original, 1 Devious, 2 Monstrous; Monstrous's parts
   (0 hit and run, 1 calls, 2 pack courage, 3 fall back, 4 strike together, 5 calls passed on, 6 the messages); part
   7 the friendly creatures (Devious and Monstrous); who decides (0 the rules, 1 the language model). v < 0 reads,
   else sets, taken at once and kept */
int port_ai_level(int v);
int port_ai_part(int part, int v);
int port_ai_by_model(int v);
/* the parts 8 to 17 are the rows MO_AIX0 to MO_AIX9: 8 hears you run, 9 searches the ways out, 10 more
   ambushers, 11 wary after a fight (Devious); 12 fetches kin, 13 the lure, 14 the cut-off, 15 fighting roles, 16 the
   alarm, 17 hunters bring the pack (Monstrous). A part's row is shown from its own level up: cun_part_level */
int cun_part_level(int part);
enum { MO_AIX0 = 150, MO_AIX1, MO_AIX2, MO_AIX3, MO_AIX4, MO_AIX5, MO_AIX6, MO_AIX7, MO_AIX8, MO_AIX9,
       /* the third round, the parts 18 to 24: takes cover, picks its ground, runs out of sight, learns how you fight
          (Devious); won't be drawn out, death cry, surround (Monstrous) */
       MO_AIX10, MO_AIX11, MO_AIX12, MO_AIX13, MO_AIX14, MO_AIX15, MO_AIX16, MO_AIXEND = MO_AIX16,
       MO_AILOGFILE = 129 };            /* ai_log_file: the AI's lines always into the log file (its own id: after
                                           MO_AIXEND it would be 167, the first enhancement's) */
unsigned cun_fired_count(int part);     /* how often the part was used this session */
int port_ai_logfile(int v);
int port_feud_mode(int v);              /* the row Rival creatures fight (underworld-exhumed port\sys\feud.c): 0 Off, 1 Rare, 2 Sometimes */
/* a part's row value: On / Off, and how often it was used this session once it was */
static void ai_part_value(char *b, int part)
{
    unsigned n = cun_fired_count(part);
    if (n) snprintf(b, 128, "%s (used %u)", port_ai_part(part, -1) ? "On" : "Off", n);
    else snprintf(b, 128, "%s", port_ai_part(part, -1) ? "On" : "Off");
}   /* (apart from the rows 0-97, MO_SLOT 100-103 and the dials from 196) */
/* the language model's own settings (uwmindhost.c): 0 the graphics card or the processor, 1 grammar or scoring,
   2 questions a second, 3 ms before an answer is too old (0 never), 4 minutes idle before it is let go (0 never) */
int port_mind_tune(int which, int v);
static const int LLM_RATES[] = {1, 2, 5, 10, 20, 30, 40, 60};
static const int LLM_STALES[] = {1000, 2000, 3000, 5000, 8000, 12000, 0};
static const int LLM_IDLES[] = {1, 2, 5, 10, 20, 30, 60, 0};
/* the next (step > 0) or the one before of a list's values, from the value now (one not in the list: the nearest) */
static int list_step(const int *list, int n, int now, int step)
{
    int at = 0, i;
    for (i = 0; i < n; i++) if (abs(list[i] - now) < abs(list[at] - now)) at = i;
    for (i = 0; i < n; i++) if (list[i] == now) at = i;
    return list[(at + (step > 0 ? 1 : n - 1)) % n];
}
static void our_action(int a, int down);
/* our Esc menu (below) */
static int menu_open, menu_sel, menu_hover = -1, menu_mx, menu_my;
static void menu_choose(int row);
static void ctrl_key(int c);
static void menu_choose_step(int row, int step);
static int fight_mode, display_mode = -1;
static int full_play(void);
static void game_key(int c, int down);
static void send_key(uint8_t make);
/* the rune shelf and the active spells for our HUD: PANELS.C's set_runes and active_spells hand them here on the game thread (port_hud_runes / _spells); the DLL's uwv_hud_magic draws them */
static volatile unsigned char hud_runes[3] = {0x18, 0x18, 0x18}, hud_spells[3] = {0xFF, 0xFF, 0xFF};
static void (*hud_magic_fn)(const unsigned char *runes, const unsigned char *spells);
static void (*hud_combat_fn)(int eyes, int pow);   /* uwv_hud_combat: our gargoyle's glow and the swing indicator */
static void (*hud_vitals_fn)(const int *v);        /* uwv_hud_vitals: health, mana, poison for the panel's beakers */
void port_hud_runes(const unsigned char *runes) { for (int i = 0; i < 3; i++) hud_runes[i] = runes[i]; }
void port_hud_spells(const unsigned char *spells) { for (int i = 0; i < 3; i++) hud_spells[i] = spells[i]; }
static int world_click(int type, unsigned button);
static void motion_note(float dx, float dy);   /* the pointer's motion, for a throw's momentum */
static int focused = 1;               /* the window has the keyboard's focus (no WM_KILLFOCUS since the last WM_SETFOCUS) */
int port_mouse_look(void);             /* ui/MOUSE.C: the game's mouse-look is on */
#ifndef UWPORT_PLAIN
void port_mouse_look_want(int on);     /* ui/MOUSE.C (uw-new): the mouse-look we want, applied at the game's key poll (-1: no wish) */
#endif
/* the window's input (uwinput.c): its state now, each frame's step (held presses their owners may no longer keep let
   go, mouse-look's want, the capture) */
static void input_state(UwiState *s);
static void input_sync(void);
static void capture_sync(void);
static int shift_ours(void);
/* our inventory panel (Tab in our display; uwinv.c, src/uwinv_draw.c): open, with the character sheet beside it */
static int inv_open, inv_sheet = 1;
static int inv_pointer(int type, int cx, int cy, unsigned button);
static void inv_toggle(void);
static void inv_pump(void);
static void inv_draw(void);

void port_pause(int on);               /* the runtime's sys/pit.c */

const PlatHooks *hooks;
PlatConfig live;                       /* the options as they stand: plat_set_display and plat_set_mouse_lock change it */
static int fade_secs(void);           /* the message box's fade, seconds (the Esc menu's option) */
static int door_step(int v);          /* a door shut from close up shuts all the same (the Esc menu's option) */
static int door_pick(int v);          /* a door picked by its drawn frame and leaf (the Esc menu's option) */
static int door_cue(int v);           /* the door's "in the way" message line (the Esc menu's option) */
static int log_back;                 /* the message box scrolled back this many lines (the wheel) */
static unsigned long long log_touch;  /* the wheel last used: the box shown again */
static struct { int on, obj, have, n; char buf[4]; } ask;   /* a stack's count asked in our message box (ask_begin) */
static int ask_key(int c, int down, int repeat);
static int host_pointer, cut_now;   /* our display shows the host's pointer; a cutscene is up (no pointer) */
static uint64_t panel_left;         /* a conversation, the map or the options panel just closed (uwconv.c): the original fades
                                       its 3D picture back in before its view is 'on screen' again; ours stays up meanwhile */
extern int port_host_pointer;       /* MOUSE.C, port only: the game's usual pointer not drawn */
extern int port_stack_whole;        /* INVPANEL.C, port only: a stack taken whole (no 'Move how many?') */
static int locked, captured;                  /* the pointer lock option's capture, mouse-look's (the game's
                                                  port_mouse_look() as last seen); the host's cursor hidden: the backend's */
static int clip_on;                            /* the pointer held in the window now (set_capture(1)) */
static unsigned buttons;                       /* the buttons the game saw go down */
static float last_x, last_y;                   /* where the game's pointer last was */
static int left_down;   /* the left button as the settings screen last saw it; cleared when the screen opens */
static int was_open, scaled_up;
static volatile long capture_req;      /* plat_pointer_capture's request: 0 none, 1 capture, 2 release */

void plat_pointer_capture(int on) { uwp_xchg(&capture_req, on ? 1 : 2); }

/* ---- the screen the game shows (uw-new's port/sys/screen.c tells it: port_screen), for the window thread ----
   The values are screen.h's (kept here as numbers so the plain build, whose game has no screen.c, builds too). */
enum { PS_BOOT = 0, PS_MENU, PS_GAME, PS_MAP, PS_CONV, PS_CUT_WINDOW, PS_CUT_FULL, PS_DEATH, PS_ENDING, PS_CHARGEN, PS_TEXT };
_Static_assert(UWI_PS_BOOT == PS_BOOT && UWI_PS_MENU == PS_MENU && UWI_PS_GAME == PS_GAME && UWI_PS_MAP == PS_MAP &&
               UWI_PS_CONV == PS_CONV && UWI_PS_CUT_WINDOW == PS_CUT_WINDOW && UWI_PS_CUT_FULL == PS_CUT_FULL &&
               UWI_PS_DEATH == PS_DEATH && UWI_PS_ENDING == PS_ENDING && UWI_PS_CHARGEN == PS_CHARGEN && UWI_PS_TEXT == PS_TEXT,
               "uwinput.h's screen states are screen.h's");
_Static_assert(UWI_PTR_MOVE == PLAT_POINTER_MOVE && UWI_PTR_DOWN == PLAT_POINTER_DOWN && UWI_PTR_UP == PLAT_POINTER_UP,
               "uwinput.h's pointer events are the runtime's");
static volatile long screen_state;    /* PS_BOOT until the game says */
static volatile long fade_state;       /* the death's fade to black: 0 none, 1 running, 2 done (black) */
static volatile long view_epoch;       /* one more each time the game's view is gone for good (PS_MENU, PS_CHARGEN, PS_DEATH):
                                          our last frame (view_world's snapshot) and "seen our view" (view_present) are
                                          forgotten, so a new game's first frames never show the last game's scene */
static uint64_t fade_t0;
static int screen_now(void) { return (int)screen_state; }
static void dbg_ms(const char *what, int v);   /* (below L: "... N at MS ms" under UWPORT_LOOKDBG) */
void plat_screen(int ps)
{
    uwp_xchg(&screen_state, ps);
    if (ps != PS_DEATH) uwp_xchg(&fade_state, 0);
    if (ps == PS_MENU || ps == PS_CHARGEN || ps == PS_DEATH) uwp_inc(&view_epoch);   /* that game's view is gone */
    dbg_ms("screen", ps);
}
int plat_death_ours(void);              /* (below: our display with the renderer up) */
void plat_death_fade_start(void) { fade_t0 = uwp_ms(); uwp_xchg(&fade_state, 1); dbg_ms("the death fade", 0); }
int plat_death_fade(void) { return fade_state == 1 && uwp_ms() - fade_t0 < 1500; }   /* (a deadline: no frame composing must not hang the game) */

/* ---- keys: set-1 codes, 0x100 for the E0 prefix ---- */

/* UWPORT_INPUTTRACE=1 (the input rewrite's trace comparison, tools\inputcheck.py trace): every byte and every pointer
   event the window's thread gives the game, at the one place each goes through (key_byte, to_game) */
static int input_trace(void)
{
    static int v = -1;
    if (v < 0) { const char *e = getenv("UWPORT_INPUTTRACE"); v = e && *e && *e != '0'; }
    return v;
}
static unsigned key_bytes_sent;        /* (a count: "the focus lost: ... let go" when a release sent any) */
/* a byte to the game's keyboard (the trace line stays raw: inputcheck.py matches it whole, ^in: ...$) */
static void key_byte(uint8_t b)
{
    if (input_trace()) fprintf(stderr, "in: key %02X\n", b);
    key_bytes_sent++;
    hooks->key(b);
}

/* a key down or up to the game (uwinput's game_key, and ours: a Ctrl shortcut, the charge's Space) */
static void game_key(int c, int down)
{
    if (!hooks || !hooks->key || !(c & 0x7F)) return;
    if (c & 0x100) key_byte(0xE0);
    key_byte((uint8_t)((c & 0x7F) | (down ? 0 : 0x80)));
}

/* the settings screen's key for a set-1 code, -1 none */
static int set_key_of(int c)
{
    switch (c) {
    case 0x148: return SET_KEY_UP;
    case 0x150: return SET_KEY_DOWN;
    case 0x14B: return SET_KEY_LEFT;
    case 0x14D: return SET_KEY_RIGHT;
    case 0x0F: return SET_KEY_TAB;
    case 0x1C: case 0x11C: case 0x39: return SET_KEY_ENTER;
    case 0x01: return SET_KEY_ESC;
    default: return -1;
    }
}

/* a pointer event to the game (its trace line raw, as key_byte's) */
static void to_game(const PlatPointer *p)
{
    if (input_trace()) fprintf(stderr, "in: ptr %d %.2f %.2f %u\n", p->type, p->x, p->y, p->button);
    key_bytes_sent++;
    hooks->pointer(p);
}

static void client_size(int *w, int *h);
static void mouse_title(int is_locked);

/* The settings screen opened or closed (plat_sdl3.c's sync_open) */
static void sync_open(void)
{
    int o = settings_open();
    if (o == was_open) return;
    was_open = o;
    if (o) {
        port_pause(1);
        left_down = 0;
        capture_sync();                 /* the pointer let go while the screen is up (and held again after) */
        if (uwp_window() && uwp_cursor_hidden()) uwp_cursor_hide(0);
        if (uwp_window() && live.scale < 2 && !live.fullscreen) {
            uwp_resize_client(640, live.aspect ? 480 : 400);
            scaled_up = 1;
        }
    } else {
        port_pause(0);
        if (uwp_window() && scaled_up) {
            scaled_up = 0;
            if (!live.fullscreen) uwp_resize_client(320 * live.scale, (live.aspect ? 240 : 200) * live.scale);
        }
        capture_sync();
        uwi_release_all();              /* every press held let go (the old release_all's) */
    }
}

/* One key of the window (set-1 code, 0x100 for E0), to its owner (uwinput.c: the settings screen, our menu, the
   world's key table, the game) */
static int view_full_ok(void);
static void interface_toggle(void);
static void handle_key(int c, int down, int repeat)
{
    static int f1_esc;   /* F1 on the original's options panel: given as Esc, its loop's KEY_ESC (close_option_panel) */
    UwiState s;
    if (ask.on && ask_key(c, down, repeat)) return;   /* a stack's count being typed (our message box) */
    if (c == 0x3B && down == 1 && !repeat && uwl_left_panel() == 1) f1_esc = 1;
    if (c == 0x3B && f1_esc) { c = 0x01; if (!down) f1_esc = 0; }
    input_state(&s);
    uwi_key(&s, c, down, repeat);
    if (c == 0x01 && down == 1 && !repeat && uwlog_instrument()) uwlog_instrument_clear();   /* (that Esc ends the playing:
                                                                     the cup's tune ends it without its message) */
}

/* ---- the window ---- */

struct loop L = { .w = 320, .hgt = 200 };

static uint64_t ms_now(void) { return uwp_ms(); }
/* a "what N at MS ms" line under UWPORT_LOOKDBG (the checks read them) */
static void dbg_ms(const char *what, int v)
{
    if (getenv("UWPORT_LOOKDBG")) RLOG_I("front", "%s %d at %lu ms", what, v, (unsigned long)(ms_now() - L.start));
}

static int view_w, view_h;   /* the renderer's pixels while it runs (V.cw, V.ch; set by view_size) */
static void client_size(int *w, int *h)
{
    if (view_w > 0 && view_h > 0) { *w = view_w, *h = view_h; return; }   /* the renderer's: the window stretches them */
    uwp_window_size(w, h);
}
/* a point in the window's own pixels into the renderer's (the window may be bigger: full screen) */
static void real_to_view(int *x, int *y)
{
    int rw, rh;
    if (view_w <= 0 || view_h <= 0 || !uwp_window_size(&rw, &rh) || rw <= 0 || rh <= 0) return;
    if (rw != view_w) *x = (int)((long long)*x * view_w / rw);
    if (rh != view_h) *y = (int)((long long)*y * view_h / rh);
}

/* the picture's place: the 4:3 shape when aspect is on, the largest that fits, whole multiples with
   integer_scale, centred (plat_sdl3.c's place) */
static void place(int w, int h)
{
    int ow, oh;
    float lw = (float)w, lh = live.aspect ? (float)w * 3.0f / 4.0f : (float)h, s;
    client_size(&ow, &oh);
    s = (float)ow / lw < (float)oh / lh ? (float)ow / lw : (float)oh / lh;
    if (live.integer_scale && s >= 1.0f) s = (float)(int)s;
    L.dst.left = (long)(((float)ow - lw * s) / 2);
    L.dst.top = (long)(((float)oh - lh * s) / 2);
    L.dst.right = L.dst.left + (long)(lw * s);
    L.dst.bottom = L.dst.top + (long)(lh * s);
    L.have_dst = L.dst.right > L.dst.left && L.dst.bottom > L.dst.top;
}

/* UWPORT_OFFSCREEN=1 (the checks): the window never shown, never full screen, and the real mouse never clipped or
   hidden, so that a check takes nothing from the user working at the machine */
static int offscreen(void)
{
    static int v = -1;
    if (v < 0) { const char *e = getenv("UWPORT_OFFSCREEN"); v = e && *e && *e != '0'; }
    return v;
}
/* the window's title with the mouse's state (captured: how to let it go) */
static void mouse_title(int is_locked)
{
    char t[256];
    snprintf(t, sizeof t, "%s - %s", live.title ? live.title : PLAT_TITLE,
             is_locked ? "Ctrl+F10 releases the mouse" : "click to capture the mouse");
    uwp_title(t);
}

/* a client point into the game's 320 x 200 picture */
static void to_picture(int cx, int cy, float *x, float *y)
{
    *x = (float)(cx - L.dst.left) * (float)L.w / (float)(L.dst.right - L.dst.left);
    *y = (float)(cy - L.dst.top) * (float)L.hgt / (float)(L.dst.bottom - L.dst.top);
}

/* a client point to the settings screen's pointer (its SET_W x SET_H) */
static void settings_at(int cx, int cy, int down)
{
    settings_pointer((cx - L.dst.left) * SET_W / (L.dst.right - L.dst.left),
                     (cy - L.dst.top) * SET_H / (L.dst.bottom - L.dst.top), down);
}

/* a pointer event from the window: client coordinates, the button that changed (0 for a move) */
/* UWPORT_NO_INPUT=1 (the checks): the window's own keys and pointer reach nothing, so that a replay in a window
   goes as recorded whatever the mouse over it does */
static int no_input(void)
{
    static int v = -1;
    if (v < 0) { const char *e = getenv("UWPORT_NO_INPUT"); v = e && *e && *e != '0'; }
    return v;
}

/* A pointer event of the window (client pixels; the button that changed, 0 for a move), to its owner (uwinput.c):
   the settings screen, our menu, our panel, the world (look, use, attack at the crosshair), a centred piece of the
   original's (a conversation, the map), else the game (pointer_game) */
static void pointer_msg(int type, int cx, int cy, unsigned button)
{
    UwiState s;
    if (no_input()) return;
    real_to_view(&cx, &cy);
    input_state(&s);
    if (type == PLAT_POINTER_MOVE) uwi_move(&s, cx, cy);
    else uwi_button(&s, button, type == PLAT_POINTER_DOWN, cx, cy);
}

/* the game's own screen gets a window pointer event (uwinput's GAME owner, and CENTRED's: the pointer lock's first
   click, the original's point of the window's, on a centred piece the point on it) */
static unsigned uwc_pressed;            /* buttons pressed on a centred panel: their release at the panel's point */
static void pointer_game(int type, int cx, int cy, unsigned button)
{
    PlatPointer p;
    float x, y;
    if (!hooks->pointer || !L.have_dst) return;
    memset(&p, 0, sizeof p);
    if (live.mouse_lock && !locked) {
        if (type == PLAT_POINTER_DOWN) {
            locked = 1;
            capture_sync();
            L.swallow |= button;
            mouse_title(1);
        }
        return;
    }
    if (type == PLAT_POINTER_UP && (L.swallow & button)) { L.swallow &= ~button; return; }
    if (type == PLAT_POINTER_MOVE && (locked || captured)) return;   /* the raw input's motion instead */
    if (uwc_shown()) {                  /* a conversation in our display: the original's point on the piece under the pointer */
        /* our answers (uwconv.c, step A2): a left press on one gives the game its number's key, as typing it does
           (conv_play_menu(option) in either case): our lines wrap otherwise than the original's, so its row under the
           pointer may be another answer's */
        int answer = type == PLAT_POINTER_DOWN && button == PLAT_BUTTON_LEFT ? uwc_answer_at(cx, cy) : 0;
        if (answer > 0) {
            if (getenv("UWPORT_CONVLOG")) RLOG_I("front", "conv: our answer %d clicked", answer);
            send_key((uint8_t)(1 + answer));   /* scan codes 02..0A: the keys 1..9 */
            L.swallow |= button;
            return;
        }
        if (type == PLAT_POINTER_DOWN) uwc_pressed |= button;
        else if (type == PLAT_POINTER_UP) uwc_pressed &= ~button;
        if (!uwc_point(cx, cy, &x, &y)) {
            if (type == PLAT_POINTER_DOWN) { L.swallow |= button; return; }   /* off the pieces: nothing (its release too) */
            if (type == PLAT_POINTER_MOVE) return;
            x = last_x, y = last_y;                                        /* a drag let go off them: where it last was */
        }
    } else if (type == PLAT_POINTER_UP && (uwc_pressed & button)) {
        /* the panel closed on the press (the map's close button): the release where the press was, as the original's
           screen had it, not the window's point mapped onto the 3D screen (which reopened the map) */
        uwc_pressed &= ~button;
        x = last_x, y = last_y;
    } else to_picture(cx, cy, &x, &y);
    p.type = type;
    if (type == PLAT_POINTER_MOVE) {
        p.dx = x - last_x;
        p.dy = y - last_y;
    } else {
        p.button = button;
        buttons = type == PLAT_POINTER_DOWN ? buttons | button : buttons & ~button;
    }
    p.x = x, p.y = y;
    p.buttons = buttons;
    p.absolute = !locked && !captured;
    last_x = x, last_y = y;
    to_game(&p);
}

/* an input script's pointer event (plat_pointer_event) on its way through uwinput: the game's own (its 320 x 200
   point, its motion) as before, or our menu's and the settings screen's */
static const PlatPointer *script_ev;
/* a script's pointer event to the game as it is, with the buttons kept */
static void script_to_game(const PlatPointer *ev)
{
    PlatPointer p = *ev;
    if (p.type == PLAT_POINTER_DOWN) buttons |= p.button;
    else if (p.type == PLAT_POINTER_UP) buttons &= ~p.button;
    p.buttons = buttons;
    if (p.absolute) { last_x = p.x; last_y = p.y; }
    to_game(&p);
}

/* a captured pointer's motion (raw input, in the mouse's counts as the host's pointer moves) */
static void raw_motion(long dx, long dy)
{
    PlatPointer p;
    if (no_input()) return;
    motion_note((float)dx, (float)dy);   /* the throw's momentum */
    if (!hooks->pointer || !L.have_dst || settings_open() || menu_open || !(locked || captured)) return;
    /* raw motion is uwinput's one exception (uwinput.h): relative, with no point, it goes straight to the game while
       the pointer is captured; but in our display not while mouse-look is no longer wanted (our panel, the options
       panel, the Tab or R interface just opened: the frame until the game applies the want off), unless the pointer
       lock option holds it. (The original's display and GDI alone: the ` key's mouse-look, as before.) */
    if (display_mode == 0 && !locked) {
        UwiState s;
        input_state(&s);
        if (!uwi_look_wanted(&s)) return;
    }
    memset(&p, 0, sizeof p);
    p.type = PLAT_POINTER_MOVE;
    p.dx = (float)dx * (float)L.w / (float)(L.dst.right - L.dst.left);
    p.dy = (float)dy * (float)L.hgt / (float)(L.dst.bottom - L.dst.top);
    p.x = last_x, p.y = last_y;
    p.buttons = buttons;
    p.absolute = 0;
    to_game(&p);
}

static void menu_wheel_pass(int notches);   /* the wheel to our menu (after the renderer's table, V) */

/* the pointer lock option set: the capture and the window's title follow */
void plat_set_mouse_lock(int on)
{
    live.mouse_lock = on != 0;
    RLOG_I("front", "mouse: %s", on ? "lock" : "follow");
    if (!uwp_window()) return;
    if (!on && locked) {
        locked = 0;
        capture_sync();
    }
    if (on) mouse_title(locked);
    else uwp_title(live.title ? live.title : PLAT_TITLE);
}

/* ---- input from an input script (inscript.c) ---- */

/* an input script's key byte (set-1, after an E0 prefix byte) to its owner */
void plat_key_byte(uint8_t b)
{
    static int e0;
    int c;
    if (b == 0xE0) { e0 = 1; return; }
    c = (b & 0x7F) | (e0 ? 0x100 : 0);
    e0 = 0;
    if (!hooks) return;
    handle_key(c, !(b & 0x80), 0);
}

/* a script's pointer event: through uwinput as the window's are, but only our menu (its 320 x 200 point as the
   window's would be), the settings screen and the game ever take it (as before: never our panel, the world's
   buttons or a centred piece; the game gets it as it is, its motion and its absolute point) */
void plat_pointer_event(const PlatPointer *ev)
{
    UwiState s;
    int cx = 0, cy = 0;
    if (!hooks || !hooks->pointer) return;
    if (uwp_window()) {
        int crw = 0, crh = 0;
        uwp_window_size(&crw, &crh);
        cx = (int)(ev->x * (crw / 320.0f)), cy = (int)(ev->y * (crh / 200.0f));
        real_to_view(&cx, &cy);
    }
    input_state(&s);
    s.panel = 0, s.full_live = 0;       /* (the world's and the panel's never: the game's, as before) */
    script_ev = ev;
    if (ev->type == PLAT_POINTER_MOVE) uwi_move(&s, cx, cy);
    else uwi_button(&s, ev->button, ev->type == PLAT_POINTER_DOWN, cx, cy);
    script_ev = NULL;
}

/* ---- our D3D12 renderer (uwview.dll, built from uo-new's src/render.c): when it is beside the program, the
   picture goes through it (as its interface layer over the scene); else GDI. UWPORT_GDI=1 keeps GDI. ---- */

/* the level for the sound, as the renderer gives it (src\uwview.c's structs, repeated: the DLL has no header here;
   docs\superpowers\plans\2026-11-14-acoustics-ours.md, Task 2) */
typedef struct { unsigned char type, height; unsigned short wall_tex, floor_tex; } UwvAcTile;
typedef struct { int id, tx, tz, heading, item, picture, shut; } UwvAcDoor;
typedef struct { int item; float lo[3], hi[3]; } UwvAcBox;   /* a 3D model object's box in the world (uwv_acoustic_things) */

static struct view {
    void *dll;
    int (*init)(void *hwnd, int w, int h);
    void (*ui_size)(int *w, int *h);
    int (*frame)(const unsigned char *rgba);
    void (*shutdown)(void);
    const char *(*status)(void);
    const unsigned char *(*compose)(const unsigned int *src, int sw, int sh, const unsigned char *clear, int x0, int y0, int x1, int y1);
    const unsigned char *(*hud)(const unsigned int *src, int sw, int sh);
    void (*log)(const char *lines, int n, int w, float alpha, int more_up);   /* our message box (uwv_log) */
    void (*fight)(int on);   /* fight mode: the weapon shown (uwv_fight) */
    int (*menu)(int mode, int sel, int mx, int my);
    void (*mouse)(float *sensitivity, int *invert);
    void (*toggle)(int what);
    void (*readout)(const char *text);
    void (*readout2)(const char *name, const char *hint, int cursor);
    void (*held)(int item);
    int (*pick)(float fx, float fy, int *out, int max);
    int (*record)(int seconds);
    int (*recording)(void);
    void (*clip_shot)(void);
    int (*option)(int id, int step, char *value, int n);
    int (*inventory)(const UwiSnap *s, int mx, int my, int sheet);
    unsigned (*pointer)(unsigned *out, int cap, int *w, int *h, int *hx, int *hy);   /* our panels' pointer picture (uwv_pointer) */
    int (*inv_hit)(const UwiSnap *s, int mx, int my, int sheet, float *fx, float *fy);
    void (*inv_wheel)(int notches);           /* the open bags' rows scrolled (the wheel over our panel: up positive) */
    int (*inv_click)(int mx, int my, int down); /* a left press / release on their slider: 1 taken */
    int (*menu_rows)(const char *title, const char *const *labels, const char *const *values, int n, int sel, int mx, int my);
    /* the same with a line a row saying what it does, shown at the menu's foot (NULL in an older DLL) */
    int (*menu_rows2)(const char *title, const char *const *labels, const char *const *values, const char *const *hints, int n, int sel, int mx, int my);
    void (*menu_wheel)(int notches);          /* the menu's list scrolled (the wheel: up positive) */
    int (*menu_click)(int mx, int my, int down); /* a left press / release on the menu's scrollbar: 1 taken */
    /* the Fog and smoke page (all NULL in an older DLL: no page): the rows' sliders drawn and under the
       pointer (uwv_menu_sliders, uwv_menu_slider), and the rows themselves (src\volfog.inc's uwv_vol_*) */
    void (*menu_sliders)(const float *frac, int n);
    int (*menu_slider)(int mx, int my, int row, float *frac);
    int (*vol_rows)(void);
    int (*heat_spots)(float *out, int max);   /* (the sound's heat shimmer; optional) */
    int (*vol_row)(int i, const char **label, const char **hint, int *kind, float *frac);
    int (*vol_value)(int i, int step, char *value, int n);
    int (*vol_set)(int i, const char *text);
    int (*vol_set_frac)(int i, float frac);
    const char *(*vol_preset_name)(int i);    /* the Fog and smoke tab's presets (uwv_vol_preset_name, uwv_vol_preset) */
    int (*vol_preset)(int set);
    /* the menu's tabs (NULL in an older DLL: no strip, the tabs by the keys alone): their names and the
       open one before the rows, and the tab under the pointer (uwv_menu_tabs, uwv_menu_tab_at) */
    void (*menu_tabs)(const char *const *names, int n, int open);
    int (*menu_tab_at)(int mx, int my);
    float (*fov)(void);
    void (*want_shot)(void);
    int (*readback)(unsigned char *rgba);
    int (*level)(const unsigned char *block, int n);
    int (*level_state)(const unsigned char *block, int n);
    void (*level_wait)(int on);
    void (*camera)(float x, float y, float z, float yaw, float pitch);
    void (*roll)(float roll);
    void (*palette)(const unsigned char *pal);
    void (*view_rect)(int x0, int y0, int x1, int y1, int w, int h);
    void (*light)(int level);
    void (*lens)(float tan_x, float tan_y);
    void (*shade_scale)(float k);
    void (*fx_event)(int kind, float x, float y, float z, int fx, int mob);   /* the effects (uwv_fx_event) */
    int (*ac_grid)(UwvAcTile out[64 * 64], int *ceil_tex);   /* the level for the sound (uwv_acoustic_*) */
    int (*ac_doors)(UwvAcDoor *out, int cap);
    int (*seen)(unsigned char out[64 * 64]);   /* what the player can see this frame, for our map (uwv_seen; optional) */
    int (*ac_things)(UwvAcBox *out, int cap);   /* (optional: an older renderer has none) */
    unsigned (*ac_epoch)(void);
    int ok, cw, ch;                     /* started, at this client size */
    int failed;                         /* frames that did not go: the renderer is given up for GDI */
    int uw, uh;                         /* the interface layer's size */
    unsigned char *ui;                  /* its pixels */
} V;

/* the renderer chosen (docs\superpowers\plans\2026-11-15-vulkan-renderer.md): UWPORT_RENDERER, else the port's settings
   ("renderer", the Esc menu's Renderer row, at the next start), else Vulkan, the default. 1 Vulkan (uwview_vk.dll),
   0 only when d3d12 is asked for by name */
static int view_vk;             /* the renderer loaded is the Vulkan one */
static int renderer_wanted(void)
{
    char v[16] = "";
    const char *e = getenv("UWPORT_RENDERER");
    if (e && *e) snprintf(v, sizeof v, "%s", e);
    else if (!plat_home() || port_config_get(plat_home(), "renderer", v, sizeof v) != 0) v[0] = 0;
    return uwf_stricmp(v, "d3d12") != 0;
}
static void view_open(const char *dll);
/* the renderer DLL loaded: Vulkan when wanted, Direct3D 12 the fallback on Windows; none when GDI is asked for */
static void view_start(void)
{
    /* UWPORT_GDI: a check's only (the original's own picture to compare ours with), taken with EXHUME_TEST_CHEATS=1.
       A player's ways back to the original's picture are two: the Esc menu's Display row,
       and the renderer not starting (view_size) */
    const char *gdi = getenv("EXHUME_TEST_CHEATS") ? getenv("UWPORT_GDI") : NULL;
    if ((gdi && *gdi && *gdi != '0') || !plat_base_dir()) return;
#ifdef UWPORT_PLAIN
    return;   /* uw-original.exe: the original's own picture (its 320 x 200 screen, drawn with GDI), no renderer */
#endif
    view_vk = 0;
#ifndef _WIN32
    /* Linux: the Vulkan renderer only (uo-new's tools\buildview_linux.py), beside the program; without it, or when it
       cannot start (view_size), the original's picture */
    view_open("libuwview.so");
    view_vk = V.dll != NULL;
    if (!V.dll) RLOG_W("front", "the Vulkan renderer (libuwview.so) is not here or did not load: the original's picture");
#else
    if (renderer_wanted()) {
        view_open("uwview_vk.dll");
        if (V.dll) { view_vk = 1; return; }
        RLOG_W("front", "the Vulkan renderer (uwview_vk.dll) is not here or did not load: Direct3D 12");
    }
    view_open("uwview.dll");
#endif
}
/* the DLL beside the program opened and its functions looked up into V; V cleared when one it needs is missing */
static void view_open(const char *dll)
{
    char path[UWF_PATH];
    snprintf(path, sizeof path, "%s%s", plat_base_dir(), dll);
    V.dll = uwp_lib_open(path);
    if (!V.dll) return;
    V.init = (int (*)(void *, int, int))uwp_lib_sym(V.dll, "uwv_init");
    V.ui_size = (void (*)(int *, int *))uwp_lib_sym(V.dll, "uwv_ui_size");
    V.frame = (int (*)(const unsigned char *))uwp_lib_sym(V.dll, "uwv_frame");
    V.shutdown = (void (*)(void))uwp_lib_sym(V.dll, "uwv_shutdown");
    V.status = (const char *(*)(void))uwp_lib_sym(V.dll, "uwv_status");
    V.compose = (const unsigned char *(*)(const unsigned int *, int, int, const unsigned char *, int, int, int, int))uwp_lib_sym(V.dll, "uwv_compose");
    V.hud = (const unsigned char *(*)(const unsigned int *, int, int))uwp_lib_sym(V.dll, "uwv_hud");
    hud_magic_fn = (void (*)(const unsigned char *, const unsigned char *))uwp_lib_sym(V.dll, "uwv_hud_magic");
    hud_combat_fn = (void (*)(int, int))uwp_lib_sym(V.dll, "uwv_hud_combat");
    hud_vitals_fn = (void (*)(const int *))uwp_lib_sym(V.dll, "uwv_hud_vitals");
    V.log = (void (*)(const char *, int, int, float, int))uwp_lib_sym(V.dll, "uwv_log");
    V.fight = (void (*)(int))uwp_lib_sym(V.dll, "uwv_fight");
    V.menu = (int (*)(int, int, int, int))uwp_lib_sym(V.dll, "uwv_menu");
    V.mouse = (void (*)(float *, int *))uwp_lib_sym(V.dll, "uwv_mouse");
    V.toggle = (void (*)(int))uwp_lib_sym(V.dll, "uwv_toggle");
    V.readout = (void (*)(const char *))uwp_lib_sym(V.dll, "uwv_readout");
    V.readout2 = (void (*)(const char *, const char *, int))uwp_lib_sym(V.dll, "uwv_readout2");
    V.held = (void (*)(int))uwp_lib_sym(V.dll, "uwv_held");
    V.pick = (int (*)(float, float, int *, int))uwp_lib_sym(V.dll, "uwv_pick");
    V.record = (int (*)(int))uwp_lib_sym(V.dll, "uwv_record");
    V.recording = (int (*)(void))uwp_lib_sym(V.dll, "uwv_recording");
    V.clip_shot = (void (*)(void))uwp_lib_sym(V.dll, "uwv_clipboard_shot");
    V.option = (int (*)(int, int, char *, int))uwp_lib_sym(V.dll, "uwv_option");
    V.menu_rows = (int (*)(const char *, const char *const *, const char *const *, int, int, int, int))uwp_lib_sym(V.dll, "uwv_menu_rows");
    V.menu_rows2 = (int (*)(const char *, const char *const *, const char *const *, const char *const *, int, int, int, int))uwp_lib_sym(V.dll, "uwv_menu_rows2");
    V.menu_wheel = (void (*)(int))uwp_lib_sym(V.dll, "uwv_menu_wheel");
    V.menu_click = (int (*)(int, int, int))uwp_lib_sym(V.dll, "uwv_menu_click");
    V.menu_sliders = (void (*)(const float *, int))uwp_lib_sym(V.dll, "uwv_menu_sliders");
    V.menu_slider = (int (*)(int, int, int, float *))uwp_lib_sym(V.dll, "uwv_menu_slider");
    V.vol_rows = (int (*)(void))uwp_lib_sym(V.dll, "uwv_vol_rows");
    V.heat_spots = (int (*)(float *, int))uwp_lib_sym(V.dll, "uwv_heat_spots");
    V.vol_row = (int (*)(int, const char **, const char **, int *, float *))uwp_lib_sym(V.dll, "uwv_vol_row");
    V.vol_value = (int (*)(int, int, char *, int))uwp_lib_sym(V.dll, "uwv_vol_value");
    V.vol_set = (int (*)(int, const char *))uwp_lib_sym(V.dll, "uwv_vol_set");
    V.vol_set_frac = (int (*)(int, float))uwp_lib_sym(V.dll, "uwv_vol_set_frac");
    V.vol_preset_name = (const char *(*)(int))uwp_lib_sym(V.dll, "uwv_vol_preset_name");
    V.vol_preset = (int (*)(int))uwp_lib_sym(V.dll, "uwv_vol_preset");
    V.menu_tabs = (void (*)(const char *const *, int, int))uwp_lib_sym(V.dll, "uwv_menu_tabs");
    V.menu_tab_at = (int (*)(int, int))uwp_lib_sym(V.dll, "uwv_menu_tab_at");
    V.inventory = (int (*)(const UwiSnap *, int, int, int))uwp_lib_sym(V.dll, "uwv_inventory");
    V.pointer = (unsigned (*)(unsigned *, int, int *, int *, int *, int *))uwp_lib_sym(V.dll, "uwv_pointer");
    V.inv_hit = (int (*)(const UwiSnap *, int, int, int, float *, float *))uwp_lib_sym(V.dll, "uwv_inv_hit");
    V.inv_wheel = (void (*)(int))uwp_lib_sym(V.dll, "uwv_inv_wheel");
    V.inv_click = (int (*)(int, int, int))uwp_lib_sym(V.dll, "uwv_inv_click");
    {   /* the panel only with a DLL of this exe's build: the same UwiSnap (a DLL with a larger one reads past the
           exe's snapshot, its words from the exe's mouse, and the game crashes) */
        int (*snap_size)(void) = (int (*)(void))uwp_lib_sym(V.dll, "uwv_snap_size");
        int ds = snap_size ? snap_size() : -1;
        if (ds != (int)sizeof(UwiSnap)) {
            RLOG_W("front", "uwview.dll is of another build (its inventory snapshot %d bytes, ours %d): our "
                   "inventory panel off, the original's interface instead", ds, (int)sizeof(UwiSnap));
            V.inventory = NULL, V.inv_hit = NULL;
        }
    }
    V.fov = (float (*)(void))uwp_lib_sym(V.dll, "uwv_fov");
    V.want_shot = (void (*)(void))uwp_lib_sym(V.dll, "uwv_want_shot");
    V.readback = (int (*)(unsigned char *))uwp_lib_sym(V.dll, "uwv_readback");
    V.level = (int (*)(const unsigned char *, int))uwp_lib_sym(V.dll, "uwv_level");
    V.level_state = (int (*)(const unsigned char *, int))uwp_lib_sym(V.dll, "uwv_level_state");
    V.level_wait = (void (*)(int))uwp_lib_sym(V.dll, "uwv_level_wait");
    V.camera = (void (*)(float, float, float, float, float))uwp_lib_sym(V.dll, "uwv_camera");
    V.roll = (void (*)(float))uwp_lib_sym(V.dll, "uwv_roll");
    V.palette = (void (*)(const unsigned char *))uwp_lib_sym(V.dll, "uwv_palette");
    V.view_rect = (void (*)(int, int, int, int, int, int))uwp_lib_sym(V.dll, "uwv_view_rect");
    V.light = (void (*)(int))uwp_lib_sym(V.dll, "uwv_light");
    V.lens = (void (*)(float, float))uwp_lib_sym(V.dll, "uwv_lens");
    V.shade_scale = (void (*)(float))uwp_lib_sym(V.dll, "uwv_shade_scale");
    V.fx_event = (void (*)(int, float, float, float, int, int))uwp_lib_sym(V.dll, "uwv_fx_event");
    V.ac_grid = (int (*)(UwvAcTile *, int *))uwp_lib_sym(V.dll, "uwv_acoustic_grid");
    V.seen = (int (*)(unsigned char *))uwp_lib_sym(V.dll, "uwv_seen");
    V.ac_doors = (int (*)(UwvAcDoor *, int))uwp_lib_sym(V.dll, "uwv_acoustic_doors");
    V.ac_things = (int (*)(UwvAcBox *, int))uwp_lib_sym(V.dll, "uwv_acoustic_things");
    V.ac_epoch = (unsigned (*)(void))uwp_lib_sym(V.dll, "uwv_acoustic_epoch");
    {
        void (*game_dir)(const char *) = (void (*)(const char *))uwp_lib_sym(V.dll, "uwv_game_dir");
        if (game_dir) game_dir(plat_data_root());   /* the game's files, for the renderer's data */
    }
    if (!V.init || !V.ui_size || !V.frame || !V.shutdown) { uwp_lib_close(V.dll); memset(&V, 0, sizeof V); return; }
    /* a conversation's pieces (uwconv.c), centred over our dimmed last frame (UWPORT_NOCONV=1 shows the original's
       whole screen instead, for a comparison) */
    uwc_bind(getenv("UWPORT_NOCONV") ? NULL : V.dll);
}
/* a backend's own drawing of our menu (plat_sdl3.c's uwmenu.c over src\menu_draw.c), for when our renderer is not up
   (Linux; Windows without uwview.dll): uwf_soft_menu; NULL none (plat_win32.c: no menu without the renderer, as before) */
static const UwfMenuFns *soft_menu;
void uwf_soft_menu(const UwfMenuFns *f) { soft_menu = f; }
static int soft_menu_up(void) { return !V.ok && soft_menu != NULL; }
/* the soft menu can open: the game's 3D screen in play, nothing of the original's centred over it */
static int soft_live(void) { return soft_menu_up() && screen_now() == PS_GAME && !uwc_shown(); }
/* the wheel to our menu: the renderer's, or the soft menu's */
static void menu_wheel_pass(int notches)
{
    if (V.menu_wheel) V.menu_wheel(notches);
    else if (soft_menu_up()) soft_menu->wheel(notches);
}
/* a left press or release on the menu's scrollbar (the renderer's, or the soft menu's): 1 taken */
static int menu_click_pass(int x, int y, int down)
{
    if (V.menu_click) return V.menu_click(x, y, down);
    return soft_menu_up() ? soft_menu->click(x, y, down) : 0;
}

/* The Esc menu's Log detail row (Game tab): how much goes into uwr.log, rlog's levels 0 Errors only to 3
   Everything; default Normal (2). Kept in uw1port.cfg (log_detail). The exe and the renderer DLL each have their own
   copy of rlog.c and so their own level: the row sets both, here with rlog_set_level and in the DLL through
   uwv_option OPT_LOG_DETAIL (step = the level + 1). When UWR_LOGLEVEL is set it wins at the start: the saved value
   is then not applied (the row still changes the level for the rest of the run). */
enum { OPT_LOG_DETAIL = 105 };
static const char *const LOG_DETAIL[4] = {"Errors only", "Warnings", "Normal", "Everything"};
static int log_detail_saved = -2;   /* -2: not read yet; -1: nothing to apply (nothing saved, or UWR_LOGLEVEL set) */
/* the saved level read once from uw1port.cfg and put in force in the exe (not when UWR_LOGLEVEL is set) */
static void log_detail_load(void)
{
    char v[16];
    const char *env = getenv("UWR_LOGLEVEL");
    if (log_detail_saved != -2) return;
    log_detail_saved = -1;
    if (env && env[0]) return;
    if (!plat_home() || port_config_get(plat_home(), "log_detail", v, sizeof v) != 0) return;
    if (v[0] < '0' || v[0] > '3' || v[1]) {
        RLOG_W("front", "uw1port.cfg log_detail %s is not 0 to 3: left at %s", v, LOG_DETAIL[rlog_level()]);
        return;
    }
    log_detail_saved = v[0] - '0';
    rlog_set_level(log_detail_saved);
}
/* a level handed to the renderer DLL's own logger */
static void log_detail_to_view(int level)
{
    char v[32];
    if (V.option) V.option(OPT_LOG_DETAIL, level + 1, v, sizeof v);
}

/* The Esc menu's "Map what I saw" (Game tab, 2026-11-21): the map screen shows every tile our picture let the player
   see (uwview.c uwv_seen: in the lens, a clear line, lit), kept by the game's side (underworld-exhumed port\sys\seenmap.c,
   in SAVE0\UWRMAP.DAT with the saves); Off: the original's map alone (its 16-deep, lit-only vision grid). Default On,
   uw1port.cfg map_seen. The game's own PlayersMap, its saves and the experience for seen tiles are not touched. */
extern int port_seen_on;
extern int port_map_arrows8, port_map_notes_fixed;   /* the rows Map level arrows, Map notes kept right (AUTOMAP.C) */
void port_seen_mark(int lev, const unsigned char *vis);
static void seen_mark_frame(void)
{
    static uint64_t last;
    static unsigned char vis[64 * 64];
    uint64_t now = ms_now();
    if (!V.seen || !port_seen_on || display_mode == 1 || now - last < 100) return;   /* (ten times a second is plenty) */
    last = now;
    int lev = V.seen(vis);
    if (lev) port_seen_mark(lev, vis);
}

/* (re)started at the window's client size; 0: GDI from now on */
static int view_size(void)
{
    int cw, ch;
    if (!V.dll) return 0;
    if (V.ok) return 1;   /* started: its size kept (the window stretches its picture; a restart cannot be made) */
    view_w = view_h = 0;
    client_size(&cw, &ch);
    if (cw < 64 || ch < 64) return V.ok;
    RLOG_I("front", "starting the renderer (%s) at %dx%d", view_vk ? "Vulkan" : "Direct3D 12", cw, ch);
    V.ok = V.init(uwp_window(), cw, ch);
    V.cw = cw, V.ch = ch;
#ifdef _WIN32
    if (!V.ok && view_vk) {   /* Vulkan did not start here: Direct3D 12 instead (the log says why) */
        RLOG_W("front", "the Vulkan renderer did not start: Direct3D 12");
        V.shutdown();
        uwp_lib_close(V.dll);
        memset(&V, 0, sizeof V);
        view_vk = 0;
        view_open("uwview.dll");
        if (!V.dll) return 0;
        V.ok = V.init(uwp_window(), cw, ch);
        V.cw = cw, V.ch = ch;
    }
#endif
    if (!V.ok) {
        rlog(RLOG_WARN, "front", "the %s renderer did not start: %s", view_vk ? "Vulkan" : "D3D12",   /* (rlog, not RLOG_W: #ifdef in the arguments) */
#ifdef _WIN32
                "GDI");
#else
                "the original's picture");
#endif
        V.shutdown();
        uwp_lib_close(V.dll);
        memset(&V, 0, sizeof V);
        return 0;
    }
    V.ui_size(&V.uw, &V.uh);
    view_w = cw, view_h = ch;
#ifdef UWPORT_GAMEVIEW
    uwg_gameview_replace(1);   /* the game leaves its 3D drawing out from its very first frame */
    uwg_scene_beat();          /* the display is alive from here: the gate waits for the first level's scene */
#endif
    free(V.ui);
    V.ui = calloc((size_t)V.uw * (size_t)V.uh, 4);
    if (!V.ui) {
        RLOG_E("front", "out of memory (%zu bytes) for the %dx%d interface layer", (size_t)V.uw * (size_t)V.uh * 4, V.uw, V.uh);
        V.ok = 0;
        return 0;
    }
    RLOG_I("front", "%s renderer at %dx%d, interface layer %dx%d (%s)", view_vk ? "Vulkan" : "D3D12", cw, ch, V.uw, V.uh,
           V.status ? V.status() : "");
    log_detail_load();   /* the saved Log detail, to the DLL's logger too */
    if (log_detail_saved >= 0) log_detail_to_view(rlog_level());
    return 1;
}

/* Our 3D view in the original's 3D window. The window's pixels that are still the original's last 3D frame (its
   frame buffer, as uwgame.c kept it when the game copied it to the screen) are left clear in the interface layer,
   so our scene shows through them; whatever the game drew over the frame (the cursor, an object on it) stays.
   The frame the screen shows can be a frame or two behind the newest, so the kept frame most like the screen is
   the one used, and only when nearly all of the window is it (else the window holds something else: a
   conversation, the map; the original's pixels are shown). The camera is that frame's player (PLAYMOVE.C's
   get_eye: PN, the eye 0xA4 above the feet; PlayerFacing and PlayerPitch, a full turn 0x10000; heading 0 north,
   east positive as MOTION.C's sin/cos; pitch up positive as MOUSE.C's mouse look). 1 if the view is ours now. */
static uint8_t clear_px[640 * 480];     /* per picture pixel: 1 where the layer is clear */
static int view_marks;                  /* how many of them the game left for our view in the last picture */

/* Full-screen play (with our renderer): while the game's 3D view is live and the interface is not asked for (Tab),
   our view fills the window with only the original's compass, flasks and message lines over it, and the mouse turns
   the view (the game's mouse-look, wanted on exactly while this play is up with nothing over it: input_sync).
   Tab: the original's whole screen with the pointer free (the inventory, the icons, the menus), Tab again back. */
static int iface;                     /* the interface asked for (Tab) */
/* The display, ours (0, the default) or the original's (1): the original's screen, its pointer and its keys, our
   renderer in its 3D window. Chosen in our Esc menu, kept in the port's settings ("display"). Ours turns on the
   port's mouse-look and modern-keys (enhance_on's bits), the original's turns them off. */

void mouse_look_speed(int pct);        /* the port's (sys/mousedrv.c): mouse-look's speed, a percentage */
int port_mouse_look(void);             /* ui/MOUSE.C: the game's mouse-look is on */
#ifndef UWPORT_PLAIN
extern int port_pitch_wide;            /* uw-new's game/PLAYER.C: wide-pitch's bound */
#endif
/* the original's display is chosen (before the first frame: as the settings have it): the sound's start asks */
static int display_orig(void)
{
#ifdef UWPORT_PLAIN
    return 0;   /* (uw-original.exe: as the command line has it) */
#else
    char v[16];
    if (display_mode >= 0) return display_mode == 1;
    return plat_home() && port_config_get(plat_home(), "display", v, sizeof v) == 0 && !strcmp(v, "original");
#endif
}
/* the display chosen (display_mode) applied: the enhancements, wide pitch, the door helpers, mouse-look's speed */
static void display_apply(void)
{
#ifdef UWPORT_PLAIN
    display_mode = 1;   /* uw-original.exe: the original's display; the enhancements as the command line has them */
    return;
#else
    /* wide-pitch (a much larger look up and down): the original's 0x1000 (22.5
       degrees) either way becomes 0x38E3, 80 degrees, in our display; the original's renderer draws what is behind
       for the steep looks (VIEW3D.C's back pass) and ours follows the eye's pitch (get_eye's cPlayer->pitch) */
    uint32_t ours = (1u << ENH_MOUSE_LOOK) | (1u << ENH_MODERN_KEYS) | (1u << ENH_RUNE_KEYS) | (1u << ENH_COMBINED_MOVES)
                  | (1u << ENH_WIDE_PITCH);
    port_pitch_wide = display_mode == 1 ? 0x3000 : 0x38E3;
    port_interact_take = display_mode != 1;   /* e takes what can be taken (port/ui/keys.c), as the old version's e */
    door_step(-1), door_pick(-1);             /* and the two door helpers */
    door_cue(-1);                             /* the door's message line as the config has it (EFFECT.C's port_door_cue_on) */
    /* The one switch: the original's display turns everything of ours off (the enhancements, the key bindings),
       ours turns it all back on. The original's: every enhancement off (creature AI, hunters and friends with
       them: smart-hunting is the flag their hooks test), the game's own 3D picture (view_present), its own sound (fx_audio_start), its half-second fades,
       its door rules (door_step, door_pick, door_cue). What was on is kept here and comes back with ours; nothing
       of it is written to the settings. A replay keeps its enhancements (they are the recording's). */
    static uint32_t enh_kept;
    static int enh_have, fades_kept = -1;
    extern int port_quick_fades;
    extern int16_t rp_request;
    if (display_mode == 1) {
        if (rp_request == 2) enhance_on &= ~(ours | (1u << ENH_INVERT_LOOK));
        else {
            if (!enh_have) enh_kept = enhance_on & ~(ours | (1u << ENH_INVERT_LOOK)), enh_have = 1;
            enhance_on = 0;
            if (fades_kept < 0) fades_kept = port_quick_fades;
            port_quick_fades = 0;
        }
    } else {
        float sens = 1;
        int inv = 0;
        if (enh_have) enhance_on = enh_kept, enh_have = 0;
        if (fades_kept >= 0) port_quick_fades = fades_kept, fades_kept = -1;
        enhance_on |= ours;
        if (V.mouse) V.mouse(&sens, &inv);    /* uw.exe's mouse_sensitivity and invert_mouse */
        mouse_look_speed((int)(sens * 100.0f + 0.5f));
        if (inv) enhance_on |= 1u << ENH_INVERT_LOOK;
        else enhance_on &= ~(1u << ENH_INVERT_LOOK);
    }
#endif
}
static int full_live;                 /* the 3D view live at the last frame */
/* a stack's count asked in our message box (Shift + a left click on a stack in our panel asks
   "how many" in the message box we have, not in the original's own typed dialog; the number typed and Enter puts that
   many on the pointer, carried). Digits (the top row or the number pad), Backspace, Enter takes, Esc or a click
   anywhere gives it up. The split itself is uwinv.c's take (UWI_OP_PICK with the count), the game's own bookkeeping. */
static void ask_begin(int obj, int have) { ask.on = 1, ask.obj = obj, ask.have = have, ask.n = 0, ask.buf[0] = 0; log_touch = ms_now(); }
static uint64_t inv_hold_t;
/* a key while the count is asked: 1 taken (every press but Shift's; the releases go on as ever) */
static int ask_key(int c, int down, int repeat)
{
    static const short PAD[10] = {0x52, 0x4F, 0x50, 0x51, 0x4B, 0x4C, 0x4D, 0x47, 0x48, 0x49};
    int d = -1;
    if (!down || c == 0x2A || c == 0x36) return 0;
    if (c == 0x0F || c == 0x17) { ask.on = 0; return 0; }   /* Tab, I: given up, and the key does what it does (the panel shuts) */
    if (c >= 0x02 && c <= 0x0A) d = c - 1;
    else if (c == 0x0B) d = 0;
    else for (int i = 0; i < 10; i++) if (c == PAD[i]) d = i;
    if (d >= 0) { if (ask.n < 3 && !(ask.n == 0 && d == 0)) ask.buf[ask.n++] = (char)('0' + d), ask.buf[ask.n] = 0; }
    else if (c == 0x0E) { if (ask.n > 0) ask.buf[--ask.n] = 0; }
    else if (c == 0x01) ask.on = 0;
    else if ((c == 0x1C || c == 0x11C) && !repeat) {
        int n = atoi(ask.buf);
        ask.on = 0;
        if (n > 0 && uwi_cmd(UWI_OP_PICK, ask.obj, n >= ask.have ? 0 : n, 0)) inv_hold_t = ms_now();
    }
    return 1;
}
/* our message box: the scroll's last lines, opaque while new, fading out from 10 s to 12 s after the newest message
   (or the wheel's last turn); at full while the Tab panel is open */
static void log_draw(void)
{
    static char lines[UWLOG_ROWS][UWLOG_W];
    static unsigned long long seen;
    unsigned long long newest, now = ms_now();
    int all, n;
    if (!V.log) return;
    {   /* UWPORT_LOGTEST=1 (the checks): the box filled once with sample lines, the AI log's among them */
        static int done;
        const char *e;
        if (!done && (e = getenv("UWPORT_LOGTEST")) && *e == '1') {
            static const char *const S[] = {
                "You see a goblin.\n", "The goblin is hostile.\n",
                "AI: goblin #218 at 10,38: sees the player, 3 squares, wounded -> goal=ambush (41 ms)\n",
                "You are currently well fed and wide awake.\n", "You are on the first level of the Abyss.\n",
                "It is the first day of your imprisonment.\n", "You guess that it is currently early evening.\n",
                "AI: hunter #77 plan: goto_door, wait, goto_last, search, goto_home, give_up\n",
                "You have no room in your pack for that.\n", "The newest line of the test.\n"};
            for (int i = 0; i < (int)(sizeof S / sizeof S[0]); i++) port_scroll_note(S[i]);
            done = 1;
        }
        (void)now;
    }
    n = uwlog_get(lines, UWLOG_ROWS, log_back, &newest, &all);
    if (newest != seen) seen = newest, log_back = 0, n = uwlog_get(lines, UWLOG_ROWS, 0, &newest, &all);   /* a new message: the newest shown */
    unsigned long long tick = ms_now(), last = newest > log_touch ? newest : log_touch;
    float F = (float)fade_secs(), age = (float)(tick - last) / 1000.0f;
    float a = F <= 0 ? 1.0f : age < F ? 1.0f : age < F + 1.5f ? 1.0f - (age - F) / 1.5f : 0.0f;   /* the option's time, then 1.5 s out */
    if (inv_open) a = 1.0f;
    (void)now;
    if (ask.on && (!inv_open || !full_play())) ask.on = 0;   /* (the panel shut, or play not up: given up) */
    if (ask.on) {   /* the count asked: the box's last line, at full */
        if (n == UWLOG_ROWS) memmove(lines[0], lines[1], (size_t)(UWLOG_ROWS - 1) * UWLOG_W), n--;
        snprintf(lines[n++], UWLOG_W, "Move how many? (1 to %d, Enter takes, Esc gives up): %s_", ask.have, ask.buf);
        a = 1.0f;
    }
    V.log(&lines[0][0], n, UWLOG_W, a, log_back + n < all);
}
static int full_play(void) { return V.ok && !iface && V.hud && full_live && display_mode != 1 && !menu_open; }
int plat_death_ours(void) { return V.ok && display_mode == 0; }
/* full-screen play as shown: the same while our menu is open over it (the game paused, its view still there) */
static int full_show(void) { return V.ok && !iface && V.hud && full_live && display_mode != 1; }
static int view_full_ok(void) { return V.ok && V.hud; }

/* ---- our inventory panel (Tab): the player's things as the original holds them (uwinv.c's snapshot from the game's
   thread) drawn by the DLL (src/uwinv_draw.c) at the right; every pointer event while it is open is turned into one on the
   ORIGINAL's own screen, where its code does the rest (INVPANEL.C's DoInventoryMouse: pick up, put down, wear, open a bag;
   INTERACT.C's mous_in_3d for the world): over one of the panel's holes, the point of the original's panel rectangle of
   that display position (the one its FindInventoryHit gives that position); over the world, the point of the original's
   own 3D view that our view shows there. A press is kept down for a while at least (the game polls the buttons). ---- */
static void send_key(uint8_t make);
static void send_key_later(int make, int ms);
static UwiSnap inv_snap;
static int inv_mx, inv_my, inv_hover = -1;
static unsigned inv_swallow;                 /* the buttons pressed on the panel's own parts (the check box): their release is ours */
static uint64_t inv_hold_t;                  /* a pick or take was asked (uwi_cmd): held, to us, till the snapshot shows it */
#define UWI_PACK_LAST 19                     /* INVPANEL.C display positions: the pack's last hole (12..19), the open bag's picture */
#define UWI_BAG 20

/* a pointer event at a point of the original's screen to the game (our panel's) */
static void inv_send(int type, float x, float y, unsigned button)
{
    PlatPointer p;
    if (!hooks || !hooks->pointer) return;
    memset(&p, 0, sizeof p);
    p.type = type;
    if (type == PLAT_POINTER_MOVE) p.dx = x - last_x, p.dy = y - last_y;
    else {
        p.button = button;
        buttons = type == PLAT_POINTER_DOWN ? buttons | button : buttons & ~button;
    }
    p.x = x, p.y = y;
    p.buttons = buttons;
    p.absolute = !locked && !captured;
    last_x = x, last_y = y;
    to_game(&p);
}
static void key_later_pump(void);

/* ---- the left button on things in the world (a click looks, held or dragged it carries, let go it throws), on the
   original's own routines (uw-new's
   port/ui/keys.c port_key_grab: n takes as the get mode's click, player_3dget; o puts the object on the cursor into the
   world as its right click in the 3D view, DoSpecialActions(DISP_WORLD) -> MISSILE.C's ReturnObject; u puts it in the
   first empty pack hole). The throw is the original's: ReturnObject throws (missile_fire, class 0xF) when the pointer
   is high enough in the 3D view (player_settr: y 0x25 and up of its 0..0x71), the higher the further and the more up
   (missile_try, its vertical speed, + (y - 0x38) / 6, and the player's pitch), its x turning it up to 33 of 256 either
   way (missile_trx); lower it drops it in front. Where it is let go, and how fast it flies (its speed class, ReturnObject's
   0xF, which uw-new lets the throw set: port_throw_speed), come from the mouse's momentum, the last 100 ms of its motion:
   still, the lowest throw (0x25) at speed THROW_SLOW, a small toss forward along the view that lands just ahead (never a
   dead drop); a flick of THROW_FULL window heights a second or more the original's own 0xF, upward the highest (0x71,
   further and higher), sideways bent up to the view's edge; in between in proportion. ---- */
#define THROW_MS 100                  /* the momentum's window */
#define THROW_FULL 2.5f               /* window heights a second for the strongest throw (and the most bend) */
#define THROW_SLOW 2                  /* the still let-go's speed class (the original's throw 0xF) */
#define TAP_MS 300                    /* a press let go sooner, the view still: a tap (into the pack, as E) */
static struct { uint64_t t; float dx, dy; } mot[64];
static unsigned mot_n;
static struct { int on, grab, stack, panel, thr_x, thr_y, thr_speed, drag; uint64_t t0, t_end; float moved; } carry;
/* the left button down on a thing in the world (a click looks, a drag carries, a right click takes): it goes
   onto the cursor once the mouse moves this far with the button held */
static struct { int on, stack; float moved; uint64_t t0; } wdrag;
#define HOLD_PICK_MS 250   /* a press held this long picks up (a shorter one looks) */
#define DRAG_START 10.0f
static void carry_begin(int grab, int stack, int panel);
static void grab_key(int how);   /* the left button holds a thing */
static void motion_note(float dx, float dy)
{
    mot[mot_n & 63].t = ms_now(), mot[mot_n & 63].dx = dx, mot[mot_n & 63].dy = dy;
    mot_n++;
    if (carry.on) carry.moved += fabsf(dx) + fabsf(dy);
    if (wdrag.on && (wdrag.moved += fabsf(dx) + fabsf(dy)) > DRAG_START) {   /* dragged: onto the cursor (the port's n) */
        wdrag.on = 0;
        carry_begin(1, wdrag.stack, 0), carry.drag = 1;
        grab_key(0);
    }
}
/* the motion of the last THROW_MS, in window heights a second: right and up */
static void motion_speed(float *vx, float *vy)
{
    uint64_t now = ms_now();
    float sx = 0, sy = 0;
    int cw, ch;
    for (unsigned k = 0; k < 64 && k < mot_n; k++) {
        unsigned i = (mot_n - 1 - k) & 63;
        if (now - mot[i].t > THROW_MS) break;
        sx += mot[i].dx, sy += mot[i].dy;
    }
    client_size(&cw, &ch);
    if (ch <= 0) ch = 1;
    *vx = sx / (float)ch * (1000.0f / THROW_MS), *vy = -sy / (float)ch * (1000.0f / THROW_MS);
}
/* the screen point (the game's, y up as mouse_getxy has it) to let the object go at for its throw, base_x the point
   across player_settr's 0..0xAC frame it is let go over (the crosshair's 0x56) */
static void throw_point(int base_x, int *sx, int *sy, int *speed)
{
    float vx, vy, up, side, strength;
    int x, y;
    motion_speed(&vx, &vy);
    up = vy < 0 ? 0 : vy > THROW_FULL ? THROW_FULL : vy;
    side = vx < -THROW_FULL ? -THROW_FULL : vx > THROW_FULL ? THROW_FULL : vx;
    strength = sqrtf(up * up + side * side) / THROW_FULL;
    strength = strength > 1 ? 1 : strength;
    y = 0x25 + (int)(up / THROW_FULL * (float)(0x71 - 0x25) + 0.5f);
    x = base_x + (int)(side / THROW_FULL * (float)0x56);
    x = x < 1 ? 1 : x > 0xAB ? 0xAB : x;
    y = y > 0x70 ? 0x70 : y;
    *sx = 0x34 + x, *sy = 0x43 + y;
    *speed = THROW_SLOW + (int)(strength * (float)(0xF - THROW_SLOW) + 0.5f);
    if (getenv("UWPORT_GRABDBG")) RLOG_I("front", "throw: momentum %.2f right %.2f up -> view %d,%d (screen %d,%d) speed %d", vx, vy, x, y, *sx, *sy, *speed);
}
/* n (0), o (1), u (2): the port's keys for the take, the throw and the putting away, armed first */
static void grab_key(int how)
{
#ifndef UWPORT_PLAIN
    if (how < 0) return;   /* (the queue's pause) */
    static const uint8_t make[3] = {0x31, 0x18, 0x16};
    if (how == 1) port_throw_x = carry.thr_x, port_throw_y = carry.thr_y, port_throw_speed = carry.thr_speed;
    port_grab_armed[how]++;
    send_key(make[how]);
    if (getenv("UWPORT_GRABDBG")) RLOG_I("front", "grab key %c at %lu ms", "nou"[how], (unsigned long)(ms_now() - L.start));
#else
    (void)how;
#endif
}
/* the left button holds a thing from now on (grab: just taken; panel: in our panel) */
static void carry_begin(int grab, int stack, int panel)
{
    carry.on = 1, carry.grab = grab, carry.stack = stack, carry.panel = panel, carry.t0 = ms_now(), carry.moved = 0, carry.drag = 0;
}
/* the left button let go while it holds a thing: a tap on a thing just taken puts it in the pack; else it is thrown
   (a stack being counted, 'Move how many?', is left on the cursor: its keys are the count's) */
static void carry_release(int over_world, int let_go_x)
{
    int tap = carry.grab && !carry.drag && ms_now() - carry.t0 < TAP_MS && carry.moved < (carry.panel ? 6.0f : 12.0f);
    carry.on = 0, carry.t_end = ms_now();
    if (carry.grab && carry.stack) return;
    if (tap) {
        /* into the pack (the port's u); with our panel open a command, in order after the take's own */
        if (carry.panel) uwi_cmd(UWI_OP_PACK, 0, 0, 0), inv_hold_t = 0;
        else grab_key(2);
        return;
    }
    if (!over_world) return;
    throw_point(let_go_x, &carry.thr_x, &carry.thr_y, &carry.thr_speed);
    if (carry.panel) uwi_cmd(UWI_OP_DROP, carry.thr_x, carry.thr_y, carry.thr_speed), inv_hold_t = 0;
    else grab_key(1);
}

/* a left press on a thing in our panel: a look when let go soon, onto the pointer once held or dragged.
   Picked up by this press (lift), it is put where the button comes up: another hole, or the world (thrown); on its own
   hole it stays on the pointer */
static struct { int on, hit, obj, cx, cy, qty; uint64_t t0; } inv_lp;   /* qty: the stack's count (1: a single thing) */
static struct { int on, cx, cy; } lift;   /* something came onto the pointer by the press at (cx, cy): the release puts it */
static void inv_pick(void)
{
    inv_lp.on = 0;
    /* (held or dragged: the whole stack, with Shift too; Shift + a click is the only way to split) */
    if (!uwi_cmd(UWI_OP_PICK, inv_lp.obj, 0, 0)) return;
    inv_hold_t = ms_now();
    lift.on = 1, lift.cx = inv_lp.cx, lift.cy = inv_lp.cy;
}
/* a press held long enough picks up: the world's, our panel's */
static void hold_pump(void)
{
    if (wdrag.on && ms_now() - wdrag.t0 >= HOLD_PICK_MS) {   /* the world: held, onto the cursor */
        wdrag.on = 0;
        carry_begin(1, wdrag.stack, 0), carry.drag = 1;
        grab_key(0);
    }
    if (inv_lp.on && ms_now() - inv_lp.t0 >= HOLD_PICK_MS) inv_pick();
}
/* each picture: the holds and the late key */
static void inv_pump(void)
{
    hold_pump();
    key_later_pump();
}
/* our panel opened or closed (the original's interface when the renderer has no panel) */
static void inv_toggle(void)
{
    if (!V.inventory || !V.inv_hit) { interface_toggle(); return; }   /* no renderer: the original's interface */
    inv_open = !inv_open;
    uwi_set_active(inv_open);
    if (inv_open) {
        int ptx, pty;
        int cw, ch;
        client_size(&cw, &ch);
        inv_mx = cw / 2, inv_my = ch / 2;
        if (uwp_pointer_pos(&ptx, &pty)) {
            int px = ptx, py = pty;
            real_to_view(&px, &py);
            if (px >= 0 && py >= 0 && px < cw && py < ch) inv_mx = px, inv_my = py;
        }
        inv_hover = -1, inv_snap.valid = 0;
        uwi_want_panel();                  /* the original's panel on the inventory page */
    } else {
        inv_swallow = 0, inv_lp.on = 0, lift.on = 0;
    }
}
/* the original's screen point for the window point (cx, cy): 1 and *gx, *gy when there is one; *hit is what the point is on */
static float view_tan(int zoom);
/* our grid (uwinv_draw.c): its bags open in the grid (objects; kept while the game runs) */
static int inv_open_bags[16], inv_nopen;
/* the open bags written to the port's settings ("open_bags", the objects' numbers: they are the save's own, so the same
   bags open after a restore), read once */
static void inv_bags_save(void)
{
    char v[160] = "";
    int i, n = 0;
    for (i = 0; i < inv_nopen && n < (int)sizeof v - 8; i++) n += snprintf(v + n, sizeof v - (size_t)n, "%s%d", i ? "," : "", inv_open_bags[i]);
    if (plat_home()) port_config_set(plat_home(), "open_bags", v);
}
/* the open bags read from the settings, once */
static void inv_bags_load(void)
{
    static int done;
    char v[160];
    const char *p = v;
    if (done) return;
    done = 1;
    if (!plat_home() || port_config_get(plat_home(), "open_bags", v, sizeof v) != 0) return;
    while (*p && inv_nopen < 16) {
        int o = atoi(p);
        if (o > 0) inv_open_bags[inv_nopen++] = o;
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
}
/* a bag opened or closed in our grid. The list keeps the order the bags were opened in, the oldest first; when it is
   full the oldest goes to make room (else the bags of older saves, which nothing takes off, fill it and no new bag
   opens) */
static void inv_bag_toggle(int obj)
{
    int i;
    for (i = 0; i < inv_nopen; i++)
        if (inv_open_bags[i] == obj) {
            memmove(&inv_open_bags[i], &inv_open_bags[i + 1], (size_t)(inv_nopen - i - 1) * sizeof inv_open_bags[0]);
            inv_nopen--;
            inv_bags_save();
            return;
        }
    if (inv_nopen == 16) memmove(&inv_open_bags[0], &inv_open_bags[1], 15 * sizeof inv_open_bags[0]), inv_nopen--;
    inv_open_bags[inv_nopen++] = obj;
    inv_bags_save();
}
/* the world point of the window point (cx, cy) on the original's screen: 1 and *gx, *gy (pointer coordinates, y down)
   when it is on the 3D view; *hit is what the point is on (uwinv_draw.c's codes; -1 the world) */
static float view_tan(int zoom);
static int inv_map(int cx, int cy, int *gx, int *gy, int *hit)
{
    int zoom, w, h;
    float fx = 0.5f, fy = 0.5f;
    *hit = V.inv_hit(&inv_snap, cx, cy, inv_sheet, &fx, &fy);
    if (*hit != -1 || !V.cw || !V.ch || !uwg_view_info(&zoom, &w, &h) || zoom <= 0 || w <= 0 || h <= 0) return 0;
    {   /* the world: our view's tangent at the point, as the original's view (the zoom's lens) has it there */
        float ty = V.fov ? tanf(V.fov() * 0.5f) : 0.7f;
        float th = ((float)cx / (float)V.cw * 2.0f - 1.0f) * ty * (float)V.cw / (float)V.ch;
        float tv = ((float)cy / (float)V.ch * 2.0f - 1.0f) * ty;
        float tx = view_tan(zoom), tyo = tx * (float)h / (float)w * 1.2f;
        float px = (th / tx + 1.0f) * 0.5f * (float)w, py = (tv / tyo + 1.0f) * 0.5f * (float)h;
        if (getenv("UWPORT_INVDBG")) RLOG_I("front", "world map zoom %d view %dx%d tan %g,%g -> %g,%g", zoom, w, h, th, tv, px, py);
        if (px < 1 || py < 1 || px >= (float)w - 1 || py >= (float)h - 1) return 0;
        *gx = UWG_VIEW_X + (int)px, *gy = UWG_VIEW_Y + (int)py;
        return 1;
    }
}
/* ---- a pointer event of the window while the panel is open: a left click on a thing looks at it,
   held or dragged it comes onto the pointer and is put down where the button comes up (another hole, or thrown into
   the world); a right click uses it (a bag: its rows in our grid; armour or a weapon in the pack: worn or wielded; on
   the body: off, into the pack; Shift: out into the world, or out of a bag into the pack). With something on the pointer
   a left click puts it down there. Every action is a command for the game's thread (uwi_cmd), run at its next frame
   by the original's own routines; nothing waits here and nothing is thrown away. 1: taken ---- */
static int inv_runes_open;       /* our rune page is open (a right click on the rune bag) */
static int inv_held(void) { return (inv_snap.held && !inv_snap.target) || ms_now() - inv_hold_t < 600; }
static int inv_hit_node(int hit)   /* the node of our grid a hit is on (a thing, a bag, or an empty hole of a bag: the bag), -1 */
{
    int n = -1;
    if (hit >= UWI_HIT_NODE && hit < UWI_HIT_BAGBAR) n = hit - UWI_HIT_NODE;
    else if (hit >= UWI_HIT_BAGBAR && hit < UWI_HIT_INTO) n = hit - UWI_HIT_BAGBAR;
    else if (hit >= UWI_HIT_INTO && hit < UWI_HIT_PACK) n = hit - UWI_HIT_INTO;
    return n >= 0 && n < inv_snap.nnodes ? n : -1;
}
static int inv_hit_obj(int hit)   /* the thing a hit is on: its object, -1 none (a hole of the grid, or of the body, with something in it) */
{
    if (hit >= UWI_HIT_NODE && hit < UWI_HIT_BAGBAR) { int n = inv_hit_node(hit); return n >= 0 ? inv_snap.node[n].obj : -1; }
    if (hit >= 1 && hit <= 11) return inv_snap.disp_obj[hit];
    return -1;
}
/* the held thing put down on a hit: a command (the original's rules: a stack joins, a bag takes it in, what was there
   comes onto the pointer); 0 where nothing can be put */
static int inv_put_at(int hit)
{
    int n;
    inv_hold_t = 0;   /* no longer held, to us, the moment it is put down (a right click just after must not be lost) */
    if (hit >= UWI_HIT_PACK && hit < UWI_HIT_PACK + 8) return uwi_cmd(UWI_OP_PUT_SLOT, 11 + hit - UWI_HIT_PACK, 0, 0);
    if ((n = inv_hit_node(hit)) >= 0) return uwi_cmd(UWI_OP_PUT_ON, inv_snap.node[n].obj, 0, 0);
    if (hit >= 1 && hit <= 11) return uwi_cmd(UWI_OP_PUT_DISP, hit, 0, 0);
    return 0;
}
static void inv_throw(int gx)   /* the held thing let go over the world: thrown as the mouse's momentum has it (throw_point) */
{
    int ux, uy, sp;
    inv_hold_t = 0;
    throw_point(gx - 0x34, &ux, &uy, &sp);
    uwi_cmd(UWI_OP_DROP, ux, uy, sp);
}
/* a pointer event over our open panel, made into commands for the game (as the block above says): 1 taken */
static int inv_pointer(int type, int cx, int cy, unsigned button)
{
    int gx = 0, gy = 0, hit, m, obj, n;
    if (!inv_snap.valid) return 0;
    if (button == PLAT_BUTTON_LEFT && getenv("UWPORT_INVTEST_RIGHT")) button = PLAT_BUTTON_RIGHT;   /* (a check's: the script's wclick is a left click) */
    if (type == PLAT_POINTER_MOVE) motion_note((float)(cx - inv_mx), (float)(cy - inv_my));
    inv_mx = cx, inv_my = cy;
    if (type == PLAT_POINTER_DOWN) ask.on = 0;   /* (a press anywhere: the count asked is given up) */
    m = inv_map(cx, cy, &gx, &gy, &hit);
    /* the open bags' slider (uwinv_draw.c): a left press on it scrolls or drags, its release ends the drag */
    if (type == PLAT_POINTER_UP && button == PLAT_BUTTON_LEFT && V.inv_click && V.inv_click(cx, cy, 0)) { inv_swallow &= ~button; return 1; }
    if (type == PLAT_POINTER_DOWN && hit == UWI_HIT_SCROLL) {
        inv_swallow |= button;
        if (button == PLAT_BUTTON_LEFT && V.inv_click) V.inv_click(cx, cy, 1);
        return 1;
    }
    if (inv_snap.panel != 0 && type == PLAT_POINTER_DOWN && inv_snap.panel != 4) uwi_want_panel();   /* the original's panel back on the inventory (our top copies it) */
    if (type == PLAT_POINTER_DOWN && hit >= UWI_HIT_RUNE && hit <= UWI_HIT_CAST) {   /* our rune page */
        inv_swallow |= button;
        if (hit == UWI_HIT_CAST) { if (button == PLAT_BUTTON_LEFT) uwi_cmd(UWI_OP_CAST, 0, 0, 0); }
        else if (hit == UWI_HIT_SHELF) { if (button == PLAT_BUTTON_LEFT) uwi_cmd(UWI_OP_RUNE, -1, 0, 0); }
        else if (button == PLAT_BUTTON_LEFT) uwi_cmd(UWI_OP_RUNE, hit - UWI_HIT_RUNE, 0, 0);
        else if (button == PLAT_BUTTON_RIGHT) uwi_cmd(UWI_OP_RUNELOOK, hit - UWI_HIT_RUNE, 0, 0);
        return 1;
    }
    if (type == PLAT_POINTER_MOVE) {
        if (inv_lp.on && abs(cx - inv_lp.cx) + abs(cy - inv_lp.cy) > 8) inv_pick();   /* dragged: onto the pointer */
        return 1;
    }
    obj = inv_hit_obj(hit), n = inv_hit_node(hit);
    if (type == PLAT_POINTER_DOWN) {
        inv_swallow |= button;
        if (hit == -2) { if (button == PLAT_BUTTON_LEFT) inv_sheet = !inv_sheet; return 1; }
        if (inv_snap.target) {   /* the game wants a target ('Use key on what?') or an aim (a missile spell): the right
                                    button, as the original's (mous_in_3d acts on it): the world at the point, a thing of
                                    the panel (the original's panel click, UWI_OP_TARGET); the left cancels a target */
            int aim = inv_snap.input_mode == 3;
            if (button == PLAT_BUTTON_RIGHT && hit == -1 && m) uwi_cmd(aim ? UWI_OP_AIM3D : UWI_OP_TARGET3D, gx, 199 - gy, 0);
            else if (button == PLAT_BUTTON_RIGHT && obj >= 0 && !aim) uwi_cmd(UWI_OP_TARGET, obj, 0, 0);
            else if (!aim) uwi_cmd(UWI_OP_TARGET, -1, 0, 0);
            return 1;
        }
        if (hit == -1) {   /* the world off the panel, at the pointer */
            if (!m) return 1;
            inv_send(PLAT_POINTER_MOVE, (float)gx, (float)gy, 0);
            if (button == PLAT_BUTTON_LEFT && inv_held()) { carry_begin(0, 0, 1); return 1; }   /* let go there: thrown (the release) */
            if (button == PLAT_BUTTON_LEFT) {
                /* a thing that can be taken: onto the pointer, held while the button is (thrown where it comes up, or put
                   in a hole; a tap puts it in the pack); else looked at (the port's n does both: player_3dget / 3dlook) */
                UwlInfo li;
                uwl_info(&li);
                if (li.kind == 3 && li.take && li.reach) carry_begin(1, li.stack, 1), inv_hold_t = ms_now();
                uwi_cmd(UWI_OP_TAKE, gx, 199 - gy, 0);
                return 1;
            }
            if (button == PLAT_BUTTON_RIGHT) send_key_later(0x12, 60);   /* e: used (talk, open, flip) */
            return 1;
        }
        if (hit == 0) return 1;
        if (button == PLAT_BUTTON_LEFT) {
            if (inv_held()) {   /* put down here (what was there may come onto the pointer: dragged on, the release puts it) */
                inv_put_at(hit);
                lift.on = 1, lift.cx = cx, lift.cy = cy;
                return 1;
            }
            if (obj >= 0) {   /* a look if let go soon, onto the pointer when held or dragged */
                const UwiThing *pt = n >= 0 ? &inv_snap.node[n].t : hit >= 1 && hit <= 11 ? &inv_snap.t[hit] : NULL;
                inv_lp.on = 1, inv_lp.hit = hit, inv_lp.obj = obj, inv_lp.cx = cx, inv_lp.cy = cy, inv_lp.t0 = ms_now();
                inv_lp.qty = pt ? pt->qty : 1;
                return 1;
            }
            if ((hit == 8 || hit == 9) && hit == 9 - inv_snap.lefty) uwi_cmd(UWI_OP_FIGHT, 0, 0, 0);   /* the empty weapon hand: fight mode, the original's click there */
            return 1;
        }
        if (button == PLAT_BUTTON_RIGHT && inv_held()) {   /* a thing on the pointer: back into the pack (it was dead) */
            uwi_cmd(UWI_OP_PACK, 0, 0, 0), inv_hold_t = 0, lift.on = 0;
            return 1;
        }
        if (button == PLAT_BUTTON_RIGHT && !inv_held()) {
            const UwiThing *t = n >= 0 ? &inv_snap.node[n].t : hit >= 1 && hit <= 11 ? &inv_snap.t[hit] : NULL;
            if (n >= 0 && inv_snap.node[n].bag && !shift_ours()) { inv_bag_toggle(inv_snap.node[n].obj); return 1; }   /* a bag: its rows */
            if (t && t->runebag && !shift_ours()) { inv_runes_open = !inv_runes_open; return 1; }   /* the rune bag: our rune page */
            if (obj < 0 || !t) return 1;
            if (shift_ours() && n >= 0 && inv_snap.node[n].parent >= 0) {   /* Shift, in a bag: out of it into the pack */
                uwi_cmd(UWI_OP_PICK, obj, 0, 0), uwi_cmd(UWI_OP_PACK, 0, 0, 0);
            } else if (shift_ours()) {   /* Shift: down in the world near the player (let go low in the view: put in front) */
                /* (a drop the game refuses, "There is no space to drop that.", would leave the thing on the pointer,
                   where every right click and E is then dead: back into the pack then; PACK does nothing when the drop
                   went through) */
                uwi_cmd(UWI_OP_PICK, obj, 0, 0), uwi_cmd(UWI_OP_DROP, 0x34 + 0x56, 0x43 + 8, 0), uwi_cmd(UWI_OP_PACK, 0, 0, 0);
            } else if (n >= 0 && t->wear >= 1 && t->wear <= 11) {
                /* armour, a weapon, a shield, a ring in the pack or a bag: on, where it goes; what was there back here */
                uwi_cmd(UWI_OP_PICK, obj, 0, 0), uwi_cmd(UWI_OP_PUT_DISP, t->wear, 0, 0);
                if (inv_snap.node[n].parent < 0) uwi_cmd(UWI_OP_PUT_SLOT, inv_snap.node[n].slot, 0, 0);
                else uwi_cmd(UWI_OP_PUT_ON, inv_snap.node[inv_snap.node[n].parent].obj, 0, 0);
            } else if (hit >= 1 && hit <= 11 && (hit <= 5 || hit >= 10 || t->wear == hit)) {
                /* worn or wielded (the armour, the rings, the hand's weapon or shield): off, into the pack */
                uwi_cmd(UWI_OP_PICK, obj, 0, 0), uwi_cmd(UWI_OP_PACK, 0, 0, 0);
            } else uwi_cmd(UWI_OP_USE, obj, 0, 0);   /* used: eat, read, light, a key on a lock ... */
            return 1;
        }
        return 1;
    }
    if (type == PLAT_POINTER_UP) {
        if (button == PLAT_BUTTON_LEFT && inv_lp.on) {   /* a click: a look at it */
            inv_lp.on = 0, inv_swallow &= ~button;
            if (shift_ours() && inv_lp.qty > 1) ask_begin(inv_lp.obj, inv_lp.qty);   /* Shift + a click on a stack: how many */
            else uwi_cmd(UWI_OP_LOOK, inv_lp.obj, 0, 0);
            return 1;
        }
        if (button == PLAT_BUTTON_LEFT && lift.on) {
            /* on the pointer since the press: let go where it has been dragged to (a hole: put there; the world: thrown);
               let go where it was pressed it stays on the pointer */
            lift.on = 0, inv_swallow &= ~button;
            if (abs(cx - lift.cx) + abs(cy - lift.cy) < 8) return 1;
            if (hit == -1 && m) inv_throw(gx);
            else inv_put_at(hit);
            return 1;
        }
        if (button == PLAT_BUTTON_LEFT && carry.on && carry.panel) {
            /* a thing taken from the world, or held, let go: thrown over the world, put in a hole over the panel; a tap
               into the pack (a stack being counted is left on the pointer: its keys are the count's) */
            int tap = carry.grab && ms_now() - carry.t0 < TAP_MS && carry.moved < 6.0f;
            inv_swallow &= ~button;
            if (hit >= 1 && !tap && !(carry.grab && carry.stack)) carry.on = 0, carry.t_end = ms_now(), inv_put_at(hit);
            else carry_release(m && hit == -1, m ? gx - 0x34 : 0x56);
            return 1;
        }
        if (inv_swallow & button) inv_swallow &= ~button;
        return 1;
    }
    return 1;
}
/* our panel drawn from the game's latest snapshot, with our open bags */
static void inv_draw(void)
{
    inv_bags_load();
    if (!uwi_snapshot(&inv_snap)) return;
    memcpy(inv_snap.open_bags, inv_open_bags, sizeof inv_open_bags), inv_snap.nopen = inv_nopen;
    inv_snap.runes_open = inv_runes_open;
    inv_hover = V.inventory(&inv_snap, inv_mx, inv_my, inv_sheet);
}
/* Our options (uw.exe's menu, made again): the rows; the game's are carried out by the original's own shortcuts
   (PLAYER.C: Ctrl+S save, Ctrl+R restore, Ctrl+M music, Ctrl+F sound, Ctrl+Q quit: do_option_shortcut), the
   renderer's by the DLL (uwv_option), the window's here */
enum { MO_RESUME, MO_DISPLAY, MO_FULL, MO_SAVE, MO_LOAD, MO_NEW, MO_DLSS, MO_DLSS5, MO_STYLE, MO_RT, MO_BOUNCE, MO_SHADOWS, MO_RELIEF, MO_NSTR,
       MO_MUSIC, MO_MUSVOL, MO_SOUND, MO_FXVOL, MO_FADE, MO_MOUSE, MO_LLM, MO_FOV,
       MO_PARTICLES, MO_DECALS, MO_WATER, MO_FOG, MO_FXAUDIO, MO_ACOUSTICS, MO_SMOOTH, MO_CONVFONT, MO_HUNTERS, MO_MENUFONT, MO_HRTF, MO_PANFIX, MO_RES, MO_DLSSMODE, MO_RR, MO_D5INT, MO_D5STR, MO_CHARSHEET, MO_LOGFONT, MO_ACWET, MO_ACSIZE, MO_ACOCCL, MO_ACDETOUR, MO_FOOTSTEPS, MO_FONT, MO_FONTSIZE, MO_QUIT, MO_TEXAO, MO_TEXSHADOW, MO_TEXRES, MO_LIGHTCOL, MO_ADAPT, MO_HDR, MO_HDRPEAK, MO_HDRPAPER, MO_RENDERER, MO_MAPNORMAL, MO_MAPAO, MO_MAPHEIGHT, MO_HEARRANGE, MO_FEETSPREAD, MO_ECHOQ, MO_STEPVOL, MO_STEPECHO, MO_AILEVEL, MO_AIBY, MO_AIP0, MO_AIP1, MO_AIP2, MO_AIP3, MO_AIP4, MO_AIP5, MO_AIP6, MO_BREATH, MO_DOORCUE, MO_LIGHTFADE, MO_DOORSTEP, MO_DOORPICK, MO_MENUBG, MO_AIFRIENDS, MO_AILOG, MO_LLMGPU, MO_LLMMODE, MO_LLMRATE, MO_LLMSTALE, MO_LLMIDLE, MO_GLOWSTONES, MO_MOONLIGHT, MO_ELEMLIGHT, MO_CAMPSMOKE, MO_WATERMIST, MO_COUNT, MO_HEAD = 98 };
_Static_assert(MO_COUNT <= 98, "the rows' ids run into MO_HEAD");
/* the language model's files and the one in use (uwmindhost.c): the Language model row */
int port_mind_models(const char **names, const char **paths, int max);
void port_mind_pick(const char *path);
const char *port_mind_current(void);
const char *port_mind_current_path(void);
int port_mind_first_start(void);
void port_mind_keep(void);
static int pick_prompt;               /* the first start's model picker is the menu that is open */
/* the message box's fade (the Esc menu's option): seconds after the newest message, 0 never */
static const int FADES[] = {2, 3, 5, 8, 10, 15, 30, 0};
static int fade_i = -1;
/* the message fade in seconds, read from the settings once */
static int fade_secs(void)
{
    if (fade_i < 0) {
        char v[16];
        int want = plat_home() && port_config_get(plat_home(), "message_fade", v, sizeof v) == 0 ? atoi(v) : 5;
        fade_i = 2;
        for (int i = 0; i < (int)(sizeof FADES / sizeof FADES[0]); i++) if (FADES[i] == want) fade_i = i;
    }
    return FADES[fade_i];
}
/* a door the player shuts while he stands just inside its space shuts all the same (USEITEMS.C's port_door_step_on: it
   DEPARTS from the original's rule, Off is the original): the config's door_shut_close, on unless 0. Never in a replay. */
extern int port_door_step_on;
static int door_step(int v)
{
    static int on = -1;
    extern int16_t rp_request;
    if (v >= 0) {
        on = v != 0;
        if (plat_home()) port_config_set(plat_home(), "door_shut_close", on ? "1" : "0");
    } else if (on < 0) {
        char b[16];
        on = !(plat_home() && port_config_get(plat_home(), "door_shut_close", b, sizeof b) == 0 && !strcmp(b, "0"));
    }
    port_door_step_on = on && rp_request != 2 && display_mode != 1;   /* (the original's display: its rule) */
    return on;
}
/* a door picked by the frame and leaf our display draws, when nothing else is at the point (uwlook.c's
   uwl_door_targets; the use is the original's): the config's door_targeting, on unless 0 */
static int door_pick(int v)
{
    static int on = -1;
    if (v >= 0) {
        on = v != 0;
        if (plat_home()) port_config_set(plat_home(), "door_targeting", on ? "1" : "0");
    } else if (on < 0) {
        char b[16];
        on = !(plat_home() && port_config_get(plat_home(), "door_targeting", b, sizeof b) == 0 && !strcmp(b, "0"));
    }
    uwl_door_targets = on && display_mode != 1;
    return on;
}
extern int port_door_cue_on;
/* the message line when a closing door turns round because the player is in its way (EFFECT.C's port_door_cue_on: the
   rule is the original's, the line is ours): the config's door_blocked_message, on unless 0; v 0 or 1 sets and keeps
   it. Never in a replay (a line more in the scroll). */
static int door_cue(int v)
{
    static int on = -1;
    extern int16_t rp_request;
    if (v >= 0) {
        on = v != 0;
        if (plat_home()) port_config_set(plat_home(), "door_blocked_message", on ? "1" : "0");
    } else if (on < 0) {
        char b[16];
        on = !(plat_home() && port_config_get(plat_home(), "door_blocked_message", b, sizeof b) == 0 && !strcmp(b, "0"));
    }
    port_door_cue_on = on && rp_request != 2 && display_mode != 1;
    return on;
}
void port_sound_volumes(int music, int fx);   /* SOUND.C, port only: the theme's and the effects' volume, percent */
static int mus_vol = -1, fx_vol = -1;        /* the menu's Music volume and Sound volume (the config's music_volume, sound_volume) */
static void volumes_load(void)
{
    char v[16];
    if (mus_vol >= 0) return;
    mus_vol = plat_home() && port_config_get(plat_home(), "music_volume", v, sizeof v) == 0 ? atoi(v) : 100;
    fx_vol = plat_home() && port_config_get(plat_home(), "sound_volume", v, sizeof v) == 0 ? atoi(v) : 100;
    if (mus_vol < 0 || mus_vol > 100) mus_vol = 100;
    if (fx_vol < 0 || fx_vol > 100) fx_vol = 100;
    port_sound_volumes(mus_vol, fx_vol);
}
/* a step of 5: a left click or Right up (to 100), a right click or Left down (to 0) */
static void volume_step(int *v, int step, const char *key)
{
    char b[16];
    volumes_load();
    *v = step > 0 ? *v + 5 : *v - 5;
    if (*v < 0) *v = 0;
    if (*v > 100) *v = 100;
    port_sound_volumes(mus_vol, fx_vol);
    if (v == &fx_vol) uwaudio_volume(fx_vol / 100.0f);   /* our positional effects follow the Sound volume too */
    snprintf(b, sizeof b, "%d", *v);
    if (plat_home()) port_config_set(plat_home(), key, b);
}
static int menu_ids[160], menu_n;   /* (160: MENU_MAX, below) */
/* our save and load pages (our display shows none of the original's panel): 0 the options, 1 save, 2 load;
   a page's rows are the four slots (MO_SLOT + 1..4) and Back. The save goes through the game's own DoSaveRest on its
   thread (uwinv.c's UWI_OP_SAVE / UWI_OP_LOAD), the description written for the player: the level and the time */
enum { MO_BACK = 99, MO_SLOT = 100 };
/* the Fog and smoke tab (its rows are sliders and typed numbers): MO_VOLPAGE stands in MENU_ROWS where the renderer's own rows go (uwv_vol_row: MO_VOL + its number
   there), none named here. MO_PRESET and MO_DEFAULTS: the two rows at the top of a tab (its presets, its defaults) */
enum { MO_VOLPAGE = 105, MO_PRESET = 106, MO_DEFAULTS = 107, MO_VOL = 400 };
/* the Keys tab (key_rows_build): MO_KEYPAGE stands in MENU_ROWS where uwinput.c's commands go (MO_KEY + its number
   there); MO_KEYDEFAULT its Default keys row */
enum { MO_KEYPAGE = 108, MO_KEYDEFAULT = 109, MO_KEY = 2000 };
/* the Sprites tab (spr_rows_build): MO_SPRPAGE stands in MENU_ROWS where the rows of our own object pictures go
   (MO_SPR + the object: one for each picture that is in uwr\newsprites\objects, none when there is none) */
enum { MO_SPRPAGE = 110, MO_SPR = 3000, MO_SPRALL = 3999 };   /* MO_SPRALL: the tab's first row, every picture at once (uwv_option 1999) */
static int menu_page;
static char slot_desc[4][48], slot_time[4][24];
static int slot_used[4];
/* the four save slots' names and times, read from SAVEn/DESC */
static void slots_read(void)
{
    const char *home = plat_home();
    for (int i = 0; i < 4; i++) {
        char path[1200];
        FILE *f;
        struct stat st;
        slot_used[i] = 0, slot_desc[i][0] = 0, slot_time[i][0] = 0;
        if (!home) continue;
        snprintf(path, sizeof path, "%s/SAVE%d/DESC", home, i + 1);
#ifndef _WIN32   /* (a case-sensitive file system: the game writes SAVE0\desc; its name as the home tree has it) */
        {
            char dos[16];
            snprintf(dos, sizeof dos, "SAVE%d\\DESC", i + 1);
            if (plat_resolve(dos, PLAT_READ, path, sizeof path) != 0) continue;
        }
#endif
        if ((f = fopen(path, "rb")) == NULL) {   /* (no file: an empty slot; any other reason is said) */
            if (errno != ENOENT) RLOG_W("front", "cannot open %s: %s (the slot shown empty)", path, rlog_errno());
            continue;
        }
        size_t n = fread(slot_desc[i], 1, sizeof slot_desc[i] - 1, f);
        if (ferror(f)) RLOG_W("front", "cannot read %s", path);
        fclose(f);
        slot_desc[i][n] = 0;
        for (size_t k = 0; k < n; k++) if ((unsigned char)slot_desc[i][k] < 32) slot_desc[i][k] = ' ';
        slot_used[i] = 1;
        if (stat(path, &st) == 0) {
            struct tm *t = localtime(&st.st_mtime);
            if (t) strftime(slot_time[i], sizeof slot_time[i], "%m-%d %H:%M", t);
        }
    }
}
/* our menu opened on a page (0 the options, 1 save, 2 load), the game paused */
static void menu_open_page(int page)
{
    menu_open = 1, menu_page = page, menu_sel = 0;
    if (page) slots_read();
    port_pause(1);
}
static int quit_armed;               /* Quit chosen once: the next choice quits */
/* a Ctrl shortcut of the game's (Ctrl down, the key, both up) */
static void ctrl_key(int c)
{
    /* the game reads Ctrl's state as it takes the key: give it a tick (55 ms) between the events */
    game_key(0x1D, 1), uwp_sleep_ms(70), game_key(c, 1), uwp_sleep_ms(70), game_key(c, 0), uwp_sleep_ms(70), game_key(0x1D, 0);
}
/* The rows in the order shown, under their headings (every setting a row). opt: the renderer's option (uwv_option) or -1 ours; hint: what the row does, shown at the menu's foot. A
   renderer's row is there once the renderer has the option. */
#define MENU_MAX 160
/* The sound's dials, to tune the reverb and the rest by ear: every number that was fixed in uwaudio.c, uwsteam.c
   and uwacoustic.c is a key of settings.txt (uwaudio.c's TU has the list) with a row here, under "Sound tuning".
   Ours, not the renderer's: read once from settings.txt, written there at each step, heard at once
   (fx_acoustics_tune). dflt is the number as it was in the code; a row is shown when its part of the sound runs
   (need). MO_ROOMINFO is the read-only row of what the rays measure where the player stands. */
enum { MO_CARDMUSIC = 196, MO_CARDSPEECH = 197, MO_ROOMINFO = 199, MO_DIAL = 200 };
/* rows for the options that had none (the Esc menu audit, work\escaudit): the renderer's (uwv_option 90-104),
   the enhancements and fixes (MO_ENH + the flag's ENH_ number: uw1port.cfg enhance=, the ones the Display switch
   does not set), Quick fades, and the port's own settings screen (F11) */
enum { MO_RVSYNC = 111, MO_REXPOSE, MO_RGAMMA, MO_RINVERT, MO_RHAND, MO_RSHEEN, MO_RBOUNCES, MO_RRAYLEN, MO_RSPRN, MO_RSPRAO,
       MO_RSPRH, MO_RD5TONE, MO_RD5SAT, MO_RCAMPSW, MO_RFLAMESW, MO_QFADES = 126, MO_SETSCREEN = 127, MO_LOGDETAIL = 128,
       MO_SEENMAP = 130, MO_MAPARROWS = 131, MO_MAPNOTES = 132, MO_FEUD = 133, MO_MENUFAST = 134, MO_SYSPTR = 135,
       MO_ENH = 167 };
_Static_assert(MO_ENH + ENH_COUNT <= MO_CARDMUSIC, "the enhancements' rows run into the sound cards'");
/* The game's own sound devices (the Roland, a Sound Blaster, ...): DATA\UW.CFG's two cards in the home folder, as the settings screen's Sound tab
   writes them (the port's uwcfg.c and settab.c: the same numbers and names). The game loads its drivers at its
   start, so a change holds from the next start; cards_run is what this run started with. With a music card that
   is not the Roland our own Roland renderings of the effects stand down (uwaudio_stand_down): the effects are
   that card's own, as the original paired them, without places or acoustics. */
void read_uw_cfg(int *music, int *speech);
int write_uw_cfg(const char *spec);
static const char *const CARD_MUSIC[7] = {"None", "PC speaker", "Ad Lib", "Sound Blaster", "Sound Blaster Pro", "Pro Audio Spectrum", "Roland MT-32"};
static const char *const CARD_SPEECH[4] = {"None", "Sound Blaster", "Sound Blaster Pro", "Pro Audio Spectrum"};
static int cards[2] = {-2, -2}, cards_run[2];
/* the two sound cards read from UW.CFG, once */
static void cards_load(void)
{
    if (cards[0] != -2) return;
    read_uw_cfg(&cards[0], &cards[1]);
    cards_run[0] = cards[0], cards_run[1] = cards[1];
}
/* a sound card's row value */
static void card_text(int which, char *out, size_t n)
{
    cards_load();
    int v = cards[which], most = which ? 3 : 6;
    snprintf(out, n, "%s%s", v < 0 || v > most ? "Chosen by the game" : which ? CARD_SPEECH[v] : CARD_MUSIC[v], v != cards_run[which] ? " (from the next start)" : "");
}
/* the next or the previous sound card, written to UW.CFG (heard from the next start) */
static void card_step(int which, int step)
{
    char spec[16];
    cards_load();
    int most = which ? 3 : 6, v = cards[which] < 0 || cards[which] > most ? (which ? 1 : 6) : cards[which];
    v = (v + (step > 0 ? 1 : most)) % (most + 1);
    cards[which] = v;
    snprintf(spec, sizeof spec, "%d,%d", cards[0] < 0 ? 6 : cards[0], cards[1] < 0 ? 1 : cards[1]);
    write_uw_cfg(spec);
    if (plat_home()) port_config_set(plat_home(), "sound-auto", "0");   /* the player has chosen: the port does not */
}
enum { DK_NUM, DK_ONOFF, DK_SECONDS, DK_BOUNCES, DK_AUTO };   /* DK_AUTO: 0 Off, 1 On, 2 Automatic */
enum { DN_AUDIO, DN_AC, DN_HIGH, DN_EFX, DN_ALWAYS };   /* positional sound on; Acoustics On or High; High alone; On alone */
static const struct { const char *key; int dflt, lo, hi, step, kind, need; const char *unit; } SOUND_DIALS[] = {
    /*  0 */ {"acoustics_echo_first", 1, 0, 1, 1, DK_ONOFF, DN_HIGH, ""},
    /*  1 */ {"acoustics_echo_level", 5, 0, 60, 1, DK_NUM, DN_HIGH, "%"},
    /*  2 */ {"acoustics_echo_ms", 2000, 500, 6000, 250, DK_SECONDS, DN_HIGH, ""},
    /*  3 */ {"acoustics_bounces", 0, 0, 64, 4, DK_BOUNCES, DN_HIGH, ""},
    /*  4 */ {"acoustics_scatter", 25, 0, 100, 5, DK_NUM, DN_HIGH, "%"},
    /*  5 */ {"acoustics_decay", 100, 10, 400, 10, DK_NUM, DN_EFX, "%"},
    /*  6 */ {"acoustics_decay_hf", 100, 10, 400, 10, DK_NUM, DN_EFX, "%"},
    /*  7 */ {"acoustics_decay_lf", 100, 10, 400, 10, DK_NUM, DN_EFX, "%"},
    /*  8 */ {"acoustics_refl_delay", 100, 0, 400, 10, DK_NUM, DN_EFX, "%"},
    /*  9 */ {"acoustics_late_delay", 100, 0, 400, 10, DK_NUM, DN_EFX, "%"},
    /* 10 */ {"acoustics_reverb_gain", 32, 0, 100, 2, DK_NUM, DN_EFX, "%"},
    /* 11 */ {"acoustics_ease_ms", 170, 0, 2000, 10, DK_NUM, DN_EFX, " ms"},
    /* 12 */ {"acoustics_filter_ms", 300, 0, 2000, 20, DK_NUM, DN_AC, " ms"},
    /* 13 */ {"acoustics_room_rays", 64, 16, 512, 16, DK_NUM, DN_AC, " rays"},
    /* 14 */ {"acoustics_room_legs", 12, 3, 48, 1, DK_NUM, DN_AC, " bounces"},
    /* 15 */ {"acoustics_room_reach", 16, 4, 64, 2, DK_NUM, DN_AC, " squares"},
    /* 16 */ {"acoustics_way_reach", 12, 4, 32, 1, DK_NUM, DN_AC, " squares"},
    /* 17 */ {"fx_makeup_stereo", 215, 25, 1000, 5, DK_NUM, DN_AUDIO, "%"},
    /* 18 */ {"fx_makeup_hrtf", 520, 25, 2000, 10, DK_NUM, DN_AUDIO, "%"},
    /* 19 */ {"acoustics_echo_ahead", 4, 2, 6, 1, DK_NUM, DN_HIGH, " blocks"},
    /* 20 */ {"acoustics_echo_direct", 1, 0, 1, 1, DK_ONOFF, DN_HIGH, ""},
    /* 21 */ {"acoustics_steam_direct", 1, 0, 1, 1, DK_ONOFF, DN_HIGH, ""},
    /* 22 */ {"acoustics_steam_occlusion", 1, 0, 1, 1, DK_ONOFF, DN_HIGH, ""},
    /* 23 */ {"acoustics_direct_level", 30, 0, 100, 2, DK_NUM, DN_HIGH, "%"},
    /* 24 */ {"acoustics_source_cm", 50, 5, 300, 5, DK_NUM, DN_HIGH, " cm"},
    /* 25 */ {"acoustics_door_frames", 1, 0, 1, 1, DK_ONOFF, DN_AC, ""},
    /* 26 */ {"fx_music_interrupt", 0, 0, 1, 1, DK_ONOFF, DN_AUDIO, ""},
    /* 27 */ {"fx_step_plain", 0, 0, 1, 1, DK_ONOFF, DN_AUDIO, ""},
    /* 28 */ {"menu_music", 1, 0, 1, 1, DK_ONOFF, DN_ALWAYS, ""},
    /* 29 */ {"music_buffer_ms", 150, 50, 600, 25, DK_NUM, DN_ALWAYS, " ms"},
    /* 30 */ {"music_thread_high", 1, 0, 1, 1, DK_ONOFF, DN_ALWAYS, ""},
    /* 31 */ {"acoustics_steam_pathing", 1, 0, 1, 1, DK_ONOFF, DN_HIGH, ""},
    /* 32 */ {"acoustics_path_level", 100, 0, 400, 10, DK_NUM, DN_HIGH, "%"},
    /* 33 */ {"acoustics_things", 1, 0, 1, 1, DK_ONOFF, DN_AC, ""},
    /* 34 */ {"fx_roland_reverb", 2, 0, 2, 1, DK_AUTO, DN_AUDIO, ""},
    /* 35 */ {"acoustics_embree", 1, 0, 1, 1, DK_ONOFF, DN_HIGH, ""},   /* (Steam Audio started again, below) */
    /* 36 */ {"music_roland_voices", 256, 32, 256, 32, DK_NUM, DN_ALWAYS, " voices"},
    /* 37 */ {"music_clean_volume", 1, 0, 1, 1, DK_ONOFF, DN_ALWAYS, ""},   /* (SOUND.C mus_clean, audio.c music_gain) */
    /* 38 */ {"fx_limiter", 1, 0, 1, 1, DK_ONOFF, DN_AUDIO, ""},   /* (OpenAL Soft's output limiter, uwaudio.c context_attrs; the sound started again) */
    /* 39 */ {"fx_heat_shimmer", 100, 0, 300, 10, DK_NUM, DN_AC, "%"},   /* (uwaudio.c heat_step, volfog.inc uwv_heat_spots) */
    /* 40 */ {"fx_own_step_volume", 100, 0, 200, 10, DK_NUM, DN_AUDIO, "%"},   /* (uwaudio.c own_step_vol, on top of Footstep volume) */
    /* (2026-11-19, the sound review: work\soundreview\REPORT-2026-11-19.md) */
    /* 41 */ {"acoustics_echo_voices", 16, 1, 32, 1, DK_NUM, DN_HIGH, " sounds"},
    /* 42 */ {"acoustics_direct_thread", 1, 0, 1, 1, DK_ONOFF, DN_HIGH, ""},
    /* 43 */ {"acoustics_steam_speakers", 1, 0, 1, 1, DK_ONOFF, DN_HIGH, ""},
    /* 44 */ {"acoustics_head", 0, 0, 31, 1, DK_NUM, DN_HIGH, ""},       /* (dial_text: the file's name) */
    /* 45 */ {"acoustics_echo_order", 1, 1, 2, 1, DK_NUM, DN_HIGH, ""},  /* (Steam Audio started again: uwaudio_tune) */
    /* 46 */ {"acoustics_scatter_by_mat", 1, 0, 1, 1, DK_ONOFF, DN_HIGH, ""},
    /* 47 */ {"acoustics_materials", 0, 0, 1, 1, DK_NUM, DN_AC, ""},     /* (the sound started again: the table is read then) */
    /* 48 */ {"acoustics_surround", 1, 0, 1, 1, DK_ONOFF, DN_HIGH, ""},  /* (the sound started again: uwaudio.c hi_channels) */
    /* 49 */ {"fx_feet_below", 1, 0, 1, 1, DK_ONOFF, DN_AUDIO, ""},       /* (uwaudio.c own_place) */
};
#define SOUND_NDIALS ((int)(sizeof SOUND_DIALS / sizeof SOUND_DIALS[0]))
static int dial_val[sizeof SOUND_DIALS / sizeof SOUND_DIALS[0]], dials_read;
static void dials_load(void);
/* the Roland voices row, for the music's Munt as it opens (our copy of the runtime's sound\audio.c): the value of
   this start; a change in the menu is heard from the next start */
static int roland_voices_run = -1;
int port_roland_voices(void)
{
    dials_load();
    if (roland_voices_run < 0) roland_voices_run = dials_read ? dial_val[36] : SOUND_DIALS[36].dflt;
    return roland_voices_run;
}
void uwf_our_data(char *out, size_t n);
static void fx_acoustics_tune(void);
/* the dials read from settings.txt once, each kept inside its range */
static void dials_load(void)
{
    char data[UWF_PATH];
    if (dials_read || !plat_base_dir()) return;
    uwf_our_data(data, sizeof data);
    for (int i = 0; i < SOUND_NDIALS; i++) {
        int v = uwaudio_setting(data, SOUND_DIALS[i].key, SOUND_DIALS[i].dflt);
        dial_val[i] = v < SOUND_DIALS[i].lo ? SOUND_DIALS[i].lo : v > SOUND_DIALS[i].hi ? SOUND_DIALS[i].hi : v;
    }
    dials_read = 1;
}
/* a dial's key line of our settings.txt written (or added): the file read, the one line changed, written again, as the
   renderer does for its own rows (src\uwview.c settings_save) */
static void fx_setting_save(const char *key, int value)
{
    static char out[64 * 1024];
    char data[UWF_PATH], p[UWF_PATH + 32], line[512];
    size_t n = 0, kl = strlen(key);
    int done = 0;
    FILE *f;
    if (!plat_base_dir()) return;
    uwf_our_data(data, sizeof data);
    snprintf(p, sizeof p, "%s/settings.txt", data);
    if ((f = fopen(p, "r")) == NULL && errno != ENOENT)   /* (the file written below then holds this key alone) */
        RLOG_E("front", "cannot open %s: %s", p, rlog_errno());
    if (f != NULL) {
        while (fgets(line, sizeof line, f)) {
            if (!done && !strncmp(line, key, kl) && (line[kl] == ' ' || line[kl] == '\t')) {
                n += (size_t)snprintf(out + n, sizeof out - n, "%s %d\n", key, value), done = 1;
                continue;
            }
            n += (size_t)snprintf(out + n, sizeof out - n, "%s", line);
            if (n >= sizeof out - 600) { RLOG_E("front", "%s is too long (over %zu bytes): its rest is dropped", p, sizeof out - 600); break; }
        }
        fclose(f);
    }
    if (!done) n += (size_t)snprintf(out + n, sizeof out - n, "%s %d\n", key, value);
    if ((f = fopen(p, "w")) != NULL) {
        if (fwrite(out, 1, n, f) != n) RLOG_E("front", "cannot write %s: %s", p, rlog_errno());
        if (fclose(f) != 0) RLOG_E("front", "cannot write %s: %s", p, rlog_errno());
    } else RLOG_E("front", "cannot open %s: %s", p, rlog_errno());
}
/* a dial's row is shown: its part of the sound runs */
static int dial_shown(int i)
{
    int ac = uwaudio_acoustics_on(), need = SOUND_DIALS[i].need;
    return need == DN_ALWAYS ? 1 : need == DN_AUDIO ? uwaudio_on() : need == DN_AC ? ac >= 1 : need == DN_HIGH ? ac == 2 : ac == 1;
}
/* a dial's row value */
static void dial_text(int i, char *out, size_t n)
{
    int v;
    dials_load();
    v = dial_val[i];
    switch (SOUND_DIALS[i].kind) {
    case DK_ONOFF: snprintf(out, n, "%s", v ? "On" : "Off"); break;
    case DK_SECONDS: snprintf(out, n, "%d.%02d s", v / 1000, v % 1000 / 10); break;
    case DK_BOUNCES: if (v) snprintf(out, n, "%d", v); else snprintf(out, n, "As Echo detail"); break;
    case DK_AUTO: snprintf(out, n, "%s", v == 2 ? "Automatic" : v ? "On" : "Off"); break;
    default: snprintf(out, n, "%d%s", v, SOUND_DIALS[i].unit); break;
    }
    if (i == 44) {   /* the head: Steam Audio's own, or a .sofa file of our data's hrtf folder */
        static char names[32][96];
        char data[UWF_PATH];
        uwf_our_data(data, sizeof data);
        int nh = uwaudio_heads(data, names, 32);
        if (v == 0) snprintf(out, n, "Steam Audio's own");
        else if (v <= nh) {
            snprintf(out, n, "%s", names[v - 1]);
            size_t l = strlen(out);
            if (l > 5 && !strcmp(out + l - 5, ".sofa")) out[l - 5] = 0;
        } else snprintf(out, n, "(file %d is not there)", v);
    }
    if (i == 45) snprintf(out, n, "%s", v == 2 ? "Fine (2nd order, 9 channels)" : "Plain (1st order, 4 channels)");
    if (i == 47) snprintf(out, n, "%s", v == 1 ? "Earlier (2026-11-14)" : "Researched (2026-11-19)");
    if (i == 36) {   /* the Roland voices: 32 is the real unit; a change is heard from the next start */
        size_t l = strlen(out);
        if (v == 32 && l < n) snprintf(out + l, n - l, " (original)"), l = strlen(out);
        if (roland_voices_run >= 0 && v != roland_voices_run && l < n) snprintf(out + l, n - l, " (from the next start)");
    }
}
static void fx_audio_start(int again);
/* a row's change that needs the positional sound started again asks for it here; fx_pump does it once, a third of a
   second after the last asking (a preset steps several such rows one after the other: one restart, not one each) */
static uint64_t fx_restart_at;
static void fx_restart_soon(void) { fx_restart_at = ms_now() + 300; }
/* a dial stepped, kept in settings.txt and heard at once (or the sound started again) */
static void dial_step(int i, int step)
{
    int v;
    dials_load();
    v = dial_val[i];
    if (SOUND_DIALS[i].kind == DK_ONOFF) v = !v;
    else if (SOUND_DIALS[i].kind == DK_AUTO) v = (v + (step > 0 ? 1 : 2)) % 3;
    else v += step > 0 ? SOUND_DIALS[i].step : -SOUND_DIALS[i].step;
    v = v < SOUND_DIALS[i].lo ? SOUND_DIALS[i].lo : v > SOUND_DIALS[i].hi ? SOUND_DIALS[i].hi : v;
    if (i == 44) {   /* (the head: no further than the files there are) */
        static char names[32][96];
        char data[UWF_PATH];
        uwf_our_data(data, sizeof data);
        int nh = uwaudio_heads(data, names, 32);
        if (v > nh) v = nh;
    }
    if (v == dial_val[i]) return;
    dial_val[i] = v;
    fx_setting_save(SOUND_DIALS[i].key, v);
    if (i == 34 || i == 35 || i == 38 || i == 47 || i == 48) { fx_restart_soon(); return; }   /* (the Roland's own reverb is in the renderings: made again;
                                                               the limiter is the device's: opened again;
                                                               the echoes' ray tracer is chosen as Steam Audio starts) */
    fx_acoustics_tune();   /* heard at once */
}
static const struct { int id; const char *label; int opt; const char *hint; } MENU_ROWS[] = {
    {MO_HEAD, "#Game", -1, ""},
    {MO_RESUME, "Resume", -1, "Close this menu and go on playing. Esc does the same."},
    {MO_SAVE, "Save game (F5)", -1, "Opens the save page: pick one of four slots. A slot already used is overwritten."},
    {MO_LOAD, "Load game (F9)", -1, "Opens the load page: pick one of your four saved games. Anything not saved is lost."},
    {MO_NEW, "New game (title screen)", -1, "Back to the title screen to start a new character. Anything not saved is lost."},
    {MO_SETSCREEN, "Start-up settings (F11)", -1, "Opens the settings screen the game has at its start: mouse capture, mouse-look speed, master volume, the window and its scaling, the folders, recording sessions. F11 closes it."},
    {MO_QFADES, "Quick fades", -1, "On: the map and the game's screens fade in at once. Off: the original's half-second fades. Default On."},
    {MO_SEENMAP, "Map what I saw", -1, "On: the map shows every place you have seen in the 3D view, kept with your saves. Off: the original's map alone, which only marks what its own narrow, short view saw. Default On."},
    {MO_MAPARROWS, "Map level arrows", -1, "Levels 1 to 8: the map's arrows stop at the last level that has a map. Original: they go on to level 99, every page past 8 blank."},
    {MO_MAPNOTES, "Map notes kept right", -1, "On: notes you erase stay erased and every note you keep is kept. Off: the original's faults (an erased last note comes back, of two erased notes in a row one stays). Default On."},
    {MO_LOGDETAIL, "Log detail", -1, "How much the game writes to uwr.log. Normal is enough unless you are chasing a problem."},
    {MO_QUIT, "Quit the game", -1, "Closes the game at once, without asking. Anything not saved is lost."},
    {MO_HEAD, "#Display", -1, ""},
    {MO_DISPLAY, "Display", -1, "Ours: the new 3D view with everything we added. The original's: the 1992 game as it was, all our changes off, its own keys."},
    {MO_FULL, "Full screen", -1, "On: the game fills the whole screen. Off: it runs in a window."},
    {MO_RENDERER, "Renderer", -1, "The graphics system that draws the picture: Vulkan (the default) or Direct3D 12. Takes hold at the next start."},
    {MO_FOV, "Field of view", 6, "How wide an angle you see: 50 to 120 degrees, in steps of 5. Higher shows more at the sides. Default 85."},
    {MO_SMOOTH, "Smooth motion", 16, "On: the camera glides between the game's movement steps. Off: it jumps from step to step. Default On."},
    {MO_RVSYNC, "Vertical sync", 90, "On: each picture waits for the screen's refresh, so there is no tearing. Off: pictures go out as soon as they are made. From the next start. Default On."},
    {MO_FADE, "Message fade", -1, "Seconds the message box stays on screen after the newest message: 2 to 30, or Never (it stays). Default 5."},
    {MO_DOORSTEP, "Shut doors from close up", -1, "On: a door you shut while standing just inside its swing still shuts. Off: as the original, it swings open again."},
    {MO_DOORPICK, "Easier door targeting", -1, "On: point at a door's frame, or its open leaf from any side, to use it. Off: as the original, the leaf only."},
    {MO_DOORCUE, "Door blocked message", -1, "On: a message tells you when a door swings open again because you stand in its way. Step back and it shuts."},
    {MO_FONT, "Font", 42, "The letters of the whole interface: an easy-to-read font of your choice, or The game's own."},
    {MO_FONTSIZE, "Font size", 43, "Size of the interface's letters, in steps of 5%. 100% is the size picked for your screen resolution."},
    {MO_MENUFONT, "Menu text", 18, "This menu's letters: Smooth (easy to read, the default) or the original game's small capitals."},
    {MO_MENUBG, "Menu background", 63, "What lies behind this menu's rows and text: Solid colour (dark, easiest to read, the default) or Leather, the game's own."},
    {MO_MENUFAST, "Quick menu drawing", 64, "On: this menu is drawn again only when something in it changes, and the pointer moves at the game's full speed. Off: the whole menu is drawn again every frame, which can make the pointer jerky. Default On."},
    {MO_SYSPTR, "Pointer in menus", 65, "System: in this menu and the inventory the pointer is the system's own, which moves as smoothly as your mouse whatever the game's frame rate (it looks the same: the game's cross, or the thing you carry). Drawn: the game draws it into its picture, as before, so it moves only as often as a frame is drawn. Default System."},
    {MO_CONVFONT, "Conversation text", 17, "The letters in conversations: Smooth (easy to read, the default) or the original game's font."},
    {MO_LOGFONT, "Message text", 41, "The message box's letters: Smooth (smaller and easier to read, the default) or the original game's font."},
    {MO_CHARSHEET, "Character sheet", 40, "The sheet in the inventory (Tab). Full: every number the rules use (the default). Or the game's own statistics page."},
    {MO_HEAD, "#Graphics", -1, ""},
    {MO_RES, "Resolution", 23, "Information only: the size the 3D view is drawn at -> the size shown on screen. The DLSS rows below set it."},
    {MO_DLSS, "DLSS Super Resolution", 0, "NVIDIA DLSS makes the picture sharp and steady, and can enlarge a smaller one for speed. Off: plain full size."},
    {MO_DLSSMODE, "DLSS mode", 21, "The size drawn before DLSS enlarges it. DLAA: full size, the best picture. The others: smaller and faster."},
    {MO_RT, "Ray tracing", 3, "On: light and shadows are worked out by tracing rays of light, on graphics cards that can do it."},
    {MO_RR, "DLSS Ray Reconstruction", 22, "NVIDIA's cleaner for the grain in ray-traced light; it also enlarges the picture. Needs DLSS and Ray tracing."},
    {MO_DLSS5, "DLSS 5", 1, "NVIDIA's neural rendering: it reworks the DLSS picture. Only shown on graphics cards that have it."},
    {MO_STYLE, "DLSS 5 style", 2, "The look DLSS 5 gives the picture: A - Default, B - Natural or C - Cinematic."},
    {MO_D5INT, "DLSS 5 intensity", 24, "How strongly DLSS 5 changes the picture: 0% to 100%, in steps of 5. Higher is stronger."},
    {MO_D5STR, "DLSS 5 structure", 25, "How much fine local detail DLSS 5 adds: 0% to 100%, in steps of 5. Higher is more."},
    {MO_RD5TONE, "DLSS 5 tone", 101, "DLSS 5's tone, 0.00 to 2.00 in steps of 0.05. From the next start. Default 1.00."},
    {MO_RD5SAT, "DLSS 5 saturation", 102, "DLSS 5's colour strength, 0.00 to 2.00 in steps of 0.05. From the next start. Default 1.15."},
    {MO_BOUNCE, "Bounced light", 4, "On: light bounces off walls and floors into the shadows, so they are less dark. Off: no bounced light."},
    {MO_RBOUNCES, "Light bounces", 96, "How many times bounced light bounces again: 1 to 4, or As many as build up (the light probes feed back). Default 1."},
    {MO_RRAYLEN, "Bounce ray reach", 97, "How far a bounced light ray goes: 2 to 32 tiles. Longer finds more light, a little slower. Default 8."},
    {MO_SHADOWS, "Object shadows (.)", 9, "On: objects and creatures cast shadows (needs Ray tracing). The . key switches it too."},
    {MO_TEXRES, "Textures", 52, "The pictures on walls and floors: Original, or sharper copies enlarged to 256 or 512 pixels in the same colours."},
    {MO_MAPNORMAL, "Normal map detail", 53, "Detail of the maps that give the stone its bumps: As the textures, Original (64 pixels), 256 or 512."},
    {MO_MAPAO, "Joint shadow map detail", 54, "Detail of the maps that darken the joints between stones: As the textures, Original, 256 or 512."},
    {MO_MAPHEIGHT, "Relief height map detail", 55, "Detail of the height maps the stones' own small shadows come from: As the textures, Original, 256 or 512."},
    {MO_RELIEF, "Normal maps (F6)", 5, "On: light picks out the bumps and grooves of the stone, so walls and floors look less flat. F6 switches it too."},
    {MO_NSTR, "Normal map strength", 8, "How deep the stone's bumps look: 0.25 (faint) to 4.00 (strong), in steps of 0.25. Used by Normal maps."},
    {MO_RSHEEN, "Stone sheen", 95, "A shine on the stone walls and floors: 0.00 (none, the default) to 1.00."},
    {MO_TEXAO, "Joint shadows (AO)", 50, "Darkens the joints between the stones of walls and floors: Off, or 25% to 200%. Higher is darker."},
    {MO_TEXSHADOW, "Relief shadows", 51, "Stones cast small shadows of their own from each light: Off, or Depth 1 to 8. Higher: deeper relief, more shadow."},
    {MO_RSPRN, "Sprite normal strength", 98, "How strongly the objects' and creatures' pictures are lit by their normal maps: 0.00 (flat) to 1.00. Default 0.50."},
    {MO_RSPRAO, "Sprite joint shadows", 99, "How dark the objects' and creatures' occlusion maps make their creases: 0.00 (none) to 1.00. Default 0.50."},
    {MO_RSPRH, "Sprite relief depth", 100, "How deep the objects' and creatures' height maps look, 0 to 4 texels. Default 0 (off)."},
    {MO_LIGHTCOL, "Light colour", 31, "The colour maths for how your eye adapts to firelight: CAT16 (the newer standard, the default) or Bradford."},
    {MO_LIGHTFADE, "Light fade", 62, "How light dims with distance. 2 / 3 / 4 tiles: half at 2 tiles, 30% at 3, dark at 4. Original: the game's shading."},
    {MO_RHAND, "Your light", 94, "Where your torch or lantern is held: Over the head (the default) or At the eye."},
    {MO_BREATH, "Fire breathing", 61, "Now and then a fire sags or flares for a few seconds, like real flames. Off, or 10% to 40%: how strongly."},
    {MO_ADAPT, "Light adaptation", 32, "How far your eye gets used to firelight: 0% leaves it orange, 100% makes it white. Automatic works it out."},
    {MO_REXPOSE, "Exposure", 91, "The picture's overall brightness before tone mapping: 0.50 to 4.00 in steps of 0.25. From the next start. Default 2.00."},
    {MO_RGAMMA, "Light gamma", 92, "The lights' falloff curve: 0.50 to 2.00 in steps of 0.05. Higher keeps rooms brighter away from the lights. Default 1.00."},
    {MO_HDR, "HDR output", 33, "On an HDR monitor (HDR on in Windows): fire and bright light go brighter than white. Other monitors: no change."},
    {MO_HDRPEAK, "HDR peak brightness", 34, "The brightest that HDR highlights go: The monitor's (what it reports), or 400 to 4000 nits in steps of 100."},
    {MO_HDRPAPER, "HDR paper white", 35, "How bright plain white and the interface are in HDR: 80 to 500 nits, in steps of 10. Default 200."},
    {MO_HEAD, "#Sprites", -1, ""},
    {MO_SPRPAGE, "", -1, ""},   /* (a row for each object picture of ours that is there: spr_rows_build) */
    {MO_HEAD, "#Effects", -1, ""},
    {MO_PARTICLES, "Particles", 10, "On: sparks, smoke and dust in the air. Off: none."},
    {MO_DECALS, "Decals", 11, "On: marks such as blood are left on floors and walls. Off: none."},
    {MO_WATER, "Water", 12, "On: water moves and bends the view of what is under it. Off: plain water."},
    {MO_FOG, "Fog and light shafts", 13, "Fog, and shafts of light through it."},
    {MO_CAMPSMOKE, "Campfire smoke", 83, "On: grey smoke rises from every campfire, spreads and thins out over a few seconds. Needs Particles. Off: none."},
    {MO_WATERMIST, "Water fog", 84, "On: a thin pale fog lies over the water near you and drifts slowly. Needs Particles. Off: none."},
    {MO_GLOWSTONES, "Glowing stones", 80, "On: glowing rocks, orbs and orb rocks shine and light the floor about a foot round them. Off: as plain objects."},
    {MO_MOONLIGHT, "Moonstone light", 81, "The moonstone fills the room it lies in with a deep blue light. Off, or 5% to 50% of a candle. Default 10%."},
    {MO_ELEMLIGHT, "Fire elemental light", 82, "Fire elementals cast a smouldering orange light as they move. Off, or 10% to 100% of a candle. Default 30%."},
    {MO_RCAMPSW, "Campfire swell", 103, "How much the campfires swell and shrink as they burn: 0.00 (steady) to 0.60. Default 0.35."},
    {MO_RFLAMESW, "Flame swell", 104, "How much the other fires (torches, braziers, burning things) swell and shrink: 0.00 (steady) to 0.50. Default 0.15."},
    {MO_HEAD, "#Fog and smoke", -1, ""},
    {MO_VOLPAGE, "", -1, ""},   /* (the renderer's rows: vol_rows_build) */
    {MO_HEAD, "#Sound", -1, ""},
    {MO_MUSIC, "Music on / off", -1, "Turns the game's music on or off."},
    {MO_MUSVOL, "Music volume", -1, "How loud the music is: 0% to 100%, in steps of 5."},
    {MO_SOUND, "Sound on / off", -1, "Turns the game's sound effects on or off."},
    {MO_FXVOL, "Sound volume", -1, "How loud the sound effects are: 0% to 100%, in steps of 5."},
    {MO_CARDMUSIC, "Music and effects device", -1, "What plays music and effects. Only the Roland MT-32 (needs its ROM files) gives positional sound. From the next start."},
    {MO_CARDSPEECH, "Speech device", -1, "The device that plays the game's recorded speech, or None. Takes hold at the next start."},
    {MO_FXAUDIO, "Positional sound", 14, "On: each sound comes from the place where it happens. The sound rows below need it. Off: the game's plain sound."},
    {MO_HRTF, "Headphone 3D sound", 19, "For headphones: each sound is made for your two ears, so it comes from where it is, ahead, behind, above or below. Automatic: on when Windows sees headphones. The value says whether it is working now. Default On."},
    {MO_PANFIX, "Effects' sides", 20, "The Roland MT-32 swaps left and right. Corrected (the default) puts each sound on its true side."},
    {MO_HEARRANGE, "Hearing range", 56, "How far away a sound can be heard: Original (6 tiles), or 9, 12, 16 or 24 tiles for a longer fade-out."},
    {MO_FEETSPREAD, "Footstep width", 57, "How far apart your two feet sound: Middle (both centred), Original (barely apart), up to 800% (wide apart)."},
    {MO_STEPVOL, "Footstep volume", 59, "How loud all footsteps are, yours and the creatures', next to other sounds: Off, or 10% to 200%. Default 80%."},
    {MO_DIAL + 49, "Footsteps at your feet", -1, "On: your own footsteps come from below you, where your feet are, as they would. Off: from just in front of you at ear height, as before. Best heard with Headphone 3D sound. Default On."},
    {MO_DIAL + 40, "Your own footsteps", -1, "How loud your own footsteps are, on top of Footstep volume; the creatures' steps stay as they are: 0% (silent) to 200%. Default 100%."},
    {MO_STEPECHO, "Footstep echo", 60, "How strongly footsteps echo, next to other sounds' echoes: Off, or 10% to 200%. Default 50%. Acoustics High only."},
    {MO_FOOTSTEPS, "Footsteps by floor", 30, "On: steps sound like the floor under them (wood, water, earth, metal). Off: the original's two step sounds."},
    {MO_ACOUSTICS, "Acoustics", 15, "On: walls and shut doors muffle sounds, rooms echo by their size. High: lifelike echoes traced for every sound."},
    {MO_ECHOQ, "Echo detail", 58, "Sound rays traced for the echoes: Low (1,024) to Very high (8,192). More is smoother but works the processor harder."},
    {MO_ACWET, "Reverb level", 26, "How loud the rooms' echo is: Off, or 10% to 200%. Off leaves only the muffling. Default 100%."},
    {MO_ACSIZE, "Map square size", 27, "Metres to a map square: 1.5 to 4.0. Bigger makes every room sound bigger, with longer echoes. The game's own: 2.4."},
    {MO_ACOCCL, "Muffling by walls", 28, "How much rock and shut doors quieten and dull a sound: Off, or 10% to 100%. Default 100%."},
    {MO_ACDETOUR, "Sound round corners", 29, "On: a sound round a corner seems to come from the opening it passes through, not through the rock. Default On."},
    {MO_HEAD, "#Sound tuning", -1, ""},
    {MO_ROOMINFO, "Room measured here", -1, "Information only: how long this room echoes (low / middle / high notes) and how far sound travels between its walls."},
    {MO_DIAL + 35, "Fast echo rays", -1, "On: the echoes' sound rays are traced by Intel's Embree on several threads, about six times quicker. Off: Steam Audio's own, on two. Default On."},
    {MO_DIAL + 41, "Sounds with echoes", -1, "How many sounds at once have their own echoes: 1 to 32. Up to 64 sounds play at once, every one shaped by Steam Audio; the newest this many also echo. More costs processor time. Default 16."},
    {MO_DIAL + 42, "Muffling on its own thread", -1, "On: Steam Audio's muffling and sound paths are worked out on a thread of their own, so the sound is never late waiting for them. Off: worked out while the sound is mixed, as before. Default On."},
    {MO_DIAL + 43, "Steam Audio on speakers", -1, "On: with speakers (no Headphone 3D) Steam Audio still shapes each sound, with its muffling and its paths round corners, panned between the speakers. Off: speakers get OpenAL's sound, as before. Default On."},
    {MO_DIAL + 48, "Surround speakers", -1, "On: with 4, 5.1 or 7.1 speakers, Steam Audio's sound and echoes come from every speaker around you. Off: from the front two only. Headphones are not changed. Starts the sound again. Default On."},
    {MO_DIAL + 44, "Head shape", -1, "The head your headphone sound is made for: Steam Audio's own, or a measured head (.sofa file) in uwr\\hrtf. Pick the one where sounds behind and above you sound most real. Starts the echoes again."},
    {MO_DIAL + 45, "Echo direction detail", -1, "Plain: echoes come from 4 broad directions. Fine: 9, so each echo comes more clearly from its wall; about twice the work. Starts the echoes again. Default Plain."},
    {MO_DIAL + 21, "Sound by Steam Audio", -1, "On: Steam Audio shapes each sound for your two ears. Off: OpenAL does. Needs Headphone 3D sound. Default On."},
    {MO_DIAL + 22, "Muffling by Steam Audio", -1, "On: Steam Audio works out what walls, doors and floors let through, by their material. Off: the game's own rays."},
    {MO_DIAL + 23, "Sound level (Steam Audio)", -1, "How loud the sounds played by Steam Audio are, next to the game's own mix: 0% to 100%. Default 30%."},
    {MO_DIAL + 31, "Sound paths by Steam Audio", -1, "On: a sound you cannot see reaches you along real routes, round corners and through open doors. A shut door blocks it."},
    {MO_DIAL + 32, "Sound paths level", -1, "How loud a sound is when it reaches you along a route round corners: 0% to 400%. Default 100%."},
    {MO_DIAL + 24, "Sound source size", -1, "How big a sound's source counts as: 5 to 300 cm. Bigger: it fades more gently behind a corner. Default 50 cm."},
    {MO_DIAL + 29, "Music buffer", -1, "Music kept ready ahead: 50 to 600 ms. More: fewer gaps when the PC is busy, but the music answers later. Default 150."},
    {MO_DIAL + 30, "Music thread first", -1, "On: making the music comes before the game's other work, so a busy moment leaves no gap in the tune. Default On."},
    {MO_DIAL + 37, "Music volume, clean", -1, "On: the Roland plays the music at its own full level and the Music volume turns the sound down afterwards, so the Roland itself (its reverb too) always works at full precision; as loud as the old way at every step. Off: the volume is turned down inside the Roland (the old way). Default On."},
    {MO_DIAL + 36, "Roland voices", -1, "Notes the Roland can sound at once for the music: the real unit has 32, and a busy passage cuts notes short. More: none cut. From the next start. Default 256."},
    {MO_DIAL + 28, "Music in the menu", -1, "On: the music plays on while this menu is open and while a new level is being built. Off: it stops with the paused game. Default On."},
    {MO_DIAL + 39, "Heat shimmer", -1, "A sound heard through the hot air over a fire, lava or a blast flickers a little, the high notes most, as real hot air bends sound. 0% off, 100% as real, up to 300%. Default 100%."},
    {MO_DIAL + 38, "Loudness limiter", -1, "On: when many loud sounds land at once, the effects are turned down for that moment instead of crackling. Quieter moments are untouched. Off: the sound can crackle at its loudest. Default On."},
    {MO_DIAL + 27, "Own footsteps plain", -1, "On: your own footsteps play untouched: no echo, floor sound, 3D or muffling. For comparing by ear. Default Off."},
    {MO_DIAL + 34, "Roland's own reverb", -1, "The Roland adds a small room echo to every effect. Automatic: off while Acoustics adds the real room's. On: always."},
    {MO_DIAL + 26, "Effects interrupt music", -1, "On: as the original, each sound effect cuts notes out of the music. Off (the default): the music plays whole."},
    {MO_DIAL + 33, "Furniture in the sound", -1, "On: tables, barrels, chests, pillars and boulders block and reflect sound, each by its material. Off: empty rooms."},
    {MO_DIAL + 25, "Doorways their real size", -1, "On: for sound, a doorway is a narrow opening in a wall. Off: an open door leaves its whole square open. Default On."},
    {MO_DIAL + 0, "Echoes at once", -1, "On: a new sound echoes at once, with the echo of where you stand until its own is ready. Off: echoes can come late."},
    {MO_DIAL + 1, "Echo strength", -1, "How loud echoes are next to the sound itself: 0% to 60%. Reverb level then scales them. Default 5%."},
    {MO_DIAL + 2, "Longest echo", -1, "The longest an echo may ring: 0.5 to 6 seconds. A change starts the echoes again. Default 2 s."},
    {MO_DIAL + 3, "Echo bounces", -1, "How often a sound ray bounces before it is dropped: 4 to 64. Too few cuts echoes short. Default: As Echo detail."},
    {MO_DIAL + 4, "Wall roughness", -1, "How much of each bounce scatters every way instead of reflecting as off a mirror: 0% to 100%. Default 25%."},
    {MO_DIAL + 46, "Roughness by material", -1, "On: each material scatters as it would (rough rock and cloth a lot, marble, metal and water hardly), as acoustics.txt says; Wall roughness for any it does not. Off: Wall roughness for all. Default On."},
    {MO_DIAL + 47, "Material table", -1, "Researched: stone, rock, brick, marble, plaster, wood and the rest from published measurements (marble halls ring longer). Earlier: every stone surface rough rock, as until 2026-11-19. Starts the sound again."},
    {MO_DIAL + 19, "Echo buffer", -1, "Blocks of echo (11 ms each) kept ready ahead: 2 to 6. Too few: crackling. More: echoes come later. Default 4."},
    {MO_DIAL + 20, "Echoes straight to the ears", -1, "On: echoes go to your headphones as made. Off: placed again by Headphone 3D sound, duller. Default On."},
    {MO_DIAL + 5, "Ring time", -1, "How long rooms echo, against what the sound rays measure: 10% to 400%. 100% (the default): as measured."},
    {MO_DIAL + 6, "Ring time, high notes", -1, "How long high notes echo, against what the sound rays measure: 10% to 400%. 100% (the default): as measured."},
    {MO_DIAL + 7, "Ring time, low notes", -1, "How long low notes echo, against what the sound rays measure: 10% to 400%. 100% (the default): as measured."},
    {MO_DIAL + 8, "First echo delay", -1, "The wait before the first echoes, against the measured distance to the nearest surfaces: 0% to 400%. Default 100%."},
    {MO_DIAL + 9, "Late ring delay", -1, "The wait before the room's long echo follows the first echoes, against what is measured: 0% to 400%. Default 100%."},
    {MO_DIAL + 10, "Reverb strength", -1, "The reverb's own loudness: 0% to 100%. Reverb level then scales it. Default 32%."},
    {MO_DIAL + 11, "Room change time", -1, "How fast the echo changes as you enter another room: 0 to 2000 ms; all but done after 3 times this. Default 170."},
    {MO_DIAL + 12, "Muffling change time", -1, "How fast a sound turns muffled or clear when a wall or door comes between: 0 to 2000 ms. Default 300."},
    {MO_DIAL + 13, "Room rays", -1, "How many sound rays measure the room round you: 16 to 512. Default 64."},
    {MO_DIAL + 14, "Room ray bounces", -1, "How many bounces each room ray is followed for: 3 to 48. Default 12."},
    {MO_DIAL + 15, "Room ray reach", -1, "How far a room ray may go before its sound counts as gone: 4 to 64 map squares. Default 16."},
    {MO_DIAL + 16, "Corner reach", -1, "How far round corners a sound is followed: 4 to 32 map squares walked. Default 12."},
    {MO_DIAL + 17, "Loudness, speakers", -1, "Loudness of positional sound against the game's own mix, Headphone 3D sound off: 25% to 1000%. Default 215%."},
    {MO_DIAL + 18, "Loudness, headphone 3D", -1, "Loudness of positional sound against the game's own mix, Headphone 3D sound on: 25% to 2000%. Default 520%."},
    {MO_HEAD, "#Controls", -1, ""},
    {MO_MOUSE, "Mouse sensitivity", 7, "How fast the mouse turns the view: 0.1 (slow) to 4.0 (fast), in steps of 0.1. Default 1.0."},
    {MO_RINVERT, "Invert mouse", 93, "On: moving the mouse forward looks down, as a flight stick. Off: forward looks up. Default Off."},
    {MO_HEAD, "#Keys", -1, ""},
    {MO_KEYPAGE, "", -1, ""},
    {MO_HEAD, "#AI", -1, ""},
    {MO_AILEVEL, "Creature AI", -1, "Original: as shipped. Devious: each creature is cunning on its own. Monstrous: they also work together, as packs."},
    {MO_FEUD, "Rival creatures fight", -1, "Now and then two creatures of rival kinds (the two goblin tribes; trolls and mountain folk against goblins; spiders hunting rats) that have not noticed you fight each other. Rare: about once in a long while. Off: as the original, where they never do."},
    {MO_AIFRIENDS, "Friendly creatures", -1, "Friendly creatures nod to you, follow a while if your weapon is drawn, back away, or point out a foe. Off: as the original."},
    {MO_AIP0, "Hit and run", -1, "Creatures that shoot or cast wait up to 20 s for you to come in range, strike, then back off. At Monstrous they fall back to kin."},
    {MO_AIX0, "Hears you run", -1, "Creatures near you hear you run and come for you. Walking is silent to them: walk when you want to pass unheard."},
    {MO_AIX1, "Searches the ways out", -1, "A creature that loses you also checks the nearest door you may have left by, before it gives up the hunt."},
    {MO_AIX2, "More ambushers", -1, "Any clever creature may lie in ambush, not only goblins, mountainmen, lizardmen, outcasts and giant spiders."},
    {MO_AIX3, "Wary after a fight", -1, "A creature that lost you or ran from you in the last 5 minutes lies in wait for you the next time, and does not charge."},
    {MO_AIX10, "Takes cover", -1, "A creature you hurt from where it cannot get to you (across water, up a ledge) goes out of your sight and waits there."},
    {MO_AIX11, "Picks its ground", -1, "A clever creature holds a doorway or narrow passage and lets you come to it. Shooters put water or a drop between you."},
    {MO_AIX12, "Runs out of sight", -1, "A creature that flees runs for the nearest corner you cannot see round, not straight away from you, and hides there."},
    {MO_AIX13, "Learns how you fight", -1, "Shot from afar, a creature stops walking into it and waits out of sight. Shooters that felt your blade back off sooner."},
    {MO_AIX4, "Fetches kin", -1, "Monstrous only. A clever creature that loses you or hears you fetches the kin in its earshot and goes after you with them."},
    {MO_AIP1, "Calls for help", -1, "Monstrous only. A creature that meets you with kin in earshot but none fighting calls them to you. Once a minute at most."},
    {MO_AIP5, "Calls passed on", -1, "Monstrous only. A clever creature that is called repeats the call to its own neighbours, twice at most, so word spreads."},
    {MO_AIP4, "Strike together", -1, "Monstrous only. Kin called to a fight gather three squares short of it, then come at you together, after 10 s at most."},
    {MO_AIX6, "Cut off the retreat", -1, "Monstrous only. When two or more kin are called, one goes to the door behind you, so that your way back is closed."},
    {MO_AIX5, "The lure", -1, "Monstrous only. A clever creature shows itself, then falls back to kin who lie in wait, to lead you in among them."},
    {MO_AIP2, "Pack courage", -1, "Monstrous only. Spiders, bats and other dim creatures fall back to kin in earshot and attack together. Alone, they attack."},
    {MO_AIP3, "Fall back when hurt", -1, "Monstrous only. A clever creature under about half its health runs to its kin, once a fight, and comes back with them."},
    {MO_AIX16, "Surround", -1, "Monstrous only. When two kin already fight you hand to hand, the next one goes round to your side or back."},
    {MO_AIX14, "Won't be drawn out", -1, "Monstrous only. A creature does not follow you more than 8 squares from where its kin were: it goes back and waits."},
    {MO_AIX15, "Death cry", -1, "Monstrous only. Kin in earshot of a creature that dies are roused and wary. They go to the body, not to you."},
    {MO_AIX7, "Fighting roles", -1, "Monstrous only. Shooters and casters keep behind kin who fight you hand to hand, and back off when you come near them."},
    {MO_AIX8, "Alarm", -1, "Monstrous only. Kin called to a fight stay roused for 3 minutes: they watch the doors and corners near it and hunt again."},
    {MO_AIX9, "Hunters bring the pack", -1, "Monstrous only. A level hunter fetches its kin first, every time it goes after you. Off: hunters come alone."},
    {MO_AIP6, "AI messages", -1, "Lines in the message box: a creature shrieks for help, runs to its kin or backs away; a friendly one nods, follows or warns."},
    {MO_HUNTERS, "Level hunters", -1, "One or two clever creatures a level track you across the whole level by a plan: your last place, doors, light, kin."},
    {MO_AILOG, "AI log (Ctrl+L)", -1, "Each creature's decision and its reason, in the message box as it is made. Not kept: off again at the next start."},
    {MO_AILOGFILE, "AI log in the log file", -1, "Every decision is also written to home\\logs\\uwr.log with the game time, Ctrl+L or not. Each part's row counts its uses."},
    {MO_AIBY, "Decisions by", -1, "Who picks among a creature's choices. Rules: the built-in rulebook, at once. Language model: an AI model file, slower."},
    {MO_LLM, "Language model", -1, "The AI model file creatures decide with when Decisions by is Language model. Left and Right pick another file."},
    {MO_LLMGPU, "Model runs on", -1, "Graphics card: fast, uses its memory. Processor: slower, leaves the card alone. Takes hold when the model next loads."},
    {MO_LLMMODE, "Model answers by", -1, "Scoring: every choice rated in one pass, eight questions at once; fast. Grammar: the model writes its answer out; slower."},
    {MO_LLMRATE, "Model questions", -1, "The most questions creatures may put to the model each second: 1 to 60. Any beyond that are not asked. Default 20."},
    {MO_LLMSTALE, "Model answer too old", -1, "An answer that comes later than this is dropped, as the moment has passed: 1 to 12 s, or Never. Default 3 s."},
    {MO_LLMIDLE, "Model unloaded after", -1, "Minutes without a question before the model's memory is freed; it loads again when needed: 1 to 60, or Never. Default 10."},
    {MO_HEAD, "#Enhancements", -1, ""},
    {MO_ENH + ENH_SKIP_INTRO, "Skip the introduction", -1, "On: the game starts at its main menu, without the title and the introduction. From the next start. Default Off."},
    {MO_ENH + ENH_WRAP_MENU, "Menus wrap around", -1, "On: the start menu and the saved games wrap from the last item to the first. Default Off."},
    {MO_ENH + ENH_FAST_PANELS, "Fast panels", -1, "On: the right-hand panel slides twice as fast. Default Off."},
    {MO_ENH + ENH_FREE_HEADING, "Free heading", -1, "On: sliding along a wall no longer turns your view. Default Off."},
    {MO_ENH + ENH_SUBTITLES, "Cutscene subtitles", -1, "On: cutscenes show their text while the speech plays. Default Off."},
    {MO_ENH + ENH_SKILL_MESSAGES, "Skill messages", -1, "On: says which skill a skill point or a trainer raised, and to what. Default Off."},
    {MO_ENH + ENH_PERSPECTIVE, "Straight walls when looking up", -1, "On: in the original's 3D picture, floors, ceilings and walls stay straight when the view pitches. Default Off."},
    {MO_ENH + ENH_FULL_SPRITES, "Full-size sprites when looking up", -1, "On: in the original's 3D picture, creatures and objects keep their size when the view pitches. Default Off."},
    {MO_ENH + ENH_KEY_RING, "Key ring", -1, "On: a key that does not fit tries the other keys you carry, bags included, and uses the one that fits. Default Off."},
    {MO_ENH + ENH_TIDY_OBJECTS, "Fix: lost object slots", -1, "On: a stack too heavy to take part of no longer loses an object slot, and lost slots are freed. Changes play. Default Off."},
    {MO_ENH + ENH_RESTORE_AT_REST, "Fix: restore at rest", -1, "On: a restored game starts at rest, without the motion or fall it was saved in. Changes play. Default Off."},
    {MO_ENH + ENH_PLAIN_DEBRIS, "Fix: broken enchanted things", -1, "On: an enchanted object that breaks loses its spell, so its debris casts nothing. Changes play. Default Off."},
    {MO_ENH + ENH_ARMOUR_SPELLS, "Fix: armour spells", -1, "On: Resist Blows, Thick Skin and Iron Flesh add to your armour, as they were meant to. Changes play. Default Off."},
    {MO_ENH + ENH_TRUE_ENCHANTS, "Fix: enchantment names", -1, "On: a melee weapon's enchantment is named for what it does (Damage or Accuracy); the original names them the other way. Default Off."},
};
#define MENU_NROWS ((int)(sizeof MENU_ROWS / sizeof MENU_ROWS[0]))
/* The Fog and smoke tab's rows, as the renderer has them (uwv_vol_row: 0 a heading, 1 a switch, 2 a slider, 3 a
   choice, 4 an action that Enter does). A slider's row has its track drawn (vol_frac, to uwv_menu_sliders
   before the rows), is dragged with the left button (vol_drag: the row held) and takes a number typed in (vol_edit,
   shown as the row's value with a mark after it, until Enter gives it to uwv_vol_set). */
static float vol_frac[MENU_MAX];
static int vol_kind[MENU_MAX], vol_edit_on, vol_drag = -1;
static char vol_edit[16];
static int vol_have(void) { return V.dll && V.vol_rows && V.vol_row && V.vol_value; }
/* the Fog and smoke tab's rows, from the renderer */
static void vol_rows_build(const char **lab, const char **val, const char **hint, char vbuf[][128])
{
    int n = vol_have() ? V.vol_rows() : 0;
    for (int i = 0; i < n && menu_n < MENU_MAX - 1; i++) {
        const char *l = "", *h = "";
        int kind = 0;
        float f = -1;
        if (!V.vol_row(i, &l, &h, &kind, &f) || !l) continue;
        if (l[0] == '#') kind = 0;
        vbuf[menu_n][0] = 0;
        if (kind == 0) {   /* (a heading with no row under it is taken back, as the options') */
            if (menu_n && menu_ids[menu_n - 1] == MO_HEAD) menu_n--;
        } else V.vol_value(i, 0, vbuf[menu_n], 128);
        if (kind == 2 && vol_edit_on && menu_n == menu_sel) snprintf(vbuf[menu_n], 128, "%s_", vol_edit);   /* the number being typed */
        vol_kind[menu_n] = kind, vol_frac[menu_n] = kind == 2 ? (f < 0 ? 0 : f) : -1;
        menu_ids[menu_n] = kind == 0 ? MO_HEAD : MO_VOL + i, lab[menu_n] = l, val[menu_n] = vbuf[menu_n], hint[menu_n] = h ? h : "", menu_n++;
    }
    if (menu_n && menu_ids[menu_n - 1] == MO_HEAD) menu_n--;
}
/* The Sprites tab's rows: one for each object picture of ours that is there, as the renderer says
   (uwv_option 1000 + the object: its value, 0 when it has no picture of ours; 3000 + the object: its name). list: the
   tab is open, its rows are listed. 1 when there is any (the tab is shown). */
static char spr_lab[MENU_MAX][48];
static int spr_rows_build(int list, const char **lab, const char **val, const char **hint, char vbuf[][128])
{
    int any = 0;
    char v[128];
    if (!V.dll || !V.option) return 0;
    if (list && menu_n < MENU_MAX - 1 && V.option(1999, 0, v, sizeof v)) {   /* first: all of them at once */
        snprintf(vbuf[menu_n], 128, "%s", v);
        menu_ids[menu_n] = MO_SPRALL, lab[menu_n] = "All redrawn pictures", val[menu_n] = vbuf[menu_n];
        hint[menu_n] = "Turns every redrawn item picture on, or all of them off again. Each item also has its own row below.";
        menu_n++;
    }
    for (int k = 0; k < 512; k++) {
        if (!V.option(1000 + k, 0, v, sizeof v)) continue;
        any = 1;
        if (!list || menu_n >= MENU_MAX - 1) continue;
        snprintf(vbuf[menu_n], 128, "%s", v);
        if (!V.option(3000 + k, 0, spr_lab[menu_n], sizeof spr_lab[0])) snprintf(spr_lab[menu_n], sizeof spr_lab[0], "Object %03d", k);
        menu_ids[menu_n] = MO_SPR + k, lab[menu_n] = spr_lab[menu_n], val[menu_n] = vbuf[menu_n];
        hint[menu_n] = "This item's picture, lying in the world and in your pack. Original: the game's own. New: ours, redrawn at twice the detail. Changes at once.";
        menu_n++;
    }
    return any;
}
/* The tabs (each with a Default settings row and presets; a change of a row makes it Custom). A tab is a section of MENU_ROWS (the rows from one heading to the next); the menu lists the open tab's
   rows alone (menu_tab: kept while the game runs), the strip of names is drawn by the renderer (uwv_menu_tabs). A tab
   none of whose rows is shown now is left out (tab_shown: the rows shown in each, counted as the rows are built). */
#define MENU_TABS 16
static int menu_tab, menu_ntabs, tab_shown[MENU_TABS], tab_vis[MENU_TABS], tab_nvis, menu_shift;
static int tab_last[MENU_TABS];   /* the preset last taken on each tab, + 1: named first when two presets match */
/* tab t's name (its heading without the #) */
static const char *tab_name(int t)
{
    for (int i = 0, sec = -1; i < MENU_NROWS; i++)
        if (MENU_ROWS[i].id == MO_HEAD && ++sec == t) return MENU_ROWS[i].label + 1;
    return "";
}
static int menu_tab_of(int id)   /* the tab a row is on */
{
    for (int i = 0, sec = -1; i < MENU_NROWS; i++) {
        if (MENU_ROWS[i].id == MO_HEAD) sec++;
        else if (MENU_ROWS[i].id == id) return sec < 0 ? 0 : sec;
    }
    return 0;
}
/* A tab's presets. A preset is a list of rows and the value each should show; the tab's first preset, Default, lists
   every row the tab can put back (the value the game has when the settings say nothing), and each other preset is
   Default with the rows it lists changed. A value ending in * is matched by its start (the rest is the machine's,
   "DLAA (1920 x 1080)", or a count), one beginning with * anywhere in the text. A row that is not shown now, or
   reads "Not on this card" or "Needs ...", is passed over. own: set by Default alone (the other presets leave it).
   Not in any preset, as they need a restart or are the machine's: Display, Full screen, Renderer, Font, the two
   device rows, Music and Sound on / off (the game's own panel), the HDR levels, the language model's file and where
   it runs, the AI log's rows, and the rows that only tell. */
typedef struct { int id; const char *want; int own; } PresetRow;
static const PresetRow PR_DISPLAY[] = {
    {MO_FOV, "85"}, {MO_SMOOTH, "On"}, {MO_FADE, "5 s"}, {MO_DOORSTEP, "On"}, {MO_DOORPICK, "On"}, {MO_DOORCUE, "On"},
    {MO_FONTSIZE, "*(100%)"}, {MO_MENUFONT, "Smooth"}, {MO_MENUBG, "Solid colour"}, {MO_MENUFAST, "On"}, {MO_SYSPTR, "System"}, {MO_CONVFONT, "Smooth"}, {MO_LOGFONT, "Smooth"},
    {MO_CHARSHEET, "Full"},
};
static const PresetRow PR_GRAPHICS[] = {   /* (uwview.c's S, render.c's and render_vk.c's statics) */
    {MO_DLSS, "On"}, {MO_DLSSMODE, "DLAA*"}, {MO_RT, "On"}, {MO_RR, "Off"}, {MO_DLSS5, "Off"}, {MO_STYLE, "B - Natural"},
    {MO_D5INT, "100%"}, {MO_D5STR, "100%"}, {MO_BOUNCE, "On"}, {MO_SHADOWS, "On"}, {MO_TEXRES, "Original"},
    {MO_MAPNORMAL, "As the textures"}, {MO_MAPAO, "As the textures"}, {MO_MAPHEIGHT, "As the textures"}, {MO_RELIEF, "On"},
    {MO_NSTR, "1.00"}, {MO_TEXAO, "100%"}, {MO_TEXSHADOW, "Depth 3"}, {MO_LIGHTCOL, "CAT16"}, {MO_LIGHTFADE, "2 / 3 / 4 tiles"},
    {MO_BREATH, "20%"}, {MO_ADAPT, "Automatic*"}, {MO_HDR, "On*"},
};
static const PresetRow PR_G_FAST[] = { {MO_DLSSMODE, "Performance*"}, {MO_BOUNCE, "Off"}, {MO_SHADOWS, "Off"}, {MO_TEXSHADOW, "Off"} };
static const PresetRow PR_G_BALANCED[] = { {MO_DLSSMODE, "Quality*"}, {MO_TEXRES, "Upscaled 256"} };
static const PresetRow PR_G_BEST[] = { {MO_RR, "On"}, {MO_TEXRES, "Upscaled 512"}, {MO_TEXSHADOW, "Depth 8"} };
static const PresetRow PR_EFFECTS[] = {    /* (uwview.c's FX, all off, and GL) */
    {MO_PARTICLES, "Off"}, {MO_DECALS, "Off"}, {MO_WATER, "Off"}, {MO_CAMPSMOKE, "On"}, {MO_WATERMIST, "On"}, {MO_GLOWSTONES, "On"},
    {MO_MOONLIGHT, "10% of a candle"}, {MO_ELEMLIGHT, "30% of a candle"},
};
static const PresetRow PR_E_OFF[] = { {MO_CAMPSMOKE, "Off"}, {MO_WATERMIST, "Off"}, {MO_GLOWSTONES, "Off"}, {MO_MOONLIGHT, "Off"}, {MO_ELEMLIGHT, "Off"} };
static const PresetRow PR_E_ON[] = { {MO_PARTICLES, "On"}, {MO_DECALS, "On"}, {MO_WATER, "On"} };
/* (Positional sound and Acoustics last: the rows above them are shown only while those are on) */
static const PresetRow PR_SOUND[] = {
    {MO_MUSVOL, "100%", 1}, {MO_FXVOL, "100%", 1}, {MO_HRTF, "On"}, {MO_PANFIX, "Corrected"}, {MO_HEARRANGE, "Original (6 tiles)"},
    {MO_FEETSPREAD, "Original"}, {MO_STEPVOL, "80%"}, {MO_STEPECHO, "50%"}, {MO_FOOTSTEPS, "On"}, {MO_ECHOQ, "Medium (2,048 rays)"},
    {MO_ACWET, "100%"}, {MO_ACSIZE, "2.4 m a tile"}, {MO_ACOCCL, "100%"}, {MO_ACDETOUR, "On"}, {MO_FXAUDIO, "On"}, {MO_ACOUSTICS, "High"},   /* (as the game comes, settings.txt) */
};
static const PresetRow PR_S_ORIGINAL[] = { {MO_FXAUDIO, "Off"} };
static const PresetRow PR_S_POSITIONAL[] = { {MO_FXAUDIO, "On"}, {MO_ACOUSTICS, "On"} };
static const PresetRow PR_S_FULL[] = { {MO_FXAUDIO, "On"}, {MO_ACOUSTICS, "High"}, {MO_HRTF, "On"} };
static const PresetRow PR_CONTROLS[] = { {MO_MOUSE, "1.0"} };
/* (the level first: the parts' rows are shown from their level up. With nothing in the settings the game starts at
   Devious: main.c's smart-hunting on; every part on: cunning.c; 2 hunters a level: hunter.c; uwmindhost.c's tuning) */
static const PresetRow PR_AI[] = {
    {MO_AILEVEL, "Devious"}, {MO_AIFRIENDS, "On*"},
    {MO_AIP0, "On*"}, {MO_AIP1, "On*"}, {MO_AIP2, "On*"}, {MO_AIP3, "On*"}, {MO_AIP4, "On*"}, {MO_AIP5, "On*"}, {MO_AIP6, "On"},
    {MO_AIX0, "On*"}, {MO_AIX1, "On*"}, {MO_AIX2, "On*"}, {MO_AIX3, "On*"}, {MO_AIX4, "On*"}, {MO_AIX5, "On*"}, {MO_AIX6, "On*"},
    {MO_AIX7, "On*"}, {MO_AIX8, "On*"}, {MO_AIX9, "On*"}, {MO_AIX10, "On*"}, {MO_AIX11, "On*"}, {MO_AIX12, "On*"}, {MO_AIX13, "On*"},
    {MO_AIX14, "On*"}, {MO_AIX15, "On*"}, {MO_AIX16, "On*"},
    {MO_HUNTERS, "2 a level*"}, {MO_AIBY, "Rules"}, {MO_LLMMODE, "Scoring"}, {MO_LLMRATE, "20 a second"}, {MO_LLMSTALE, "3.0 s"},
    {MO_LLMIDLE, "10 min"},
};
static const PresetRow PR_AI_ORIGINAL[] = { {MO_AILEVEL, "Original"} };
static const PresetRow PR_AI_DEVIOUS[] = { {MO_AILEVEL, "Devious"} };
static const PresetRow PR_AI_MONSTROUS[] = { {MO_AILEVEL, "Monstrous"} };
#define PR(a) a, (int)(sizeof a / sizeof a[0])
static const struct { const char *tab, *name; const PresetRow *rows; int n; } TAB_PRESETS[] = {   /* a tab's first: its Default */
    {"Display", "Default", PR(PR_DISPLAY)},
    {"Graphics", "Default", PR(PR_GRAPHICS)}, {"Graphics", "Fastest", PR(PR_G_FAST)}, {"Graphics", "Balanced", PR(PR_G_BALANCED)},
    {"Graphics", "Best picture", PR(PR_G_BEST)},
    {"Effects", "Default", PR(PR_EFFECTS)}, {"Effects", "All off", PR(PR_E_OFF)}, {"Effects", "All on", PR(PR_E_ON)},
    {"Sound", "Default", PR(PR_SOUND)}, {"Sound", "Original", PR(PR_S_ORIGINAL)}, {"Sound", "Positional", PR(PR_S_POSITIONAL)},
    {"Sound", "Full", PR(PR_S_FULL)},
    {"Controls", "Default", PR(PR_CONTROLS)},
    {"AI", "Default", PR(PR_AI)}, {"AI", "Original", PR(PR_AI_ORIGINAL)}, {"AI", "Devious", PR(PR_AI_DEVIOUS)},
    {"AI", "Monstrous", PR(PR_AI_MONSTROUS)},
};
#undef PR
#define TAB_NPRESETS ((int)(sizeof TAB_PRESETS / sizeof TAB_PRESETS[0]))
/* two tabs have their presets elsewhere: Fog and smoke the renderer's (uwv_vol_preset), Sound tuning one Default,
   the dials' own (SOUND_DIALS' dflt) */
static int tab_is_vol(int t) { return t == menu_tab_of(MO_VOLPAGE); }
static int tab_is_dials(int t) { return t == menu_tab_of(MO_ROOMINFO); }
static int tab_preset_at(int t, int k)   /* tab t's k-th preset in TAB_PRESETS, -1 none */
{
    const char *nm = tab_name(t);
    for (int i = 0; i < TAB_NPRESETS; i++) if (!strcmp(TAB_PRESETS[i].tab, nm) && k-- == 0) return i;
    return -1;
}
static const char *tab_preset_name(int t, int k)   /* NULL past the last */
{
    if (tab_is_vol(t)) return vol_have() && V.vol_preset_name && V.vol_preset ? V.vol_preset_name(k) : NULL;
    if (tab_is_dials(t)) return k == 0 ? "Default" : NULL;
    k = tab_preset_at(t, k);
    return k < 0 ? NULL : TAB_PRESETS[k].name;
}
/* how many presets tab t has */
static int tab_preset_count(int t)
{
    int n = 0;
    while (n < 32 && tab_preset_name(t, n)) n++;
    return n;
}
/* tab t's k-th preset as rows: Default's, each replaced by the preset's own where it has one; the count */
static int tab_preset_rows(int t, int k, PresetRow *out, int cap)
{
    int d = tab_preset_at(t, 0), p = tab_preset_at(t, k), n = 0;
    if (d < 0 || p < 0) return 0;
    for (int i = 0; i < TAB_PRESETS[d].n && n < cap; i++) {
        PresetRow r = TAB_PRESETS[d].rows[i];
        if (p != d) {
            if (r.own) continue;
            for (int j = 0; j < TAB_PRESETS[p].n; j++) if (TAB_PRESETS[p].rows[j].id == r.id) r = TAB_PRESETS[p].rows[j];
        }
        out[n++] = r;
    }
    return n;
}
/* a row's value matches a preset's (a * at the end: its start; at the start: anywhere in it) */
static int preset_text_is(const char *have, const char *want)
{
    size_t l = strlen(want);
    if (want[0] == '*') return strstr(have, want + 1) != NULL;
    if (l && want[l - 1] == '*') return !strncmp(have, want, l - 1);
    return !strcmp(have, want);
}
static int preset_text_none(const char *have) { return !strncmp(have, "Not on this card", 16) || !strncmp(have, "Needs ", 6); }
/* the preset the open tab's rows match now (the rows as just built: val), -1 none: Custom. Never kept: worked out
   from the rows each time the menu is built */
static int tab_preset_now(int t, const char **val)
{
    int n = tab_preset_count(t), first = -1;
    if (tab_is_vol(t)) return n ? V.vol_preset(-1) : -1;
    if (tab_is_dials(t)) {
        dials_load();
        for (int i = 0; i < SOUND_NDIALS; i++) if (dial_shown(i) && dial_val[i] != SOUND_DIALS[i].dflt) return -1;
        return 0;
    }
    for (int k = 0; k < n; k++) {
        PresetRow rows[64];
        int nr = tab_preset_rows(t, k, rows, 64), same = 1;
        for (int i = 0; i < nr && same; i++)
            for (int r = 0; r < menu_n; r++)
                if (menu_ids[r] == rows[i].id && !preset_text_none(val[r]) && !preset_text_is(val[r], rows[i].want)) same = 0;
        if (same && tab_last[t] == k + 1) return k;
        if (same && first < 0) first = k;
    }
    return first;
}
/* the two rows at the top of a tab that has presets: Preset (its value is filled in when the tab's rows are built:
   menu_rows_build) and Default settings (Enter alone does it: vol_kind 4, as an action of the renderer's) */
static void tab_head_rows(int t, const char **lab, const char **val, const char **hint, char vbuf[][128])
{
    if (display_mode == 1 || menu_n > MENU_MAX - 2 || tab_preset_count(t) <= 0) return;
    vbuf[menu_n][0] = 0;
    menu_ids[menu_n] = MO_PRESET, lab[menu_n] = "Preset", val[menu_n] = vbuf[menu_n];
    hint[menu_n] = "A ready-made set of this tab's settings. Left and Right pick one, and it is applied at once. Custom: your own mix, which matches none of them.", menu_n++;
    snprintf(vbuf[menu_n], 128, "%s", "Enter");
    vol_kind[menu_n] = 4;
    menu_ids[menu_n] = MO_DEFAULTS, lab[menu_n] = "Default settings", val[menu_n] = vbuf[menu_n];
    hint[menu_n] = "Enter: every row of this tab goes back to its default. Rows that need a restart or depend on your machine are left alone.", menu_n++;
}
static int menu_opt(int id)   /* the renderer's option of a row, -1 none */
{
    for (int i = 0; i < MENU_NROWS; i++) if (MENU_ROWS[i].id == id && id != MO_HEAD) return MENU_ROWS[i].opt;
    return -1;
}
/* The Keys tab: a row for each command of our display (uwinput.c's CMD), its value the key it has. Enter (or a
   click) asks for the new key: the next key pressed is it, with Shift or Ctrl if one is held; Shift alone is taken
   when it is let go with nothing else pressed. Esc gives up, Delete leaves the command with no key. A key another
   command had is taken from it (that one then shows "none"). Left (or a right click) puts the shipped key back.
   Kept in the settings as key_<id>=<code>,<modifier>. Esc, Tab, F1, F11 and ` are the menu's, the panel's and the
   settings screen's own and cannot be given away. The original's display has its own keys: no tab there. */
static int key_ask = -1, key_ask_mod;
/* a set-1 code's name for the Keys tab */
static const char *key_name(int c)
{
    static const struct { int c; const char *n; } K[] = {
        {0x10, "Q"}, {0x11, "W"}, {0x12, "E"}, {0x13, "R"}, {0x14, "T"}, {0x15, "Y"}, {0x16, "U"}, {0x17, "I"}, {0x18, "O"}, {0x19, "P"},
        {0x1E, "A"}, {0x1F, "S"}, {0x20, "D"}, {0x21, "F"}, {0x22, "G"}, {0x23, "H"}, {0x24, "J"}, {0x25, "K"}, {0x26, "L"},
        {0x2C, "Z"}, {0x2D, "X"}, {0x2E, "C"}, {0x2F, "V"}, {0x30, "B"}, {0x31, "N"}, {0x32, "M"},
        {0x02, "1"}, {0x03, "2"}, {0x04, "3"}, {0x05, "4"}, {0x06, "5"}, {0x07, "6"}, {0x08, "7"}, {0x09, "8"}, {0x0A, "9"}, {0x0B, "0"},
        {0x0C, "-"}, {0x0D, "="}, {0x0E, "Backspace"}, {0x1A, "["}, {0x1B, "]"}, {0x1C, "Enter"}, {0x27, ";"}, {0x28, "'"},
        {0x2A, "Left Shift"}, {0x2B, "\\"}, {0x33, ","}, {0x34, "."}, {0x35, "/"}, {0x36, "Right Shift"}, {0x37, "Keypad *"},
        {0x39, "Space"}, {0x3A, "Caps Lock"}, {0x3B, "F1"}, {0x3C, "F2"}, {0x3D, "F3"}, {0x3E, "F4"}, {0x3F, "F5"}, {0x40, "F6"},
        {0x41, "F7"}, {0x42, "F8"}, {0x43, "F9"}, {0x44, "F10"}, {0x57, "F11"}, {0x58, "F12"},
        {0x47, "Keypad 7"}, {0x48, "Keypad 8"}, {0x49, "Keypad 9"}, {0x4A, "Keypad -"}, {0x4B, "Keypad 4"}, {0x4C, "Keypad 5"},
        {0x4D, "Keypad 6"}, {0x4E, "Keypad +"}, {0x4F, "Keypad 1"}, {0x50, "Keypad 2"}, {0x51, "Keypad 3"}, {0x52, "Keypad 0"},
        {0x53, "Keypad ."}, {0x11C, "Keypad Enter"}, {0x135, "Keypad /"}, {0x147, "Home"}, {0x148, "Up arrow"}, {0x149, "Page Up"},
        {0x14B, "Left arrow"}, {0x14D, "Right arrow"}, {0x14F, "End"}, {0x150, "Down arrow"}, {0x151, "Page Down"}, {0x152, "Insert"},
    };
    static char other[16];
    for (int i = 0; i < (int)(sizeof K / sizeof K[0]); i++) if (K[i].c == c) return K[i].n;
    snprintf(other, sizeof other, "Key %X", c);
    return other;
}
/* a key and its modifier as a row shows them */
static void key_text(int key, int shift, char *out, size_t n)
{
    if (!key) snprintf(out, n, "%s", "none");
    else snprintf(out, n, "%s%s", shift == 1 ? "Shift + " : shift == 2 ? "Ctrl + " : "", key_name(key));
}
static void keys_load(void)   /* once, when the settings can be read */
{
    static int done;
    char name[48], v[32];
    if (done || !plat_home()) return;
    done = 1;
    for (int i = 0; i < uwi_cmd_count(); i++) {
        int k = 0, sh = 0;
        if (uwi_cmd_label(i)[0] == '#') continue;
        snprintf(name, sizeof name, "key_%s", uwi_cmd_id(i));
        if (port_config_get(plat_home(), name, v, sizeof v) != 0 || sscanf(v, "%d,%d", &k, &sh) != 2) continue;
        if (k >= 0 && k < 0x200 && sh >= 0 && sh <= 2) uwi_cmd_set(i, k, sh, 1);
    }
}
static void keys_save(int i)   /* command i's key to the settings (-1: every command's) */
{
    char name[48], v[32];
    if (!plat_home()) return;
    for (int c = 0; c < uwi_cmd_count(); c++) {
        int k, sh;
        if ((i >= 0 && c != i) || uwi_cmd_label(c)[0] == '#') continue;
        uwi_cmd_get(c, 0, &k, &sh);
        snprintf(name, sizeof name, "key_%s", uwi_cmd_id(c)), snprintf(v, sizeof v, "%d,%d", k, sh);
        port_config_set(plat_home(), name, v);
    }
}
/* command i given a key (taken from the command that had it), kept in the settings */
static void key_bind(int i, int key, int shift)
{
    char t[48];
    int lost = uwi_cmd_set(i, key, shift, 0);
    keys_save(i);
    if (lost >= 0) keys_save(lost);
    key_text(key, shift, t, sizeof t);
    RLOG_I("front", "keys: %s = %s%s%s", uwi_cmd_label(i), t, lost >= 0 ? ", taken from " : "", lost >= 0 ? uwi_cmd_label(lost) : "");
}
/* a key while the Keys tab asks for one: 1 always (every key is the question's) */
static int key_ask_event(int c, int down)
{
    int shiftkey = c == 0x2A || c == 0x36, mod = shiftkey || c == 0x1D || c == 0x11D || c == 0x38 || c == 0x138;
    if (down == 1) {
        if (c == 0x01) key_ask = -1;
        else if (c == 0x153) key_bind(key_ask, 0, 0), key_ask = -1;                    /* Delete: no key */
        else if (mod) key_ask_mod = c;
        else if (c != 0x57 && c != 0x0F && c != 0x29 && c != 0x3B)                     /* (F11, Tab, `, F1: not to be given away) */
            key_bind(key_ask, c, menu_shift ? 1 : uwp_ctrl_down() ? 2 : 0), key_ask = -1, key_ask_mod = 0;
    } else if (!down && key_ask_mod == c) {
        if (shiftkey) key_bind(key_ask, c, 0), key_ask = -1;                           /* Shift alone, let go */
        key_ask_mod = 0;
    }
    return 1;
}
/* the Keys tab's rows: Default keys, then each command with its key */
static void key_rows_build(const char **lab, const char **val, const char **hint, char vbuf[][128])
{
    static char hints[96][224];
    keys_load();
    if (menu_n < MENU_MAX - 2) {
        snprintf(vbuf[menu_n], 128, "%s", "Enter");
        vol_kind[menu_n] = 4;
        menu_ids[menu_n] = MO_KEYDEFAULT, lab[menu_n] = "Default keys", val[menu_n] = vbuf[menu_n];
        hint[menu_n] = "Enter: every command goes back to the key it came with.", menu_n++;
    }
    for (int i = 0; i < uwi_cmd_count() && i < 96 && menu_n < MENU_MAX - 1; i++) {
        const char *l = uwi_cmd_label(i);
        int k, sh, dk, dsh;
        char d[48];
        vbuf[menu_n][0] = 0;
        if (l[0] == '#') {   /* (a heading with no row under it is taken back, as the options') */
            if (menu_n && menu_ids[menu_n - 1] == MO_HEAD) menu_n--;
            menu_ids[menu_n] = MO_HEAD, lab[menu_n] = l, val[menu_n] = vbuf[menu_n], hint[menu_n] = "", menu_n++;
            continue;
        }
        uwi_cmd_get(i, 0, &k, &sh), uwi_cmd_get(i, 1, &dk, &dsh);
        if (key_ask == i) snprintf(vbuf[menu_n], 128, "%s", "press a key ...");
        else key_text(k, sh, vbuf[menu_n], 128);
        key_text(dk, dsh, d, sizeof d);
        snprintf(hints[i], sizeof hints[i], "Enter, then the new key (hold Shift or Ctrl for a combination). Esc: give up. Delete: no key. Left: back to %s.", d);
        menu_ids[menu_n] = MO_KEY + i, lab[menu_n] = l, val[menu_n] = vbuf[menu_n], hint[menu_n] = hints[i], menu_n++;
    }
    if (menu_n && menu_ids[menu_n - 1] == MO_HEAD) menu_n--;
}
/* the rows of the open tab (menu_tab), and how many rows each tab shows now (tab_shown); menu_rows_build, below, is
   what the menu calls */
static void menu_rows_pass(const char **lab, const char **val, const char **hint, char vbuf[][128])
{
    int sec = -1;   /* the section (the tab) the row is in */
    menu_n = 0;
    memset(tab_shown, 0, sizeof tab_shown);
    for (int i = 0; i < MENU_MAX; i++) vol_kind[i] = 0, vol_frac[i] = -1;
    if (menu_page) {
        static const char *SL[4] = {"Slot I", "Slot II", "Slot III", "Slot IV"};
        for (int i = 0; i < 4; i++) {
            if (slot_used[i]) snprintf(vbuf[menu_n], 128, "%.44s  %s", slot_desc[i][0] ? slot_desc[i] : "(no name)", slot_time[i]);
            else snprintf(vbuf[menu_n], 128, "%s", "(empty)");
            menu_ids[menu_n] = MO_SLOT + i + 1, lab[menu_n] = SL[i], val[menu_n] = vbuf[menu_n];
            hint[menu_n] = menu_page == 1 ? "Save the game here." : slot_used[i] ? "Load this game." : "", menu_n++;
        }
        vbuf[menu_n][0] = 0;
        menu_ids[menu_n] = MO_BACK, lab[menu_n] = "Back", val[menu_n] = vbuf[menu_n], hint[menu_n] = "Back to the options.", menu_n++;
        return;
    }
    for (int i = 0; i < MENU_NROWS && menu_n < MENU_MAX; i++) {
        int id = MENU_ROWS[i].id;
        /* the original's display: everything of ours is off, so only the rows that still do something are shown (the
           game's own, the Display row to come back by, the window, the music and sound the original had, and this
           menu's own letters and background) */
        if (display_mode == 1 && id != MO_HEAD && id != MO_RESUME && id != MO_SAVE && id != MO_LOAD && id != MO_NEW && id != MO_QUIT
            && id != MO_DISPLAY && id != MO_FULL && id != MO_MUSIC && id != MO_MUSVOL && id != MO_SOUND && id != MO_FXVOL
            && id != MO_CARDMUSIC && id != MO_CARDSPEECH && id != MO_MENUFONT && id != MO_MENUBG && id != MO_MENUFAST) continue;
        vbuf[menu_n][0] = 0;
        if (id == MO_HEAD) {   /* a section's heading: a tab (its name is in the strip, not in the list); the open tab's own two rows first */
            if (++sec < MENU_TABS && sec == menu_tab) tab_head_rows(sec, lab, val, hint, vbuf);
            continue;
        } else if (sec < 0 || sec >= MENU_TABS) continue;
        else if (MENU_ROWS[i].opt >= 0) {
            if (!V.option || !V.option(MENU_ROWS[i].opt, 0, vbuf[menu_n], 128)) continue;
            if (id == MO_HRTF) {   /* what the sound device made of it, so the player can see 3D sound is working */
                int h = uwaudio_hrtf();
                size_t l = strlen(vbuf[menu_n]);
                const char *now = h == 1 || h == 3 || h == 4 ? " (working now)" : h == 0 ? " (not in use now)"
                                : h == 2 || h == 5 ? " (this output cannot)" : "";
                snprintf(vbuf[menu_n] + l, 128 - l, "%s", now);
            }
        } else if (id == MO_DISPLAY) {   /* (no renderer: no display of ours to choose) */
            if (!V.ok) continue;
            snprintf(vbuf[menu_n], 128, "%s", display_mode == 1 ? "The original's" : "Ours");
        }
        else if (id == MO_CARDMUSIC || id == MO_CARDSPEECH) card_text(id == MO_CARDSPEECH, vbuf[menu_n], 128);
        else if (id == MO_ROOMINFO) {   /* what the rays measure at the listener (shown while the acoustics run) */
            /* (asked of the sound four times a second, not every frame: the asking waits on the acoustics' thread,
               0.8 ms, and the menu builds its rows every frame; 2026-11-22, the menu's slow frames) */
            static char room_txt[128];
            static uint64_t room_at;
            float rt[3], mfp = 0, first = 0;
            if (!uwaudio_acoustics_on()) continue;
            char qv[16] = "";
            if (!room_txt[0] || ms_now() - room_at >= 250 || !V.option || !V.option(64, 0, qv, sizeof qv) || strcmp(qv, "On")) {   /* (every frame with Quick menu drawing Off) */
                room_at = ms_now();
                if (uwaudio_room(rt, &mfp, &first)) snprintf(room_txt, sizeof room_txt, "rings %.1f / %.1f / %.1f s, %.1f m between walls", rt[0], rt[1], rt[2], mfp);
                else snprintf(room_txt, sizeof room_txt, "%s", "(no room here)");
            }
            snprintf(vbuf[menu_n], 128, "%s", room_txt);
        }
        else if (id >= MO_DIAL && id < MO_DIAL + SOUND_NDIALS) {
            if (!dial_shown(id - MO_DIAL)) continue;
            dial_text(id - MO_DIAL, vbuf[menu_n], 128);
        }
        else if (id == MO_KEYPAGE) {   /* the Keys tab: the commands' rows */
            tab_shown[sec]++;
            if (sec == menu_tab) key_rows_build(lab, val, hint, vbuf);
            continue;
        }
        else if (id == MO_VOLPAGE) {   /* the Fog and smoke tab: the renderer's rows, when it has them */
            if (vol_have()) {
                tab_shown[sec]++;
                if (sec == menu_tab) vol_rows_build(lab, val, hint, vbuf);
            }
            continue;
        }
        else if (id == MO_SPRPAGE) {   /* the Sprites tab: our own object pictures that are there */
            if (spr_rows_build(sec == menu_tab, lab, val, hint, vbuf)) tab_shown[sec]++;
            continue;
        }
        else if (id == MO_FULL) snprintf(vbuf[menu_n], 128, "%s", live.fullscreen ? "On" : "Off");
        else if (id == MO_RENDERER) {   /* (Windows with a renderer up: the two DLLs; the one wanted, and the one running) */
#ifndef _WIN32
            continue;
#endif
            if (!V.ok) continue;
            int want = renderer_wanted();
            snprintf(vbuf[menu_n], 128, "%s%s", want ? "Vulkan" : "Direct3D 12",
                     want == view_vk ? "" : view_vk ? " (Vulkan now)" : " (Direct3D 12 now)");
        }
        else if (id == MO_MUSIC) snprintf(vbuf[menu_n], 128, "%s", uwl_music_on() ? "On" : "Off");
        else if (id == MO_SOUND) snprintf(vbuf[menu_n], 128, "%s", uwl_fx_on() ? "On" : "Off");
        else if (id == MO_MUSVOL) volumes_load(), snprintf(vbuf[menu_n], 128, "%d%%", mus_vol);
        else if (id == MO_FXVOL) volumes_load(), snprintf(vbuf[menu_n], 128, "%d%%", fx_vol);
        else if (id == MO_FADE) { int f = fade_secs(); if (f) snprintf(vbuf[menu_n], 128, "%d s", f); else snprintf(vbuf[menu_n], 128, "Never"); }
        else if (id >= MO_ENH && id < MO_ENH + ENH_COUNT) snprintf(vbuf[menu_n], 128, "%s", ENHANCED(id - MO_ENH) ? "On" : "Off");
        else if (id == MO_QFADES) { extern int port_quick_fades; snprintf(vbuf[menu_n], 128, "%s", port_quick_fades ? "On" : "Off"); }
        else if (id == MO_SEENMAP) snprintf(vbuf[menu_n], 128, "%s", port_seen_on ? "On" : "Off");
        else if (id == MO_MAPARROWS) snprintf(vbuf[menu_n], 128, "%s", port_map_arrows8 ? "Levels 1 to 8" : "Original (to 99)");
        else if (id == MO_MAPNOTES) snprintf(vbuf[menu_n], 128, "%s", port_map_notes_fixed ? "On" : "Off");
        else if (id == MO_LOGDETAIL) snprintf(vbuf[menu_n], 128, "%s", LOG_DETAIL[rlog_level()]);
        else if (id == MO_SETSCREEN) snprintf(vbuf[menu_n], 128, "%s", "Enter");
        else if (id == MO_DOORSTEP) snprintf(vbuf[menu_n], 128, "%s", door_step(-1) ? "On" : "Off");
        else if (id == MO_DOORPICK) snprintf(vbuf[menu_n], 128, "%s", door_pick(-1) ? "On" : "Off");
        else if (id == MO_DOORCUE) snprintf(vbuf[menu_n], 128, "%s", door_cue(-1) ? "On" : "Off");
        else if (id == MO_AILEVEL) { int lv = port_ai_level(-1); snprintf(vbuf[menu_n], 128, "%s", lv == 0 ? "Original" : lv == 1 ? "Devious" : "Monstrous"); }
        else if (id >= MO_AIP0 && id <= MO_AIP6) {   /* Monstrous's parts on Monstrous alone; the messages on both smarter levels */
            if (port_ai_level(-1) < cun_part_level(id - MO_AIP0)) continue;   /* (a part's row from its own level up) */
            if (id == MO_AIP6) snprintf(vbuf[menu_n], 128, "%s", port_ai_part(6, -1) ? "On" : "Off");
            else ai_part_value(vbuf[menu_n], id - MO_AIP0);
        }
        else if (id >= MO_AIX0 && id <= MO_AIXEND) {   /* the parts 8 to 24 */
            if (port_ai_level(-1) < cun_part_level(8 + id - MO_AIX0)) continue;
            ai_part_value(vbuf[menu_n], 8 + id - MO_AIX0);
        }
        else if (id == MO_AIFRIENDS) {   /* the friendly creatures' part (7): the two smarter levels */
            if (port_ai_level(-1) < 1) continue;
            ai_part_value(vbuf[menu_n], 7);
        }
        else if (id == MO_FEUD) {   /* rival creatures fight (feud.c): the two smarter levels */
            static const char *const FEUD[3] = {"Off", "Rare", "Sometimes"};
            if (port_ai_level(-1) < 1) continue;
            snprintf(vbuf[menu_n], 128, "%s", FEUD[port_feud_mode(-1) % 3]);
        }
        else if (id == MO_AILOGFILE) {
            if (port_ai_level(-1) < 1) continue;
            snprintf(vbuf[menu_n], 128, "%s", port_ai_logfile(-1) ? "On" : "Off");
        }
        else if (id == MO_AILOG) {   /* Ctrl+L's switch (hunt.c's port_ai_log; not kept) */
#ifdef UWPORT_PLAIN
            continue;
#else
            extern volatile int port_ai_log;
            if (port_ai_level(-1) < 1) continue;
            snprintf(vbuf[menu_n], 128, "%s", port_ai_log ? "On" : "Off");
#endif
        }
        else if (id >= MO_LLMGPU && id <= MO_LLMIDLE) {   /* the model's own settings: only while the model decides */
            int v;
            if (port_ai_level(-1) < 1 || port_mind_models(NULL, NULL, 0) <= 0 || !port_ai_by_model(-1)) continue;
            v = port_mind_tune(id - MO_LLMGPU, -1);
            if (id == MO_LLMGPU) snprintf(vbuf[menu_n], 128, "%s", v ? "Graphics card" : "Processor");
            else if (id == MO_LLMMODE) snprintf(vbuf[menu_n], 128, "%s", v ? "Grammar" : "Scoring");
            else if (id == MO_LLMRATE) snprintf(vbuf[menu_n], 128, "%d a second", v);
            else if (id == MO_LLMSTALE) { if (v > 0) snprintf(vbuf[menu_n], 128, "%d.%d s", v / 1000, v % 1000 / 100); else snprintf(vbuf[menu_n], 128, "Never"); }
            else { if (v > 0) snprintf(vbuf[menu_n], 128, "%d min", v); else snprintf(vbuf[menu_n], 128, "Never"); }
        }
        else if (id == MO_AIBY) {   /* (no row at Original, or without model files: the rules decide) */
            if (port_ai_level(-1) < 1 || port_mind_models(NULL, NULL, 0) <= 0) continue;
            snprintf(vbuf[menu_n], 128, "%s", port_ai_by_model(-1) ? "Language model" : "Rules");
        }
        else if (id == MO_LLM) {   /* the file in use, without .gguf, whole (the renderer fits it to the row); no row without the files */
            if (port_mind_models(NULL, NULL, 0) <= 0) continue;   /* (shown whoever decides: tools\menucheck.py expects the row) */
            const char *nm = port_mind_current();
            int l = (int)strlen(nm);
            if (l > 5 && !uwf_stricmp(nm + l - 5, ".gguf")) l -= 5;
            snprintf(vbuf[menu_n], 128, "%.*s", l, nm);
        }
        else if (id == MO_HUNTERS) {   /* llm_hunters (hunter.c): off, 1 or 2 a level; why they cannot run, if so */
            int hv = port_hunter_setting(-1), us = port_hunter_usable();
            if (port_ai_level(-1) < 1) continue;   /* (Original: no hunters, no row) */
            snprintf(vbuf[menu_n], 128, "%s%s", hv <= 0 ? "Off" : hv == 1 ? "1 a level" : "2 a level",
                     hv <= 0 ? "" : us == 0 ? " (Creature AI off)" : us == 2 ? " (no model)" : "");
        }
        tab_shown[sec]++;
        if (sec != menu_tab) continue;   /* (another tab's row: counted, not listed) */
        menu_ids[menu_n] = id, lab[menu_n] = MENU_ROWS[i].label, val[menu_n] = vbuf[menu_n], hint[menu_n] = MENU_ROWS[i].hint, menu_n++;
    }
    menu_ntabs = sec + 1 > MENU_TABS ? MENU_TABS : sec + 1;
    if (menu_n && menu_ids[menu_n - 1] == MO_HEAD) menu_n--;
}
/* the rows the menu shows now: the open tab's (the first that has any, if it has none), and the Preset row's value */
static void menu_rows_build(const char **lab, const char **val, const char **hint, char vbuf[][128])
{
    menu_rows_pass(lab, val, hint, vbuf);
    if (menu_page) return;
    if (menu_tab < 0 || menu_tab >= menu_ntabs || !tab_shown[menu_tab]) {   /* the open tab shows no row now: the first that does */
        for (int t = 0; t < menu_ntabs; t++) if (tab_shown[t]) { menu_tab = t, menu_sel = 0; break; }
        menu_rows_pass(lab, val, hint, vbuf);
    }
    if (menu_n && menu_ids[0] == MO_PRESET) {   /* the Preset row: the one the tab's rows match, or Custom */
        int now = tab_preset_now(menu_tab, val);
        snprintf(vbuf[0], 128, "%s", now < 0 ? "Custom" : tab_preset_name(menu_tab, now));
    }
}
/* the tabs: another one opened (its first row chosen), the next or the one before that shows a row, and the shown
   tabs' names given to whoever draws the menu (before the rows) */
static void menu_tab_set(int t)
{
    if (t == menu_tab || t < 0 || t >= menu_ntabs) return;
    menu_tab = t, menu_sel = 0, vol_edit_on = 0, vol_drag = -1;
}
/* the next (dir 1) or the previous (-1) tab that shows a row opened */
static void menu_tab_go(int dir)
{
    for (int k = 1; k < menu_ntabs; k++) {
        int t = ((menu_tab + dir * k) % menu_ntabs + menu_ntabs) % menu_ntabs;
        if (tab_shown[t]) { menu_tab_set(t); return; }
    }
}
/* the shown tabs' names to whoever draws the menu */
static void menu_tabs_give(void)
{
    const char *names[MENU_TABS];
    int open = 0;
    tab_nvis = 0;
    if (!menu_page)
        for (int t = 0; t < menu_ntabs; t++)
            if (tab_shown[t]) {
                if (t == menu_tab) open = tab_nvis;
                names[tab_nvis] = tab_name(t), tab_vis[tab_nvis++] = t;
            }
    if (soft_menu_up()) { if (soft_menu->tabs) soft_menu->tabs(names, tab_nvis, open); }
    else if (V.menu_tabs) V.menu_tabs(names, tab_nvis, open);
}
/* the chosen row is never a heading: moved on the way dir (1 down, -1 up) to the next row that can be chosen */
static void menu_sel_fix(int dir)
{
    if (menu_n <= 0) return;
    if (menu_sel < 0 || menu_sel >= menu_n) menu_sel = 0;
    for (int k = 0; k < menu_n && menu_ids[menu_sel] == MO_HEAD; k++) menu_sel = (menu_sel + dir + menu_n) % menu_n;
}
/* the sliders' own keys (a press or a repeat of set-1 code c): 1 taken. On a slider's row a digit, '.' (or ',') or '-'
   (the main keys and the number pad) starts a number or adds to it, Backspace takes the last one away, Enter gives
   the number to the renderer (which keeps it inside the slider's range), Esc drops it and leaves the menu open; any
   other key drops it and does what it does. With no number being typed: Left and Right do nothing on a row that
   Enter acts on (Default settings). */
static int vol_key(int c)
{
    static const struct { int code; char ch; } K[] = {
        {0x02, '1'}, {0x03, '2'}, {0x04, '3'}, {0x05, '4'}, {0x06, '5'}, {0x07, '6'}, {0x08, '7'}, {0x09, '8'}, {0x0A, '9'}, {0x0B, '0'},
        {0x0C, '-'}, {0x34, '.'}, {0x33, '.'},
        {0x47, '7'}, {0x48, '8'}, {0x49, '9'}, {0x4A, '-'}, {0x4B, '4'}, {0x4C, '5'}, {0x4D, '6'}, {0x4F, '1'}, {0x50, '2'}, {0x51, '3'},
        {0x52, '0'}, {0x53, '.'},
    };
    int kind = menu_sel >= 0 && menu_sel < menu_n ? vol_kind[menu_sel] : 0, len;
    char ch = 0;
    for (int i = 0; i < (int)(sizeof K / sizeof K[0]); i++) if (K[i].code == c) ch = K[i].ch;
    if (ch && kind == 2 && V.vol_set) {
        if (!vol_edit_on) vol_edit_on = 1, vol_edit[0] = 0;
        len = (int)strlen(vol_edit);
        if (len < 12 && (ch != '-' || len == 0) && (ch != '.' || !strchr(vol_edit, '.'))) vol_edit[len] = ch, vol_edit[len + 1] = 0;
        return 1;
    }
    if (vol_edit_on) {
        len = (int)strlen(vol_edit);
        if (c == 0x0E) {   /* Backspace; the last one gone: no number being typed any more */
            if (len > 0) vol_edit[--len] = 0;
            if (len == 0) vol_edit_on = 0;
            return 1;
        }
        if (c == 0x39) return 1;   /* (Space: nothing while a number is typed) */
        vol_edit_on = 0;
        if (c == 0x1C || c == 0x11C) {
            if (kind == 2 && V.vol_set) V.vol_set(menu_ids[menu_sel] - MO_VOL, vol_edit);
            return 1;
        }
        if (c == 0x01) return 1;
        return 0;
    }
    if ((c == 0x14B || c == 0x14D) && kind == 4) return 1;
    return 0;
}
/* the sliders' pointer (menu_event's, after the pointer's place is kept): 1 taken. A left press on a slider's track
   puts the value there and holds the slider, which then follows the pointer until the release (the scrollbar and
   the row's own click never see that press); any press drops a number being typed */
static int vol_mouse(int kind, int c, int down, int x, int y)
{
    float f = 0;
    if (kind == UWI_BUTTON && down) vol_edit_on = 0;
    if (kind == UWI_BUTTON && c == PLAT_BUTTON_LEFT && !down) { vol_drag = -1; return 0; }
    if (!V.menu_slider || !V.vol_set_frac) return 0;
    if (kind == UWI_BUTTON && c == PLAT_BUTTON_LEFT && down) {
        int r = V.menu_slider(x, y, -1, &f);
        if (r < 0 || r >= menu_n || vol_kind[r] != 2) return 0;
        vol_drag = r, menu_sel = r;
        V.vol_set_frac(menu_ids[r] - MO_VOL, f);
        return 1;
    }
    if (vol_drag >= 0 && vol_drag < menu_n && vol_kind[vol_drag] == 2) {
        if (V.menu_slider(x, y, vol_drag, &f) >= 0) V.vol_set_frac(menu_ids[vol_drag] - MO_VOL, f);
        return 1;
    }
    if (kind == UWI_BUTTON && down && c == PLAT_BUTTON_RIGHT && menu_hover >= 0 && menu_hover < menu_n && vol_kind[menu_hover] == 4) return 1;
    return 0;
}
static int panel_wait;                /* the original's options panel (save, restore, quit) is up: no mouse-look till it is gone */
static unsigned long long panel_t0;
/* a shortcut that opens the original's options panel (F5 save, F9 restore): mouse-look wanted off first (the panel
   takes every key while it is up; the want, published before the Ctrl key goes in, is applied at the game's key poll
   ahead of it), then the Ctrl key */
static void look_publish(void);
static int look_off_due;   /* switched from our display to the original's: the want stays 0 till the game's mouse-look is off */
static void panel_shortcut(int c)
{
    panel_wait = 1, panel_t0 = ms_now();
    look_publish();
    ctrl_key(c);
}
/* our menu closed: the first start's model kept, the game going on */
static void menu_close(void)
{
    menu_open = 0, menu_page = 0;
    if (pick_prompt) {   /* the first start's picker: the model shown kept, so it never asks again */
        pick_prompt = 0;
        port_mind_keep();
        printf("AI: first start: the language model kept: %s\n", port_mind_current());
        fflush(stdout);
    }
    quit_armed = 0;
    port_pause(0);
}
/* Music / Sound on the original's own panel, made to toggle at once: its Ctrl shortcut opens the page with the
   current state lit; Down lights "off", Up "on", Enter presses it (and, opened by a shortcut, closes the panel).
   The game runs while the keys go in (the pause is lifted for them), and is paused again */
static void panel_toggle(int ctrl_scan, int now_on)
{
    port_pause(0);
    ctrl_key(ctrl_scan);
    uwp_sleep_ms(120);
    game_key(now_on ? 0x150 : 0x148, 1), uwp_sleep_ms(70), game_key(now_on ? 0x150 : 0x148, 0), uwp_sleep_ms(120);
    game_key(0x1C, 1), uwp_sleep_ms(70), game_key(0x1C, 0), uwp_sleep_ms(250);
    port_pause(1);
}
static void fx_audio_start(int again);
static void fx_pan_fix_read(void);
static void fx_acoustics_tune(void);
static void tab_preset_apply(int k);
/* a row of a tab's own stepped (Fog and smoke, Sprites, Keys, Preset, Default settings): 1 when it was one */
static int menu_step_tab_row(int id, int step)
{
    char v[128];
    if (id >= MO_VOL && id < MO_VOL + 1000) {   /* a row of the Fog and smoke tab: the renderer steps it */
        if (vol_have()) V.vol_value(id - MO_VOL, step, v, sizeof v);
        return 1;
    }
    if (id == MO_SPRALL) {   /* the Sprites tab's first row: every picture on, or all off */
        if (V.option) V.option(1999, step ? step : 1, v, sizeof v);
        return 1;
    }
    if (id >= MO_SPR && id < MO_SPR + 512) {   /* a row of the Sprites tab: the other picture, at once */
        if (V.option) V.option(1000 + id - MO_SPR, step ? step : 1, v, sizeof v);
        return 1;
    }
    if (id == MO_KEYDEFAULT) {          /* the Keys tab: every command's shipped key */
        uwi_cmd_defaults(), keys_save(-1);
        RLOG_I("front", "keys: every command back to its default key");
        return 1;
    }
    if (id >= MO_KEY && id < MO_KEY + 200) {   /* a command's row: Enter or Right asks for the key, Left its shipped one */
        if (step > 0) key_ask = id - MO_KEY, key_ask_mod = 0;
        else {
            int k, sh;
            uwi_cmd_get(id - MO_KEY, 1, &k, &sh);
            key_bind(id - MO_KEY, k, sh);
        }
        return 1;
    }
    if (id == MO_DEFAULTS) { tab_preset_apply(0); return 1; }   /* the tab's rows back to their defaults */
    if (id == MO_PRESET) {   /* the next preset (or the one before) taken; from Custom, the first (or the last) */
        const char *lab[MENU_MAX], *val[MENU_MAX], *hint[MENU_MAX];
        static char vb[MENU_MAX][128];
        int n = tab_preset_count(menu_tab), now;
        if (n <= 0) return 1;
        menu_rows_build(lab, val, hint, vb);
        now = tab_preset_now(menu_tab, val);
        tab_preset_apply(now < 0 ? (step > 0 ? 0 : n - 1) : (now + (step > 0 ? 1 : n - 1)) % n);
        return 1;
    }
    return 0;
}
/* a row of the save and load pages, or one of ours with its own code (the devices, the dials, the enhancements,
   Quick fades, Log detail, the settings screen): 1 when it was one */
static int menu_step_page_row(int id, int step)
{
    if (id == MO_BACK) { menu_page = 0, menu_sel = 0; return 1; }
    if (id > MO_SLOT && id <= MO_SLOT + 4) {
        int slot = id - MO_SLOT;
        if (menu_page == 2 && !slot_used[slot - 1]) return 1;   /* nothing there to load */
        if (menu_page == 1) {
            UwgSnap sn;
            char d[48];
            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            char when[24] = "";
            if (t) strftime(when, sizeof when, "%m-%d %H:%M", t);
            snprintf(d, sizeof d, "Level %d, %s", uwg_snapshot(&sn) ? sn.level : 0, when);
            uwi_save_desc(d);
            menu_close();
            uwi_cmd(UWI_OP_SAVE, slot, 0, 0);
        } else {
            menu_close();
            uwi_cmd(UWI_OP_LOAD, slot, 0, 0);
        }
        return 1;
    }
    if (id == MO_ROOMINFO) return 1;   /* (read-only) */
    if (id == MO_CARDMUSIC || id == MO_CARDSPEECH) { card_step(id == MO_CARDSPEECH, step); return 1; }
    if (id >= MO_DIAL && id < MO_DIAL + SOUND_NDIALS) { dial_step(id - MO_DIAL, step); return 1; }
    if (id >= MO_ENH && id < MO_ENH + ENH_COUNT) {   /* an enhancement: at once, and kept in uw1port.cfg */
        uint32_t keep = 0, bit = 1u << (id - MO_ENH);
        enhance_on ^= bit;
        if (plat_home()) {
            enhance_load(plat_home(), &keep);   /* (the file's own line, the Display switch's flags as they were) */
            enhance_save(plat_home(), (keep & ~bit) | (enhance_on & bit));
        }
        return 1;
    }
    if (id == MO_MAPARROWS) {   /* the map's level arrows stop at 8 (uw1port.cfg map_arrows8, 1 unless 0) */
        port_map_arrows8 = !port_map_arrows8;
        if (plat_home()) port_config_set(plat_home(), "map_arrows8", port_map_arrows8 ? "1" : "0");
        return 1;
    }
    if (id == MO_MAPNOTES) {   /* the map's notes kept right (uw1port.cfg map_notes_fixed, 1 unless 0) */
        port_map_notes_fixed = !port_map_notes_fixed;
        if (plat_home()) port_config_set(plat_home(), "map_notes_fixed", port_map_notes_fixed ? "1" : "0");
        return 1;
    }
    if (id == MO_SEENMAP) {   /* our map of what the player saw (uw1port.cfg map_seen, 1 unless 0) */
        port_seen_on = !port_seen_on;
        if (plat_home()) port_config_set(plat_home(), "map_seen", port_seen_on ? "1" : "0");
        RLOG_I("front", "map what I saw: %s", port_seen_on ? "on" : "off");
        return 1;
    }
    if (id == MO_QFADES) {   /* the map's and screens' fades instant (uw1port.cfg quick_fades, 1 unless 0) */
        extern int port_quick_fades;
        port_quick_fades = !port_quick_fades;
        if (plat_home()) port_config_set(plat_home(), "quick_fades", port_quick_fades ? "1" : "0");
        return 1;
    }
    if (id == MO_LOGDETAIL) {   /* how much goes into uwr.log: in the exe and in the renderer DLL; kept in uw1port.cfg (log_detail) */
        char b[4];
        int lv = (rlog_level() + (step > 0 ? 1 : 3)) % 4;
        rlog_set_level(lv);
        log_detail_to_view(lv);
        snprintf(b, sizeof b, "%d", lv);
        if (plat_home()) port_config_set(plat_home(), "log_detail", b);
        RLOG_I("front", "log detail: %s", LOG_DETAIL[lv]);
        return 1;
    }
    if (id == MO_SETSCREEN) {   /* the port's own settings screen, as F11 opens it */
        menu_close();
        settings_show(1);
        sync_open();
        return 1;
    }
    return 0;
}
/* a row the renderer keeps (uwv_option) stepped, and what follows from it here: 1 when it was one */
static int menu_step_renderer_row(int id, int step)
{
    char v[128];
    if (menu_opt(id) >= 0) {
        if (V.option) V.option(menu_opt(id), step, v, sizeof v);
        if (id == MO_MOUSE || id == MO_RINVERT) display_apply();   /* the new speed (or the inverting) to the port's mouse-look */
        /* the setting (the renderer keeps it) read again: the audio started again for these three, the effects' sides
           taken at once */
        if (id == MO_FXAUDIO || id == MO_ACOUSTICS || id == MO_HRTF) fx_restart_soon();
        if (id == MO_PANFIX) fx_pan_fix_read();
        if (id == MO_ACWET || id == MO_ACSIZE || id == MO_ACOCCL || id == MO_ACDETOUR || id == MO_FOOTSTEPS || id == MO_HEARRANGE || id == MO_FEETSPREAD || id == MO_ECHOQ || id == MO_STEPVOL || id == MO_STEPECHO) fx_acoustics_tune();   /* heard at once, no restart */
        return 1;
    }
    return 0;
}
/* one of our own rows stepped: the display, the window, the sound, the AI, the message fade, Quit */
static void menu_step_own_row(int id, int step)
{
    switch (id) {
    case MO_RESUME: menu_close(); break;
    case MO_DISPLAY:                        /* the other display, kept */
        display_mode = !display_mode;
        if (display_mode == 1) look_off_due = 1;   /* the want's mouse-look given back off before "no wish" (look_publish) */
        display_apply();
        if (plat_home()) port_config_set(plat_home(), "display", display_mode ? "original" : "ours");
        if (!display_mode) iface = 0;
        RLOG_I("front", "display: %s", display_mode ? "the original's (its own 3D picture, sound, keys and rules; "
               "everything of ours off)" : "ours (everything of ours back as it was set)");
#ifndef UWPORT_PLAIN
        fx_audio_start(1);                  /* the sound follows: the game's own, or ours started again */
        menu_sel = 0;                       /* (the rows changed: the choice found again below) */
        {
            const char *lab[MENU_MAX], *val[MENU_MAX], *hint[MENU_MAX];
            static char vb[MENU_MAX][128];
            menu_rows_build(lab, val, hint, vb);
            for (int i = 0; i < menu_n; i++) if (menu_ids[i] == MO_DISPLAY) menu_sel = i;
        }
#endif
        break;
    case MO_FULL:
        plat_set_display(!live.fullscreen, live.scale, live.aspect, live.integer_scale);
        if (plat_home()) port_config_set(plat_home(), "fullscreen", live.fullscreen ? "1" : "0");
        break;
    case MO_RENDERER:                       /* the other renderer, from the next start (UWPORT_RENDERER still wins) */
        if (plat_home()) port_config_set(plat_home(), "renderer", renderer_wanted() ? "d3d12" : "vulkan");
        break;
    case MO_SAVE: menu_page = 1, menu_sel = 0, slots_read(); break;   /* our save page (the original's panel no more) */
    case MO_LOAD: menu_page = 2, menu_sel = 0, slots_read(); break;   /* our load page */
    case MO_NEW: {   /* back to the start (the title, the introduction, the menu: a new character): the program started
                        again with --menu (no autoload), as it was started, and this one closed */
        uwp_new_game();
        break;
    }
    case MO_LLM: {   /* the next (or the previous) model file found under llm\models */
        const char *nm[32], *pa[32];
        int n = port_mind_models(nm, pa, 32), i, at = -1;
        const char *cur = port_mind_current_path();
        if (n <= 0) break;
        if (n > 32) n = 32;
        for (i = 0; i < n; i++) if (!uwf_stricmp(pa[i], cur)) at = i;
        at = at < 0 ? (step > 0 ? 0 : n - 1) : (at + (step > 0 ? 1 : n - 1)) % n;
        port_mind_pick(pa[at]);
        break;
    }
    case MO_MUSIC: panel_toggle(0x32, uwl_music_on()); break;   /* Ctrl+M: its music page, the other state pressed */
    case MO_SOUND: panel_toggle(0x21, uwl_fx_on()); break;      /* Ctrl+F: its sound page */
    case MO_MUSVOL: volume_step(&mus_vol, step, "music_volume"); break;
    case MO_FXVOL: volume_step(&fx_vol, step, "sound_volume"); break;
    case MO_DOORSTEP: door_step(!door_step(-1)); break;
    case MO_DOORPICK: door_pick(!door_pick(-1)); break;
    case MO_DOORCUE: door_cue(!door_cue(-1)); break;
    case MO_AILEVEL: port_ai_level((port_ai_level(-1) + (step > 0 ? 1 : 2)) % 3); break;   /* Original -> Devious -> Monstrous */
    case MO_AIP0: case MO_AIP1: case MO_AIP2: case MO_AIP3: case MO_AIP4: case MO_AIP5: case MO_AIP6:
        port_ai_part(id - MO_AIP0, !port_ai_part(id - MO_AIP0, -1));
        break;
    case MO_AIBY: port_ai_by_model(!port_ai_by_model(-1)); break;
    case MO_AIFRIENDS: port_ai_part(7, !port_ai_part(7, -1)); break;
    case MO_AIX0: case MO_AIX1: case MO_AIX2: case MO_AIX3: case MO_AIX4: case MO_AIX5: case MO_AIX6: case MO_AIX7:
    case MO_AIX8: case MO_AIX9: case MO_AIX10: case MO_AIX11: case MO_AIX12: case MO_AIX13: case MO_AIX14: case MO_AIX15:
    case MO_AIX16:
        port_ai_part(8 + id - MO_AIX0, !port_ai_part(8 + id - MO_AIX0, -1));
        break;
    case MO_AILOGFILE: port_ai_logfile(!port_ai_logfile(-1)); break;
    case MO_FEUD: port_feud_mode((port_feud_mode(-1) + (step > 0 ? 1 : 2)) % 3); break;   /* Off -> Rare -> Sometimes */
    case MO_AILOG: our_action(UWI_A_AI_LOG, 1); break;   /* (Ctrl+L's own switch: the note in the message box too) */
    case MO_LLMGPU: port_mind_tune(0, !port_mind_tune(0, -1)); break;
    case MO_LLMMODE: port_mind_tune(1, !port_mind_tune(1, -1)); break;
    case MO_LLMRATE: port_mind_tune(2, list_step(LLM_RATES, 8, port_mind_tune(2, -1), step)); break;
    case MO_LLMSTALE: port_mind_tune(3, list_step(LLM_STALES, 7, port_mind_tune(3, -1), step)); break;
    case MO_LLMIDLE: port_mind_tune(4, list_step(LLM_IDLES, 8, port_mind_tune(4, -1), step)); break;
    case MO_HUNTERS:   /* Off -> 1 -> 2 a level (left: back); taken at once and kept (hunter.c) */
        port_hunter_setting((port_hunter_setting(-1) + (step > 0 ? 1 : 2)) % 3);
        break;
    case MO_FADE: {
        int n = (int)(sizeof FADES / sizeof FADES[0]);
        char b[16];
        fade_secs();
        fade_i = (fade_i + (step > 0 ? 1 : n - 1)) % n;
        snprintf(b, sizeof b, "%d", FADES[fade_i]);
        if (plat_home()) port_config_set(plat_home(), "message_fade", b);
        log_touch = ms_now();   /* the box shown, to see the new time */
        break;
    }
    case MO_QUIT:   /* asked here (the original's own prompt cannot be seen over our display), then the window's close:
                       the port's quit request, as the close button */
        uwp_close();   /* one click, no question */
        break;
    }
}
/* a row of the menu stepped (Right or Enter: 1, Left: -1): each kind of row to its handler, in this order */
static void menu_choose_step(int row, int step)
{
    if (row < 0 || row >= menu_n) return;
    int id = menu_ids[row];
    if (id == MO_HEAD) return;
    if (menu_step_tab_row(id, step)) return;
    if (menu_step_page_row(id, step)) return;
    if (menu_step_renderer_row(id, step)) return;
    menu_step_own_row(id, step);
}
static void menu_choose(int row) { menu_choose_step(row, 1); }
/* the open tab's k-th preset taken (0: its defaults). The renderer's for Fog and smoke; the dials' own numbers for
   Sound tuning; else each listed row is stepped, as Right and Left step it, until it shows the value wanted: up
   first, and back down when it comes to its end or round to where it began. Twice over the list, as a row may only
   be shown once another is set (DLSS mode with DLSS, the sound rows with Positional sound). */
static void tab_preset_apply(int k)
{
    const char *lab[MENU_MAX], *val[MENU_MAX], *hint[MENU_MAX];
    static char vb[MENU_MAX][128];
    PresetRow rows[64];
    int t = menu_tab, nr;
    if (k < 0 || k >= tab_preset_count(t)) return;
    tab_last[t] = k + 1;
    if (tab_is_vol(t)) { V.vol_preset(k); return; }
    if (tab_is_dials(t)) {
        int again = 0, any = 0;
        dials_load();
        for (int i = 0; i < SOUND_NDIALS; i++) {
            if (!dial_shown(i) || dial_val[i] == SOUND_DIALS[i].dflt) continue;
            dial_val[i] = SOUND_DIALS[i].dflt, any = 1;
            fx_setting_save(SOUND_DIALS[i].key, dial_val[i]);
            if (i == 34 || i == 35 || i == 38 || i == 47 || i == 48) again = 1;   /* (the Roland's own reverb, the echoes' ray tracer, the limiter: started again, as dial_step) */
        }
        if (again) fx_restart_soon();
        else if (any) fx_acoustics_tune();
        return;
    }
    nr = tab_preset_rows(t, k, rows, 64);
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < nr; i++)
            for (int dir = 1, done = 0; dir >= -1 && !done; dir -= 2) {
                char first[128] = "", prev[128] = "";
                for (int n = 0; n < 420; n++) {
                    int r = -1;
                    menu_rows_build(lab, val, hint, vb);
                    for (int q = 0; q < menu_n; q++) if (menu_ids[q] == rows[i].id) r = q;
                    if (r < 0 || preset_text_none(val[r]) || preset_text_is(val[r], rows[i].want)) { done = 1; break; }
                    if (n && (!strcmp(val[r], prev) || !strcmp(val[r], first))) break;   /* at its end, or round once: the other way */
                    if (!n) snprintf(first, sizeof first, "%s", val[r]);
                    snprintf(prev, sizeof prev, "%s", val[r]);
                    menu_choose_step(r, dir);
                }
            }
}
/* our menu's events (uwinput's MENU owner). Esc opens it while the game's 3D view is live (uwinput gives it Esc's
   press then: esc_menu; elsewhere, on the map, in a conversation, Esc is the game's); while it is open it has the keys
   (their repeats too: down 2): up and down choose, Enter acts, Esc closes; and the pointer: a left click acts, a right
   click steps back */
static int menu_event(int kind, int c, int down, int x, int y)
{
    if (kind == UWI_KEY) {
        if (c == 0x2A || c == 0x36) menu_shift = down != 0;   /* (Shift held: Shift+Tab, the tab before) */
        if (!menu_open) {
            key_ask = -1;
            if (c == 0x01 && down == 1 && ((V.ok && V.menu) || soft_live())) menu_open = 1, menu_sel = 0, port_pause(1);
            return 1;
        }
        if (key_ask >= 0 && !menu_page) return key_ask_event(c, down);   /* the Keys tab asks: this key is the answer */
        if (!down) return 1;
        if (!menu_page && vol_key(c)) return 1;   /* a slider's row: a number typed in; a row that only Enter acts on */
        if (!menu_page && (c == 0x0F || c == 0x149 || c == 0x151)) {   /* Tab, PgDn: the next tab; Shift+Tab, PgUp: the one before */
            quit_armed = 0, menu_tab_go(c == 0x149 || (c == 0x0F && menu_shift) ? -1 : 1);
            return 1;
        }
        if (c == 0x01) { if (menu_page) menu_page = 0, menu_sel = 0; else menu_close(); }   /* Esc: a page back to the options */
        else if (c == 0x148) quit_armed = 0, menu_sel = (menu_sel + menu_n - 1) % (menu_n ? menu_n : 1), menu_sel_fix(-1);   /* up (never onto a heading) */
        else if (c == 0x150) quit_armed = 0, menu_sel = (menu_sel + 1) % (menu_n ? menu_n : 1), menu_sel_fix(1);            /* down */
        else if (c == 0x14B) menu_choose_step(menu_sel, -1);                                /* left: less */
        else if (c == 0x14D) menu_choose_step(menu_sel, 1);                                 /* right: more */
        else if (c == 0x1C || c == 0x11C || c == 0x39) {                                    /* Enter, Space */
            if (pick_prompt && menu_sel < menu_n && menu_ids[menu_sel] == MO_LLM) menu_close();   /* the first start: keeps this one */
            else menu_choose(menu_sel);
        }
        return 1;
    }
    if (!menu_open) return 1;           /* (a press's release after the menu closed: nothing) */
    menu_mx = x, menu_my = y;
    if (key_ask >= 0 && kind == UWI_BUTTON && down) { key_ask = -1; return 1; }   /* a click while a key is asked: given up */
    if (!menu_page && kind == UWI_BUTTON && down && c == PLAT_BUTTON_LEFT) {   /* a left press on a tab's name opens it */
        int t = soft_menu_up() ? (soft_menu->tab_at ? soft_menu->tab_at(x, y) : -1) : V.menu_tab_at ? V.menu_tab_at(x, y) : -1;
        if (t >= 0 && t < tab_nvis) { quit_armed = 0, vol_edit_on = 0, menu_tab_set(tab_vis[t]); return 1; }
    }
    if (!menu_page && vol_mouse(kind, c, down, x, y)) return 1;   /* the sliders (the Fog and smoke tab's) */
    /* the scrollbar's (the renderer's: a press on its track or thumb scrolls and drags, the release ends the drag) */
    if (kind == UWI_BUTTON && c == PLAT_BUTTON_LEFT && menu_click_pass(x, y, down ? 1 : 0)) return 1;
    if (kind == UWI_BUTTON && down && c == PLAT_BUTTON_LEFT && menu_hover >= 0) menu_sel = menu_hover, menu_choose(menu_hover);
    else if (kind == UWI_BUTTON && down && c == PLAT_BUTTON_RIGHT && menu_hover >= 0) menu_choose_step(menu_hover, -1);
    return 1;
}
/* Our taps for the game (C's F5, a click's q or e, the left button's n, o and u): the make now, the break once the game
   has taken the key. The game's own key handlers never act on key()'s result: INPUT.C's input_dispatch reads
   MOUSE.C's do_keyboard_input(1), which throws key()'s result away for do_keyarray_input's, a scan of key_on (the keys
   held) at most every 30 ticks of the game's clock (117 ms) since the last key it gave, round from the last one. A make
   and its break sent together are both read by key() between two scans, so such a tap counted only when no key had
   been given for 30 ticks: while W walks (the walk's X given every 30 ticks), or PageDown or a slide is held, C, a
   click's look or use and the hold's take were lost. A keyboard's press lasts as
   long as the finger holds it; a tap here lasts until MOUSE.C counts the key given (port_key_taken, by scan code) or
   TAP_HOLD_MS, before the keyboard's typematic would repeat it (keyqueue.c: 500 ms); the scan that could give it again
   is 30 ticks after it was given, and the break goes at the window's next step (tap_pump). Replays: key() and the
   scans are the recording's, nothing here is read. */
#define TAP_HOLD_MS 400
#ifndef UWPORT_PLAIN
extern volatile unsigned port_key_taken[0x80];   /* ui/MOUSE.C (uw-new): the keys the game's key reader gave */
#endif
static struct { uint8_t make; unsigned seen; uint64_t t0; } taps[8];
static int ntaps;
/* tap i's break sent (its key let go) */
static void tap_end(int i)
{
    key_byte((uint8_t)(taps[i].make | 0x80));
    if (getenv("UWPORT_GRABDBG"))
        RLOG_I("front", "tap %02X held %lu ms", taps[i].make, (unsigned long)(ms_now() - taps[i].t0));
    taps[i] = taps[--ntaps];
}
/* each step: the taps the game has taken (or held long enough) let go */
static void tap_pump(void)
{
    for (int i = ntaps - 1; i >= 0; i--) {
#ifndef UWPORT_PLAIN
        if (port_key_taken[taps[i].make & 0x7F] != taps[i].seen || ms_now() - taps[i].t0 >= TAP_HOLD_MS) tap_end(i);
#else
        tap_end(i);
#endif
    }
}
/* a tap of a key for the game: the make now, the break once the game has taken it (tap_pump) */
static void send_key(uint8_t make)
{
    if (!hooks || !hooks->key) return;
#ifndef UWPORT_PLAIN
    /* a click's key with Ctrl held (Ctrl + a left click would open the original's quit panel: the
       look's q read as Ctrl+Q, PLAYER.C's do_option_shortcut). Our display has no way to the original's interface but
       the Display row, so Ctrl is let go to the game first, both of them; the next press of it is seen as ever */
    if (display_mode != 1 && uwp_ctrl_down()) key_byte(0x9D), key_byte(0xE0), key_byte(0x9D);
#endif
    for (int i = ntaps - 1; i >= 0; i--) if (taps[i].make == make) tap_end(i);   /* (the same key again: the last let go first) */
    if (ntaps == (int)(sizeof taps / sizeof taps[0])) tap_end(0);
#ifndef UWPORT_PLAIN
    taps[ntaps].seen = port_key_taken[make & 0x7F];
#endif
    taps[ntaps].make = make, taps[ntaps].t0 = ms_now(), ntaps++;
    key_byte(make);
#ifdef UWPORT_PLAIN
    tap_pump();   /* (the plain build: no count; as before, the break at once) */
#endif
}
/* the first start with more than one language model (uwmindhost.c's port_mind_first_start): at the first frame of play
   our menu opens once on its Language model row; Left / Right choose, Enter or Esc keeps the one shown (llm_model
   written: never asked again). Not in a replay, not off-screen (the checks; UWPORT_PICKTEST=1 lets one ask) */
static void first_pick(void)
{
    static int want = -1;
    if (!want) return;
    if (want < 0) {
        extern int16_t rp_request;
        const char *t = getenv("UWPORT_PICKTEST");
        want = 0;
#ifndef UWPORT_PLAIN
        want = ENHANCED(ENH_SMART_HUNTING) && rp_request != 2 && port_ai_by_model(-1) && port_mind_first_start();
#endif
        if (want && offscreen() && !(t && *t == '1')) {
            printf("AI: first start: %d language models, the picker not opened (an off-screen run)\n", port_mind_models(NULL, NULL, 0));
            fflush(stdout);
            want = 0;
        }
        if (!want) return;
    }
    static uint64_t play_t;
    if (!V.ok || !V.menu || !V.menu_rows || !full_live || display_mode != 0 || menu_open || settings_open() || screen_now() != PS_GAME) {
        play_t = 0;
        return;
    }
    if (!play_t) play_t = ms_now();
    if (ms_now() - play_t < 3000) return;   /* after 3 s of play: the world seen first, and the keys that started it gone */
    want = 0;
    menu_open_page(0);
    pick_prompt = 1;
    {
        const char *lab[MENU_MAX], *val[MENU_MAX], *hint[MENU_MAX];
        static char vb[MENU_MAX][128];
        menu_tab = menu_tab_of(MO_LLM);   /* (the row's tab opened) */
        menu_rows_build(lab, val, hint, vb);
        for (int i = 0; i < menu_n; i++) if (menu_ids[i] == MO_LLM) menu_sel = i;
    }
    printf("AI: first start: pick the language model (Left / Right choose, Enter keeps this one): %s\n", port_mind_current());
    fflush(stdout);
}
/* the first frame: the display kept (ours when none), the volumes, the language model brought up */
static void display_init(void)
{
    if (display_mode >= 0) return;
    char v[16];
    keys_load();                        /* the player's own keys (the Esc menu's Keys tab) */
    display_mode = plat_home() && port_config_get(plat_home(), "display", v, sizeof v) == 0 && !strcmp(v, "original") ? 1 : 0;
    log_detail_load();                  /* the saved Log detail (also without a renderer) */
#ifndef UWPORT_PLAIN
    {   /* quick_fades (1 unless set to 0): UWEDIT.C strt_demscr's fades instant (the map, a
           conversation and other screens back to the 3D view at once); 0 keeps the original's half-second fades */
        extern int port_quick_fades;
        port_quick_fades = !(plat_home() && port_config_get(plat_home(), "quick_fades", v, sizeof v) == 0 && atoi(v) == 0);
        port_seen_on = !(plat_home() && port_config_get(plat_home(), "map_seen", v, sizeof v) == 0 && atoi(v) == 0);
        port_map_arrows8 = !(plat_home() && port_config_get(plat_home(), "map_arrows8", v, sizeof v) == 0 && atoi(v) == 0);
        port_map_notes_fixed = !(plat_home() && port_config_get(plat_home(), "map_notes_fixed", v, sizeof v) == 0 && atoi(v) == 0);
    }
#endif
    display_apply();
    volumes_load();                     /* the saved Music volume and Sound volume */
    {   /* the language model brought up now, in the background, whatever the enhancements; llm=0 in the settings
           still keeps it off (uwmindhost.c) */
        int port_mind_ready(void);
        if (port_ai_by_model(-1))       /* (only when the model decides; the rules need none) */
            port_mind_ready();
    }
}

/* uw.exe's keys (data\keys.txt's defaults) in our display while playing: uwinput.c's key table turns them into the
   port's (modern-keys, rune-keys): W / Up walk, with Shift run, S / Down back, A / D slide, the grey arrows turn, PageUp /
   PageDown look up / down, Space jumps, E uses, L looks, Q casts the shelf, Z sleeps, K tracks, M the map; and these,
   ours (uwinput's our_action, acting on the press only): Ctrl+L the AI log, Shift+L full bright, F5 / F9 our save and
   load pages, P a screenshot, Shift+P a video, . and Shift+O the objects' shadows, C fight mode (a left click then
   attacks), I our panel, R the runes and , the character (the interface first), ' DLSS 5, ; ray tracing, ] DLSS 5's
   style, F6 the normal maps; Tab our panel (the original's interface in its display) */
static void our_action(int a, int down)
{
    if (down != 1) return;
    switch (a) {
    case UWI_A_AI_LOG: {                /* Ctrl+L: the AI log on / off */
#ifndef UWPORT_PLAIN                    /* (the plain build has no smart-hunting) */
        extern volatile int port_ai_log;
        void port_scroll_note(const char *s);
        port_ai_log = !port_ai_log;
        printf("AI: log %s%s\n", port_ai_log ? "on" : "off",
               port_ai_log && !ENHANCED(ENH_SMART_HUNTING) ? " (smart-hunting is off: no decisions to show)" : "");
        fflush(stdout);
        port_scroll_note(port_ai_log ? "AI: log on\n" : "AI: log off\n");
#endif
        break;
    }
    case UWI_A_FULLBRIGHT: if (V.toggle) V.toggle(4); break;
    case UWI_A_SAVE_PAGE:               /* F5 (and Ctrl+S in ours): our save page; the original's panel (Ctrl+S) only in its display */
        if (((V.ok && V.menu_rows && full_live) || soft_live()) && !menu_open) menu_open_page(1);
        else if (!menu_open && display_mode != 0) panel_shortcut(0x1F);   /* (ours: nothing until our menu can show) */
        break;
    case UWI_A_LOAD_PAGE:               /* F9 (and Ctrl+R in ours): our load page; Ctrl+R's panel only in the original's display */
        if (((V.ok && V.menu_rows && full_live) || soft_live()) && !menu_open) menu_open_page(2);
        else if (!menu_open && display_mode != 0) panel_shortcut(0x13);
        break;
    case UWI_A_MENU:                    /* F1, Ctrl+M/F/D/Q in our display: our menu (never the original's options panel) */
        if (((V.ok && V.menu_rows && full_live) || soft_live()) && !menu_open) menu_open_page(0);
        break;
    case UWI_A_SHOT: if (V.clip_shot) V.clip_shot(); break;
    case UWI_A_RECORD:                  /* Shift+P (no video: the screenshot, as before) */
        if (V.record) V.record(15);
        else if (V.clip_shot) V.clip_shot();
        break;
    case UWI_A_SHADOWS: if (V.toggle) V.toggle(5); break;
    case UWI_A_PANEL: inv_toggle(); break;
    case UWI_A_DLSS5: if (V.toggle) V.toggle(0); break;
    case UWI_A_RT: if (V.toggle) V.toggle(1); break;
    case UWI_A_STYLE: if (V.toggle) V.toggle(2); break;
    case UWI_A_NORMALS: if (V.toggle) V.toggle(3); break;
    /* R and ,: our panel (the Esc menu's Display row is the only way to the original's interface). R our rune page, , the character sheet;
       pressed again on that page, the panel closes */
    case UWI_A_RUNES:
        if (inv_open && inv_runes_open) inv_toggle();
        else { if (!inv_open) inv_toggle(); inv_runes_open = inv_open; }
        break;
    case UWI_A_CHARACTER:
        if (inv_open && !inv_runes_open) inv_toggle();
        else { if (!inv_open) inv_toggle(); inv_runes_open = 0; }
        break;
    case UWI_A_FIGHT:   /* C: fight mode, the original's: its F5 (the fight icon, INTERACT.C's deal_with_icons(1): the
                           weapon drawn or sheathed; F5 itself is our save); fight_mode follows the game's player->drawn */
        send_key(0x3F), RLOG_I("front", "fight mode %s asked", fight_mode ? "off" : "on");
        break;
    case UWI_A_TAB:     /* Tab: our panel in our display; the original's interface (full-screen play and back) in its */
        if (display_mode == 0) inv_toggle();
        else interface_toggle();
        break;
    }
}

/* the original's interface asked for or given up (Tab in its display, R and , in ours): mouse-look follows (the want
   is off while it is up: input_state's display_ours) */
static void interface_toggle(void)
{
    iface = !iface;
}
/* The crosshair and the readout, as the old version had them (its game.c): CURSORS.GR's picture 0, 10 (the blue
   pointer) on something to look at (out of fight mode), 9 (the red ring) on a hostile creature in reach, none while a
   blow charges; the name at the bottom right, with what a key or a click does to it under it. What it is comes from
   the game's own data (uwlook.c: pick_3d at the view's centre, its attitude, InPickRange, releaseable). An object put
   on the cursor by a take with no empty hole in the pack opens our panel to put it somewhere. */
static void readout_show(const UwlInfo *li, int on)
{
    static int was_held;
    char hint[64] = "";
    if (on) fight_mode = li->drawn;   /* the original's fight mode: its weapon drawn */
    const char *lk = fight_mode ? "L - LOOK" : "CLICK - LOOK";
    int foe = li->kind == 1 && li->hostile && li->reach, cur;
    if (li->kind == 1) {
        if (foe) snprintf(hint, sizeof hint, "%s", fight_mode ? "CLICK - ATTACK (HOLD TO CHARGE)" : "C - FIGHT   CLICK - LOOK");
        else snprintf(hint, sizeof hint, "%s%s", li->reach ? "E - TALK   " : "", lk);
    } else if (li->kind == 2)
        snprintf(hint, sizeof hint, "%s%s", !li->reach ? "" : li->open ? "E - CLOSE   " : "E - OPEN   ", lk);
    else if (li->kind == 3) {
        if (li->reach && li->take && !fight_mode) snprintf(hint, sizeof hint, "%s", "RIGHT CLICK - TAKE   DRAG - CARRY   CLICK - LOOK");
        else snprintf(hint, sizeof hint, "%s%s", !li->reach ? "" : li->sw ? "E - USE   " : li->take ? "E - TAKE   " : "", lk);
    }
    if (on && li->held && !inv_open && !fight_mode) snprintf(hint, sizeof hint, "%s", carry.on ? "LET GO - THROW" : "CLICK - THROW");
    if (fight_mode && !hint[0]) snprintf(hint, sizeof hint, "%s", "CLICK - ATTACK (FLICK UP BASH, DOWN STAB)   C - PUT AWAY");   /* fight mode, nothing in front */
    if (V.fight) V.fight(fight_mode);
    cur = li->charging ? -1 : !fight_mode && li->kind ? 10 : foe ? 9 : 0;
    V.readout2(li->name, hint, cur);
    if (getenv("UWPORT_GRABDBG")) {   /* (a check's) what the crosshair is on, when that changes */
        static char was[96];
        static int was_h = -2;
        if (strcmp(was, li->name) || was_h != li->held_item)
            RLOG_I("front", "crosshair on \"%s\" kind %d take %d reach %d stack %d, held %d (item %d) carry %d at %lu ms", li->name,
                   li->kind, li->take, li->reach, li->stack, li->held, li->held_item, li->carry, (unsigned long)(ms_now() - L.start));
        snprintf(was, sizeof was, "%s", li->name), was_h = li->held_item;
    }
    if (V.held) V.held(on && li->held && !inv_open ? li->held_item : -1);   /* the held object on the crosshair */
    /* (not while the left button holds a thing, nor just after it let go: the throw or the packing is on its way) */
    if (carry.on || ms_now() - carry.t_end < 600) return;
    if (on && li->held && !was_held && !inv_open) inv_toggle();
    was_held = on ? li->held : 0;
}

/* a key for the game a little later (the pointer moved first, so that the game's own pointer is there when it reads the
   key: the port's q and e act at the game's pointer) */
static struct { int pending, make; uint64_t due; } key_later;
/* a tap a little later (one already waiting goes first) */
static void send_key_later(int make, int ms)
{
    if (key_later.pending) send_key((uint8_t)key_later.make);
    key_later.pending = 1, key_later.make = make, key_later.due = ms_now() + (uint64_t)ms;
}
/* the late tap sent once its time has come */
static void key_later_pump(void)
{
    if (key_later.pending && ms_now() >= key_later.due) key_later.pending = 0, send_key((uint8_t)key_later.make);
}

/* Full-screen play: the buttons at the crosshair (the view's centre, where mouse-look keeps the original's pointer) as
   the old version had them, through the original's own routines: left looks (the port's q: port_key_interact(0),
   INTERACT.C's player_3dlook, or look_nothing on a wall), right uses (the port's e: talk, take, open or close a door,
   flip a switch); in fight mode left attacks (the port's Space: held charges, let go strikes). With an object on the
   cursor either button puts it down in the world: the original's right click in its 3D view while carrying
   (mous_in_3d's DoSpecialActions(0x17)). The original's own left button in its 3D view walks (PLAYMOVE.C), so it gets
   no left button here. 1: taken. */
static unsigned world_held;           /* the buttons pressed here (their release is ours too) */
static int world_space;               /* the attack's key that is down (0 none): the port's Space, . ; or p */
/* The attack's kind in mouse-look, by the mouse's flick. The original takes the kind from where the pointer
   is in its 3D view (INTERACT.C player_3dattack: the top third bashes, the middle slashes, the bottom stabs), and
   mouse-look keeps the pointer at the middle; the port's Space (keys.c port_key_attack(0)) repeats its last kind, the
   slash until . ; or p choose another, keys our table has for other things. So the kind is the mouse's motion in the
   100 ms before the press (motion_speed, the throw's): up at ATTACK_FLICK window heights a second or more, and more
   up than sideways, the bash (the port's p: swing 9), down the stab (its .: swing 3), else the slash: its Space as
   before while the last kind sent was the slash, else its ; (swing 6), which sets the port's last attack back. Only
   the key sent differs: the blow is the original's player_attack with the port's own swings. */
#define ATTACK_FLICK 0.4f
/* the attack's key for the mouse's flick: Space (the slash again), p the bash, . the stab, ; the slash */
static int attack_key(void)
{
    static int last = 0x27;             /* the kind sent last, as keys.c's last_attack follows it (the slash at first) */
    float vx, vy;
    int k;
    motion_speed(&vx, &vy);
    k = fabsf(vy) >= ATTACK_FLICK && fabsf(vy) > fabsf(vx) ? (vy > 0 ? 0x19 : 0x34) : 0x27;
    if (k == 0x27 && last == 0x27) return 0x39;
    last = k;
    if (getenv("UWPORT_LOOKDBG")) RLOG_I("front", "attack: %s", k == 0x19 ? "bash (flick up)" : k == 0x34 ? "stab (flick down)" : "slash");
    return k;
}
/* a press or release at the crosshair in full-screen play: look, use, take, throw, attack, a target: 1 taken */
static int world_click(int type, unsigned button)
{
    UwlInfo li;
    if (type == PLAT_POINTER_MOVE) return 0;
    if (type == PLAT_POINTER_DOWN && (uwi_input_mode() == 2 || uwi_input_mode() == 3)) {
        /* the game wants a target (a key used) or an aim (a missile spell): the right button, as the original's, at the
           crosshair (port_target_at, port_aim_at); the left cancels a target. The release is handled as any (below). */
        int aim = uwi_input_mode() == 3;
        world_held |= button;
        if (button == PLAT_BUTTON_RIGHT) uwi_cmd(aim ? UWI_OP_AIM3D : UWI_OP_TARGET3D, -1, -1, 0);
        else if (button == PLAT_BUTTON_LEFT && !aim) uwi_cmd(UWI_OP_TARGET, -1, 0, 0);
        return 1;
    }
    if (button == PLAT_BUTTON_LEFT && getenv("UWPORT_WCLICK_RIGHT")) button = PLAT_BUTTON_RIGHT;   /* (a check's: wclick is a left click) */
    if (type == PLAT_POINTER_UP) {
        if (!(world_held & button)) return (buttons & button) ? 0 : 1;   /* a press the game saw: its release too */
        world_held &= ~button;
        if (button == PLAT_BUTTON_LEFT && wdrag.on) { wdrag.on = 0; send_key(0x10); return 1; }   /* a click, no drag: looks */
        if (button == PLAT_BUTTON_LEFT && world_space) game_key(world_space, 0), world_space = 0;   /* the blow strikes */
        if (button == PLAT_BUTTON_LEFT && carry.on) carry_release(1, 0x56);   /* thrown along the crosshair, or a tap: packed */
        return 1;
    }
    if (type != PLAT_POINTER_DOWN || (button != PLAT_BUTTON_LEFT && button != PLAT_BUTTON_RIGHT)) return 1;
    world_held |= button;
    uwl_info(&li);
    if (button == PLAT_BUTTON_LEFT && !fight_mode && (li.held || (li.kind == 3 && li.take && li.reach))) {
        /* the left button on a thing that can be taken: onto the cursor (the port's n: player_3dget), held
           while the button is (its picture on the crosshair), thrown out where the crosshair aims when it comes up, the
           mouse's momentum its strength (carry_release); a tap with the view still puts it in the pack instead, as E.
           Something already on the cursor is thrown the same way. */
        if (!li.held) { wdrag.on = 1, wdrag.stack = li.stack, wdrag.moved = 0, wdrag.t0 = ms_now(); return 1; }   /* (taken once it drags or is held) */
        carry_begin(0, li.stack, 0);
        return 1;
    }
    if (li.held) {                    /* put down where the pointer is: the original's right click there (DoSpecialActions(DISP_WORLD)) */
        uwi_cmd(UWI_OP_DROP, (int)last_x, 199 - (int)last_y, 0);
        return 1;
    }
    if (button == PLAT_BUTTON_LEFT && fight_mode) world_space = attack_key(), game_key(world_space, 1);   /* the attack: held charges */
    else send_key(button == PLAT_BUTTON_LEFT ? 0x10 : 0x12);           /* q looks, e uses */
    return 1;
}

/* the world's left or right press let go when its full play is no longer up (the death, our menu, the focus lost, Tab:
   uwinput gives the world every release of its presses, early ones too): the attack's Space let go whatever else has
   changed (before, the Space stayed down: the game's key_on held, the keyboard's repeat), a look or a carry dropped */
static void world_let_go(unsigned button)
{
    int left = button == PLAT_BUTTON_LEFT;
    if (left && getenv("UWPORT_WCLICK_RIGHT")) button = PLAT_BUTTON_RIGHT;   /* (as world_click) */
    world_held &= ~button;
    if (!left) return;                  /* (the Space, the look's drag and a carry are the left press's) */
    if (world_space) game_key(world_space, 0), world_space = 0;
    wdrag.on = 0;
    if (carry.on) carry.on = 0, carry.t_end = ms_now();
}

/* an input script's motion met by the panel, the world or a centred piece (uwinput gives a move to the owner of a
   held button): the game's as before (taken: not sent twice) */
static int script_move(void)
{
    script_to_game(script_ev);
    return 1;
}

/* ---- the window's input through uwinput.c (docs\superpowers\specs\2026-11-12-input-rewrite-design.md): each press
   goes to one owner, its release always to the same one. The owners' handlers: ---- */

/* the settings screen (F11 opens and closes it; it has every key and the pointer while it is open) */
static int settings_event(int kind, int c, int down, int x, int y)
{
    if (kind == UWI_KEY) {
        if (c == 0x57) {                /* F11 */
            if (down == 1) {
                if (settings_open()) settings_key(SET_KEY_F11);
                else settings_show(1);
                sync_open();
            }
            return 1;
        }
        if (settings_open()) {
            int k = set_key_of(c);
            if (down && k >= 0) settings_key(k);   /* (its repeats too) */
            sync_open();
        }
        return 1;
    }
    if (!settings_open()) return 1;     /* (a press's release after the screen closed: nothing) */
    if (script_ev) {                    /* an input script's: its 320 x 200 point */
        settings_pointer((int)(script_ev->x * SET_W / 320), (int)(script_ev->y * SET_H / 200),
                         script_ev->type != PLAT_POINTER_UP && (script_ev->buttons & PLAT_BUTTON_LEFT));
        sync_open();
        return 1;
    }
    if (!hooks->pointer || !L.have_dst) return 1;
    if (kind == UWI_BUTTON && c == PLAT_BUTTON_LEFT) left_down = down;
    if (kind == UWI_BUTTON && down && c == PLAT_BUTTON_LEFT) settings_at(x, y, 0);
    settings_at(x, y, left_down);
    sync_open();
    return 1;
}

/* a panel press's release with the focus gone (finding 1): let go, nothing done */
static void panel_let_go(unsigned button)
{
    if (button == PLAT_BUTTON_LEFT && getenv("UWPORT_INVTEST_RIGHT")) button = PLAT_BUTTON_RIGHT;   /* (as inv_pointer) */
    inv_swallow &= ~button;
    if (button != PLAT_BUTTON_LEFT) return;
    inv_lp.on = 0, lift.on = 0;         /* (no look; a thing lifted stays on the pointer) */
    if (carry.on && carry.panel) carry.on = 0, carry.t_end = ms_now();   /* (no throw, no packing: it stays on the pointer) */
}
/* our panel (I, Tab): its pointer events while full-screen play is up (inv_pointer); a press it took has its release
   here whatever has changed since (the panel closed meanwhile, or the focus lost: panel_let_go). Keys: not its own (the world's) */
static int panel_event(int kind, int c, int down, int x, int y)
{
    if (kind == UWI_KEY) return 0;
    if (script_ev && kind == UWI_MOVE) return script_move();   /* (an input script's motion: the game's, as before) */
    if (kind == UWI_BUTTON && !down) {
        /* (the focus lost: uwi_release_all's early release, full_play not depending on the focus; the press let go
           with nothing done: what is on the pointer stays there; the final review's finding 1) */
        if (inv_open && focused) inv_pointer(PLAT_POINTER_UP, x, y, (unsigned)c);
        else panel_let_go((unsigned)c);
        return 1;
    }
    if (!(inv_open && display_mode == 0 && full_play())) return 0;
    return inv_pointer(kind == UWI_MOVE ? PLAT_POINTER_MOVE : PLAT_POINTER_DOWN, x, y, kind == UWI_MOVE ? 0 : (unsigned)c);
}

/* the world (full-screen play in our display, no panel, no centred piece): look, use, attack at the crosshair
   (world_click). Its presses' releases are its own: in play as world_click has them, else world_let_go (the attack's
   Space let go whatever full_play says) */
static int world_event(int kind, int c, int down, int x, int y)
{
    /* (focused: a release uwi_release_all gives at WM_KILLFOCUS takes the let-go path, never world_click's: no look,
       no throw, no packing; the final review's finding 1) */
    int live = display_mode == 0 && full_play() && !inv_open && !settings_open() && !uwc_shown() && focused;
    if (kind == UWI_KEY) return 0;
    if (script_ev && kind == UWI_MOVE) return script_move();   /* (an input script's motion: the game's, as before) */
    if (kind == UWI_MOVE) {             /* not the world's: a held button's drag goes on to the game as before */
        if (world_held) pointer_game(PLAT_POINTER_MOVE, x, y, 0);
        return 0;
    }
    if (down) return live && world_click(PLAT_POINTER_DOWN, (unsigned)c);
    if (live) world_click(PLAT_POINTER_UP, (unsigned)c);
    else world_let_go((unsigned)c);
    return 1;
}

/* a centred piece of the original's (a conversation, the map, typed text in our display: uwconv.c): the pointer onto
   the piece (pointer_game's uwc part); a press it took has its release at the press's point if the piece is gone */
static int centred_event(int kind, int c, int down, int x, int y)
{
    if (kind == UWI_KEY || (script_ev && down)) return 0;   /* (a script's press: the game's, as before) */
    if (script_ev && kind == UWI_MOVE) return script_move();   /* (an input script's motion: the game's, as before) */
    if (kind == UWI_BUTTON && !down) { pointer_game(PLAT_POINTER_UP, x, y, (unsigned)c); return 1; }
    if (!uwc_shown()) return 0;
    pointer_game(kind == UWI_MOVE ? PLAT_POINTER_MOVE : PLAT_POINTER_DOWN, x, y, kind == UWI_MOVE ? 0 : (unsigned)c);
    return 1;
}

/* the game's own screen (uwinput's GAME owner): a script's event as it is, the window's through pointer_game */
static void game_pointer(int type, float x, float y, unsigned button)
{
    if (script_ev && script_ev->type == type && (type == PLAT_POINTER_MOVE || script_ev->button == button)) {
        script_to_game(script_ev);
        return;
    }
    pointer_game(type, (int)x, (int)y, button);
}

static const UwiOut uwi_out = { game_key, game_pointer, settings_event, menu_event, panel_event, world_event,
                                centred_event, our_action };

/* Shift held as ours (the run: uwinput's world owns a Shift key): the panel's Shift clicks, a stack taken whole */
static int shift_ours(void) { return uwi_owner_of_key(0x2A) == UWI_WORLD || uwi_owner_of_key(0x36) == UWI_WORLD; }

/* the state the window's input is decided in, now */
static void input_state(UwiState *s)
{
    memset(s, 0, sizeof *s);
    s->screen = screen_now();
    s->settings = settings_open();
    s->menu = menu_open;
    s->panel = inv_open && display_mode == 0;
    /* the original's options panel (save, restore, quit: it takes every key) up, or a piece of its centred */
    s->options_panel = panel_wait || uwl_left_panel() == 1 || uwc_shown();
    s->focused = focused;
    s->display_ours = display_mode == 0 && !iface;   /* (Tab's or R's interface: the original's presentation) */
    s->full_live = full_play();
    s->mouse_look_enh = ENHANCED(ENH_MOUSE_LOOK) != 0;
#ifndef UWPORT_PLAIN
    {
        extern int16_t rp_request;
        s->replaying = rp_request == 2;
    }
#endif
    s->ctrl_down = uwp_ctrl_down();
    /* (not over the original's options panel: Esc is its own there, it closes it; our menu opened under it unseen and
       paused the game) */
    /* (nor while an instrument is played: SOUND.C play_instrument's loop ends only at Esc; uwlog.c knows it from the
       game's messages.) */
    s->esc_menu = (V.ok && V.menu ? full_live : soft_live()) && !s->settings && uwl_left_panel() != 1 && !uwlog_instrument();
    s->tab_ours = view_full_ok() && !s->settings;
    s->look_ours = display_mode == 0;   /* (the want's mouse-look: ` swallowed; the original's display: ` its toggle) */
}

/* mouse-look's want to the game (ui/MOUSE.C applies it at its key poll, on its own thread): ours once our display has
   started (display_mode set by display_init; GDI alone and the plain build: the ` key's toggle alone, as before). In
   the original's display (display_mode 1, also after a switch from ours at run time) "no wish" (-1): the game's state
   stays as it is and the ` key's toggle rules, as before the rewrite */
static void look_publish(void)
{
#ifndef UWPORT_PLAIN
    static int said = -2;
    UwiState s;
    int w;
    if (display_mode < 0) return;
    input_state(&s);
    if (look_off_due && (display_mode != 1 || !port_mouse_look())) look_off_due = 0;
    /* (after a switch to the original's display: 0 until the game has applied it, as b9ba607's follower turned it off
       there; MOUSE.C applies an off whatever the enhancement, at key(), recorded) */
    w = display_mode == 0 ? uwi_look_wanted(&s) : look_off_due ? 0 : -1;
    if (w != said) said = w, dbg_ms(w > 0 ? "input: mouse-look wanted, screen" : w == 0 ? "input: mouse-look not wanted, screen"
                                    : "input: mouse-look no wish (the original's display: the ` toggle), screen", s.screen);
    port_mouse_look_want(w);
#endif
}

/* the pointer held in the window (ClipCursor, the host's cursor hidden) exactly while the game's mouse-look is on
   (port_mouse_look(), the game's applied state) and the window has the focus, not under our menu or the settings
   screen; or while the pointer lock option holds it */
static void capture_sync(void)
{
    int look = port_mouse_look() != 0, clip;
    if (look != captured) {
        captured = look;
        if (getenv("UWPORT_LOOKDBG")) RLOG_I("front", "mouse-look's capture %s at %lu ms", captured ? "on" : "off", (unsigned long)(ms_now() - L.start));
    }
    clip = uwp_window() && focused && !settings_open() && !menu_open && (locked || captured);   /* (our menu frees it, the lock's too: b9ba607's) */
    if (clip != clip_on) {
        clip_on = clip;
        uwp_capture(clip);
    }
    {   /* the original's options panel up (mouse-look let go): the pointer held on its buttons, and when it comes up
           put in their middle with the game's cursor there (mouse-look had left the hidden pointer at the window's edge,
           and the drawn cursor stopped at the panel's edge while the hidden one went on: clicks missed) */
        static int pclip;
        int x0, y0, x1, y1, rw, rh, mx, my;
        int pc = !clip && display_mode == 0 && uwp_window() && focused && !offscreen() && !settings_open() && !menu_open &&
                 uwc_options() && uwc_options_rect(&x0, &y0, &x1, &y1) && uwp_window_size(&rw, &rh) && rw > 0 && rh > 0;
        if (pc) {
            if (view_w > 0 && view_h > 0)   /* the view's pixels into the window's (real_to_view's inverse) */
                x0 = (int)((long long)x0 * rw / view_w), x1 = (int)((long long)x1 * rw / view_w),
                y0 = (int)((long long)y0 * rh / view_h), y1 = (int)((long long)y1 * rh / view_h);
            /* (held on the screen's rectangle of it, again when that moved; the first time the pointer put in its middle,
               and the game's cursor there) */
            if (uwp_clip_hold(x0, y0, x1, y1, !pclip, &mx, &my)) pointer_msg(PLAT_POINTER_MOVE, mx, my, 0);
        } else if (pclip && !clip_on) uwp_clip_free();
        pclip = pc;
    }
}

/* each pass of the loop: held presses their owners may no longer keep let go (the screen changed, our menu opened),
   mouse-look's want, the capture */
static void input_sync(void)
{
    UwiState s;
    /* the original's options panel (save, restore, quit) takes the keys till it closes */
    if (panel_wait && ms_now() - panel_t0 > 800 && uwl_left_panel() != 1) panel_wait = 0;
    input_state(&s);
    uwi_state(&s);
    look_publish();
    capture_sync();
}

/* the half width's tangent of the original's lens: the zoom word the renderer really works with is 7ED2h for
 * the player's view even where curZoom (place_3d_view) is 61A8h / 6062h (measured: 0.99, not 0.76) */
static float view_tan(int zoom)
{
    if (zoom == 0x61A8 || zoom == 0x6062) zoom = 0x7ED2;
    return (float)zoom / 32768.0f;
}
#ifdef UWPORT_GAMEVIEW
int uwc_options(void);   /* uwconv.c: the original's options panel up */
/* uwconv.c's text (step A2): palette colour i as the screen picture has it (0x00RRGGBB; present() fills L.lut) */
unsigned plat_pal_rgb(int i) { return L.lut[i & 255]; }
int uwc_mapping(void);   /* uwconv.c: the automap */
/* our display on the game's 3D screen (PS_GAME): a frame whose 3D view cannot be shown (why: "no view", "no match",
   "level") keeps our last frame, for as long as it lasts, and never shows the original's 3D-screen picture in its
   place (the input rewrite's design, "The display"); have: a last frame to keep (its width). Logged under
   UWPORT_LOOKDBG at most once a second. 0 where the screen has its own presentation (a conversation, the options
   panel, the map: uwconv.c's; the original's interface asked for: iface) or the original's display. */
static int keep_last(const char *why, int have)
{
    static uint64_t said;
    if (display_mode != 0 || !V.hud || iface || have <= 0 || screen_now() != PS_GAME || uwc_talking() || uwc_options() || uwc_mapping())
        return 0;
    if (getenv("UWPORT_LOOKDBG") && (!said || ms_now() - said >= 1000)) {
        said = ms_now();
        RLOG_I("front", "display: kept our last frame (%s) at %lu ms", why, (unsigned long)(said - L.start));
    }
    return 1;
}
#endif
/* the level's scene, the game held still while it is built (at a start, a load and a level change; else the game
   plays on, unseen, while this thread builds the scene). Held as under our menu (port_pause: the
   game gets no time and waits at its next clock read); not when the menu or the settings screen holds it already */
static unsigned game_renders;   /* the game's count of frames drawn (view_world: uwg_gameview's) */
/* our sound silent from the moment a level change is seen until a quarter of a second after its scene is up (no
   sound before the level is seen), and Steam Audio's pathing told (uwaudio_level_change) */
#ifndef UWPORT_GAMEVIEW
#define uwg_scene(level, building) ((void)0)
#define uwg_scene_beat() ((void)0)
#define uwg_scene_level() (-1)
#endif
static int hush_on;
static uint64_t hush_end;
/* our sound hushed for a level's build (begin 1), let back in a quarter second after it (begin 0) */
static void hush(int begin)
{
    if (begin) {
        if (!hush_on) uwaudio_volume(0), uwaudio_level_change();
        hush_on = 1, hush_end = 0;
    } else if (hush_on && !hush_end) hush_end = ms_now() + 250;
}
/* the hush ended once its time has come */
static void hush_tick(void)
{
    if (hush_on && hush_end && ms_now() >= hush_end) hush_on = 0, hush_end = 0, uwaudio_volume(fx_vol / 100.0f);
}
static int game_level;          /* and the level of its last one */
/* the level's scene built (or kept) by the renderer, the game held still and our sound hushed meanwhile: 0 none */
static int level_scene(const unsigned char *block, int level)
{
    int st =V.level_state ? V.level_state(block, level) : 0, hold, r;
#ifdef UWPORT_GAMEVIEW
    /* (the block's shape changed under the same level number: the game is reading a new level in, and the number
       we have is the last drawn frame's. Nothing is built from that mix, and uwview does not read the block,
       until the game has drawn a frame again (its number is then the block's); 15 s at most) */
    static int waiting;
    static unsigned wait_from;
    static uint64_t wait_t;
    if (st == 2 && V.level_wait) {
        if (!waiting) waiting = 1, wait_from = game_renders, wait_t = ms_now();
        if ((game_renders == wait_from || game_level != level) && ms_now() - wait_t < 15000) {
            V.level_wait(1);
            hush(1);
            return 1;   /* (the scene there stays up) */
        }
    }
    if (waiting && V.level_wait) V.level_wait(0);
    waiting = 0;
#endif
    hold = st != 0 && !menu_open && !settings_open();
    if (st != 0) hush(1), uwg_scene(-1, 1);
    /* (the build takes 0.2 to 0.8 s; in DOS the music played on through a level's load, from the timer's interrupt.
       The "Music in the menu" row decides here too, as under our menu: on, the game's time stands still and the
       music plays on; the effects stay hushed.) */
    if (hold) port_pause(1);
    r = V.level(block, level);
    uwg_scene(level, 0);   /* (done, or as far as it goes: the gate does not wait for a scene that cannot be made) */
    hush(0);
    if (hold) port_pause(0);
    return r;
}
/* our 3D view set up for this picture from the game's view (camera, level scene, lens, the pixels left clear): 1 ours */
static int view_world(void)
{
#ifdef UWPORT_GAMEVIEW
    /* uw-new: the game says (portview.h's hooks, uwgame.c's uwg_gameview): its 3D window shows its frame buffer
       (on_screen: from the first copy to the screen until the automap, a conversation, a cutscene or the start menu
       takes the window), with the camera, level, light and lens of the frame drawn last. Full-screen play
       (full_live) unless a fade (out3d, in3d) is under way: the fades work on the original's own picture
       (portview.c draws it for them), shown in its window. No frame is matched. */
    static UwgSnap one, prev;
    static long epoch;
    UwgView g;
    int x, y, stale = 0;
    full_live = 0;
    if (epoch != view_epoch) epoch = view_epoch, one.w = prev.w = 0;   /* the last game's view gone: nothing to keep */
    if (!V.level || !V.camera || L.w != 320 || L.hgt != 200) return 0;
    int gok = uwg_gameview(&g);
    if (gok) game_renders = g.renders, game_level = g.level;
    uwg_scene_beat(), hush_tick();
    /* the game stands at the gate with a level we have no scene for (uwgame.c's scene_gate): built now, whatever
       its screen shows, and the kept frame's level and camera are that frame's from here */
    if (gok && g.block && g.level >= 1 && g.level <= 9 && g.level != uwg_scene_level()) {
        level_scene(g.block, g.level);
        if (one.w > 0)
            one.x = g.x, one.y = g.y, one.z = g.z, one.facing = g.facing, one.pitch = g.pitch, one.bank = g.bank, one.eye = 1,
            one.level = g.level, one.light = g.light, one.zoom = g.zoom, one.block = g.block, prev = one;
    }
    if (!gok) {
        if (!keep_last("no view", one.w)) return 0;
        stale = 1;
    } else if (!g.on_screen || g.w <= 0 || g.w > 320 - UWG_VIEW_X || g.h <= 0 || g.h > 200 - UWG_VIEW_Y) {
        /* the view not on the game's screen: after a conversation, the map or the options panel the game redraws its
           screen and fades its own picture in before the first copy; our last frame stays up through that (not the
           original's picture, stretched, for a second): on the 3D screen in our display for as long as it lasts
           (keep_last), elsewhere (PS_TEXT) up to two seconds */
        if (!keep_last("no match", one.w)
            && !(panel_left && one.w > 0 && (uwc_on_view_screen() || screen_now() == PS_TEXT) && ms_now() - panel_left < 2000)) return 0;
        stale = 1;
    } else {
        panel_left = 0;
        prev = one;
        one.w = g.w, one.h = g.h, one.x = g.x, one.y = g.y, one.z = g.z, one.facing = g.facing, one.pitch = g.pitch;
        one.bank = g.bank, one.eye = 1, one.level = g.level, one.light = g.light, one.zoom = g.zoom, one.block = g.block;
    }
    const UwgSnap *s = &one;
    /* a level whose scene failed is tried again at most once a second: uwview's level_build gives up its scene before
       it fails, so each try costs a rebuild of the last level too (before, both every frame; the final review's
       finding 2). Between the tries the last frame's snapshot stays, with no V.level call at all while the scene
       built for it is still there (lv_fail.kept) */
    static struct { int on, level, kept, kept_level; const void *block, *kept_block; uint64_t t; } lv_fail;
    int retry = !(lv_fail.on && lv_fail.block == (const void *)s->block && lv_fail.level == s->level
                  && ms_now() - lv_fail.t < 1000);
    if (!retry || !level_scene(s->block, s->level)) {
        /* the frame's level has no scene (yet): the last frame's camera and level stay */
        if (stale || !keep_last("level", prev.w)) return 0;
        if (retry) {
            lv_fail.on = 1, lv_fail.block = (const void *)s->block, lv_fail.level = s->level, lv_fail.t = ms_now(), lv_fail.kept = 0;
            if (getenv("UWPORT_LOOKDBG"))
                RLOG_I("front", "display: level %d has no scene: level %d's kept (tried again in 1 s) at %lu ms",
                       s->level, prev.level, (unsigned long)(ms_now() - L.start));
        }
        one = prev, stale = 1;
        if (!(lv_fail.kept && lv_fail.kept_block == (const void *)s->block && lv_fail.kept_level == s->level)) {
            lv_fail.kept = 0;
            if (!level_scene(s->block, s->level)) return 0;   /* (s is &one: now prev's level, built again) */
            lv_fail.kept = 1, lv_fail.kept_block = (const void *)s->block, lv_fail.kept_level = s->level;
        }
    } else if (lv_fail.on && lv_fail.block == (const void *)s->block && lv_fail.level == s->level) lv_fail.on = 0, lv_fail.kept = 0;
    else lv_fail.kept = 0;   /* (another level's scene built: the kept one gone) */
    full_live = stale || g.effect != 2 || display_mode == 0;   /* ours keeps its own view through a fade (no flash of the original screen) */
#else
    static UwgSnap k[UWG_KEEP];
    int best = -1, bk = -1, n = 0, i, x, y;
    unsigned long long now = ms_now();
    if (!V.level || !V.camera || L.w != 320 || L.hgt != 200) return 0;
    for (i = 0; i < UWG_KEEP && uwg_snapshot_back(i, &k[i]); i++) {
        int same = 0;
        (void)now;
        if (k[i].w <= 0 || k[i].w > 320 - UWG_VIEW_X || k[i].h <= 0 || k[i].h > 200 - UWG_VIEW_Y) continue;
        for (n = 0, y = 0; y < k[i].h; y += 2)
            for (x = 0; x < k[i].w; x += 3, n++)
                same += L.pix[(UWG_VIEW_Y + y) * L.w + UWG_VIEW_X + x] == uwg_fb_at(&k[i], x, y);
        if (same > best) best = same, bk = i;
    }
    if (bk < 0 || best * 10 < n * 8) return 0;
    const UwgSnap *s = &k[bk];
    if (!level_scene(s->block, s->level)) return 0;
#endif
    /* the eye: get_eye's (every camera the original has, its jolts included), or the feet 0xA4 below it */
    V.camera((float)s->x / 256.0f, (float)(s->z + (s->eye ? 0 : 0xA4)) / 256.0f, (float)s->y / 256.0f,
             (float)s->facing * 6.2831853f / 65536.0f, (float)s->pitch * 6.2831853f / 65536.0f);
    if (V.roll) V.roll((float)s->bank * 6.2831853f / 65536.0f);
    if (V.palette) V.palette(L.pal);   /* the DAC now: colour cycling, fades and flashes */
    if (V.light) V.light(s->light);
    seen_mark_frame();   /* our map of what the player saw (the Esc menu's Map what I saw) */
    /* the lens (INSTANCE.ASM's scale_matrix): x and y are scaled so that the view's edges are the 45-degree planes
       CLIP.ASM clips at; measured on the original's own pictures (a test pattern on the wall textures, the white and
       yellow lines of its tile edges and its wall tops; work\agentE\fit4.py): the half width's tangent is 0.99 =
       7ED2h / 8000h, whatever curZoom says (view_tan), and the projection is the same across and up on the 4:3
       screen, so up it is that times the view's half height over half width times 1.2 (the original's pixels are
       1.2 tall) */
    if (V.lens && s->zoom > 0 && s->w > 0) {
        float tx0 = (float)s->zoom / 32768.0f, tx = view_tan(s->zoom);
        if (V.shade_scale) V.shade_scale(tx0);  /* the original's shading measures depth by its own zoom */
        if (full_show() && V.fov) {                                  /* the whole window: uw.exe's field of view */
            float ty = tanf(V.fov() * 0.5f);
            V.lens(ty * (float)V.cw / (float)V.ch, ty);
        } else if (full_show()) V.lens(tx, tx * (float)V.ch / (float)V.cw);
        else V.lens(tx, tx * (float)s->h / (float)s->w * 1.2f);   /* the original's pixels are 1.2 tall on the 4:3 screen */
    }
    memset(clear_px, 0, sizeof clear_px);
    view_marks = 0;
    for (y = 0; y < s->h; y++)
        for (x = 0; x < s->w; x++) {
            int at = (UWG_VIEW_Y + y) * L.w + UWG_VIEW_X + x;
#ifdef UWPORT_GAMEVIEW
            /* exactly the pixels of the 3D picture: the game filled its frame buffer with the mark in place of the
               drawing, and every picture it puts into it or over it afterwards (the weapon, the ornaments, the cursor)
               leaves colour 0 alone (Transparency) */
            clear_px[at] = L.pix[at] == UWG_MARK;
            view_marks += clear_px[at];
#else
            clear_px[at] = L.pix[at] == uwg_fb_at(s, x, y);
#endif
        }
    if (V.view_rect && full_show()) V.view_rect(0, 0, V.cw, V.ch, V.cw, V.ch);
    else if (V.view_rect) {
        int pw = L.dst.right - L.dst.left, ph = L.dst.bottom - L.dst.top;
        V.view_rect(L.dst.left + UWG_VIEW_X * pw / L.w, L.dst.top + UWG_VIEW_Y * ph / L.hgt,
                    L.dst.left + (UWG_VIEW_X + s->w) * pw / L.w, L.dst.top + (UWG_VIEW_Y + s->h) * ph / L.hgt, V.cw, V.ch);
    }
    return 1;
}

/* our 3D view this pass (view_world), given up in a conversation, in our display kept over a few frames that do
   not match: 1 when the view is ours */
static int view_ours(int sw, int sh)
{
#ifdef UWPORT_GAMEVIEW
    /* our renderer draws the 3D view: the game leaves its rasterization out. In the original's display the game
       draws its own again (nothing of ours there) */
    uwg_gameview_replace(display_mode != 1);
#endif
    int ours = sw == L.w && sh == L.hgt && view_world();
    int orig = 0;   /* the original's display with the game's own 3D picture in its window: shown as it is */
#ifdef UWPORT_GAMEVIEW
    /* (a picture the game made before the switch still has our view's marks in its window, and the game is paused
       under the menu: our view stays in them until the game draws again) */
    if (display_mode == 1 && ours && !view_marks) ours = 0, orig = 1;
#endif
    if (display_mode == 0 && V.hud && uwc_talking()) ours = 0;   /* a conversation: its pieces over our last frame (uwconv.c) */
#ifdef UWPORT_GAMEVIEW
    if (!ours && !orig) full_live = 0;   /* view_world set it: the game's word, nothing to bridge */
#else
    {   /* a frame the 3D view cannot be matched in (the game mid-redraw) is not a reason to show the original's
           screen for one frame in our display (a flash): a few in a row keep ours, the last camera;
           the map, a conversation and the like still change over within about a tenth of a second */
        static int misses;
        if (ours) misses = 0, panel_left = 0;
        else if (full_live && display_mode != 1 && sw == L.w && sh == L.hgt
                 && (++misses <= 6 || (panel_left && uwc_on_view_screen() && ms_now() - panel_left < 2000))) ours = 1;
    }
#endif
    return ours;
}
/* what our view shows where the game's pointer stands for (the crosshair; our panel's pointer over the world): the
   game's thread picks from it (uwlook.c's uwl_pick) rather than from its own picture of the view */
static void view_targets(int ours)
{
    int t[UWL_TARGETS], n = -1;
    static int nopick = -1;   /* UWPORT_NOPICK=1 (a check's): the original's pick alone */
    if (nopick < 0) nopick = getenv("UWPORT_NOPICK") != NULL;
    if (V.pick && !nopick && ours && full_play() && V.cw > 0 && V.ch > 0) {
        float fx = 0.5f, fy = 0.5f;
        if (inv_open) fx = (float)inv_mx / (float)V.cw, fy = (float)inv_my / (float)V.ch;
        n = V.pick(fx, fy, t, UWL_TARGETS);
    }
    uwl_set_targets(t, n);
}
/* the crosshair and its readout: asked for in full-screen play, then given to the HUD */
static void view_readout(int ours)
{
    if (V.readout2) {                   /* the crosshair and its readout: asked for in full-screen play, then given to the HUD */
        UwlInfo li;
        int on = ours && full_play();
        uwl_enable(on);
        uwl_info(&li);
        if (!on) memset(&li, 0, sizeof li);
        readout_show(&li, on);
    } else if (V.readout) {
        char name[96];
        uwl_enable(ours && full_play());
        uwl_get(name, sizeof name);
        V.readout(ours && full_play() ? name : "");
    }
}
/* a conversation in our display (uwconv.c): the original's conversation pieces, centred and scaled up, over our 3D
   view's last frame, dimmed; the pointer free (mouse-look not wanted off PS_GAME: input_sync). The layer, or NULL */
static const unsigned char *view_conv_layer(const uint32_t *src, int sw, int sh)
{
    const unsigned char *layer;
    int was = uwc_shown();
    layer = uwc_frame(display_mode == 0 && V.hud ? src : NULL, sw, sh, V.uw, V.uh, V.cw, V.ch);
    if (was && !uwc_shown() && display_mode == 0) panel_left = ms_now();   /* its fade-in: our last frame stays (above) */
    return layer;
}
/* a new game: black until its first view (the game's screen before it is not shown in our display) */
static const unsigned char *view_first_view(int ours, const unsigned char *layer, int sw)
{
    static int seen_view;
    static long epoch;
    if (epoch != view_epoch) epoch = view_epoch, seen_view = 0;   /* a new game: black until its first view */
    if (ours && !seen_view && getenv("UWPORT_LOOKDBG"))   /* (the check's: our first view since the start or the reset) */
        RLOG_I("front", "display: our first view at %lu ms", (unsigned long)(ms_now() - L.start));
    if (ours) seen_view = 1;
    else if (!layer && !seen_view && display_mode == 0 && V.hud && V.ui && sw == 320 && uwc_on_view_screen()) {
        for (size_t i = 0, all = (size_t)V.uw * (size_t)V.uh; i < all; i++) ((uint32_t *)V.ui)[i] = 0xFF000000u;
        layer = V.ui;   /* black until our first view */
    }
    return layer;
}
/* a cutscene in the 3D window (PS_CUT_WINDOW: a vision): it alone, centred, over our view's last frame dimmed; the
   death (PS_DEATH): the skulls alone over black, then the fade. A full-screen cutscene (PS_CUT_FULL: the
   Introduction, a dream, the ending) and the menu are the plain picture, whole (the compose) */
static const unsigned char *view_cut_layer(const unsigned char *layer, int ours, const uint32_t *src, int sw, int sh)
{
    int ps = screen_now();
    if (!layer && !ours && display_mode == 0 && V.hud && L.w == 320 && sw == 320 && (ps == PS_CUT_WINDOW || ps == PS_DEATH))
        layer = uwc_frame_cut(src, sw, sh, V.uw, V.uh, ps == PS_DEATH), cut_now = layer != NULL;
    return layer;
}
/* full-screen play: the HUD only (the rune shelf, the spells, the gargoyle, the beakers), and our message box */
static const unsigned char *view_hud_layer(const uint32_t *src, int sw, int sh)
{
    const unsigned char *layer;
    if (hud_magic_fn) {   /* the rune shelf and the active spells, as PANELS.C last showed them */
        unsigned char r[3], sp[3];
        for (int i = 0; i < 3; i++) r[i] = hud_runes[i], sp[i] = hud_spells[i];
        hud_magic_fn(r, sp);
    }
    if (hud_combat_fn) {   /* the gargoyle's eyes and the blow's charge, for our HUD's pictures */
        int eyes, pow;
        uwl_combat(&eyes, &pow);
        hud_combat_fn(eyes, pow);
    }
    {   /* health, mana and poison for the beakers at our bottom panel's ends (uwv_hud_vitals) */
        if (hud_vitals_fn) {
            int v[5];
            uwi_vitals(v);
            hud_vitals_fn(v);
        }
    }
    layer = V.hud(src, sw, sh);   /* full-screen play: the HUD only */
    if (layer) log_draw();
    return layer;
}
/* (the check's, inputcheck.py display: never expected) the original's 3D screen whole, in place of ours: a line at
   most once a second, with the frames composed so since the last line */
static void view_note_original(void)
{
    static uint64_t said;
    static unsigned frames;
    frames++;
    if (!said || ms_now() - said >= 1000) {
        said = ms_now();
        RLOG_I("front", "display: original picture in PS_GAME at %lu ms (%u frames)", (unsigned long)(said - L.start), frames);
        frames = 0;
    }
}
/* the host's pointer in our display, but for a cutscene (the game's cursor never stands in for it then) */
static int sys_ptr_on;   /* our panels' pointer is the host's cursor now (view_sys_pointer) */
static void view_host_pointer(void)
{
    /* (never the host's pointer over our display, the game's cursor only: the game draws its own, our menu and
       panel draw the game's picture of it) */
    port_stack_whole = display_mode == 0;   /* our display: a stack whole, never the original's typed
                                               dialog; split in our panel (Shift + a click, ask_begin) */
    host_pointer = 0, port_host_pointer = display_mode == 0 && inv_open ? 2 : 0;   /* our panel draws the pointer and the thing on it (the top is the original's screen: its cursor would show twice) */
    if (display_mode == 0 && !sys_ptr_on && !uwp_cursor_hidden() && focused && !offscreen() && !settings_open()) {
        int cpx, cpy, crw, crh;
        if (uwp_pointer_pos(&cpx, &cpy) && uwp_window_size(&crw, &crh) && cpx >= 0 && cpy >= 0 && cpx < crw && cpy < crh)
            uwp_cursor_hide(1);
    }
}
/* the picture scaled into the interface layer (V.ui) where GDI would have put it, black round it, clear where our
   3D view shows (when the renderer did not build the layer itself) */
static void view_fill_layer(const uint32_t *src, int sw, int sh, int ours, int x0, int y0, int x1, int y1)
{
    int x, y;
    for (y = 0; y < V.uh; y++) {
        unsigned char *row = V.ui + (size_t)y * (size_t)V.uw * 4;
        int in_y = y >= y0 && y < y1;
        int sy = in_y ? (y - y0) * sh / (y1 - y0) : 0;
        const uint32_t *srow = in_y ? src + (size_t)sy * (size_t)sw : NULL;
        for (x = 0; x < V.uw; x++) {
            int inside = in_y && x >= x0 && x < x1, sx = inside ? (x - x0) * sw / (x1 - x0) : 0;
            uint32_t c = inside ? srow[sx] : 0;
            row[x * 4] = (unsigned char)(c >> 16), row[x * 4 + 1] = (unsigned char)(c >> 8), row[x * 4 + 2] = (unsigned char)c;
            row[x * 4 + 3] = ours && inside && clear_px[sy * sw + sx] ? 0 : 255;
        }
    }
}
/* our panels' pointer (the inventory's, the Esc menu's) as the host's cursor, when the panel drawn this frame asked for
   it (the row Pointer in menus: System): its picture given to the window when it changed; the host's cursor shown
   while it is ours, and the window's own way again when no panel is open (PLAN-fast-ui.md step A) */
static void view_sys_pointer(void)
{
    static unsigned last, buf[256 * 256];
    int w = 0, h = 0, hx = 0, hy = 0;
    unsigned sig = V.pointer ? V.pointer(buf, 256 * 256, &w, &h, &hx, &hy) : 0;
    /* (our menu frees the pointer even while mouse-look's capture is wanted: capture_sync's clip; the inventory only when
       the pointer is free) */
    int panel = display_mode == 0 && !settings_open() && (menu_open || (inv_open && uwf_pointer_free()));
    if (panel && sig && sig != last) {
        uwp_cursor_picture(buf, w, h, hx, hy), last = sig;
        RLOG_I("front", "pointer: the system's cursor %dx%d (point %d,%d) for the %s at %lu ms", w, h, hx, hy,
               menu_open ? "Esc menu" : "inventory", (unsigned long)(ms_now() - L.start));
    }
    static int none;   /* frames in a row a panel was open but did not ask (the row set to Drawn, or not drawn) */
    none = panel && !sig ? none + 1 : 0;
    if (panel && last && sig && !sys_ptr_on) sys_ptr_on = 1, uwp_cursor_hide(0);
    if ((!panel || none >= 3) && sys_ptr_on) {
        sys_ptr_on = 0, last = 0;
        uwp_cursor_picture(NULL, 0, 0, 0, 0);
        RLOG_I("front", "pointer: the game's own again at %lu ms", (unsigned long)(ms_now() - L.start));
    }
}
/* our menu drawn over the layer by the renderer: its tabs, its rows, the sliders */
static void view_menu_draw(void)
{
    const char *lab[MENU_MAX], *val[MENU_MAX], *hint[MENU_MAX];
    static char vbuf[MENU_MAX][128];
    const char *title = menu_page == 1 ? "Save game" : menu_page == 2 ? "Load game" : pick_prompt ? "Pick a model" : "Options";
    menu_rows_build(lab, val, hint, vbuf);
    menu_sel_fix(1);
    menu_tabs_give();
    if (V.menu_sliders) V.menu_sliders(!menu_page ? vol_frac : NULL, !menu_page ? menu_n : 0);   /* (the sliders' rows: the Fog and smoke tab's) */
    menu_hover = V.menu_rows2 ? V.menu_rows2(title, lab, val, hint, menu_n, menu_sel, menu_mx, menu_my)
                              : V.menu_rows(title, lab, val, menu_n, menu_sel, menu_mx, menu_my);
}
/* the death's fade: the composed picture darkened to black over 1000 ms (port_death_scene waits on it), then black
   until the game leaves PS_DEATH (real_start's menu) */
static const unsigned char *view_death_fade(const unsigned char *layer)
{
    size_t all = (size_t)V.uw * (size_t)V.uh * 4, i;
    unsigned k = fade_state == 2 ? 0 : (unsigned)(ms_now() - fade_t0 >= 1000 ? 0 : 255 - (ms_now() - fade_t0) * 255 / 1000);
    if (fade_state == 1 && k == 0) uwp_xchg(&fade_state, 2);
    if (layer != V.ui) memcpy(V.ui, layer, all);
    for (i = 0; i < all; i += 4)
        V.ui[i] = (unsigned char)(V.ui[i] * k / 255), V.ui[i + 1] = (unsigned char)(V.ui[i + 1] * k / 255),
        V.ui[i + 2] = (unsigned char)(V.ui[i + 2] * k / 255), V.ui[i + 3] = 255;
    layer = V.ui;
    return layer;
}
/* the window's title: the video being made, its seconds left */
static void view_record_title(void)
{
    static int rec_shown = 0;
    int left = V.recording();
    if (left != rec_shown) {
        rec_shown = left;
        if (left > 0) {
            char t[160];
            snprintf(t, sizeof t, "%s - RECORDING VIDEO: %d s left", live.title ? live.title : PLAT_TITLE, left);
            uwp_title(t);
        } else if (live.mouse_lock) mouse_title(locked);
        else uwp_title(live.title ? live.title : PLAT_TITLE);
    }
}
/* where a frame's time goes, every 120 frames (UWPORT_TIMING=1): the 3D view's state (the game's frame kept, the
   level), the interface layer filled, the renderer's frame (its upload, drawing, DLSS, present) */
static void view_timing(uint64_t t0, uint64_t t1, uint64_t t2)
{
    static int on = -1, n;
    static double a, b, c, worst, gap_worst;
    static uint64_t last_t3;
    uint64_t t3 = plat_counter();
    double hz = (double)plat_counter_hz() / 1000.0;
    if (on < 0) { const char *e = getenv("UWPORT_TIMING"); on = e && *e && *e != '0'; }
    a += (double)(t1 - t0) / hz, b += (double)(t2 - t1) / hz, c += (double)(t3 - t2) / hz;
    /* the worst of them too: one slow frame is a hitch the average hides (the second review's item 7: the
       language model on the same GPU as DLSS and ray tracing) */
    if ((double)(t3 - t2) / hz > worst) worst = (double)(t3 - t2) / hz;
    if (last_t3 && (double)(t3 - last_t3) / hz > gap_worst) gap_worst = (double)(t3 - last_t3) / hz;
    last_t3 = t3;
    if (on && ++n == 120) {
        RLOG_I("front", "a frame: the view's state %.2f ms, the interface layer %.2f ms, the renderer %.2f ms "
               "(worst %.2f), frame to frame worst %.2f ms", a / n, b / n, c / n, worst, gap_worst);
        n = 0, a = b = c = worst = gap_worst = 0;
    }
}
/* the window's contents as presented (the renderer's own picture) written as the window shot */
static void view_window_shot(void)
{
    uint8_t *rgba = malloc((size_t)V.cw * (size_t)V.ch * 4), *rgbp = malloc((size_t)V.cw * (size_t)V.ch * 3);
    if (!rgba || !rgbp) RLOG_E("front", "out of memory (%zu bytes) for the window shot", (size_t)V.cw * (size_t)V.ch * 7);
    if (rgba && rgbp && V.readback(rgba)) {
        int i;
        for (i = 0; i < V.cw * V.ch; i++) rgbp[i * 3] = rgba[i * 4], rgbp[i * 3 + 1] = rgba[i * 4 + 1], rgbp[i * 3 + 2] = rgba[i * 4 + 2];
        if (plat_write_png_rgb(live.window_shot_path, rgbp, V.cw, V.ch) == 0)
            RLOG_I("front", "wrote %s (%dx%d, the window, %s)", live.window_shot_path, V.cw, V.ch, view_vk ? "Vulkan" : "D3D12");
        else RLOG_E("front", "cannot write the window shot %s", live.window_shot_path);
    } else if (rgba && rgbp) RLOG_E("front", "the renderer gave no picture for the window shot %s", live.window_shot_path);
    free(rgba), free(rgbp);
    L.shot = 2;
}
/* the picture (sw by sh, 0x00RRGGBB) into the interface layer, where GDI would have put it, black round it; clear
   where our 3D view shows */
static void view_present(const uint32_t *src, int sw, int sh)
{
    uint64_t t0 = plat_counter();
    int ours = view_ours(sw, sh);
    uint64_t t1 = plat_counter();
    int x0 = L.dst.left * V.uw / V.cw, x1 = L.dst.right * V.uw / V.cw;
    int y0 = L.dst.top * V.uh / V.ch, y1 = L.dst.bottom * V.uh / V.ch;
    const unsigned char *layer = NULL;
#ifndef UWPORT_GAMEVIEW
    full_live = ours;
#endif
    view_targets(ours);
    view_readout(ours);
    layer = view_conv_layer(src, sw, sh);
    layer = view_first_view(ours, layer, sw);
    cut_now = 0;
    layer = view_cut_layer(layer, ours, src, sw, sh);
    if (!layer && ours && full_show() && V.hud) layer = view_hud_layer(src, sw, sh);
    if (!layer && !ours && V.compose && display_mode == 0 && V.hud && !iface && screen_now() == PS_GAME && getenv("UWPORT_LOOKDBG")) view_note_original();
    if (!layer) layer = V.compose ? V.compose(src, sw, sh, ours ? clear_px : NULL, x0, y0, x1, y1) : NULL;
    display_init();
    first_pick();
    view_host_pointer();
    if (!layer) {                      /* the renderer did not build it (optimised): the loop of view_fill_layer */
        view_fill_layer(src, sw, sh, ours, x0, y0, x1, y1);
        layer = V.ui;
    }
    inv_pump();
    if (inv_open && full_play() && V.inventory && layer) inv_draw();   /* our inventory panel */
    if (menu_open && V.menu_rows && layer) view_menu_draw();
    view_sys_pointer();
    if (L.shot == 1 && live.window_shot_path && V.want_shot) V.want_shot();
    if (screen_now() == PS_DEATH && fade_state && layer && V.ui) layer = view_death_fade(layer);
    uint64_t t2 = plat_counter();
    int frame_ok = V.frame(layer);
    if (V.recording) view_record_title();
    view_timing(t0, t1, t2);
    if (!frame_ok) {
        if (++V.failed >= 3) {
            RLOG_E("front", "the %s renderer failed three frames in a row: GDI from now on", view_vk ? "Vulkan" : "D3D12");
            V.shutdown();
            uwp_lib_close(V.dll);
            free(V.ui);
            memset(&V, 0, sizeof V);
            uwp_repaint();
        }
        return;
    }
    V.failed = 0;
    if (L.shot == 1 && live.window_shot_path && V.readback) view_window_shot();
}

/* ---- the loop ---- */

/* the effects (docs\superpowers\plans\2026-11-12-fx-audio-and-visual.md): the game's effect events (fxevent.c: one way,
   presentation only) drained once a picture, each to our audio module (uwaudio.c, fx_audio) and to the renderer
   (uwv_fx_event); the listener at the eye the 3D view was drawn from. Places: the game's eighths of a tile (x east, y
   north) as the renderer's tiles (x, z); the height the eye's, or an object's z (32 a tile, as the renderer's). */
static int fx_pan_fix = 1;   /* data\settings.txt fx_pan_fix (1 unless written 0): the Roland effects panned as the game meant */
static unsigned fx_ac_seen;   /* the renderer's acoustic epoch the audio has (0: none) */
/* Our own files' folder (settings.txt, acoustics.txt, our maps): data\ beside the program. In a flat folder (the
   program beside the game's own UW.EXE, no game\ folder) it is uwr\, so that nothing of ours lands in
   the game's DATA\. The renderer makes the same choice (src\uwview.c data_dirs). */
void uwf_our_data(char *out, size_t n)
{
    static const char *const old_way[2] = { "game/UW.EXE", "game/uw.exe" }, *const flat[2] = { "UW.EXE", "uw.exe" };
    static int is_flat = -1;
    const char *base = plat_base_dir();
    if (!base) base = "";
    if (is_flat < 0) {
        char p[UWF_PATH + 16];
        FILE *f = NULL;
        int k;
        is_flat = 0;
        for (k = 0; k < 2 && !f; k++) snprintf(p, sizeof p, "%s%s", base, old_way[k]), f = fopen(p, "rb");
        if (f) fclose(f);
        else {
            for (k = 0; k < 2 && !f; k++) snprintf(p, sizeof p, "%s%s", base, flat[k]), f = fopen(p, "rb");
            if (f) fclose(f), is_flat = 1;
        }
    }
    snprintf(out, n, "%s%s", base, is_flat ? "uwr" : "data");
}
/* our positional sound started (again): OpenAL, the acoustics, the Roland's effects; none in the original's display */
static void fx_audio_start(int again)
{
    char bin[UWF_PATH], data[UWF_PATH];
    const char *base = plat_base_dir();
    if (!base) return;
    if (again) uwaudio_shutdown();
    if (display_orig()) {   /* the original's display: the game's own sound alone (uwaudio_taken 0: SOUND.C plays it) */
        RLOG_I("fx", "the original's display: the game's own sound, no positional sound");
        return;
    }
    snprintf(bin, sizeof bin, "%sbin", base), uwf_our_data(data, sizeof data);
    uwaudio_game_dir(plat_data_root());
    fx_pan_fix = uwaudio_setting(data, "fx_pan_fix", 1);
    volumes_load();
    uwaudio_volume(hush_on ? 0 : fx_vol / 100.0f);   /* (a level's build under way stays silent) */
    RLOG_I("fx", "starting the positional sound (OpenAL, the acoustics, the Roland's effects)");
    uwaudio_init(bin, data);
    fx_ac_seen = 0;   /* (the acoustics' materials are read by uwaudio_init: the level handed over again) */
}
static void fx_pan_fix_read(void)   /* the menu's "Effects' sides" changed: taken at once (fx_pump hands it to the game) */
{
    char data[UWF_PATH];
    const char *base = plat_base_dir();
    if (!base) return;
    uwf_our_data(data, sizeof data);
    fx_pan_fix = uwaudio_setting(data, "fx_pan_fix", 1);
}
/* the level for the sound (the acoustics plan's Tasks 2 and 4): taken from the renderer whenever it built a level
   (its epoch moved) or the audio was started again, the textures turned into their materials (uwacmat.c: the walls',
   the floors', the one ceiling's) and handed to the audio (uwaudio_acoustics); the doors every picture, handed over
   when one opened or shut. With UWPORT_FX_LOG one line says what came. */
/* What a 3D model object is made of, for the sound (acoustics.txt's preset names; -1: it is not there for the
   sound). The original's items 0x150 to 0x16C: the bench, the table, the beam, the barrel, the chair, the chest,
   the nightstand and the bridge are wood; the boulders, the shrine and the pillar stone. Arrows, the moongate,
   levers and the rest are left out. */
static int fx_thing_mat(int item)
{
    const char *name = NULL;
    switch (item) {
    case 0x150: case 0x158: case 0x159: case 0x15B: case 0x15C: case 0x15D: case 0x15E: case 0x164: name = "wood"; break;
    case 0x153: case 0x154: case 0x155: case 0x156: case 0x157: case 0x160: name = "stone"; break;
    default: return -1;
    }
    for (int i = 0; i < uwacmat_count(); i++) if (!strcmp(uwacmat_get(i)->name, name)) return i;
    return 0;
}
/* the level's tiles, doors and things handed to the audio when they changed (the fx: lines stay raw: fxcheck.py reads them by their start) */
static void fx_acoustic_level(void)
{
    static UwvAcTile tile[64 * 64];
    static UwvAcDoor door[128];
    static UwaTile at[64 * 64];
    static UwaDoor ad[128];
    static unsigned char was_shut[128];
    static int was_n = -1, log = -1;
    unsigned e;
    int ceil = -1, nd, open = 0, shut = 0, moved, fresh;
    if (!V.ok || V.failed || !V.ac_grid || !V.ac_doors || !V.ac_epoch) return;
    e = V.ac_epoch();
    fresh = e != fx_ac_seen;
    if (fresh && !V.ac_grid(tile, &ceil)) return;
    nd = V.ac_doors(door, 128);
    moved = fresh || nd != was_n;
    for (int i = 0; i < nd && !moved; i++) moved = (door[i].shut != 0) != was_shut[i];
    if (!moved) return;
    for (int i = 0; i < nd; i++) {
        ad[i].id = door[i].id, ad[i].tx = door[i].tx, ad[i].tz = door[i].tz, ad[i].heading = door[i].heading;
        ad[i].mat = uwacmat_door(door[i].item, door[i].picture), ad[i].shut = door[i].shut;
        was_shut[i] = door[i].shut != 0, shut += door[i].shut != 0;
    }
    was_n = nd;
    if (fresh) {
        for (int i = 0; i < 64 * 64; i++) {
            int w = uwacmat_for(UWA_WALL, tile[i].wall_tex), f = uwacmat_for(UWA_FLOOR, tile[i].floor_tex);
            at[i].type = tile[i].type, at[i].height = tile[i].height;
            at[i].wall_mat = (unsigned char)(w < 0 ? 0 : w), at[i].floor_mat = (unsigned char)(f < 0 ? 0 : f);
            open += tile[i].type != 0;
        }
        int cm = uwacmat_for(UWA_CEILING, ceil);
        uwaudio_acoustics(at, cm < 0 ? 0 : cm, ad, nd);
        {   /* the things standing in the level, as boxes of their material */
            static UwvAcBox vb[256];
            static UwaBox ab[256];
            int nb = V.ac_things ? V.ac_things(vb, 256) : 0, na = 0;
            for (int i = 0; i < nb; i++) {
                int mt = fx_thing_mat(vb[i].item);
                if (mt < 0) continue;
                memcpy(ab[na].lo, vb[i].lo, sizeof ab[na].lo), memcpy(ab[na].hi, vb[i].hi, sizeof ab[na].hi), ab[na++].mat = mt;
            }
            uwaudio_things(ab, na);
            if (getenv("UWPORT_FX_LOG")) fprintf(stderr, "fx: acoustics: %d things standing in the level (of %d model objects)\n", na, nb);
        }
        fx_ac_seen = e;
    } else uwaudio_acoustics(NULL, 0, ad, nd);
    if (log < 0) log = getenv("UWPORT_FX_LOG") != NULL;
    if (!log) return;
    if (fresh) fprintf(stderr, "fx: acoustics: grid epoch %u: %d open tiles, %d doors (%d shut), ceiling texture %d\n", e, open, nd, shut, ceil);
    else fprintf(stderr, "fx: acoustics: doors: %d shut of %d\n", shut, nd);
}
/* a dial of the acoustics changed in the menu (Reverb level, Room size, Muffling by walls): read again, heard at once */
static void fx_acoustics_tune(void)
{
    char data[UWF_PATH];
    const char *base = plat_base_dir();
    if (!base) return;
    uwf_our_data(data, sizeof data);
    uwaudio_tune(data);
}
/* at exit: how often the music's stream ran dry in the run */
static void underruns_at_exit(void)
{
    extern unsigned audio_underruns(void);
    RLOG_I("front", "audio: the music's stream ran dry %u times in this run", audio_underruns());
}
static double fxp_ms(void) { return (double)plat_counter() * 1000.0 / (double)plat_counter_hz(); }
static void fx_pump1(void);
/* each picture: the effects pumped (timed under UWPORT_TIMING) */
static void fx_pump(void)
{
    static int timing = -1;
    if (timing < 0) { const char *e = getenv("UWPORT_TIMING"); timing = e && *e && *e != '0'; }
    double a = timing ? fxp_ms() : 0;
    fx_pump1();
    if (timing && fxp_ms() - a > 30) RLOG_I("front", "a slow sound pump at %lu ms: %.0f ms", (unsigned long)(ms_now() - L.start), fxp_ms() - a);
}
static double fxp_t[4];
/* the effects' events to the audio and the renderer, the listener, the game's sound switches, the music's dry runs */
static void fx_pump1(void)
{
#ifndef UWPORT_PLAIN
    if (fx_restart_at && ms_now() >= fx_restart_at) fx_restart_at = 0, fx_audio_start(1);
    fxp_t[0] = fxp_ms();
    fx_acoustic_level();
    fxp_t[1] = fxp_ms();
    if (fxp_t[1] - fxp_t[0] > 30) RLOG_I("front", "(timing) the level's acoustics %.0f ms", fxp_t[1] - fxp_t[0]);
    PortFxEvent ev[64];
    float ex = 0, ey = 0, ez = 0;
    int n = port_fx_drain(ev, 64), have = 0;
#ifdef UWPORT_GAMEVIEW
    UwgView g;
    if (uwg_gameview(&g)) have = 1, ex = g.x / 256.0f, ey = g.z / 256.0f, ez = g.y / 256.0f,
                          uwaudio_listener(ex, ey, ez, (float)g.facing * 6.2831853f / 65536.0f);
#else
    UwgSnap g;
    if (uwg_snapshot(&g)) have = 1, ex = g.x / 256.0f, ey = (g.z + (g.eye ? 0 : 0xA4)) / 256.0f, ez = g.y / 256.0f,
                          uwaudio_listener(ex, ey, ez, (float)g.facing * 6.2831853f / 65536.0f);
#endif
    for (int i = 0; i < n && have; i++) {
        const PortFxEvent *e = &ev[i];
        float x = e->x < 0 ? -1.0f : (e->x + 0.5f) / 8.0f, z = e->y < 0 ? -1.0f : (e->y + 0.5f) / 8.0f;
        float y = e->z < 0 ? ey : e->z / 32.0f;
        uwaudio_event(e->kind, x, y, z, e->fx, e->vol);
        if (V.fx_event && !V.failed) V.fx_event(e->kind, e->x < 0 ? ex : x, y, e->x < 0 ? ez : z, e->fx, e->mob);
    }
    fxp_t[2] = fxp_ms();
    uwaudio_pump();
    fxp_t[3] = fxp_ms();
    if (fxp_t[3] - fxp_t[2] > 30) RLOG_I("front", "(timing) uwaudio_pump %.0f ms; the events before it %.0f ms", fxp_t[3] - fxp_t[2], fxp_t[2] - fxp_t[1]);
    else if (fxp_t[2] - fxp_t[1] > 30) RLOG_I("front", "(timing) the sound events %.0f ms", fxp_t[2] - fxp_t[1]);
    /* the game's side (SOUND.C fx_play): the effects we play ourselves have their Roland copy silent; the pan mirrored
       for the MT-32's inverted reading of it (portgame.h port_fx_control) */
    cards_load();
    uwaudio_stand_down(cards_run[0] >= 0 && cards_run[0] != 6);   /* (this run's music card is not the Roland: its own effects) */
    port_fx_control(uwaudio_taken(), fx_pan_fix);
    dials_load();
    port_fx_interrupts(display_mode == 1 ? 1 : dial_val[26]);   /* (the original's display: its effects take the music's channels) */
    port_pause_music(dial_val[28]);     /* (the "Music in the menu" row: on unless turned off) */
    port_music_clean_set(dial_val[37]);   /* (the "Music volume, clean" row) */
    {   /* the heat shimmer (the "Heat shimmer" row): the hot air near the eye, from the renderer, to the sound */
        static float heat[96 * 5];
        int nh = dial_val[39] > 0 && V.dll && V.heat_spots ? V.heat_spots(heat, 96) : 0;
        uwaudio_heat(heat, nh, (float)dial_val[39] / 100.0f);
    }
    uwp_music_room(dial_val[29], dial_val[30]);   /* (the "Effects interrupt music" row: 0 unless turned on) */
    {   /* the music's stream (Exhume's audio.c): the times the device found it empty, into the log once a minute
           when there were more, and at the end */
        extern unsigned audio_underruns(void);
        static unsigned said;
        static unsigned long long next;
        static int hooked;
        unsigned long long now = ms_now();
        if (!hooked) hooked = 1, atexit(underruns_at_exit), next = now + 60000;
        {   /* each one as it happens (at most 40 a run): when, and what was up */
            static unsigned each, lines;
            static unsigned long long t0;
            unsigned u = audio_underruns();
            if (!t0) t0 = now;
            if (u != each) {
                if (lines++ < 40) RLOG_W("front", "audio: the music's stream ran dry (%u) %.1f s into the run: %s", u, (double)(now - t0) / 1000.0,
                                         menu_open ? "our menu open" : settings_open() ? "the settings screen open" : screen_now() == PS_GAME ? "in the game" : "another screen");
                each = u;
            }
        }
        if (now >= next) {
            unsigned u = audio_underruns();
            next = now + 60000;
            if (u != said) RLOG_W("front", "audio: the music's stream ran dry %u times so far (%u in the last minute)", u, u - said), said = u;
        }
    }
#endif
}


/* present's first part (the backend's present does the rest with GDI): the effects, the palette, the picture's place,
   the settings screen over it, our renderer */
const uint32_t *uwf_frame(int *swp, int *shp)
{
    int i, sw = L.w, sh = L.hgt;
    const uint32_t *src = L.rgb;
    fx_pump();
    for (i = 0; i < 256; i++) {
        const uint8_t *c = L.pal + 3 * i;
        L.lut[i] = (uint32_t)((c[0] << 2) | (c[0] >> 4)) << 16 | (uint32_t)((c[1] << 2) | (c[1] >> 4)) << 8
                 | (uint32_t)((c[2] << 2) | (c[2] >> 4));
    }
    for (i = 0; i < L.w * L.hgt; i++) L.rgb[i] = L.lut[L.pix[i]];
    place(L.w, L.hgt);
    if (!L.have_dst) return NULL;
    if (settings_draw(L.layer)) {
        /* the settings screen: the picture dimmed (black at 160 of 255), the layer over it, at the
           layer's 640x400 */
        static uint32_t comp[SET_W * SET_H];
        int x, y;
        for (y = 0; y < SET_H; y++)
            for (x = 0; x < SET_W; x++) {
                uint32_t pc = L.rgb[(y * L.hgt / SET_H) * L.w + x * L.w / SET_W];
                uint32_t la = L.layer[y * SET_W + x];      /* ABGR8888: R, G, B, A bytes in memory */
                unsigned a = la >> 24, k = 95;             /* 255 - 160 */
                unsigned r = ((pc >> 16) & 255) * k / 255, g = ((pc >> 8) & 255) * k / 255, b = (pc & 255) * k / 255;
                r = (r * (255 - a) + (la & 255) * a) / 255;
                g = (g * (255 - a) + ((la >> 8) & 255) * a) / 255;
                b = (b * (255 - a) + ((la >> 16) & 255) * a) / 255;
                comp[y * SET_W + x] = r << 16 | g << 8 | b;
            }
        src = comp, sw = SET_W, sh = SET_H;
    }
    if (view_size()) {
        view_present(src, sw, sh);
        return NULL;
    }
#ifdef UWPORT_GAMEVIEW
    uwg_gameview_replace(0);   /* GDI (no renderer, or it failed): the game draws its own 3D view again */
#endif
    if (menu_open && soft_menu_up()) {   /* our menu without our renderer: the backend draws it over this picture */
        const char *lab[MENU_MAX], *val[MENU_MAX], *hint[MENU_MAX];
        static char vbuf[MENU_MAX][128];
        const char *title = menu_page == 1 ? "Save game" : menu_page == 2 ? "Load game" : pick_prompt ? "Pick a model" : "Options";
        menu_rows_build(lab, val, hint, vbuf);
        menu_sel_fix(1);
        menu_tabs_give();
        menu_hover = soft_menu->rows(title, lab, val, hint, menu_n, menu_sel, menu_mx, menu_my);
    }
    *swp = sw, *shp = sh;
    return src;
}

/* loop_step's after the window's messages and the folder picker's answer: the game's picture, the screenshots, a timed
   run's end */
void uwf_tick_scan(void)
{
    hooks->scanout(L.pix, &L.w, &L.hgt, L.pal);
    /* UWPORT_SHOT_FRAME=N (the checks): the screenshots at the game's Nth 3D frame instead of a time, so that two
       replays of a session are taken at the same place (a replay is exact frame by frame, not in time) */
    static long shot_frame = -2;
    if (shot_frame == -2) { const char *e = getenv("UWPORT_SHOT_FRAME"); shot_frame = e && *e ? atol(e) : -1; }
    int frame_due = 0;
    if (shot_frame > 0) {
        static UwgSnap fs;
        frame_due = uwg_snapshot(&fs) && fs.frames >= (unsigned)shot_frame;
    }
    if (!L.shot && ((shot_frame > 0 && frame_due) ||
                    (shot_frame <= 0 && live.screenshot_after_ms > 0 && ms_now() - L.start >= (uint64_t)live.screenshot_after_ms))) {
        const char *path = live.screenshot_path ? live.screenshot_path : PLAT_NAME ".png";
        L.shot = 1;
        if (plat_write_png(path, L.pix, L.w, L.hgt, L.pal) == 0)
            RLOG_I("front", "wrote %s (%dx%d) at %lu ms", path, L.w, L.hgt, (unsigned long)(ms_now() - L.start));
        else RLOG_E("front", "cannot write the screenshot %s", path);
        {
            /* UWPORT_FBDUMP=FILE: the 3D view's frame buffer at its last frame, as it lies in memory (the checks) */
            const char *fbp = getenv("UWPORT_FBDUMP");
            static UwgSnap s;
            if (fbp && *fbp && uwg_snapshot(&s)) {
                int rows = UWG_FB_LEN / s.w;
                if (plat_write_png(fbp, s.fb, s.w, rows, L.pal) == 0)
                    RLOG_I("front", "wrote %s (%dx%d, the 3D frame buffer; %u frames; level %d at %d,%d,%d facing %d pitch %d)",
                           fbp, s.w, rows, s.frames, s.level, s.x, s.y, s.z, s.facing, s.pitch);
                else RLOG_E("front", "cannot write the frame buffer picture %s", fbp);
            }
        }
    }
    if (live.exit_after_ms > 0 && ms_now() - L.start >= (uint64_t)live.exit_after_ms) L.quit = 1;
}

/* ---- the events the backend's window thread gives (uwfront.h) ---- */

void uwf_key(int c, int down, int repeat) { handle_key(c, down, repeat); }
void uwf_key_byte(uint8_t b) { key_byte(b); }
void uwf_pointer(int type, int cx, int cy, unsigned button) { pointer_msg(type, cx, cy, button); }
void uwf_raw_motion(long dx, long dy) { raw_motion(dx, dy); }

/* a move of the pointer, after its event: the game's cursor stands in for the host's over the picture */
int uwf_hover(int x, int y)
{
    if (sys_ptr_on && !settings_open()) return 0;   /* our panels' pointer is the host's cursor: it shows (the menu: even with mouse-look's capture wanted) */
    if (!(!locked && !captured && !settings_open() && L.have_dst)) return -1;   /* our panels' pointer is the host's cursor: it shows */
    float gx, gy;   /* (in a conversation in our display, over its pieces: uwconv.c) */
    int over = uwc_shown() ? uwc_point(x, y, &gx, &gy) : x >= L.dst.left && x < L.dst.right && y >= L.dst.top && y < L.dst.bottom;
    if (display_mode == 0) over = 1;   /* our display: never the host's pointer, the game's only */
    return over;
}
int uwf_pointer_free(void) { return !locked && !captured; }

/* the wheel: our menu's list while it is open, else our panel's open bags, a conversation's scroll back, or the
   message box (up older, down newer; the game has no use for the wheel) */
#define WHEEL_STEP 120                 /* (Win32's WHEEL_DELTA: a notch) */
void uwf_wheel(int delta)
{
    if (menu_open) {
        static int wheel_rest;     /* (a fine wheel's part notches kept till they make one) */
        wheel_rest += delta;
        if (wheel_rest / WHEEL_STEP) menu_wheel_pass(wheel_rest / WHEEL_STEP);
        wheel_rest %= WHEEL_STEP;
    } else if (display_mode == 0 && inv_open && V.inv_wheel && V.inv_hit && inv_snap.valid
               && V.inv_hit(&inv_snap, inv_mx, inv_my, inv_sheet, NULL, NULL) != -1) {
        /* over our panel: the open bags' rows */
        static int inv_rest;
        inv_rest += delta;
        if (inv_rest / WHEEL_STEP) V.inv_wheel(inv_rest / WHEEL_STEP);
        inv_rest %= WHEEL_STEP;
    } else if (display_mode == 0 && uwc_shown() && uwc_talking()) {
        /* a conversation: the NPC's scroll back through the whole conversation (uwconv.c, step A2) */
        void uwc_wheel(int notches);
        static int conv_rest;
        conv_rest += delta;
        if (conv_rest / WHEEL_STEP) uwc_wheel(conv_rest / WHEEL_STEP);
        conv_rest %= WHEEL_STEP;
    } else if (display_mode == 0) {
        int steps = delta / WHEEL_STEP, all = 0;
        char tmp[1][UWLOG_W];
        uwlog_get(tmp, 1, 0, NULL, &all);
        log_back += steps;
        if (log_back > all - UWLOG_ROWS) log_back = all - UWLOG_ROWS;
        if (log_back < 0) log_back = 0;
        log_touch = ms_now();
    }
}

/* the focus lost: every press held let go through its owner (uwinput), mouse-look wanted off and the pointer let go at
   once; on the focus's return the want is ours again (input_sync, each frame) */
void uwf_focus(int on)
{
    if (on) {
        focused = 1;
        input_sync();
        return;
    }
    unsigned n = key_bytes_sent;
    focused = 0;
    left_down = 0;
    L.swallow = 0;
    uwi_release_all();
    if (key_bytes_sent != n) RLOG_I("front", "the window lost the focus: the buttons and keys held let go");
    input_sync();
}
void uwf_set_focused(int on) { focused = on; }
void uwf_resized(void) { if (clip_on) uwp_capture(1); }   /* the pointer held in the window's new rectangle */
void uwf_mouse_unlock(void)
{
    if (locked) {
        locked = 0;
        capture_sync();
        mouse_title(0);
    }
}
void uwf_mouse_title(int is_locked) { mouse_title(is_locked); }

/* loop_step's before the window's messages: the game's mouse-look turned on or off (mousedrv.c's
   plat_pointer_capture): the capture follows it now */
void uwf_tick_begin(void) { if (uwp_xchg(&capture_req, 0)) capture_sync(); }
/* after them: the runtime's tick, the settings screen, the input's step, our taps' breaks once the game has taken them */
void uwf_tick_input(void)
{
    if (hooks->tick) hooks->tick();
    sync_open();
    input_sync();
    tap_pump();
}

void uwf_client_size(int *w, int *h) { client_size(w, h); }
int uwf_offscreen(void) { return offscreen(); }
int uwf_no_input(void) { return no_input(); }
void uwf_view_start(void) { view_start(); }
/* the renderer started before the game's thread runs (started by the first frame, the game's music and sounds
   played for seconds over a window with no picture) */
void uwf_view_warm(void) { view_size(); }
int uwf_view_loaded(void) { return V.dll != NULL; }
int uwf_view_ok(void) { return V.ok; }
void uwf_fx_audio_start(void) { fx_audio_start(0); }   /* fx_audio 1: OpenAL Soft (bin\soft_oal.dll); else nothing */
void uwf_input_init(void) { uwi_init(&uwi_out); }      /* the window's input (uwinput.c) */

/* the loop ended */
void uwf_shutdown(void)
{
    {   /* the language model's worker stopped and the model let go before the DLLs go (uwmindhost.c) */
        void port_mind_stop(void);
        port_mind_stop();
    }
    if (uwp_window()) RLOG_I("front", "%u presents", L.presents);
    if (clip_on) uwp_capture(0);
    if (V.ok) V.shutdown();
    /* OpenAL Soft (fx_audio) closed and let go before the process ends: left open, its threads die in ExitProcess and
       the DLL's own teardown then fails (libstdc++'s "std::__condvar::~__condvar(): Assertion '__e != 16' failed",
       a condition variable destroyed with a waiter) */
    uwaudio_shutdown();
}

/* the checks' own window events, each pass before the window's messages (here, so that every backend has them) */
void uwf_tick_tests(void)
{
    {   /* UWPORT_FOCUSTEST=T1,T2[,T3,T4] (a check's): the window loses the focus at T1 ms and gets it back at T2 (and again
           at T3 / T4), as Alt-Tab away and back does (the messages the window gets then), so that an off-screen run shows
           mouse-look given up and taken again */
        static int fn = -1, fi;
        static unsigned long ft[4];
        if (fn < 0) {
            const char *e = getenv("UWPORT_FOCUSTEST");
            fn = e ? sscanf(e, "%lu,%lu,%lu,%lu", &ft[0], &ft[1], &ft[2], &ft[3]) : 0;
            if (fn < 0) fn = 0;
            fn &= ~1;                       /* (pairs only) */
        }
        if (fi < fn && uwp_window() && ms_now() - L.start >= ft[fi]) {
            int away = !(fi & 1);
            fi++;
            uwp_test_focus(away);
            RLOG_I("front", "focus test: %s", away ? "away" : "back");
        }
    }
    {   /* UWPORT_MOUSETEST="T:EV,T:EV,..." (a check's): the window's own mouse events at T ms, as the window gets them (its
           messages' handlers; nothing of the system's input is made): ld / lu / rd / ru a button down or up and mv a move,
           each @X/Y in the client's 0..10000 (the middle without), raw DX/DY a captured mouse's motion, wh@N/0 the wheel
           N notches (WM_MOUSEWHEEL), sd / su Shift,
           kd / ku @CODE/0 a key's press or release (its set-1 code in decimal, 256 + for E0) */
        static int mt = -1, mn, mi;
        static struct { unsigned long t; char ev[4]; int x, y; } me[256];
        if (mt < 0) {
            const char *e = getenv("UWPORT_MOUSETEST");
            mt = 0;
            while (e && *e && mn < 256) {
                char ev[8] = "";
                int x = 5000, y = 5000, n = 0;
                unsigned long t;
                if (sscanf(e, "%lu:%3[a-z]%n", &t, ev, &n) < 2) break;
                e += n;
                if (*e == '@' || *e == ' ') sscanf(e + 1, "%d/%d%n", &x, &y, &n), e += 1 + n;
                me[mn].t = t, me[mn].x = x, me[mn].y = y, memcpy(me[mn].ev, ev, 4), me[mn].ev[3] = 0, mn++;
                while (*e && *e != ',') e++;
                if (*e == ',') e++;
            }
            mt = mn > 0;
        }
        while (mt && mi < mn && uwp_window() && ms_now() - L.start >= me[mi].t) {
            int crw = 0, crh = 0;
            const char *ev = me[mi].ev;
            int cx, cy;
            uwp_window_size(&crw, &crh);
            cx = (int)((long)me[mi].x * crw / 10000), cy = (int)((long)me[mi].y * crh / 10000);
            RLOG_I("front", "mouse test at %lu ms: %s %d %d", (unsigned long)(ms_now() - L.start), ev, me[mi].x, me[mi].y);
            if (!strcmp(ev, "raw")) uwf_raw_motion(me[mi].x, me[mi].y);
            else if (!strcmp(ev, "mv")) uwf_pointer(PLAT_POINTER_MOVE, cx, cy, 0);
            else if (!strcmp(ev, "sd") || !strcmp(ev, "su")) uwf_key(0x2A, ev[1] == 'd', 0);
            else if (!strcmp(ev, "kd") || !strcmp(ev, "ku")) uwf_key(me[mi].x, ev[1] == 'd', 0);   /* kd@CODE/0: a key */
            else if (!strcmp(ev, "wh"))   /* wh@N/0: the wheel, N notches (+ up), as the window's own wheel event */
                uwf_wheel((short)(me[mi].x * WHEEL_STEP));
            else if (ev[1] == 'd' || ev[1] == 'u')
                uwf_pointer(ev[1] == 'd' ? PLAT_POINTER_DOWN : PLAT_POINTER_UP, cx, cy, ev[0] == 'r' ? PLAT_BUTTON_RIGHT : PLAT_BUTTON_LEFT);
            mi++;
        }
    }
}
