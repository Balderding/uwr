# Input Rewrite Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** One input module that gives every press one owner (its release always matched), mouse-look as the mouse's own mode with no ` keys, and a display that never falls back to the original's 3D-screen interface in our display.

**Architecture:** A pure-C module port\win32\uwinput.c (no Win32 calls; unit-tested) decides the owner of each event from one state (the game's screen, our overlays, focus); plat_win32.c only turns window messages into its events and keeps the handlers it calls. Mouse-look's wanted state is published by the window thread and applied on the game's thread at its own key read (keyqueue.c's key(), which recordings record), so it replays exactly.

**Tech Stack:** C17, LLVM clang 23.1.4 (MinGW target, O:\appsnotonc\llvm), Win32; Python 3.13 (O:\appsnotonc\sentinel\uv\python\cpython-3.13-windows-x86_64-none\python.exe) for the checks.

**Spec:** docs\superpowers\specs\2026-11-12-input-rewrite-design.md

## Global Constraints

- Mechanics exact: no change to the game's rules; every replay and proof byte-identical: `bash work/locked.sh cmd //c "O:\\uw\\uo-new\\run-tests.cmd"` ends "all tests passed" at every commit (CLAUDE.md rule 3).
- Every build and run-tests go through the lock `bash /o/uw/uo-new/work/locked.sh <cmd>`. Port build: `bash work/locked.sh <python> -I tools/portwin32.py` (copies port\win32\* into underworld-exhumed\uw1\src\port\platform\win32 and builds underworld-exhumed\uw1\build\port\uw1port.exe).
- Controls exactly as today's our_keys and world_click (port\win32\plat_win32.c at b9ba607): no key or click changes meaning.
- No ` (0x29) key is ever sent by our code.
- Hidden runs: UWPORT_OFFSCREEN=1, UWPORT_LLM=0, a copy of work\in2-4 as home with `settings-at-start=0` appended. Visible runs are allowed for this work only and never while `tasklist` shows uw-new.exe.
- Never delete files without asking; commit by explicit path; commit messages quote the check's verdict and end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Original game code (underworld-exhumed\uw1\src outside port\) changes only where named here, inside `#ifndef __TURBOC__` port-only blocks.
- Do not change O:\appsnotonc\exhume (the runtime) or any verifier.

## Review Focus

1. A key held while the window loses focus (Alt-Tab) and released elsewhere: on return nothing is held in the game and mouse-look is back on in play. (Task 1 test `focus_lost_releases_all`; Task 5 visible run `alttab`.)
2. A mouse button pressed on our inventory panel and released over the world after the panel closed (or the reverse): the release goes to the panel, the game sees no stray release or press. (Task 1 test `button_release_follows_press_owner`.)
3. The game flushing its input (real_death's `while (mouse_get_input() > -1);`) while a mouse-look change is pending: the change still applies afterwards. (Task 2 test: death with mouse-look on, then a restore: capture on within 1 s of PS_GAME.)
4. Two physical keys translated to the same game code (W and Up both the walk): releasing one while the other is held keeps walking. (Task 1 test `two_keys_one_code`.)
5. A key pressed while our Esc menu is open and released after it closed: nothing reaches the game. (Task 1 test `menu_press_play_release`.)

---

### Task 1: The input module (pure C) and its unit test

**Files:**
- Create: `port\win32\uwinput.h`, `port\win32\uwinput.c`
- Create: `tests\uwinput_test.c`
- Modify: `build.cmd` (build `build\uwinput_test.exe` beside hunternav_test), `tools\verify.cmd` (run it)

**Interfaces:**
- Produces (uwinput.h):
  - `enum UwiOwner { UWI_NONE, UWI_SETTINGS, UWI_MENU, UWI_PANEL, UWI_WORLD, UWI_CENTRED, UWI_GAME };`
  - `typedef struct { int screen; int settings, menu, panel, options_panel, focused, display_ours, full_live, mouse_look_enh, replaying, shift_down; } UwiState;` (screen = screen.h's PS_* value)
  - `typedef struct UwiOut { void (*game_key)(int code, int down); void (*game_pointer)(int type, float x, float y, unsigned button); int (*settings_event)(int kind, int code, int down, int x, int y); int (*menu_event)(int kind, int code, int down, int x, int y); int (*panel_event)(int kind, int code, int down, int x, int y); int (*world_event)(int kind, int code, int down, int x, int y); int (*centred_event)(int kind, int code, int down, int x, int y); void (*our_action)(int action, int down); } UwiOut;` (kind: UWI_KEY 0, UWI_BUTTON 1, UWI_MOVE 2; a handler returns 1 when it took the event)
  - `void uwi_init(const UwiOut *out);`
  - `void uwi_key(const UwiState *s, int code, int down, int repeat);` (code: set-1, 0x100 for E0)
  - `void uwi_button(const UwiState *s, unsigned button, int down, int x, int y);`
  - `void uwi_move(const UwiState *s, int x, int y);`
  - `void uwi_state(const UwiState *s);` (called every frame and on every change: an owner no longer valid for a held press gets its release now)
  - `void uwi_release_all(void);` (focus lost)
  - `int uwi_look_wanted(const UwiState *s);` (spec "Mouse-look": 1 iff display_ours && screen == PS_GAME && !settings && !menu && !panel && !options_panel && focused && mouse_look_enh && !replaying)
  - the key table: `static const struct { int key; int shift; int action; int code; } keys[]` with action UWI_HOLD (code held while the key is), UWI_TAP (code down and up at once), UWI_OURS (our_action(action-id)); entries exactly today's our_keys (plat_win32.c b9ba607 lines of `static int our_keys`), including W/Up -> 0x2D walk or 0x11 with Shift, Space -> 0x2A, L -> 0x10 (Shift+L ours, Ctrl+L ours), Q -> 0x42, Z -> 0x44, K -> 0x43, Down -> 0x1F, PageUp -> 0x148, PageDown -> 0x150, M -> 0x2C, C -> tap 0x3F, R -> tap 0x13, , -> tap 0x21, and the rest UWI_OURS.

- [ ] **Step 1: Write the failing tests** in tests\uwinput_test.c with a fake UwiOut recording every call into an array, each test a function, main runs all and prints "uwinput_test: N passed, M failed", exit code M:
  - `z_into_cutscene`: state PS_GAME; key 0x2C down -> game_key(0x44,1); state PS_CUT_FULL; key 0x2C up -> game_key(0x44,0) exactly once, no game_key(0x2C,...).
  - `space_into_menu`: PS_GAME, 0x39 down -> (0x2A,1); menu=1 via uwi_state -> (0x2A,0) sent at once; 0x39 up -> nothing more.
  - `menu_press_play_release`: menu=1, key 0x2C down -> menu_event called, no game_key; menu=0; 0x2C up -> no game_key.
  - `two_keys_one_code`: W down (0x11) -> (0x2D,1); Up down (0x148) -> nothing new; W up -> nothing; Up up -> (0x2D,0).
  - `shift_run`: W down -> (0x2D,1); Shift 0x2A down -> (0x2D,0),(0x11,1); Shift up -> (0x11,0),(0x2D,1).
  - `button_release_follows_press_owner`: panel=1, button 1 down -> panel_event; panel=0; button 1 up -> panel_event gets the up, game_pointer never called.
  - `focus_lost_releases_all`: Z down, Space down, button 2 down to WORLD; uwi_release_all -> (0x44,0),(0x2A,0), world up; later physical ups -> nothing.
  - `repeat_only_walk`: repeat of W -> (0x2D,1) again; repeat of Z -> nothing.
  - `look_wanted_table`: uwi_look_wanted for PS_GAME plain =1; each of menu, panel, settings, options_panel, !focused, PS_MAP, PS_CONV, PS_TEXT, PS_DEATH, replaying, !display_ours =0.
  - `c_taps_f5`: PS_GAME, C down -> (0x3F,1),(0x3F,0); C up -> nothing.
- [ ] **Step 2: Build and run; expect failure** (no uwinput.c): add to build.cmd `clang %CLANG_FLAGS% -std=c17 -O2 -Wall -Iport\win32 -o build\uwinput_test.exe tests\uwinput_test.c port\win32\uwinput.c || exit /b 1` and run `build\uwinput_test.exe`. Expected: link error.
- [ ] **Step 3: Implement uwinput.c.** Owner choice for a new press, in this order: settings -> UWI_SETTINGS; menu -> UWI_MENU; panel && the panel handler takes it -> UWI_PANEL; display_ours && screen == PS_GAME && full_live && the key table has the key (or a button) -> UWI_WORLD for buttons when world_event takes it, the table's action for keys; screen in {PS_MAP, PS_CONV, PS_TEXT} && centred_event takes it -> UWI_CENTRED; else UWI_GAME (game_key(code) / game_pointer as it comes). Per physical key: owner and the code sent (0 none). Per button: owner. Walk: one shared "walk code sent" with a held-set of W/Up, as walk_update today. uwi_state: for each held press whose owner is UWI_GAME or UWI_WORLD and the new state no longer allows it (screen left PS_GAME for WORLD; menu/settings opened for GAME/WORLD), release it now and mark the key "released early" so its later up sends nothing.
- [ ] **Step 4: Run** `build\uwinput_test.exe`. Expected: `uwinput_test: 10 passed, 0 failed`.
- [ ] **Step 5: Add it to tools\verify.cmd** after hunterfile_test (`"%~dp0..\build\uwinput_test.exe"` / `if errorlevel 1 exit /b 1`), run run-tests.cmd: "all tests passed". Commit uwinput.h, uwinput.c, tests\uwinput_test.c, build.cmd, tools\verify.cmd with the verdict.

### Task 2: Mouse-look as the mouse's mode (game side)

**Files:**
- Modify: `underworld-exhumed\uw1\src\port\sys\keyqueue.c` (key())
- Modify: `underworld-exhumed\uw1\src\ui\MOUSE.C` (do_keyboard_input, port_mouse_look_screen, new entry)
- Modify: `underworld-exhumed\uw1\src\port\sys\screen.c` (port_screen no longer calls port_mouse_look_screen)
- Test: `tools\inputcheck.py` (new; subcommand `look`)

**Interfaces:**
- Produces: `void port_mouse_look_want(int on);` (any thread: stores an atomic int) in MOUSE.C; `int port_mouse_look(void);` unchanged.
- Reserved key() results: `PORT_LOOK_KEY_OFF 0xFE00`, `PORT_LOOK_KEY_ON 0xFE01` (defined in keyqueue.c and MOUSE.C's port block; first verify no entry of the Asc table at seg063 0x10..0x10F and no scan code can produce 0xFE in key()'s high byte; if one can, pick another unused high byte and record why).

- [ ] **Step 1: Write the check** tools\inputcheck.py `look`: hidden run of the menu restore (`3000 key esc / 5000 key esc / 7000 key esc / 9000 key end / 9300 key enter / 12000 key home / 12600 key enter / 14000 key enter`), `18000 cheat die`, then the restore again at 33000..38000, with UWPORT_LOOKDBG=1 and UWPORT_FOCUSTEST=1000,1500; assert from stderr: no line contains "follower"; "mouse-look on" (the new line, Step 3) after each "screen 2"; "mouse-look off" right after "screen 7"; exactly one on per PS_GAME entry.
- [ ] **Step 2: Run it on the current build.** Expected FAIL ("follower" lines present).
- [ ] **Step 3: Implement.** keyqueue.c key(): before reading the ring, when not replaying (rp_request != 2) and `port_look_want_pending()` (MOUSE.C: want != look_on and ENHANCED(ENH_MOUSE_LOOK)), return PORT_LOOK_KEY_ON or _OFF (the RP_KEY_READ hook records it). MOUSE.C do_keyboard_input: right after `c = KEY();`, if c is one of the two, `mouse_look_set(c & 1)`, print under UWPORT_LOOKDBG "uw1port: mouse-look on|off (want)" to stderr, return -1. Remove port_mouse_look_screen's use from screen.c (keep the function unused only if something else calls it; else delete it). The ` binding in PLAYER.C stays.
- [ ] **Step 4: Run** `python -I tools/inputcheck.py look` (with Task 3 not yet done the backend still runs its follower: run this check after Task 3's Step 3; until then verify only that key() returns the code: a hidden run with UWPORT_LOOKDBG shows "mouse-look on (want)" lines when port_mouse_look_want(1) is called from plat_win32.c's first frame). Run run-tests.cmd: "all tests passed" (replays carry no want: rp_request 2 never returns the codes).
- [ ] **Step 5: Replay check:** a hidden run with `--record` that enters play (mouse-look on via want) and turns with `look 200 0` events, then `uw1port.exe --replay <its RECORD.OUT>` hidden: the final player heading (uwlook / the "uwview: level" camera line, or `--screenshot` compared) equals the recording run's. Commit keyqueue.c, MOUSE.C, screen.c (in underworld-exhumed) and tools\inputcheck.py with the verdicts.

### Task 3: The backend through the module

**Files:**
- Modify: `port\win32\plat_win32.c`
- Modify: `tools\portwin32.py` only if uwinput.c is not picked up by "every file of port\win32" (check first)

**Interfaces:**
- Consumes: Task 1's uwinput.h; Task 2's `port_mouse_look_want`, `port_mouse_look`.

- [ ] **Step 1: Save the old build for the trace comparison**: copy underworld-exhumed\uw1\build\port\uw1port.exe built at b9ba607 to `work\inputrw\old\` with its uwview.dll, uw_nvngx.dll and dlss\ (rebuild b9ba607's plat_win32.c if the current build already differs).
- [ ] **Step 2: Add the trace**: `UWPORT_INPUTTRACE=1` prints every byte the window thread gives the game (`in: key XX`) and every pointer event (`in: ptr type x y button`) at one place each (send_code / send_down / to_game's callers all go through `game_key` / `game_pointer`). Add the same two printfs to the old build's copy of plat_win32.c (work\inputrw\old-src) before building it in Step 1.
- [ ] **Step 3: Rewire.** WM_KEYDOWN/UP, WM_*BUTTON*, WM_MOUSEMOVE, WM_INPUT, WM_KILLFOCUS/SETFOCUS and plat_key_byte / plat_pointer_event (input scripts) build a UwiState (screen_now(), settings_open(), menu_open, inv_open && display_mode == 0, panel_wait / uwl_left_panel() == 1, focused, display_mode == 0, full_live, ENHANCED(ENH_MOUSE_LOOK), rp_request == 2, shift_held) and call uwi_key / uwi_button / uwi_move; WM_KILLFOCUS calls uwi_release_all. UwiOut's handlers wrap the existing code: settings (settings_key / settings_at), menu (menu_key's body, the menu's pointer part of pointer_msg), panel (inv_pointer), world (world_click), centred (pointer_msg's uwc part), game (send_code / to_game with to_picture), our_action (the UWI_OURS cases of our_keys). Each frame: `uwi_state(&s)`, `port_mouse_look_want(uwi_look_wanted(&s))`, and the capture follows `port_mouse_look()` (set_capture(1) when it turns on with focus, set_capture(0) when off). Delete: our_keys, our_release, xl_down, xl_sent, our_forget, walk_update's state (moved), mouse_look_follow's look logic and its sends, look_auto, the ` in panel_shortcut / shortcut_pump (open the panel by its key without toggling look: the want is off while options_panel is up), first_pick's dependence on look stays as is.
- [ ] **Step 4: Build and run uwinput_test, deathcheck, followcheck (its "no ` sent" asserts now hold trivially; update nothing in it), inputcheck look**: all PASS. run-tests.cmd: "all tests passed".
- [ ] **Step 5: Trace comparison** `python -I tools/inputcheck.py trace`: one hidden script in play (work\in2-4) using every control of the key table once and each click (left tap, left drag, right click, C, Z with release after the cutscene) run on work\inputrw\old and the new build with UWPORT_INPUTTRACE=1; the two `in:` sequences equal after removing old 0x29 bytes; differences only where the old build split a press (the Z release: old `key 2C` up, new `key 44` up), each listed and explained in the commit message. Commit plat_win32.c, tools\inputcheck.py with the verdicts.

### Task 4: The display never falls back to the original's 3D screen

**Files:**
- Modify: `port\win32\plat_win32.c` (view_world's caller in the compose: where `!layer` with screen PS_GAME leads to V.compose of the original's picture)

- [ ] **Step 1: Write the check** `inputcheck.py display`: hidden runs on work\in2-4: a level change (walk down the stairs used by tools\deathcheck's home or a `cheat level 2` if the cheat list has it: read inscript/cheat.c's list first), a conversation started and ended, the map opened and closed; count frames where screen is PS_GAME and the compose showed the original's picture (a new UWPORT_LOOKDBG line "display: original picture in PS_GAME at N ms"); expect 0, and expect "display: kept our last frame" lines where a frame did not match.
- [ ] **Step 2: Run on the current build**: record the counts (expected > 0 for at least one case if the user's report reproduces; if 0, keep the check as a guard).
- [ ] **Step 3: Implement**: in our display with screen PS_GAME, when view_world returns 0, keep the last composed frame (the existing stale path, without its 2 s limit and without requiring panel_left) and log "display: kept our last frame (no match|no view|level) at N ms" at most once a second; never compose the original's 3D-screen picture in PS_GAME in our display.
- [ ] **Step 4: Run** `inputcheck.py display` (0 original pictures in PS_GAME), deathcheck, run-tests.cmd: all pass. Commit with the verdicts.

### Task 5: The real capture (visible runs), the user's scenarios, docs, dist

**Files:**
- Modify: `tools\inputcheck.py` (subcommand `visible`), `NOTES.md`
- Run: `tools\makedist.py`

- [ ] **Step 1: Write `inputcheck.py visible`** (refuses to run while uw-new.exe is running): visible runs (no UWPORT_OFFSCREEN) of uw1port.exe with UWPORT_LOOKDBG=1, input scripts as the hidden ones, the window's own pointer events by UWPORT_MOUSETEST, focus by UWPORT_FOCUSTEST: `play` (capture on in PS_GAME, the cursor hidden), `menu` (Esc: capture off, cursor shown; Esc again: on), `panel` (I: off; I: on), `conv` (a conversation: off, back on after), `death` (off at PS_DEATH, on after a restore), `newchar` (death, Create Character, play: on once), `alttab` (focus away and back while W and the right button are held: nothing held after, capture back on), `sleep` (Z held into the dream: one dream). Each asserted from the log lines (capture on/off, mouse-look on/off, screen) and `ClipCursor` state reported by a new UWPORT_LOOKDBG line at each set_capture.
- [ ] **Step 2: Run** `inputcheck.py visible` (about 8 runs of ~1 minute; the user allowed it). Expected: all PASS.
- [ ] **Step 3: NOTES.md**: a section "Input (2026-11-12)": the module, the owner rule, mouse-look by want at key() (recorded), the display rule, the checks; remove statements in NOTES.md about the follower.
- [ ] **Step 4: run-tests.cmd** "all tests passed"; `tasklist | grep uw-new` empty, then `python tools/makedist.py`; commit NOTES.md and inputcheck.py with every verdict.

### Task 6: A key ring that cannot overrun, and key_on kept in step (approved by the user 2026-11-13)

**Files:**
- Modify: `underworld-exhumed\uw1\src\port\sys\keyqueue.c`
- Test: `underworld-exhumed\uw1\tests\keyqueue_test.c` (new; built by build.cmd beside hunternav_test, run by tools\verify.cmd), `tools\inputcheck.py` (subcommand `ring`)

**Interfaces:**
- Consumes: keyqueue.c's kbd_byte / kbd_tick_ms / int9 / key() and the Task 3 diagnostics ("keyqueue: ring overrun", "key_on: 0xNN held").
- Produces: no new interface; key() returns exactly the results it returns today for the same byte sequence.

Why: the 64-byte ring (seg063 0300h-033Fh) overruns when the game stops reading (paused under our menu or settings, a long load) while bytes keep coming (held keys' typematic repeats, presses): the write pointer laps the read pointer, everything in the ring is lost at once and a lost break leaves key_on counting a key as held (the user's random failures). The rule: no make or break byte is ever lost; only typematic repeats are dropped while the ring is full (a real keyboard's buffer drops them too).

- [ ] **Step 1: Write the failing test** keyqueue_test.c (links keyqueue.c with a stub seg063 and port_setw): `overrun_keeps_pairs` — install, send 40 make/break pairs (80 bytes) with no key() read, then read key() until 0: every pair's character comes out once, in order, and every key_on entry is 0 afterwards; `repeat_dropped_when_full` — hold a key (make only), run kbd_tick_ms for 10 s with no read: the ring never overruns, then the break, then reading: one make result, key_on 0 at the end; `results_unchanged` — a sequence that fits in the ring gives the same key() results and the same seg063 bytes as the current code (record them from the current keyqueue.c first).
- [ ] **Step 2: Run; expect FAIL** on the first two (bytes lost).
- [ ] **Step 3: Implement**: a host-side overflow queue (plain static array, 4096 bytes) behind the ring: int9 stores into the ring only while it has room (keeping one slot free, as the ring's full test needs), else appends to the queue; a typematic repeat (from kbd_tick_ms) is dropped when the ring is full; key() (and whole()) first moves queued bytes into the ring as room allows, under the same spinlock, before reading. Keep the Task 3 diagnostics (the overrun line becomes "keyqueue: ring full, N bytes queued", once per occurrence, printed outside the lock). Replays: key() is not run in a replay (the hook returns the recorded results), so nothing changes there.
- [ ] **Step 4: Run** keyqueue_test (all pass), inputcheck all hidden subcommands, deathcheck, run-tests.cmd "all tests passed"; add `inputcheck.py ring`: a hidden run in play holding W (`down w`) while our Esc menu is open for 20 s, then closing it and releasing W: no "ring" overrun line, no "key_on: 0x11 held" line after the release, the player stops walking (the uwlook / camera position no longer changes 2 s after the release).
- [ ] **Step 5: Commit** keyqueue.c, keyqueue_test.c (underworld-exhumed), build.cmd, tools\verify.cmd, tools\inputcheck.py, NOTES.md (uo-new) with the verdicts; makedist (tasklist empty first).
