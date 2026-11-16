# tests\ - test programs and verifier drivers

Most files here are small C programs. Some are full checks on their own. Others are drivers that a Python verifier
in `tools\` feeds input to: the verifier runs the same input on the original UW.EXE in the Unicorn emulator and
compares the outputs. CLAUDE.md explains why: "proved" means an emulator verifier compared our routine with the
original's and found them identical.

The everyday entry point is `run-tests.cmd` at the repository root. It builds through `build.cmd`, then runs
`build\check.exe`, a set of `build\uw.exe` screenshots into `build\shots`, `tools\verify.cmd`, and
`tests\xmidi_test.cmd 3000`. It ends with "all tests passed". It sets `UWPORT_LLM=0`, so no check ever loads the
language model.

Rules for running checks (CLAUDE.md): headless only, nothing that takes the user's focus, scratch files in `work\`.
Do not run underworld-exhumed's own test suites. The three tests under `underworld-exhumed\uw1\tests` that `build.cmd`
builds are ours (see `..\underworld-exhumed-additions.md`).

## Overview

| File | Kind | Built by | Run by |
| --- | --- | --- | --- |
| `rtest.c` | verifier driver | `build.cmd` -> `build\rtest.exe` | `tools\verify.cmd` (`verify.py`, `sfx_verify.py`, `songs_verify.py`, `collide_verify.py`) |
| `coretest.c` | verifier driver | `build.cmd` -> `build\coretest.exe` | `tools\coreproof.py`, the `tools\proof_*.py` scripts (`tools\proofs.cmd`) |
| `coreworld.c` | smoke test | see UNSURE below | by hand |
| `models_test.c` | check | `build.cmd` -> `build\models_test.exe` | `build.cmd` itself (`build\models_test.exe game`) |
| `uwinput_test.c` | unit test | `build.cmd` -> `build\uwinput_test.exe` | `tools\verify.cmd` |
| `uwaudio_test.c` | unit test | `tools\fxcheck.py audio` | `tools\fxcheck.py audio` |
| `xmidi_test.c` + `xmidi_test.cmd` + `xmidi_scripts\` | verifier driver | `xmidi_test.cmd` | `run-tests.cmd` (`xmidi_test.cmd 3000`) |
| `midicheck.c` + `midicheck.cmd` | diagnostic | `midicheck.cmd` | by hand |
| `mt32test.c` + `mt32test.cmd` | diagnostic | `mt32test.cmd` | by hand |
| `uwtour.c` | renderer tour | `tools\buildview.cmd` -> `build\uwtour.exe` | by hand |
| `uwmind_bench.c` | benchmark | `tools\buildmind.cmd` -> `build\llama\bin\uwmind_bench.exe` | by hand |
| `uwmind_score.c` | benchmark | `tools\buildmind.cmd` -> `build\llama\bin\uwmind_score.exe` | by hand |
| `uwmind_tokcheck.c` | check | `tools\buildtokcheck.cmd` -> `build\llama\bin\uwmind_tokcheck.exe` | `tools\buildtokcheck.cmd` |

## The files

### rtest.c - our hand-ported routines, for the emulator verifiers

This program runs uo-new's own ports (`src\orig.c`, `src\sfx.c`, `src\songs.c`, `src\collide.c`) on lines read from
stdin. `tools\verify.cmd` runs it beside UW.EXE in the emulator, and every answer must match.

- **Default mode:** each line is `<seed hex> <routine> <args...>` and the answer is `<result> <seed after, hex>`.
  The routines are `rand`, `dice`, `check` (skill check), `resist`, `isqrt`, `sin`, `getexp` and `killexp`
  (`main`, line 206).
- **`rtest sfx SOUNDS.DAT`** (`sfx_mode`, line 25): the sound effects. Operations are `play`, `at`, `tick`, `stop`,
  `stopall`, `tstat`, `lock`. Every driver call is printed as it happens. Used by `tools\sfx_verify.py`.
- **`rtest songs`** (`songs_mode`, line 83): the music choice. Used by `tools\songs_verify.py`.
- **`rtest collide ARK TERRAIN COMOBJ`** (`collide_mode`, line 144): the terrain collision on the real levels
  (`level`, `oc`, `cp`, `tc`, `slope`). Used by `tools\collide_verify.py`.

To run: `run-tests.cmd`, or `tools\verify.cmd` after `build.cmd`.

### coretest.c - the rules core, driven by name

The rules core is underworld-exhumed's byte-matched C, compiled unchanged into `build\core\core.a` by
`tools\core.py`. `coretest.c` drives that core through commands on stdin. `tools\coreproof.py` sends the same
commands to UW.EXE in the emulator and compares everything.

Commands (the file's header):

- `seed` / `gseed`: set and read the rand state.
- `w` / `r`: write and read bytes.
- `p` / `q`: write and read pointers, as SPECs (`0`, `L+HEX` the level block, `S+HEX` scratch, `@NAME+HEX` a name).
- `call` / `callp`: call a routine. The answer is `ret HEX` and `calls N ...`, the outside calls it made.
- `exe`: the math tables from UW.EXE.
- `dirs`: the game and write folders.
- `traceon` / `traceoff`: the functions entered.
- `known`.

It is built by `build.cmd`, linked with `build\gen\core_tab.c`, `build\gen\core_stubs.c` and `build\core\core.a`.
Run it through `tools\proofs.cmd` (one `tools\proof_<file>.py` per source file). A passing proof adds its routines
to `re\proved.txt` through `tools\proved.py`.

### coreworld.c - the core runs the world

A smoke test of the rules core as a game. It does the original's start-up (UWEDIT.C's `init_world` without the
screen, sound and input), starts a new game on level 1, then runs `check_physics` for N steps with the player walking
forward and turning right between steps 100 and 120. Every 40 steps it prints the player's tile and position.

```
coreworld GAME_DIR WRITE_DIR [steps]     (default 200 steps)
```

It must be compiled like the core (`compat.h` force-included). It was added with commit b9b442f.

### models_test.c - the 3D models from UW.EXE

Loads the 32 models that `src\models.c` decodes from `game\UW.EXE`. Each model must run to its end with no unknown
opcode, give triangles, and stay inside a sane box. It prints the counts beside `tools\models.py`'s face counts.

```
build\models_test.exe GAME_DIR     (build.cmd runs it with "game"; it fails the build on an error)
```

### uwinput_test.c - the window's input module

Runs `port\win32\uwinput.c` alone with a fake backend that records every output call (game key, game pointer, each
owner's handler, our actions). The cases cover:

- which owner each press goes to;
- that a release goes to the owner of its press;
- the early release of presses that are no longer valid;
- the key table;
- the swallowed ` key;
- mouse-look's want.

Some cases are named after findings in the input rewrite's review, for example `game_key_released_back_in_play`.
It prints `uwinput_test: N passed, M failed`, and the exit code is M. Built by `build.cmd`, run by `tools\verify.cmd`.

### uwaudio_test.c - positional effects over the loopback device

Runs `port\win32\uwaudio.c` over OpenAL Soft's loopback device, which renders into memory and plays nothing on the
speakers. The cases:

| Case | Check |
| --- | --- |
| `east` | An effect 3 tiles east: the right channel is more than 1.5x louder, alGetError is 0. |
| `west` | 3 tiles west: the left channel is louder. |
| `far` | 7 tiles away: not played. |
| `muted` | FX_MUTED: not played. |
| `none` | An effect with no sample file: not played, no error. |

```
uwaudio_test.exe BINDIR WORKDIR    (BINDIR holds soft_oal.dll, else "SKIP"; WORKDIR gets sfx\007.wav, a 440 Hz tone)
```

It prints one PASS / FAIL line per case, and the exit code is the number of failures. It is built and run by
`python -I tools\fxcheck.py audio`, which works in `work\fx`.

### xmidi_test.c, xmidi_test.cmd, xmidi_scripts\ - the music driver

`src\xmidi.c` is our port of UW's MT32MPU.ADV music driver. `xmidi_test.c` plays a song the way
`tools\adv_music.py` has the original driver play it, and writes a record: `tick port value` for every byte to the
MPU-401, and `tick vr` for every retrace wait.

```
xmidi_test UW.MT song.XMI seqnum ticks out.txt          (song mode)
xmidi_test UW.MT --script script.txt out.txt            (script mode, script(), line ~45)
```

`xmidi_test.cmd [ticks]` (default 3000) builds `build\xmidi_test.exe`, then runs two checks:

1. `tools\xmidi_verify.py TICKS`: every song, compared with the original driver in the emulator.
2. `tools\xmidi_script.py tests\xmidi_scripts`: every script, run on the original driver in the emulator and on
   our port. The records (every byte, every wait, every returned value) must be identical.

Any difference fails. `run-tests.cmd` runs `xmidi_test.cmd 3000`.

**The script format** (`tests\xmidi_scripts\*.txt`). One command per line, words separated by spaces. Both readers
(`tools\xmidi_script.py` and `xmidi_test.c`'s `script()`) skip blank lines and lines starting with `#`, so `#`
lines are comments. Each script starts with one that says what it covers. H, a song handle, is the n-th `reg` line,
counting from 0. Return values go into the record as `TICK ret VALUE`.

| Command | Meaning |
| --- | --- |
| `at TICK` | Serve the driver until that tick. |
| `reg FILE.XMI N` | Register sequence N of an XMI file. |
| `start H`, `stop H`, `resume H`, `release H` | Control a song. |
| `vol H PCT MS`, `tempo H PCT MS` | Relative volume or tempo fades. |
| `status H`, `beat H`, `bar H`, `notes H CHAN` | Read a song's state. |
| `ctl H CHAN CON` | Read a controller's value. |
| `setctl H CHAN CON VAL` | Set a controller. |
| `branch H MARKER` | Jump to a branch marker. |
| `lock` | Lock a channel. |
| `relch C` | Release a channel. |
| `inst BANK PATCH` | Install a timbre from UW.MT. |
| `tstat BANK PATCH` | Read a timbre's status. |
| `protect BANK PATCH`, `unprotect BANK PATCH` | Protect or unprotect a timbre. |
| `cv STATUS D1 D2` | Send a channel voice message. |
| `display` | Write the display text ("Ultima Underworld"). |
| `sysex A B C` | Send a raw sysex. |
| `shutdown` | Shut the driver down. |

The full list is in `tools\xmidi_script.py`, `original()`, and in `xmidi_test.c`, `script()` (whose comment at
line ~40 lists the arguments).

| Script | Covers |
| --- | --- |
| `branch.txt` | Branch markers. |
| `fades.txt` | Volume and tempo fades, stop / resume, controllers. |
| `misc.txt` | Display, raw sysex, timbre protection, shutdown with two songs registered. |
| `rolandctl.txt` | The Roland controllers: sysex queues (32-46), patch reverb / bender, reverb, rhythm timbre, timbre protect, indirect. |
| `sfx.txt` | Sound effects: locked channels and UW.MT timbres (bank 1) with a song playing. |
| `twoseq.txt` | Two sequences, release while playing, restart. |

The scripts name the XMI files by absolute path (`O:/uw/uo-new/game/SOUND/...`).

### midicheck.c, midicheck.cmd - can Windows' MIDI output open?

Lists the MIDI output devices, opens and closes the MIDI mapper and device 0, and prints their volumes. It plays
nothing. Run `tests\midicheck.cmd`, which builds `build\midicheck.exe` and runs it.

### mt32test.c, mt32test.cmd - Munt and the ROMs

Loads `build\mt32emu.dll` with the CM-32L ROMs in `roms\` and renders a C major chord to `build\mt32test.wav`. This
checks that the DLL, the ROMs and the engine's way of loading them all work. Run `tests\mt32test.cmd`, from the
repository root (the .cmd changes there itself).

### uwtour.c - the renderer without the game

Runs the port's renderer (`src\uwview.c` over `src\render.c`) with no game and no window. It builds every level of
DATA\LEV.ARK in turn and takes pictures of the items in each, with the camera in an open square beside the item,
facing it at eye height. It checks that every level loads, and gives pictures to look at.

```
build\uwtour.exe GAME_DIR OUT_DIR [PER_LEVEL]     (UWVIEW_DATA names our extracted data)
```

It writes `OUT_DIR\levelN_K.png` (640x480) and `OUT_DIR\levelN_sheet.png`. Built by `tools\buildview.cmd`, which
`tools\portwin32.py` runs.

### uwmind_bench.c - the first step of our own LLM runner

Probes the PC (each DXGI adapter's VRAM and budget, cores, RAM, the ggml backends found), places a model on the GPU,
split, or on the CPU, and times one creature-sized decision held to a grammar.

```
uwmind_bench MODEL.gguf [cpu|gpu|N] [runs]
```

Built by `tools\buildmind.cmd` against `build\llama` (run `tools\buildllama.cmd` first). Background:
`LLM_NPC_AI.md`.

### uwmind_score.c - uwmind.dll timed as the game uses it

Compares the old way (`uwm_ask`, the whole prompt and a grammar) with the new one (`uwm_score`: one forward pass,
the choices' first tokens scored), alone and batched. It gives the before / after table in `LLM_NPC_AI.md`.

```
uwmind_score MODEL.gguf gpu|cpu [runs]     (run from build\llama\bin, uwmind.dll beside it)
```

### uwmind_tokcheck.c - the choice-token check

Includes `src\uwmind.c` and runs its load-time check of the choice tokens (`choice_token_ok`) on llama.cpp's
vocabulary-only files in `external\llama.cpp\models`. SentencePiece vocabularies (llama-spm, phi-3) must fail the
check. The GPT-2-style ones (qwen2, qwen35, gpt-2, llama-bpe) must pass.

```
uwmind_tokcheck VOCAB.gguf expect(0|1) ...     (exit code: the mismatches)
```

`tools\buildtokcheck.cmd` builds and runs it with the expected values.

## The root scripts: build.cmd, run-tests.cmd, play.cmd

These three are for **our own C engine** (`src\`, `build\uw.exe`). The game as played now, underworld-exhumed's port
on our backend, is built by `tools\portwin32.py` instead (see `port\README.md`).

- **`build.cmd [debug|new]`**: loads `O:\appsnotonc\llvm\env.cmd`, then:
  - compiles the HLSL shaders with DXC into `build\gen\*.h`;
  - builds `build\mt32emu.dll` once, through `external\mt32emu\build-mt32emu.cmd`;
  - builds the test programs `rtest.exe`, `hunternav_test.exe`, `models_test.exe` (and runs it), `hunterfile_test.exe`,
    `keyqueue_test.exe` and `uwinput_test.exe`;
  - builds the rules core (`tools\core.py` -> `build\core\core.a`) and `coretest.exe`;
  - builds `check.exe`, then `uw.exe`, then the DLSS 5 bridge `uw_nvngx.dll`.

  `debug` builds with `-O0 -g` and undefined-behaviour traps. `new` writes `build\uw_new.exe` and leaves the bridge
  alone, for when `uw.exe` is running and cannot be replaced. If a proof is running, `coretest.exe` is left as it was.
- **`run-tests.cmd`**: sets `UWPORT_LLM=0`, runs `build.cmd`, and makes the extracted reference data if it is missing
  (`tools\loose.py write`, `tools\assets.py export`). Then it runs:
  - `build\check.exe`;
  - about thirty headless `build\uw.exe --shot=build\shots\*.png ...` runs (levels, ray tracing, DLSS 5, the weapon,
    save and load into `build\savetest`, character creation, panels, spells, a door);
  - `tools\verify.cmd`;
  - `tests\xmidi_test.cmd 3000`;
  - a sound render, `build\uw.exe --sound-wav=work\sound_check.wav,5`.

  It prints "all tests passed". Nothing is written to `data\` or `game\`.
- **`play.cmd [args]`**: starts `build\uw.exe` with the arguments given. Settings are in `data\settings.txt` and keys
  in `data\keys.txt`; both are made on the first run.

## UNSURE

- `coreworld.c`: no build script in `tools\` or the `.cmd` files names it. The exact compile line is not recorded
  here.
