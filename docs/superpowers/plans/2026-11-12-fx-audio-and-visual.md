# Effects: OpenAL Soft, Steam Audio, volumetric fog and god rays, particles, decals, water: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans. One commit per task, run-tests.cmd's verdict quoted, a before/after window shot for every visual task kept under work\fx\.
> Builder: the uwr session, after the hunters and the LLM default task. Reviewer: the second uwr session's loop.
> Order: Task 1 (audio events + OpenAL), Task 3 (particles: it gives the event stream the others reuse), Task 4 (decals), Task 5 (water), Task 2 (fog), Task 6 (Steam Audio: last, the heaviest).

**Goal:** six presentation-only additions to uw-new: positional sound effects, acoustics traced through the level, volumetric fog with light shafts, particles, decals, and water with refraction, ripples and caustics. The game's rules, its choice of what plays and when, and every number stay the original's.

**Architecture:** one event stream from the game to the backend, `port_fx_event(kind, x, y, z, fx, mob)`, fed from the places the original already decides an effect happened (SOUND.C play_effect and play_effect_here, which hunt.c's noise hook already taps; the effect objects ANIMOBJ.C creates; DAMAGE.C's hits; a splash). The renderer DLL (src\uwview.c over src\render.c, D3D12) gets `uwv_fx_event` and three new passes (particles, decals, fog); the backend gets an audio module (port\win32\uwaudio.c) over OpenAL Soft, with Steam Audio as its optional acoustics. Every feature is a setting in data\settings.txt and a row in our Esc menu, off in every proof and replay (the renderer is not driven in replays' checks; the audio module is off unless the setting is on and never in hidden runs).

**Tech Stack:** C (clang, MinGW target), D3D12 compute and pixel shaders (HLSL, compiled as our other shaders are), OpenAL Soft (LGPL 2.1, used as a DLL: soft_oal.dll, no static link), Steam Audio (Apache 2.0, phonon.dll). Both SDKs go under O:\appsnotonc\<name>, never C:; ask the user before downloading either (the project's rule for anything fetched).

**Spec:** the user, 2026-11-12: "OpenAL Soft, Volumetric fog and god rays, Particles, Decals, Water, Steam Audio plan these"; the replaceable-parts review (work\REVIEW-2026-11-12-replaceable.md): presentation may be ours, mechanics never.

## Global Constraints
- No rule number changes: nothing here reads back into the game; the event stream is one-way (game to backend).
- Settings (data\settings.txt, uwview.c's S struct, Esc menu rows): `fx_audio=0|1`, `fx_acoustics=0|1`, `fx_fog=0..3`, `fx_particles=0|1`, `fx_decals=0|1`, `fx_water=0|1`. All default 0 until each is judged by the user in a visible run; then the user decides the dist default.
- Off = byte-identical replays and the same frames as today (UWVIEW_COMPARE on the existing shots).
- The MT-32 path (Munt) stays the sound of the game; OpenAL only adds positional digital samples where a sample exists, else the effect plays as today.
- A sample pack is data, not code: `data\sfx\NNN.wav` by SOUNDS.DAT entry; none shipped until the user provides or approves one. Without a pack, Task 1 still positions nothing but is proved by the harness.
- Frame budget: the three passes together under 1.5 ms at 1920x1080 on the 4090 (UWPORT_TIMING); fog scales by `fx_fog` 1-3.

## Review Focus
1. An effect at a position with no line of sight to the player (behind a wall): fog shafts and particles must not show through walls (depth-tested), sound must be occluded only with acoustics on. Tests in Tasks 2, 3, 6.
2. A level change mid-effect: every particle, decal and fog volume of the old level dropped; nothing drawn from stale positions. Test in Task 3 and 4.
3. The renderer falling back to GDI (V.failed): all passes absent, no crash, the audio module unaffected. Test in Task 3.
4. A save made with decals on the floor: decals are not saved (presentation); a load shows a clean floor; documented. Test in Task 4.
5. Audio device missing or OpenAL DLL absent: `fx_audio` silently off with one log line; the MT-32 path untouched. Test in Task 1.

---

### Task 1: the effect event stream and OpenAL Soft positional effects

**Files:**
- Create: `port/win32/uwaudio.c`, `uwaudio.h` (uo-new): OpenAL loaded at run time (LoadLibrary soft_oal.dll from dist\UW1\bin; functions by GetProcAddress), one context, up to 32 sources, the listener at the camera.
- Modify: `underworld-exhumed/uw1/src/sound/SOUND.C` play_effect / play_effect_here (port-only, beside the existing port_hunt_noise calls): `port_fx_event(FX_SOUND, x, y, z, fx, mob)`; `port/sys/portgame.h` the declaration; `port/win32/plat_win32.c` (uwaudio_listener each frame from the camera snapshot; the setting row).
- Test: `tools\fxcheck.py audio` (uo-new): a hidden run with `UWPORT_FX_LOG=1` asserting `fx: sound N at X,Y` lines for a weapon swing and a door, and a run with `fx_audio=1` and no soft_oal.dll asserting `fx: audio off (no soft_oal.dll)`; a C harness `tests\uwaudio_test.c` that opens OpenAL (if the DLL is beside it), plays a generated 440 Hz WAV at (+10, 0, 0) and checks alGetError is 0 (skipped with a note when the DLL is absent).

**Interfaces:**
```c
/* portgame.h (game side, port only) */
enum { FX_SOUND = 1, FX_HIT, FX_SPELL, FX_SPLASH, FX_DEATH, FX_DOOR, FX_FIRE };
void port_fx_event(int kind, int x, int y, int z, int fx, int mob);   /* x, y in 1/8 tiles (play_effect's), z the object's; fx SOUNDS.DAT entry or item id; mob the mobile or 0 */
/* uwaudio.h (backend) */
int  uwaudio_init(const char *bindir, const char *datadir);   /* 1 ok; 0 = off (logged) */
void uwaudio_listener(float x, float y, float z, float yaw);
void uwaudio_event(int kind, float x, float y, float z, int fx);   /* plays data\sfx\<fx>.wav at the place if the file exists (cached), else nothing */
void uwaudio_shutdown(void);
```
- Consumes: the camera as uwgame.c snapshots it (eye x, y, z, facing); the game's play_effect position (1/8 tiles) converted to world units as the renderer does (uwview.c's camera scale).

- [ ] **Step 1: Write fxcheck.py audio and uwaudio_test.c; run: FAIL (no lines, no file).**
- [ ] **Step 2: Implement port_fx_event (a ring of 64 events under the existing uwgame.c lock; the backend drains it each frame: uwaudio and, later, uwv_fx_event), uwaudio.c, the setting and the log.**
- [ ] **Step 3: Run the checks: PASS. run-tests.cmd. Commit.**

### Task 2: volumetric fog and god rays

**Files:**
- Create: `src\shaders\fog.hlsl` (compute), `src\render_fog.c`; Modify: `src\render.c` (the pass after lighting, before the HUD), `src\uwview.c` (settings `fx_fog`, the Esc row "Fog").
- Test: `tools\fxcheck.py fog`: UWVIEW_COMPARE shots of level 1's entrance with fx_fog 0 and 2; the fog shot differs, and a pixel behind a wall from a torch (a chosen window coordinate) is unchanged (no light through walls).

**Interfaces:** `void render_fog(const GpuLight *lights, int n, int quality)` reads the depth buffer and the light list already built for the ray tracer; ray-marches 16/32/64 steps per pixel (quality 1-3) through a uniform density `S.fog_density` with per-light in-scatter (a Henyey-Greenstein phase, g 0.3), shadowed by the ray tracer's visibility for quality 3 and by the depth buffer alone below. God rays are the same pass at doorways and grates: no separate code; the shadowing gives them.
- [ ] Steps: write the check (FAIL: no difference); implement; PASS; UWPORT_TIMING under 0.8 ms at quality 2; commit.

### Task 3: particles

**Files:**
- Create: `src\shaders\particles.hlsl` (compute update + a billboard draw), `src\render_particles.c`; Modify: `src\uwview.c` (`uwv_fx_event`, emitters from the level's burning objects it already lists as lights, the setting), `port/win32/plat_win32.c` (drains port_fx_event to uwv_fx_event).
- Test: `tools\fxcheck.py particles`: a shot near a brazier with fx_particles 1 vs 0 differs above the flame only (a region test); after `cheat goto 2 ...` no particle survives (the renderer's count readback `uwv_status` shows 0 live at the first frame of level 2); with UWPORT_GDI=1 the run is clean.

**Interfaces:**
```c
UWV_API void uwv_fx_event(int kind, float x, float y, float z, int fx, int mob);
/* emitters (render_particles.c): sparks and embers above every burning light (continuous), a burst per FX_HIT (blood or sparks by the hit creature's kind: the renderer knows the item), FX_SPELL by the effect object's item (fire, ice, acid, magic), FX_SPLASH at water, dust motes in lit shafts when fog is on */
```
- GPU: one structured buffer of 65536 particles (pos, vel, life, kind), one compute dispatch per frame (gravity, drag, kill), one indirect draw of camera-facing quads lit by the nearest light, depth-tested, soft against the depth buffer.
- [ ] Steps: check (FAIL); implement; PASS; timing under 0.4 ms; commit.

### Task 4: decals

**Files:**
- Create: `src\render_decals.c`, `src\shaders\decal.hlsl`; Modify: `src\uwview.c` (FX_HIT and FX_SPELL leave decals, FX_DEATH a larger one; the setting), level change clears them.
- Test: `tools\fxcheck.py decals`: three hits on a creature (the fight script from work\t12) leave 1-3 blood decals on the floor under it (a region test on the shot), a level change clears them, a save and load shows none (documented as presentation).

**Interfaces:** `void render_decals_add(float x, float y, float z, int kind, float size, float yaw)` up to 512 per level (a ring); drawn as floor and wall projected quads after the geometry pass with a normal-biased depth test; kinds: blood (by creature kind: red, green for lizardmen and slugs, black for undead), scorch, ice, acid.
- [ ] Steps: check (FAIL); implement; PASS; commit.

### Task 5: water

**Files:**
- Modify: `src\render.c` (water tiles as a separate surface: the floor textures the game cycles for water, which uwview.c already knows for colour cycling), `src\shaders\water.hlsl`, `src\uwview.c` (the setting, FX_SPLASH ripples).
- Test: `tools\fxcheck.py water`: a shot over level 1's water with fx_water 1 vs 0 differs on the water only; a splash event adds a ripple (two shots 300 ms apart differ on the water); ray tracing off still gives refraction (screen-space) and caustics come only with ray tracing on.

**Interfaces:** the water surface at the tile's height with a normal map of travelling waves plus ripples from FX_SPLASH (up to 16 live, 2 s each); refraction by the depth buffer below; caustics from the flame lights projected through the surface when the ray tracer is on; the original's colour cycling kept as the surface's base colour so the look stays the game's.
- [ ] Steps: check (FAIL); implement; PASS; commit.

### Task 6: Steam Audio acoustics

**Files:**
- Modify: `port/win32/uwaudio.c` (phonon.dll loaded at run time when `fx_acoustics=1`; a static mesh of the level built from the tile map: floors, ceilings and walls as quads with one material; direct path occlusion and a reverb estimate per source, applied through OpenAL's gain and a convolution or the EFX reverb preset chosen from Steam Audio's RT60).
- Test: `tools\fxcheck.py acoustics`: the C harness places the listener and a source with a wall between (from a tile map fixture) and asserts the occlusion gain below 0.5, then without the wall above 0.9; the game run asserts `fx: acoustics on (phonon)` and no frame-time change (the simulation runs on the audio thread).

**Interfaces:** `int uwaudio_acoustics_level(const unsigned char *block)` rebuilds the mesh on uwv_level; the simulation thread updates every 100 ms.
- [ ] Steps: check (FAIL); implement; PASS; commit; the user judges reverb in a visible run (level 1's great hall against a corridor).
