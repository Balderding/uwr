# tools\ (a to options_patch.py): what each one is for

This guide covers the files in `tools\` whose names sort before `p` (`adv.py` to `options_patch.py`, plus the
`*.cmd`, `*.txt` and `*.cmake` files in that range). The `proof_*`, `verify*`, `portwin32.py` and the other later
names are described elsewhere. `uwr_export.py` is not covered here.

Background (CLAUDE.md, DECOMP_RULESET.md): the game's rules must be exactly UW.EXE's. A routine counts as **proved**
only when an emulator verifier runs the original routine (`tools\emu.py`, Unicorn, 16-bit real mode) beside ours on
the same inputs and every result, outside call and piece of state matches. The proofs are listed in `re\proved.txt`;
`tools\checklist.py` turns that into `ROUTINES.md`.

Conventions used by nearly every script:

- Run Python with `-I` (isolated mode), from the uo-new root (`O:\uw\uo-new`) unless the usage line says otherwise.
- Scripts that need Unicorn or Capstone are run through uv with the package pinned on the command line:
  `uv run --no-project --with unicorn==2.0.1.post1 python -I tools\X.py ...` or `... --with capstone ...`.
- `game\` is the md5-checked copy of the original UW1 (UW.EXE 547248 bytes, MD5 8afc3c29ef5667a3ab6667e8cbc8d63a).
  No tool writes to it.
- Addresses: "file 0x..." means a file offset in UW.EXE; `SEG:OFF` means a segment of the load image (file offset
  = 0x3200 + SEG*16 + OFF). DGROUP (DS) is paragraph 0x5AAC.

---

## 1. Emulator and proof libraries

These are libraries (imported by the `proof_*` scripts and the verifiers). They are not run on their own.

| File | What it is |
| --- | --- |
| `emu.py` | The emulator harness. `UWEmu('game/UW.EXE')` loads UW.EXE's load image at linear 0 (so its segment numbers need no relocation), sets DS = SS = DGROUP, and runs one far call at a time with `call(seg, off, [word args])` -> `(AX, DX)`. Calls into a Borland VROOMM overlay stub reach `int 3Fh`; the harness loads that overlay once above the image, applies its segment fixups and jumps to the routine. Any other interrupt stops the run (the routine touched DOS or hardware). `hook(seg, off, fn)` / `hook_file(file_off, fn)` replace a routine with Python (`emu.arg(n)` gives its arguments). `set_seed()` / `seed()` read and write Borland `rand`'s state (DS 0x207e). Also `rb/rw/wb/ww/read/write`, `call_file(file_off, ...)`. |
| `coreproof.py` | The proof pair for the rules core. `Pair(game_dir, coretest_exe)` drives UW.EXE's own code in `emu.py` and our core (underworld-exhumed's C built by `core.py`, run through `build\coretest.exe` over a command pipe) side by side. Names and addresses come from underworld-exhumed's `uw1\symbols.tsv`. Compares bytes, pointers (as "the level block" or a named symbol both sides have), returns, Borland rand state and the sequence of outside calls (on our side the stubs of `build\gen\core_stubs.c`; in the emulator a stand-in at each address that records the name and returns 0). Main methods: `level(n)` (LEV.ARK block n as the level), `w/p/q/r` (write bytes, write/read pointers, read bytes), `call(name, args)`, `same_bytes(...)`, `trace(...)` (with the instrumented core, `CORE_INSTR=1`), `coverage()`. Raises `Differ` with context when the two sides disagree. |
| `cproof_lib.py` | Shared setup for the combat and creature proofs (`proof_combat`, `proof_ai`, `proof_creature`, `proof_crittime`, `proof_crpages`, `proof_pathfind`). Wraps `coreproof.Pair`: loads OBJECTS.DAT's class tables on both sides, sets the player record and critter globals, picks real critters of a level and places the player near them, scrambles a critter's AI fields within sound ranges, and compares the whole state after a call (return, outside calls, rand, level block, far buffers, a list of globals). |
| `motion_common.py` | Shared setup for the movement proofs (`proof_collide`, `proof_motion`, `proof_objphys`, `proof_physics`, `proof_playmove`). Class `World`: the player record, the clock and the player object placed on a real level, set the same on both sides; generators of sound inputs (open tiles, real objects, movers); `w.check(ctx, result)` compares the return, outside calls, rand, the whole level block, the physics records, the collision record and every global listed in `STATE`. |
| `magic_common.py` | Shared setup for the magic and trap proofs (`proof_spells`, `proof_runes`, `proof_trigger`, `proof_trigsave`, `proof_worldev`). Builds on `motion_common.World` and adds the data the magic reads, the globals magic and traps write, a level that keeps its real triggers and traps, placing the player near the thing under test, and checks of those globals. |
| `inv_common.py` | Shared setup for the inventory / player / map / system proofs (`proof_bags`, `proof_invdata`, ...). `build_exe()` builds `build\coretest_inv.exe`, a private copy of `tests\coretest.c` with two extra commands (`qn`: a pointer as a number; `rm`: bytes at an address) so farmalloc'd open bags can be read on our side; it is rebuilt when the core is newer, and `tests\coretest.c` itself is not changed. Class `Inv`: a Pair with the game's files, class tables, strings and player set up; objects made on both sides through the proven `Obj_Alloc` / `Obj_AddEnd`; random but sound inventories; `check()` compares return, outside calls, rand, level block, player record, inventory slots, cursor object, open bags (walked on both sides) and every shared global (raw bytes, up to 0x100 each, pointer variables excluded). |
| `inv_sizes.py` | Writes `work\inv_sizes.json`: the `sizeof` of each global both sides share, as the core's headers declare it, found by building and running a small clang program. The inventory proofs use it to compare a global up to its real size rather than up to the next symbol. A name the headers do not declare keeps the gap to the next symbol (at most 0x40). Run: `python tools\inv_sizes.py`. |
| `adv.py` | The sound-driver emulator. `Driver('game/SOUND/MT32MPU.ADV')` loads one of UW's Miles AIL 2 drivers (a flat binary at offset 0 of its own segment) in Unicorn; `call(fn, ...)` calls an AIL function by number through the driver's function table (the driver handle is passed for you, at [bp+6]); `serve(n)` runs the service routine n times; `out` is the list of `(tick, port, value)` writes. The MPU-401 is modelled as always ready (status port 0x331 reads 0x80, except right after a command, when the 0xFE acknowledgement waits at 0x330). `gtl_index()` indexes a timbre library (UW.MT). The reference our C port of the driver must match byte for byte. |

## 2. Verifiers and checks

### Emulator verifier

| File | What it does |
| --- | --- |
| `collide_verify.py` | Proof of the terrain collision: UW.EXE's segment 2529 (TerrainCheck 0x28809 with GetHgt, SolvePnt, SolveCenter; ComputeHeading 0x28c86; GetSlopeHgt 0x28635) run in `emu.py` on the real levels beside our `src\collide.c` (through `build\rtest.exe`). The level block comes from LEV.ARK, the terrain words from TERRAIN.DAT. Every result must match. Args: game dir, rtest exe, optional number of cases per level (default 1500). `tools\verify.cmd` runs it with 600. |
| `adv_music.py` | Plays one XMI song through UW's own MT32MPU.ADV in `adv.py` as the game does (describe, init, register the sequence, install every timbre it asks for from UW.MT, start, serve for N ticks) and writes every port write with its tick to a text file (`tick port value` per line, or `tick vr`). The reference for our music driver port. Args: `ADV GTL XMI sequence_number ticks out.txt`. |

### Off-screen behaviour checks of the port (uw1port.exe)

All of these start the test exe headless (never a visible window, except `inputcheck.py visible`) with a fresh copy
of the test home `work\in2-4`, and assert on its stderr/stdout (and sometimes window shots). Each takes optional
case names on the command line (all cases when none) and exits with the number of failures. The exe is
`underworld-exhumed\uw1\build\port\uw1port.exe` unless noted.

| File | What it checks |
| --- | --- |
| `deathcheck.py` | The death scene (plan `docs\superpowers\plans\2026-11-12-death-scene.md`, Task 1). A: mouse-look capture released before the first pointer move, the menu up after PS_DEATH, the pointer drawn only where moved (crosshair colour counted in the window shot). B: a key during the skulls ends them and leaves the menu's four buttons intact. C: focus lost and regained during the skulls gives no "capture on". |
| `followcheck.py` | The mouse-look follower (`plat_win32.c`'s `mouse_look_follow`) only sends its ` key while the game is on its 3D screen (PS_GAME = 2). Cases `death`, `play`. Uses `UWPORT_LOOKDBG` and `UWPORT_INPUTTRACE`. |
| `inputcheck.py` | The input rewrite (spec `docs\superpowers\specs\2026-11-12-input-rewrite-design.md`). Cases: `look`, `trace` (key/pointer event trace of the new build against the old one in `work\inputrw\old`, the proof that gameplay input is unchanged), `charge`, `sleep`, `display`, `original`, `ring`, `focusheld`, `levelfail`, `fightwalk`. `inputcheck.py visible [scenario ...]` is the only mode that shows a window on the user's screen; it refuses to run while uw-new.exe runs. |
| `fxcheck.py` | The visual and audio effects (plan `docs\superpowers\plans\2026-11-12-fx-audio-and-visual.md`), one case per task: `audio` (tests\uwaudio_test.c through OpenAL Soft's loopback device, plus `UWPORT_FX_LOG` lines), `particles`, `decals`, `water`, `fog`, `acoustics`. Picture cases compare the frame with the effect on and off and require the difference to be only where the effect is. Runs under `work\fx\`, with `--no-audio`. |
| `huntercheck.py` | The level hunters (plan `docs\superpowers\plans\2026-11-12-level-hunters.md`, Task 3) with a fixed plan (`UWPORT_HUNTER_PLAN`, no language model), asserted from the "AI: hunter" lines. Cases `plan1`, `plan_giveup`, `plan_blocked`. Uses the private copy `work\hunters\bin\uw1port.exe` (refreshed by `work\hunters\mkbin.cmd` after each build). |
| `llmcheck.py` | Smart-hunting defaults and the Esc menu's language-model picker (plan `docs\superpowers\plans\2026-11-12-llm-default-and-picker.md`). Cases `a` to `h`: on by default, off without model files, off by setting, model paths with spaces, the first-start picker, the old `llm_model=lfm` value, close during a stalled load, failed calibration. Cases b, d, f, h run a copy of the exe with `build\llama\bin\uwmind.dll`. |

## 3. Reverse-engineering and map tools (UW.EXE)

| File | What it does | Run |
| --- | --- | --- |
| `exe_map.py` | Map of UW.EXE's code: every routine in the load image and the VROOMM overlays, its callers, callees, and the game messages (strings block 1) it prints -> `re\uw_map.txt`. The docstring records the MZ / FBOV / overlay stub layout. | `uv run --no-project --with capstone python -I tools/exe_map.py game/UW.EXE data/strings re/uw_map.txt` |
| `checklist.py` | Every routine from the map with its status (proved / unproved / replaced / notneeded / todo) -> `ROUTINES.md`. Proved comes from `re\proved.txt`; unproved = our source names the address but no verifier compares it; the hand-kept `re\routines_status.txt` gives per-routine or per-segment statuses. Also shows hints: int and port I/O counts, messages printed. Regenerate after each proved routine. | `uv run --no-project --with capstone python -I tools/checklist.py game/UW.EXE re/uw_map.txt re/routines_status.txt src ROUTINES.md` |
| `crosswalk.py` | Puts underworld-exhumed's names and matched source locations (`uw1\map\crosswalk.tsv`) onto our routine addresses -> `re\crosswalk.tsv` (`file offset, name, exhumed file:line, size`). Listing load-image segments = ours + 0x1ED paragraphs; overlay numbering is checked by how many routines land on our map's starts. | `uv run --no-project python -I tools/crosswalk.py game/UW.EXE underworld-exhumed/uw1/map/crosswalk.tsv re/uw_map.txt re/crosswalk.tsv` |
| `coverage.py` | Every message UW.EXE prints (strings block 1, per `re\uw_map.txt`) against the uo-new source: which ones our engine prints (by `str_get(1, N)` or the same text), grouped by the printing routine -> `re\coverage.txt`. | `python -I tools/coverage.py` |
| `dis16.py` | Capstone 16-bit disassembly of UW.EXE at a file offset; prints file offsets. Optional count (default 60) and `back` (start that many bytes earlier). | `uv run --no-project --with capstone python -I tools/dis16.py game/UW.EXE <hex offset> [count] [back]` |
| `callsites.py` | Finds every 5-byte far call (`lcall`) to `SEG:OFF` across the load image and the overlays (overlay segment selectors resolved through the segment table) and shows the instructions before each. Optional count (default 8) and instructions before (default 5). | `uv run --no-project --with capstone python -I tools/callsites.py game/UW.EXE SEG OFF [count] [before]` |
| `models.py` | Reads the 32 3D models inside UW.EXE (door frame, bridge, bench, ...): header, extents, vertices and faces, printed. Table and node format follow UnderworldAdventures' ModelDecoder (used as a map only). The scale of the 8.8 coordinates is marked as not certain in the file. | `python -I tools/models.py game/UW.EXE [model numbers...]` (all when none) |

## 4. Data decoders and extractors

| File | What it does | Run |
| --- | --- | --- |
| `assets.py` | Python decoders for UW's data files, the reference for the C engine's loaders. `check` decodes everything and prints counts, writes nothing; `export` also writes PNGs under `extracted\`. Optional UW folder argument. `run-tests.cmd` runs `export` when `extracted\levels\level1.png` is missing. | `python -I tools\assets.py check\|export [UW folder]` |
| `loose.py` | Writes the engine's loose data files (`data\`: wall/floor textures as 8-bit indexed PNGs, `levels\levelN.txt` lossless text of LEV.ARK's blocks) byte for byte as `src\extract.c` does, so the C extractor can be checked against it. `write [out]` (default `extracted\data-ref`); `roundtrip` rebuilds every level block from its text and compares bytes. `run-tests.cmd` runs `write` when the reference is missing. | `python -I tools\loose.py write [out]` / `roundtrip` |
| `gr.py` | Decoder for the .GR picture files (8-bit, 4-bit, 4-bit run-length; palettes from ALLPALS.DAT). `sheet NAME.GR [palette]` writes `extracted\gr\NAME.png` with every picture. | `python -I tools\gr.py sheet NAME.GR [palette]` |
| `crit.py` | Decoder for the creature animations (CRIT\ASSOC.ANM, CRIT\CRxxPAGE.Nyy). `sheet NN [page]` writes `extracted\crit\crNN_pY.png` with every frame of the page; `assoc` prints ASSOC.ANM's animation names and creature table. | `python -I tools\crit.py sheet NN [page]` |
| `chain.py` | Follows an object's link chain in an extracted level text (what a switch or trigger sets off), with object names from a strings block. | `python -I tools/chain.py data/levels/level1.txt data/strings/block0004.txt OBJ [OBJ ...]` |
| `find_obj.py` | Lists where objects of the given item kinds lie in an extracted level: object index, tile, position, fields. | `python -I tools/find_obj.py data/levels/level1.txt 358 221 128` |

## 5. Builders

| File | What it does | Run |
| --- | --- | --- |
| `core.py` | Builds the rules core: underworld-exhumed's byte-matched C (`uw1\src`: motion, combat, critter, obj, event, inv, conv, map, game, plus listed sys helpers) compiled unchanged with clang through Exhume's portability headers -> `build\core\core.a`. Sources and headers are copied to `build\core\src` / `build\core\inc` with Turbo C's plain `unsigned` made `uint16`. Links `src\core_glue.c` and the other glue files; for every name still missing it writes stubs into `build\gen\core_stubs.c` (function stubs log their name and return 0; data stubs are zeroed blocks; function vs data decided from clang's AST), plus `core_tab.c`, `core_stubs.txt`, `core_sinks.txt`. `CORE_INSTR=1` builds an instrumented copy into `build\core_instr` for `coreproof`'s trace. | `uv run --no-project python -I tools/core.py` |
| `buildview.cmd` | Compiles the HLSL shaders with dxc into `build\gen\*.h`, then builds our D3D12 renderer `build\uwview.dll` (`src\uwview.c` over `src\render.c` and the shared engine sources), the DLSS 5 bridge `build\uw_nvngx_port.dll` (`src\nrbridge.c`), and `build\uwtour.exe` (the renderer with no game, every level and item pictures, `tests\uwtour.c`). Run by `tools\portwin32.py`. | `tools\buildview.cmd` |
| `buildmunt.cmd` | Builds Munt's libmt32emu (`external\mt32emu`, LGPL 2.1+) into `build\munt\mt32emu.dll` and `build\munt\libmt32emu.dll.a`, with `build\munt\include\mt32emu` as a junction to the source. Skips the build when both files exist. Run by `tools\portwin32.py`. | `tools\buildmunt.cmd` |
| `buildllama.cmd` | Builds llama.cpp (`external\llama.cpp`) out of source into `build\llama` with clang for the MinGW target: the core as DLLs and every CPU variant as its own DLL (ggml picks the best one at start). `vulkan` adds the Vulkan backend (needs the Vulkan SDK in `O:\appsnotonc\vulkan-sdk`). Uses `cmake --fresh`. | `tools\buildllama.cmd [vulkan]` |
| `clang-mingw-host.cmake` | CMake toolchain file for llama.cpp's `vulkan-shaders-gen` helper (built and run only during the build), so it is also built with clang for the MinGW target. Used by `buildllama.cmd vulkan`. | (not run directly) |
| `buildmind.cmd` | Against `build\llama` (run `buildllama.cmd` first): builds `build\llama\bin\uwmind_bench.exe` (`tests\uwmind_bench.c`), `uwmind.dll` (`src\uwmind.c`, the model runner the game loads, `port\win32\uwmindhost.c`), and `uwmind_score.exe` (`tests\uwmind_score.c`, times uwmind.dll as the game uses it). | `tools\buildmind.cmd` |
| `buildtokcheck.cmd` | Builds `build\llama\bin\uwmind_tokcheck.exe` (`tests\uwmind_tokcheck.c`: uwmind.c's choice-token check) and runs it on llama.cpp's vocabulary-only files: the SentencePiece ones must fail (0), the GPT-2-style ones pass (1). | `tools\buildtokcheck.cmd` |
| `makedist.py` | Assembles `dist\UW1`, the self-contained game folder: uw-new.exe, uw-original.exe, uwview.dll, uw_nvngx.dll, mt32emu.dll + roms\, dlss\, bin\ (OpenAL Soft, Steam Audio), game\, Play.cmd / UW-New.cmd / UW-Original.cmd. Copies only named files and folders, checks the total size before writing (a whole-tree copy once filled O:), leaves identical files alone, never deletes. | `O:\appsnotonc\sentinel\uv\python\cpython-3.13.17-windows-x86_64-none\python.exe -I tools\makedist.py` |
| `makerelease.py` | Packs `dist\UW1` into `release\UW1-<date>` (a folder) and `release\UW1-<date>.zip`, including game\, renderer, DLSS, roms\, llm\ and home\ with this PC's paths removed from the settings; leaves out logs\, videos\, home\recordings\. Refuses if the folder or zip already exists; writes a VERSION.txt. | `python -I tools\makerelease.py [NAME]` |

## 6. One-off source patch scripts (`*_patch.py`)

All of these date from 2026-11-07 / 2026-11-08 and edited **uo-new's own C engine** (`src\*.c`, `src\*.h`) or
`NOTES.md`, before the rules core (`core.py`, 2026-11-09) took over the rules. Each was run once as
`python -I tools\X_patch.py <uo-new root>` and is kept as a record of the change; its result is in git history
(the commit that added the script). Each replaces anchor texts after asserting each anchor occurs exactly once, and
most keep the file's line ends (CRLF or LF). They are not meant to be run again.

| File | Edited | The change it applied |
| --- | --- | --- |
| `eat_patch.py` | extract.c, objdata.h/.c, inventory.c/.h | Eating and drinking (UW.EXE 0x374ee); hunger and fatigue from the player tick (0x2acbd: counter 1-24, at 24 hunger falls 3-6 and fatigue rises); status line; objects.txt v3 with the food table. |
| `notes_eat_patch.py` | NOTES.md | Notes for eating, hunger, fatigue and the player tick's 24-tick cycle (with the lantern-duration correction). |
| `game_creatures_patch.py` | game.c, game.h, main.c | Creatures: animation, simple AI, combat. |
| `game_doors_patch.py` | game.h, game.c, main.c | Doors, move triggers (stairs, traps), collision with doors. |
| `game_items_patch.py` | game.c, game.h, main.c | game.c uses objdata (names, look) and inventory (pack, lights); Look (L / left click on a non-creature); E on what cannot be picked up looks at it; main.c: the look key, right click = use. |
| `game_talk_patch.py` | game.h, game.c, main.c | Talking to creatures (conversations). |
| `main_game_patch.py` | main.c | Wires the game module (inventory, light from items, interface) into main.c. |
| `main_music_patch.py` | main.c | Music settings and start/stop. |
| `objects_extract_patch.py` | extract.c | extract.c writes `data\objects\objects.txt` (COMOBJ.DAT + OBJECTS.DAT's armour and container tables), the table objdata.c reads. |
| `objects_kind_patch.py` | extract.c, objdata.c/.h, inventory.c | COMOBJ byte 3 bits 6-7 read as a kind (0 thing, 1 weapon/armour, 3 container), not "stack"; objects.txt gets a version line (v2) and the engine remakes an older copy. |
| `notes_items_patch.py` | NOTES.md, run-tests.cmd | Notes on objects, look, inventory, the exe map, the DGROUP correction; new checks in run-tests.cmd. |
| `inv_layout_patch.py` | inventory.c | Wider inventory board with labelled holes; equipping by dropping on the figure (armour by the armour table, weapons right hand, shields left, lights a shoulder, rings a ring), the replaced item onto the pointer. |
| `keys_patch.py` | game.c, inventory.h/.c | Locks and keys as UW.EXE has them (file 0x36b55, 0x3831b): lock 0x10F locked when flags bit 0 set; a key (0x100, 0x102-0x10E) fits when its owner field = the lock's link & 0x1ff. |
| `lights_original_patch.py` | extract.c, game.c, lights.c | Lights as in the original: no wall torches, no floating light; the player's lights burn down by UW.EXE's rule (light table of OBJECTS.DAT, a tick every 21 s). |
| `lights_original_patch2.py` | lights.c (CRLF) | Second half of the above: default lights.txt and `lights_load` without the wall torches; the floating light becomes a comment. |
| `notes_lights_patch.py` | NOTES.md | Notes for the lights as in the original. |
| `levels_patch.py` | game.c, inventory.h/.c | Experience, levels, skill points, max HP / mana and carry limit as UW.EXE has them (0x341d8, 0x8138a, 0x81305). |
| `combat_data_patch.py` | extract.c, objdata.h/.c | objects.txt v5 with the weapons' power bytes and the critter table. |
| `combat_patch.py` | game.c, inventory.h/.c | Combat as UW.EXE does it: attack score and damage, charge, skill check, criticals, dice, hit location, armour, angle bonus, creature attacks from the critter table, experience; the character's numbers in `data\character.txt`. |
| `dlss5_switch_patch.py` | render.c, render.h | DLSS 5 gets its own switch (`render_set_dlss5`) and availability; RTX 20-series or older cards do not try DLSS 5. |
| `options_patch.py` | main.c | Inserts the options menu from `options_menu.c.txt`; Esc backs out one step, else opens the menu; the game pauses while it is open; the view ray goes through the mouse while the inventory is open. |

### C snippets kept as text

| File | What it is |
| --- | --- |
| `options_menu.c.txt` | The options-menu C code (`g_menu`, `settings_save` writing one value back to `data\settings.txt`, the OPT_* rows: DLSS, DLSS 5, style, ray tracing, bounce, relief, music, volume, mouse, FOV, quit) that `options_patch.py` reads and inserts into `src\main.c`. |
| `lights_auto.c.txt` | The automatic wall-torch code (`write_auto`: torches on room walls, larger rooms first, none within 5 tiles of another), added 2026-11-07. The lights patches above later removed the wall torches from the engine. No tool reads this file. |

---

## How to run the main ones

```
cd /d O:\uw\uo-new
rem the code map, then the routine checklist (after a routine is proved and listed in re\proved.txt)
uv run --no-project --with capstone python -I tools/exe_map.py game/UW.EXE data/strings re/uw_map.txt
uv run --no-project --with capstone python -I tools/checklist.py game/UW.EXE re/uw_map.txt re/routines_status.txt src ROUTINES.md

rem the rules core (before the proof_* scripts, which drive build\coretest.exe)
uv run --no-project python -I tools/core.py

rem the collision verifier as verify.cmd runs it
uv run --no-project --with unicorn==2.0.1.post1 python -I tools/collide_verify.py game build/rtest.exe 600

rem reading the original
uv run --no-project --with capstone python -I tools/dis16.py game/UW.EXE 2acbd 40
uv run --no-project --with capstone python -I tools/callsites.py game/UW.EXE 35bd 011e

rem the distributable folder
O:\appsnotonc\sentinel\uv\python\cpython-3.13.17-windows-x86_64-none\python.exe -I tools\makedist.py
```

Off-screen checks: `python -I tools\inputcheck.py` (or `deathcheck`, `followcheck`, `fxcheck`, `huntercheck`,
`llmcheck`), optionally followed by case names; the exit code is the number of failures.
