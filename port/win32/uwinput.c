/* uwinput: who owns each press of the window's keys and mouse buttons, and the key table of our display
   (docs\superpowers\specs\2026-11-12-input-rewrite-design.md; uwinput.h). Pure C: no Win32 here.

   A new press goes to the first owner that applies, in this order: SETTINGS (the settings screen is open), MENU
   (our Esc menu), then three keys outside the key table (F11: SETTINGS, whose handler opens the screen; Esc where
   our menu can open, esc_menu: MENU, whose handler opens it; Tab where it is ours, tab_ours: the WORLD's
   UWI_A_TAB), PANEL (our inventory panel, when its handler takes the event), WORLD (our display's 3D play:
   the key table below, or the world's handler for a button), CENTRED (a conversation, the map, typed text: the
   original's piece centred, when its handler takes it), else GAME (the byte or pointer event to the game as it
   comes). The owner and what was sent are kept per physical key and per button, and the release goes to the same
   owner whatever has changed since; a held WORLD or GAME press the new state no longer allows (the screen left the
   3D view; our menu or the settings opened) is released at once (uwi_state) and its later release sends nothing.
   Before, a release after the screen changed went to the game as the key's own code: a translated key stayed down
   and the keyboard's repeat sent it again (Z slept, dreamed, slept again), Space left the port's Shift held. */
#include <string.h>
#include "uwinput.h"

#define NKEYS 0x200
enum { SH_ANY = 0, SH_SHIFT = 1, SH_CTRL = 2 };   /* an entry's modifier: none needed, Shift, Ctrl */

/* our display's controls: exactly the old our_keys (plat_win32.c b9ba607). Entries with a modifier come before the
   key's plain entry; a key not here goes on to CENTRED / GAME (O alone, S, A, D, E, the digits, ...) */
static const struct { int key; int shift; int action; int code; } keys[] = {
    { 0x2A,  SH_ANY,   UWI_RUN,  0 },                 /* Shift (left): ours, the run */
    { 0x36,  SH_ANY,   UWI_RUN,  0 },                 /* Shift (right) */
    { 0x11,  SH_ANY,   UWI_WALK, 0x2D },              /* W: walk (the port's X), with Shift run (W, 0x11) */
    { 0x148, SH_ANY,   UWI_WALK, 0x2D },              /* Up: the same */
    { 0x150, SH_ANY,   UWI_HOLD, 0x1F },              /* Down: back (S) */
    { 0x149, SH_ANY,   UWI_HOLD, 0x148 },             /* PageUp: look up */
    { 0x151, SH_ANY,   UWI_HOLD, 0x150 },             /* PageDown: look down */
    { 0x39,  SH_ANY,   UWI_HOLD, 0x2A },              /* Space: jump (the port's Shift) */
    { 0x26,  SH_CTRL,  UWI_OURS, UWI_A_AI_LOG },      /* Ctrl+L: the AI log on / off */
    { 0x26,  SH_SHIFT, UWI_OURS, UWI_A_FULLBRIGHT },  /* Shift+L: full bright */
    { 0x26,  SH_ANY,   UWI_HOLD, 0x10 },              /* L: look (the port's Q) */
    { 0x10,  SH_ANY,   UWI_HOLD, 0x42 },              /* Q: cast (F8, PLAYER.C's try_cast) */
    { 0x2C,  SH_ANY,   UWI_HOLD, 0x44 },              /* Z: sleep (F10, player_key_sleep) */
    { 0x25,  SH_ANY,   UWI_HOLD, 0x43 },              /* K: track (F9, player_use_skill 2) */
    { 0x3F,  SH_ANY,   UWI_OURS, UWI_A_SAVE_PAGE },   /* F5: our save page */
    { 0x43,  SH_ANY,   UWI_OURS, UWI_A_LOAD_PAGE },   /* F9: our load page */
    { 0x19,  SH_SHIFT, UWI_OURS, UWI_A_RECORD },      /* Shift+P: 15 s of video */
    { 0x19,  SH_ANY,   UWI_OURS, UWI_A_SHOT },        /* P: a screenshot to the clipboard */
    { 0x34,  SH_ANY,   UWI_OURS, UWI_A_SHADOWS },     /* .: the objects' shadows on / off */
    { 0x18,  SH_SHIFT, UWI_OURS, UWI_A_SHADOWS },     /* Shift+O: the same (O alone the game's) */
    { 0x2E,  SH_ANY,   UWI_OURS, UWI_A_FIGHT },       /* C: fight mode, the original's F5 (the fight icon) */
    { 0x17,  SH_ANY,   UWI_OURS, UWI_A_PANEL },       /* I: our inventory panel */
    { 0x13,  SH_ANY,   UWI_OURS, UWI_A_RUNES },       /* R: our panel on our rune page */
    { 0x33,  SH_ANY,   UWI_OURS, UWI_A_CHARACTER },   /* ,: our panel, its character sheet */
    { 0x32,  SH_ANY,   UWI_HOLD, 0x2C },              /* M: the map (the port's Z) */
    { 0x28,  SH_ANY,   UWI_OURS, UWI_A_DLSS5 },       /* ': DLSS 5 */
    { 0x27,  SH_ANY,   UWI_OURS, UWI_A_RT },          /* ;: ray tracing */
    { 0x1B,  SH_ANY,   UWI_OURS, UWI_A_STYLE },       /* ]: DLSS 5's style */
    { 0x40,  SH_ANY,   UWI_OURS, UWI_A_NORMALS },     /* F6: the normal maps */
};

static UwiOut O;
static struct { unsigned char owner, action; unsigned short code; } kp[NKEYS];   /* per physical key */
static unsigned char bown[3];                                                    /* per button: its owner */
static int last_x, last_y;
static int w_held, run_held, walk_sent;   /* w_held: bit 0 W, bit 1 Up; run_held: bit 0 left, bit 1 right Shift */

static void gkey(int code, int down) { if (O.game_key) O.game_key(code, down); }
static void gptr(int type, int x, int y, unsigned b) { if (O.game_pointer) O.game_pointer(type, (float)x, (float)y, b); }
static int handler(int owner, int kind, int code, int down, int x, int y)
{
    int (*h)(int, int, int, int, int) =
        owner == UWI_SETTINGS ? O.settings_event : owner == UWI_MENU ? O.menu_event : owner == UWI_PANEL ? O.panel_event :
        owner == UWI_WORLD ? O.world_event : owner == UWI_CENTRED ? O.centred_event : 0;
    return h ? h(kind, code, down, x, y) : 0;
}

/* the forward code the game should see held: W or Up with Shift is its run (W), without its walk (X); it follows
   Shift going down or up while forward is held (as the old walk_update) */
static void walk_update(void)
{
    int want = w_held ? (run_held ? 0x11 : 0x2D) : 0;
    if (want == walk_sent) return;
    if (walk_sent) gkey(walk_sent, 0);
    if (want) gkey(want, 1);
    walk_sent = want;
}

static int world_on(const UwiState *s) { return s->display_ours && s->screen == UWI_PS_GAME && s->full_live; }
static int centred_on(const UwiState *s) { return s->screen == UWI_PS_MAP || s->screen == UWI_PS_CONV || s->screen == UWI_PS_TEXT; }
static int in_key_table(int c)
{
    for (unsigned i = 0; i < sizeof keys / sizeof keys[0]; i++) if (keys[i].key == c) return 1;
    return 0;
}
/* a held press its owner may keep in this state (only WORLD and GAME presses are taken back); c the key (-1 a button).
   A GAME press of a key in the key table (W, say, pressed over a conversation) is let go once the world is on again
   (back in PS_GAME): the key is the world's there, the spec's owner change (the final review's finding 5) */
static int keeps(int owner, const UwiState *s, int c)
{
    if (owner == UWI_WORLD) return s->screen == UWI_PS_GAME && !s->menu && !s->settings;
    if (owner == UWI_GAME) return !s->menu && !s->settings && !(c >= 0 && world_on(s) && in_key_table(c));
    return 1;
}

static int bit_of(int code) { return code == 0x11 ? 1 : code == 0x148 ? 2 : code == 0x2A ? 1 : code == 0x36 ? 2 : 0; }

/* a held key let go through its owner; afterwards it has none (its physical release then sends nothing) */
static void release_key(int c)
{
    int owner = kp[c].owner;
    if (owner == UWI_NONE) return;
    kp[c].owner = UWI_NONE;
    switch (owner) {
    case UWI_GAME: gkey(c, 0); break;
    case UWI_WORLD:
        switch (kp[c].action) {
        case UWI_HOLD: gkey(kp[c].code, 0); break;
        case UWI_OURS: if (O.our_action) O.our_action(kp[c].code, 0); break;
        case UWI_WALK: w_held &= ~bit_of(c); walk_update(); break;
        case UWI_RUN: run_held &= ~bit_of(c); walk_update(); break;
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

void uwi_init(const UwiOut *out)
{
    memset(&O, 0, sizeof O);
    if (out) O = *out;
    memset(kp, 0, sizeof kp);
    memset(bown, 0, sizeof bown);
    last_x = last_y = 0;
    w_held = run_held = walk_sent = 0;
}

void uwi_state(const UwiState *s)
{
    for (int c = 0; c < NKEYS; c++)
        if (kp[c].owner != UWI_NONE && !keeps(kp[c].owner, s, c)) release_key(c);
    for (int i = 0; i < 3; i++)
        if (bown[i] != UWI_NONE && !keeps(bown[i], s, -1)) release_button(i);
}

void uwi_release_all(void)
{
    for (int c = 0; c < NKEYS; c++) release_key(c);
    for (int i = 0; i < 3; i++) release_button(i);
    w_held = run_held = walk_sent = 0;
}

int uwi_look_wanted(const UwiState *s)
{
    return s->display_ours && s->screen == UWI_PS_GAME && !s->settings && !s->menu && !s->panel &&
           !s->options_panel && s->focused && s->mouse_look_enh && !s->replaying;
}

int uwi_owner_of_key(int code) { return code >= 0 && code < NKEYS ? kp[code].owner : UWI_NONE; }

static int find_key(const UwiState *s, int c)
{
    int shift = run_held != 0, ctrl = s->ctrl_down != 0;   /* Shift: ours only (the old shift_held); Ctrl: the live keyboard's */
    for (unsigned i = 0; i < sizeof keys / sizeof keys[0]; i++) {
        if (keys[i].key != c) continue;
        if (keys[i].shift == SH_CTRL && !ctrl) continue;
        if (keys[i].shift == SH_SHIFT && !shift) continue;
        return (int)i;
    }
    return -1;
}

void uwi_key(const UwiState *s, int c, int down, int repeat)
{
    if (c < 0 || c >= NKEYS) return;
    uwi_state(s);
    if (!down) { release_key(c); return; }   /* to its press's owner; none: nothing */
    if (repeat) {   /* to a handler that owns the press, as down=2; a GAME or WORLD press's repeats (the walk's too,
                       as the old our_keys swallowed them) go nowhere: the game's keyqueue repeats a held code itself */
        int o = kp[c].owner;
        if (o == UWI_SETTINGS || o == UWI_MENU || o == UWI_PANEL || o == UWI_CENTRED) handler(o, UWI_KEY, c, 2, 0, 0);
        return;
    }
    if (kp[c].owner != UWI_NONE) release_key(c);   /* a press with no release seen: the old one ends first */
    if (s->settings) { kp[c].owner = UWI_SETTINGS; handler(UWI_SETTINGS, UWI_KEY, c, 1, 0, 0); return; }
    if (s->menu) { kp[c].owner = UWI_MENU; handler(UWI_MENU, UWI_KEY, c, 1, 0, 0); return; }
    /* the three keys of ours outside the key table (the old handle_key and menu_key had them): F11 opens the settings
       screen (its handler), Esc our menu where it can (its handler opens it), Tab our panel or the original's
       interface; each owned like any press, so its release (and Esc's repeats) go to the same owner */
    if (c == 0x57) { kp[c].owner = UWI_SETTINGS; handler(UWI_SETTINGS, UWI_KEY, c, 1, 0, 0); return; }
    if (c == 0x01 && s->esc_menu) { kp[c].owner = UWI_MENU; handler(UWI_MENU, UWI_KEY, c, 1, 0, 0); return; }
    if (c == 0x0F && s->tab_ours) {
        kp[c].owner = UWI_WORLD, kp[c].action = UWI_OURS, kp[c].code = UWI_A_TAB;
        if (O.our_action) O.our_action(UWI_A_TAB, 1);
        return;
    }
    /* ` in our display: nothing (mouse-look is the want's there: the game's toggle would print "Mouse look disabled."
       and be undone at its next poll); no owner, so its release and repeats send nothing either */
    if (c == 0x29 && s->look_ours) return;
    if (s->panel && handler(UWI_PANEL, UWI_KEY, c, 1, 0, 0)) { kp[c].owner = UWI_PANEL; return; }
    /* our display has no way to the original's interface but the Esc menu's Display row (the user, 2026-11-15): the
       original's own keys for its options panel (PLAYER.C: F1 deal_with_icons(5); Ctrl+S/R/M/F/D/Q
       do_option_shortcut) go to ours on every screen of it: Ctrl+S our save page, Ctrl+R our load page, the rest our
       menu. Never to the game here, so its panel cannot open */
    if (s->display_ours) {
        int a = -1;
        if (c == 0x3B) a = UWI_A_MENU;
        else if (s->ctrl_down)
            a = c == 0x1F ? UWI_A_SAVE_PAGE : c == 0x13 ? UWI_A_LOAD_PAGE : (c == 0x32 || c == 0x21 || c == 0x20 || c == 0x10) ? UWI_A_MENU : -1;
        if (a >= 0) {
            kp[c].owner = UWI_WORLD, kp[c].action = UWI_OURS, kp[c].code = (unsigned short)a;
            if (O.our_action) O.our_action(a, 1);
            return;
        }
    }
    int e;
    if (world_on(s) && (e = find_key(s, c)) >= 0) {
        kp[c].owner = UWI_WORLD, kp[c].action = (unsigned char)keys[e].action, kp[c].code = (unsigned short)keys[e].code;
        switch (keys[e].action) {
        case UWI_HOLD: gkey(keys[e].code, 1); break;
        case UWI_TAP: gkey(keys[e].code, 1), gkey(keys[e].code, 0); break;
        case UWI_OURS: if (O.our_action) O.our_action(keys[e].code, 1); break;
        case UWI_WALK: w_held |= bit_of(c); walk_update(); break;
        case UWI_RUN: run_held |= bit_of(c); walk_update(); break;
        }
        return;
    }
    if (centred_on(s) && handler(UWI_CENTRED, UWI_KEY, c, 1, 0, 0)) { kp[c].owner = UWI_CENTRED; return; }
    kp[c].owner = UWI_GAME;
    gkey(c, 1);
}

static int button_index(unsigned b) { return b == 1 ? 0 : b == 2 ? 1 : b == 4 ? 2 : -1; }

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
