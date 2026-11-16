# uwr: Ultima Underworld Rekonfigmasturfication

Ultima Underworld: The Stygian Abyss (1992) running on Windows and Linux with a renderer, sound and interface of our
own, in plain C (clang). The game's rules are meant to be EXACTLY those of the original UW.EXE; only the presentation
(graphics, sound, interface, controls) and a few optional extras may differ, and each of those has a switch in the
Esc menu. The program is `uwr` (`uwr.exe` on Windows), short for Ultima Underworld Rekonfigmasturfication. This repository holds only what we made ourselves: no original game files, no executables,
no ROMs and no third-party sources. You need your own copy of the game to run anything.

This is a private snapshot, taken by `tools/uwr_export.py`. The source of truth is the working tree it is exported from.

## Built on others' work

This port would not exist without two projects, both under the MIT licence:

- **Underworld Exhumed** by abedegno (https://github.com/abedegno/underworld-exhumed): a byte-matching rebuild of
  UW.EXE in C. We borrowed from it heavily. Its code is the rules core the game runs on (compiled here unchanged,
  with our own glue files beside it), and some of our own rule code is adapted from it, each routine naming its
  source file and line. Its files are not in this repository.
- **Ultima Underworld Editor** by Karl Clinckspoor (https://github.com/KarlClinckspoor/UltimaUnderworldEditorAvalonia):
  a level editor. We built a test tool on its core library, `uwcheck`, which starts our game hidden, visits every
  level, saves, and runs the editor's own checks on the saved levels. It is for testing only: nothing of the editor
  is in the game, and the tool lives in a repository of its own.

The full list of what we use, with the licences, is in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

## What it looks like

The same places in the same game: the original 1992 display on the left of each pair, ours on the right with
ray tracing, upscaled textures, relief maps and the effects on. Click a picture for its full size (7824 x 5156).

![The original next to uwr: level 1](docs/images/uwr-collage-1.jpg)

![The original next to uwr: levels 2 to 8](docs/images/uwr-collage-2.jpg)

## What is in it

- **Renderer.** Vulkan, the default on both systems since 2026-11-16; the older Direct3D 12 renderer is still there
  on Windows (the Esc menu's Renderer row). Hardware ray tracing (light, shadows, bounced light), DLSS / DLAA and
  DLSS Ray Reconstruction, HDR output. DLSS 5 exists in the Direct3D 12 renderer only and is not being carried on.
- **Walls and floors.** The game's own textures, or upscaled to 256 / 512 pixels in the same colours; normal maps,
  joint shadows (AO) and relief shadows, with maps made at each texture size.
- **Effects.** Particles, decals, water, fog and light shafts, firelight with the eye's adaptation.
- **Sound.** The Roland MT-32's own voices, placed in space (OpenAL Soft, headphone 3D); walls and doors muffle,
  rooms ring by their size, sound comes round corners; ray-traced echoes through the level's shape (Steam Audio);
  footsteps by the floor.
- **Interface.** A full-screen 3D view with our own HUD, character sheet, inventory, automap and conversation
  screen, one smooth font, mouse-look and modern keys. The 1992 screen as it was is one menu row away.
- **Creatures.** Optional AI levels above the original's (Original, Devious, Monstrous), by rules or by a small
  language model run on the player's machine. Original is the game as shipped.
- **The original's 3D models** (doors, portcullises, bridges, weapons on the floor) decoded from UW.EXE and drawn.

## Where it is going

- One Vulkan renderer for Windows and Linux; new renderer work goes there, Direct3D 12 stays as it is.
- Linux: the target is Linux Mint. The Linux programs are cross-built on Windows
  ([docs/LINUX_BUILD.md](docs/LINUX_BUILD.md)) and have so far only been start-checked under WSL.
- The rules core keeps being checked routine by routine against the original ([ROUTINES.md](ROUTINES.md)).
- Open at the last cut (2026-11-16): the newest sound rows and the Vulkan renderer's Linux side have been built and
  started, not yet played through.

## Where to start

| You want to | Read |
| --- | --- |
| know the rules every session follows | [CLAUDE.md](CLAUDE.md), [DECOMP_RULESET.md](DECOMP_RULESET.md) |
| see status and next steps | [NOTES.md](NOTES.md), [ROADMAP.md](ROADMAP.md), the newest `HANDOFF-*.md` |
| see which original routines are ported and proved | [ROUTINES.md](ROUTINES.md), [re/README.md](re/README.md) |
| read the decoded game mechanics | [MECHANICS.md](MECHANICS.md) |
| understand the engine source | [src/README-src-a.md](src/README-src-a.md), [src/README-src-b.md](src/README-src-b.md) |
| understand the Windows backend | [port/README.md](port/README.md) |
| build for Linux, and how the renderer got there | [docs/LINUX_BUILD.md](docs/LINUX_BUILD.md), [docs/LINUX_RENDERER.md](docs/LINUX_RENDERER.md) |
| understand our additions to the rules core | [underworld-exhumed-additions.md](underworld-exhumed-additions.md) |
| run or write tests | [tests/README.md](tests/README.md) |
| use the proof, patch and build scripts | [tools/README-tools-a.md](tools/README-tools-a.md), [tools/README-tools-b.md](tools/README-tools-b.md) |
| make normal maps | [tools/laigter-cli/README.md](tools/laigter-cli/README.md) |
| learn about the AI-driven creatures / models | [LLM_NPC_AI.md](LLM_NPC_AI.md), [docs/MODELS.md](docs/MODELS.md) |
| see design plans and specs | [docs/superpowers/](docs/superpowers/) |
| see third-party credits | [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) |

## How the pieces fit

- **Rules core**: underworld-exhumed's C (a byte-matching rebuild of UW.EXE, MIT) plus our own glue files. Our
  additions to it are listed in [underworld-exhumed-additions.md](underworld-exhumed-additions.md); its own files are
  not in this repository.
- **Our engine** (`src/`): front end, the two renderers (`render_vk*.c` Vulkan, `render.c` Direct3D 12, one set of
  HLSL shaders in `shaders/` for both), audio, inventory, level and world handling.
- **Backends** (`port/`): `win32/` is the Windows platform layer and holds the files both systems share (front
  end, menu, audio, acoustics, input); `sdl3/` is the Linux platform layer over SDL3.
- **Proof** (`tools/proof_*.py`, `tools/emu.py`): each ported routine is run beside the original under an emulator
  on the same inputs; matching routines are listed in `re/proved.txt`.
- **Data** (`data/`): our own normal, height and occlusion maps only.

## Notes and warnings

- The `tools/*_patch.py` scripts were each run once, in 2026-11, to edit the source. They are history; do not re-run
  them (their safety asserts do not always refuse a second run).
- Root `build.cmd`, `run-tests.cmd` and `play.cmd` serve our own C engine (`build/uw.exe`); the exhumed-based port
  is built by `tools/portwin32.py`.
- `src/render.c`, `src/render_vk*.c`, `src/render.h` and `src/shaders/` are not described in a guide yet; the Vulkan
  renderer's plan and ledger are in `docs/superpowers/plans/2026-11-15-vulkan-renderer.md`.

## Known mismatches and open questions

The eight listed here until 2026-11-16 were checked against the source and the original's disassembly, and put right:

- Fixed in the code: the inventory panel's "Carrying" weight now counts the item on the pointer, as the carry limit
  does; `main.c`'s two options that were tested twice have names of their own (`--conv=N`, `--frame-picks=`), so
  the later branches can be reached. `run-tests.cmd`'s chargen line still passes `--picks=` and behaves as before.
- Fixed in the comments (the code was right): only `data/lights.txt` is read (`lights.h`, `main.c`); `Mesh.grid`
  byte 2 is the floor's terrain (`level.h`); creatures are hostile by their attitude bits (`game.c`); `core.py`
  takes no argument; `tools/relief_section.c.txt` is marked as history (relief v4; `src/normals.c` has v7).
- Not mismatches after all: the two songs' addresses in `songs.h` and `songs.c` are both in UW.EXE (a block's start
  and the lines inside it; the header now names both), and `sfx_stop`'s test of slots 0-7 matches the original's
  8-bit mask (four slots are ever set).
- Still as it was: `tools/laigter-cli/pixelart.preset` is not used by `make-normals.cmd`; it is kept as the
  settings `sprite_op.preset` was made from.

The per-area guides list more ("UNSURE" at the end of `src/README-src-a.md` and `src/README-src-b.md`); those have
not been checked yet.
