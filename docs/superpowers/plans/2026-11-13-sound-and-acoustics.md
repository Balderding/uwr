# Sound and acoustics: the plan (2026-11-13)

> Builder: whoever takes it, one commit a task, run-tests.cmd's verdict quoted, every check hidden (nothing heard on the
> machine: OpenAL Soft's loopback device rendered to a WAV, tools\fxcheck.py). The user judges the result in a visible
> run before a setting's dist default changes. Mechanics exact; all of this is presentation (what the game decides to
> play, when, and at what place and volume are the original's; how it reaches the ears is ours).

## Where sound stands (so the plan starts from facts)

- **The Roland is decoded and exact.** UW's own MT32MPU.ADV driver is ported byte for byte (src\xmidi.c, 24 songs
  identical), the effects' routines proved (sfx.c), SOUNDS.DAT (24 effects: a bank-1 timbre of SOUND\UW.MT, a note,
  a velocity, a length) and the music choice (songs.c) proved. The unit is Munt (mt32emu.dll) with the user's own
  CM-32L ROMs; the music mixes through the runtime's waveOut path.
- **Sound Blaster / AdLib.** UW1 has no digital effects: every effect is a MIDI note on the music card; only the
  cutscenes' speech is digital (SOUND\*.VOC). The FM path (ADLIB.ADV's YAMAHA + TVFX) runs in the exhume runtime
  against Nuked OPL3 when that card is chosen, but its driver is not ported routine by routine in our engine (NOTES
  "Sound", step 4): not needed for the Roland game; a later task if the user wants the FM sound as an option.
- **OpenAL and the Roland (2026-11-13, committed da67d62 / f2c1249).** OpenAL plays PCM only; Munt makes PCM. Each
  effect is rendered once at start by a Munt of uwaudio.c's own (set up as the driver sets the unit, the timbre
  installed as the driver installs it) into a mono buffer; OpenAL places it at the game's position with the
  original's falloff; the music driver's copy of that effect is sent at volume 0. Music stays out of OpenAL (stereo,
  not spatialised, no reverb send), exactly as it should. Also fixed: the MT-32 reads the pan controller inverted,
  so every Roland effect sat on the wrong side (fx_pan_fix).
- **EAX** is Creative's proprietary API of 1998-2005, gone since Vista; its open successor is OpenAL Soft's EFX
  (reverb, filters, sends), which we already load. Nothing better exists for a game of this size; Steam Audio
  (phonon.dll, already in dist\UW1\bin, Apache 2.0) sits above it for propagation.

## Task 1: the HUD's power gem (done in this session's next commit) and the effects' caller volume

- The caller's `vol` (play_effect's third argument, e.g. a softer landing) is not in the event: add it to
  PortFxEvent (vol, signed) and give OpenAL `AL_GAIN` = the original's `(e->vol + vol)` over `e->vol`, clamped as
  fx_play clamps. Check: a dropped item's thud (vol < 0) renders quieter than the same effect at vol 0 in the
  loopback WAV. Half a day.

## Task 2: acoustic materials (the shader-as-material step)

The textures are already named loose files (data\textures\*.png, with data\lights.txt the model of a per-texture
text table). Add `data\acoustics.txt`: one line a texture name (or a prefix: `wall_stone`, `floor_water`) -> a
preset, and the presets themselves:

```
preset stone   absorb 0.02 0.03 0.04  transmit 0.00
preset wood    absorb 0.10 0.08 0.10  transmit 0.05
preset carpet  absorb 0.05 0.25 0.55  transmit 0.00
preset metal   absorb 0.02 0.02 0.03  transmit 0.00
preset water   absorb 0.01 0.01 0.02  transmit 0.00
preset dirt    absorb 0.15 0.30 0.45  transmit 0.00
texture *      stone
texture water* water
```

The renderer already knows each tile face's texture (level.c / world.c); it exposes `uwv_acoustic_material(face)`
= the preset index. Nothing of this touches the game. Check: every texture of levels 1-9 resolves to a preset
(a tool prints the ones falling to `*`). One day.

## Task 3: the room probe and EFX reverb

- Each 100 ms, on the audio thread, 64 rays from the listener (a fixed Fibonacci sphere) through the tile grid and
  the model BVH (the ray tracer's: reuse src\render's BVH query on the CPU side, or a plain tile-grid march, which
  is enough for UW's box rooms). Each hit: distance, preset.
- Room size from the mean free path (the average hit distance: V/S ~ mfp/4 for a convex room), the average
  absorption per band from the hits; Sabine `RT60 = 0.161 V / (S a)`; high-frequency damping from the high band;
  pre-delay from room size (~ mfp / 343 s).
- EFX: one auxiliary effect slot with `AL_EFFECT_EAXREVERB`, `AL_EAXREVERB_DECAY_TIME`, `_DECAY_HFRATIO`,
  `_REFLECTIONS_DELAY`, `_LATE_REVERB_DELAY`, `_GAIN`; every world source gets the send (`AL_AUXILIARY_SEND_FILTER`);
  the music never does (it is not in OpenAL at all). Parameters lerped over 0.5 s.
- Setting `fx_acoustics 1` (the Esc row "Acoustics" exists). Check (hidden, loopback): the same effect at the same
  distance in the great hall (level 1, 32,2 area) and in a corridor: the hall's tail longer (RT60 estimate printed,
  and the WAV's energy 300 ms after the note-off higher by > 2x); no reverb when off. Two days.

## Task 4: occlusion and transmission

- Per playing source, each 100 ms: one ray source -> listener through the tile grid (doors as their open/closed
  state, the models' BVH optional). Blocked: gain by the blocking preset's transmit, plus a low-pass
  (`AL_FILTER_LOWPASS`, gainHF from the high band) on the direct path; the reverb send unchanged (sound still
  reaches the room). Lerped.
- Check (hidden): a door's own sound from the far side of a closed door 2 tiles away: direct energy under a third of
  the open door's; open: equal. One day.

## Task 5: early reflections (optional)

- The first-bounce hits of Task 3 nearest the source (up to 4): image sources at the mirrored positions, each a
  delayed, attenuated copy (`AL_EAXREVERB_REFLECTIONS_GAIN` / `_DELAY` are per slot, so true per-source early
  reflections need separate sources: 4 extra OpenAL sources per playing effect, capped at 8 effects). Judge by ear
  first; may stay out. One to two days.

## Task 6: Steam Audio instead of Tasks 3-5 (the alternative, the fx plan's Task 6)

- phonon.dll (loaded as soft_oal.dll is, no link): the level as a static mesh (floors, ceilings, walls, doors as
  quads) with the preset's absorption / scattering / transmission per triangle; `iplSimulatorRunDirect` for
  occlusion and transmission, `iplSimulatorRunReflections` for the reverb (its own ray tracer, on its own thread);
  the output as per-source direct gain + EFX reverb parameters, or Steam Audio's convolution reverb mixed by our
  own render into an OpenAL streaming source.
- Choose between 3-5 and 6 after Task 3 is heard: Task 3 is 300 lines of ours and the whole chain stays
  inspectable; Steam Audio is a 20 MB DLL with better reflections. Recommendation: Tasks 2-4 first; Steam Audio
  only if the user wants the reflections.

## Task 7: footsteps and impacts by material (optional)

The game's footstep effects (1 and 2) are two Roland notes whatever the floor. With the preset known at the
player's feet, pick a different rendering per preset (the same Roland timbre on another note, or a short sample
of ours in data\sfx\, which already takes precedence). Presentation only; the game's choice of when a step sounds
is untouched. Half a day.

## Not in this plan

- GPU shaders for the acoustics themselves: 64 rays at 10 Hz is nothing for the CPU, and the audio thread cannot
  wait on a GPU readback; the ray tracer's BVH is reused on the CPU side if the tile-grid march is not enough.
- The FM / Sound Blaster driver port (NOTES step 4): separate, if asked.
