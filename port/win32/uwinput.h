/* uwinput: the window's input in one place (docs\superpowers\specs\2026-11-12-input-rewrite-design.md).
   Pure C, no Win32: the backend (plat_win32.c) hands it the window's key and mouse events with the state they
   happen in, and it decides who owns each press (SETTINGS, MENU, PANEL, WORLD, CENTRED, GAME) and sends it, and its
   release, to that owner only: a release always goes where its press went, whatever has changed since; a held press
   whose owner is no longer valid (the screen left the 3D view, our menu opened) is released at once.
   The one exception: a captured mouse's raw motion (the backend's WM_INPUT) is not an event here. It is relative,
   has no point and must reach the game while a world press is held (turning while a blow charges), so the backend
   sends it straight to the game while the pointer is captured, and in our display only while uwi_look_wanted (or
   the pointer lock option holds the pointer). */
#ifndef UWINPUT_H
#define UWINPUT_H

enum UwiOwner { UWI_NONE, UWI_SETTINGS, UWI_MENU, UWI_PANEL, UWI_WORLD, UWI_CENTRED, UWI_GAME };

/* the screen states: the same values as underworld-exhumed's port\sys\screen.h PS_* (kept here so this stays alone) */
enum { UWI_PS_BOOT = 0, UWI_PS_MENU, UWI_PS_GAME, UWI_PS_MAP, UWI_PS_CONV, UWI_PS_CUT_WINDOW, UWI_PS_CUT_FULL,
       UWI_PS_DEATH, UWI_PS_ENDING, UWI_PS_CHARGEN, UWI_PS_TEXT };

/* an event's kind, for the handlers. A UWI_KEY event's down is 1 for a press, 0 for its release and 2 for a key
   repeat of a press the handler owns (SETTINGS, MENU, PANEL, CENTRED: the keyboard's typematic, as menu_key and
   settings_key had it); repeats of a GAME or WORLD press are never sent anywhere (the game's own keyqueue repeats a
   held code) */
enum { UWI_KEY = 0, UWI_BUTTON = 1, UWI_MOVE = 2 };

/* game_pointer's type: the runtime's PLAT_POINTER_* values; buttons are PLAT_BUTTON_* (1 left, 2 right, 4 middle) */
enum { UWI_PTR_MOVE = 0, UWI_PTR_DOWN = 1, UWI_PTR_UP = 2 };

/* the key table's actions: a code held while the key is, a tap of a code, one of our functions (our_action), the
   walk (W / Up: 0x2D, or 0x11 with Shift) and Shift itself (ours in the world: the run) */
enum { UWI_HOLD = 1, UWI_TAP, UWI_OURS, UWI_WALK, UWI_RUN };

/* our_action's ids: these are all of them. our_action is called with down=1 on the press and down=0 on its release
   (or its early release); the backend acts on down=1 only */
enum {
    UWI_A_AI_LOG = 1,    /* Ctrl+L: the AI log on / off */
    UWI_A_FULLBRIGHT,    /* Shift+L: full bright (V.toggle(4)) */
    UWI_A_SAVE_PAGE,     /* F5: our save page (or the panel's shortcut) */
    UWI_A_LOAD_PAGE,     /* F9: our load page (or the panel's shortcut) */
    UWI_A_SHOT,          /* P: a screenshot to the clipboard */
    UWI_A_RECORD,        /* Shift+P: 15 s of video */
    UWI_A_SHADOWS,       /* . and Shift+O: the objects' shadows (V.toggle(5)) */
    UWI_A_PANEL,         /* I: our inventory panel */
    UWI_A_DLSS5,         /* ': DLSS 5 (V.toggle(0)) */
    UWI_A_RT,            /* ;: ray tracing (V.toggle(1)) */
    UWI_A_STYLE,         /* ]: DLSS 5's style (V.toggle(2)) */
    UWI_A_NORMALS,       /* F6: the normal maps (V.toggle(3)) */
    UWI_A_RUNES,         /* R: our panel on our rune page (2026-11-15; it was the original's interface and its runes) */
    UWI_A_CHARACTER,     /* ,: our panel, its character sheet (2026-11-15; it was the original's interface and its page) */
    UWI_A_FIGHT,         /* C: a tap of 0x3F (the original's fight icon) and the log's "fight mode ... asked" */
    UWI_A_TAB,           /* Tab (when tab_ours): our inventory panel in our display, the original's interface in its */
    UWI_A_MENU           /* F1 and Ctrl+M/F/D/Q in our display: our Esc menu (the original's options panel's keys) */
};

typedef struct {
    int screen;            /* UWI_PS_* (= screen.h's PS_*) */
    int settings, menu, panel, options_panel, focused, display_ours;
    int full_live;         /* the backend's full_live folded with !iface, V.hud and !menu_open */
    int mouse_look_enh, replaying;
    int ctrl_down;         /* Ctrl held on the live keyboard (the backend: GetKeyState(VK_CONTROL) < 0), for Ctrl+L */
    int esc_menu;          /* Esc opens our menu (the backend: the renderer's menu, the 3D view live, no settings screen) */
    int tab_ours;          /* Tab is ours (the backend: the renderer's HUD, no settings screen): UWI_A_TAB */
    int look_ours;         /* our display (the backend's display_mode 0, the Tab or R interface too): mouse-look is the
                              want's there, so the ` key (0x29, the game's toggle) is swallowed; in the original's
                              display ` goes to the game, its toggle */
} UwiState;

/* the outputs; a handler returns 1 when it took the event */
typedef struct UwiOut {
    void (*game_key)(int code, int down);                         /* set-1 code, 0x100 for E0 */
    void (*game_pointer)(int type, float x, float y, unsigned button);
    int (*settings_event)(int kind, int code, int down, int x, int y);
    int (*menu_event)(int kind, int code, int down, int x, int y);
    int (*panel_event)(int kind, int code, int down, int x, int y);
    int (*world_event)(int kind, int code, int down, int x, int y);
    int (*centred_event)(int kind, int code, int down, int x, int y);
    void (*our_action)(int action, int down);
} UwiOut;

void uwi_init(const UwiOut *out);
void uwi_key(const UwiState *s, int code, int down, int repeat);   /* code: set-1, 0x100 for E0 */
void uwi_button(const UwiState *s, unsigned button, int down, int x, int y);
void uwi_move(const UwiState *s, int x, int y);
void uwi_state(const UwiState *s);       /* every frame and on every change: invalid held presses released now */
void uwi_release_all(void);              /* the focus lost */
int uwi_look_wanted(const UwiState *s);  /* mouse-look wanted (the spec's "Mouse-look") */
int uwi_owner_of_key(int code);          /* (tests, logs) the owner of a held key, UWI_NONE when none */

#endif
