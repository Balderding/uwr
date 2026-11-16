# Acoustics Without Steam Audio Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The game's sound effects are muffled by walls and shut doors and ring as the room they are in would, by
our own small ray code over the level's tile grid and OpenAL Soft's EFX. No Steam Audio.

**Architecture:** The renderer DLL, which already holds the level, hands the backend a snapshot of the tile grid and
the doors. A new module `uwacoustic.c` (plain C, no OpenAL, no DLL: testable alone) casts rays through that grid: 64
from the listener for the room's size and absorption, a few per playing source for occlusion. `uwaudio.c` turns the
answers into EFX: one reverb slot for the room, one filter per source for what stands between. Everything runs on
the window thread every 100 ms (a few hundred short rays: microseconds), so there are no new threads.

**Tech Stack:** C17 (clang, MinGW target), OpenAL Soft 1.25.2 with its EFX extension (already shipped in
`dist\UW1\bin\soft_oal.dll`), Munt for the effects' PCM (unchanged).

**Spec:** `docs\superpowers\plans\2026-11-13-sound-and-acoustics.md` (its Tasks 2, 3, 4 and 7; its Task 1 is already
in: `PortFxEvent.vol` reaches `uwaudio_event`). Steam Audio (its Task 6 and
`docs\superpowers\plans\2026-11-13-steam-audio.md`) is shelved by the user, 2026-11-14; phonon.dll stays in
`dist\UW1\bin` unused and nothing here loads it.

## Global Constraints

- Presentation only. What plays, when, where and at what velocity are the game's; nothing here is read by the game.
  `fx_acoustics 0` (and every proof, every existing check) leaves the output as it is today: `run-tests.cmd` passes
  at every commit and its verdict is quoted in the commit message.
- The original's falloff stays: full within a tile, nothing beyond six, linear between.
- The music never enters OpenAL.
- Every check is hidden: OpenAL Soft's loopback device into memory or a WAV. Nothing is heard on the machine. The
  user judges by ear in a visible run before `dist\UW1\data\settings.txt` gets `fx_acoustics 1`.
- No `ALC_EXT_EFX` in the device: one line `fx: acoustics off (no EFX)`, the effects play as with `fx_acoustics 0`.
- Nothing on C:. Scratch in `O:\uw\uo-new\work\acoustics`. Never delete; only add. Build through `work\locked.sh`;
  commit by explicit path, one commit a task.
- Distances are tiles in the code; the reverb formulas need metres: `acoustics_tile_m` (settings.txt, default 2.4,
  **a guess** to be judged by ear: it sets how long every room rings).
- **Every setting is a row of the Esc menu** (the user, 2026-11-14: what can be an option there will be). No
  setting of this plan lives in settings.txt alone. A row is added in the task that makes the setting work, changes
  take effect at once (no restart of the game; the audio module restarts only for the two rows marked so below), and
  the value is kept in `data\settings.txt`. The rows, under the menu's "Sound" heading (Task 0):

  | Row | settings.txt key | Values (default first) | Task |
  | --- | --- | --- | --- |
  | Positional sound | `fx_audio` | exists | - |
  | Headphone 3D sound | `fx_hrtf` | On / Off / Automatic (restarts the audio) | 0 |
  | Effects' sides | `fx_pan_fix` | Corrected / As the MT-32 plays them | 0 |
  | Acoustics | `fx_acoustics` | Off / On (restarts the audio; the row exists, it does nothing yet) | 4 |
  | Reverb level | `acoustics_wet` | 100 %, 0 to 200 in tens | 4 |
  | Room size | `acoustics_tile_m` | 2.4 m a tile, 1.5 to 4.0 in tenths | 4 |
  | Muffling by walls | `acoustics_occlusion` | 100 %, 0 to 100 in tens (0: no filter at all) | 4 |
  | Sound round corners | `acoustics_detour` | On / Off | 5 |
  | Footsteps by floor | `fx_footsteps` | On / Off | 6 |

  The four acoustics rows below "Acoustics" are shown only while it is On.

## What this will and will not do

It will: muffle a sound behind rock or a shut door (duller as well as quieter), let it through a door by the door's
material, give each room a reverb whose length follows its size and surfaces, change that reverb as the player walks
from a corridor into a hall, and (Task 5) let a sound round a corner arrive weakened by the detour instead of cut.

It will not: place individual echoes (one reverb for the room the listener is in, not one per sound), make a sound
in the next room ring with that room's own tail, or model sound through a portcullis's bars (left acoustically
open). Those were Steam Audio's side of the comparison and are out with it.

## Review Focus

1. A level change, a load or a teleport: the grid snapshot is replaced before the next probe; the reverb eases to
   the new room, never a probe on a half-copied grid (Task 2's `epoch` assertion, Task 4's `jump` test).
2. The listener or a source inside rock or outside the map (a creature's event at a tile edge, a clipped camera):
   the probe returns "no result" and the last values hold; no read outside the 64 x 64 grid (Task 3's `inside` and
   `edge` tests).
3. An effect at the listener (`x < 0`, play_effect_here) or closer than 0.1 tile: never occluded (Task 4's `here`).
4. A source stolen by a newer effect while its filter still holds the old sound's occlusion: the new sound starts
   from its own first result, not the old one (Task 4's `steal`).
5. A door opened while its own sound plays from the far side: the filter eases open over about 0.3 s, no click
   (Task 4's `door`).

---

### Task 0: The Esc menu made legible

> **Built 2026-11-14** (NOTES.md "The Esc menu made legible"; `tools\menucheck.py smooth original keys kept`: 47
> PASS). Decided while building, where this text says otherwise:
> - `src\uwconv_draw.c` keeps its own smooth text; `src\smoothtext.c` is new beside it (no risk to the conversation
>   screen; about 60 lines are alike in the two).
> - The checks were written after the code, so they were never seen failing first.
> - "The original's letters equal a reference picture" was dropped: the panel is wider and has headings in both
>   fonts. The check asserts the font in use and that the two pictures differ on the panel only.
> - `narrow` is checked at the hidden window's own size (1920 x 1080), not at 1280 x 720: the checks have no
>   window-size switch. The width is clamped to the window in the code.
> - The model fixture is `UWPORT_LLM_MODEL` (the name the code has), not `UWPORT_MIND_MODEL`.
> - The first-start model picker shows the whole menu with the Language model row chosen, as before.
> - Rows whose option the renderer does not have yet are not shown (Fog, Acoustics), as before.

Independent of the acoustics; first, because every later task adds rows to it.

What is wrong now (read from the code, 2026-11-14): `uwv_menu_rows` (src\uwview.c) draws every label and value in
the game's smallest pixel font (`FONT_SMALL`), forced to capitals, enlarged 3x; `menu_rows_build`
(port\win32\plat_win32.c) cuts the language model's name to 16 characters and ".." (`"%.16s.."`), and every value
to 47; 34 rows run in one unbroken list.

**Files:**
- Create: `src\smoothtext.c`, `src\smoothtext.h` (the smooth-font glyph cache now private to `src\uwconv_draw.c`'s
  `smooth_build`, made shared; uwconv_draw.c then uses it, its output unchanged)
- Modify: `src\uwview.c` (`uwv_menu_rows`, `uwv_option` ids 18 and 19, a `menu_font` setting),
  `port\win32\plat_win32.c` (`menu_rows_build`, the row table, the value buffers)
- Test: `tools\menucheck.py` (new, added to `tools\verify.cmd`)

**Interfaces:**
- Produces:
  ```c
  /* text at the window's own resolution into an RGBA layer; em the letters' height in window pixels */
  enum { ST_REGULAR, ST_SEMIBOLD };
  int st_width(int face, int em, const char *s);
  int st_draw(unsigned *rgba, int w, int h, int x, int y, int face, int em, const char *s, unsigned rgb);  /* the pen's end */
  ```
  A label beginning with `#` passed to `uwv_menu_rows` is a heading: drawn in gold without a button, never chosen
  (the keys skip it, a click on it does nothing).

Decisions:
- **The font:** Segoe UI through GDI (on every Windows 10 and 11; Tahoma when it is missing), semibold for labels
  and headings, regular for values, antialiased, at the window's resolution: the leather, the buttons and the cursor
  stay the original's art at 3x, the text is drawn over them after the overlay is blended. Mixed case as written,
  no forced capitals. Letter height 60 % of a row's height in window pixels.
- **A row of the Esc menu "Menu text": Smooth / The original's** (`menu_font 0 / 1`, default 0), beside
  "Conversation text"; the original's keeps today's drawing exactly.
- **Width:** the panel 300 wide (from 216), narrower when the window is (never off the screen). A value that still
  does not fit beside its label is first drawn smaller (down to 75 %), then shortened in its middle with "..."
  (start and end kept: a model's quantisation is at the end).
- **The foot of the panel:** two lines for the row chosen or under the pointer: its value in full, and one line
  saying what the row does (a third column of the row table; every row gets one, e.g. Acoustics: "Walls and shut
  doors muffle sounds; rooms ring by their size").
- **The model's name in full:** `menu_rows_build` no longer cuts it (the file's name without `.gguf`); value
  buffers 128 characters (from 48) in plat_win32.c and uwview.c.
- **Headings:** Game (Resume, Save, Load, New game, Quit), Display, Graphics, Effects, Sound, Controls, AI. Row
  order within a heading as now. The first-start model picker ("Pick a model") shows the AI rows only, as now.
- The two rows that were settings.txt-only: "Headphone 3D sound" (`fx_hrtf`) and "Effects' sides" (`fx_pan_fix`).

- [ ] **Step 1:** `tools\menucheck.py`: hidden runs that open the Esc menu by the test keys and take the window's
  picture, with `UWVIEW_MENU_DUMP=FILE` (the DLL writes each row drawn: label, value as given, value as drawn, the
  value's width and the room it had, heading or not). Asserts:
  - `full name`: the Language model row's value as drawn equals the model file's name without `.gguf`, whole
    (with `UWPORT_MIND_MODEL` set to a fixture name of 40 characters: drawn whole or shortened in the middle with
    both ends present, and whole in the foot line).
  - `fits`: for every row, the drawn width is within its room; no label overlaps its value.
  - `case`: "Language model" is drawn with a capital L and small letters.
  - `smooth`: the pictures with `menu_font 0` and `1` differ inside the panel and nowhere else; with `menu_font 1`
    the panel's text pixels equal a reference picture taken before this task.
  - `headings`: Down pressed from the top row 60 times never rests on a heading; a click on a heading chooses nothing.
  - `narrow`: at a 1280 x 720 window the panel lies wholly inside the window.
  - `rows`: "Headphone 3D sound" and "Effects' sides" are there; a Right press on each changes its value and the
    key in `data\settings.txt` (a scratch data folder).
  - `conversation`: `tools\convcheck.py` still passes (the conversation's smooth text unchanged by the move).
  Run: FAIL.
- [ ] **Step 2:** Implement. Run: PASS.
- [ ] **Step 3:** `run-tests.cmd`, commit. The user looks at the menu in a visible run; the font's size and weight
  are theirs to change before the acoustics rows go in.

One and a half days.

### Task 0B: The DLSS and ray tracing rows, by their names, changed on the fly

> **Built 2026-11-14** (NOTES.md "The DLSS rows by name"; `tools\menucheck.py dlss`: 24 PASS). Decided while
> building, where this text says otherwise:
> - No render target is made again: every target is the output's size already, so only NGX's features are.
> - `render_set_rt_high` / `_wanted` / `_active` keep their names; Ray Reconstruction is kept as `rt_quality high /
>   normal` (the key uw.exe reads too), not a new `ray_reconstruction` key.
> - The sizes were checked at the hidden window's 1920 x 1080 (1280 x 720, 1114 x 626, 960 x 540, 640 x 360) by the
>   same ratios; the 2560 x 1440 numbers below follow from them and are for the user to see in a visible run.
> - "No render target leaked" is checked as 30 changes made with no failure and a clean exit (nothing is allocated
>   by a change but NGX's own feature).
> - A window resize does not go through this path (the renderer has no resize).
> - `proofs`: run-tests.cmd's pictures at the default (DLAA) stand in for it.

The user, 2026-11-14: every DLSS setting by name (ray tracing, then Ray Reconstruction; the mode: Quality, DLAA and
the rest), changeable during play, and the menu shows the resolution drawn and the resolution it is upscaled to
(their screen: 2560 x 1440).

What is there now (read from the code): "DLSS" On / Off; no mode row: the render size is fixed when the renderer
starts (`render.c` `start_dlss`, from settings.txt `port_dlss_quality`, default DLAA); "Ray tracing" is one row
Off / Normal / High where High means Ray Reconstruction, never named; "DLSS 5" and its style have rows, its
intensity and structure are settings.txt only. Frame Generation is not in the renderer (only NVIDIA's header is in
`dlss\include`): no row, and not part of this plan.

**Files:**
- Modify: `src\render.c`, `src\render.h`, `src\ngx.h` (the Ultra Performance value), `src\uwview.c` (`uwv_option`,
  the settings), `port\win32\plat_win32.c` (the row table)
- Test: `tools\menucheck.py` mode `dlss`

**Interfaces:**
- Produces (render.h):
  ```c
  enum { DLSS_Q_PERF, DLSS_Q_BALANCED, DLSS_Q_QUALITY, DLSS_Q_DLAA, DLSS_Q_ULTRA };   /* the first four as now */
  bool render_set_dlss_quality(int q);                 /* during play; false: it could not, the old mode is back */
  int  render_dlss_quality(void);
  void render_sizes(int *rw, int *rh, int *ow, int *oh);          /* drawn now -> shown */
  void render_size_for(int q, int *rw, int *rh);                  /* what mode q would draw at this window */
  void render_set_ray_reconstruction(bool on);  bool render_ray_reconstruction_wanted(void);
  bool render_ray_reconstruction_active(void);  /* (the present render_set_rt_high / _wanted / _active, renamed) */
  ```

The rows, under the "Graphics" heading, in this order:

| Row | Values | Notes |
| --- | --- | --- |
| Resolution | `1707 x 960 -> 2560 x 1440` | not chosen (information): drawn size -> the window's; follows every change at once |
| DLSS Super Resolution | On / Off | today's "DLSS" row, named in full; Off: drawn at the window's size |
| DLSS mode | DLAA / Quality / Balanced / Performance / Ultra Performance | each value followed by its size, e.g. `Quality (1707 x 960)`; shown while Super Resolution is On |
| Ray tracing | On / Off | "Not on this card" where there is no DXR, as now |
| DLSS Ray Reconstruction | On / Off | replaces "High"; needs ray tracing and Super Resolution: otherwise the value reads `Needs ray tracing` / `Needs DLSS` and the row does nothing |
| DLSS 5 | On / Off | exists |
| DLSS 5 style | A / B / C | exists |
| DLSS 5 intensity | 0 to 100 % in fives | `dlss5_intensity`, through `ngx_nr_set_params` |
| DLSS 5 structure | 0 to 100 % in fives | `dlss5_structure`, the same call |

Decisions:
- Ratios as `start_dlss` has them (NVIDIA's): Performance 1/2, Balanced 0.58, Quality 2/3, DLAA 1; Ultra
  Performance 1/3 added (NGX's PerfQuality value 3). At 2560 x 1440: DLAA 2560 x 1440, Quality 1707 x 960, Balanced
  1485 x 835, Performance 1280 x 720, Ultra Performance 853 x 480.
- `render_set_dlss_quality`: the GPU's work finished first (`submit_and_wait`), the Super Resolution and Ray
  Reconstruction features released, every render-size target made again at the new size, the features created
  again, the history reset (one frame's cut, as a camera cut). If any step fails the old mode is put back the same
  way and the row keeps its old value. A window resize goes through the same path.
- Kept as `port_dlss_quality` (dlaa / quality / balanced / performance / ultra) and a new `ray_reconstruction 0 / 1`;
  an old `rt_quality high` is read as ray tracing On + Ray Reconstruction On, `normal` as Off.
- Each row's foot line (Task 0) says what it is, e.g. DLSS mode: "The size the 3D view is drawn at before DLSS
  enlarges it to the window. DLAA draws at full size."

- [ ] **Step 1:** menucheck `dlss` (hidden runs on the GPU, a 2560 x 1440 hidden window: `UWPORT_WINDOW=2560x1440`,
  added if the checks have no such switch yet). Asserts:
  - `sizes`: stepping "DLSS mode" through its five values, the Resolution row and the log's
    `ngx: DLSS created W x H -> 2560 x 1440` both give 2560 x 1440, 1707 x 960, 1485 x 835, 1280 x 720, 853 x 480.
  - `live`: after each step the next frame's picture is not black, the level is still drawn (differs from the
    previous mode's picture by less than a set amount), and no restart happened (the same process, the frame
    counter goes on).
  - `repeat`: 30 mode changes in a row: the count of live render targets printed at the end equals that at the
    start (nothing leaked), no D3D12 error.
  - `rr`: Ray tracing Off: the Ray Reconstruction row reads "Needs ray tracing"; On + On: the log has
    `ngx: Ray Reconstruction created` at the mode's size; a mode change with it on creates it again at the new size.
  - `off`: Super Resolution Off: Resolution reads `2560 x 1440 -> 2560 x 1440` and the mode row is not shown.
  - `kept`: each row's value is in settings.txt after the change and read back by a second run; a settings.txt
    with `rt_quality high` starts with both rows On.
  - `dlss5`: intensity stepped to 0 and to 100 gives different pictures (where DLSS 5 is on this card; SKIP
    elsewhere).
  - `proofs`: with every row at its default the hidden reference pictures of `tools\verify.cmd` are unchanged.
  Run: FAIL.
- [ ] **Step 2:** Implement. Run: PASS.
- [ ] **Step 3:** `run-tests.cmd`, commit. The user tries each mode in a visible run at 2560 x 1440.

One and a half days.

### Task 1: The materials table

**Files:**
- Create: `data\acoustics.txt`, `port\win32\uwacmat.c`, `port\win32\uwacmat.h`, `tools\acoustics_list.py`
- Test: `tests\uwacmat_test.c`, `tools\fxcheck.py` (new mode `materials`)

**Interfaces:**
- Produces:
  ```c
  typedef struct { char name[16]; float absorb[3], transmit; int step; } UwaMaterial;   /* absorb: low, mid, high */
  enum { UWA_FLOOR, UWA_CEILING, UWA_WALL, UWA_DOOR };
  int uwacmat_load(const char *datadir);            /* presets read; 0 and one log line when the file is bad */
  int uwacmat_count(void);
  const UwaMaterial *uwacmat_get(int preset);
  int uwacmat_for(int kind, int tex);               /* preset index; tex = the texture's file number, a door's item */
  ```

`data\acoustics.txt` (the spec's values; `step` is Task 6's, 0 when absent):

```
preset stone   absorb 0.02 0.03 0.04  transmit 0.00
preset wood    absorb 0.10 0.08 0.10  transmit 0.05
preset carpet  absorb 0.05 0.25 0.55  transmit 0.00
preset metal   absorb 0.02 0.02 0.03  transmit 0.00
preset water   absorb 0.01 0.01 0.02  transmit 0.00
preset dirt    absorb 0.15 0.30 0.45  transmit 0.00
wall * stone
floor * stone
ceiling * stone
door * wood
```

> **Changed 2026-11-14 (the user): transmit is three bands.** `transmit LOW MID HIGH` (one number still reads, as all
> three bands), `UwaMaterial.transmit[3]`. Shipped: wood `0.070 0.014 0.005`, stone `0.015 0.002 0.001` (Steam
> Audio's "wood" and "rock"), metal `0.018 0.004 0.001` (our estimate: wood less about 6 dB); the rest 0. Task 4's
> occlusion uses each band's own number: no "high band squared".

A line `wall 012 wood` names `data\textures\walls\012.png`; `floor NNN` names `data\textures\floors\NNN.png`;
`door 0x141` a door's item number (0x140-0x14F). `*` is the fallback of its kind; the last matching line wins.

- [ ] **Step 1:** `tests\uwacmat_test.c`: a fixture file with the ten lines above plus `wall 012 wood`; asserts
  `uwacmat_count() == 6`, `uwacmat_for(UWA_WALL, 12)` is wood, `uwacmat_for(UWA_WALL, 13)` is stone,
  `uwacmat_for(UWA_DOOR, 0x140)` is wood, wood's `transmit == 0.05f`; a texture line naming an unknown preset makes
  `uwacmat_load` return 0; a missing file returns 0. Run through a new `test_materials` in fxcheck.py: FAIL (nothing
  to build).
- [ ] **Step 2:** Implement `uwacmat.c`. Run: PASS.
- [ ] **Step 3:** `tools\acoustics_list.py`: for levels 1-9 print each wall and floor texture number in use with the
  preset it resolves to and the count of those on `*`. Look at each PNG that is plainly not stone (planks, water,
  lava, earth, rugs, metal plates) and add its line to `data\acoustics.txt`. The check `materials` also asserts the
  tool exits 0 and every texture resolves.
- [ ] **Step 4:** `run-tests.cmd`, commit.

Half a day.

### Task 2: The tile grid and the doors from the renderer

**Files:**
- Modify: `src\uwview.c` (three exports), `port\win32\plat_win32.c` (three `GetProcAddress` lines beside
  `uwv_fx_event`)
- Test: `tools\fxcheck.py` mode `grid`

**Interfaces:**
- Produces (the DLL):
  ```c
  typedef struct { unsigned char type, height; unsigned short wall_tex, floor_tex; } UwvAcTile;  /* level.h's T_*, steps */
  typedef struct { int id, tx, tz, heading, item, shut; } UwvAcDoor;        /* id the door's object number */
  UWV_API int uwv_acoustic_grid(UwvAcTile out[64 * 64], int *ceil_tex);     /* [z * 64 + x]; 0 when no level is built */
  UWV_API int uwv_acoustic_doors(UwvAcDoor *out, int cap);                  /* as they stand this frame; the count */
  UWV_API unsigned uwv_acoustic_epoch(void);                                /* changes whenever level_build made a level */
  ```

`wall_tex` / `floor_tex` are the textures' file numbers, picked as `level.c`'s mesh picks them for that tile's faces
(`lv.walls[t->wall]`, `lv.floors[t->floor]`; the ceiling's as `level.c:853`). `shut` is `open < 0.5` from uwview.c's
`doors[]`; the portcullis (the door item `world_door_model_quads` draws with model 12) always reports `shut = 0`.

- [ ] **Step 1:** fxcheck `grid`: a hidden run on level 1 with `UWVIEW_ACOUSTIC_DUMP=FILE` (the DLL writes the grid
  and the doors once). Asserts: 4,096 tiles; the player's start tile is not `T_SOLID`; the count of non-solid tiles
  equals that of `data\levels\level1.txt`; every `wall_tex` names an existing file in `data\textures\walls`; the door
  count equals the level's objects 0x140-0x14F; `epoch` differs after a hidden run's level change (level 1 to 2).
  Run: FAIL.
- [ ] **Step 2:** Implement, export, load in plat_win32.c (`V.ac_grid`, `V.ac_doors`, `V.ac_epoch`). Run: PASS.
- [ ] **Step 3:** `run-tests.cmd`, commit.

Half a day.

### Task 3: The ray code and the room probe

**Files:**
- Create: `port\win32\uwacoustic.c`, `port\win32\uwacoustic.h`
- Test: `tests\uwacoustic_test.c`, fxcheck mode `acoustics`

**Interfaces:**
- Consumes: `UwaMaterial`, `uwacmat_get` (Task 1).
- Produces:
  ```c
  typedef struct { unsigned char type, height, wall_mat, floor_mat; } UwaTile;   /* mats: preset indices */
  typedef struct { int id, tx, tz, heading, mat, shut; } UwaDoor;
  typedef struct { float dist; int mat, kind; } UwaHit;      /* kind UWA_*; dist in tiles */
  typedef struct { float rt60[3], mfp_m; int hits; } UwaRoom; /* seconds, low / mid / high; mean free path in metres */
  void uwac_grid(const UwaTile t[64 * 64], int ceil_mat, float tile_m);
  void uwac_doors(const UwaDoor *d, int n);
  int  uwac_ray(const float o[3], const float dir[3], float max, UwaHit *hit);   /* 1 hit, 0 none within max, -1 o not in open air */
  int  uwac_room(const float at[3], UwaRoom *out);                               /* 1 ok; 0: at is not in open air */
  int  uwac_path(const float from[3], const float to[3], float gain[3]);         /* Task 4 */
  ```

Decisions:
- `uwac_ray` walks the grid tile by tile (a 2D DDA in x, z, y carried along). It hits: the floor (y under the tile's
  floor: `height * 0.25`, a slope's by its plane as `level.c`'s `corner_h` gives the corners); the ceiling (y above
  4.0); a wall on entering a `T_SOLID` tile or a tile whose floor is above the ray's y there (kind wall, the entered
  tile's `wall_mat`); a diagonal tile's wall across it; a shut door as a plane across the middle of its tile
  (heading 0 / 4: spanning x; 2 / 6: spanning z), full width (kind door). Outside the 64 x 64 grid is solid.
- `uwac_room`: 64 fixed directions (a Fibonacci sphere), `max` 16 tiles. A ray with no hit counts as a hit at 16
  tiles with absorption 1 (the sound left). `mfp` = the mean hit distance in metres; per band `a` = the mean
  absorption of the hits; `rt60 = 0.161 * (mfp / 4) / a`, each clamped to 0.1..10 s.

- [ ] **Step 1:** `uwacoustic_test.c`: fixtures built in the test (all stone unless said): room A 4 x 4 tiles, room B
  12 x 12, a 1 x 12 corridor, A and B side by side sharing a wall with a doorway tile holding a wood door. Cases,
  each printing PASS / FAIL:
  - `ray wall`: from A's middle east: a wall hit at 2.0 tiles (within 0.01), stone.
  - `ray floor / ceiling`: straight down from y 1.0 in a tile of height 2: floor at 0.5; straight up: ceiling at 3.0.
  - `ray step`: toward a neighbour tile of height 8 at y 1.0: a wall hit at the tile's edge; at y 3.0: none there.
  - `ray door`: through the doorway shut: a door hit, wood; open: passes into B.
  - `ray diagonal`: into a `T_DIAG_SE` tile from its open side: a hit inside the tile, not at its edge.
  - `inside`: `uwac_ray` from a solid tile returns -1; `uwac_room` there returns 0.
  - `edge`: a ray from tile (0, 0) west: a wall hit, no read outside the grid (run under clang's
    `-fsanitize=address` in this test build).
  - `room size`: `rt60[1]` of B above that of A; the corridor's below B's.
  - `room material`: A with every material carpet: `rt60[2]` under a quarter of stone A's.
  - `room open door`: A with the door open: `mfp_m` above A's with it shut.
  Run through fxcheck `acoustics`: FAIL (nothing to build).
- [ ] **Step 2:** Implement `uwac_grid`, `uwac_doors`, `uwac_ray`, `uwac_room`. Run: PASS. Print the time of one
  `uwac_room` on level 1's grid (expected well under 0.1 ms) and record it in NOTES.md.
- [ ] **Step 3:** `run-tests.cmd`, commit.

One and a half days.

### Task 4: Reverb and occlusion in the game (`fx_acoustics 1`)

**Files:**
- Modify: `port\win32\uwacoustic.c` (`uwac_path`), `port\win32\uwaudio.c`, `port\win32\uwaudio.h`,
  `port\win32\plat_win32.c` (`fx_pump`), `tools\portwin32.py` if the backend's source list needs the two new files
- Test: `tests\uwaudio_test.c`, `tests\uwacoustic_test.c`, fxcheck `acoustics`

**Interfaces:**
- Consumes: Task 3's `uwac_*`; Task 2's `V.ac_grid / ac_doors / ac_epoch`; Task 1's `uwacmat_for`.
- Produces: `void uwaudio_acoustics(const UwaTile *t, int ceil_mat, const UwaDoor *d, int nd);` (the backend's
  hand-off each time the epoch or a door changes; also the checks').

Decisions:
- **EFX** functions loaded by `alGetProcAddress`, constants copied from OpenAL Soft 1.25.2's `efx.h` as the others
  are.
- **Reverb:** one auxiliary slot with `AL_EFFECT_EAXREVERB`; every source sends to it (`AL_AUXILIARY_SEND_FILTER`,
  no filter). From `uwac_room` at the listener every 100 ms: `DECAY_TIME = rt60[1]`,
  `DECAY_HFRATIO = clamp(rt60[2] / rt60[1], 0.1, 2)`, `DECAY_LFRATIO = clamp(rt60[0] / rt60[1], 0.1, 2)`,
  `REFLECTIONS_DELAY = min(0.3, mfp_m / 343)`, `LATE_REVERB_DELAY = min(0.1, 2 * mfp_m / 343)`; the rest OpenAL's
  defaults. Eased over 0.5 s. The slot's gain is `acoustics_wet` (settings.txt, a percentage, default 100; the
  user's dial); 0 until the first result. When `uwac_room` returns 0 the last values hold.
- **Occlusion,** `uwac_path(from source, to listener, gain[3])`: 9 rays from the listener, one to the source and
  eight to points 0.3 tile around it (the corners of a cube). Each ray's gain per band: 1 when clear; through shut
  doors only, the product of their `transmit[band]` (three bands since 2026-11-14: a door dulls more than it quiets); through
  rock, 0. The result is the mean over the rays, floored at `{0.15, 0.10, 0.03}` (a sound round a corner is muffled,
  never gone: the reverb send is untouched, so it is still heard in the room).
- Applied as one `AL_FILTER_BANDPASS` per source on `AL_DIRECT_FILTER`: `GAIN = gain[1]`,
  `GAINLF = min(1, gain[0] / gain[1])`, `GAINHF = min(1, gain[2] / gain[1])`; recomputed every 100 ms for each
  playing source, eased with a 0.3 s time constant. A sound's first value is computed in `uwaudio_event` before
  `alSourcePlay` (no ease from a stale filter).
- Timing by rendered frames in a loopback run and by the clock in a visible run, both inside `uwaudio_pump`.
- **The menu rows** (Global Constraints' table): "Acoustics" now works; "Reverb level", "Room size" and "Muffling by
  walls" added (`uwv_option` ids after Task 0's, shown while Acoustics is On). `acoustics_occlusion` scales the
  filter: each gain becomes `1 - (1 - gain) * percent / 100`. A change reaches the sound at once through
  `void uwaudio_tune(const char *datadir);` (new in uwaudio.h: the three values read again and applied, no restart);
  plat_win32.c calls it after any of the three rows changes.

- [ ] **Step 1:** `uwacoustic_test.c`: `path clear` (same room: all three gains 1), `path rock` (through the shared
  wall, door shut and made stone: the floor values), `path door` (source just behind the shut wood door: mid gain
  between 0.03 and 0.3, high below mid), `path corner` (source half hidden by a corner: mid gain between 0.3 and
  0.9). Run: FAIL.
- [ ] **Step 2:** `uwaudio_test.c` with the same fixtures, the 440 Hz sample, `UWPORT_FX_ACOUSTICS=1`:
  - `tail`: in room B the RMS of the 300 ms after the sample ends is above 2 x that in carpeted room A, and above
    20 x that with `UWPORT_FX_ACOUSTICS=0`.
  - `dry`: with `acoustics_wet 0` the tail equals `fx_acoustics 0`.
  - `occluded`: the source behind rock: the first 100 ms under a quarter of the same distance in one room.
  - `muffled`: the same pair with a 4 kHz sample loses more than the 440 Hz one.
  - `door`: behind the shut wood door, then `uwaudio_acoustics` with it open and 0.5 s rendered: back within 10 % of
    clear, and no sample-to-sample jump above 4,000 in between.
  - `here`: an effect at `x < 0` with the listener against a wall: its first 100 ms equal `fx_acoustics 0` within 2 %.
  - `steal`: 32 sources playing behind rock, a 33rd beside the listener: its first 50 ms are not occluded.
  - `jump`: the listener moved from A to B in one call: the decay eases (two renders 100 ms apart differ), no error.
  - `off`: with `UWPORT_FX_ACOUSTICS=0` the `east` case renders sample for sample as a reference saved before this
    task.
  - `tune`: `acoustics_wet` written 0 and `uwaudio_tune` called while a tail rings: the tail is gone within 0.2 s,
    no restart (`uwaudio_playing()` unchanged); `acoustics_occlusion 0`: the `occluded` pair renders equal;
    `acoustics_tile_m 4.0` gives a longer tail than `1.5` in the same room.
  - menucheck `acoustics rows`: the three rows appear only with Acoustics On; a Right press on each changes its
    value and its key in settings.txt; each has its foot line.
  Run: FAIL.
- [ ] **Step 3:** Implement. Run: all PASS; fxcheck `audio` and `sides` still PASS.
- [ ] **Step 4:** Hidden game runs (fxcheck `acoustics`, from the loopback WAV): level 1, a door's own sound from the
  far side shut and open (shut under half of open); the same effect at the same distance in the great hall (around
  32, 2) and in a corridor (the rt60 printed for each, the hall's longer, the hall's tail energy higher). The log
  has `fx: acoustics on (EFX)`. Quote the numbers in NOTES.md.
- [ ] **Step 5:** `run-tests.cmd`, commit. **Stop here for the user's ear:** a visible run with `fx_acoustics 1`;
  `acoustics_tile_m` and `acoustics_wet` set from what they hear, then the dist default.

Two and a half days.

### Task 5 (optional, after the user has heard Task 4): sound round corners

A sound round a corner sits at the floor value from Task 4. Better: let it arrive along the way a person would walk.

**Files:** `port\win32\uwacoustic.c / .h`, `port\win32\uwaudio.c`; tests in both test files.

**Interfaces:**
- Produces: `float uwac_detour(const float from[3], const float to[3]);   /* walked tiles minus straight tiles; -1 no way within 12 */`

Decisions: a breadth-first search over the open tiles (8 neighbours, a diagonal step only when both side tiles are
open, a step up of more than 4 height steps or a shut door blocks), at most 12 tiles. When Task 4's mid gain is
under 0.5 and a way exists: `gain[b] = max(gain[b], k[b])` with `k = {1, 0.8, 0.5} * clamp(1 - detour / 4, 0, 1)`.

- [ ] **Step 1:** Tests: `detour straight` (0 within 0.5), `detour corner` (an L of two corridors: between 0.5 and
  2), `detour door` (-1 shut, a number open), and in uwaudio_test `round corner`: louder than Task 4's floor,
  quieter than clear. FAIL.
- [ ] **Step 2:** Implement with its menu row "Sound round corners" (On / Off, `acoustics_detour`, default On, read
  by `uwaudio_tune`; menucheck asserts the row and its key). PASS, `run-tests.cmd`, commit; the user compares on and
  off from the menu.

One day.

> **Built 2026-11-14, with one addition (the user: "I can hear things and cannot find them"):** besides the gain,
> the source is placed toward the last point of its way that the listener sees (`uwac_waypoint`), at its own
> distance: a sound round a corner comes from the opening, not through the rock. The search runs once from the
> listener's tile (Dijkstra, diagonal steps 1.41), not once a source. Tests as built: `detour straight`, `detour
> corner`, `detour door`, `detour step`, `waypoint`, `path round`, and uwaudio_test's `round corner`.

### Task 6 (optional): footsteps by the floor's material

> **Built 2026-11-14.** `step` may be negative (-24 to 24). The row is shown while Positional sound is on and needs
> no Acoustics: the materials are read at every start of the audio (the table in the code when there is no file).
> "carpet with half velocity" is: a floor whose high-band absorption is over 0.5. Tests as built: `step wav`,
> `step stone` (the same within OpenAL's dither, 2), `step wood` (more zero crossings than stone's).

The game's footstep effects (1 and 2) are two Roland notes whatever the floor.

**Files:** `port\win32\uwaudio.c`, `port\win32\uwacoustic.c / .h`; `tests\uwaudio_test.c`.

**Interfaces:**
- Produces: `int uwac_floor_mat(const float at[3]);   /* the preset under a point; -1 none */`

Decisions: for effects 1 and 2 only, in `uwaudio_event`: a `data\sfx\NNN-<preset>.wav` (e.g. `001-water.wav`) plays
when present; else the effect's own Roland rendering with its note moved by the preset's `step` semitones (a second
rendering per preset, asked of the worker as the velocities are). `stone` has `step 0`: the original's sound. Start
values to be judged by ear: wood +3, water -5, dirt -2, carpet -2 with half velocity, metal +7. When and whether a
step sounds stays the game's.

- [ ] **Step 1:** Tests: `step stone` (sample for sample the same as before this task), `step wood` (differs from
  stone's rendering; the spectrum's peak higher), `step wav` (a `001-water.wav` fixture plays on water, not on
  stone). FAIL.
- [ ] **Step 2:** Implement with its menu row "Footsteps by floor" (On / Off, `fx_footsteps`, default On; Off is
  the original's two notes everywhere; menucheck asserts the row and its key, uwaudio_test that Off renders
  `step wood` as stone's). PASS, `run-tests.cmd`, commit.

One day.

## Order and total

> **Added 2026-11-14 (the user): Task 7, optional, after the user has heard Task 4: Steam Audio as "Acoustics: High".**
> On top of our own system, not in place of it: `fx_acoustics 2` (the menu row's third value) keeps our occlusion
> and adds Steam Audio's reflections per sound (each sound rings with the room it is in; the shelved plan
> `2026-11-13-steam-audio.md`, its Tasks 2, 3 and 6: the level's faces from the renderer as a second export beside
> Task 2's grid, phonon.dll started with its worker, the convolution mixed into one OpenAL streaming source). The
> materials table, the doors, the menu rows and `acoustics_tile_m` are shared. Its gain is the echo only: the
> muffling by walls and the finding of a sound (Task 5) are no better with it. About five days; to be planned in
> detail when chosen.

> **Task 7 built 2026-11-14** (the user: "begin doing the tasks 5 6 7"). As planned above, with these choices: the
> level's faces are made in `port\win32\uwsteam.c` from the tile grid the acoustics already get (no second export
> from the renderer); doors are part of the one mesh, made again when a door moves; reflections for the first 8
> sounds that ask (a ninth stays dry until a voice is free), not "the 8 nearest"; no pathing (Task 5's way round is
> ours). Tests: `tests\uwsteam_test.c` (load, scene, tail, side, next room, drain, swap), uwaudio_test's `high on`,
> `high tail`, `high dry`, `high occluded`, `high drain`, the game run with `fx_acoustics 2`, menucheck's Acoustics
> row stepped to High.

0 (the user looks at the menu), 0B (the user tries the DLSS rows), 1, 2, 3, 4 (the user listens). The menu after
one and a half working days, the DLSS rows after three, the first audible result after about eight. Tasks 5 and 6 are a day each, chosen after that. Not in this plan: per-sound early reflections (the spec's Task 5), judged unnecessary until the
room reverb has been heard; Steam Audio.
