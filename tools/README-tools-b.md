# tools\ guide, part B (files from `p` to `x`)

The tools in `tools\` whose names sort from `p` on (`panel2_*` ... `xmidi_verify.py`), grouped by purpose. Part A
(`README-tools-a.md`, if present) covers the names before `p`. `uwr_export.py` is described in its own header and
not repeated here. `laigter-cli\` is a separate program and not covered.

What every Python tool here assumes (from `CLAUDE.md`):

- Run from the repository root `O:\uw\uo-new` with uv and `python -I`, `UV_CACHE_DIR=O:\appsnotonc\sentinel\uv\cache`,
  and **unicorn pinned to 2.0.1.post1** (2.1.x crashes on this machine in `mem_map`).
- `game\` is the original game folder (`game\UW.EXE`, MD5 8afc3c29ef5667a3ab6667e8cbc8d63a, never changed).
- The verifiers run the ORIGINAL routine in a Unicorn emulator (`tools\emu.py`, or `tools\adv.py` for the sound
  driver) beside OUR code on the same inputs, and compare. A difference is fixed on our side, never in the check
  (`DECOMP_RULESET.md` / `CLAUDE.md`: "proved" means such a verifier says "same").

---

## 1. How to run the main ones

### `proofs.cmd`: the rules-core proofs

The game's rules are underworld-exhumed's byte-matched C compiled unchanged into the rules core (`tools\core.py` ->
`build\core\core.a`, test harness `build\coretest.exe`). Each `proof_<file>.py` proves one source file of it against
UW.EXE through `tools\coreproof.py` (the `Pair` / `Differ` harness) and `tools\cproof_lib.py` (`World`, random sound
game states on the nine real levels of LEV.ARK).

```
tools\proofs.cmd                 every proof_*.py in turn; stops at the first that fails
tools\proofs.cmd objects         one proof (tools\proof_objects.py)
tools\proofs.cmd objects 50      one proof with its count argument (operations / cases per level; meaning per script)
```

- First run: it makes `work\venv` (Python 3.10, `unicorn==2.0.1.post1`, `capstone`, `setuptools<70`) with uv.
- Each script is called as `proof_X.py <game dir> <build\coretest.exe> [count]`.
- Output: per-routine counts and `all the same`, or `<name> DIFFER: ...` and exit code 1. A passing run also prints
  the coverage (which of the file's routines UW.EXE actually ran) and writes `work\ran\<script>.txt` (written by
  `coreproof.py`'s `coverage()`).
- `proof_bags.py`'s header says it ignores the exe argument and uses the proofs' own harness copy
  `build\coretest_inv.exe`, built from the current core.
- Environment switches seen in the scripts: `PROOF_SEED=n` (another random run, `cproof_lib.World`); `PROOF_TRACE`
  (extra trace printing in proof_ai/combat/crittime/objects/pathfind; in proof_objects `PROOF_TRACE="level n"` also
  writes an instruction trace to `work\itrace.txt`); `PROOF_NEG=1` (proof_ai: run the negative-distance
  `turn_real_fine` cases the core is known to differ on); `UW1_PROOF_UB=1` (proof_playdata: put back the
  undefined-behaviour slots loop); `PROOF_CRITS=1` (proof_playmove: let `move_physics` call `move_mobile`).

After a proof printed `all the same`, record what it proved:

```
python -I tools\proved.py proof_objects.py [proof_look.py ...]
```

then regenerate `ROUTINES.md` (`tools\checklist.py`, see `CLAUDE.md`) and `ROADMAP.md` (`tools\roadmap.py`).

### `verify.cmd`: the commit gate (run by `run-tests.cmd`)

Must pass at every commit (`CLAUDE.md` rule 3). It runs, stopping at the first failure:

1. `verify.py game build\rtest.exe 1000`: small routines (rand, dice, skill check, resist, experience).
2. `sfx_verify.py game build\rtest.exe 2000`: the sound-effect routines and their math.
3. `songs_verify.py game build\rtest.exe 4000`: the music choice.
4. `collide_verify.py game build\rtest.exe 600`: terrain collision (segment 2529) on the nine levels (part A).
5. `build\hunternav_test.exe`, `build\hunterfile_test.exe` (run from the repository root), `build\keyqueue_test.exe`,
   `build\uwinput_test.exe`: our own C unit tests (the level hunters' navigation and side file, the keyboard ring,
   the window's input module).

It needs `build\rtest.exe` and the four test exes built first. `run-tests.cmd` calls it, then `tests\xmidi_test.cmd`
(which builds `build\xmidi_test.exe` and runs `xmidi_verify.py`).

---

## 2. Rules-core proofs (`proof_*.py`)

Every proof: same routine on both sides, same inputs; after every call the return value, the outside calls (with
their arguments, printed text, sound arguments), Borland rand's state, the whole level block and the subsystem's own
records/globals must be equal. Usage of each: `python -I tools\proof_X.py game build\coretest.exe [count]`. The
header docstring of each script lists exactly what is compared and what is not run and why; the summary here names
the source file (underworld-exhumed `uw1\src`), its segment/overlay where the header gives it, and the main routines.

| Script | Proves (original source file, routines) | Not proved / limits (from the header) |
|---|---|---|
| `proof_ai.py` | AI.C (seg007_1798): critter goals and AI (`crit_mill`, `crit_guard`, `crit_offense`, `crit_attack`, `crit_flee` ...), `critter_ai`, `critter_mv`, `set_critter_vars`, `damage_critter`, `crit_die`, `timetodo`, `move_mobile` | negative-distance `turn_real_fine` cases kept out unless `PROOF_NEG=1` (core differs there) |
| `proof_bags.py` | BAGS.C (ovr117): opening/closing/scrolling containers, putting in and swapping, `BagWeight`, `DoSpecialActions` | `DisplayOpenBag` (drawing only, replaced) |
| `proof_chargen.py` | CHARGEN.C: `init_char`, `roll_stats`, `set_skills`, `add_to_skill`, `set_sklmnu`, `show_atts`, `show_skills`, `adr_chrpic`, `move_chrpic` | the question screens (`drawopt`, `selopt`, `mousopt`, `pickopt`, `gen_char`, `strt_chargen`: wait on input) |
| `proof_collide.py` | COLLIDE.C (seg026): `GetHgt`, `GetSlopeHgt`, `SolvePnt`, `TerrainCheck`, `obj_coll_check` ..., placing objects | |
| `proof_combat.py` | COMBAT.C (seg022_230E): `pickloc`, `resolve_attack`, `do_damage`, `do_miss`, the player's swing (`player_attack`), `critter_attack`, `missile_thwack`, kill experience | |
| `proof_creature.py` | CREATURE.C (ovr101): `creature_class_data`, `creature_obj_init` | `creature_init` / `creature_save` only read/write a file (outside calls) |
| `proof_crittime.py` | CRITTIME.C (ovr104): critters between moments (`up_crit`, `up_mob`, `change_critter_goal`, `yearly_checkup`, `wandering_monster_check`, `hostile_creatures_near`, `init_level_creature_stuff` ...) | |
| `proof_crpages.py` | CRPAGES.C (ovr113): the critter art page cache (`preload_cr`, `NightCleanCritPages`, `punt_idle_crpage`, `swap_ws_out`, `unswap_ws`) | |
| `proof_invdata.py` | INVDATA.C (ovr120): inventory as data (`AddToInventory`, `FindSlot`, `FindObj`, taking out whole or part, damage to worn gear, weights) | |
| `proof_invpanel.py` | INVPANEL.C (ovr121): `DoInventoryMouse`, `DoInventoryDrag`, `ItemFitsSlot`, `AddTogether`, `AddToEmptySlot`, `AddToOccupiedSlot`, `RearrangeInventory`, `FindInventoryHit` ... | pick-up branch (needs `mouse_dragged`), `AskHowMany`'s question, the barter area |
| `proof_invsave.py` | INVSAVE.C (ovr118): saving/restoring the inventory via the workspace and PLAYER.DAT (bytes compared); PLAYER.C's `Punt_player_inv` | |
| `proof_look.py` | LOOK.C (ovr122): `LookAt` at every lore level, `do_mods`, `do_of`, `BookLook`, `KeyLook`, `BonesLook`, `RectLook`, `CritterLook`, `SpecialLook` (strings from STRINGS.PAK) | |
| `proof_motion.py` | MOTION.C (seg030): `do_physics` whole and step by step (`space_to_motion`, `grid_move`, `check_positions`, `do_2dbounce`, `do_zbounce` ...) for player, critters, missiles | |
| `proof_objclass.py` | object class data: OBJCLASS.C (ovr129 `init_objects`, `get_class_data`), HACK.C (ovr116), MISC.C (ovr125), ANIMOBJ.C (ovr089), empty handlers OVR139/144/146.C; tables from OBJECTS.DAT / COMOBJ.DAT | |
| `proof_objects.py` | OBJECTS.C (seg027_2861): the object lists (`Obj_Alloc`, adding/removing/freeing, `Obj_GarbageCollect`, `Obj_Check`, `Map_ObjFix` ...) | |
| `proof_objphys.py` | OBJPHYS.C (seg029): `bounce_obj`, `missile_newhit`, `do_objhit`, `get_phys_data`, `set_phys_data`, `static_to_mob`, `mob_init`, `mob_to_static`, `obj_deal` | |
| `proof_pathfind.py` | PATHFIND.C (seg006_1477): critter motion, path finding, doors (`init_ai`, `move_me_joe`, `get_terrain`, `crit_hndlr_walk`, `crit_hndlr_fly` ...) | |
| `proof_physics.py` | PHYSICS.C (seg008): the player's physics (`water_set`, `parse_player_terr`, `simple_fizix`, `player_setup` ...) | |
| `proof_playdata.py` | PLAYDATA.C (ovr133): `FixPlayerEquips`, `player_affected_by`, `parse_spells`, `parse_aspells`, `armor_val`, `init_spells`, `set_maze`, `set_drugged` | `save_player_data` / `read_player_data`, `swap_tmap`, `set_drugged` effects past the first; a UB loop unless `UW1_PROOF_UB=1` |
| `proof_player.py` | PLAYER.C (ovr134): `InitPlayerRec`, `init_player` (key and mouse dispatch tables compared entry by entry), `mous_player`, the camera and view-angle routines | `port_pitch_bound` (ours, not in the original) |
| `proof_playmove.py` | PLAYMOVE.C (seg034): `parse_playin`, `do_player_keyboard`, `move_physics`, `move_player`, `make_noise`, sounds, head bob, camera; `check_physics` (the whole clock) | `player_simple_move` (busy wait), mouse branch of `parse_playin`; `move_mobile` off unless `PROOF_CRITS=1` |
| `proof_playtime.py` | PLAYTIME.C: `duration_check`, `dispel_spell`, `DegradeLights`, `sink_sink_sink`, `set_curmagic`; UTIL.C's `rollem` | |
| `proof_runes.py` | RUNES.C (ovr119): the rune bag and casting (`add_rune`, `clear_runes`, `try_cast`, `player_cast` ...), casting through SPELLS.C's `do_spell` | |
| `proof_skills.py` | SKILLS.C: `use_skill`, `player_use_skill`, `player_compute`, `advance`, `great_advance`, eating, dreams, trap detection/removal ... | `player_sleep`, `mantra_advance`, `dream` when none due, `drop_drunk_player`, `check_victory`, `player_is_dead` |
| `proof_spells.py` | SPELLS.C (seg038_3307): `do_spell`, `cast`, the spell table, missiles, area spells (`nail_area`), summoning, `xt_spells` | gate travel with a moonstone |
| `proof_treasure.py` | TREASURE.C (ovr150): critter loot, spilling containers; COMBINE.C (ovr099): combining by CMB.DAT | |
| `proof_trigger.py` | TRIGGER.C (ovr153): `UseTrigger`, `SetOffTrap`, `UseTrap`, per-type trap actions, hack traps, removals, wandering monsters, closing doors | crystal ball (hack quality 2), talking door (0x2A), conversations |
| `proof_trigsave.py` | TRIGSAVE.C `trap_save` with TRIGGER.C's `trap_init` (file bytes and table compared) | |
| `proof_useitems.py` | USEITEMS.C (seg040_352B): `UseObj` on every kind of object; doors, locks, keys, `DumpTheBag`, object spells, `place_new`, `using_punt` | |
| `proof_worldev.py` | WORLDEV.C (ovr107): teleports, `change_terrain` (through callers: 9 arguments), traps, plot deaths, item repair (`do_repair`, `repair_item`) ... | `talking_door_trap`; `find_good_x_and_y` only where it does not read its half-cleared array |

Bookkeeping for the proofs:

- `proofs.cmd`: runner, see section 1.
- `proved.py`: appends to `re\proved.txt` the routines listed in `work\ran\<script>.txt` (looked up by name in
  `re\crosswalk.tsv`, skipping ones already there); prints `re/proved.txt: N added`. Run only after a passing proof.

---

## 3. Older verifiers of our own ports (`build\rtest.exe`, `build\xmidi_test.exe`)

These predate the rules core: they compare routines we ported by hand in `src\` with the original in the emulator.

| Script | What | Run |
|---|---|---|
| `verify.cmd` | The commit gate (section 1). | `tools\verify.cmd` |
| `verify.py` | UW.EXE routines vs `build\rtest.exe`: `rand` (0xec5:0xde7), `dice` (0x35bd:0x11e), skill `check` (0x30f9:0xc), `resist` (0x229b:0x2ff, COMOBJ.DAT loaded at DS 0x5b6e), `getexp` / `killexp` (SKILLCHK.C `player_get_exp`, COMBAT.C `player_killed_a`); result and rand seed after each case. Prints `<name> N cases same` per routine, then `all the same`. | `uv run --no-project --with unicorn==2.0.1.post1 --with capstone python -I tools/verify.py game build/rtest.exe [cases]` |
| `sfx_verify.py` | Sound effects of segment 1bd8 (play, play at a place, 16 Hz timer, stop) and the math (isqrt, sin/cos) vs `src\sfx.c` in `build\rtest.exe`: driver calls, returns, effect state. | `... python -I tools/sfx_verify.py game build/rtest.exe [operations]` |
| `songs_verify.py` | Music choice of segment 1bd8 (play, resume, on/off, toggle, wandering song, the turn) vs `src\songs.c`: driver calls, file and timbre loads, state. | `... python -I tools/songs_verify.py game build/rtest.exe [operations]` |
| `xmidi_verify.py` | Every song (`game\SOUND\*.XMI`, or those named) through UW's own MT32MPU.ADV in the emulator (`tools\adv.py`) and through our port `build\xmidi_test.exe` (src\xmidi.c, timbres from UW.MT): the bytes sent to the MPU-401 must be identical. Called by `tests\xmidi_test.cmd`. | `... python -I tools/xmidi_verify.py [ticks] [song...]` |
| `xmidi_script.py` | Same driver comparison for driver-call scripts (`tests\xmidi_scripts\*.txt`, commands such as status, beat, ctl, sysex, protect ...): every byte to the MPU, every wait, every return. Writes our side's record to `work\xmidi_script_ours.txt`. | `... python -I tools/xmidi_script.py tests/xmidi_scripts` (files or folders) |
| `portverify.py` | Was: Exhume's `replay.py verify` on the `portwin32.py` build. **Retired**: exits at once (the user, 2026-11-10: no replay tests; a run once filled O:). Do not run. | (refuses to run) |

---

## 4. Builds of the underworld-exhumed port

| Script | What |
|---|---|
| `portwin32.py` | Builds underworld-exhumed's UW1 port with our LLVM clang (MinGW target) and our Win32 backend: copies `port\win32\*` into `uw1\src\port\platform\win32\` (refuses if the copy there is different and newer, unless `--force-copy`), answers pkg-config itself, runs Exhume's `portbuild.py` unchanged, builds the renderer and Munt via `buildview.cmd` / `buildmunt.cmd`, then deploys `uwview.dll`, `uw_nvngx.dll`, `dlss\`, `mt32emu.dll` and the chosen ROM pair beside `uw1\build\port\uw1port.exe`. `--original` builds from `exhumed-original` in plain mode (no renderer/DLSS). Run: `work\venv\Scripts\python.exe -I tools\portwin32.py [--original] [portbuild options]`. |

---

## 5. Reference and research tools

| Script | What | Run |
|---|---|---|
| `roadmap.py` | Writes `ROADMAP.md`: underworld-exhumed's source files (`re\crosswalk.tsv`) with each routine's status from `ROUTINES.md`, files in the fixed finishing order (`ORDER`), with per-file counts. Prints the totals. | `uv run --no-project python -I tools/roadmap.py re/crosswalk.tsv ROUTINES.md ROADMAP.md` |
| `scan16.py` | Linear 16-bit capstone disassembly of UW.EXE from file offset 0x3200; prints windows where all given instruction fragments (substrings of "mnemonic op_str", `a|b` alternatives) occur within N instructions. Finding code by pattern. | `uv run --no-project --with capstone python -I tools/scan16.py game/UW.EXE WINDOW "frag1" "frag2" ...` |
| `strings_pak.py` | Decodes STRINGS.PAK (Huffman-coded game text; format in its header). Imported as a module (`blocks()`) or prints blocks. Reads via `tools\assets.py`. | `python -I tools/strings_pak.py dump [block]` |
| `xmi.py` | XMI (Miles MIDI) to standard MIDI file; the reference for `src\audio.c`'s converter. `info` lists programs per channel and length. | `python -I tools/xmi.py info NAME.XMI` / `convert NAME.XMI OUT.mid` |
| `rle_probe.py` | Research: tries the 8 readings of the critter art's run-length scheme (`one_skips`, `two_multi`, `minus`) on `CRIT\CRnnPAGE.Nnn` and prints, per reading, how many frames do not use up their data exactly. Uses `tools\assets.py`, `tools\crit.py`. No arguments. | `python tools/rle_probe.py` |

---

## 6. One-off source patches (`*_patch.py`) and their text inputs

Each is a one-off edit script kept for the record (`CLAUDE.md`: shell heredocs mangle backslashes, so edits were
written as scripts). Each takes the repository root as its only argument, replaces exact text in `src\` files
(asserting the old text occurs once; all but `ui_render_patch.py` keep the file's CRLF/LF line endings) and prints
`ok`. They describe past
changes to our front end; the source has moved on since, so **do not re-run them**. Checked present in today's source:
`read_patch` (`ui_clip`), `regen_patch` (`game_player_tick`), `settings_set_patch` (`apply_setting`), `panel2_patch`
(`inv_mouse_over`).

| Script | Edit it applied | Files |
|---|---|---|
| `panel2_patch.py` | Inventory panel as one combined grid with a bag bar: replaces the panel section of `inventory.c` with `panel2_new.c.txt` and its `inv_draw` with `panel2_draw.c.txt`; bag click toggles `hidden[]`; adds `inv_mouse_over()`. | src\inventory.c, src\inventory.h |
| `panel2_new.c.txt` | Input of `panel2_patch.py`: the panel code (cells, grid layout, bag sections, bag bar). C text, not compiled from here. | |
| `panel2_draw.c.txt` | Input of `panel2_patch.py`: the new `inv_draw()`. | |
| `read_patch.py` | Reading books and scrolls (UW.EXE file 0x37fa0): a reading panel in `game.c`, `ui_clip()` for wrapped text, using objects 0x130-0x13F from the inventory. | src\ui.h, ui.c, game.h, game.c, inventory.c |
| `regen_patch.py` | The original's healing/mana/poison tick (UW.EXE 0x2adee..0x2af2d) as `game_player_tick()`, called from `player_tick`; no combat messages. | src\inventory.c, game.h, game.c |
| `settings_set_patch.py` | `main.c`'s settings-key chain moved into `apply_setting()`; adds a `--set=key=value` flag for headless checks. | src\main.c |
| `switch_patch.py` | Switches (0x170-0x17F) fire their use trigger (0x1A2) into the trap runner with the trigger's target tile; the door trap (file 0x843dd) and the text string trap (0x847bb). | src\level.h, level.c, world.c, game.c |
| `tooltip_patch.py` | Item tooltips with comparisons; right click never drops (shift + right click drops); `objects.txt` v4 with OBJECTS.DAT's weapon tables. | src\extract.c, objdata.h/.c, inventory.h/.c, game.h/.c, main.c |
| `traps2_patch.py` | The create object trap (file 0x8418f) and the delete object trap (0x845dc). | src\game.c |
| `traps3_patch.py` | Trap chains (0x84803), set/check variable (0x83ea4, 0x83fc5), inventory trap (0x8463b), damage that heals when the owner is set (0x840bd), change terrain (0x83e17 -> 0x73b68) with the level rebuilt while playing. | src\game.c, game.h, inventory.h/.c, main.c |
| `triggers_patch.py` | Triggers on objects (0x83b3b): look (event 5, search check vs the trigger's z), pick up (2), use (4), open (7); OBJECTS.DAT's event table at 0xd92; special tmap objects (0x16e) on walls. | src\game.c |
| `ui_render_patch.py` | The UI overlay in the renderer. | src\render.c, render.h, shaders\present.hlsl, shaders\cbuf.hlsli |
| `world_mouse_patch.py` | With the inventory open the world stays usable under the pointer (left click / L looks, right click / E uses); Esc backs out one step (`game_escape`), else opens the options. | src\game.c, game.h, inventory.h/.c |
| `relief_section.c.txt` | C text of "the relief as geometry" section (quadric edge-collapse simplification of each texture's height mesh, written as OBJ); marked `uo-new relief v4`. No tool in `tools\` reads it; `src\normals.c` holds the current version (v7) of that section. | |

Note: several game-rule edits above (traps, triggers, healing) were our front end's hand ports from before the rules
core; the rules now come from the core and are proved by the `proof_*.py` scripts (section 2).
