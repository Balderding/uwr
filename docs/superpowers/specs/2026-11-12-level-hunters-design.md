# Level hunters: creatures that plot, with model-written plans, routes, memory and a side file

Date: 2026-11-12. Author: the second uwr session (orchestrating); builder: the uwr session.
Status: design approved in chat by the user (approach 1 of 3); this is the written spec for review.

## 1. Purpose

Give each level one or two **hunters**: clever hostile creatures that start out ordinary and, once they notice the
dungeon has changed, plot to kill the adventurer: a short plan from the language model (2-4 steps from a fixed
vocabulary), carried out with the original's movement and combat, with a memory of what they learned, all of it
kept across a save and load. The original's rules and numbers are untouched; everything here is optional and off in
every proof and off-screen check.

What stays as it is: hunt.c's goals 13-15 (HUNT, INVESTIGATE, AMBUSH), its one-choice questions for ordinary
creatures, uwmind.dll / uwmindhost.c (the model, the worker, uwm_score), the whoami rule (named creatures never take
part), the no-cheating rule (a creature acts only on what it perceived).

## 2. Boundaries and files

- **New: `uw1/src/port/sys/hunter.c`** (underworld-exhumed, port only, like hunt.c): the hunter layer. Owns the
  hunter pick, the ledger, the navigation graph, routes, plans, memory, and the side file. Exposes to hunt.c:
  `int port_hunter_step(void)` (called by port_hunt_goal first for a hunter in a plan; 1 when it ran the step),
  `void port_hunter_notice(int kind, int x, int y, int mob)` (hunt.c and the hooks report events),
  `void port_hunter_level(int entering)` (ChangeLevel), `int port_hunter_save(const char *dir)`,
  `int port_hunter_load(const char *dir)`.
- **hunt.c** gets four small hooks: a hunter in a plan is stepped by hunter.c before hunt.c's own goal handling
  (port_hunt_goal); port_hunt_noise also calls port_hunter_notice; `spotted()` and `finish()` tell hunter.c when a
  plan's leg ends; mind_collect delivers plan answers (kind 6) to hunter.c.
- **uwmindhost.c**: unchanged interface; a plan is 2-4 batched questions of kind 6 with their own choices (one per
  step), answered by uwm_score as today. No new DLL code.
- **GAMEWRAP.C** (byte-matched, port-only block): SaveGame calls port_hunter_save(slot dir) after its own files;
  RestoreGame calls port_hunter_load before returning; ChangeLevel calls port_hunter_level. Under
  `#ifndef __TURBOC__`, as the existing port-only blocks.
- **AI.C**: no change. The hunter runs inside goals 13-15 through port_hunt_goal, which already exists in the
  default branch of critter_mv.

## 3. Hunters and noticing

**Pick (port_hunter_level(1), first entry of a level, or a load with no record for it):** candidates are mobiles
with hp > 0, ATT_HOSTILE, whoami == 0, wits (Creature.attr[2]) >= 10, in the level's largest hostile race (count
by Creature.race), preferring the AMBUSHERS kinds, then the highest wits, then the lowest mobile index. Up to two.
Written to the side file at once (the record is `level, mob, item`) so the pick is stable; a later entry re-reads
it and re-validates (mob alive, same item), replacing a dead or reused slot with a new pick only when fewer than one
hunter remains.

**Ledger (per level, in memory, saved):** up to 32 entries `{time, kind, x, y, mob}` of things that happened:
`LD_KIN_DEAD` (hunt.c's port_hunter_notice from build_corpse's caller: a hostile creature died), `LD_DOOR` (a door
opened or broken: the port's door hooks in PATHFIND.C's try_to_open_door and USEITEMS.C's door use, player only),
`LD_TAKEN` (the player took an object off the floor: INTERACT.C's take path), `LD_NOISE` (port_hunt_noise's
player-made noises), `LD_LIGHT` (port_hunt_sense's light sighting), `LD_TRAP` (TRIGGER.C UseTrap by the player).
Entries older than 10 minutes of game time are dropped.

**Noticing (no cheating):** a hunter notices an entry only when it could: the entry within its hearing for noises
and traps, within its sight with line_of_sight for a corpse, a door or a missing object (checked when the hunter
walks within 4 squares of the place), the light as port_hunt_sense sees it. The first noticed entry sets the hunter
`alert` (saved) and starts its first plan. While alert, every new noticed entry is a fact for the next question.

## 4. The plan and the model

**Vocabulary (steps):** `goto_last` (the player's last known square), `goto_door` (the nearest door on the way
toward the last known square), `goto_home`, `goto_water` (swimmers), `wait` (stand facing the way the player would
come, up to 45 s, alert rate), `search` (hunt.c's HUNT search around a square, 30 s), `fetch_kin` (go to the
nearest kin within 20 squares, then both return: the kin gets a rally as today), `follow_light` (INVESTIGATE
toward the last light), `recover` (go home, wait until hp >= avghit / 2), `give_up`.

**The question (kind 6):** system prompt as today plus one sentence: "You plan ahead: answer each step in turn."
User text: who it is (hunt.c's `who`), its memory lines (section 5), the facts it noticed (the ledger entries,
in words, with how long ago and where relative to it), its state (hp, where it is, whether kin are within
hearing), then "Step N of your plan. Choices: ..." with the steps allowed now (goto_water only for swimmers,
fetch_kin only with kin within hearing, recover only when wounded, goto_last / goto_door only with a last known
square). Steps 2-4 are asked in the same batch with "So far: step 1 = X." prepended, so one decision costs one
batched decode; a step answered give_up ends the plan there.

**When asked:** on becoming alert; when a step ends (arrived, timed out, failed) and the plan is finished; when a
new entry is noticed during a plan (the plan is dropped, a new one asked with the new fact first); never more than
once per 20 s per hunter, never while the hunter sees the player (then the original's attack runs, as spotted()
does today, and the plan is dropped). A plan older than 3 minutes of game time is dropped. No answer (model off,
stale, failed) = the hunter stays with hunt.c's and the original's rules, as today.

## 5. Executing: route, waypoints, memory

**Navigation graph (per level, built at port_hunter_level(1), rebuilt after a load):** 64x64 cells; passable when
Map_GetAddr(x, y)->type != 0 and the height difference to the neighbour is small enough to walk (the same test
PATHFIND.C's flood uses for a step, read from its code), water passable for swimmers only, lava and the void never,
a door tile passable for hostiles (they open doors) unless remembered as locked by this hunter. Cost 1 per step,
2 across a door. Routes by breadth-first search from the hunter to the goal square, at most 4096 cells; the result
is reduced to waypoints: the farthest cell on the route that is within 4 squares of the current one with a clear
tile path (the original flood's reach), repeated, so each leg is one crit_head_for_loc the original can finish.

**Legs:** a leg is begin(GOAL_HUNT, wx, wy, ...) with HuntState.look cleared and HUNT's own search disabled for
the hunter (a flag in HunterState); arrive() ends the leg; a leg whose path gave up (OBJ_B18_6) is retried once
from the next cell, then the door or cell is remembered blocked and the route rebuilt. `wait` = AMBUSH's waiting
code (face, rate 4, springs when the player comes within 2 squares). `search` = HUNT as today. `fetch_kin` = a
route to the kin, then the rally written as mind_act does. Seeing the player at any point = spotted()'s attack.

**Memory (per hunter, saved):** up to 4 sightings `{time, x, y}` (the player's last known squares: from losing
him in a fight, a light, a loud noise made by him), up to 8 blocked cells/doors, up to 4 kin deaths `{time, x,
y}`, the last plan's steps and how it ended (`done`, `lost him`, `blocked`, `wounded`, `gave up`). Written into the
question as short sentences ("3 minutes ago you lost the adventurer by the north door", "the door at the east
corridor is locked", "your plan to wait by the door ended: he never came").

## 6. The side file

Path: `SAVE0\UWNEW.DAT`, beside the running game's LEV.ARK and PLAYER.DAT (GAMEWRAP.C: SAVE0 is the game in
progress; SaveGame copies SAVE0\*.* into SAVEn and RestoreGame copies SAVEn\*.* back, so the side file rides along
with every save and restore and no copy code changes). Written by port_hunter_save at every level change (before the
level is saved) and in SaveGame just before its copy_dir; read by port_hunter_load in RestoreGame after its copy_dir.
The game's own files are untouched. Format: text, one record per line, so it can be read by eye and by tests:

```
uwnew 1
hunter L MOB ITEM ALERT PLAN_T STEP S1 S2 S3 S4 ENDED
mem    L MOB kind t x y
ledger L kind t x y mob
graphblock L x y          (a remembered blocked cell)
```

Read tolerantly: unknown lines skipped, a missing file = no hunters yet. Validation on load: a hunter record whose
mobile is dead or carries another item is dropped. Written atomically (write to .tmp, then rename).

## 7. Safety, limits, settings

- Off when the model is off (llm=0, UWPORT_LLM=0), off in every hidden/off-screen run and proof, off unless
  smart-hunting is on. Setting `llm_hunters=0|1|2` (default 2) in uw1port.cfg.
- Named creatures (whoami) never hunters or kin called by hunters. Hunters never leave their level.
- The hunter's plan never changes a rule number: movement, combat, doors, rolls are the original's.
- A hunter that dies ends its record; the slot may be re-picked at the next level entry if fewer than one remains.
- CPU: the graph build is a 4096-cell BFS per route, done on the game's thread only when a route is asked (at most
  once per leg); the full graph is built once per level entry (64x64 pass).
- Log: every notice, plan, leg and ending as "AI: hunter #N ..." lines on the Ctrl+L log and the jsonl.

## 8. Testing

- `UWPORT_HUNTER_PLAN="goto_last,wait,search"` (a check's) feeds a fixed plan without the model, so a hidden run
  exercises the route builder, the legs and the side file deterministically.
- Route builder: a tool run over all nine levels (uwtour's level loads) that routes from every hunter candidate to
  the level's entrance and reports unreachable goals and the longest route; all nine must build.
- Side file: a hidden run with a fixed plan saves mid-plan (the game's own save page keys), the save is reloaded
  by a second run, and the log shows the plan resumed at the same step.
- No-regression: run-tests.cmd (every existing check) unchanged; a replay with the hunters off is byte-identical.
- The model's plans: a visible run on level 1 and 3 with the Ctrl+L log, judged by the user.

## 9. Out of scope

Free-form talk, factions fighting each other, hunters crossing levels, a director that sees everything, any change
to the model DLL or to the original's routines beyond the three port-only hooks in GAMEWRAP.C.
