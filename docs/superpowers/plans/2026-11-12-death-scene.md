# Death Scene In Our Code Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans. One commit, run-tests.cmd's verdict quoted.
> Builder: the uwr session. Reviewer: the second uwr session's loop. Priority: before the hunters' Task 1.

**Goal:** In our display the death is ours: mouse-look released and the pointer free, the skull animation shown centred and scaled with nothing of the original interface, a fade to black, then the original's start menu untouched. No more inference from scrnum / inplist->mode for this path.

**Architecture:** The game tells the backend its screen with one port-only call, `port_screen(int ps)`, at the few places the screen changes by hand; the backend keeps that state and keys the capture, the cursors and the cutscene crop on it. real_death's presentation (the cutscene, the hide, the fade) is wrapped: in our display `port_death_scene(how)` runs it; elsewhere the original's lines run as before. The game logic of death (experience, bones, reset_game, real_start, the menu) is not touched.

**Tech Stack:** C, the port's hidden runs (work\t13's death scripts), run-tests.cmd.

**Spec:** the user's words, 2026-11-12: "replace it with our own code that shows the skulls and fades to black and go back to the original start menu", on top of the chat design (the explicit screen state).

## Global Constraints

- Every hook under `#ifndef __TURBOC__`; the byte-matched files gain one-line calls only; the original build and the plain uw-original.exe (UWPORT_PLAIN, or display=original) run the original lines.
- Nothing of the death's game logic changes: player_is_dead, real_death's reset_game / real_start / editor_dispatch calls, the experience loss, the bones.
- The replays with our display off stay byte-identical; run-tests.cmd passes.
- Off-screen checks never clip or hide the real mouse (existing rule; set_capture already obeys it).

## Review Focus

1. Death with `how == 0` (SKILLS.C:692: no cutscene): our scene must still release mouse-look and fade to the menu with no animation. Test: a `cheat set hp 1` death by a fall or lava if reachable; else a code read in review.
2. A key or click during the skulls: the original's show_anm ends on input; ours must end the same way, never swallow the key into the menu (the menu's first button must not be pressed by it). Test in Step 5 (a key at skull time, then the menu's buttons intact in the shot).
3. The ending (check_victory) and the intro / dream cutscenes are NOT changed by this task (they keep the crop bug until the cutscene-rectangle fix); the new state must not make them worse: PS_CUT_FULL is set by runcutscene for n < 0x100 and the backend shows those whole (this is the one spill-over fix included, one line in runcutscene and one branch in the backend).
4. A load from the menu after death: PS_GAME is set by real_start's change_screen(MODE_GAME) path, and mouse-look returns as before (look_was). Test in Step 5.
5. Focus lost during the skulls (Alt-Tab): the follower must not send ` into the scene; the scene is not PS_GAME so the follower is idle there. Test with UWPORT_FOCUSTEST during the scene.

---

### Task 1: port_screen, the explicit screen state, and the death scene

**Files:**
- Create: `underworld-exhumed/uw1/src/port/sys/screen.c`, `screen.h` (port only): the state and `port_death_scene`.
- Modify: `uw1/src/game/UWEDIT.C` real_death (the hook), `uw1/src/gfx/CUTS.C` runcutscene (PS_CUT_WINDOW / PS_CUT_FULL around show_anm), `uw1/src/ui/MAINMENU.C` real_start (PS_MENU at entry; PS_GAME is implied by change_screen(MODE_GAME) at its end), `uw1/src/game/UWEDIT.C` change_screen (PS_GAME / PS_MAP / PS_CONV), `uw1/src/game/SKILLS.C` check_victory (PS_ENDING), `uw1/src/ui/MOUSE.C` (mouse-look follows the state: released on any state but PS_GAME, restored on PS_GAME; the moveMouse mode check removed, real_death's and check_victory's own calls removed: one mechanism), `port/win32/plat_win32.c` and `port/win32/uwconv.c` (uo-new): `uwc_on_view_screen()` becomes `port_screen_is(PS_GAME)`; the cutscene crop keys on PS_CUT_WINDOW; PS_CUT_FULL and PS_MENU show the plain picture; PS_DEATH runs the fade.
- Test: `work\t13\e.script`, `g.script` (existing, death by cheat), plus `tools\deathcheck.py` (uo-new) that runs them hidden and asserts.

**Interfaces:**
```c
/* screen.h (port only) */
enum { PS_BOOT = 0, PS_MENU, PS_GAME, PS_MAP, PS_CONV, PS_CUT_WINDOW, PS_CUT_FULL, PS_DEATH, PS_ENDING };
void port_screen(int ps);          /* the game: the screen changed (also calls the backend: plat_screen(ps)) */
int  port_screen_is(int ps);
int  port_screen_now(void);
/* real_death's presentation in our display: 1 when it ran (the caller skips runcutscene(0x103) / scroll_clear / mouse_hide); 0 to run the original's lines (plain build, display=original, hidden run) */
int  port_death_scene(int how);
/* plat.h (the backend) */
void plat_screen(int ps);          /* the state for the window thread (atomic), read by the compose and the pointer code */
int  plat_death_fade(void);        /* 1 while the fade to black runs (the game thread waits on it, polling input so a key can end the skulls early) */
```
**port_death_scene(how), in order:** `port_screen(PS_DEATH)` (releases mouse-look through MOUSE.C: look_was kept); `mouse_hide()`; if how == 1: `set_screen_frame(2, 0); set_compass(); set_flask(0);` as the original, then `runcutscene(0x103)` (the skulls: the original's decoder writes the frames into the frame buffer; the backend, in PS_DEATH, shows the 3D-window rectangle alone, centred and scaled, over black: uwconv's cut_fn as today but keyed on the state, and with no dimmed last view behind it: black); then `scroll_clear(1)`; then the fade: the backend darkens its last composed frame to black over 1000 ms (plat_death_fade), the game thread waits on it polling mouse_get_input so a key or click ends the fade at once; returns 1. real_death then continues with its own lines (editor_dispatch exit, mode 0, reset_game, fadeout, real_start): `port_screen(PS_MENU)` is set by real_start's entry hook, and the backend shows the menu plain with the pointer free.

- [ ] **Step 1: Write `tools\deathcheck.py`**: runs e.script (mouse-look on, cheat die, pointer moves at 60-66 s, shot at 68 s) and asserts: `mouse-look's capture off` appears BEFORE the first `mouse test` line (not only after a raw event, as today); the 320x200 shot shows the menu with the game's cursor at the last move's square and no second cursor image (count the cursor's colour pattern once: the crosshair pixels at the view centre absent); runs a second script with `key enter` at skull time and asserts the shot still shows the menu with four buttons (the key ended the skulls, it did not pick a button: the original's show_anm consumes it and real_start draws fresh); runs g.script with `UWPORT_FOCUSTEST=20000,25000` and asserts no "capture on" after death. Expected now: FAIL on the first assertion (capture off only at 62016 ms).
- [ ] **Step 2: Implement screen.c and the hooks** (list above). MOUSE.C: `port_mouse_look_screen` stays as the one entry; port_screen calls `port_mouse_look_screen(1, 1)` when leaving PS_GAME and `(0, 1)` when entering it; the moveMouse `!(inplist->mode & 0x11)` block and the calls added in b672973 / b200ca1 are removed (one mechanism). change_screen keeps its two existing calls (they map to PS_GAME / PS_MAP / PS_CONV through port_screen).
- [ ] **Step 3: Backend**: `plat_screen` stores the state atomically; compose: PS_CUT_WINDOW → uwc_frame_window over the dimmed last view (as today); PS_DEATH → the same crop over black, then the fade (an alpha ramp over the composed layer, 1000 ms, then black until PS_MENU); PS_CUT_FULL / PS_MENU / PS_ENDING → the plain picture, whole; PS_GAME → as today. The pointer: captured only in PS_GAME; `raw_motion` and the follower idle elsewhere; the host cursor hidden over our display as today, the game's cursor drawn by the game (PS_MENU) or hidden (PS_DEATH: mouse_hide). `uwc_on_view_screen()` returns `port_screen_is(PS_GAME)`.
- [ ] **Step 4: Run deathcheck.py; expect PASS on all three.**
- [ ] **Step 5: Run the Introduction script (work\t13\h.script, home then enter at 6 s) and a dream if cheap: the Introduction shown whole (h-w.png no longer cropped). Run run-tests.cmd: all tests passed (replays unchanged: display off = the original's lines).**
- [ ] **Step 6: Commit** (exhumed + uo-new) with the verdicts quoted; makedist; dist refreshed.

---

### Task 2: the start menu's input in our code (the original's art, screens and intro kept)

**User's words (2026-11-12):** "use the original art and intro but replace it with our code so I don't lose mouse again".
Scope: the choosing is ours; the drawing, the art (opscr.byt, opbtn, fontbig.sys), the intro (runcutscene(0)), the
credits, character creation (create_player) and Journey Onward's restore stay the original's. Character creation gets
only the screen state (PS_CHARGEN, a plain picture with a free pointer); it is not rewritten.

**Files:**
- Create: `underworld-exhumed/uw1/src/port/sys/startmenu.c` (port only): `int port_start_input(int n, struct Button far *b, int text, int sel)`.
- Modify: `uw1/src/ui/MAINMENU.C` parse_start_input: first line under `#ifndef __TURBOC__`: `if (port_screen_menu_ours()) return port_start_input(n, b, text, sel);` (one hook serves the four buttons and the save list: do_journey calls the same routine with text = 1); real_start entry: `port_screen(PS_MENU)` (Task 1); `uw1/src/game/CHARGEN.C` create_player entry: `port_screen(PS_CHARGEN)`, and `port_screen(PS_MENU)` when it returns.
- Backend (uo-new `port/win32/plat_win32.c`): in PS_MENU / PS_CHARGEN the pointer is free, moves are delivered absolute (as now when not captured), clicks go to the game as now; nothing new but the state.
- Test: `tools\deathcheck.py menu` (uo-new).

**Interfaces:**
```c
/* startmenu.c: the original's parse_start_input, with our input: the backend's absolute pointer and the keys.
   Returns as the original does: the button hit (0..n-1), or for Escape -1 where the original allows it (text lists). */
int port_start_input(int n, struct Button far *b, int text, int sel);
int port_screen_menu_ours(void);   /* our display is on (display_mode 0, not hidden, not UWPORT_PLAIN) */
```
**Behaviour (the original's, re-read from parse_start_input / parse_start_mouse before writing):** draw_start_buttons(n, b, text, cur) with the fonts as the original loads them; the keyboard: Up / Down / Left / Right / the grey keys / Ctrl+F / Ctrl+N / Ctrl+B / Ctrl+P move the highlight, Enter / Space choose, Escape = -1 when text; the mouse: the highlight follows the pointer over a button, a click chooses the button under the pointer. Our input comes from `mouse_getxy` after the driver's absolute update (never the look path: PS_MENU has released mouse-look, and the driver's look flag is off) and `do_keyboard_input(0)`; the loop sleeps the way the original's does (loop_music_maybe(); ovr138_94();) so the music and the timer keep running. The original's queued-button and warp code is not reproduced: our pointer is the host's.

- [ ] **Step 1: Write the check** `menu`: a hidden run from the title (no autoload) with UWPORT_MOUSETEST moves to each button's square and a click on "Journey Onward" at 8 s, then a move and click on the first save at 11 s; assert the game restores (an `uwview: level` line) and that a second run with keyboard only (down, down, down, enter; enter) restores too. A third run presses Escape in the save list and asserts the four buttons are back in the shot. Expected now: FAIL (the original's menu ignores MOUSETEST moves while... no: it works today; the check pins the behaviour so the replacement cannot regress it; run it first against the original to see PASS, then against ours).
- [ ] **Step 2: Implement startmenu.c and the hooks.**
- [ ] **Step 3: Run the check: PASS; run e.script's death: the menu after death takes the first pointer move (deathcheck.py's assertion from Task 1 still holds).**
- [ ] **Step 4: run-tests.cmd; commit with the verdicts; makedist.**
