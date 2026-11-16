# Our files inside underworld-exhumed

`underworld-exhumed\` is a separate git repository: underworld-exhumed's byte-matched C decompilation of UW1 (MIT),
with its portable UW1 port in `uw1\src\port`. Our changes are on the branch **`uw-new`**, on top of the upstream
commit **d8e0581**. This document covers only the files we **added** there:

```
git -C underworld-exhumed diff --diff-filter=A --name-only d8e0581 HEAD
```

It does not cover the edits we made to underworld-exhumed's own files, which are mostly short hook calls marked
`/* uw-new: ... */` or `/* port only: ... */`. Their call sites are listed under each file below, so you can follow
the links. This document lives outside `underworld-exhumed\` on purpose, so it is not part of that repository.

All of these files are "port only": none of them is part of the DOS build, and the game's rules stay the original's.
The ones that change behaviour (`hunt.c`, `hunter.c`) do so only behind the `smart-hunting` enhancement or the
hunters' settings, which are off by default.

Not listed: `uw1\src\port\platform\win32\` (untracked) holds copies of our backend. `tools\portwin32.py` copies them
there from `port\win32\` at every build (see `port\README.md`).

## Files

| File | Lines | Area |
| --- | --- | --- |
| `uw1/src/port/portview.h` | 60 | The 3D view's hooks, for our renderer |
| `uw1/src/port/sys/portview.c` | 95 | Their state |
| `uw1/src/port/sys/fxevent.c` | 67 | Effect events, game -> backend |
| `uw1/src/port/sys/screen.h`, `screen.c` | 20, 63 | The screen state, and the death scene in our display |
| `uw1/src/port/sys/startmenu.c` | 110 | The start menu's choosing, in our code |
| `uw1/src/port/sys/hunt.c` | 1171 | smart-hunting: creature goals HUNT / INVESTIGATE / AMBUSH, friendly NPCs, questions to the language model |
| `uw1/src/port/sys/hunter.h`, `hunter.c` | 28, 1080 | The level hunters |
| `uw1/src/port/sys/hunternav.h`, `hunternav.c` | 28, 121 | The hunters' navigation grid and routes (pure C) |
| `uw1/src/port/sys/hunterfile.h`, `hunterfile.c` | 36, 142 | The hunters' side file `SAVE0\UWNEW.DAT` (pure C) |
| `uw1/tests/hunternav_test.c` | 114 | Unit test of hunternav.c |
| `uw1/tests/hunterfile_test.c` | 107 | Unit test of hunterfile.c |
| `uw1/tests/keyqueue_test.c` | 219 | Unit test of `port/sys/keyqueue.c` (the keyboard ring) |

## The 3D view: portview.h, portview.c

**What they do.** Our renderer draws the 3D view in place of the original's. The game uses these files to report
what each frame shows:

- the camera as `get_eye` placed it;
- the level, the light level, the lens (zoom) and the view's size;
- whether the 3D window on the screen shows the 3D view at all.

The state lives in `PortView port_view` (portview.h, line ~31). If a backend sets `port_view_replace`, the game leaves
out only the rasterization (cRender's drawing). Its pacing, the grid walk, the render database and the pick pass all
still run. The frame buffer is then filled with `PORTVIEW_MARK` (colour 0), so whatever the game draws over it
afterwards (the weapon, the ornaments, the cursor) stays apart from the 3D picture. Without a backend that sets
`port_view_replace` and `port_view_notify`, nothing changes: the game does what the original does.

**Functions** (portview.c):

| Line | Function | Called from |
| --- | --- | --- |
| 33 | `port_view_eye` | VIEW3D.C's `setup_vars` (line 473) |
| 44 | `port_view_draw` | Replaces cRender in VIEW3D.C's `send_db` / `render_FB` (lines 378, 430) |
| 64 | `port_view_picked` | VIEW3D.C's `do_3d_grab` (line 398) |
| 67 | `port_view_shown` | VIEW3D.C (line 386) and PANELS.C (line 1303) |
| 79 | `port_view_effect` | GRFX.C's fades and flash (lines 211, 231, 273) |
| 91 | `port_view_gone` | UWEDIT.C (line 351), CUTS.C (line 808) |

**Links.** On the backend side, `port\win32\uwgame.c` uses these hooks only when it is built with
`-DUWPORT_GAMEVIEW` (uw-new; `tools\portwin32.py`). It then includes `portview.h`, sets
`port_view_notify = gv_notify` and `port_view_replace`, and hands `UwgView` to `plat_win32.c`'s `view_world`. The struct is laid out with `#pragma pack(push, 8)`: the port packs to 1,
the way Turbo C does (`compat.h`), and both sides must agree on the layout.

## Effect events: fxevent.c

**What it does.** A one-way stream of events from the game to the backend: a ring of 64 slots, lock-free, with one
writer and one reader. The places where the original already decides that an effect happens call
`port_fx_event_vol` / `port_fx_event` (line 20). Those places are SOUND.C's `play_effect`, `play_effect_here`, the
mobile's effect, and `kill_effect` / `kill_all_effects`, which send FX_STOP (SOUND.C lines 637-753). The backend
drains the ring once per frame (`port_fx_drain`, line 42). Nothing is read back by the game, so the rules and the
replays are untouched. A full ring drops the newest event. `UWPORT_FX_LOG=1` prints each event.

There is one channel the other way: `port_fx_control` (line 55), from the backend, sets which effects our audio module
is playing itself. SOUND.C's `fx_play` then asks `port_fx_taken` (line 60) and sends those at volume 0, and asks
`port_fx_pan` (line 64) whether to mirror the pan (SOUND.C lines 906-908).

The types (`PortFxEvent`, `FX_SOUND` ... `FX_STOP`, `FX_MUTED`) are declared in `uw1/src/port/portgame.h`. That is an
underworld-exhumed file we edited; we did not add it.

**Links.** `port\win32\plat_win32.c`'s `fx_pump` passes each event to `port\win32\uwaudio.c` (positional sound) and
to the renderer (`uwv_fx_event`: particles, decals). Plan: `docs\superpowers\plans\2026-11-12-fx-audio-and-visual.md`.

## The screen state and the death scene: screen.h, screen.c

**What they do.** The game tells the backend explicitly which screen it is on, so the backend no longer guesses from
`scrnum` / `inplist->mode`. The states are `PS_BOOT`, `PS_MENU`, `PS_GAME`, `PS_MAP`, `PS_CONV`, `PS_CUT_WINDOW`,
`PS_CUT_FULL`, `PS_DEATH`, `PS_ENDING`, `PS_CHARGEN` and `PS_TEXT` (screen.h). The backend only wants mouse-look on
`PS_GAME`, so the pointer is let go on every other screen.

- `port_screen` (screen.c, line 29) is called where the game changes its screen by hand: UWEDIT.C `change_screen`
  (line 292), CUTS.C `runcutscene` (lines 811-813), MAINMENU.C (line 171), CHARGEN.C (lines 784, 793), SKILLS.C
  (`PS_ENDING`, line 681), and SCROLLIO.C's text prompt (`PS_TEXT`, lines 558 / 645). It forwards each change to the
  backend's `plat_screen`.
- `port_screen_is` and `port_screen_now` read the state. `port_screen_menu_ours` is true when our display is on and
  this is not a replay.
- `port_death_scene` (line 43) is called from UWEDIT.C's `real_death` (line 408). In our display it shows the
  original's skull cutscene (0x103), then waits while the backend fades to black (`plat_death_fade_start` /
  `plat_death_fade`). A key or a click ends the wait, and music keeps playing. It returns 0 to run the original's
  own lines instead: the plain build, the original's display, or no renderer.

**Links.** `port\win32\plat_win32.c`: `plat_screen`, `plat_death_ours`, `plat_death_fade*` (lines 112-121).
`port\win32\uwinput.h` copies the `PS_*` values as `UWI_PS_*`, and `plat_win32.c` checks that they match with
`_Static_assert`. `port\win32\uwconv.c` uses `port_screen_is(PS_GAME)`. Plan:
`docs\superpowers\plans\2026-11-12-death-scene.md`.

## The start menu: startmenu.c

**What it does.** In our display, MAINMENU.C's `parse_start_input` hands over to `port_start_input` (line 44) at its
top (MAINMENU.C lines 349-350). This covers both the four buttons and Journey Onward's list of saved games. The
drawing, the art, the Introduction, the credits, character creation and the restore all stay the original's. The
behaviour also follows the original:

- the highlight follows the pointer;
- a click chooses the button under it (`under`, line 26);
- the original's keys work (the arrows, Ctrl+F / N / B / P, Home / End / PgUp / PgDn, Enter, Escape, Alt+X);
- the loop keeps the music and the colour cycle running.

The original's queued-button and keyboard-warp code is left out, so the mouse is not lost.

**Links.** The switch is `screen.c`'s `port_screen_menu_ours`. The pointer is free because of `PS_MENU`.

## smart-hunting: hunt.c

**What it does.** The `--enhance smart-hunting` option, off by default. With it off, none of this runs and AI.C is
UW.EXE's. With it on, there are three creature goals the original does not have, in the goal word's free values:

| Goal | Value | Behaviour |
| --- | --- | --- |
| HUNT | 13 | Walks to where it last knew the player was, looks about and searches. Gives up after 30 s. |
| INVESTIGATE | 14 | Goes to look at a clear noise or the player's light, then returns to its old goal (20 s). |
| AMBUSH | 15 | Clever creatures only (the `AMBUSHERS` list: goblins, mountainmen, lizardmen, outcasts, fighters, giant spiders). Waits near where it expects the player (60 s), then hunts. |

A creature acts only on what it perceived. Movement, paths, combat and every roll stay the game's. The timers are
kept in `hs[10][256]`, not in the object, so the save format is unchanged. The file also has friendly-NPC behaviour
(`port_friend_step`, line 747: greet, follow, warn) and the questions to the language model (`mind_ask` 467,
`mind_collect` 515, `mind_act` 548).

**Entry points and their callers in AI.C**, each behind `ENHANCED(ENH_SMART_HUNTING)`:

| Line | Function | AI.C line |
| --- | --- | --- |
| 1028 | `port_hunt_sense` | 315 |
| 1000 | `port_hunt_lost` | 482, 492 |
| 991 | `port_hunt_no_path` | 509 |
| 747 | `port_friend_step` | 1284 |
| 857 | `port_hunt_goal` | 1346 |

The noise hooks `port_hunt_noise` (1131) and `port_hunt_noise_here` (1167) are called from SOUND.C (lines 637, 698,
718).

Shared with `hunter.c`: `hunt_rally` (244), `port_ai_name` (146), and `port_ai_say` (176), which writes a line to the
Ctrl+L AI log through `port_scroll_note`. `UWPORT_AIDBG=1` logs every goal started and ended.

**Links.**

- The model: `port\win32\uwmindhost.c`, through `port_mind_ready`, `port_mind_ask` and `port_mind_take`.
- The log lines go to `port\win32\uwlog.c`'s `port_scroll_note`.
- The level hunters are `hunter.c`, below.
- Background: `LLM_NPC_AI.md`.

## The level hunters: hunter.h, hunter.c

**What they do.** On each level, one or two clever hostile creatures notice that the level has changed. They follow a
short plan of up to 4 steps (`goto_last`, `goto_door`, `goto_home`, `goto_water`, `wait`, `search`, `fetch_kin`,
`follow_light`, `recover`, `give_up`), remember what happened, and keep all of it across a save and a load.

The plan comes from the language model (question kind 6, one step at a time), or from `UWPORT_HUNTER_PLAN=step,...`
for the checks. A leg of a plan is a `crit_head_for_loc` toward a waypoint, through `hunt.c`'s goals. It is all off
unless `port_hunter_on()` (line 105): setting `llm_hunters` (default 2), smart-hunting on, the model loaded, and not
a replay.

The interface is `hunter.h`:

- `port_hunter_step` (line 855): from `hunt.c`, for the creature being run.
- `port_hunter_notice` (170): a ledger entry.
- `port_hunter_lost` / `port_hunter_spotted` / `port_hunter_finished` (1015-1028).
- `port_hunter_answer` (655): the model's answer.
- `port_hunter_level` (374): called from GAMEWRAP.C lines 450 / 456, before the old level is saved and after the new
  one is loaded.
- `port_hunter_save` / `port_hunter_load` (1043 / 1055): called from GAMEWRAP.C lines 333 / 261. They write and read
  `SAVE0\UWNEW.DAT`, which is copied into SAVEn with the rest of the save.

Other main functions: `pick` 273, `build_nav` 124, `plan_start` 524, `step_begin` 476, `route_to` 456 and
`notice` 812. Debug output: `UWPORT_HUNTERDBG=1|2`.

**Links.** The spec is `docs\superpowers\specs\2026-11-12-level-hunters-design.md` and the plan is
`docs\superpowers\plans\2026-11-12-level-hunters.md`. Checks: `tools\huntercheck.py`.

## Navigation: hunternav.h, hunternav.c

**What they do.** Pure C with no game globals. They work on a level's 64x64 grid of cells (`HunterNav`: walk, water,
door, and the height in bits 3-6).

- `hnav_route` (line 28): a breadth-first route over the 4 orthogonal neighbours, within 4096 visited cells and up
  to 256 cells long. A door costs 2. A step of more than one height unit up or down is not taken.
- `hnav_waypoints` (line 102): cuts a route into legs the creature's own walk can finish (`clear_line`, line 86).

**Links.** `hunter.c` fills the grid from the tile map (`build_nav`). It is tested by `uw1/tests/hunternav_test.c`.

## The side file: hunterfile.h, hunterfile.c

**What they do.** Pure C. `SAVE0\UWNEW.DAT` is a text file:

```
uwnew 1
hunter L MOB ITEM ALERT PLAN_T STEP S1 S2 S3 S4 ENDED
mem    L MOB kind t x y
ledger L kind t x y mob
graphblock L x y
```

- `hfile_write` (line 18) writes it whole and atomically: it writes `path.tmp`, then renames it over the file with
  `MoveFileExA`.
- `hfile_read` (line 72) reads it line by line, range-checking every number (`num`, line 55). Bad or unknown lines
  are skipped.

The records, kinds and limits are in `hunterfile.h` (`HunterSave`: 20 hunters, 320 memories, 320 ledger entries, 160
blocked cells). The header pins the struct layout with `#pragma pack(push, 8)`, because `compat.h` leaves
`#pragma pack(1)` on.

**Links.** Used by `hunter.c`'s `port_hunter_save` / `port_hunter_load`. Tested by `uw1/tests/hunterfile_test.c`.

## The tests: uw1/tests/*_test.c

All three are built by uo-new's `build.cmd` into `build\` and run by `tools\verify.cmd`, so they run inside
`run-tests.cmd`. Each prints a `NAME PASS|FAIL` line per test, and the exit code is the number of failures.

| Test | Built from | What it checks |
| --- | --- | --- |
| `hunternav_test.c` | `hunternav.c` | Routes and waypoints on ASCII maps (`#` solid, `.` walk, `~` water, `+` door): `test_route_straight`, `test_route_through_door`, `test_water_only_for_swimmers`, `test_waypoints_within_reach`. |
| `hunterfile_test.c` | `hunterfile.c` | The side file in and out: `test_roundtrip`, `test_tmp_rename`, `test_missing_file_is_empty`, `test_bad_header`, `test_bad_numbers_skipped`, `test_unknown_lines_skipped`, `test_truncated`. Run from uo-new, so its file goes to `build\hunterfile_test.dat`. |
| `keyqueue_test.c` | underworld-exhumed's `port/sys/keyqueue.c` (edited by us, not added) | The keyboard ring when the game stops reading: `results_unchanged`, `overrun_keeps_pairs`, `repeat_dropped_when_full`, `key_on_only_lost` (described in its header). It provides `seg063` and the names keyqueue.c needs, and includes Exhume's runtime `port.h` (`-IO:\appsnotonc\exhume\runtime\port`). |

These are our tests, not underworld-exhumed's own test suites. Those are never run (the user's rule).
