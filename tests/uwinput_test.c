/* uwinput_test: port\win32\uwinput.c alone with a fake backend recording every output.
   Prints "uwinput_test: N passed, M failed"; the exit code is M. Run by tools\verify.cmd. */
#include <stdio.h>
#include <string.h>
#include "uwinput.h"

typedef struct { char fn; int a, b, c; } Call;   /* fn: K game_key(a code, b down), P game_pointer(a type, b button),
                                                    S/M/N/W/C settings/menu/panel/world/centred (a kind, b code, c down),
                                                    O our_action(a action, b down) */
static Call calls[256];
static int ncalls;
static int take_settings = 1, take_menu = 1, take_panel = 1, take_world = 1, take_centred = 1;

static void rec(char fn, int a, int b, int c)
{
    if (ncalls < (int)(sizeof calls / sizeof calls[0])) calls[ncalls].fn = fn, calls[ncalls].a = a, calls[ncalls].b = b, calls[ncalls].c = c;
    ncalls++;
}
static void f_key(int code, int down) { rec('K', code, down, 0); }
static void f_ptr(int type, float x, float y, unsigned button) { (void)x, (void)y; rec('P', type, (int)button, 0); }
static int f_set(int k, int code, int down, int x, int y) { rec('S', k, code, down); return take_settings; }
static int f_menu(int k, int code, int down, int x, int y) { rec('M', k, code, down); return take_menu; }
static int f_panel(int k, int code, int down, int x, int y) { rec('N', k, code, down); return take_panel; }
static int f_world(int k, int code, int down, int x, int y) { rec('W', k, code, down); return take_world; }
static int f_cent(int k, int code, int down, int x, int y) { rec('C', k, code, down); return take_centred; }
static void f_ours(int action, int down) { rec('O', action, down, 0); }
static const UwiOut out = { f_key, f_ptr, f_set, f_menu, f_panel, f_world, f_cent, f_ours };

static int failed_checks;
#define CHECK(cond) do { if (!(cond)) { printf("  %s:%d: %s\n", __func__, __LINE__, #cond); failed_checks++; } } while (0)

/* a fresh module and log; the state: playing in our display */
static UwiState play(void)
{
    UwiState s;
    memset(&s, 0, sizeof s);
    s.screen = UWI_PS_GAME, s.focused = 1, s.display_ours = 1, s.full_live = 1, s.mouse_look_enh = 1;
    return s;
}
static void reset(void)
{
    uwi_init(&out);
    ncalls = 0;
    take_settings = take_menu = take_panel = take_world = take_centred = 1;
}
static int count(char fn, int a, int b) { int n = 0; for (int i = 0; i < ncalls; i++) n += calls[i].fn == fn && calls[i].a == a && calls[i].b == b; return n; }
static int count_fn(char fn) { int n = 0; for (int i = 0; i < ncalls; i++) n += calls[i].fn == fn; return n; }
static int is(int i, char fn, int a, int b) { return i < ncalls && calls[i].fn == fn && calls[i].a == a && calls[i].b == b; }

static void z_into_cutscene(void)
{
    reset();
    UwiState s = play();
    uwi_key(&s, 0x2C, 1, 0);
    CHECK(ncalls == 1 && is(0, 'K', 0x44, 1));
    s.screen = UWI_PS_CUT_FULL;
    uwi_state(&s);
    uwi_key(&s, 0x2C, 0, 0);
    CHECK(count('K', 0x44, 0) == 1);
    CHECK(count('K', 0x2C, 0) == 0 && count('K', 0x2C, 1) == 0);
}

static void space_into_menu(void)
{
    reset();
    UwiState s = play();
    uwi_key(&s, 0x39, 1, 0);
    CHECK(ncalls == 1 && is(0, 'K', 0x2A, 1));
    s.menu = 1, s.full_live = 0;
    uwi_state(&s);
    CHECK(ncalls == 2 && is(1, 'K', 0x2A, 0));
    uwi_key(&s, 0x39, 0, 0);
    CHECK(ncalls == 2);
}

static void menu_press_play_release(void)
{
    reset();
    UwiState s = play();
    s.menu = 1, s.full_live = 0;
    uwi_key(&s, 0x2C, 1, 0);
    CHECK(count_fn('M') == 1 && count_fn('K') == 0);
    s.menu = 0, s.full_live = 1;
    uwi_state(&s);
    uwi_key(&s, 0x2C, 0, 0);
    CHECK(count_fn('K') == 0);
}

static void two_keys_one_code(void)
{
    reset();
    UwiState s = play();
    uwi_key(&s, 0x11, 1, 0);
    CHECK(ncalls == 1 && is(0, 'K', 0x2D, 1));
    uwi_key(&s, 0x148, 1, 0);
    CHECK(ncalls == 1);
    uwi_key(&s, 0x11, 0, 0);
    CHECK(ncalls == 1);
    uwi_key(&s, 0x148, 0, 0);
    CHECK(ncalls == 2 && is(1, 'K', 0x2D, 0));
}

static void shift_run(void)
{
    reset();
    UwiState s = play();
    uwi_key(&s, 0x11, 1, 0);
    CHECK(ncalls == 1 && is(0, 'K', 0x2D, 1));
    uwi_key(&s, 0x2A, 1, 0);
    CHECK(ncalls == 3 && is(1, 'K', 0x2D, 0) && is(2, 'K', 0x11, 1));
    uwi_key(&s, 0x2A, 0, 0);
    CHECK(ncalls == 5 && is(3, 'K', 0x11, 0) && is(4, 'K', 0x2D, 1));
}

static void button_release_follows_press_owner(void)
{
    reset();
    UwiState s = play();
    s.panel = 1;
    uwi_button(&s, 1, 1, 10, 10);
    CHECK(ncalls == 1 && calls[0].fn == 'N' && calls[0].a == UWI_BUTTON && calls[0].b == 1 && calls[0].c == 1);
    s.panel = 0;
    uwi_state(&s);
    uwi_button(&s, 1, 0, 10, 10);
    CHECK(ncalls == 2 && calls[1].fn == 'N' && calls[1].a == UWI_BUTTON && calls[1].b == 1 && calls[1].c == 0);
    CHECK(count_fn('P') == 0 && count_fn('W') == 0);
}

static void focus_lost_releases_all(void)
{
    reset();
    UwiState s = play();
    uwi_key(&s, 0x2C, 1, 0);
    uwi_key(&s, 0x39, 1, 0);
    uwi_button(&s, 2, 1, 5, 5);
    CHECK(ncalls == 3 && is(0, 'K', 0x44, 1) && is(1, 'K', 0x2A, 1) && calls[2].fn == 'W' && calls[2].c == 1);
    uwi_release_all();
    CHECK(ncalls == 6 && is(3, 'K', 0x44, 0) && is(4, 'K', 0x2A, 0));
    CHECK(calls[5].fn == 'W' && calls[5].a == UWI_BUTTON && calls[5].b == 2 && calls[5].c == 0);
    uwi_key(&s, 0x2C, 0, 0);
    uwi_key(&s, 0x39, 0, 0);
    uwi_button(&s, 2, 0, 5, 5);
    CHECK(ncalls == 6);
}

static void repeat_only_walk(void)
{
    reset();
    UwiState s = play();
    uwi_key(&s, 0x11, 1, 0);
    CHECK(ncalls == 1 && is(0, 'K', 0x2D, 1));
    uwi_key(&s, 0x11, 1, 1);                 /* the walk's repeat: nothing (the game's keyqueue repeats it) */
    CHECK(ncalls == 1);
    uwi_key(&s, 0x2C, 1, 0);
    CHECK(ncalls == 2 && is(1, 'K', 0x44, 1));
    uwi_key(&s, 0x2C, 1, 1);
    CHECK(ncalls == 2);
    uwi_key(&s, 0x1E, 1, 0);                 /* A: the game's; its repeat too goes nowhere */
    uwi_key(&s, 0x1E, 1, 1);
    CHECK(ncalls == 3 && is(2, 'K', 0x1E, 1));
}

static void menu_repeat(void)
{
    reset();
    UwiState s = play();
    s.menu = 1, s.full_live = 0;
    uwi_key(&s, 0x150, 1, 0);
    uwi_key(&s, 0x150, 1, 1);
    uwi_key(&s, 0x150, 1, 1);
    uwi_key(&s, 0x150, 0, 0);
    CHECK(ncalls == 4 && count_fn('K') == 0);
    CHECK(calls[0].fn == 'M' && calls[0].a == UWI_KEY && calls[0].b == 0x150 && calls[0].c == 1);
    CHECK(calls[1].fn == 'M' && calls[1].b == 0x150 && calls[1].c == 2);
    CHECK(calls[2].fn == 'M' && calls[2].b == 0x150 && calls[2].c == 2);
    CHECK(calls[3].fn == 'M' && calls[3].b == 0x150 && calls[3].c == 0);
    reset();                                 /* the settings screen the same */
    s = play(), s.settings = 1;
    uwi_key(&s, 0x148, 1, 0);
    uwi_key(&s, 0x148, 1, 1);
    CHECK(ncalls == 2 && calls[1].fn == 'S' && calls[1].b == 0x148 && calls[1].c == 2);
}

static void look_wanted_table(void)
{
    reset();
    UwiState s = play(), t;
    CHECK(uwi_look_wanted(&s) == 1);
    t = s, t.menu = 1;           CHECK(uwi_look_wanted(&t) == 0);
    t = s, t.panel = 1;          CHECK(uwi_look_wanted(&t) == 0);
    t = s, t.settings = 1;       CHECK(uwi_look_wanted(&t) == 0);
    t = s, t.options_panel = 1;  CHECK(uwi_look_wanted(&t) == 0);
    t = s, t.focused = 0;        CHECK(uwi_look_wanted(&t) == 0);
    t = s, t.screen = UWI_PS_MAP;   CHECK(uwi_look_wanted(&t) == 0);
    t = s, t.screen = UWI_PS_CONV;  CHECK(uwi_look_wanted(&t) == 0);
    t = s, t.screen = UWI_PS_TEXT;  CHECK(uwi_look_wanted(&t) == 0);
    t = s, t.screen = UWI_PS_DEATH; CHECK(uwi_look_wanted(&t) == 0);
    t = s, t.replaying = 1;      CHECK(uwi_look_wanted(&t) == 0);
    t = s, t.display_ours = 0;   CHECK(uwi_look_wanted(&t) == 0);
    t = s, t.mouse_look_enh = 0; CHECK(uwi_look_wanted(&t) == 0);
}

static void c_fight_ours(void)
{
    reset();
    UwiState s = play();
    uwi_key(&s, 0x2E, 1, 0);
    CHECK(ncalls == 1 && is(0, 'O', UWI_A_FIGHT, 1));
    uwi_key(&s, 0x2E, 0, 0);
    CHECK(count_fn('K') == 0);
}

/* every key-table entry: the first output of a press in our display's play, then its release */
static void key_table(void)
{
    static const struct { int key, shift, ctrl; char fn; int a, b; } t[] = {
        { 0x11, 0, 0, 'K', 0x2D, 1 },  { 0x148, 0, 0, 'K', 0x2D, 1 },          /* W, Up: walk */
        { 0x150, 0, 0, 'K', 0x1F, 1 }, { 0x149, 0, 0, 'K', 0x148, 1 }, { 0x151, 0, 0, 'K', 0x150, 1 },   /* Down PgUp PgDn */
        { 0x39, 0, 0, 'K', 0x2A, 1 },                                          /* Space */
        { 0x26, 0, 1, 'O', UWI_A_AI_LOG, 1 }, { 0x26, 1, 0, 'O', UWI_A_FULLBRIGHT, 1 }, { 0x26, 0, 0, 'K', 0x10, 1 },   /* Ctrl+L Shift+L L */
        { 0x10, 0, 0, 'K', 0x42, 1 }, { 0x2C, 0, 0, 'K', 0x44, 1 }, { 0x25, 0, 0, 'K', 0x43, 1 },   /* Q Z K */
        { 0x3F, 0, 0, 'O', UWI_A_SAVE_PAGE, 1 }, { 0x43, 0, 0, 'O', UWI_A_LOAD_PAGE, 1 },          /* F5 F9 */
        { 0x19, 1, 0, 'O', UWI_A_RECORD, 1 }, { 0x19, 0, 0, 'O', UWI_A_SHOT, 1 },                   /* Shift+P P */
        { 0x34, 0, 0, 'O', UWI_A_SHADOWS, 1 },                                                      /* . */
        { 0x18, 1, 0, 'O', UWI_A_SHADOWS, 1 }, { 0x18, 0, 0, 'K', 0x18, 1 },                        /* Shift+O; O: the game's */
        { 0x2E, 0, 0, 'O', UWI_A_FIGHT, 1 }, { 0x17, 0, 0, 'O', UWI_A_PANEL, 1 },                   /* C I */
        { 0x13, 0, 0, 'O', UWI_A_RUNES, 1 }, { 0x33, 0, 0, 'O', UWI_A_CHARACTER, 1 },               /* R , */
        { 0x32, 0, 0, 'K', 0x2C, 1 },                                                               /* M */
        { 0x28, 0, 0, 'O', UWI_A_DLSS5, 1 }, { 0x27, 0, 0, 'O', UWI_A_RT, 1 },                      /* ' ; */
        { 0x1B, 0, 0, 'O', UWI_A_STYLE, 1 }, { 0x40, 0, 0, 'O', UWI_A_NORMALS, 1 },                 /* ] F6 */
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        reset();
        UwiState s = play();
        s.ctrl_down = t[i].ctrl;
        if (t[i].shift) uwi_key(&s, 0x2A, 1, 0);   /* Shift: ours (no output with no walk held) */
        int n0 = ncalls;
        uwi_key(&s, t[i].key, 1, 0);
        if (!(ncalls > n0 && is(n0, t[i].fn, t[i].a, t[i].b))) {
            printf("  key_table: key %#x shift %d ctrl %d: first output %c %#x %d, expected %c %#x %d\n", t[i].key,
                   t[i].shift, t[i].ctrl, ncalls > n0 ? calls[n0].fn : '-', ncalls > n0 ? calls[n0].a : 0,
                   ncalls > n0 ? calls[n0].b : 0, t[i].fn, t[i].a, t[i].b);
            failed_checks++;
        }
        uwi_key(&s, t[i].key, 0, 0);               /* the release is the press's: a held code lets go of that code */
        if (t[i].fn == 'K') CHECK(count('K', t[i].a, 0) == 1);
        if (t[i].fn == 'O') CHECK(count('O', t[i].a, 0) == 1);
    }
    reset();                                       /* Shift for Shift+L is ours only (the old shift_held): */
    UwiState s = play();                           /* a Shift the game took (pressed outside play) does not count */
    s.screen = UWI_PS_CONV, take_centred = 0;
    uwi_key(&s, 0x2A, 1, 0);
    s.screen = UWI_PS_GAME;
    uwi_key(&s, 0x26, 1, 0);
    CHECK(count('K', 0x10, 1) == 1 && count('O', UWI_A_FULLBRIGHT, 1) == 0);
}

/* the controller's ruling 5: a fight-mode left press (the world's: its Space held, the charge) held into the death:
   the world's handler gets its release at once (it lets go of the Space), the physical release later nothing */
static void left_held_into_death(void)
{
    reset();
    UwiState s = play();
    uwi_button(&s, 1, 1, 50, 50);
    CHECK(ncalls == 1 && calls[0].fn == 'W' && calls[0].a == UWI_BUTTON && calls[0].b == 1 && calls[0].c == 1);
    s.screen = UWI_PS_DEATH, s.full_live = 0;
    uwi_state(&s);
    CHECK(ncalls == 2 && calls[1].fn == 'W' && calls[1].a == UWI_BUTTON && calls[1].b == 1 && calls[1].c == 0);
    uwi_button(&s, 1, 0, 50, 50);
    CHECK(ncalls == 2);
    reset();                                 /* the focus lost while charging: the same */
    s = play();
    uwi_button(&s, 1, 1, 50, 50);
    uwi_release_all();
    CHECK(ncalls == 2 && calls[1].fn == 'W' && calls[1].c == 0);
    uwi_button(&s, 1, 0, 50, 50);
    CHECK(ncalls == 2);
}

/* the controller's ruling 2: F11, Esc and Tab (outside the key table) owned like any press */
static void esc_f11_tab_owned(void)
{
    reset();
    UwiState s = play();
    s.esc_menu = 1, s.tab_ours = 1;
    uwi_key(&s, 0x01, 1, 0);                 /* Esc: our menu's handler opens it */
    CHECK(ncalls == 1 && calls[0].fn == 'M' && calls[0].b == 0x01 && calls[0].c == 1);
    s.menu = 1, s.full_live = 0;
    uwi_key(&s, 0x01, 1, 1);                 /* its repeat: to the menu too (the old menu_key closed it) */
    uwi_key(&s, 0x01, 0, 0);
    CHECK(ncalls == 3 && calls[1].fn == 'M' && calls[1].c == 2 && calls[2].fn == 'M' && calls[2].c == 0);
    CHECK(count_fn('K') == 0);
    reset();                                 /* Esc where our menu cannot open: the game's */
    s = play();
    uwi_key(&s, 0x01, 1, 0);
    uwi_key(&s, 0x01, 0, 0);
    CHECK(ncalls == 2 && is(0, 'K', 0x01, 1) && is(1, 'K', 0x01, 0));
    reset();                                 /* F11: the settings screen's, opened or not; its release too */
    s = play();
    uwi_key(&s, 0x57, 1, 0);
    s.settings = 1;
    uwi_key(&s, 0x57, 0, 0);
    CHECK(ncalls == 2 && calls[0].fn == 'S' && calls[0].b == 0x57 && calls[0].c == 1 && calls[1].fn == 'S' && calls[1].c == 0);
    reset();                                 /* F11 with our menu open: the menu's (the old menu_key took it) */
    s = play(), s.menu = 1;
    uwi_key(&s, 0x57, 1, 0);
    CHECK(ncalls == 1 && calls[0].fn == 'M');
    reset();                                 /* Tab: ours (on any screen while tab_ours), its release nothing to the game */
    s = play(), s.tab_ours = 1, s.screen = UWI_PS_CONV, s.full_live = 0;
    uwi_key(&s, 0x0F, 1, 0);
    uwi_key(&s, 0x0F, 1, 1);
    uwi_key(&s, 0x0F, 0, 0);
    CHECK(ncalls == 2 && is(0, 'O', UWI_A_TAB, 1) && is(1, 'O', UWI_A_TAB, 0));
    reset();                                 /* Tab not ours: the game's */
    s = play();
    uwi_key(&s, 0x0F, 1, 0);
    CHECK(ncalls == 1 && is(0, 'K', 0x0F, 1));
}

/* ` (0x29): in our display (look_ours) nothing reaches the game, press, repeat or release, in play or over a centred
   piece; in the original's display (look_ours 0) it is the game's, its toggle */
static void grave_ours_swallowed(void)
{
    reset();
    UwiState s = play();
    s.look_ours = 1;
    uwi_key(&s, 0x29, 1, 0);
    uwi_key(&s, 0x29, 1, 1);
    uwi_key(&s, 0x29, 0, 0);
    CHECK(ncalls == 0);
    s.screen = UWI_PS_CONV, s.full_live = 0;
    uwi_key(&s, 0x29, 1, 0);
    uwi_key(&s, 0x29, 0, 0);
    CHECK(ncalls == 0);
    reset();
    s = play(), s.display_ours = 0, s.full_live = 0, s.look_ours = 0;
    uwi_key(&s, 0x29, 1, 0);
    uwi_key(&s, 0x29, 0, 0);
    CHECK(ncalls == 2 && is(0, 'K', 0x29, 1) && is(1, 'K', 0x29, 0));
}

/* the final review's finding 5: a key of the key table the game took (W pressed over a conversation, its centred
   piece not taking it) is let go at once when the world is on again (PS_GAME, full play): its owner has changed;
   the physical release later sends nothing */
static void game_key_released_back_in_play(void)
{
    reset();
    UwiState s = play();
    s.screen = UWI_PS_CONV, s.full_live = 0, take_centred = 0;
    uwi_key(&s, 0x11, 1, 0);
    CHECK(ncalls == 2 && calls[0].fn == 'C' && is(1, 'K', 0x11, 1));   /* (offered to the centred piece, then the game's) */
    uwi_state(&s);                                  /* still in the conversation: kept */
    CHECK(ncalls == 2);
    s.screen = UWI_PS_GAME, s.full_live = 1;
    uwi_state(&s);
    CHECK(ncalls == 3 && is(2, 'K', 0x11, 0));
    uwi_key(&s, 0x11, 0, 0);
    CHECK(ncalls == 3);
    reset();                                        /* a key not in the key table (S) stays the game's */
    s = play(), s.screen = UWI_PS_CONV, s.full_live = 0, take_centred = 0;
    uwi_key(&s, 0x1F, 1, 0);
    s.screen = UWI_PS_GAME, s.full_live = 1;
    uwi_state(&s);
    CHECK(count('K', 0x1F, 0) == 0);
    uwi_key(&s, 0x1F, 0, 0);
    CHECK(count('K', 0x1F, 0) == 1);
}

int main(void)
{
    static const struct { const char *name; void (*fn)(void); } tests[] = {
        { "z_into_cutscene", z_into_cutscene }, { "space_into_menu", space_into_menu },
        { "menu_press_play_release", menu_press_play_release }, { "two_keys_one_code", two_keys_one_code },
        { "shift_run", shift_run }, { "button_release_follows_press_owner", button_release_follows_press_owner },
        { "focus_lost_releases_all", focus_lost_releases_all }, { "repeat_only_walk", repeat_only_walk },
        { "look_wanted_table", look_wanted_table }, { "c_fight_ours", c_fight_ours },
        { "menu_repeat", menu_repeat }, { "key_table", key_table },
        { "left_held_into_death", left_held_into_death }, { "esc_f11_tab_owned", esc_f11_tab_owned },
        { "grave_ours_swallowed", grave_ours_swallowed }, { "game_key_released_back_in_play", game_key_released_back_in_play },
    };
    int passed = 0, failed = 0;
    for (unsigned i = 0; i < sizeof tests / sizeof tests[0]; i++) {
        int before = failed_checks;
        tests[i].fn();
        if (failed_checks == before) passed++;
        else { failed++; printf("FAIL %s\n", tests[i].name); }
    }
    printf("uwinput_test: %d passed, %d failed\n", passed, failed);
    return failed;
}
