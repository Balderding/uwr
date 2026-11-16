# Steam Audio Acoustics Implementation Plan

> **Shelved by the user, 2026-11-14: not to be built for now.** The plan in force is
> `docs\superpowers\plans\2026-11-14-acoustics-ours.md` (our own rays and EFX, no Steam Audio).

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The game's sound effects are occluded by walls and shut doors and reverberate as the room they are in would,
computed by Steam Audio from the level's own geometry and per-texture materials.

**Architecture:** Steam Audio (phonon.dll, loaded at run time like soft_oal.dll) simulates; OpenAL Soft still plays.
A new module `uwacoustic.c` owns everything Steam Audio (context, scene, simulator, sources) and knows nothing of
OpenAL; `uwaudio.c` feeds it the listener and its 32 sources and applies what comes back. Two levels of the setting:
`fx_acoustics 1` applies occlusion / transmission as an EFX filter on each source and the simulated decay times as one
EFX reverb (all mixing stays OpenAL's); `fx_acoustics 2` replaces that reverb with Steam Audio's convolution of the
real impulse response per source, mixed by us into one stereo OpenAL streaming source. The level's faces come from
the renderer DLL, which already holds the level (`uwv_level`).

**Tech Stack:** C17 (clang, MinGW target), Steam Audio 4.8.1 C API (Apache 2.0; SDK in
`O:\appsnotonc\steam-audio\4.8.1\extracted\steamaudio`, DLL already in `dist\UW1\bin\phonon.dll`), OpenAL Soft 1.25.2
with its EFX extension, Munt for the effects' PCM (unchanged).

**Spec:** `docs\superpowers\plans\2026-11-13-sound-and-acoustics.md`. This plan replaces its Tasks 3, 4, 5 and 6 and
changes its Task 2 (materials gain a scattering value and the faces go to Steam Audio as a mesh). Its Task 1 (the
caller's volume) is already in: `PortFxEvent.vol` reaches `uwaudio_event`. Its Task 7 (footsteps by material) stays
there, untouched by this plan.

## Global Constraints

- Presentation only. What plays, when, where and at what velocity are the game's; nothing here is read by the game.
  `fx_acoustics 0` (and every proof, every existing check) leaves the output as it is today: `run-tests.cmd` passes
  at every commit and its verdict is quoted in the commit message.
- The original's falloff stays: full within a tile, nothing beyond six, linear between (OpenAL's
  `AL_LINEAR_DISTANCE_CLAMPED`). Steam Audio's distance attenuation and air absorption are never applied to the
  direct sound.
- The music never enters OpenAL or Steam Audio.
- Every check is hidden: OpenAL Soft's loopback device into memory or a WAV. Nothing is heard on the machine. The
  user judges by ear in a visible run before `dist\UW1\data\settings.txt` gets `fx_acoustics` above 0.
- phonon.dll is never linked: `LoadLibraryExA` from `bin\`, functions by `GetProcAddress`. Missing DLL, a missing
  function or `iplContextCreate` failing: one line `fx: acoustics off (<why>)`, and the effects play as with
  `fx_acoustics 0`.
- Nothing on C:. Scratch in `O:\uw\uo-new\work\acoustics`. Never delete; only add. Build through `work\locked.sh`;
  commit by explicit path, one commit a task.
- Steam Audio works in metres, right-handed, y up, -z ahead: the same frame uwaudio.c already gives OpenAL
  (`x, y, -z`), scaled by `acoustics_tile_m` (settings.txt; default 2.4, **a guess** to be judged by ear: it sets
  how long every room rings).

## What Steam Audio does not do (so nobody expects it)

| Not done by Steam Audio | Who does it |
| --- | --- |
| Playing or mixing sound, the sound card | OpenAL Soft, as now |
| Making the Roland effects' sound | Munt, as now |
| Knowing the level | us: Task 2 hands it the faces; doors are ours to add and remove |
| Knowing stone from wood | us: `data\acoustics.txt` (Task 1) |
| Sound bending round a corner as a located source ("pathing") | only with probes baked per level; **not in this plan** (Task 7 says what it would take). Without it a sound round a corner is muffled directly and heard through the room's reflections |
| True diffraction, sound through a keyhole or a portcullis's bars | nothing; a portcullis is left acoustically open |
| Anything beyond six tiles | nothing: the game plays nothing there, so nothing echoes from afar |
| Footsteps that differ by floor | the old plan's Task 7 |
| The music, the cutscenes' speech | untouched |

Costs: phonon.dll is 53 MB; the reflection simulation takes its own threads (2 here), never the frame; reflections
follow a change (a door opening, walking into the hall) within a few tenths of a second, not at once.

## Review Focus

1. A level change, a load or a teleport while the simulation thread is mid-run: the old scene must stay alive until
   the run ends; no crash, the reverb moves to the new room within a second (Task 3's `swap` test).
2. `fx_audio` or `fx_acoustics` toggled in the Esc menu during play (`fx_audio_start(1)` shuts everything down and
   starts again): the worker thread ends first, no call into a freed phonon object (Task 3's `restart` test).
3. An effect at the listener (`x < 0`, play_effect_here) or a source closer than 0.1 tile: never occluded, no
   division by a zero distance (Task 4's `here` test).
4. A source stolen by a newer effect (all 32 playing) while its simulation slot still holds the old place: the new
   sound must not start with the old sound's occlusion (Task 4's `steal` test).
5. A door opened while its own sound plays from the far side: the filter eases open over about 0.3 s, no click
   (Task 4's `door` test measures the energy before and after).

---

### Task 1: The materials table

**Files:**
- Create: `data\acoustics.txt`, `port\win32\uwacmat.c`, `port\win32\uwacmat.h`, `tools\acoustics_list.py`
- Test: `tests\uwacmat_test.c`, `tools\fxcheck.py` (new mode `materials`)

**Interfaces:**
- Produces:
  ```c
  typedef struct { char name[16]; float absorb[3], scatter, transmit[3]; } UwaMaterial;   /* low, mid, high */
  enum { UWA_FLOOR, UWA_CEILING, UWA_WALL, UWA_DOOR };
  int uwacmat_load(const char *datadir);            /* presets read; 0 and one log line when the file is bad */
  int uwacmat_count(void);
  const UwaMaterial *uwacmat_get(int preset);
  int uwacmat_for(int kind, int tex);               /* preset index; tex = the texture's file number, a door's item */
  ```

`data\acoustics.txt`, the format and the starting values (stone, wood, metal and carpet are Steam Audio's documented
presets for rock, wood, metal and carpet as recalled: **check them against the SDK's documentation before the
commit**; water and dirt are ours):

```
preset stone   absorb 0.13 0.20 0.24  scatter 0.05  transmit 0.015 0.002 0.001
preset wood    absorb 0.11 0.07 0.06  scatter 0.05  transmit 0.070 0.014 0.005
preset metal   absorb 0.20 0.07 0.06  scatter 0.05  transmit 0.200 0.025 0.010
preset carpet  absorb 0.24 0.69 0.73  scatter 0.05  transmit 0.020 0.005 0.003
preset water   absorb 0.01 0.01 0.02  scatter 0.05  transmit 0.000 0.000 0.000
preset dirt    absorb 0.15 0.30 0.45  scatter 0.30  transmit 0.000 0.000 0.000
wall * stone
floor * stone
ceiling * stone
door * wood
```

A line `wall 012 wood` names `data\textures\walls\012.png`; `floor NNN` names `data\textures\floors\NNN.png`;
`door 0x141` a door's item number (0x140-0x14F). `*` is the fallback of its kind; the last matching line wins.

- [ ] **Step 1:** `tests\uwacmat_test.c`: a fixture file with the ten lines above plus `wall 012 wood`; asserts
  `uwacmat_count() == 6`, `uwacmat_for(UWA_WALL, 12)` is wood, `uwacmat_for(UWA_WALL, 13)` is stone,
  `uwacmat_for(UWA_DOOR, 0x140)` is wood, wood's `transmit[0] == 0.070f`; a file with an unknown preset name on a
  texture line returns 0 from `uwacmat_load`; a missing file returns 0. Run through a new `test_materials` in
  fxcheck.py: FAIL (nothing to build).
- [ ] **Step 2:** Implement `uwacmat.c`. Run: PASS.
- [ ] **Step 3:** `tools\acoustics_list.py`: for levels 1-9 print each wall and floor texture number in use with the
  preset it resolves to and the count of those on `*`. Look at each PNG that is plainly not stone (planks, water,
  lava, earth, rugs, metal plates) and add its line to `data\acoustics.txt`. The check `materials` also asserts the
  tool exits 0 and every texture resolves.
- [ ] **Step 4:** `run-tests.cmd`, commit.

Half a day.

### Task 2: The level's faces from the renderer

**Files:**
- Create: `src\acoustic.c`, `src\acoustic.h`
- Modify: `src\uwview.c` (two exports), `port\win32\plat_win32.c` (two `GetProcAddress` lines beside `uwv_fx_event`),
  the DLL's source list in the build
- Test: `tools\fxcheck.py` mode `faces`

**Interfaces:**
- Produces (the DLL):
  ```c
  typedef struct { float v[4][3]; unsigned short tex; unsigned char kind, pad; } UwvAcQuad;   /* tiles; kind UWA_* */
  typedef struct { int id, shut; UwvAcQuad leaf; } UwvAcDoor;   /* id the door's object number; tex its item */
  UWV_API int uwv_acoustic_quads(UwvAcQuad *out, int cap);      /* the level built now: count (0 none) */
  UWV_API int uwv_acoustic_doors(UwvAcDoor *out, int cap);      /* as they stand this frame */
  UWV_API unsigned uwv_acoustic_epoch(void);                    /* changes whenever level_build made a new level */
  ```

`acoustic.c` builds from `Level` (never from `Mesh`: no relief, no atlases): per open tile one floor quad and one
ceiling quad (a slope's floor on its plane, by `level.c`'s corner heights); a wall quad wherever a tile's side meets
a solid tile or a higher floor (from the lower floor to the higher, or to the ceiling); the diagonal tiles' wall
across the tile. `tex` is the texture's file number (`lv.walls[t->wall]`, `lv.floors[t->floor]`, the ceiling's as
`level.c:853` picks it). A door (`doors[]` in uwview.c): one quad across the whole doorway (the tile's full width,
`world_door_span`'s height), `shut = open < 0.5`; a portcullis (the door item `world_door_model_quads` draws with model 12) always reports `shut = 0`.

- [ ] **Step 1:** fxcheck `faces`: a hidden run on level 1 with `UWVIEW_ACOUSTIC_DUMP=FILE` (the DLL writes its
  quads once, a line each). Asserts: more than 2,000 quads; every coordinate within 0..64 and 0..4; the tile at the
  player's start has a floor quad under it and a ceiling quad at y 4.0; no quad has zero area; the door count equals
  the level's objects 0x140-0x14F. Run: FAIL.
- [ ] **Step 2:** Implement, export, load in plat_win32.c (`V.ac_quads`, `V.ac_doors`, `V.ac_epoch`). Run: PASS.
- [ ] **Step 3:** `run-tests.cmd`, commit.

One day.

### Task 3: Steam Audio started: context, scene, simulator, the worker

**Files:**
- Create: `port\win32\uwacoustic.c`, `port\win32\uwacoustic.h`,
  `external\steamaudio\include\phonon.h`, `phonon_version.h`, `phonon_interfaces.h` (copied from the SDK; Apache 2.0)
- Modify: `tools\portwin32.py` (the include folder, where `build\munt\include` is added at line 74),
  `tools\fxcheck.py` `clang()` callers, `THIRD-PARTY-NOTICES.md` (the headers' place)
- Test: `tests\uwacoustic_test.c`, fxcheck mode `acoustics`

**Interfaces:**
- Consumes: `UwaMaterial` (Task 1).
- Produces:
  ```c
  typedef struct { float v[4][3]; int mat; } UwaQuad;            /* tiles, the renderer's frame; mat a preset index */
  int  uwac_init(const char *bindir, int mode, int rate, int frame, float tile_m);   /* mode 1 or 2; 1 on */
  void uwac_shutdown(void);                                       /* the worker ended first */
  int  uwac_on(void);
  int  uwac_scene(const UwaQuad *q, int n, const UwaMaterial *m, int nm);   /* replaces the scene; queued */
  void uwac_door(int id, const UwaQuad *leaf, int shut);          /* queued; a no-op when unchanged */
  void uwac_listener(float x, float y, float z, float yaw);
  void uwac_source(int slot, float x, float y, float z, int on);  /* slot 0..31; x < 0: at the listener */
  void uwac_step(void);                                           /* one simulation run now (the checks, the worker) */
  int  uwac_direct(int slot, float *occlusion, float transmit[3]); /* 1 when a result for this slot's place exists */
  int  uwac_reverb(float rt60[3]);                                 /* at the listener; 1 when simulated */
  void uwac_thread(int run);                                       /* 1: the worker steps every 100 ms; 0: stops */
  ```

Decisions:
- `IPL_SCENETYPE_DEFAULT`. The static level is one `IPLStaticMesh` (each quad two triangles, `materialIndices` from
  `mat`); each shut door is its own small `IPLStaticMesh`, `iplStaticMeshAdd` / `iplStaticMeshRemove` then
  `iplSceneCommit`. All scene and source changes are queued and applied by whoever calls `uwac_step`, before
  `iplSimulatorCommit`, so nothing changes under a running simulation. A replaced scene is released after the swap.
- Simulator: flags direct + reflections; `reflectionType` parametric for mode 1, convolution for mode 2;
  `maxNumOcclusionSamples 16`, `maxNumRays 4096`, `numDiffuseSamples 32`, `maxDuration 2.0`, `maxOrder 1`,
  `maxNumSources 8`, `numThreads 2`, `samplingRate` / `frameSize` as given (48000 / 1024).
- Shared inputs each step: `numRays 2048`, `numBounces 16`, `duration 2.0`, `order 1`, `irradianceMinDistance 1.0`.
- Per source: direct flags occlusion + transmission only; `IPL_OCCLUSIONTYPE_VOLUMETRIC`, radius 0.5 m, 16 samples,
  `numTransmissionRays 2`. Mode 1 has one more source of our own at the listener for the reverb times.
- A direct result carries the place it was simulated for; `uwac_direct` returns 0 until a step has run since the
  slot's last `uwac_source(on)` (Review Focus 4).

- [ ] **Step 1:** `uwacoustic_test.c BINDIR`: a fixture of two rooms 4 x 4 x 4 tiles side by side sharing a stone
  wall, all stone. Cases, each printing PASS / FAIL:
  - `load`: `uwac_init` returns 1; with BINDIR a folder without phonon.dll it returns 0 and prints
    `fx: acoustics off (no phonon.dll)`.
  - `clear`: listener and source in the same room 2 tiles apart: after `uwac_step`, occlusion > 0.9.
  - `wall`: source in the other room: occlusion < 0.1 and `transmit[2] < transmit[0]`.
  - `reverb`: `uwac_reverb` gives 0.2 s < rt60[1] < 10 s; with every material carpet, rt60[1] under half of that.
  - `size`: the same room at 12 x 12 x 4 tiles: rt60[1] longer than the 4 x 4 room's.
  - `swap`: `uwac_thread(1)`, then `uwac_scene` with the other fixture 50 times 20 ms apart: no crash, `uwac_reverb`
    still answers.
  - `restart`: `uwac_shutdown` with the thread running, `uwac_init` again: 1.
  Run through fxcheck `acoustics`: FAIL (nothing to build); SKIP when BINDIR has no phonon.dll.
- [ ] **Step 2:** Implement. Run: all PASS. Record in NOTES.md the time one `uwac_step` takes on level 1's mesh.
- [ ] **Step 3:** `run-tests.cmd`, commit.

Two days.

### Task 4: Occlusion and transmission on the direct sound (`fx_acoustics 1`, first half)

**Files:**
- Modify: `port\win32\uwaudio.c`, `port\win32\uwaudio.h`, `port\win32\plat_win32.c` (`fx_pump`)
- Test: `tests\uwaudio_test.c`, fxcheck `acoustics`

**Interfaces:**
- Consumes: Task 3's `uwac_*`; Task 2's `V.ac_quads / ac_doors / ac_epoch`; Task 1's `uwacmat_*`.
- Produces: `void uwaudio_acoustic_scene(const UwaQuad *q, int n);` and
  `void uwaudio_acoustic_door(int id, const UwaQuad *leaf, int shut);` (the backend's hand-off, also the checks').

Decisions:
- uwaudio.c loads the EFX functions it needs by `alGetProcAddress` (constants copied from OpenAL Soft 1.25.2's
  `efx.h`, as the others are). No `ALC_EXT_EFX`: acoustics off with one line.
- One `AL_FILTER_BANDPASS` per source as its `AL_DIRECT_FILTER`. From a slot's result, per band
  `g[b] = occlusion + (1 - occlusion) * transmit[b]`: `GAIN = g[1]`, `GAINLF = min(1, g[0] / g[1])`,
  `GAINHF = min(1, g[2] / g[1])`. Eased toward the target once a frame in `uwaudio_pump` with a 0.3 s time constant;
  a source that starts playing waits for no result: it starts open and eases, except when a result for the same
  place within half a tile already exists (then it starts there).
- `fx_pump`: when `V.ac_epoch()` changed, read the quads, map `tex` to a preset (`uwacmat_for`), hand them over; the
  doors every frame (`uwac_door` ignores the unchanged).
- A visible run starts `uwac_thread(1)`; a loopback run steps from `uwaudio_pump` every 100 ms of rendered frames, so
  the checks are repeatable.

- [ ] **Step 1:** uwaudio_test, with Task 3's two-room fixture and the 440 Hz sample, `UWPORT_FX_ACOUSTICS=1`:
  - `occluded`: the source through the wall renders under a third of the RMS of the same distance in one room.
  - `muffled`: the same pair with a 4 kHz sample loses more than the 440 Hz one does.
  - `door`: a wood door leaf closing a gap in the wall: shut, under half the open RMS; `uwaudio_acoustic_door(.., 0)`
    then 0.5 s rendered: back within 10 % of open, and no sample-to-sample jump above 4,000 in between.
  - `here`: an effect at `x < 0` with the listener against a wall: RMS equal to `fx_acoustics 0` within 2 %.
  - `steal`: 32 sources playing in the far room, a 33rd at the listener's side: its first 50 ms are not occluded.
  - `off`: with `UWPORT_FX_ACOUSTICS=0` every existing case renders sample for sample as before this task (a saved
    reference of the `east` case compared).
  Run: FAIL.
- [ ] **Step 2:** Implement. Run: PASS; the whole of fxcheck `audio` and `sides` still PASS.
- [ ] **Step 3:** A hidden game run (fxcheck `acoustics`): level 1, a door's own sound from the far side shut and
  open, from the loopback WAV: shut under half of open; the log has `fx: acoustics on (Steam Audio 4.8.1, mode 1)`.
- [ ] **Step 4:** `run-tests.cmd`, commit.

One and a half days.

### Task 5: The room's reverb from the simulated decay (`fx_acoustics 1`, second half)

**Files:**
- Modify: `port\win32\uwaudio.c`
- Test: `tests\uwaudio_test.c`, fxcheck `acoustics`

Decisions:
- One auxiliary effect slot with `AL_EFFECT_EAXREVERB`; every source sends to it (`AL_AUXILIARY_SEND_FILTER`, no
  filter). From `uwac_reverb`: `DECAY_TIME = clamp(rt60[1], 0.1, 20)`, `DECAY_HFRATIO = clamp(rt60[2] / rt60[1],
  0.1, 2)`, `DECAY_LFRATIO = clamp(rt60[0] / rt60[1], 0.1, 2)`; the other parameters OpenAL's defaults. Eased over
  0.5 s. The slot's gain is `acoustics_wet` (settings.txt, default 100, a percentage; the user's dial).
- Until the first result the slot's gain is 0 (no made-up room).

- [ ] **Step 1:** uwaudio_test: `tail`: the 440 Hz sample (0.5 s) in the 12 x 12 fixture room: the RMS of the 300 ms
  after the sample ends is above 2 x that in the 4 x 4 carpet room, and above 20 x that with `fx_acoustics 0`;
  `dry`: with `acoustics_wet 0` the tail equals `fx_acoustics 0`. Run: FAIL.
- [ ] **Step 2:** Implement. Run: PASS.
- [ ] **Step 3:** Hidden game run: the same effect at the same distance in level 1's great hall (around 32, 2) and
  in a corridor: the rt60 printed for each, the hall's longer, the hall's tail energy higher. Quote both in NOTES.md.
- [ ] **Step 4:** `run-tests.cmd`, commit. **Stop here for the user's ear:** a visible run with `fx_acoustics 1`;
  `acoustics_tile_m` and `acoustics_wet` set from what they hear, then the dist default.

One day.

### Task 6: Convolution reflections (`fx_acoustics 2`)

What mode 1 cannot give: each sound's own early reflections and a tail that differs by where the source stands
(a sound in the next room rings with that room, arriving through the door).

**Files:**
- Modify: `port\win32\uwacoustic.c / .h`, `port\win32\uwaudio.c`
- Test: `tests\uwaudio_test.c`, fxcheck `acoustics`

**Interfaces:**
- Produces:
  ```c
  void uwac_wet_begin(int slot, const short *mono, int frames, int rate);   /* the sound this slot plays, from its start */
  void uwac_wet_end(int slot);
  int  uwac_wet_mix(short *stereo, int frames);   /* the next frames of every slot's reflections, mixed; frames = the frame size */
  ```

Decisions:
- Reflections are simulated for the 8 playing sources nearest the listener; the others stay dry.
- Per slot an `IPLReflectionEffect` (convolution, first-order ambisonics: 4 channels, 2.0 s), all into one
  `IPLReflectionMixer`; the mix decoded to stereo by `iplAmbisonicsDecodeEffect` (binaural with Steam Audio's
  default HRTF when `uwaudio_hrtf()` reports on, else stereo panning), rotated by the listener's facing.
- The sound's samples are resampled once to 48 kHz at `uwac_wet_begin` (linear) and read block by block from the
  slot's own cursor; after the sound ends the effect's tail is drained (`iplReflectionEffectGetTail`).
- Output: one stereo OpenAL source, relative to the listener, fed by `alSourceQueueBuffers` three blocks ahead
  (about 64 ms) from a thread of uwaudio.c in a visible run, from `uwaudio_pump` / `uwaudio_render` in a loopback
  run. Its gain is `acoustics_wet`. Mode 1's EFX reverb slot is off in mode 2.
- Occlusion (Task 4) is the same in both modes.

- [ ] **Step 1:** uwaudio_test with `UWPORT_FX_ACOUSTICS=2`: `tail2`: as Task 5's `tail`; `side`: listener in the
  middle of the 12 x 12 room, the source one tile from the east wall: with the direct sound muted (a test switch
  `UWPORT_FX_DRYOFF=1`) the first 80 ms of reflections are louder on the right; `next room`: source in the other
  fixture room with a doorway cut in the wall: reflections audible (RMS > 20) while the direct sound is occluded;
  `drain`: after the last tail no block is queued (`uwaudio_playing() == 0` within 3 s). Run: FAIL.
- [ ] **Step 2:** Implement. Run: PASS. Record the CPU time of one `uwac_wet_mix` block with 8 slots; if above 5 ms,
  drop to 1.0 s responses and say so in NOTES.md.
- [ ] **Step 3:** Hidden game run as Task 5's step 3 in mode 2; the frame time with and without (no change beyond
  noise: the work is on other threads).
- [ ] **Step 4:** `run-tests.cmd`, commit. The user compares modes 1 and 2 by ear; the Esc menu's "Acoustics" row
  becomes off / reverb / full.

Three days.

### Task 7 (not built; recorded so the choice is informed): pathing

Sound located as coming through the doorway needs probes: `iplProbeArrayGenerateProbes` over each level's floor,
`iplPathBakerBake` (minutes a level; baked once into `data\acoustics\levelN.probes`, rebaked when the level's shape
hash changes), `iplSimulatorRunPathing` and an `IPLPathEffect` per source in Task 6's mix. Doors need
`enableValidation` / `findAlternatePaths`. About four more days. Decide after Task 6 is heard.

## Order and total

1, 2, 3, 4, 5 (the user listens), 6 (the user compares). About nine working days; the first audible result after
Task 5 (six days).
