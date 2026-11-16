# Creature AI Levels Implementation Plan

> **For agentic workers:** the user's standing rules replace the skill's defaults here: no subagents (work in
> the session), and proof is "it compiles and the exe launches" (no test suites; the user judges play). Steps use
> checkbox (`- [ ]`) syntax for tracking.

**Goal:** one "Creature AI" setting (Original / Devious / Monstrous) whose smarter levels are decided by code, not
a language model.

**Architecture:** hunt.c and hunter.c keep their goals, hooks and answer paths. A new pure file, cunning.c, turns
plain facts into the same answer codes the model gives today; hunt.c calls it where it now rolls a die or asks the
model. A second new file, pack.c, holds the Monstrous group behaviours as sub-modes of goals 13 to 15.

**Tech Stack:** C17, clang (O:\appsnotonc\llvm), Exhume's portbuild through tools\portwin32.py, the Win32 backend.

**Spec:** docs\superpowers\specs\2026-11-16-creature-ai-levels-design.md

## Global Constraints

- No rule number changes: damage, hit chances, health, speed, rolls, the clock. Only choices.
- Only the original's own actions: attack, shoot, cast, flee, cornered fight, walk to a place, stand.
- A creature uses only what it saw or heard or what kin shouted in its hearing; never the player's real square.
- Named creatures (`meptr->whoami != 0`) keep the original's rules alone, friendly ones greet and warn only.
- Level Original: none of this runs (ENHANCED(ENH_SMART_HUNTING) false); replays (`rp_request == 2`) likewise.
- No new goal value; no save-format change; all state runtime only.
- New files in underworld-exhumed\uw1\src\port\sys. No new hook in AI.C unless Task 10 proves it needed; then
  behind ENHANCED(ENH_SMART_HUNTING) and noted in NOTES.md.
- Other sessions edit both repositories: stage and commit only this plan's files, by explicit path
  (`git commit -m ... -- <paths>`, after `git diff --cached --stat`), and only when the user says to commit.
  In port\win32\uwfront.c touch only the menu rows.
- Build: `bash work/locked.sh <python> -I tools/portwin32.py` (python: work\venv\Scripts\python.exe). Never build
  while the user's game is running. Pack: `python -I tools\makedist.py` (game closed).
- Proof per task: the build ends without error and dist\UW1\uwr.exe starts to the menu in a hidden run. Then the
  reply ends with only the run command: `O:\uw\uo-new\dist\UW1\uwr.exe`
- Scratch files go in O:\uw\uo-new\work. Nothing on C:. Nothing deleted.
- Text shown to the player is never red.

## Review Focus

1. A named quest creature: must never flee, call, kite or join a pack (every new entry point tests whoami first).
2. A pack member dies or leaves the level while others wait for it: the group must still go in (the 10 s limit).
3. A load or level change mid-behaviour: sub-mode state is gone; the creature returns to its own goal as hunt.c's
   "a hunt from before a load" does.
4. The level switched to Original mid-fight: creatures in goals 13 to 15 are released by the existing
   original_holder / discard path at their next step; no creature is left standing for good.
5. A swimmer or flier told to run to kin it cannot reach: the path gives up (arrive() returns 2) and it fights
   where it is.

Each of these is a code path, not a test: the owning task lists it under "Must hold".

## File Structure

| File | Responsibility |
| --- | --- |
| port\sys\cunning.h, cunning.c (new) | settings (level, parts, decisions-by), the trait table, the rulebook as pure functions of a facts struct |
| port\sys\pack.h, pack.c (new) | Monstrous group behaviours, the call (sound, message, rally, relay), message lines |
| port\sys\hunt.c (modify) | fills the facts, calls the decider instead of the dice, steps pack sub-modes |
| port\sys\hunter.c (modify) | hunters run without a model: plan step and reconsider from cunning.c |
| port\win32\uwfront.c (modify) | the Esc menu rows |
| NOTES.md, LLM_NPC_AI.md (modify) | what was added, the hooks, how to watch it (Ctrl+L) |

---

### Task 1: the setting and its menu row

**Files:** create cunning.h, cunning.c; modify port\win32\uwfront.c (menu rows only); add the two new files to
whatever list portbuild compiles port\sys from (exhume.toml or its glob: check first).

**Interfaces (produced):**
```c
enum { AI_ORIGINAL, AI_DEVIOUS, AI_MONSTROUS };
int port_ai_level(int v);            /* v < 0 reads, else sets 0..2; kept as creature_ai in uw1port.cfg; sets
                                        ENH_SMART_HUNTING on for 1 and 2, off for 0, at once */
enum { AIP_HITRUN, AIP_CALLS, AIP_COURAGE, AIP_FALLBACK, AIP_TOGETHER, AIP_RELAY, AIP_MESSAGES, AIP_COUNT };
int port_ai_part(int part, int v);   /* v < 0 reads, else sets 0/1; default 1; kept as ai_part_<n> */
int port_ai_by_model(int v);         /* v < 0 reads, else sets 0/1; default 0; kept as ai_by_model */
int cun_monstrous(int part);         /* level == 2 and the part on */
```

- [ ] First run with no creature_ai in the settings: 1 if smart-hunting is on in the player's settings, else 0.
- [ ] Menu: "Creature AI" (Original / Devious / Monstrous; help: "How cunning creatures are. Original is the
  game as shipped."); "Decisions by" (Rules / Language model), shown when the level is not Original and model
  files exist; six part rows shown on Monstrous only: "Hit and run", "Calls for help", "Pack courage",
  "Fall back when hurt", "Strike together", "Calls passed on"; "AI messages" on both smarter levels. Follow
  MO_HUNTERS's pattern (enum entry, row table entry, value text, step handler).
- [ ] Build; hidden launch; the rows appear and hold their values across a restart.

### Task 2: the trait table and the rulebook for hostile creatures

**Files:** cunning.h, cunning.c.

**Interfaces (produced):**
```c
struct cun_trait { unsigned char pack_need, nerve8, calls, kites, relays; };  /* nerve8: falls back below
                                                     nerve8/8 of its average health; 0 never */
const struct cun_trait *cun_trait(unsigned item);    /* by creature type (OBJ_ITEM), named or not */

struct cun_facts {
    int wits, hp, avghit, ambusher, kin, kin_fighting;
    int dist;            /* squares to where the adventurer is or was known; -1 nothing known */
    int secs;            /* seconds since that knowledge; 0 now */
    int by_player;       /* a noise known to be the adventurer's (kind 2) */
    int busy;            /* that noise was a door, lever or spell */
    int light_reach;     /* kind 2: a light seen, its reach; else 0 */
    int look, drawn, plight;   /* how the adventurer looked (0-3, -1 not seen), weapon out, their light's reach */
    unsigned offered;    /* bit per MIND_ code on offer */
};
enum { MIND_NONE, MIND_HUNT, MIND_INVESTIGATE, MIND_AMBUSH, MIND_BACK, MIND_ATTACK, MIND_FLEE, MIND_RALLY };
                         /* moved here from hunt.c, values unchanged */
int cun_seen(const struct cun_facts *f, int *rule);     /* kind 4: ATTACK, AMBUSH or FLEE */
int cun_lost(const struct cun_facts *f, int *rule);     /* kind 1: HUNT, AMBUSH, RALLY or BACK */
int cun_sensed(const struct cun_facts *f, int *rule);   /* kind 2: ATTACK, INVESTIGATE, AMBUSH, RALLY or BACK */
```

- [ ] Rules 1 to 7 of O:\appsnotonc\finetune\data\SYSTEM.txt, first match wins, never returning a code not in
  `offered`. Pinned values: badly wounded = hp * 3 < avghit; dim-witted = wits < 5; average wits = wits >= 10;
  cold trail = secs >= 300 or dist > 10; prey = look >= 2; ambush clause distances: adventurer 6 squares or more
  away; the light clause only when dist > plight (the spec's ruling); kin fighting count as beside it.
  The four rulings from the spec's "The decider" section apply.
- [ ] Trait defaults from OBJECTS.DAT fields at first use (relays = wits >= 10; kites = first weapon a missile
  class or the type casts; calls = wits >= 5 and not undead), then the overrides: spiders, rats, bats
  pack_need 2; goblins, lizardmen, mountainmen, outcasts nerve8 4; trolls, golems, skeletons, ghosts, ghouls,
  headless, wisps, elementals nerve8 0 and calls 0.
- [ ] Build (the file compiles into the port; nothing calls it yet).

### Task 3: hunt.c decides by the rules

**Files:** hunt.c.

**Consumes:** Task 1's settings, Task 2's cun_seen / cun_lost / cun_sensed.

- [ ] One static helper fills a cun_facts for the creature being run from what mind_ask and seen_ask already
  compute (facts(), is_ambusher(), port_player_look(), the noise's by_player and fx, door_by()).
- [ ] port_hunt_lost and port_hunt_sense: the `rand() % 2` ambush choice is replaced by the decider's answer
  (ambush only when ambush_spot finds a square, else the next rule's answer); the answer is acted on at once
  through the same code mind_act runs.
- [ ] port_friend_step's hostile branch: where seen_ask is called, the decider's answer is acted on through
  seen_act's code.
- [ ] The model is asked only when port_ai_by_model(-1) is 1; its answer, when it comes, still overrides.
- [ ] Log line through dbg(): "rules: <choice> (rule N)".
- [ ] Must hold: whoami creatures skipped as now; level Original untouched.
- [ ] Build; hidden launch.

### Task 4: friendlies and hunters without a model

**Files:** cunning.h, cunning.c, hunt.c, hunter.c.

**Interfaces (produced):**
```c
enum { FR_NONE, FR_GREET, FR_FOLLOW, FR_HOME, FR_WARN };     /* moved from hunt.c, values unchanged */
int cun_friend(int wits, int dist, int goal, int sees_foe, int named, int *rule);   /* rule 9; FR_NONE = ignore */
/* the hunter's plan: tools\hunter_policy.py of O:\appsnotonc\finetune ported as it stands */
struct cun_hunt { int wits, wounded, swims, kin, look, last_secs, last_dist, has_last,
                  light_secs, blocked, lost_before; unsigned allowed; const unsigned char *steps; int nsteps; };
int cun_plan_step(const struct cun_hunt *h);                 /* the step's code as hunter.c numbers them */
int cun_reconsider(int wounded, int busy_or_hurt_within4, int kin_dead, int light_out);  /* keep / rush / new */
```

- [ ] hunt.c: friend_ask's place calls cun_friend unless decisions are by the model.
- [ ] hunter.c: port_hunter_usable returns 1 and port_hunter_on is true without a model when decisions are by
  rules; plan_ask and recon_ask take cun_plan_step / cun_reconsider's answer through port_hunter_answer's path.
- [ ] Build; hidden launch. **Devious is complete here: makedist, and hand the user the run command.**

### Task 5: pack.c's base, the call, and messages

**Files:** pack.h, pack.c; hunt.c (HuntState gains `unsigned char mode; uint32_t mode_t; unsigned char mx, my;`).

**Interfaces (produced):**
```c
enum { PM_NONE, PM_TO_KIN, PM_GATHER, PM_HOLD_RANGE };       /* sub-modes of goals 13-15 */
int  pack_kin(int within, int *mobs, int max, int *fighting, int *nearest);  /* hostile kin of the creature
                                                     being run within `within` squares (hunt.c's facts scan) */
void pack_note(const char *fmt, const char *name);  /* a message-scroll line, when AIP_MESSAGES; at most one of
                                                     the same kind every 5 s */
int  pack_call(unsigned char fx, unsigned char fy); /* the creature being run calls: its sound at its square
                                                     (play_effect_on_mobile), "The %s shrieks for help.", every
                                                     kin within its hearing rallied to (fx, fy) by hunt_rally;
                                                     returns the number called; at most one call a creature a minute */
```

- [ ] Calls for help (AIP_CALLS): in port_friend_step's hostile branch, a creature that has just come to know
  where the adventurer is, whose trait calls, with kin within hearing and none fighting, calls, then attacks.
- [ ] Calls passed on (AIP_RELAY): a creature rallied by a call whose trait relays calls once itself; a call
  carries a hop count and is not repeated past 2.
- [ ] Must hold: whoami creatures neither call nor are called.
- [ ] Build; hidden launch.

### Task 6: pack courage

**Files:** pack.c, hunt.c.

- [ ] (AIP_COURAGE) A creature with pack_need > 0 that knows where the adventurer is and has fewer than
  pack_need - 1 kin within 3 squares: critter_set_goal(GOAL_FLEE, 1). When pack_kin(3) reaches pack_need - 1 and
  the creature is fleeing for this reason: every one of them gets GOAL_ATTACK and one "The %ss turn on you." line.
- [ ] The original's flee already becomes the cornered fight when it has no way out: leave that alone.
- [ ] Rule 3 first: an adventurer who looks badly hurt is attacked whatever the numbers.
- [ ] Build; hidden launch.

### Task 7: fall back when hurt

**Files:** pack.c, hunt.c.

- [ ] (AIP_FALLBACK) A creature in GOAL_ATTACK with hp * 8 < avghit * nerve8, kin within hearing, the adventurer
  not looking badly hurt: begin(GOAL_HUNT) toward the nearest kin's square with mode PM_TO_KIN and the line
  "The %s runs to its kin."; on arriving (or the path giving up, or 15 s) it attacks again from there.
  Once per creature per fight.
- [ ] Must hold: Review Focus 5 (the unreachable kin).
- [ ] Build; hidden launch.

### Task 8: strike together

**Files:** pack.c, hunt.c.

- [ ] (AIP_TOGETHER) A rallied creature heads for a square 3 to 4 squares short of the called place (ambush_spot's
  search, without the corner bonus) in mode PM_GATHER; the group goes on to the place when every creature called
  is within 2 squares of its own gather square, or 10 s after the call, whichever is first.
- [ ] Must hold: Review Focus 2.
- [ ] Build; hidden launch.

### Task 9: hit and run

**Files:** pack.c, hunt.c; AI.C only if needed.

- [ ] Read AI.C's crit_offense (lines 339 to 372 and 585 to 607) and crit_missile_attack first: find whether a
  kiting creature can be held from closing by our goals alone.
- [ ] (AIP_HITRUN) A creature whose trait kites and that knows where the adventurer is: mode PM_HOLD_RANGE
  (goal 15, standing, facing him) while he is beyond its ranged reach; once the original's ranged attack has
  fired (the creature's sequence shows it), with kin within hearing it goes PM_TO_KIN with "The %s falls back
  to its kin."; with none it backs off to its range and holds again.
- [ ] If a hook in crit_offense is the only way: one call behind ENHANCED(ENH_SMART_HUNTING), noted in NOTES.md.
- [ ] Build; hidden launch.

### Task 10: notes, and the packed game

- [ ] NOTES.md: the setting, the files, any new hook. LLM_NPC_AI.md: a "Rules decide" section pointing at the
  spec. makedist's readme sentence for the Creature AI row.
- [ ] makedist (game closed); hidden launch of dist\UW1\uwr.exe.
- [ ] Record in O:\appsnotonc\finetune\HANDOFF-2026-11-16.md that the fine-tune is set aside for coded rules,
  with label_api.py and the gold check's result.

## Not in this plan

The creatures' own call sounds as new Roland timbres (spec, "Later part"): its own plan after this one, since
each sound needs the user's ear before it goes in.
