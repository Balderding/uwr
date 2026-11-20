# Sound research: Steam Audio, materials, heads (2026-11-19)

Kept for later use. What Steam Audio can do and what we use, the measured numbers behind our material table, the
free measured heads, what each part costs, and the ideas still open. The review that started it:
`work\soundreview\REPORT-2026-11-19.md`. The code: `port\win32\uwsteam.c` (Steam Audio), `uwaudio.c` (OpenAL and
the settings), `uwacmat.c` (the materials), `uwfront.c` (the Esc menu's Sound tuning rows).

## 1. Steam Audio

- Version in use: **4.8.1** (11 Feb 2026), Valve's newest; phonon.dll is loaded at run time, never linked.
- Its three bands everywhere: **low up to 800 Hz, mid 800 Hz to 8 kHz, high above 8 kHz**.

| Feature | In uwr | Notes |
|---|---|---|
| Real-time reflections (echoes) | Yes | Convolution through a mixer; 16 slots since 2026-11-19 (8 before); order 1 or 2 (Echo direction detail) |
| Listener-centric reverb | Yes, real-time | The listener's own place as a source (a new sound's first echoes) |
| Occlusion | Yes | Volumetric, source 0.5 m, 16 samples |
| Transmission | Yes | 3 bands, 8 rays, by material |
| Binaural (HRTF) | Yes | Steam Audio's own head or a SOFA file (Head shape) |
| Panning (speakers) | Yes since 2026-11-19 | iplPanningEffect for the sound itself, the paths panned |
| Pathing | Yes | A probe per open tile (~2000), baked 0.3-0.5 s a level in the background |
| Embree ray tracer | Yes | About six times quicker than Steam Audio's own |
| Dynamic geometry (instanced meshes) | No | Measured 2026-11-19: making the whole level again for a door is 0.5-0.8 ms plus 2-39 ms to put it into Steam Audio, on the simulation's thread; not worth it |
| Baked reflections / reverb | No | No fixed sound sources; the live listener's response does it |
| Hybrid / parametric reverb | No | 4.8.1 bug: the parametric tail starts about 10 dB too low (issue #610, fix proposed #611); wait for a release |
| Air absorption, directivity | No | Under a tenth of a dB over the game's 6-12 tiles; directivity a possible later row |
| Radeon Rays / TrueAudio Next | No | AMD OpenCL only; the CPU has room |

Guide numbers (Valve's C API guide): real-time 4096 rays, 16 bounces, 2 s, order 1, 2 threads; bakes 32768 rays,
64 bounces, order 2; pathing bake visRange 50 m, pathRange 100 m. `iplSimulatorRunReflections` belongs on a thread
of its own; `iplSimulatorRunDirect` on the game's or its own (ours since 2026-11-19: its own, every 8 ms).

Known Steam Audio faults we work round (uwsteam.c has each): the path baker crashes on a NULL progress callback;
a band gain of 0 into iplPathEffect gives NaN; iplPathBakerCancelBake hangs in the Linux library (we wait the bake
out); pathing across a probe batch's change faults (every silent voice told the probes in use); hybrid reverb's
overlap under 0.01 reads past the energy field (#640).

## 2. Materials

### Steam Audio's own presets (absorb L M H | scatter | transmit L M H)

From ValveSoftware/steam-audio issue #330 (the Unity package's list):

| Preset | Absorb | Scatter | Transmit |
|---|---|---|---|
| generic | 0.10 0.20 0.30 | 0.05 | 0.100 0.050 0.030 |
| brick | 0.03 0.04 0.07 | 0.05 | 0.015 0.015 0.015 |
| concrete | 0.05 0.07 0.08 | 0.05 | 0.015 0.002 0.001 |
| ceramic | 0.01 0.02 0.02 | 0.05 | 0.060 0.044 0.011 |
| gravel | 0.60 0.70 0.80 | 0.05 | 0.031 0.012 0.008 |
| carpet | 0.24 0.69 0.73 | 0.05 | 0.020 0.005 0.003 |
| glass | 0.06 0.03 0.02 | 0.05 | 0.060 0.044 0.011 |
| plaster | 0.12 0.06 0.04 | 0.05 | 0.056 0.056 0.004 |
| wood | 0.11 0.07 0.06 | 0.05 | 0.070 0.014 0.005 |
| metal | 0.20 0.07 0.06 | 0.05 | 0.200 0.025 0.010 |
| rock | 0.13 0.20 0.24 | 0.05 | 0.015 0.002 0.001 |

Every preset has scattering 0.05 (near a mirror), which is low for rough dungeon stone.

### Measured absorption (125 / 250 / 500 / 1k / 2k / 4k Hz)

| Surface | Coefficients | Source |
|---|---|---|
| Brick, unglazed | 0.03 0.03 0.03 0.04 0.05 0.07 | common tables (usenclosure, soundproofyourhome) |
| Brick, painted | 0.01 0.01 0.02 0.02 0.02 0.03 | same |
| Limestone walls | 0.02 0.02 0.03 0.04 0.05 0.05 (8k 0.05) | pyroomacoustics' database (Vorländer) |
| Stone floor, plain or tooled | 0.02 0.00 0.02 0.00 0.05 0.05 | akustik tables |
| Concrete block, unpainted (porous) | 0.40 0.40 0.30 0.30 0.40 0.30 | GSU tables |
| Marble, granite, terrazzo (polished) | about 0.02 (NRC) | commercial guides |
| Wood floor on joists | 0.15 0.11 0.10 0.07 0.06 0.07 | common tables |
| Carpet, heavy on concrete | 0.02 0.06 0.14 0.37 0.60 0.65 | common tables |
| Water or ice surface | 0.008 0.008 0.013 0.015 0.02 0.025 | common tables |

### Rooms and caves

- Measured caves ring **1-3 s** at mid frequencies: Shuanghe karst cave, China (Scientific Reports 2025) T20
  1.4-3.1 s; Palaeolithic painted caves in Spain (JASA) mostly under 2 s, up to about 2.5 s at 500 Hz and below in
  the largest halls. Low notes ring longest, high notes shortest (Traer & McDermott, PNAS 2016).
- Our level 1's first room with rough rock: 1.4 / 0.9 / 0.7 s (low / mid / high), the user's choice on 2026-11-14
  ("the walking sound is quite long" with dressed stone's 0.02-0.04, which rang 8.9 s there).

### Scattering

- 0 is a mirror, 1 every way (ISO 17497-1 measures it in a reverberation room). Room-modelling practice: about
  0.1-0.3 for rough stone or brick at mid frequencies, 0.4-0.7 for very irregular rubble or rock; it rises with
  frequency as the roughness nears the wavelength. Polished surfaces about 0.05.

### Doors (sound through them)

- A 45 mm solid wood door: STC about 30-35 (more with seals; gaps of 3 mm bring an STC 52 door down to 21).
- A steel acoustic door with seals: 29 / 33 / 41 / 48 / 48 / 50 dB at 125 Hz-4 kHz.
- Ours (energy shares): wood 0.070 / 0.014 / 0.005 (about 12 / 19 / 23 dB: a dungeon door with gaps), iron
  0.018 / 0.004 / 0.001 (about 17 / 24 / 30 dB). Both fit the measurements for doors that do not seal.

### Our table (acoustics.txt since 2026-11-19; the earlier one is acoustics-2026-11-14.txt)

| Preset | Absorb L M H | Transmit L M H | Scatter | Step | Where | Why |
|---|---|---|---|---|---|---|
| stone | 0.13 0.20 0.24 | 0.015 0.002 0.001 | 0.30 | 0 | the plain stone walls and floors (dressed blocks), the '*' | the user's rock numbers kept (measured limestone rang too long for the user); smoother than rock |
| rock | 0.13 0.20 0.24 | 0.015 0.002 0.001 | 0.50 | 0 | irregular, natural, rough-hewn walls (mossy, cracked, vine-covered); rough floors | Steam Audio's rock; rough |
| brick | 0.03 0.04 0.07 | 0.015 0.002 0.001 | 0.20 | 0 | brick walls (084 085 089 096 097 143) | measured unglazed brick = Steam Audio's brick |
| marble | 0.01 0.02 0.02 | 0.015 0.002 0.001 | 0.05 | 0 | marble walls (20) and floors (012-015) | polished stone, measured; a marble hall rings long |
| plaster | 0.12 0.06 0.04 | 0.015 0.002 0.001 | 0.10 | 0 | stucco walls (078 082 093 094) | Steam Audio's plaster |
| wood | 0.11 0.07 0.06 | 0.070 0.014 0.005 | 0.10 | +3 | oak walls, plank doors | Steam Audio's wood |
| carpet | 0.24 0.69 0.73 | 0 | 0.60 | -2 | the straw bed | Steam Audio's carpet (deep and soft) |
| metal | 0.02 0.02 0.03 | 0.018 0.004 0.001 | 0.10 | +7 | iron doors, the Abyss's riveted doors | heavy plates (Steam Audio's metal is a thin sheet) |
| water | 0.01 0.01 0.02 | 0 | 0.05 | -5 | water, puddles, lava, waterfalls | measured water surface |
| dirt | 0.15 0.30 0.45 | 0 | 0.40 | -2 | dirt floors, the moss wall | earth |
| hanging | 0.03 0.11 0.22 | 0 | 0.50 | 0 | tapestries, banners, standards | a third cloth over stone |

The game's own names for the textures (STRINGS block 10) were used to sort them: `work\acoustics\names.py`.
Scatter reaches only Steam Audio's echoes (the On level's own rays do not use it).

## 3. Heads (SOFA HRTF files)

| Set | Heads | Licence | Notes |
|---|---|---|---|
| SADIE II (University of York) | D1 Neumann KU100, D2 KEMAR dummy heads; 18 people too | **Apache 2.0**; attribution "SADIE Database Copyright 2018 The University of York", The Audio Lab (Armstrong, Thresh, Kearney) | zenodo.org/records/12092466; 48 kHz 256-tap files are 36 MB each; in use (uwr\hrtf, O:\appsnotonc\hrtf\sadie2) |
| HUTUBS (TU Berlin) | 96 people | CC BY 4.0 (cite Brinkmann et al. 2019) | sofacoustics.org; a good next set (real heads) |
| MIT KEMAR | KEMAR | MIT Media Lab copyright, free use with credit (check the README) | old (1994) |
| ARI, CIPIC | many people | to be checked at the source (ARI likely CC BY-SA) | not used |

Steam Audio needs SOFA "SimpleFreeFieldHRIR", the listener at 0 0 0 with two receivers, one emitter, Data.Delay 0;
any sample rate (resampled). We load a SOFA head with RMS normalisation (measured heads differ in level). Shipping
any set in a release: its licence file and attribution go with it, and the user says yes first.

## 4. What each part costs (hidden runs, level 1-2 changes, 8 Embree threads)

| Setting | Steam Audio's simulation | The echoes' feed | Notes |
|---|---|---|---|
| Before 2026-11-19 (8 voices, order 1) | 65-150 ms a second | 25-63 ms a second | sndreview1 |
| 16 echo slots, 32 sounds, order 1 | 47-88 ms | 24-50 ms | snd2a |
| The same at order 2, KEMAR head | 100-185 ms | 66-102 ms | snd2c |

That is CPU time per second of sound: 100 ms is a tenth of one core. The level's paths bake in 0.4-0.5 s, the level's
triangles (about 13,000) are made in under 1 ms and put into Steam Audio in 2-39 ms, both on the simulation's
thread. No echo stream ran dry in any run.

## 5. Ideas still open (not built)

- **A test sound for choosing a head**: a row that plays a sound circling you (front, side, behind, above), so
  Head shape can be judged in seconds.
- **More heads**: a few HUTUBS people (CC BY) beside SADIE's two dummy heads.
- **Room sounds**: faint loops placed by the map (water and waterfalls, lava's rumble, wind at chasms, drips in
  caves), each through Steam Audio so they echo and muffle like everything else; each its own row.
- **Footsteps by recording**: the user's own `data\sfx\NNN-<preset>.wav` already wins over the Roland's pitched
  note; a free set of stone/wood/dirt/water steps would make floors sound real.
- **Directivity**: a creature facing away sounds duller (Steam Audio's dipole); small, its own row.
- **Hybrid reverb** once a Steam Audio release fixes #610: long tails at little cost.
- **Things that move or break** after a level is built still stand where they were for the sound; the renderer
  would have to say when one changes.
- **The levels by ear** (still the user's): the sound itself against its echoes (acoustics_direct_level), the
  paths' level, the speakers' level (the makeups' ratio, worked out, not heard), wood's high band, the footsteps.

## Sources

- Steam Audio releases: https://github.com/ValveSoftware/steam-audio/releases ; downloads:
  https://valvesoftware.github.io/steam-audio/downloads.html ; C API guide:
  https://valvesoftware.github.io/steam-audio/doc/capi/guide.html
- Material presets: https://github.com/ValveSoftware/steam-audio/issues/330 ; bands:
  https://valvesoftware.github.io/steam-audio/doc/unity/material.html
- Hybrid reverb bugs: https://github.com/ValveSoftware/steam-audio/issues/610 , /pull/611 , /issues/640
- Absorption tables: http://usenclosure.com/Diffraction%20Graphs/Sound%20Absorption%20Coefficients.pdf ,
  https://soundproofyourhome.com/absorption-coefficient-chart/ ,
  https://pyroomacoustics.readthedocs.io/en/latest/pyroomacoustics.materials.database.html ,
  https://www.usmarble.com/construction-and-building/natural-stones-secret-acoustic-power-transform-your-spaces-sound-quality/
- Caves: https://www.nature.com/articles/s41598-025-26251-2 ,
  https://www.researchgate.net/publication/319059448_Cave_acoustics_in_prehistory_Exploring_the_association_of_Palaeolithic_visual_motifs_and_acoustic_response ,
  https://pmc.ncbi.nlm.nih.gov/articles/PMC5137703/
- Scattering: https://digitalcommons.unl.edu/cgi/viewcontent.cgi?article=1013&context=archengfacpub (ISO 17497)
- Doors: https://bettersoundproofing.com/stc-ratings-doors/ , https://www.noisebarriers.com/quietswing-architectural-doors
- Heads: https://www.york.ac.uk/sadie-project/database.html , https://sofacoustics.org/data/database/hutubs/Documentation.pdf
