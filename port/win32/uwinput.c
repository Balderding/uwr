/* uwinput.c: decides who owns each key press and mouse button press of the window, and holds the key table of our
   display (see uwinput.h). Plain C, no Win32.

   A new press goes to the first owner that applies, in this order:
     SETTINGS  the settings screen is open
     MENU      our Esc menu is open
     (three keys outside the key table: F11 -> SETTINGS, whose handler opens the screen; Esc, where our menu can
      open -> MENU, whose handler opens it; Tab, where it is ours -> WORLD's UWI_A_TAB)
     PANEL     our inventory panel, when its handler takes the event
     WORLD     our display's 3D play: the key table below, or the world's handler for a button
     CENTRED   a conversation, the map or typed text: the original's piece centred, when its handler takes it
     GAME      otherwise the byte or pointer event goes to the game as it comes.
   The owner and what was sent are kept per physical key and per button, and the release always goes to the same
   owner, whatever has changed since. A held WORLD or GAME press that the new state no longer allows (the screen left
   the 3D view, or our menu or the settings opened) is released at once by uwi_state, and its later release sends
   nothing. */
#include <string.h>
#include "uwinput.h"

#define NKEYS 0x200
enum { SH_ANY = 0, SH_SHIFT = 1, SH_CTRL = 2, SH_CTRLALT = 3 };   /* an entry's modifier: none needed, Shift, Ctrl, Ctrl+Alt */
enum { W = UWI_CTX_WORLD, MAP = UWI_CTX_MAP, CONV = UWI_CTX_CONV, ANY = UWI_CTX_ANY };
#define CA (UWI_MOD_CTRL | UWI_MOD_ALT)
static int ctx_overlap(int a, int b);   /* (below: two places where one key would mean two things) */

/* our display's commands, each with the key it has by default (CMD) and bind[], the key each has now: the player's own
   (the Esc menu's Keys tab; the backend loads and keeps them). A label with # is a heading of that tab, no command.
   A WALK's or RUN's code is its bit in w_held / run_held. A key that is some command's shipped key and has no
   command now does nothing in the world (it must not fall through to the game as the command it was); any other
   key without a command goes on to CENTRED / GAME as ever (O alone, N, U, the digits ...). */
/* ctx: where the key works (W our 3D play; MAP, CONV: those screens of our display; ANY: before everything, as the
   settings screen's F11, Tab and the original's option keys always were); mods: a COMBO's Ctrl / Alt for the game.
   Every default is the key as it was before the tab had the row (2026-11-22: every function bindable; the rune keys,
   map keys, conversation answers and icons are the original's own: exhumed game/PLAYER.C, ui/AUTOMAP.C) */
static const struct { const char *label, *id; int action, code, key, shift, ctx, mods; } CMD[] = {
    { "#Moving", "", 0, 0, 0, 0 },
    { "Walk forward",              "walk",      UWI_WALK, 1,     0x11,  SH_ANY },    /* W: walk (the port's X), with the run key run (W, 0x11) */
    { "Walk forward (second key)", "walk2",     UWI_WALK, 2,     0x148, SH_ANY },    /* Up: the same */
    { "Walk back",                 "back",      UWI_HOLD, 0x1F,  0x1F,  SH_ANY },    /* S */
    { "Walk back (second key)",    "back2",     UWI_HOLD, 0x1F,  0x150, SH_ANY },    /* Down: back (S) */
    { "Step left",                 "left",      UWI_HOLD, 0x1E,  0x1E,  SH_ANY },    /* A */
    { "Step right",                "right",     UWI_HOLD, 0x20,  0x20,  SH_ANY },    /* D */
    { "Turn left",                 "turnleft",  UWI_HOLD, 0x14B, 0x14B, SH_ANY },    /* Left */
    { "Turn right",                "turnright", UWI_HOLD, 0x14D, 0x14D, SH_ANY },    /* Right */
    { "Run (held)",                "run",       UWI_RUN,  1,     0x2A,  SH_ANY },    /* Shift (left): ours, the run */
    { "Run (second key)",          "run2",      UWI_RUN,  2,     0x36,  SH_ANY },    /* Shift (right) */
    { "Jump",                      "jump",      UWI_HOLD, 0x2A,  0x39,  SH_ANY },    /* Space: jump (the port's Shift) */
    { "Look up",                   "lookup",    UWI_HOLD, 0x148, 0x149, SH_ANY },    /* PageUp */
    { "Look down",                 "lookdown",  UWI_HOLD, 0x150, 0x151, SH_ANY },    /* PageDown */
    { "Tilt the view up",          "tiltup",    UWI_HOLD, 0x02,  0x02,  SH_ANY },    /* 1 */
    { "Tilt the view down",        "tiltdown",  UWI_HOLD, 0x04,  0x04,  SH_ANY },    /* 3 */
    { "Look straight ahead",       "centre",    UWI_HOLD, 0x4C,  0x4C,  SH_ANY },    /* keypad 5 */
    { "Standing long jump",        "longjump",  UWI_COMBO, 0x2A, 0x39,  SH_CTRL, W, UWI_MOD_CTRL },   /* Ctrl+Space: PLAYER.C's Ctrl+jump */
    { "#Acting", "", 0, 0, 0, 0 },
    { "Use (talk, open, take)",    "use",       UWI_HOLD, 0x12,  0x12,  SH_ANY },    /* E */
    { "Look",                      "look",      UWI_HOLD, 0x10,  0x26,  SH_ANY },    /* L: look (the port's Q) */
    { "Fight mode",                "fight",     UWI_OURS, UWI_A_FIGHT, 0x2E, SH_ANY },   /* C: the original's F5 (the fight icon) */
    { "Cast the spell",            "cast",      UWI_HOLD, 0x42,  0x10,  SH_ANY },    /* Q: cast (F8, PLAYER.C's try_cast) */
    { "Track",                     "track",     UWI_HOLD, 0x43,  0x25,  SH_ANY },    /* K: track (F9, player_use_skill 2) */
    { "Sleep",                     "sleep",     UWI_HOLD, 0x44,  0x2C,  SH_ANY },    /* Z: sleep (F10, player_key_sleep) */
    { "Map",                       "map",       UWI_HOLD, 0x2C,  0x32,  SH_ANY },    /* M: the map (the port's Z) */
    { "Talk icon",                 "icontalk",  UWI_HOLD, 0x3C,  0x3C,  SH_ANY },    /* F2: deal_with_icons 4 */
    { "Get icon",                  "iconget",   UWI_HOLD, 0x3D,  0x3D,  SH_ANY },    /* F3: deal_with_icons 3 */
    { "Look icon",                 "iconlook",  UWI_HOLD, 0x3E,  0x3E,  SH_ANY },    /* F4: deal_with_icons 2 */
    { "Left button (look, take, attack)", "btnleft", UWI_BTN, 1, 0,     SH_ANY },    /* none: a key that clicks at the crosshair */
    { "Right button (use)",        "btnright",  UWI_BTN,  2,     0,     SH_ANY },
    { "#Panels", "", 0, 0, 0, 0 },
    { "Inventory",                 "inventory", UWI_OURS, UWI_A_PANEL, 0x17, SH_ANY },       /* I: our inventory panel */
    { "Rune bag",                  "runes",     UWI_OURS, UWI_A_RUNES, 0x13, SH_ANY },       /* R: our panel on our rune page */
    { "Character page",            "character", UWI_OURS, UWI_A_CHARACTER, 0x33, SH_ANY },   /* ,: our panel, its character sheet */
    { "Statistics page",           "stats",     UWI_HOLD, 0x21,  0x21,  SH_ANY },    /* F */
    { "Inventory: scroll down",    "invdown",   UWI_HOLD, 0x2F,  0x2F,  SH_ANY },    /* V */
    { "Inventory: scroll up",      "invup",     UWI_HOLD, 0x30,  0x30,  SH_ANY },    /* B */
    { "Read the compass",          "compass",   UWI_HOLD, 0x22,  0x22,  SH_ANY },    /* G */
    { "Check the flasks",          "flasks",    UWI_HOLD, 0x23,  0x23,  SH_ANY },    /* H */
    { "Pull the chain (flip the panel)", "chain", UWI_HOLD, 0x41,  0x41,  SH_ANY },    /* F7: pull_chain */
    { "Inventory (second key)",    "tab",       UWI_OURS, UWI_A_TAB, 0x0F, SH_ANY, ANY },   /* Tab (where it is ours) */
    { "#Menus", "", 0, 0, 0, 0 },
    { "Esc menu, and closing any menu", "esc", UWI_FIXED, 0, 0x01, SH_ANY, ANY },   /* always Esc (the rule: never given away) */
    { "Esc menu (second key)",     "menu2",     UWI_OURS, UWI_A_MENU, 0x3B, SH_ANY, ANY },   /* F1: the original's options panel's key */
    { "Settings screen",           "settings",  UWI_OURS, 0,     0x57,  SH_ANY, ANY },       /* F11 (code 0: the settings screen's own) */
    { "Esc menu (the original's Ctrl+M, music)", "ctrlm", UWI_OURS, UWI_A_MENU, 0x32, SH_CTRL, ANY },
    { "Esc menu (the original's Ctrl+F, sound)", "ctrlf", UWI_OURS, UWI_A_MENU, 0x21, SH_CTRL, ANY },
    { "Esc menu (the original's Ctrl+D, detail)", "ctrld", UWI_OURS, UWI_A_MENU, 0x20, SH_CTRL, ANY },
    { "Esc menu (the original's Ctrl+Q, quit)", "ctrlq", UWI_OURS, UWI_A_MENU, 0x10, SH_CTRL, ANY },
    { "#Saving and loading", "", 0, 0, 0, 0 },
    { "Save page",                 "save",      UWI_OURS, UWI_A_SAVE_PAGE, 0x3F, SH_ANY },   /* F5 */
    { "Load page",                 "load",      UWI_OURS, UWI_A_LOAD_PAGE, 0x43, SH_ANY },   /* F9 */
    { "Save page (the original's Ctrl+S)", "ctrls", UWI_OURS, UWI_A_SAVE_PAGE, 0x1F, SH_CTRL, ANY },
    { "Load page (the original's Ctrl+R)", "ctrlr", UWI_OURS, UWI_A_LOAD_PAGE, 0x13, SH_CTRL, ANY },
    { "#The map", "", 0, 0, 0, 0 },
    { "Map: the level above",      "mapup",     UWI_HOLD, 0x11,  0x11,  SH_ANY, MAP },   /* W: AUTOMAP.C port_map_key -1 */
    { "Map: the level below",      "mapdown",   UWI_HOLD, 0x1F,  0x1F,  SH_ANY, MAP },   /* S: +1 */
    { "Map: back to your level",   "maphere",   UWI_HOLD, 0x2E,  0x2E,  SH_ANY, MAP },   /* C: 2 */
    { "#+Conversation answers", "", 0, 0, 0, 0 },
    { "Answer 1",                  "answer1",   UWI_HOLD, 0x02,  0x02,  SH_ANY, CONV },  /* PLAYER.C conv_play_menu 1..4 */
    { "Answer 2",                  "answer2",   UWI_HOLD, 0x03,  0x03,  SH_ANY, CONV },
    { "Answer 3",                  "answer3",   UWI_HOLD, 0x04,  0x04,  SH_ANY, CONV },
    { "Answer 4",                  "answer4",   UWI_HOLD, 0x05,  0x05,  SH_ANY, CONV },
    { "#+Runes on the shelf", "", 0, 0, 0, 0 },   /* PLAYER.C: Ctrl+Alt+a..y port_key_rune; UW1 has no rune X */
    { "Rune An (A)",    "runea", UWI_COMBO, 0x1E, 0x1E, SH_CTRLALT, W, CA }, { "Rune Bet (B)",   "runeb", UWI_COMBO, 0x30, 0x30, SH_CTRLALT, W, CA },
    { "Rune Corp (C)",  "runec", UWI_COMBO, 0x2E, 0x2E, SH_CTRLALT, W, CA }, { "Rune Des (D)",   "runed", UWI_COMBO, 0x20, 0x20, SH_CTRLALT, W, CA },
    { "Rune Ex (E)",    "runee", UWI_COMBO, 0x12, 0x12, SH_CTRLALT, W, CA }, { "Rune Flam (F)",  "runef", UWI_COMBO, 0x21, 0x21, SH_CTRLALT, W, CA },
    { "Rune Grav (G)",  "runeg", UWI_COMBO, 0x22, 0x22, SH_CTRLALT, W, CA }, { "Rune Hur (H)",   "runeh", UWI_COMBO, 0x23, 0x23, SH_CTRLALT, W, CA },
    { "Rune In (I)",    "runei", UWI_COMBO, 0x17, 0x17, SH_CTRLALT, W, CA }, { "Rune Jux (J)",   "runej", UWI_COMBO, 0x24, 0x24, SH_CTRLALT, W, CA },
    { "Rune Kal (K)",   "runek", UWI_COMBO, 0x25, 0x25, SH_CTRLALT, W, CA }, { "Rune Lor (L)",   "runel", UWI_COMBO, 0x26, 0x26, SH_CTRLALT, W, CA },
    { "Rune Mani (M)",  "runem", UWI_COMBO, 0x32, 0x32, SH_CTRLALT, W, CA }, { "Rune Nox (N)",   "runen", UWI_COMBO, 0x31, 0x31, SH_CTRLALT, W, CA },
    { "Rune Ort (O)",   "runeo", UWI_COMBO, 0x18, 0x18, SH_CTRLALT, W, CA }, { "Rune Por (P)",   "runep", UWI_COMBO, 0x19, 0x19, SH_CTRLALT, W, CA },
    { "Rune Quas (Q)",  "runeq", UWI_COMBO, 0x10, 0x10, SH_CTRLALT, W, CA }, { "Rune Rel (R)",   "runer", UWI_COMBO, 0x13, 0x13, SH_CTRLALT, W, CA },
    { "Rune Sanct (S)", "runes2", UWI_COMBO, 0x1F, 0x1F, SH_CTRLALT, W, CA }, { "Rune Tym (T)",  "runet", UWI_COMBO, 0x14, 0x14, SH_CTRLALT, W, CA },
    { "Rune Uus (U)",   "runeu", UWI_COMBO, 0x16, 0x16, SH_CTRLALT, W, CA }, { "Rune Vas (V)",   "runev", UWI_COMBO, 0x2F, 0x2F, SH_CTRLALT, W, CA },
    { "Rune Wis (W)",   "runew", UWI_COMBO, 0x11, 0x11, SH_CTRLALT, W, CA }, { "Rune Ylem (Y)",  "runey", UWI_COMBO, 0x15, 0x15, SH_CTRLALT, W, CA },
    { "Take the last rune off",    "runeoff",   UWI_COMBO, 0x0E, 0x0E, SH_CTRLALT, W, CA },   /* Ctrl+Alt+Backspace */
    { "Cast the runes on the shelf", "runecast", UWI_COMBO, 0x39, 0x39, SH_CTRLALT, W, CA },  /* Ctrl+Alt+Space */
    { "#Picture and extras", "", 0, 0, 0, 0 },
    { "Screenshot",                "shot",      UWI_OURS, UWI_A_SHOT, 0x19, SH_ANY },        /* P: to the clipboard */
    { "Record 15 s of video",      "record",    UWI_OURS, UWI_A_RECORD, 0x19, SH_SHIFT },    /* Shift+P */
    { "Ray tracing on / off",      "rt",        UWI_OURS, UWI_A_RT, 0x27, SH_ANY },          /* ; */
    { "DLSS 5 on / off",           "dlss5",     UWI_OURS, UWI_A_DLSS5, 0x28, SH_ANY },       /* ' */
    { "DLSS 5 style",              "style",     UWI_OURS, UWI_A_STYLE, 0x1B, SH_ANY },       /* ] */
    { "Normal maps on / off",      "normals",   UWI_OURS, UWI_A_NORMALS, 0x40, SH_ANY },     /* F6 */
    { "Object shadows on / off",   "shadows",   UWI_OURS, UWI_A_SHADOWS, 0x34, SH_ANY },     /* . */
    { "Object shadows (second key)", "shadows2", UWI_OURS, UWI_A_SHADOWS, 0x18, SH_SHIFT },  /* Shift+O (O alone the game's) */
    { "Full bright on / off",      "fullbright", UWI_OURS, UWI_A_FULLBRIGHT, 0x26, SH_SHIFT },   /* Shift+L */
    { "AI log on / off",           "ailog",     UWI_OURS, UWI_A_AI_LOG, 0x26, SH_CTRL },     /* Ctrl+L */
};
#define NCMD ((int)(sizeof CMD / sizeof CMD[0]))
static struct { int key, shift; } bind[sizeof CMD / sizeof CMD[0]];
static int bind_made;
static void bind_make(void)
{
    if (bind_made) return;
    bind_made = 1;
    for (int i = 0; i < NCMD; i++) bind[i].key = CMD[i].key, bind[i].shift = CMD[i].shift;
}
int uwi_cmd_count(void) { return NCMD; }
const char *uwi_cmd_label(int i) { return i >= 0 && i < NCMD ? CMD[i].label : ""; }
const char *uwi_cmd_id(int i) { return i >= 0 && i < NCMD ? CMD[i].id : ""; }
void uwi_cmd_get(int i, int which, int *key, int *shift)   /* which 0: the key now; 1: the shipped one */
{
    bind_make();
    if (i < 0 || i >= NCMD) { *key = 0, *shift = 0; return; }
    *key = which ? CMD[i].key : bind[i].key, *shift = which ? CMD[i].shift : bind[i].shift;
}
/* command i's key (0: none); taken from any other command that had it (returned: that command, -1 none). how 1: as
   loaded from the settings, nothing taken */
int uwi_cmd_set(int i, int key, int shift, int how)
{
    int lost = -1;
    bind_make();
    if (i < 0 || i >= NCMD || CMD[i].label[0] == '#' || CMD[i].action == UWI_FIXED || key < 0 || key >= 0x200 || key == 0x01) return -1;
    if (key && !how)
        for (int k = 0; k < NCMD; k++)
            if (k != i && bind[k].key == key && bind[k].shift == shift && ctx_overlap(CMD[i].ctx, CMD[k].ctx) && CMD[k].action != UWI_FIXED)
                bind[k].key = 0, bind[k].shift = SH_ANY, lost = k;
    bind[i].key = key, bind[i].shift = key ? shift : SH_ANY;
    return lost;
}
void uwi_cmd_defaults(void) { bind_made = 0; bind_make(); }
int uwi_cmd_fixed(int i) { return i >= 0 && i < NCMD && CMD[i].action == UWI_FIXED; }
/* two places where one key would mean two things: the same, or either anywhere (checked before the rest) */
static int ctx_overlap(int a, int b) { return a == b || a == ANY || b == ANY; }
int uwi_cmd_conflict(int i, int key, int shift)
{
    bind_make();
    if (i < 0 || i >= NCMD || !key) return -1;
    if (key == 0x01) return -2;   /* Esc: always the menus' */
    for (int k = 0; k < NCMD; k++)
        if (k != i && CMD[k].label[0] != '#' && bind[k].key == key && bind[k].shift == shift && ctx_overlap(CMD[i].ctx, CMD[k].ctx)) return k;
    return -1;
}

static UwiOut O;
static struct { unsigned char owner, action, mods; unsigned short code; } kp[NKEYS];   /* per physical key (code: what
   the game was sent for it; mods: the Ctrl / Alt a COMBO sent itself) */
static unsigned char bown[3];                                                    /* per button: its owner */
static int last_x, last_y;
static int w_held, run_held, walk_sent;   /* w_held: bit 0 the walk's key, bit 1 its second; run_held: the run's two */
static int shift_phys;                    /* the keyboard's Shift keys held (bit 0 left, bit 1 right): a command's Shift+ */

/* sends a key or pointer event to the game, if a sink is set */
static void gkey(int code, int down) { if (O.game_key) O.game_key(code, down); }
static void gptr(int type, int x, int y, unsigned b) { if (O.game_pointer) O.game_pointer(type, (float)x, (float)y, b); }
/* calls the owner's handler; 0 when it has none or does not take the event */
static int handler(int owner, int kind, int code, int down, int x, int y)
{
    int (*h)(int, int, int, int, int) =
        owner == UWI_SETTINGS ? O.settings_event : owner == UWI_MENU ? O.menu_event : owner == UWI_PANEL ? O.panel_event :
        owner == UWI_WORLD ? O.world_event : owner == UWI_CENTRED ? O.centred_event : 0;
    return h ? h(kind, code, down, x, y) : 0;
}

/* sends the forward key the game should see held: W or Up with Shift is its run (W), without Shift its walk (X);
   it follows Shift going down or up while forward is held */
static void walk_update(void)
{
    int want = w_held ? (run_held ? 0x11 : 0x2D) : 0;
    if (want == walk_sent) return;
    if (walk_sent) gkey(walk_sent, 0);
    if (want) gkey(want, 1);
    walk_sent = want;
}

/* whether our 3D play is on: our display, the game screen, the view live */
static int world_on(const UwiState *s) { return s->display_ours && s->screen == UWI_PS_GAME && s->full_live; }
static int centred_on(const UwiState *s) { return s->screen == UWI_PS_MAP || s->screen == UWI_PS_CONV || s->screen == UWI_PS_TEXT; }
static int in_key_table(int c)   /* a key some world command has now */
{
    bind_make();
    for (int i = 0; i < NCMD; i++) if (bind[i].key == c && CMD[i].ctx == W && CMD[i].action != UWI_FIXED) return 1;
    return 0;
}
static int shipped_key(int c, int ctx, int shift)   /* some command's key as shipped, in ctx, with that modifier */
{
    for (int i = 0; i < NCMD; i++)
        if (CMD[i].key == c && CMD[i].shift == shift && CMD[i].ctx == ctx && CMD[i].label[0] != '#') return 1;
    return 0;
}
/* a held press its owner may keep in this state (only WORLD and GAME presses are taken back); c the key (-1 a button).
   A GAME press of a key in the key table (W, say, pressed over a conversation) is let go once the world is on again
   (back in PS_GAME): the key is the world's there */
static int keeps(int owner, const UwiState *s, int c)
{
    if (owner == UWI_WORLD) return s->screen == UWI_PS_GAME && !s->menu && !s->settings;
    if (owner == UWI_GAME) return !s->menu && !s->settings && !(c >= 0 && world_on(s) && in_key_table(c));
    return 1;
}

/* a held key let go through its owner; afterwards it has none (its physical release then sends nothing) */
static void release_key(int c)
{
    int owner = kp[c].owner;
    if (owner == UWI_NONE) return;
    kp[c].owner = UWI_NONE;
    switch (owner) {
    case UWI_GAME: gkey(kp[c].code ? kp[c].code : c, 0); break;   /* (code: a map or conversation key's game key) */
    case UWI_WORLD:
        switch (kp[c].action) {
        case UWI_HOLD: gkey(kp[c].code, 0); break;
        case UWI_OURS: if (O.our_action) O.our_action(kp[c].code, 0); break;
        case UWI_WALK: w_held &= ~kp[c].code; walk_update(); break;
        case UWI_RUN: run_held &= ~kp[c].code; walk_update(); break;
        case UWI_COMBO:
            gkey(kp[c].code, 0);
            if (kp[c].mods & UWI_MOD_ALT) gkey(0x38, 0);
            if (kp[c].mods & UWI_MOD_CTRL) gkey(0x1D, 0);
            break;
        case UWI_BTN: handler(UWI_WORLD, UWI_BUTTON, kp[c].code, 0, last_x, last_y); break;
        default: break;   /* UWI_TAP: its up went with its down */
        }
        break;
    default: handler(owner, UWI_KEY, c, 0, 0, 0); break;
    }
}
static void release_button(int i)
{
    int owner = bown[i];
    unsigned b = 1u << i;
    if (owner == UWI_NONE) return;
    bown[i] = UWI_NONE;
    if (owner == UWI_GAME) gptr(UWI_PTR_UP, last_x, last_y, b);
    else handler(owner, UWI_BUTTON, (int)b, 0, last_x, last_y);
}

/* starts with no key or button held, and the sinks (game, handlers) to send to */
void uwi_init(const UwiOut *out)
{
    memset(&O, 0, sizeof O);
    if (out) O = *out;
    memset(kp, 0, sizeof kp);
    memset(bown, 0, sizeof bown);
    last_x = last_y = 0;
    w_held = run_held = walk_sent = 0;
    shift_phys = 0;
    bind_make();
}

/* lets go of every held press that the state s no longer allows */
void uwi_state(const UwiState *s)
{
    for (int c = 0; c < NKEYS; c++)
        if (kp[c].owner != UWI_NONE && !keeps(kp[c].owner, s, c)) release_key(c);
    for (int i = 0; i < 3; i++)
        if (bown[i] != UWI_NONE && !keeps(bown[i], s, -1)) release_button(i);
}

/* lets go of everything held (focus lost, screen change) */
void uwi_release_all(void)
{
    for (int c = 0; c < NKEYS; c++) release_key(c);
    for (int i = 0; i < 3; i++) release_button(i);
    w_held = run_held = walk_sent = 0;
    shift_phys = 0;
}

/* whether mouse-look should be on in this state */
int uwi_look_wanted(const UwiState *s)
{
    return s->display_ours && s->screen == UWI_PS_GAME && !s->settings && !s->menu && !s->panel &&
           !s->options_panel && s->focused && s->mouse_look_enh && !s->replaying;
}

/* who owns the held key code (UWI_NONE when none) */
int uwi_owner_of_key(int code) { return code >= 0 && code < NKEYS ? kp[code].owner : UWI_NONE; }

/* the modifier the keyboard has now, as the bindings name it: Ctrl+Alt, Ctrl, Shift or none */
static int mod_now(const UwiState *s)
{
    if (s->ctrl_down && s->alt_down) return SH_CTRLALT;
    if (s->ctrl_down) return SH_CTRL;
    if (shift_phys) return SH_SHIFT;
    return SH_ANY;
}
/* the command in ctx whose key matches c now: one bound with the modifier held first, then the key alone (as before:
   Ctrl+W walks when nothing has Ctrl+W); -1 none; -2 the key with this modifier was a command's as shipped and has
   none now (it must do nothing, never the game's own meaning: a rune key given another key) */
static int find_key(const UwiState *s, int c, int ctx)
{
    int m = mod_now(s);
    bind_make();
    if (m != SH_ANY) {
        for (int i = 0; i < NCMD; i++)
            if (bind[i].key == c && bind[i].shift == m && CMD[i].ctx == ctx && CMD[i].action != UWI_FIXED && CMD[i].label[0] != '#') return i;
        if (m == SH_CTRLALT && shipped_key(c, ctx, m)) return -2;
    }
    for (int i = 0; i < NCMD; i++)
        if (bind[i].key == c && bind[i].shift == SH_ANY && CMD[i].ctx == ctx && CMD[i].action != UWI_FIXED && CMD[i].label[0] != '#') return i;
    return -1;
}
/* command e's press of key c: what it sends, and what its release must undo (kp[c]) */
static void cmd_press(const UwiState *s, int c, int e, int owner)
{
    kp[c].owner = (unsigned char)owner, kp[c].action = (unsigned char)CMD[e].action, kp[c].code = (unsigned short)CMD[e].code;
    kp[c].mods = 0;
    switch (CMD[e].action) {
    case UWI_HOLD: gkey(CMD[e].code, 1); break;
    case UWI_TAP: gkey(CMD[e].code, 1), gkey(CMD[e].code, 0); break;
    case UWI_OURS: if (O.our_action) O.our_action(CMD[e].code, 1); break;
    case UWI_WALK: w_held |= CMD[e].code; walk_update(); break;
    case UWI_RUN: run_held |= CMD[e].code; walk_update(); break;
    case UWI_COMBO: {   /* the Ctrl / Alt the game needs that the player does not hold (held: as it always was) */
        int add = 0;
        if ((CMD[e].mods & UWI_MOD_CTRL) && !s->ctrl_down) add |= UWI_MOD_CTRL;
        if ((CMD[e].mods & UWI_MOD_ALT) && !s->alt_down) add |= UWI_MOD_ALT;
        kp[c].mods = (unsigned char)add;
        if (add & UWI_MOD_CTRL) gkey(0x1D, 1);
        if (add & UWI_MOD_ALT) gkey(0x38, 1);
        gkey(CMD[e].code, 1);
        break;
    }
    case UWI_BTN: handler(UWI_WORLD, UWI_BUTTON, CMD[e].code, 1, last_x, last_y); break;
    }
}

/* an anywhere-command (ctx ANY) bound to c with the modifier held (or alone), -1 none */
static int any_key(const UwiState *s, int c) { return find_key(s, c, ANY); }
/* c is the settings screen's key now (F11 unless the player gave it another) */
static int settings_key_is(const UwiState *s, int c)
{
    int e = any_key(s, c);
    return e >= 0 && CMD[e].action == UWI_OURS && CMD[e].code == 0;
}
/* a key press, repeat or release: gives a new press to the first owner that applies (see the head comment) */
void uwi_key(const UwiState *s, int c, int down, int repeat)
{
    int e0;
    if (c < 0 || c >= NKEYS) return;
    uwi_state(s);
    if ((c == 0x2A || c == 0x36) && !repeat) shift_phys = down ? shift_phys | (c == 0x2A ? 1 : 2) : shift_phys & ~(c == 0x2A ? 1 : 2);
    if (!down) { release_key(c); return; }   /* to its press's owner; none: nothing */
    if (repeat) {   /* to a handler that owns the press, as down=2; a GAME or WORLD press's repeats (the walk's too,
                       as the old our_keys swallowed them) go nowhere: the game's keyqueue repeats a held code itself */
        int o = kp[c].owner;
        if (o == UWI_SETTINGS || o == UWI_MENU || o == UWI_PANEL || o == UWI_CENTRED) handler(o, UWI_KEY, c, 2, 0, 0);
        return;
    }
    if (kp[c].owner != UWI_NONE) release_key(c);   /* a press with no release seen: the old one ends first */
    int sk = settings_key_is(s, c);
    if (s->settings) { kp[c].owner = UWI_SETTINGS; handler(UWI_SETTINGS, UWI_KEY, sk ? 0x57 : c == 0x57 ? 0x1FF : c, 1, 0, 0); return; }
    if (s->menu) { kp[c].owner = UWI_MENU; handler(UWI_MENU, UWI_KEY, c, 1, 0, 0); return; }
    /* the three keys of ours outside the key table: F11 opens the settings
       screen (its handler), Esc our menu where it can (its handler opens it), Tab our panel or the original's
       interface; each owned like any press, so its release (and Esc's repeats) go to the same owner */
    if (sk) { kp[c].owner = UWI_SETTINGS; handler(UWI_SETTINGS, UWI_KEY, 0x57, 1, 0, 0); return; }   /* (its handler's key: F11's) */
    if (c == 0x01 && s->esc_menu) { kp[c].owner = UWI_MENU; handler(UWI_MENU, UWI_KEY, c, 1, 0, 0); return; }
    if (s->tab_ours && (e0 = any_key(s, c)) >= 0 && CMD[e0].code == UWI_A_TAB) { cmd_press(s, c, e0, UWI_WORLD); return; }
    /* ` in our display: nothing (mouse-look is the want's there: the game's toggle would print "Mouse look disabled."
       and be undone at its next poll); no owner, so its release and repeats send nothing either */
    if (c == 0x29 && s->look_ours) return;
    if (s->panel && handler(UWI_PANEL, UWI_KEY, c, 1, 0, 0)) { kp[c].owner = UWI_PANEL; return; }
    /* our display reaches the original's interface only through the Esc menu's Display row, so the
       original's own keys for its options panel (PLAYER.C: F1 deal_with_icons(5); Ctrl+S/R/M/F/D/Q
       do_option_shortcut) go to ours on every screen of it: Ctrl+S our save page, Ctrl+R our load page, the rest our
       menu. Never to the game here, so its panel cannot open */
    if (s->display_ours && (e0 = any_key(s, c)) >= 0 && CMD[e0].code != UWI_A_TAB && CMD[e0].code) {
        cmd_press(s, c, e0, UWI_WORLD);   /* (the Esc menu's second key, the original's Ctrl+S/R/M/F/D/Q: ours) */
        return;
    }
    int e = -1;
    if (world_on(s) && (e = find_key(s, c, W)) >= 0) { cmd_press(s, c, e, UWI_WORLD); return; }
    /* a key that was some command's as shipped and has none now (the player gave that command another key, or this
       key's command went to another): nothing, never the game's own meaning of it. No owner: its release is nothing */
    if (world_on(s) && (e == -2 || (shipped_key(c, W, SH_ANY) && !in_key_table(c)))) return;
    /* the map's and a conversation's keys in our display: the bound key sends the game's own (owned as the game's) */
    {
        int cx = !s->display_ours ? -1 : s->screen == UWI_PS_MAP ? MAP : s->screen == UWI_PS_CONV ? CONV : -1;
        if (cx >= 0) {
            int ce = find_key(s, c, cx);
            if (ce >= 0) { kp[c].owner = UWI_GAME, kp[c].code = (unsigned short)CMD[ce].code; gkey(CMD[ce].code, 1); return; }
            if (ce == -2 || shipped_key(c, cx, mod_now(s))) {   /* given away: nothing (its own key's meaning gone) */
                int still = 0;
                for (int i = 0; i < NCMD; i++) if (bind[i].key == c && CMD[i].ctx == cx) still = 1;
                if (!still) return;
            }
        }
    }
    if (centred_on(s) && handler(UWI_CENTRED, UWI_KEY, c, 1, 0, 0)) { kp[c].owner = UWI_CENTRED; return; }
    kp[c].owner = UWI_GAME, kp[c].code = 0;
    gkey(c, 1);
}

static int button_index(unsigned b) { return b == 1 ? 0 : b == 2 ? 1 : b == 4 ? 2 : -1; }

/* a mouse button press or release (b: 1 left, 2 right, 4 middle) at window point x, y */
void uwi_button(const UwiState *s, unsigned b, int down, int x, int y)
{
    int i = button_index(b);
    if (i < 0) return;
    uwi_state(s);
    last_x = x, last_y = y;
    if (!down) { release_button(i); return; }
    if (bown[i] != UWI_NONE) release_button(i);
    int owner;
    if (s->settings) owner = UWI_SETTINGS, handler(owner, UWI_BUTTON, (int)b, 1, x, y);
    else if (s->menu) owner = UWI_MENU, handler(owner, UWI_BUTTON, (int)b, 1, x, y);
    else if (s->panel && handler(UWI_PANEL, UWI_BUTTON, (int)b, 1, x, y)) owner = UWI_PANEL;
    else if (world_on(s) && handler(UWI_WORLD, UWI_BUTTON, (int)b, 1, x, y)) owner = UWI_WORLD;
    else if (centred_on(s) && handler(UWI_CENTRED, UWI_BUTTON, (int)b, 1, x, y)) owner = UWI_CENTRED;
    else owner = UWI_GAME, gptr(UWI_PTR_DOWN, x, y, b);
    bown[i] = (unsigned char)owner;
}

/* a move: to the owner of a held button (a drag), else to the first owner that takes it */
void uwi_move(const UwiState *s, int x, int y)
{
    uwi_state(s);
    last_x = x, last_y = y;
    for (int i = 0; i < 3; i++)
        if (bown[i] != UWI_NONE) {
            if (bown[i] == UWI_GAME) gptr(UWI_PTR_MOVE, x, y, 0);
            else handler(bown[i], UWI_MOVE, 0, 0, x, y);
            return;
        }
    if (s->settings) { handler(UWI_SETTINGS, UWI_MOVE, 0, 0, x, y); return; }
    if (s->menu) { handler(UWI_MENU, UWI_MOVE, 0, 0, x, y); return; }
    if (s->panel && handler(UWI_PANEL, UWI_MOVE, 0, 0, x, y)) return;
    if (world_on(s) && handler(UWI_WORLD, UWI_MOVE, 0, 0, x, y)) return;
    if (centred_on(s) && handler(UWI_CENTRED, UWI_MOVE, 0, 0, x, y)) return;
    gptr(UWI_PTR_MOVE, x, y, 0);
}
