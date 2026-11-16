# Creature AI levels: Original, Devious, Monstrous (design, 2026-11-16)

Status: awaiting the user's approval. Nothing is built.

## Intent

The user wants creatures that are far more cunning than the original's, chosen by one setting with three levels, and
built as ordinary game code instead of a language model. The fine-tune work showed why: the model was only being
taught to imitate a rulebook we wrote ourselves, so code can follow that rulebook exactly, instantly, and on any card.

The user's own words for Monstrous: "extremely cunning... would wait for you to get in range and range attack then run
to its kin. Scream for help and then attack you. The spiders would run away until they outnumber you by 2 or when
cornered", "stuff like that".

## The three levels

| Level | What creatures do |
| --- | --- |
| Original | UW.EXE's AI and nothing else. None of our creature code runs. |
| Devious | Today's smart-hunting (hunt where they lost you, look at noises and light, ambush, call kin that are near, level hunters), with every choice made by the rulebook in code. |
| Monstrous | Devious, plus pack cunning: hit-and-run shooters, calls for help, packs that avoid you until they have the numbers, the wounded falling back to kin, packs striking together, calls passed on from creature to creature. |

## Goals

- One Esc-menu row, "Creature AI": Original / Devious / Monstrous, taken at once and kept in the settings.
- Devious and Monstrous need no language model and no graphics memory.
- Each creature type has its own style, held as numbers in one table (the Descent and Halo way).
- The player can tell what a creature just decided: a sound and a line in the message scroll for the decisions that
  matter (a call for help, a pack turning to fight, a creature running to its kin).
- Each separable Monstrous part has its own on/off row (the user's standing rule).

## Non-goals

- No change to any rule number: damage, hit chances, health, speed, the rolls, the clock. Only choices change.
- No new attacks, spells or movement. Every action is one the original already has (attack, shoot, cast, flee, fight
  when cornered, walk to a place, stand).
- Named creatures (their own conversation: quests, duels) keep the original's rules alone, as now.
- The original's rule that only creatures near the player are run stays. Far-off creatures are not woken.
- No further fine-tune or paid labelling work. O:\appsnotonc\finetune stays as it is.

## Anti-goals (a failure even if it works)

- A creature acting on the player's real position. It may use only what it saw or heard, or what kin shouted to it.
- Original changed in any way: with the level at Original the replays and proofs are byte-identical to today.
- Cunning the player cannot notice. A retreat that looks like a creature wandering off is a failure.
- A quest stalled because a creature it needs fled, followed or hid.

## Constraints (from the project)

- The code lives beside hunt.c and hunter.c (underworld-exhumed\uw1\src\port\sys), reached through the six hooks
  AI.C already has behind ENHANCED(ENH_SMART_HUNTING). No new hook in a rules routine unless a part below cannot be
  done without it; any new hook is behind the same test and listed in NOTES.md.
- The goal word has three free values (13, 14, 15) and all are in use. New behaviours are sub-modes of those goals,
  kept in hunt.c's per-creature state, not new goal values.
- State is runtime only, as hunt.c's is: the save format is unchanged; after a load a creature starts afresh.
- Other sessions edit uo-new and underworld-exhumed at the same time (uwfront.c has uncommitted changes of
  another session today). Commit by explicit path; touch uwfront.c only for the menu rows.
- Proof for this work, by the user's rule of 2026-11-15: it compiles and the exe launches. The user judges the
  behaviour in play, with the Ctrl+L AI log showing every decision and its reason.

## Approaches considered

1. **Grow hunt.c in place.** Put the rulebook and the pack behaviour straight into hunt.c. Least plumbing, but
   hunt.c is already 1,200 lines mixing goals, the model's questions and the friendlies.
2. **A decider module and a pack module beside hunt.c (chosen).** hunt.c keeps the goals and the hooks. A new
   file answers "what should this creature do now" from plain facts; another holds the pack layer. The model's
   answer path stays and the decider feeds the same path, so everything downstream is already written and tried.
3. **A planner or behaviour-tree framework.** The research (F.E.A.R., Halo, Descent) shows the smart-looking
   behaviour came from simple rules, a per-type table, a squad layer and visible reactions, not from the planner.
   More machinery than this needs.

Chosen: 2, as my call (the user handed over the how). It keeps the tried code, keeps each file about one thing,
and lets the model remain an optional second opinion without a second code path.

## Design

### The setting
- A new setting, creature_ai = 0 Original, 1 Devious, 2 Monstrous. ENH_SMART_HUNTING stays as the switch the
  hooks test: the row sets it on for 1 and 2 and off for 0, so every existing test of the flag keeps working.
- Esc menu: "Creature AI" replaces the smart-hunting entry as the way to turn this on. Under it, shown only on
  Monstrous: "Hit and run", "Calls for help", "Pack courage", "Fall back when hurt", "Strike together",
  "Calls passed on", each On/Off; and "AI messages" On/Off for the message-scroll lines (both levels).
- "Decisions by": Rules (default) / Language model. On Language model the model is asked as today and its answer
  wins when it arrives; the rules act meanwhile. The "Language model" and "Level hunters" rows stay; level hunters
  no longer need a model (their plan comes from the rules).

### The decider (new file cunning.c)
One function per question kind the game already asks, taking a small struct of facts (creature type, wits, health,
ambusher, kin near and kin fighting, what it perceived and how long ago, distance, the adventurer's look, weapon
and light, the choices on offer) and returning one of hunt.c's MIND_ codes with the number of the rule applied
(for the log). hunt.c fills the struct where it now builds the model's question, and stores the result where
the model's answer is stored, so mind_act / seen_act / the friendly and hunter paths run unchanged.

The rules are rulebook v4 (O:\appsnotonc\finetune\data\SYSTEM.txt), rules 1 to 9, first match wins, with the four
rulings from the finetune handoff. The hunter's plan steps follow tools\hunter_policy.py, which already scores 10 of
10 on the gold steps; it is ported as it stands.

One gap found by the gold check on 2026-11-16 is settled here as my call: rule 6's "the adventurer was seen
carrying a light while the creature is in the dark" holds only when the creature stands outside that light's reach
(distance greater than the reach). Inside it the creature is lit and rule 7 applies (attack, or hunt). This gives
gold's answers on the two questions every teacher model disputed.

Where today's code rolls a die (ambush half the time when it loses the player or hears a noise), the decider's
answer replaces the roll.

### The trait table (in cunning.c)
One row per creature type, defaults derived from OBJECTS.DAT (wits, race, range, the missile or spells it has,
swims), with hand-set overrides:

| Trait | Meaning | Examples (my proposal, to be tuned in play) |
| --- | --- | --- |
| pack_need | kin that must be together before it will fight; 0 fights alone | spiders, rats, bats 2; goblins 0 |
| nerve | health fraction at which it falls back to kin | goblins a half; trolls, golems, skeletons never |
| calls | screams for help when it meets the player alone | goblins, lizardmen, mountainmen, outcasts, spiders yes; the dim-witted and the undead no |
| kites | hit and run when it can shoot or cast | mages, gazers, imps, any creature whose first weapon is a missile |
| relays | passes a call on | wits 10 and up |

"Outnumber you by 2" is read as two of them to one of you (pack_need 2). A number in the table, changed in a
moment if the user meant three.

### Monstrous behaviours (new file pack.c for the group parts)

All use only perceived facts and the original's own actions.

1. **Hit and run** (kites). A creature that can shoot or cast and knows where the adventurer is holds its ground
   out of reach until he is within its range in a clear line (the original's own test for a ranged attack), lets
   the original's attack fire, then, if kin are within its hearing, walks to the nearest of them and turns to
   fight there. With no kin near it backs off to its preferred distance and shoots again. Cornered, the
   original's cornered fight takes over.
2. **Calls for help** (calls). A creature that comes to know where the adventurer is, with no kin fighting beside
   it and kin within hearing, calls first: a sound at its square, the message line, and every kin that can hear it
   is rallied to the place (hunt.c's rally, already there). Then it attacks. One call per creature per minute.
3. **Pack courage** (pack_need). Such a creature that knows where the adventurer is, with fewer kin beside it than
   it needs, flees him (the original's flee, which already turns to the cornered fight when it has no way out).
   When enough kin are within 3 squares and know of him too, the whole group turns and attacks, with the message
   line.
4. **Fall back when hurt** (nerve). Hurt below its nerve, with kin within hearing, it runs to the nearest kin
   instead of fleeing blindly, and attacks again beside them. Rule 3 still wins: a badly hurt adventurer is prey.
5. **Strike together.** Kin rallied to a place gather 3 to 4 squares short of it and go in when all called have
   arrived or 10 seconds have passed, instead of arriving one at a time.
6. **Calls passed on** (relays). A creature that hears a call and has the wits repeats it once, so word travels
   from creature to creature, at most two repeats from the first call. This is the level-wide alarm from Descent,
   cut down to honest shouting: nobody learns anything that was not shouted within their hearing.

### Making it visible
- Message lines (short, in the scroll, switchable): "The goblin shrieks for help.", "The spiders turn on you.",
  "The mage falls back to its kin.", "The goblin runs to its kin." One per event, not per creature per step.
- The call's sound: the creature type's own sound, played at its square, which also counts as a noise other
  creatures can hear. The builder picks the effect per creature sound class from what SOUND.C offers.
- Later part (the user, 2026-11-16: "make a sound for the creatures too... build them so they go through the
  MT-32"): our own call sounds as new Roland timbres. Each is a timbre definition (four partials from the unit's
  own waveforms and samples) plus a note, velocity and length, rendered by the same Munt path uwaudio.c already
  uses for the SOUNDS.DAT effects, and played positionally at the caller's square. One per creature voice (goblin
  shriek, spider hiss, lizardman bark, and so on). Each is first rendered to a WAV for the user to hear and approve;
  until a creature has its own, it uses the existing effect. Own Esc row: "Creature calls" On/Off.
- Ctrl+L log: every decision with its rule number and the facts it used, as the model's answers are logged now.

## Left to the builder
- The exact struct of facts, and which of hunt.c's helpers move to a shared header.
- How a sub-mode of goals 13 to 15 is stored and stepped (hunt.c's HuntState gains a mode field).
- The effect numbers for calls; the wording of the message lines.
- Whether hit and run needs one new hook in crit_offense (to hold fire until in range) or can be done from the
  existing hooks; if a hook is needed it goes behind the ENH test and is noted.
- The order of work: the setting and the decider first (Devious complete and playable), then the Monstrous
  behaviours one at a time, each with its menu row.

## Calls made here that the user should check
1. "Outnumber by 2" read as two to one.
2. Rule 6's light clause settled by the light's reach (above).
3. The language model kept as an optional "Decisions by" choice, off by default, instead of being removed.
4. The alarm is relayed shouting within the original's active range, not a level-wide meter that wakes far creatures.
5. Which creature types call, kite and need a pack (the trait table's examples).
6. Proof is compile and launch only; no gold-set test is wired into the build.
7. The default is Devious: smart-hunting has been on by default since the user's choice of 11-12 (corrected while
   building: the first draft of this line said Original). A player who had turned smart-hunting off starts at Original.
8. Friendly creatures follow rule 9 on Devious and Monstrous without a model: they greet, follow, warn and keep
   away as the model-led ones do today. Named ones only greet and warn, as now.
