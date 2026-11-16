# src guide, part B

This guide covers one set of files in `src/`. Part A covers the rest. Line numbers refer to git HEAD as of 2026-11-13, so they drift as the code changes; search for the function name to find it.

Abbreviations used below:

- **UW.EXE 0x...**: a file offset in the original executable. **DS 0x...**: an address in the original's data segment.
- **exhumed**: the underworld-exhumed source (DRAWOBJ.C, INTERP.ASM, USEITEMS.C and so on). The code comments name its labels.
- **proved**: an original routine run in the emulator beside ours with the results diffed (see `DECOMP_RULESET.md`, `re\proved.txt`).

The files fall into four groups:

| Group | Files |
|---|---|
| Programs and front ends | `main.c` (our own engine's uw.exe), `uwview.c` + `uwrec.c` + `uwinv_draw.c` + `uwconv_draw.c` (uwview.dll, the renderer under the port), `uwmind.c` (uwmind.dll, LLM creature AI), `nrbridge.c` (uw_nvngx.dll) |
| Rules ported from UW.EXE | `orig.c`, `sfx.c`, `songs.c`, `xmidi.c` |
| Data and geometry | `objdata.c`, `world.c`, `models.c`, `normals.c`, `png.c` |
| Presentation and back ends | `ui.c`, `sound.c`, `ngx.c`, `render_decals.c`, `render_particles.c` |

How the builds use these files:

- **`build.cmd`** builds `uw.exe` (our own engine, entry `main.c`), plus `build\uw_nvngx.dll` from `nrbridge.c`.
- **`tools\buildview.cmd`**:
  - builds `build\uwview.dll` from `uwview.c`, `uwrec.c`, `uwinv_draw.c`, `uwconv_draw.c` and the FX sources (`FXSRC=src\render_particles.c src\render_decals.c`), together with the COMMON sources (`world.c`, `objdata.c`, `models.c`, `ui.c`, `png.c`, ...);
  - builds `build\uwtour.exe`, which has the FX sources and `uwinv_draw.c` but not `uwconv_draw.c`;
  - builds `build\uw_nvngx_port.dll`, which `tools\portwin32.py` deploys as `uw_nvngx.dll`.
- **`tools\buildmind.cmd`** builds `uwmind.dll`.
- **The port**: `port\win32\plat_win32.c` loads uwview.dll with `LoadLibrary` and finds every `uwv_*` export with `GetProcAddress`.

---

## main.c

**Purpose.** The entry point and Win32 front end of our own engine (uw.exe). It:

- reads `data\settings.txt` and `data\keys.txt`;
- extracts missing loose data from `game\`;
- creates the window and the D3D12 renderer;
- runs the frame loop.

It has two modes besides normal play:

- a headless `--shot=` mode that draws scripted frames to a PNG (used by the checks);
- a console conversation mode.

It also holds our own player movement (used when `original_movement` is 0 or the rules core is not running), the save slots, the Esc options menu and the clipboard screenshot.

**Main functions.**

*Settings and keys*

- `apply_setting()` (82): applies one `key value` setting to `Settings`. Unknown keys are ignored. `rt_quality` and `dlss_quality` take words (`high`; `dlaa`/`quality`/`balanced`/`performance`).
- `read_settings()` (119):
  1. Fills in the defaults, and creates the file with a header line if it is missing.
  2. Parses each line, and appends the default line from `SETTING[]` for any key the file lacks.
  3. Clamps the values: width 320..7680, height 200..4320, fov 40..120, start_level 1..9, dlss5_style 0..2; an exposure of 0 or less becomes 2.
- `key_code()` (212): turns a key name into a VK code. It accepts named keys, Mouse1-5, punctuation, F1-F12, a single letter or digit, or a plain number taken as a raw VK code.
- `read_keys()` (237): fills `g_bind[ACTIONS][4]` (up to 4 keys per action, `#` starts a comment). It writes `DEFAULT_KEYS` if the file is missing, and appends `ACTION_DEFAULT` for any action the file lacks.
- `down()` (292) and `pressed()` (299): an action's key is held (`GetAsyncKeyState & 0x8000`, focus only), or was pressed since the last time the action was asked about (an edge, detected with a static `was[]`).

*Window and input*

- `capture()` (311): hides the cursor and clips it to the window, or releases it.
- `wndproc()` (323):
  - Handles focus, raw mouse input (WM_INPUT), WM_CHAR for character creation (8, 13, 32..126) and Alt+F4.
  - Right click: "use" while the mouse is captured; an inventory right click otherwise.
  - Left click while captured: starts an attack and sets `g_lmb_held`. Button up releases a charged attack.
- `core_clock_now()` (369): real time in 1/256 s from QueryPerformanceCounter. It stands in for the DOS timer the original read, and is handed to the rules core through `cw_clock`.

*Our own movement*

- `fits()` (389) and `floor_under()` (400): collision for our movement. `fits` probes 8 points at RADIUS 0.18 against solid tiles, `game_blocked`, and floors more than MAX_STEP 0.3 above the feet. `floor_under` takes the highest floor of 5 probe points.
- `player_step()` (410): mouse and keyboard look, with pitch clamped to ±1.45 rad.
  - **With `original_movement` and `cw_running()`**: it sends ONE movement to the rules core per frame, in the priority jump > forward > back > left > right (a comment says this follows the original's keyboard handler). There is no movement while `game_talking()`. It then calls `cw_step` and reads the position back with `cw_player`.
  - **Otherwise, our own physics**:
    - walk 2.2 or run 4.5 tiles/s, ×0.55 in water (marked "ours");
    - F4 toggles flying;
    - each axis is moved in 0.05 sub-steps, so the player slides along walls;
    - jump vy 2.6, gravity 9.
  - **Spell bits from `game_motion()`**: 1 leap (gravity 4.5); 2 slow fall (fall speed capped at -1.73, fall harm ×0.7071); 4|16 levitate or fly; 8 water walk.
  - The comment at line 462 cites "UW.EXE 0x1c62f, 0x1d2c2; the original: jump 141, gravity 4 a tick" and slow fall "94 / 141 of a jump's speed". The floats are approximations of these.
  - A landing with vy < -3 sends fall harm to `game_landed()`.

*Levels and saves*

- `remesh_level()` (494): rebuilds the mesh and lights after a trap changes tiles (`game_remesh`).
- `load_level()` (509):
  1. Saves the level being left (`game_store_level` + `level_save` into `level_dir()`).
  2. Loads level N with its lights, mesh and objects.
  3. If `place` is set, puts the player at (32, 1.5) on level 1. Otherwise, or if that spot is solid, it uses the open tile nearest the centre.
- `slot_dir()` (561), `run_new()` (567), `copy_levels()` (581), `save_game()` (597) and `load_game()` (612): the save system.
  - The live levels are in `data\saves\run-N`, and `current.txt` names the current run. A new run folder is used on every new game or load (nothing is deleted).
  - Slots are quick (0), `slot1-4` and `autosave` (9). Each holds the `level*.txt` files, the `game_save` state and `where.txt` (`level/x/z/yaw/pitch`).

*Rendering, torch and clipboard*

- `render_config()` (639): builds a `RenderConfig`. The DLSS folder is `<root>\dlss` and the logs go to `<root>\logs`.
- `show_title()` (654): puts the DLSS and ray-tracing state in the window title.
- `copy_to_clipboard()` (663): the frame as CF_DIB (BGRA, bottom-up).
- `place_torch()` (688): the hand light sits right of, below and ahead of the eyes, pulled back to 0.2 from walls. It takes `game_light()`, flickers with 3 sines, and is scaled by `torch_intensity/1.3` and `torch_range/6`. The `unused` parameter and `warm[]` are dead.

*Settings file and Esc menu*

- `settings_save()` (726), with `save_int()` (758) and `save_float()` (763): rewrite one key in settings.txt, keeping that line's comment at its column, or append the key.
- The Esc options menu: `opt_shown()` (769), `opt_text()` (775), `opt_change()` (805), `opt_rows()` (850), `menu_rect()` (858), `menu_update()` (862), `menu_draw()` (877).
  - Rows: Resume, Save slot, Save, Load, New game, DLSS, DLSS 5, DLSS 5 style, Ray tracing, Bounced light, Stone relief, Music, Music volume, Sound volume, Mouse sensitivity, Field of view, Quit.
  - The DLSS 5 rows appear only if `render_dlss5_available()`.
  - The menu is 250 px wide on `ui_parchment`, over the game dimmed with alpha 110.

**`main()` (896).** Order at start-up:

1. `arena_init(1536 MB)` and argument parsing.
2. `extract_missing`; if the originals are missing it shows a message box and exits 1.
3. `--sound-wav`, which renders sound to a WAV and exits.
4. `normals_missing`.
5. Settings and keys.
6. The `--normals`, `--torch`, `--set` and `--rt` overrides.
7. Console talk mode, shot mode, or the window.

*Command line* (lines 926-982; prefix matches, the first matching branch wins):

| Option | What it does |
|---|---|
| `--shot=FILE.png` | headless mode |
| `--level=N`, `--pos=X,Z` | start level and position |
| `--yaw=DEG`, `--pitch=DEG` | view direction in degrees |
| `--size=WxH` | shot size (default 1280x720) |
| `--dlss=0\|1\|2` | off, DLSS, or DLSS + DLSS 5; also used in window mode, and in shot mode DLSS is off unless this is given |
| `--frames=N` | render N frames of the same view (for DLSS history) |
| `--walk=N` | N frames walking forward on the rules core, then face the nearest visible creature |
| `--toggle-every=N`, `--rt-every=N` | flip DLSS, or cycle ray tracing, every N frames |
| `--style-at=F,S` | set the DLSS 5 style at frame F |
| `--fight=F` | press fight at frame F |
| `--talk=F`, `--picks=a,b,..` | talk to the creature faced on the core at frame F, giving those answers |
| `--sheet=F`, `--sleep-at=F`, `--plant-at=F`, `--fish-at=F`, `--runes=F` | press those keys at frame F |
| `--char-level=N` | set the character's level |
| `--cast=F:Rune,Rune` | put those runes on the shelf and cast at frame F |
| `--give=KIND` | give 5 of that item (hex allowed, via `strtol` base 0) |
| `--wield=KIND` | put one in the right hand (hex allowed) |
| `--hurt-at=F,DMG` | deal damage at frame F |
| `--chargen=1` | show character creation |
| `--saves=DIR`, `--save-at=F`, `--load-at=F` | own saves folder; save or load slot 1 at frame F (only together with `--saves`) |
| `--converse=N` | open conversation N at frame 1 |
| `--normals=`, `--torch=`, `--rt=` | override those settings |
| `--ui=1` | run `game_update` and draw the interface in shot mode |
| `--use=F`, `--look=F`, `--inv=F` | press those actions at frame F |
| `--shift=F` | hold Shift from frame F |
| `--menu` | draw the options menu |
| `--view2=YAW,PITCH,F` | change the view from frame F |
| `--mouse=X,Y`, `--mouse2=X,Y,F` | set the pointer, the second from frame F |
| `--click=F`, `--click2=F`, `--rclick=F`, `--rclick2=F` | clicks at frame F |
| `--attack=F` + `--hold=N` | with `--walk`, a swing every 60 frames after F |
| `--choose=a,b` | answers for console conversation mode |
| `--set=key=value` | a one-run setting override (up to 16), applied after settings.txt and not saved |
| `--sound-wav=FILE,SECONDS[,EFFECT]` | scanned separately at line 991; see step 3 above |

Note that `--talk=` and `--picks=` each appear twice in the chain (lines 941/980 and 942/956). The later branches are never reached. (2026-11-16: the later ones are now `--conv=` and `--frame-picks=`.)

*Shot mode* (lines 1049-1219):

1. `render_init` with no window, `ui_start`, `game_start`, then the level.
2. The optional core walk, in the working folder `<root>\build\core_run\SAVE0`.
3. For each frame: apply the scheduled events, then `render_frame`.
4. `render_readback`, then `png_write`.

The torch does not flicker in shot mode (t = 0).

*Window mode* start-up:

- Window class "uo-new" (WS_POPUP at desktop size when fullscreen), with raw mouse input registered.
- `render_init`, `ui_start`, `game_start`.
- Every start is a new game; the autosave is not resumed (`run_new`, `game_new`, `game_init_char(false)`).
- With `original_movement`: `cw_start(game_dir, <root>\save\core)` plus `cw_clock`, and the torch is given if the setting says so.
- Sound: the Roland through `sound_start` and title song 1, falling back to `music_start`.

*Main loop* (lines 1291-1489), each frame:

1. Pump messages; dt is clamped to 0.05.
2. Esc closes the menu, or calls `game_escape()`, or opens the menu.
3. Hotkeys: F6 relief; `'` DLSS 5, or DLSS when there is no DLSS 5; `]` style; `;` ray tracing; F3/F2 change level.
4. `player_step`, skipped while the menu or character creation is up.
5. Follow a level change made by the core.
6. Build `GameInput`: keys, clicks, typed text, digits, the pointer in overlay pixels, and a world ray through the pointer.
7. While the menu is up the game gets dt 0, so it is paused.
8. `game_update`, then save the pack checkbox if it changed.
9. `menu_update`, then capture or release the mouse.
10. `game_draw_ui`, then `menu_draw` + `ui_cursor(10)`, then `render_set_ui`.
11. Remesh if wanted.
12. F5/F9 and save/load requests.
13. `game_death_choice()`: 1 loads the newest `where.txt` among slots 0-4 and 9 (a new game if there is none); 2 starts a new game.
14. A new-game request: restart the core, level 1, character creation.
15. `game_travel`.
16. `place_torch`.
17. P copies a screenshot to the clipboard and shows it in the title for 2 s.
18. `render_frame`; Sleep(30) while minimized.

On exit it stops the sound and writes the autosave (slot 9), unless character creation is on.

**Original routines cited.** UW.EXE 0x1c62f and 0x1d2c2 (jump and gravity), and 0x74d53 (the title song at the start, line 1277).

**Dependencies.**

- Includes `base.h extract.h level.h normals.h render.h world.h game.h core_world.h inventory.h ui.h audio.h sound.h conv.h`.
- Declares `game_core_conv()` itself (defined in game.c).
- Calls into extract, normals, level, lights, render, ui, game, inventory, core_world (`cw_*`), conv, sound/audio and png.

---

## uwview.c

**Purpose.** The C face of `uwview.dll`, which puts our D3D12 renderer (render.c: DLSS, DLSS 5, ray tracing) under the port: underworld-exhumed's UW1 on our Win32 backend, `port/win32/plat_win32.c`. The backend loads the DLL when it sits beside the program, and draws with GDI when it does not.

Each frame the backend hands over the game's live level block, camera, palette, light level and 320x200 screen. From them the DLL:

- builds the 3D scene: level mesh, lights, object and creature cards, doors, animated effects, particles;
- composes the interface layer: the full-screen HUD, our message box, our menus, our inventory panel.

The header comment ("Step 1 ... over an empty scene") is older than the code, which builds the whole level.

**Settings and start-up**

- `read_settings()` (46): parses `data\settings.txt` with the same keys and defaults as main.c's `read_settings`. It covers vsync, the dlss* and dlss5_* keys, exposure, raytracing/rt_quality, normal_maps, gamma, bounce, the sprite_* keys, fov, mouse_sensitivity, invert_mouse, object_shadows, flame_swell, sheen, torch_in_hand, moving_light and fx_audio/acoustics/fog/particles/decals/water.
  - All `FX` effects are off by default.
  - Sheen is 0 by default because the user wants no shiny walls.
- `settings_env()` (100): environment overrides used by the checks: `UWVIEW_NORMALS`, `UWVIEW_RT`, `UWVIEW_DLSS5`, `UWVIEW_FULLBRIGHT`, `UWVIEW_DEBUG=normals|water`, `UWVIEW_HAND`, `UWVIEW_FX_*`.
- `find_dirs()` (142): walks up to 8 folders from the DLL looking for `dlss\nvngx_dlss.dll`. That folder is `root`, and it gives `dlss_dir` and `log_dir`.
- `uwv_init()` (167): reads the settings, fills `RenderConfig`, attaches the FX passes and calls `render_init`.
  - DLSS stays off until a level arrives.
  - `UWVIEW_TEST=fail` returns 0, so the port falls back to GDI; `UWVIEW_TEST=nodlss` simulates having no DLSS.
- `uwv_ui_size()` (208), `uwv_game_dir()` (238), `data_dirs()` (239): the data folder is `UWVIEW_DATA`, or `<root>\data` when that is not set.

**Level and camera**

- `level_build()` (256): on first use it sets up a 1 GB arena, `extract_missing` and `models_load(game)`. Each build then runs `level_load`, `level_from_block`, `lights_load`/`lights_tiles`, `level_mesh`, `render_upload`, the door list and the shape hash, and forces the palette's cycling search to run again.
- `uwv_level()` (296): rebuilds only when the level number (1-9) changes.
- `uwv_camera()` (322): a move of more than 2 tiles (squared distance > 4) counts as a teleport. It resets motion and clears the decals. The camera is ignored while a comparison runs.
- `uwv_light()` (339): the original's light level, 0-7.
- `uwv_roll()` (422), `uwv_lens()` (426), `uwv_shade_scale()` (428, override `UWVIEW_SHADEK`), `uwv_view_rect()` (435): pass-throughs to render.

**Palette**

- `cycle_find()` (361): loads palette 0 of `DATA\PALS.DAT`. It records atlas pixels in rows 0-1023 within ±2 per channel of a cycling entry, up to 65536 of them.
- `uwv_palette()` (390) reproduces two effects of the original's palette (COLCYCLE.C's cycle_colors):
  - **Fades and flashes** become a per-channel tint: the sum of the current palette over the sum of the base palette. Entry 0 and the cycling entries are skipped, the tint is clamped at 2, and it is applied only when it differs from 1 by more than 0.02.
  - **Colour cycling** (entries 0x10-0x17 and 0x30-0x3F) recolours the recorded atlas pixels and re-uploads atlas rows 0-1023.
  - 6-bit DAC values are expanded as `v<<2|v>>4`.

**Moving light**

- `moving_light_place()` (445): a test orb that runs along a hall. It is fixed at tile (31,5) on level 1 and takes the longer open run through that tile.

**Things in the level** (rebuilt every frame)

- `doors_find()` (483) and `door_quads()` (495): doors are items 0x140-0x14F, and 0x148 and up are open.
  - A swing takes about a third of a second.
  - The moving door 0x1CF is read through its owner field (USEITEMS.C's moveDoor): `own&7` is the door kind and `own<8` means opening.
  - The original's models (`world_door_model_quads`) are used unless `UWVIEW_OLD_DOORS` is set or the models are not loaded.
- `shape_hash()` (529): an FNV-1a hash of the tile shapes. A change (a trigger, a secret door) makes `things_update` rebuild the mesh.
- `read_light_table()` (557) and `burning_find()` (574): turn objects into lights. The comment says the original lights only the player, so these lights are ours.
  - **Lit lantern, torch, candle and taper (0x94-0x97)**: brightness from `data\objects\lights.txt`, defaults {0,0,0,0,4,2,1,3}.
  - **Campfire 0x12A**: intensity 4, range 5, 1800 K.
  - **Ankhs (0xAD, 0x157)**: a gold glow.
  - **Spark 0x1CB and explosions 0x1C2-0x1C4, 0x1C7**: a flash.
  - The function returns an FNV signature of what burns where.
- `lights_rebuild()` (644): the level's lights, then the burning things, then the moving light.
- *CPU particles* (used when `FX.particles` is off):
  - `part_add()` (679) adds one particle. When `FX.particles` is on it spawns GPU particles instead.
  - `blood_of()` (698) picks the blood colour by creature: green, grey dust for the undead, golems and the wisp, or red.
  - `creature_near()` (710) finds the nearest mobile record (at 0x4000 + i*27) within 1.5 tiles.
  - `part_burst()` (728) holds each effect's recipe.
  - `part_update()` (783) moves them: gravity 7 for glowing particles, -0.3 for smoke, 9 otherwise; sparks bounce; dt is capped at 0.05.
  - `part_cards()` (847) draws them.
- `halo_card()` (812) and `halo_cards()` (830): a breathing halo on the ankhs.
- `animo_cards()` (875): animated objects 0x1C0-0x1CE (EFFECT.C's do_animobj), skipped when invisible.
  - The frame is the owner field, or `item-0x1C0` when that is 0.
  - On its first appearance an effect fires its burst, plus a scorch decal and a ripple when those effects are on.
  - The spark is drawn 8/32 of a tile lower than the original's z+8 (COMBAT.C find_wall_coll), where the blade struck.
- `creature_frame()` (935): picks the animation frame as the original does (pgcache_x.c parse_cpg, L7D04, L7D0B-L7D11, L7CF3).
  - Sequences 0x20 and up use page 1, slot `((seq-0x20)<<3)+view`.
  - Other sequences except 0xC use the standing view when `((view+5)&7)>=3`.
  - The index is `segment*8 + (frame&15)`, so frames past 7 read on into the next segment.
  - An entry of 0xFF draws frame 0; a missing slot draws nothing.
- `creature_cards()` (970): walks the object chains and draws mobiles 2-255 with items 0x40-0x7F.
  - The view is `DIRTAB[((heading<<2)+0x20-(viewyaw>>11)) % 0x20]` (DRAWOBJ.C dirtab).
  - Last frame's positions are kept for DLSS motion vectors.
- `things_update()` (1021), once per frame:
  1. Checks the shape hash and rebuilds if it changed.
  2. `level_from_block`, then `burning_find`, rebuilding the lights if the signature changed.
  3. Gathers sprites, doors, models, animo, particles, halos and creatures into `render_set_sprites`.

**Picking**

- `pick_card()` (1054): ray against an upright card facing the eye, with a 0.03 margin. It steps along the ray in 0.05 steps and rejects hits behind a solid tile or the floor.
- `uwv_pick()` (1073): builds the ray from yaw, pitch, roll and the lens. It tests sprites and creatures, sorts the hits by distance, and packs each as `index | tile<<10 | min(dist*8,511)<<22`. Called from the port's uwlook.c.

**Interface layer**

- `uwv_compose()` (1141): scales the game's 0x00RRGGBB screen into the layer through column and row lookup tables. Alpha is 0 where `clear[]` marks the 3D view.
- `border_keys()` (1188) and `hud_piece()` (1202): cut a piece out of the original screen, key out its border colours, and optionally "ink" the rest in one colour.
- `uwv_held()` (1227) and `crosshair_ours()` (1228): draw one of these:
  - the held item at 2x;
  - cursor 0 as a thin gold-and-black cross with a centre dot;
  - another CURSORS.GR picture (10 is the pointer, 9 the hostile ring).
- `uwv_mouse()` (1298): mouse sensitivity and invert.
- `uwv_toggle()` (1303), the renderer hotkeys: 0 DLSS 5 / DLSS, 1 ray tracing High → Normal → Off, 2 DLSS 5 style, 3 normal maps, 4 full bright, 5 object shadows (saved).
- `hud_put()` (1331): no caller in this file.
- `uwv_readout2()` (1350), `uwv_readout()` (1356), `hud_readout_draw()` (1357): the name and hint text at the bottom right (gold 255,230,160; the hint in grey, small font).
- `uwv_screen()` (1386): the last 320x200 screen, used by uwinv_draw.c. Not exported.
- `uwv_fight()` (1389): fight mode on or off.
- `uwv_hud()` (1390): the full-screen HUD (layer cleared to transparent, scale `uh/270`). It draws:
  - the compass (112,131, 56x24);
  - the flasks (248,125 and 284,125, 24x36);
  - the power gem (4,139), only in fight mode;
  - the message-scroll text (14,169, 290x28), unless `log_ours`;
  - in fight mode, the weapon (rows 70-127, x 64-211, scaled by uh/113);
  - the crosshair, except within 2 frames of the inventory being drawn.
- `uwv_log()` (1497): our message box, 5 rows on parchment, with "^" when there are older lines.
- `uwv_menu()` (1539): an older 2-row "Options" menu. The port still looks it up (plat_win32.c:990).
- `settings_save()` (1585) and `uwv_fov()` (1611).
- `uwv_option()` (1615): menu values with their steps, saved to settings.txt. Ids: 0 DLSS, 1 DLSS 5, 2 style, 3 RT, 4 bounce, 5 normal maps, 6 FOV 50-120, 7 mouse 0.1-4, 8 normal strength 0.25-4, 9 object shadows, 10 particles, 11 decals, 12 water, 14 fx_audio. Id 13 is not handled.
- `uwv_menu_rows()` (1714): the Esc menu in the original's OPTBTNS art at 3x, over the screen dimmed to alpha 140, with the game's cursor 10.
- `ui_ready()` (1781), `ui_blend_overlay()` (1792), `uwv_inventory()` (1816), `uwv_inv_hit()` (1826): call `uwi_panel()` in uwinv_draw.c. `uwv_inventory` stamps `inv_frame`, which hides the crosshair.

**Frame: `uwv_frame()` (1834)**

1. Sets the UI layer (frozen during a comparison).
2. Builds `Look` and runs the `UWVIEW_COMPARE` / `UWVIEW_COMPARE_FX` logic.
3. `fx_frame()`.
4. Places the torch in the right hand (eased in with `1-exp(-18dt)`, kept 0.08 off walls and 0.15 above the floor). At light level 0 it becomes a steady white "eyes" light. The colour is 1700 K.
5. Sets the torch motion. The flicker formula is computed and then overridden (`flick = 1`) because the flame model in render.c now does it.
6. `render_set_torch_original(...)`.
7. The moving light.
8. `things_update()`.
9. Screenshots: `UWVIEW_SHOT_CRIT`, P to the clipboard, Shift+P to the recorder.
10. `render_frame`.
11. The comparison PNGs and diff statistics.

**Effects and exports**

- `cmp_at()` (312): the comparison frames 60/120, or 64/128 for FX, so both shots sit at the same point of DLSS's 16-frame jitter.
- `gpu_part()` (2015), `fx_decal()` (2024).
- `fx_frame()` (2034): embers and the odd spark from every flame light within 14 tiles, at a rate of `3+45*flame` per second.
- `fx_decal_screen()` (2096), `fx_level_change()` (2108).
- `uwv_want_shot()` (2117), `uwv_readback()` (2126), `uwv_record()` (2130, into `<root>\videos`), `uwv_recording()` (2138), `uwv_clipboard_shot()` (2140), `uwv_shutdown()` (2142), `uwv_status()` (2177).
- `uwv_fx_event()` (2151), for sound events:
  - sound 4 (a blow landed, COMBAT.C's sound 4) leaves a blood decal;
  - sound 0 (the player in water, PLAYMOVE.C) makes a ripple and a GPU splash.

**Original routines cited.** One address: "SHADES.DAT's 4th word, UW.EXE 0x810f8" (line 1864). Labels from exhumed:

- pgcache_x.c parse_cpg / L7D04 / L7D0B-L7D11 / L7CF3;
- DRAWOBJ.C do_obj and dirtab;
- COLCYCLE.C cycle_colors;
- LIGHTING.C cur_light_level;
- PLAYDATA.C FixPlayerEquips;
- USEITEMS.C moveDoor;
- EFFECT.C do_animobj;
- COMBAT.C find_wall_coll;
- PANELS.C (change_sprite, FL_X 0xF8/0x11C, adjust_power);
- CHARGEN.C (start square 0x20,2).

**Layout of the level block, as read here**

- 64x64 tiles of 4 bytes; the object chain head is word 2 >> 6.
- Objects start at 0x4000: mobiles 0-255 are 27 bytes each, and the statics after them are 8 bytes.
- The next pointer is the word at +4 >> 6, and the item is word 0 & 511.
- Mobile fields: position at +2 (z = &127, heading = >>7 &7, y = >>10 &7, x = >>13); home at +0x16; sequence = byte 0x15 & 0x3F; frame = word 0x0B >> 12.
- The game's tile (x, y) is our (x, z).

**Debug environment variables** (all read only here): `UWVIEW_SHOT_CRIT`, `_COMPARE`, `_COMPARE_FX`, `_ANIMODBG`, `_CRIT_LOG`, `_CRIT_FORCE`, `_PARTTEST`, `_PARTCELL`, `_FX_LOG`, `_FX_TEST`, `_OLD_DOORS`, `_SHADEK`.

**Shared layer buffer.** `ui_buf` is shared: `uwv_log`, `uwv_menu`, `uwv_menu_rows` and `uwv_inventory` draw into whichever layer the last `uwv_hud` or `uwv_compose` made.

**Dependencies.**

- Includes `render.h base.h extract.h lights.h world.h models.h ui.h uwrec.h ../port/win32/uwinv.h`.
- Calls render (`render_*`, the `rfx_*` FX passes), world, level, lights, models, ui, uwrec, and `uwi_panel`.
- Callers: every `uwv_*` export is looked up by `plat_win32.c` (lines 981-1017).

---

## uwrec.c / uwrec.h

**Purpose.** Shift+P video recording and P clipboard shots for uwview.dll.

- A recording is a 15-second H.264 MP4 at the window's size, with no sound, written through Windows Media Foundation's sink writer.
- Each frame read back goes into a ring of 6 slots. A worker thread encodes it, stamped with the real time it was shown, so the video plays at the speed the game ran.
- When the encoder is behind, the frame is dropped rather than slowing the game.
- Files go to `videos\uw-YYYYMMDD-HHMMSS.mp4`.

**Main functions.**

- `rec_worker()` (37): the encoder thread. It wakes on an event or every 100 ms and drains the full slots.
  - Each slot is converted RGBA → BGRA with alpha 255.
  - The timestamp is `(when-start)*1e7/freq`, forced to increase strictly, with a duration of 1/60 s.
  - On stop it finalizes the file, logs the frames written and dropped, shuts down MF and COM, and opens Explorer with `/select` on the video. It skips Explorer if no frames were written or if `UWPORT_OFFSCREEN` is set.
- `rec_open()` (88): sets up the sink writer with hardware transforms allowed: H.264 at 24 Mbit/s and 60 fps, with an RGB32 input written top row first.
- `uwrec_start()` (134): CoInitializeEx and MFStartup, the file, 6 slot buffers (allocated once and reused), the end time and the worker. It refuses while a recording or its worker thread still exists.
- `rec_finish()` (155); `uwrec_active()` (162); `uwrec_path()` (163, no caller found); `uwrec_seconds_left()` (164).
- `uwrec_frame(readback)` (173): reaps a finished worker and ends a recording on time. When the next slot is still full it counts a drop; otherwise it reads into the slot, stamps the QPC time and signals the worker.
- `uwrec_clipboard()` (194): one frame as a 32-bit CF_DIB, bottom-up BGRA.
- `uwrec_shutdown()` (224): finishes any recording and waits up to 10 s for the worker.

**uwrec.h.** It declares the functions above. `hwnd` is `void *` so that no Windows header is needed, and `readback` is `bool (*)(unsigned char *rgba)`.

**Notes.**

- All state is one static `Rc`.
- Each slot has a `volatile LONG full[]` flag, set with InterlockedExchange. `put` is touched only by the render thread and `get` only by the worker.
- MF and COM are started per recording and shut down by the worker.

**Dependencies.** Media Foundation (`mfapi.h mfidl.h mfreadwrite.h`) with COBJMACROS. Its only caller is uwview.c.

---

## uwinv_draw.c

**Purpose.** Our inventory panel (Tab in the full-screen display), in uwview.dll and uwtour.exe. It shows the original's right-hand panel made tall:

- At the top, the original's live panel copied from its 320x200 screen: the figure, the worn slots and the weight stone.
- Below it, in a Bagnon-like style: the pack's 8 holes, then every opened bag's contents in rows of 4, using art from `data\ui\screens\MAIN.png`.
- A gold name line and a "SHEET" check box.
- Optionally, the character sheet (PANELS.GR picture 2) and the rune page.

The data comes as a `UwiSnap` from `port\win32\uwinv.c`. The backend handles the clicks; this file only returns hit codes.

**Main functions.**

- `capital()` (32), `thing_in()` (39): an object's picture centred in a hole, with the count in gold when qty > 1.
- `tip_add()` (58) and `tip_of()` (67): the item tooltip, ported from the old inventory.c `tip_build`/`tip_draw`.
  - It shows the name, condition, "Enchanted", protection, damage, weight and worth.
  - Then "If you equip this:", with green and red differences against what is worn in that slot.
- `slot_name()` (102): names for display positions 1..11, shown on hover over an empty hole.
- `sheet_draw()` (113): the character sheet on `panels\002.png`.
  - The top 66 rows are drawn as they are, with the name and level/class written above.
  - The stats go at rows 21+7k.
  - The skills box (rows 66..90) is repeated to fit 20 skills, followed by its bottom and the page edge from `000.png`.
  - Skills at 0 are dimmed.
- `sheet_tip()` (156): hover text with this character's numbers filled in, ported from game.c's `sheet_tip`. For example, attack = Attack/2 + weapon skill + Dex/7; max mana = (Mana+1)*Int/8; next level at `NEXT[level]*50`.
- `blit_pic()` (265) and `blit_screen()` (280): opaque copies from a Pic or from the original's screen.
- `top_keep()` (303) and `blit_cache()` (319): a ring of the last 8 drawn tops. The top from 6 frames ago is used while the original's panel is not on the inventory page (for example the rune page), and for 6 frames after it returns, because the screen can be a frame ahead of the snapshot.
- `bag_open()` (357).
- `layout()` (364): the rows and holes, from the original's own slot rectangles `s->rect[12]` and `s->rect[16]`.
  - Pitch defaults to 18 and hole height to 15.
  - The pack holes are nodes with parent < 0 and slot 11+k.
  - Each open bag gets whole rows of 4.
  - Limits: 40 rows, 140 holes.
  - There is no bag bar any more: a right click on a bag in the grid opens or closes it.
- `uwi_panel()` (419): the entry point. It lays out, hit-tests and optionally draws.
  - Hit codes: -1 off the panel; 0 elsewhere on the panel; -2 the check box; 1..11 the body slots (screen origin OX=237, OY=7); `UWI_HIT_NODE+n`; `UWI_HIT_PACK+k`; `UWI_HIT_INTO+bag`; on the rune page `UWI_HIT_RUNE+cell` (RUNES.C ShowRune: 18 apart, 15 a row, x 0xF4), `UWI_HIT_CAST` and `UWI_HIT_SHELF`.
  - It writes the fractional position inside the hit hole to `*fx/*fy`.
  - When drawing it paints over the original's open-bag and scroll-arrow positions 20..22 with plain leather, and shows an open bag (an even item in 128..138) with its +1 "open" picture.

**Notes.**

- `Hole.dim` is always 0, so the dimmed-bag drawing is unreachable.
- The header comment still mentions a bag bar (`UWI_HIT_BAGBAR`), which the code no longer has.

**Dependencies.**

- Includes `ui.h` and `../port/win32/uwinv.h`.
- Calls `uwv_screen()` in uwview.c.
- Called by `uwview.c` (lines 1820 and 1829).
- It is compiled as its own translation unit; nothing #includes it.

---

## uwconv_draw.c

**Purpose.** Two DLL-exported compositors in uwview.dll:

- conversations: pieces of the original's screen over our dimmed 3D view;
- cutscenes (for example death): a cutscene alone over it.

`port\win32\uwconv.c` does the layout, and finds these by name with GetProcAddress (lines 66-67).

**Main functions.**

- `uwv_conv()` (14):
  1. Fills the layer with black at alpha `dim`.
  2. Takes n pieces of 6 ints each: screen x, y, w, h and layer dx, dy.
  3. Draws each piece with a dark edge (alpha 0xE0, width scale/2+1), then blits it `scale`× opaque, swapping R and B.
  4. Skips the screen's palette-0 black 0x000004, so the dimmed view shows through.
- `uwv_cutscene()` (57): one rectangle, scaled, with no edge.
  - It skips 0 and 0x000004.
  - It skips any pixel within a summed RGB difference of 24 of MAIN.png at the same position (the original's own frame), so only what the cutscene painted shows.

**Notes.** Both functions share one static output buffer, so each call overwrites the other's result. The byte order is R, G, B, A.

**Dependencies.** `ui.h` (`ui_pic_file`, included mid-file). It is in the uwview.dll build only, not uwtour.exe.

---

## uwmind.c

**Purpose.** Not part of the original game. It is our own "Strata" runner for the LLM creature AI (see `LLM_NPC_AI.md`), built as `uwmind.dll` against llama.cpp by `tools\buildmind.cmd` and placed in `dist\UW1\llm\bin`.

- `port/win32/uwmindhost.c` loads it at run time (GetProcAddress at lines 366-373). If the DLL or the model is missing, the game plays on with its rules alone.
- All calls happen on the game's worker thread.
- Exports: `uwm_load`, `uwm_score`, `uwm_ask`, `uwm_free`.

**Main functions.**

- `now_ms()` (45); `quiet()` (54), a log sink that drops llama.cpp's logging; `uwm_free()` (56).
- `make_ctx()` (66): `n_ctx = n_batch = 8192`, `n_ubatch 512`, `n_seq_max = UWM_BATCH+1 = 9`, unified KV. A comment says a batch larger than n_batch would abort on a GGML_ASSERT.
- `uwm_load()` (76):
  - Loads the ggml backends, then the model with ngl GPU layers. If the model or context fails with ngl ≠ 0, it retries on the CPU.
  - It uses a quarter of the logical CPUs, and on the CPU its own low-priority thread pool.
  - It checks that the choice words (`hunt ambush rally return attack investigate flee wait`) are scorable, and sets `score_off` if one is not.
  - It writes an info string and returns 1, or 0 on failure.
- `template_split()` (138): applies the chat template around a marker and splits it into prefix and suffix, falling back to ChatML.
- `prefix_ready()` (157): decodes the system prefix once into sequence 0, cached by its text.
- `first_token()` (179), `token_after_goal()` (189), `choice_token_ok()` (203): check that a word's first token is the same alone and after `goal=`. `token_after_goal` returns -2 when the tokenizer merges the boundary (Qwen joins "=hunt").
- `uwm_score()` (215):
  - Takes up to 8 questions, copying sequence 0 into sequences 1..n, all in one batch with logits only at each question's last token.
  - The pick is the choice whose first token has the highest logit, or -1 on a clash or when `score_off` is set.
  - An oversized batch is retried one question at a time.
  - Returns elapsed ms, or -1.
- `uwm_ask()` (281): a full chat prompt with a GBNF grammar and greedy decoding, at most 24 tokens. It clears the memory, so the next score re-reads the prefix.

**Dependencies.** `windows.h`, `llama.h`, `ggml-backend.h`. It includes none of the port's headers.

---

## nrbridge.c

**Purpose.** The source of `uw_nvngx.dll`, the bridge to the user's DLSS 5 neural DLL (`nvngx_dlssnr.dll`).

That DLL's Init_Ext looks up the module it was called from (GetModuleHandleExW FROM_ADDRESS on its return address). It refuses with 0xBAD00002 unless that module's path contains "nvngx.dll". So every call to it is made from here. The calls match the ones OptiScaler's forwarder makes, decoded with REA.

**Main functions.**

- `nrb_load()` (26): LoadLibraryExW with altered search path, then resolves Init_Ext, CreateFeature, EvaluateFeature and ReleaseFeature, plus the optional `NVFP4_GetLastError`.
- `nrb_init()` (37), `nrb_create()` (42), `nrb_evaluate()` (47), `nrb_release()` (52): each stores its result in a `volatile int` before returning, so the call returns into this module instead of tail-jumping.
- `nrb_error()` (57).

**Notes.** It must be compiled with `-fno-optimize-sibling-calls` (both build scripts do this). Its only user is `ngx_nr_start` in ngx.c.

---

## ngx.c / ngx.h

**Purpose.** Drives NVIDIA NGX from plain C, without NVIDIA's MSVC loader library. It finds the driver's `_nvngx.dll` through the registry and calls its exports the way the loader does (read from nvsdk_ngx_s.lib, SDK 310.9.16; the code came from shock-engine). It provides three features to render.c:

- DLSS Super Resolution;
- DLSS Ray Reconstruction (feature 13, nvngx_dlssd.dll);
- DLSS 5 neural rendering (feature 18), through nrbridge.c.

**Main functions.**

*Start-up and DLSS Super Resolution*

- `ngx_start()` (63):
  - Reads `HKLM\SOFTWARE\NVIDIA Corporation\Global\NGXCore\FullPath`, loads `_nvngx.dll` and resolves 8 exports.
  - Calls Init_Ext with APP_ID 231313132 (the SDK samples' development id).
  - Queries `SuperSampling.Available` and allocates the parameter object.
  - The env var `SHOCK_NGX_LOG` turns on verbose NGX logging.
- `ngx_dlss_available()` (114).
- `ngx_dlss_create()` (116): flags MVLowRes | IsHDR | AutoExposure. All `DLSS.Hint.Render.Preset.*` are set to 0, as NVIDIA's loader fills them in when unset (Populate_Unspecified_Defaults).
- `ngx_dlss_evaluate()` (145): the motion-vector scale comes from env `UW_MV_SCALE` (a test; default 1).
- `ngx_dlss_release()` (174).

*Ray Reconstruction*

- `ngx_rr_ready()` (186).
- `ngx_rr_create()` (188): its own parameter object. Denoise.Mode 1, Roughness.Mode 1 (packed in the normals' w), Use.HW.Depth 1. A failure latches `RR.failed`.
- `ngx_rr_evaluate()` (222): diffuse and specular albedo, and one normal+roughness texture passed as both Normals and Roughness, plus the view and clip matrices. Only the first failure is logged.
- `ngx_rr_release()` (255).

*DLSS 5*

- `ngx_nr_passes()`, `_set_params()`, `_set_style()`, `_style()`, `_error()` (288-294); `nr_fail()` (296).
- `ngx_nr_start()` (302):
  - Loads `uw_nvngx.dll` from beside the exe.
  - Init with app id 0x24480451 and API version 0x15.
  - Creates feature 18 once per pass (up to 3, each with its own history). A failure on pass 0 is fatal.
- `ngx_nr_subrect()` (356): limits DLSS 5 to the original's 3D window, because given the whole frame the model darkened the window's tone.
- `ngx_nr_evaluate()` (358): colour and output in output pixels; depth and motion vectors scaled to render pixels. The style settings are re-sent every frame.
- `ngx_nr_stop()` (407).

*Shutdown*

- `ngx_stop()` (417).

**Notes.**

- `NVSDK_NGX_Parameter` is an MSVC C++ object. It is called through a hand-built 17-slot vtable `ParamVtbl` "in MSVC's slot order (overloads reversed)".
- The function-pointer typedefs use the loader's signatures, which differ from the header in places.
- ngx.h declares the `DLSS_PERF / BALANCED / QUALITY / DLAA` values, `NrSettings`, and the input formats: colour RGBA16F, motion vectors RG16F in render pixels, depth D32F, output UAV. The styles are 0 A-Default, 1 B-Natural, 2 C-Cinematic.

**Dependencies.**

- Includes `d3d12.h`, `dxgi1_4.h` and `nvsdk_ngx_defs.h` (dlss\include).
- Called only from render.c. render_decals.c and render_particles.c include ngx.h only to get d3d12.h with COBJMACROS.

---

## render_decals.c

**Purpose.** The decal pass (Task 4 of `docs\superpowers\plans\2026-11-12-fx-audio-and-visual.md`; shader `shaders\decal.hlsl`). It puts marks on the floor: blood, scorch, ice, acid.

- The marks are a ring of 512 squares a hair above the floor, turned by yaw.
- They are drawn after the level, depth-tested with no depth write, and multiply the floor colour.
- This is presentation only: decals are never saved or read by the game, and walls get none.

**Main functions.**

- `upload()` (34).
- `rfx_decals_init()` (48): the root signature (one CBV) and the PSO, which uses the 40-byte `DVert` layout and blend DEST_COLOR × src with alpha unchanged. It also creates a persistently mapped vertex buffer (6 × 512) and the constant buffer.
- `render_decals_add()` (96): overwrites the oldest slot.
  - The default colours per kind: blood 0.40/0.04/0.03, scorch 0.22/0.19/0.16, ice 0.75/0.88/1.0, acid 0.45/0.75/0.12.
  - It stamps a seed and the birth time.
- `render_decals_clear()` (108), `render_decals_count()` (109).
- `rfx_decals_draw()` (112): each decal grows from 0.6× to full size over 0.25 s, with alpha `0.85*a`, placed at y + 0.006. The constant buffer holds the view-projection and the time.

**Dependencies.**

- `render.h` (`RD_KINDS`) and `ngx.h`, plus the generated shader headers `decal_ps.h` and `decal_vs.h`.
- Registered by uwview.c through `RenderFxPasses` / `render_fx_attach`. `render_decals_add` and `render_decals_clear` are called from uwview.c.
- Not in uw.exe.

---

## render_particles.c

**Purpose.** The GPU particle pass (Task 3 of the same plan; `shaders\particles.hlsl`). It uses one structured buffer of `RP_MAX` (65536) 64-byte `RParticle`s as a ring. Each frame:

1. A compute update moves every particle: gravity, drag, the floor, the end of life.
2. A compute spawn copies the frame's new particles into the next slots.
3. One draw of 6 vertices per particle blends them in, depth-tested, with no depth write and no motion vectors.

The live count is tracked on the CPU from each particle's life, with no readback.

**Main functions.**

- `clock_s()` (45), `buffer()` (47), `root()` (60).
- `rfx_particles_init()` (70): the compute and graphics roots, and the spawn, update and draw PSOs (premultiplied blend ONE / INV_SRC_ALPHA). It sets `clear = true` because the buffer starts as whatever the memory held.
- `render_particles_spawn()` (126): up to `RP_SPAWN_MAX` 8192 per frame, recording each slot's death time.
- `render_particles_clear()` (136), `render_particles_live()` (143).
- `rfx_particles_frame()` (152), before the scene:
  1. dt is clamped to 0.05 and `RpConst` is filled.
  2. Update (RP_MAX/256 groups), then a UAV barrier, then spawn. New particles first move on the next frame.
  3. The buffer goes to the SRV state.
- `rfx_particles_draw()` (196): draws `RP_MAX*6` vertices, then goes back to the UAV state. It only runs if the compute ran this frame.

**Notes.** `RParticle.kind`: 1 glowing (bounces), 2 lit (gone on the floor), 3 smoke.

**Dependencies.** `render.h`, `ngx.h`, and the generated `particles_cs_spawn.h`, `particles_cs_update.h`, `particles_ps.h` and `particles_vs.h`. The spawns come from uwview.c's emitters.

---

## models.c / models.h

**Purpose.** Runs the original game's 32 3D models straight from their bytecode inside `game\UW.EXE`: door frame and leaves, the shrine's ankh, benches, boulders, pillars, tables, chests, barrels, chairs, the bridge, the moongate and so on. It turns the result into our `Vertex` triangles.

The caller fills a `ModelInst` that mirrors what DRAWOBJ.C's do_rect and do_door set up: the Clk(n) variables, heading, texture and ceiling delta point. The interpreter follows exhumed's INTERP.ASM and TMAPOPS.ASM, and builds the points the same way, so a sub-program turned by an angle (a door leaf on its hinge) lands where the original puts it.

**Main functions.**

- `models_for_item()` (26): DRAWOBJ.C's `rect_sub` model for items 0x150-0x16F, or -1.
- `models_cols()` (31): `rect_cols[model]`. The table has 30 rows, while `MODELS_N` is 32.
- `models_ready()` (43) and `models_palette()` (44).
- `models_load()` (48): finds the model table in UW.EXE at one of {0x4e910, 0x4ccd0, 0x4e370, 0x4ec70}, accepting the first one whose first words are 0x4ab6 and 0x4006. The model base is the table + 0x8E. It also reads palette 0 from PALS.DAT.
- `rot()`, `def_pt()`, `rel_pt()` (106-129): points in 8.8 fixed point (1.0 = a tile). A triple comes in (x, z, y) wire order (INTERP.ASM's do_defres).
- `colour_index()` (131): for flag 0x20 models, var0..var0+7 darken the texture's RGB by 7% a step.
- `face()` (144): the normal comes from the last jnorm, or Newell's method. The polygon is ear-clipped in its dominant plane, because the original's filler takes any simple polygon and a fan would fill the ankh's hole. Faces are textured, flat, or Gouraud (`1 - 0.045*shade`).
- `sub_turned()` (235): a sub-program with an extra rotation.
- `run()` (246): the bytecode interpreter, with recursion capped at 24.

  | Opcode(s) | Name in the comments | What it does |
  |---|---|---|
  | 0000 | do_eof | end of program |
  | 0006 / 000C / 000E / 0010 | sort planes | run both halves |
  | 0012 | do_sfcal | call a sub-program |
  | 0014 / 0016 | do_ifle / do_ifge | compare a variable, skip if true |
  | 0002 | do_movec | store a constant in a variable |
  | 0040 / 0044 | fill choices | ignored |
  | 0048 | do_ijmp | jump |
  | 004A | do_rorg | translate the origin |
  | 0058, 005E-0068 | jnorm | set the next face's normal |
  | 0078 | parapiped | view test, skipped |
  | 007A, 0082 | define points | points from triples |
  | 0086 / 0088 / 008A | x_rel / y_rel / z_rel | point = point + one-axis offset |
  | 008C | addres | point = a + b |
  | 0090 / 0092 / 0094 | two offsets | point = point + two offsets |
  | 007E / 0022 | polyres | flat polygon |
  | 00A0 | compact tmap | textured quad |
  | 00A8 / 00B4 / 00CE | textured faces | (point, u, v) lists |
  | 0050 | ihcall | sub-program turned by a constant |
  | 00BA | ihcall_ind | sub-program turned by a variable |
  | 00BC / 002E / 005C / 00C8 / 00CA | colour ops | set the flat colour |
  | 00BE | movei | skipped |
  | 00D4 | uwshade | per-point shades |
  | 00D6 | standard surface | switch to Gouraud |
  | 00D8 | (none) | no-op |

  Any other opcode logs a message and stops the model.
- `models_place()` (406): starts at `mbase + moffs[model] + 4 + 6` (skipping the header and extents). The delta point is slot 256. Returns the vertex count.

**models.h.** `MODELS_N 32`, `MODEL_VARS 16`, `ModelInst`, the `MCOL_*` flags (NCOLS, DELTA 0x08, TEXTURED 0x10, TEXCOL 0x20, HEADING 0x40, ANIMATED 0x80) and the API.

**Notes.**

- The variables live at 0x2920 + 2n ("Clk(0)'s address").
- Point operands are byte offsets >> 3, into 512 point slots.
- UnderworldAdventures' ModelDecoder was used as a map for the colour operands.
- The context is static, so the interpreter is not re-entrant.

**Dependencies.**

- `level.h` (Vertex, base helpers).
- Callers: world.c (lines 370-462) and uwview.c (`models_load`, `models_ready`).
- `tests\models_test.c`, `tools\models.py` and `docs\MODELS.md` cover the same programs.

---

## normals.c / normals.h

**Purpose.** Makes the "stone relief" assets from the textures: per-texture normal maps (RGBA at 4x size) and simplified relief meshes (OBJ). This is presentation only.

- Mortar is found as pixels clearly darker than their surroundings.
- A stone's height grows with its distance from the nearest mortar and levels off (a cushion), plus a little relief from the speckle painted inside it.
- It runs at start-up and only remakes the maps the engine made itself, so maps the user has edited are kept.

**Main functions.**

- `wrap()` (10); `drop_specks()` (13), which clears mortar blobs smaller than a minimum (8-connected, wrapping).
- `normals_make()` (38):
  1. Luminance and a 9×9 local mean.
  2. Mortar = `mean > 0.06 && lum < loc*0.62 && lum < 0.45`, so all-dark textures stay flat.
  3. Speck removal.
  4. Height `1-(1-t)^3` over the bevel.
  5. A 3×3 soften, plus speckle `(lum-loc)*0.25`.
  6. A 4x bilinear upsample and central-difference normals scaled by `NORMAL_DEPTH_TILES * texels_per_tile`.
  - Alpha is the height.
- `height_at()` (138); the quadric helpers (151-163); the heap (185-204); `tri_area2()` (206); `collapse_cost()` (211); `merge()` (231); `push_edges_of()` (257).
- `relief_current()` (273): an OBJ is current if its first line is `# uo-new relief v7`. Older engine headers are remade, and any other file is user work and is kept.
- `relief_mesh_write()` (284):
  1. A full grid, with each diagonal along the smaller height change.
  2. Plane quadrics.
  3. Borders simplified by Douglas-Peucker (0.8 texels), identically on opposite borders so the texture still tiles.
  4. Garland-Heckbert edge collapses down to about `MAX(64, w*w/14)` triangles.
- `file_hash()` (411).
- `normals_missing()` (421): walls 000-209 and floors 000-051.
  - It keeps `data\textures\normal_maps.txt` (version plus NAME HASH).
  - A map is remade when it is missing, or when the version is old and the file's hash still matches the listed one.
  - The bevel is `4.5*w/64`; a tile is 64 texels for walls and 32 for floors.
  - Returns the count made, or -1.

**normals.h.** `NORMAL_SCALE 4`, `NORMAL_DEPTH_TILES 0.035f`. The outputs are `walls_normal\NNN.png` (256²), `floors_normal\NNN.png` (128²) and `walls_mesh` / `floors_mesh\NNN.obj`.

**Dependencies.**

- `base.h`.
- Called from main.c (`normals_missing`).
- render.c uses `NORMAL_DEPTH_TILES`; level.c reads the maps and meshes.
- `tools\relief_section.c.txt` holds a copy of `relief_mesh_write`.

---

## world.c / world.h

**Purpose.** Turns a level's objects into vertex geometry for the renderer. This is presentation only; what blocks and what is pointed at belong to game.c. It builds:

- billboard cards for item pictures (OBJECTS.GR), animated effects (ANIMO.GR), creatures and wall torches;
- flat decals for wall decorations;
- doors, either ours or through the original's models;
- the original's 3D object models (0x150-0x16F).

**Main functions.**

- `world_item_shown()` (9): a plain item picture. It excludes 0, ≥0x140, creatures 0x40-0x7F and invisible objects (flags bit 5).
- `world_decal_cell()` (19): the sprite cell of a wall decoration: writing 0x166, gravestone 0x165, switches 0x170-0x17F.
- `world_decal_place()` (31): on the wall named by the heading (0 N/+z, 2 E/+x, 4 S, 6 W), 0.045 off it.
- `world_object_pos()` (42): tile + (x+0.5)/8 and (y+0.5)/8, height z/32.
  - **Quirk**: anything below the floor surface is lifted onto it. Our renderer is depth-tested and the original's is not, so on slopes things vanished under the floor. The comment cites AI.C move_mobile `SET_Z(to->height << 3)` and CRITTIME.C.
- `card()` (57): a billboard. The atlas cell is at `(k%32*32, 1024+k/32*32)`, and the colour alpha carries the shader "kind".
- `world_animo_card()` (74), `world_item_card()` (80), `quad()` (86), `quad4()` (109).
- `world_door_span()` (101).
- `world_door_quads()` (128): our own door.
  - The frame is 0.125 deep with stone reveals.
  - The leaf is a 0.04-thick board hinged at the picture's right edge, swung `open*90°`.
  - A portcullis rises instead.
  - The opening is cut to the picture's opaque bounds.
  - The leaf edges sample a quarter of the way in, so the outline does not streak.
- `world_door_quads_flat()` (225): an older flat door, prototyped locally, with no callers.
- `world_creature_card()` (275) and `world_creature_card_mv()` (278): a creature placed by its hotspot (the feet). `_mv` keeps the previous position for motion vectors.
- `world_sprites()` (298), in three passes:
  1. Item cards. These glow: 0x94-0x97, 0x12A, 0xAD (boosted gold). These are glass (kind 3.3): 0xBA-0xBF, 0x12D.
  2. Wall decals.
  3. Wall-torch cards for `LIGHT_WALL_TORCH` lights.
- `atlas_colour()` (353), the texture's first pixel, as in DRAWOBJ.C do_rect for flag 0x20 (seg009_38C); `wall_rect()` (359); `inst_init()` (363).
- `world_door_model_quads()` (374): the door as DRAWOBJ.C do_door draws it.
  - Frame model 1; leaf 14, 15 when secret, 12 for a portcullis.
  - An open or moving door is lowered by 0.75 (OpenDoor's SET_Z + 0x18, objyloc -= 0xC0).
  - Portcullis: vars[5] = steps 0-4 (USEITEMS.C moveDoor), delta_y = 0.8125 - 0.1875*steps.
  - Swinging door: vars[5] = `dir*4096`, signed by flags bit 4 (do_door's `Clk(5) = dir << 12`).
- `floor_rect_of()` (427).
- `world_model_quads()` (429): objects 0x150-0x16F, except 0x165/0x166, which are decals.
  - Heading is used only with `MCOL_HEADING`.
  - `MCOL_TEXTURED` picks the texture `(first&0x1F) + oflags%count`.
  - The bridge (model 2) uses the level floor `floors[(oflags-count)%10]` (do_rect's txtflr).

**world.h.** `SPRITE_SCALE` 1/48, `MAX_SPRITE_VERTS`, `DECAL_SCALE` 1/64, `DOOR_WIDTH` 0.5, `CREATURE_SCALE` 1/60. The `Door` struct: `open` runs from 0 (shut) to 1 (90°); heading 0/4 means the door spans x, 2/6 that it spans z.

**Notes.**

- Vertex `color[3]` is the shader kind: 1 level-lit, 3 lit card, 3.3 glass, 4 self-lit, 5 door/world quad.
- On cards, `nrm` holds the corner offsets, not a normal.
- Chain walks are guarded at 1024 steps.

**Dependencies.**

- `level.h`, `lights.h`, `models.h`.
- Callers: game.c (sprites, door quads, cards, decals) and uwview.c (the door models, animo, creatures, models).

---

## objdata.c / objdata.h

**Purpose.** Loads the per-object-kind table `ObjInfo[512]` from `data\objects\objects.txt` (extracted from COMOBJ.DAT/OBJECTS.DAT), and the string blocks from `data\strings\blockNNNN.txt`. On these it implements the original's naming, look ("You see ...") and container-fit rules, following UW.EXE (see NOTES.md, "Objects").

**Main functions.**

- `load_block()` (16): `N: text` lines, with `\n` turned into a newline. It uses malloc so the strings survive the arena reset between levels.
- `str_get()` (52): returns "" when a string is missing. There are 11 blocks.
- `objdata_load()` (58):
  - Loads string blocks 1-6 and 8-10. Block 7 is not loaded.
  - Parses lines of the form `<kind> <item> key value ...`.
    - The raw bytes after `#`: byte 8 is `resist`.
    - `durability` goes to the weapon's or the armour's field.
    - `armour` takes 4 numbers; `a1`-`a3` take 3.
- `obj_info()` (145).
- `obj_quantity()` (148): when flags 0x40 is set and 0 < link < 0x200, link is the count.
- `obj_stacks()` (153), `obj_mass()` (155), the mass × quantity, without contents.
- `name_parts()` (158), `obj_noun()` (169), `obj_name()` (175): from the `article_noun&plural` names.
- `obj_describe()` (192): the look text as UW.EXE builds it (file 0x7ba10):
  - "You see ", then a count or a/an, then a quality word from block 5 at `qtype*6 + bucket`, then the name, then " belonging to" + block 1 string `370+owner`.
  - The bucket is 0 at quality 0, 5 if qclass is 3, otherwise quality/16+1.
  - qtype 13 has no article.
  - Objects without the look flag (file 0x7c0d1): writing and gravestones read block 8; the bridge is block 1 string 171.
- `wall_describe()` (234) and `floor_describe()` (238): block 10, by tex for walls and `510 - tex` for floors.
- `obj_fits()` (248): the container check (UW.EXE file 0x7aaeb).
  1. Weight first: the capacity must not be exceeded, else "The <noun> is too full.".
  2. Then what the container accepts:
     - -1: anything pickable;
     - 0x200: runes 0xE8-0xFF;
     - 0x201: ammo 0x10-0x12;
     - 0x202: maps and scrolls 0x138-0x13F;
     - 0x203: food (0xB0-0xBF, 0xCE, 0xCF, 0x92, 0x125, 0x11B, 0xD9);
     - any other value must equal the item.

**objdata.h.** The `ObjInfo` fields with their sources:

- resist: UW.EXE DS 0x5b76. Bits 0-1 magic, 8 fire, 0x10 poison, 0x40 missiles, 0x80 undead.
- swing: UW.EXE file 0x2574b and 0x257c8.
- critter: OBJECTS.DAT 0x132, 48 bytes; UW.EXE file 0x24b68 and 0x259ee.
- nutrition: OBJECTS.DAT 0xd82.
- material bits: 6-7 weapon, 4-5 hide, 0-3 walk sound.
- armour slots: 0 helm, 1 chest, 2 gloves, 3 leggings, 4 boots, 9 ring.

**Dependencies.**

- `level.h` and base.
- `objdata_load` is called from game.c:119. The rest is used widely from game.c, inventory.c and uwview.c.

---

## orig.c / orig.h

**Purpose.** One-to-one ports of UW.EXE's shared rule routines, so every roll follows the original:

- the Borland C random number generator;
- dice;
- the skill check;
- resistance;
- experience gain and kill experience.

`tools\verify.py` runs each original beside these in the emulator, and `re\proved.txt` lists them as proved.

**Main functions.**

| Function | Line | Original | What it does |
|---|---|---|---|
| `uw_srand()` | 6 | 0x12c26 | the seed is the u16 argument, so the high word is cleared |
| `uw_seed()`, `uw_set_seed()` | 7, 8 | DS 0x207e (the seed dword) | read or write the seed; no callers in src |
| `uw_rand()` | 10 | 0x12c37 | `seed = seed*0x015A4E35 + 1; return (seed>>16)&0x7FFF` |
| `uw_rand_mod()` | 15 | | `rand() % n` |
| `uw_rand_scaled()` | 17 | (helpers 0x35fb / 0x3636 cited) | `rand()*n/0x8000` |
| `uw_dice()` | 19 | 0x38eee | count + the sum of `rand_scaled(sides)`, so each die is 1..sides |
| `uw_skill_check()` | 26 | 0x3419c | `r = skill - diff + rand()%31`; >28 critical (2), >15 1, >2 0, else -1 |
| `uw_resist()` | 34 | 0x25eaf | magic bits are resisted when `rand()%3 < (res&3)`, otherwise dropped; any other bit still shared means no harm |
| `uw_player_get_exp()` | 44 | 0x341d8 (SKILLCHK.C:72, SKILLS.C:119) | see below |
| `uw_kill_exp()` | 66 | 0x25b35 (COMBAT.C:830) | `(i16)(exp*4 + dice(2,exp))`; if powerful (attitude bit 10), `×(rand()%24+24)` wrapped to i16, then /16 |

`uw_player_get_exp()` in detail:

- A loss floors exp at 0, keeping the i16 -(-32768) quirk.
- A gain stops once exp > 0x17700.
- The gain is halved +1 when `(i16)(dlvl*2+2) < level`.
- One skill point per 3000 exp.
- Levels come from the table {1,2,3,4,6,8,12,...,192} against exp/500, capped at 16.
- It returns the number of levels gained.

**orig.h.** `UwExp` maps the player bytes: 0x3D level, 0x4E exp in tenths, 0x52 skill points, 0x53 skill points earned.

**Notes.** The original reads the level table before testing the level, touching the byte past the table at 16. The C code tests the bound first, which gives the same result.

**Dependencies.**

- `base.h`.
- The rules are called from game.c. orig.h is also included by inventory.c and sound.c (`uw_rand` for the songs).

---

## png.c

**Purpose.** Despite the name, it holds the shared path helpers as well as PNG I/O. Its API is declared in `base.h`; there is no png.h.

- Writing uses filter 0 and stored deflate, identical to `tools\loose.py`.
- Reading has a full RFC 1951 inflate.

**Main functions.**

- `root_dir()` (10): the exe's folder, or its parent when only the parent has `game\DATA` (the exe in `build\`).
- `path_join()` (33); `make_dirs_for()` (35).
- `crc_init()` / `crc()` (50/57), `put32be` / `get32be`, `chunk()` (68).
- `png_encode()` (77): zlib 0x78 0x01, stored blocks of up to 65535 bytes, Adler-32. Scratch memory is in the arena.
- `png_write()` (130): colour type 2 or 6. `png_write_indexed()` (133): type 3 with a 768-byte PLTE.
- `bits()`, `decode()`, `build()`, `put()`, `codes()`, `inflate_raw()` (148-237): a canonical-Huffman inflate in puff's structure.
- `paeth()` (303).
- `png_read()` (308):
  - 8-bit, non-interlaced, types 0/2/3/4/6, up to 16384 a side. tRNS is read only for type 3.
  - Adler-32 is not checked.
  - It undoes filters 0-4 and expands to RGBA; `img->idx` holds the indices of an indexed file.

**Notes.** `png_read` leaves its buffers in the arena, so callers take `arena_mark` / `arena_reset` themselves (ui.c does).

**Dependencies.** `base.h`, `windows.h`. It is used by check.c, level.c, normals.c, ui.c, extract.c, main.c and uwview.c, and `make_dirs_for` by many more.

---

## ui.c / ui.h

**Purpose.** The CPU-drawn interface layer. Everything is drawn into an RGBA overlay a whole factor smaller than the window, about 300 rows high, so the original's pixel art keeps its look; the present pass lays it over the 3D picture.

- It loads the game's `.SYS` bitmap fonts and PNG pictures (converted .GR files under `data\ui` and `data\sprites`).
- It provides blending, fills, text, wrapped text, parchment and leather panels, buttons, insets and tooltips.

**Main functions.**

*Set-up and pictures*

- `ui_set_scale()` (10): a temporary scale for the Esc menu; 0 goes back to the window's scale.
- `ui_start()` (26): the scale is `MAX(1, (wh+150)/300)`. The overlay is allocated at full window size so any scale fits. It loads FONT4X5P, FONT5X6P and FONTBIG.SYS, copying them from `game\DATA` if they are missing.
- `ui_overlay()` (52), `ui_clear()` (53).
- `ui_pic_file()` (66): a PNG by path, with an 8-entry cache.
- `ui_pic()` (86): `<data>\<dir>\NNN.png`, with a 512-entry cache. The pixels are malloc'd so they survive level changes, and failures are cached too.

*Drawing*

- `blend()` (105), `ui_draw()` (117), `ui_fill()` (127).
- `ui_font_height()` (133), `glyph_width()` (135).
- `ui_clip()` (144).
- `ui_text_wrap()` (149): wraps at the last space or at '\n'. Line height is the font height + 2. With `draw` false it only measures.
- `ui_text_width()` (174).
- `ui_text()` (180): 1-bit glyphs, MSB leftmost. A black shadow is added only for light text (luminance ≥ 160).
- `ui_cursor()` (201): the hotspot is the centre, except cursor 0 at (7,7) and cursor 10 at (5,5).

*Panels*

- `ui_parchment()` (211), `ui_blit()` (220).
- `ui_leather()` (232): tiles OPTBTNS.GR picture 3.
- `ui_button()` (247): OPTBTNS pictures 24 and 25, stretched by repeating column 15 and row 4, which clears the lettering.
- `ui_inset()` (260).
- `ui_ink()` (267): a bright colour turned into a dark ink.
- `ui_tip()` (280): a tooltip at the pointer, kept on screen, with an optional heading.

**ui.h.** `Overlay`, `Pic`, `FONT_SMALL/NORMAL/BIG`, and the colours. The paper colours come from the original's conversation scroll (CONV.BYT); the other inks are ours.

**.SYS font format.** A 12-byte header (u16 1, bytes per char, space width, height, bytes per row, widest), then for each character its rows plus 1 width byte.

**Dependencies.**

- `base.h`.
- `ui_start` is called by main.c. The API is used by core_world.c, game.c, inventory.c, main.c, uwconv_draw.c, uwinv_draw.c and uwview.c.

---

## sound.c / sound.h

**Purpose.** The Windows audio back end.

- It loads Munt (`mt32emu.dll`) with the user's Roland ROMs from `<data>\..\roms`, CM-32L preferred.
- It drives two Munt instances through the ported driver (xmidi.c), one for music and one for effects.
- An audio thread mixes them to 32 kHz stereo through waveOut, servicing the driver 120 times a second and the effects timer 16 times a second (UW.EXE 0x1fe99).
- It implements the `SongsAil` and `SfxAil` callback tables that link songs.c and sfx.c to the driver and to the `UW.MT` timbre library.

**Main functions.**

*Munt and the driver*

- `munt_load()` (28): 8 Munt C-API entries.
- `munt_open()` (59): the ROM pairs in order: cm32l 1.02, CM32L, mt32 1.07, MT32.
- `route()` (102): note on/off go to the effects Munt when `xmidi_channel_locked`, otherwise to music. Everything else goes to both.
- `mpu_out()` (111): reassembles MIDI messages from the port 0x330 bytes.
- `mpu_wait()` (126): a no-op.

*The AIL callbacks*

- `read_heap()` (128).
- `gtl_find()` (143): UW.MT has 6-byte entries (patch, bank, u32 offset).
- `ail_file()` (154, UW.EXE 0x1f6db), `ail_register()` (163), `ail_install()` (175, 0x1f341), `ail_rand()`.
- The `SONGS_AIL` table is at line 170 and the SfxAil table at line 328.

*Effects*

- `sound_effect()` (185), `sound_effect_at()` (192), `sound_effect_stop()` (199): under the lock; they return 0xFF with no Munt.

*Audio thread*

- `serve()` (207) and `fill()` (208): 512 frames at a time, interleaving the driver services, `sfx_tick` and both Munts' rendering.
- `audio_thread()` (239): 6 waveOut buffers.

*Start, stop and checks*

- `sound_render_wav()` (262): an offline check. It plays the title song, or fires an effect after 1 s, and writes a WAV. Returns true when the peak is > 0.
- `start()` (287): two Munts at 32000 Hz, waveOut, `sound\uw.mt`, `xmidi_init`, `songs_init`, then `sound\sounds.dat` into `sfx_init(...,6)` (device 6 is the Roland; UW.EXE 0x1fd8e), then the thread.
- `sound_stop()` (344).
- The `SONGS(call)` wrappers (366-389) take the lock, and do nothing while sound is off.

**sound.h.** The public API, with the original routines named per wrapper:

- `sound_music` 0x1f6e8;
- `sound_song_play` 0x1f39c, `_set_next` 0x2053a, `_random` 0x20545, `_restart` 0x2055c;
- `sound_songs_update` 0x2058d, `_engaged` 0x1b652, `_blow` 0x1c447, `_fight_mode` 0x274c5 / 0x27621;
- `sound_effect` / `_at` 0x1fa9b / 0x1f82e, `sound_effect_stop_all` 0x1fbfc.

**Notes.**

- Effect positions are in eighths of a tile.
- `songs_update` is always called with `main_screen=true`.

**Dependencies.**

- `orig.h` (uw_rand), `sfx.h`, `songs.h`, `xmidi.h`.
- Callers: main.c, game.c, inventory.c.

---

## sfx.c / sfx.h

**Purpose.** A routine-by-routine port of UW.EXE's sound effects (segment 1bd8).

- SOUNDS.DAT entries play on up to 4 slots. Each slot locks a driver channel, selects a bank-1 timbre and plays one note, ended by the 16 Hz timer.
- An effect's volume and pan come from its position relative to the player's position and heading.
- The driver is reached only through `SfxAil`, so `tools\sfx_verify.py` (via `tests\rtest.c`) can compare the calls with the original's.
- It also holds the original math library's isqrt and sin/cos.

**Main functions.**

- `sin_table()` (12): `round(32768 sin(2πi/256))`, clamped to ±32767, with cos = sin[i+64] (DS 0x5624:0x4e0).
- `uw_sincos()` (19, 0x23009).
- `uw_isqrt()` (24, 0x23018): Newton's method, 5 steps, with a starting guess picked by the highest nonzero byte. The 16-bit truncations are kept.
- `sfx_init()` (50; 0x1fd8e + 0x1fe99).
- `sfx_start()` (61, 0x1fee9):
  1. Takes the first free slot. Device 1 (PC speaker) is not ported and returns 0xFF.
  2. Installs the timbre if it is missing, and locks a channel.
  3. Sends bank 1, the program, reset controllers, volume, expression and pan, then the note.
- `sfx_play()` (90, 0x1fa9b): velocity + vol_delta, clamped 0-127.
- `sfx_play_at()` (98, 0x1f82e):
  - `dist = isqrt(dx²+dy²)`; pan = `0x40 - ((ny*cos - nx*sin) >> 8)`.
  - Beyond 0x30 (6 tiles) it returns 0xFF, out of hearing.
  - From 8 (1 tile) the volume falls as `vol*(0x30-dist)/0x28`.
  - An exact ±dist direction gives 0x7f or 0x80, the original's quirk.
- `end_slot()` (130), `sfx_tick()` (140, 0x1fc95), `sfx_stop()` (148, 0x1fb7d), `sfx_stop_all()` (152, 0x1fbfc).

**sfx.h.** `SfxAil` and `g_sfx`, with each field on its DS address:

| Field | DS address |
|---|---|
| active | 0x25f6 |
| chan | 0x2612 |
| time | 0x262c |
| note | 0x25f7 |
| timbre | 0x2606 |
| chans | 0x14c |
| device | 0x13a |
| on | 0x137 |
| ready | 0x136 |
| count | 0x2644 |

**Notes.**

- The status bytes use the 1-based channel (`0xaf+ch` = 0xB0 | (ch-1)).
- A SOUNDS.DAT record is timbre, note, velocity, then a big-endian duration in 1/256 s.

---

## songs.c / songs.h

**Purpose.** A routine-by-routine port of UW.EXE's music choice (seg 1bd8, 0x1f39c-0x2076c): which XMI plays and when. The choices are title, wandering, fight, Fight mode, kill, death and quiet. It is verified by `tools\songs_verify.py`. The caller must hold the sound lock.

**Main functions.**

- `songs_init()` (12, 0x1f26e).
- `songs_play()` (20, 0x1f39c):
  - The file is `uwNN.xmi` from two octal digits; `aw` replaces `uw` for AdLib.
  - Any failure turns the music off for good (`loaded=false`).
  - A start runs at tempo 100 % and volume 96 %.
- `songs_resume()` (51, 0x1f58d), `songs_current()` (57, 0x1f6b8), `songs_enable()` (60, 0x1f6e8), `songs_toggle()` (71, 0x1f75f), `songs_set_next()` (75, 0x2053a).
- `songs_next_random()` (76, 0x20545): `rand()%3 + 2`.
- `songs_ended()` (79, 0x20741).
- `songs_restart_if_over()` (82, 0x2055c): song 1 becomes 4.
- `fight_song()` (89): songs 5-7.
- `songs_update()` (92, 0x2058d):
  - Songs 9 and 11 play to their end.
  - A fight song more than 0xa00 ticks (10 s) after the last blow gives way to 8 in Fight mode, or otherwise to a random song.
  - One fight song switches to another only after 0x800 ticks (8 s).
  - When a song ends, a wandering song follows, or 8 in Fight mode.
- `songs_fight()` (120), `songs_engaged()` (125, 0x1b652), `songs_blow()` (132, 0x1c447): `q = hp*64/(max+1)`; below 16 it plays 5 when the player struck or 7 when a creature did, otherwise 6.
- `songs_fight_mode()` (138, 0x274c5 / 0x27621).

**songs.h.** `SongsAil` and `g_songs` with their DS addresses: cur 0x261e, next 0x261f, handle 0x2646, loaded 0x138, on 0x135, roland (0x13a == 6), combat_t 0x2482, switch_t 0x2634. The song list:

| Song | Use |
|---|---|
| 1 | title |
| 2-4 | wandering |
| 5-7 | fight |
| 8 | Fight mode |
| 9 | kill |
| 10 | death |
| 11 | played to its end |
| 13 | map, conversation, sleep |

`REPEATS[16]` (561b:0000, file 0x593b0) marks the songs that restart when they end. The clock is 256 Hz.

---

## xmidi.c / xmidi.h

**Purpose.** A routine-by-routine C port of UW's Roland driver `SOUND\MT32MPU.ADV`: the Miles AIL 2 XMIDI sequencer, an MT-32 interpreter and an MPU-401 interface.

- AIL 2.14's XMIDI.ASM v1.10, MT32.INC and MPU401.INC served as the map. The truth is UW's own 1991 driver, run in the emulator (`tools\adv.py` / `tools\adv_music.py`) and compared byte for byte by `tests\xmidi_test.c`.
- The driver's only output is bytes written to the MPU-401 ports, which sound.c turns into MIDI for Munt.
- The original routine names and 16-bit arithmetic are kept.

**Main functions.**

*MPU-401 (MPU401.INC)*

- `send_byte()` (81) and `send_cmd()` (82): ports 0x330 and 0x331.
- `reset_interface()` (83) and `init_interface()` (84, UART 0x3F).

*MT-32 (MT32.INC)*

- `sysex_wait()` (87).
- `send_sysex_msg()` (90): Roland DT1 with checksum.
- `add_sysex_addr()` (102): a 3 × 7-bit address with carries.
- `write_system()` (108), `write_rhythm_setup()` (112), `write_patch()` (118).
- `index_timbre()` (124), `setup_patch()` (129): timbre group 2 (memory) when cached, otherwise group A or B by patch < 64.
- `reset_synth()` (143).
- `init_synth()` (147): partial reserve {3,4,3,4,3,4,3,4,4}, reverb {0,3,2}.
- `send_MIDI_sysex()` (156).
- `send_MIDI_message()` (160): the MT-32 interpreter.
  - Controllers 32-46 are 3 sysex queues of 5, flushed as DT1.
  - It handles patch reverb and bender, reverb mode/time/level, bank select, rhythm timbre and timbre protect. 110-120 are swallowed.
  - A program change sets up the patch; a note bumps the LRU counter.

*Sequencer (XMIDI.ASM)*

- `find_seq()` (234): the n-th `FORM XMID` inside IFF `CAT `/`FORM`.
- `rewind_seq()` (256): volume 90, tempo 100 %, 120 bpm.
- `flush_channel_notes()` (269), `flush_note_queue()` (286).
- `reset_sequence()` (297): global sustain is cleared ("version 1.04: UW's driver may not do this").
- `restore_sequence()` (313), `XMIDI_volume()` (337).
- `XMIDI_control()` (348): indirect values (115), logging, volume scaling, CLEAR_BEAT_BAR 118, CALLBACK_TRIG 119, FOR/NEXT loops 116/117 (nesting 4, count 0 = forever), CHAN_PROTECT and CHAN_LOCK.
- `vlq()` (420).
- `XMIDI_note_on()` (429): a 32-slot note queue. When full, slot 0 is overwritten (the original's repne scasb).
- `XMIDI_meta()` (446): 0x2F end, 0x58 time signature and 0x51 tempo (used for beat counting only).
- `XMIDI_sysex()` (471).
- `xmidi_serve()` (479): one 1/120 s service, guarded against re-entry. For each playing sequence:
  1. The tempo accumulator runs one interval per 100 %.
  2. Each interval ages the note queue and runs the events due.
  3. Then the beat and bar counts, and the tempo and volume gradients.

*Public API*

- `xmidi_init()` (577): sends the channel 2-10 defaults and the Roland default programs {68,48,95,78,41,3,110,122}.
- `xmidi_shutdown()` (612).
- `xmidi_register()` (627): finds TIMB, RBRN and EVNT. The bounds check is ours.
- `xmidi_release()` (654), deferred while playing; `xmidi_start()` (665); `xmidi_stop()` (675); `xmidi_resume()` (683).
- `xmidi_status()` (690): a released handle returns 0, mimicking the original's stale pointer.
- `xmidi_rel_volume()`, `xmidi_rel_tempo()`, `xmidi_set_rel_tempo()`, `xmidi_set_rel_volume()` (695-712).
- Counts: `advance_count()`, `xmidi_beat_count()`, `xmidi_bar_count()` (727-743).
- `xmidi_branch_index()` (751): RBRN markers, BRANCH_EXIT per version 1.08.
- Channels (1-based): `xmidi_control_value()`, `xmidi_set_control_value()`, `xmidi_channel_notes()`, `xmidi_map_channel()`, `xmidi_true_channel()` (769-791).
- `xmidi_lock_channel()` (796): the channel 2-9 with the fewest notes. The first pass skips locked or protected channels, the second only locked ones.
- `xmidi_release_channel()` (815), `xmidi_send_cv()`, `xmidi_send_sysex()`, `xmidi_write_display()` (833).
- Timbres: `xmidi_timbre_request()` (841), `xmidi_install_timbre()` (855: a 14-byte common part plus four 58-byte partials into Roland timbre memory, replacing the LRU slot when needed), `xmidi_protect_timbre()`, `xmidi_unprotect_timbre()`, `xmidi_timbre_status()` (902).
- `xmidi_channel_locked()` (908): ours, used by sound.c's routing.

**xmidi.h.** `XmidiIO {out, retrace, user}`, `XMIDI_RATE 120`, `XMIDI_SEQS 8`. Sequence handles are 0, 4, 8 and so on, as in the original. Each function is tagged with its AIL function number:

| AIL # | Functions |
|---|---|
| 102-104 | init, serve, shutdown |
| 151-152 | register, release |
| 155-159 | timbre request, install, protect, unprotect, status |
| 170-181 | start, stop, resume, status, volume, tempo, set volume, set tempo, beat, bar, branch |
| 182-194 | control value, set control value, channel notes, send cv, send sysex, write display, lock, map, release, true channel |

**Notes.**

- The controller log is 9 rows × 16 channels: PV, MODUL, PAN, EXP, SUS, PBS, C_LOCK, C_PROT, V_PROT.
- `lock_status` bits: 0x80 locked, 0x40 protected.
- `QUANT_TIME` is 8333 µs.

---

## Open questions

These came up while reading the code. They are not settled in it.

- **main.c:941/980, 942/956**: `--talk=` and `--picks=` are tested twice. The later branches (`talk_arg`, `picks_arg`) can never be reached, so console talk mode and the per-frame picks look dead. (2026-11-16: put right, see the root README)
- **main.c:432**: F4 flying only works on our own movement path.
- **uwview.c:1615**: `uwv_option` has no id 13.
- **uwview.c:1948**: `render_set_torch_original`'s 5th parameter is named `range` in render.h, but uwview.c passes `flick` (always 1).
- **ngx.c:127**: DLSS is created with the IsHDR flag, while a comment calls the colour display-referred (0..1).
- **models.c:283, 351**: jnorm reads (x, y, z) order while point triples are (x, z, y); compact tmap uses raw byte indices. Neither is confirmed against TMAPOPS.ASM.
- **models.c:49**: the source of the four candidate table offsets is not stated.
- **sfx.c:149**: `sfx_stop` accepts slots up to 7, while there are 4. (2026-11-16: checked, the original tests the bit of an 8-bit mask with no range check: not a mismatch)
- **songs.h vs songs.c**: engaged is 0x1b666 in the header and 0x1b652 in the .c; blow is 0x1c44d/0x1c48d and 0x1c447. (2026-11-16: checked in UW.EXE, both are right: the block's start and the lines in it)
- **world.c:225**: `world_door_quads_flat` has no callers.
- **uwinv_draw.c**: `Hole.dim` is never set; the header still describes the bag bar.
- **objdata.c:59**: string block 7 is not loaded.
- **xmidi.c:61**: `trigger_fn` is always NULL, so CALLBACK_TRIG only stores the value.
