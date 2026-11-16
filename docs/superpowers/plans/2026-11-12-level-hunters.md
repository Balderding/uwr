# Level Hunters Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
> Here the builder is the uwr session, one task per commit, each commit reviewed by the second uwr session's loop
> before the next task starts. A task's commit message quotes run-tests.cmd's verdict (CLAUDE.md rule 3).

**Goal:** One or two hunters per level that notice the level has changed, get a 2-4 step plan from the model, carry it out with the original's movement through the existing goals 13-15, remember, and keep it all across save and load.

**Architecture:** Three new port-only files in underworld-exhumed's uw1/src/port/sys: `hunternav.c` (pure: a level's passability grid, BFS routes, waypoints; no game globals), `hunterfile.c` (pure: the UWNEW.DAT text records in and out), `hunter.c` (the game-facing layer: the pick, the ledger, noticing, plans, legs, memory; calls hunt.c's begin/arrive through four small hooks). Three port-only hooks in GAMEWRAP.C. The model is asked through uwmindhost.c as today, question kind 6.

**Tech Stack:** C (clang, MinGW target, as the rest of the port), the port's own input scripts and hidden runs for checks (docs/BUILDING.md), uwtour for level loads, run-tests.cmd.

**Spec:** O:\uw\uo-new\docs\superpowers\specs\2026-11-12-level-hunters-design.md

## Global Constraints

- Port-only code: every new file and hook is outside `#ifdef __TURBOC__` paths; AI.C, PATHFIND.C and all byte-matched files get no change except the three GAMEWRAP.C hooks under `#ifndef __TURBOC__` (spec 2).
- No rule number changes: movement, combat, doors, rolls stay the original's (spec 7).
- Off when the model is off (llm=0, UWPORT_LLM=0), off in every hidden/off-screen run and proof unless UWPORT_HUNTER_PLAN is set, off unless smart-hunting is on; setting `llm_hunters=0|1|2`, default 2 (spec 7).
- Named creatures (whoami != 0) are never hunters and never kin a hunter calls (spec 3, 7).
- Hunters: hp > 0, ATT_HOSTILE, whoami == 0, wits >= 10, largest hostile race, AMBUSHERS first, then highest wits, then lowest mobile index; at most two (spec 3).
- Limits: ledger 32 entries, entries older than 10 min dropped; memory 4 sightings, 8 blocked cells, 4 kin deaths; plan 4 steps; one question per hunter per 20 s; a plan older than 3 min dropped; BFS at most 4096 cells; a waypoint within 4 squares with a clear tile path (spec 4, 5).
- Side file `SAVE0\UWNEW.DAT`, text, `uwnew 1` header, written atomically (.tmp then rename), read tolerantly (spec 6).
- Log lines "AI: hunter #N ..." on the Ctrl+L log (port_ai_say) and the jsonl (spec 7).
- Every commit: run-tests.cmd passes and its verdict is quoted; the replay checks stay byte-identical with hunters off.

## Review Focus

1. A hunter whose mobile slot is reused by another creature after a load: the record must be dropped (item check), not applied to the newcomer. Test in Task 2 (parse) and Task 5 (load validation).
2. A route to an unreachable goal (the player's last known square across water for a walker, or inside a wall after a door was locked): hnav_route returns 0 and the leg ends "blocked", never a spin. Test in Task 1 and Task 3.
3. The side file absent, truncated or from a newer version: no hunters, no crash, a log line. Test in Task 2.
4. The model answering give_up at step 1, or no answer at all: the hunter keeps hunt.c's and the original's behaviour; no plan is left half-applied. Test in Task 3 (fixed plan "give_up") and Task 6.
5. A save made mid-leg then loaded on another level than the hunter's: its record stays for its own level and does nothing until that level is entered. Test in Task 5.

---

### Task 1: hunternav.c, the pure navigation grid and routes

**Files:**
- Create: `underworld-exhumed/uw1/src/port/sys/hunternav.c`, `underworld-exhumed/uw1/src/port/sys/hunternav.h`
- Test: `underworld-exhumed/uw1/tests/hunternav_test.c` (a plain C program; add it to uo-new `tools\verify.cmd` as `build\hunternav_test.exe`, built by `tools\buildtests.cmd` or the existing test build step uwmind_score uses in `tools\buildmind.cmd`)

**Interfaces:**
- Produces:
  ```c
  #define HNAV_WALK  1   /* an open tile a walker can stand on */
  #define HNAV_WATER 2   /* water: swimmers only */
  #define HNAV_DOOR  4   /* a door tile: passable for hostiles, cost 2 */
  typedef struct { unsigned char cell[64][64]; } HunterNav;            /* bits above */
  typedef struct { unsigned char x[256], y[256]; int n; } HunterRoute;  /* cells from start (excluded) to goal (included) */
  typedef struct { unsigned char x[64], y[64]; int n; } HunterWps;
  /* 1 and a route when the goal is reachable within 4096 visited cells; 0 otherwise. blocked: 64x64 bytes, 1 = never enter (may be NULL) */
  int hnav_route(const HunterNav *nav, int sx, int sy, int gx, int gy, int swims, const unsigned char (*blocked)[64], HunterRoute *out);
  /* waypoints along a route: each the farthest route cell within `reach` squares (Chebyshev) of the previous one whose straight tile path is clear in nav; the goal is always last */
  int hnav_waypoints(const HunterNav *nav, const HunterRoute *r, int sx, int sy, int reach, int swims, HunterWps *out);
  ```
- Consumes: nothing from the game.

- [ ] **Step 1: Write the failing tests** in `hunternav_test.c`: a 64x64 grid built by a helper from an ASCII map (`#` solid, `.` walk, `~` water, `+` door). Tests: `test_route_straight` (route length equals the Manhattan distance down an open corridor), `test_route_through_door` (a corridor cut by `+` is routed through it; with that cell in `blocked` the route fails: return 0), `test_water_only_for_swimmers` (a goal across `~` returns 0 for swims=0 and 1 for swims=1), `test_unreachable_returns_0` (a goal inside `#`), `test_waypoints_within_reach` (every consecutive waypoint pair within 4 squares, the last equals the goal, and a 30-cell corridor yields at most 8 waypoints), `test_limit_4096` (an open 64x64 grid with the goal in the far corner still returns 1: the limit is visited cells, not route length). Each test prints its name and PASS/FAIL; main returns the number of failures.
- [ ] **Step 2: Build and run; expect link failure** (`hnav_route` undefined).
- [ ] **Step 3: Implement `hnav_route` (breadth-first search over the 4 orthogonal neighbours, as PATHFIND.C's flood steps; a door tile costs 2: it is expanded one round later) and `hnav_waypoints` (clear tile path = every cell on the Bresenham line between the two cells has HNAV_WALK or, for swimmers, HNAV_WATER; doors count as clear).**
- [ ] **Step 4: Build and run; expect all six PASS, exit 0.**
- [ ] **Step 5: Add the test to tools\verify.cmd (uo-new) and run run-tests.cmd; expect "all tests passed".**
- [ ] **Step 6: Commit** (exhumed: hunternav.c/.h, the test; uo-new: verify.cmd, the build line) with the verdict quoted.

### Task 2: hunterfile.c, the side file's records in and out

**Files:**
- Create: `underworld-exhumed/uw1/src/port/sys/hunterfile.c`, `hunterfile.h`
- Test: `underworld-exhumed/uw1/tests/hunterfile_test.c` (added to verify.cmd as in Task 1)

**Interfaces:**
- Produces:
  ```c
  enum { HM_SIGHT = 1, HM_BLOCKED = 2, HM_KINDEAD = 3 };                  /* mem kinds */
  enum { LD_KIN_DEAD = 1, LD_DOOR, LD_TAKEN, LD_NOISE, LD_LIGHT, LD_TRAP }; /* ledger kinds */
  enum { HS_GOTO_LAST = 1, HS_GOTO_DOOR, HS_GOTO_HOME, HS_GOTO_WATER, HS_WAIT, HS_SEARCH, HS_FETCH_KIN, HS_FOLLOW_LIGHT, HS_RECOVER, HS_GIVE_UP };
  enum { HE_NONE = 0, HE_DONE, HE_LOST, HE_BLOCKED, HE_WOUNDED, HE_GAVE_UP };
  typedef struct { unsigned char level, mob, alert, step, ended; uint16_t item; uint32_t plan_t; unsigned char steps[4]; } HunterRec;
  typedef struct { unsigned char level, mob, kind; uint32_t t; unsigned char x, y; } HunterMem;
  typedef struct { unsigned char level, kind; uint32_t t; unsigned char x, y, mob; } HunterLedger;
  typedef struct { unsigned char level, x, y; } HunterBlock;
  typedef struct {
      HunterRec rec[20];   int nrec;      /* 2 per level, 10 levels */
      HunterMem mem[320];  int nmem;      /* 16 per hunter */
      HunterLedger led[320]; int nled;    /* 32 per level */
      HunterBlock blk[160]; int nblk;     /* 8 per hunter */
  } HunterSave;
  int hfile_write(const char *path, const HunterSave *s);   /* path.tmp then rename; 1 ok */
  int hfile_read(const char *path, HunterSave *s);          /* 1 ok (also for a missing file: empty), 0 unreadable; unknown lines skipped; a header other than "uwnew 1" = empty + 0 */
  ```
- Consumes: nothing from the game.

- [ ] **Step 1: Write the failing tests**: `test_roundtrip` (fill one of everything, write to a temp path, read back, memcmp equal), `test_missing_file_is_empty` (read a path that does not exist: 1 and all counts 0), `test_unknown_lines_skipped` (a file with a `future X Y` line between valid lines reads the valid ones), `test_bad_header` (`uwnew 2` → 0 and empty), `test_truncated` (a file cut mid-line reads the whole lines before it), `test_tmp_rename` (after a write no `.tmp` remains).
- [ ] **Step 2: Build and run; expect link failure.**
- [ ] **Step 3: Implement** with fprintf/fscanf-free parsing (strtok on lines; every number range-checked: level 1-9, mob 0-255, kind within its enum, x/y 0-63; a line failing a check is skipped).
- [ ] **Step 4: Run; expect all PASS.**
- [ ] **Step 5: Add to verify.cmd, run run-tests.cmd, commit with the verdict.**

### Task 3: hunter.c, the hunter layer with a fixed plan (no model yet)

**Files:**
- Create: `underworld-exhumed/uw1/src/port/sys/hunter.c`, `hunter.h`
- Modify: `underworld-exhumed/uw1/src/port/sys/hunt.c` (port_hunt_goal: the hunter's step first; port_hunt_noise: notice; spotted() and finish(): tell hunter.c; expose `hunt_begin`, `hunt_arrive`, `hunt_stand_face` as non-static wrappers of begin/arrive/stand+face for hunter.c)
- Test: a hidden run script `work\hunters\plan1.script` + `tools\huntercheck.py` (uo-new) that runs it and greps the log

**Interfaces:**
- Produces (hunter.h):
  ```c
  int  port_hunter_step(void);                       /* for meptr in a plan: 1 when it ran this step (hunt.c then returns 1) */
  void port_hunter_notice(int kind, int x, int y, int mob);   /* LD_* kinds; x, y tile; mob 0 when none */
  void port_hunter_level(int entering);              /* ChangeLevel: 1 after the new level is loaded, 0 before the old is saved */
  int  port_hunter_save(const char *dir);            /* dir ends with a backslash; 1 ok */
  int  port_hunter_load(const char *dir);
  int  port_hunter_on(void);                         /* the setting, the model, smart-hunting, not hidden (or UWPORT_HUNTER_PLAN) */
  ```
- Consumes: Task 1's hnav_*, Task 2's hfile_* and types; hunt.c's `void hunt_begin(unsigned char goal, unsigned char x, unsigned char y, const char *why)`, `int hunt_arrive(void)`, `void hunt_stand_face(unsigned char x, unsigned char y)`, `int port_mind_ready(void)`; AI.C globals via critter.h as hunt.c uses them.
- State: `static HunterSave S;` plus per-hunter runtime `{ HunterNav nav (per level, rebuilt in port_hunter_level(1)), HunterRoute route, HunterWps wps, int wp, uint32_t leg_t, wait_t, ask_t }`.

- [ ] **Step 1: Write the check** `tools\huntercheck.py plan1`: copies work\in2-4 to a fresh home, runs uw1port.exe hidden with `UWPORT_HUNTER_PLAN=goto_last,wait,search UWPORT_AILOG=1 EXHUME_TEST_CHEATS=1 UWPORT_LLM=0` and a script that restores a game, `cheat goto 1 31 5`, then `cheat anger N` on a goblin (the existing cheat) and a noise (`key` for a weapon swing), 60 s; asserts the stdout has, in order: `AI: hunter #N picked`, `AI: hunter #N noticed`, `AI: hunter #N plan: goto_last, wait, search`, `AI: hunter #N leg 1`, `AI: hunter #N step done: wait` or `plan ended`. A second script `plan_giveup` with `UWPORT_HUNTER_PLAN=give_up` asserts `plan ended: gave up` and no `leg` line. A third, `plan_blocked`, with a goal across water (`cheat goto` the player onto an island of level 1) asserts `plan ended: blocked`.
- [ ] **Step 2: Run the check; expect failure (no hunter lines).**
- [ ] **Step 3: Implement hunter.c:** the pick in port_hunter_level(1) (spec 3, logged "picked"); the grid from Map_GetAddr (type != 0 → WALK; the water floor textures as hunt.c's swimmers test them → WATER; Tile.door → DOOR; height steps above the flood's limit, read from PATHFIND.C's flood_path step test, not passable); the ledger and noticing (hearing: mycst->hearing * loudness / 16 as port_hunt_sense; sight: line_of_sight within mycst->sight); the plan state machine: `plan_start(steps)` from UWPORT_HUNTER_PLAN when set, else nothing yet (Task 6 adds the model); legs = hnav_route + hnav_waypoints(reach 4) then hunt_begin(GOAL_HUNT, wp) per waypoint with HuntState.look search disabled for hunters (a `hunter` flag in HuntState); HS_WAIT = hunt_begin(GOAL_AMBUSH, here) with hunt_stand_face toward the goal for 45 s; HS_SEARCH = hunt.c's HUNT as is for 30 s; HS_FETCH_KIN = route to the nearest kin (hunt.c's facts() kin list) then the rally write as mind_act does; HS_RECOVER = goto_home then wait until hp >= avghit / 2; HS_GIVE_UP = finish. A leg whose hunt_arrive() reports the path gave up is retried from the next waypoint once, then the cell is added to blk and the route rebuilt; a second failure ends the plan HE_BLOCKED. spotted() → the plan ends HE_LOST with the sighting remembered (HM_SIGHT). Every transition logged `AI: hunter #N ...` through port_ai_say.
- [ ] **Step 4: Run the three checks; expect PASS.**
- [ ] **Step 5: run-tests.cmd (hunters off in all existing checks: unchanged replays); commit with the verdicts.**

### Task 4: hunt.c hooks are byte-neutral with hunters off

**Files:**
- Modify: `hunt.c` (only what Task 3 touched)
- Test: the existing replay checks in tools\verify.cmd

- [ ] **Step 1: Run run-tests.cmd with the Task 3 build; expect "all tests passed" (the replays are byte-identical: port_hunter_on() is 0 in them).**
- [ ] **Step 2: Run one replay with `UWPORT_HUNTER_PLAN` set but `--hidden`'s model off: expect the hunters off too (spec 7: off in every hidden run unless the plan knob is set: the knob is the only exception, documented in hunter.h).**
- [ ] **Step 3: Commit if anything changed; else note in the Task 3 commit.**

### Task 5: GAMEWRAP.C hooks and the side file in play

**Files:**
- Modify: `underworld-exhumed/uw1/src/game/GAMEWRAP.C` (SaveGame before its copy_dir: `port_hunter_save("SAVE0\\")`; RestoreGame after its copy_dir and GetLevel: `port_hunter_load("SAVE0\\")`; ChangeLevel: `port_hunter_level(0)` before SaveLevel, `port_hunter_level(1)` after GetLevel; all under `#ifndef __TURBOC__` with local prototypes, like port_save_desc)
- Test: `tools\huntercheck.py save` (uo-new)

- [ ] **Step 1: Write the check**: run A as Task 3's plan1 but with the save page keys (F5, Enter) at a time the log shows `leg 2`; assert `home\SAVE1\UWNEW.DAT` exists, has `uwnew 1`, a `hunter 1 N ...` line with STEP 1 (the second step, 0-based) and at least one `mem`. Run B loads that slot (the menu keys) and asserts `AI: hunter #N resumed: step 2 of 3` within 10 s and then a `leg` line. Run C edits the saved file's hunter line to another ITEM and loads: asserts `AI: hunter record dropped (mob N holds another creature)`. Run D loads the save after `cheat goto 2 ...` moved the player to level 2 before saving (a second save): asserts no hunter line until `cheat goto 1 ...` returns, then `resumed`.
- [ ] **Step 2: Run; expect failures (no file).**
- [ ] **Step 3: Implement the three hooks and port_hunter_save/load (write at port_hunter_level(0) too, so a level change carries the state; load validates every rec against the mobile's item and hp).**
- [ ] **Step 4: Run A-D; expect PASS. Confirm the hooks sit inside `#ifndef __TURBOC__` blocks (read the diff); never run exhumed's own test suites (the user's standing rule).**
- [ ] **Step 5: run-tests.cmd; commit with the verdict.**

### Task 6: the model's plan, question kind 6

**Files:**
- Modify: `hunter.c` (plan_ask), `hunt.c` (mind_collect delivers kind 6 answers to `port_hunter_answer(int mob, int step, int pick)`), `port/win32/uwmindhost.c` (uo-new; no interface change, only the SYSTEM sentence "You plan ahead: answer each step in turn." appended)
- Test: a visible run judged by the user (spec 8) plus `tools\huntercheck.py ask` hidden with the model on (the user runs it: the model loads in a hidden run only with `UWPORT_LLM=1` set explicitly for this check)

**Interfaces:**
- Produces: `void port_hunter_answer(int level, int mob, int step, int pick);` (pick is the index into that step's choices; -1 none)
- Consumes: `port_mind_ask(level, mob, 6, user, grammar)` as hunt.c calls it; the answer arrives through mind_collect with kind 6 and `ans` = `goal=<word>`; hunter.c maps the word to HS_*.

- [ ] **Step 1: Write the check** `ask`: with the model on and `UWPORT_HUNTER_PLAN` unset, the log shows `AI: hunter #N asks: ...` with the facts and choices, then `AI: hunter #N plan: a, b[, c, d]` where every word is in the vocabulary and every step was offered (goto_water only to swimmers, fetch_kin only with kin within hearing, recover only when wounded, goto_last/goto_door only with a sighting).
- [ ] **Step 2: Implement plan_ask**: port_mind_ask takes one question per creature (dedup by level+mob), so the steps are asked in turn: step 1 alone; on its answer step 2 with "So far: step 1 = X." prepended, and so on, until give_up or four steps; the 20 s rate applies per plan, not per step; the plan starts when complete, or when a step answers give_up after at least one step. (Batching the four steps is a later improvement, not now.)
- [ ] **Step 3: User's visible run on level 1 and 3 with Ctrl+L; the user judges the plans.**
- [ ] **Step 4: run-tests.cmd; commit with the verdict and the user's word.**

### Task 7: the route check over all nine levels, docs, dist

**Files:**
- Create: `tools\hunterroutes.py` (uo-new): drives `build\uwtour.exe` (or a small C tool `tests\hunterroutes.c` linking hunternav.c and reading each level's tilemap from the user's LEV.ARK through the same reader hunt.c's original_holder uses) to build each level's grid, pick the hunters by the spec's rule, and route each to the level's entrance; prints per level the hunters, route lengths, waypoint counts, and `UNREACHABLE` where hnav_route fails.
- Modify: `LLM_NPC_AI.md` (a "Hunters" section: what exists, the vocabulary, the side file, the checks), `NOTES.md` (one line), `tools\makedist.py` (nothing new to ship: hunter.c is in the exe; confirm), `README` text in makedist (one sentence: the hunters, llm_hunters=0 turns them off).

- [x] **Step 1: Write and run hunterroutes.py; expect nine levels built, no UNREACHABLE for walkers' own regions (an island hunter is expected unreachable: listed, not failed).**
  Done 2026-11-13 as `tools\huntercheck.py routes` (the game's own grid via UWPORT_HUNTER_ROUTES, judged against a
  reference of the original walker's rule from the level file) rather than a separate tool. Found on the way: the
  grid closed lava and ignored bridges (PATHFIND.C: lava costs 2 danger, a solid object's top is walked), routes were
  capped at 256 cells, and three levels were never entered (no creature near the player stepped): fixed.
- [x] **Step 2: Add it to verify.cmd (fast: no model).** (two runs, about 2.5 minutes)
- [ ] **Step 3: Docs; run-tests.cmd; commit with the verdict; makedist; dist refreshed.**
