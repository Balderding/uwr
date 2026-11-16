# port\ - our Win32 backend for underworld-exhumed's UW1 port

`port\win32\` holds the platform backend the game runs on. The game itself is underworld-exhumed's whole UW1 port:
the original's code, byte-matched C, in `underworld-exhumed\uw1`, branch `uw-new`. Everything in this folder is ours.
Nothing here changes a game rule. The backend gives the game its window, input, sound output and timing, and reads
the game's state for our renderer (`uwview.dll`) and our own interface.

The source of truth is this folder. `underworld-exhumed\uw1\src\port\platform\win32\` only holds copies (untracked
in the exhumed repo).

## How it is built

The backend is not built by `build.cmd`. It is built by `tools\portwin32.py` (see its docstring):

```
O:\appsnotonc\sentinel\uv\python\cpython-3.13.17-windows-x86_64-none\python.exe -I tools\portwin32.py [--debug|--release ...]
```

(`work\locked.sh` wraps a build in the shared build lock when several sessions or agents work at once.)

What `tools\portwin32.py` does, in order:

1. Copies every file of `port\win32\` into `underworld-exhumed\uw1\src\port\platform\win32\`, where Exhume's
   `portbuild.py` looks for a backend. It refuses to overwrite a copy that differs from its source and is newer,
   unless you pass `--force-copy`. So always edit `port\win32\`, never the copy.
2. Answers `pkg-config` itself: `win32` gives the Windows libraries (`-luser32 -lgdi32 -lwinmm -ldwmapi -lshell32
   -lole32`), and `mt32emu` gives our Munt build in `build\munt`.
3. Adds the LLVM clang MinGW target to every compile. It also adds `-DUWPORT_GAMEVIEW` (uw-new), or
   `-DUWPORT_PLAIN` with `--original`. Every link gets `-static`, and the program's link gets
   `-Wl,--wrap=cFBtoScreen -Wl,--wrap=get_eye` (used by `uwgame.c`).
4. Compiles the backend folder with `-O2`, like the port's translated modules.
5. Runs `tools\buildview.cmd` first. That builds the renderer `build\uwview.dll`, the DLSS 5 bridge
   `build\uw_nvngx_port.dll` and `build\uwtour.exe`. It also runs `tools\buildmunt.cmd`.
6. Runs Exhume's `portbuild.py` on `underworld-exhumed\uw1\exhume.toml`, unchanged.
7. Copies what the program needs beside it in `underworld-exhumed\uw1\build\port\`: `uwview.dll`, `uw_nvngx.dll`,
   `dlss\*`, `mt32emu.dll`, and the user's two CM-32L ROMs from `roms\` if they are there.

The result is `underworld-exhumed\uw1\build\port\uw1port.exe`. `tools\makedist.py` packs it into `dist\UW1`.

There are two builds:

| Build | Define | Source tree | What it is |
| --- | --- | --- | --- |
| uw-new | `UWPORT_GAMEVIEW` | `underworld-exhumed` (branch uw-new) | Everything: our renderer, our display, our panels. |
| uw-original (`--original`) | `UWPORT_PLAIN` | `exhumed-original` (a worktree at the upstream commit) | The original's own 320x200 picture through GDI. No renderer, no DLSS. |

`UWPV_OFF=1` in the environment builds uw-new without `UWPORT_GAMEVIEW`. That build matches the old frame instead,
for comparisons.

## Threads

- **The main (window) thread** runs `plat_run`'s loop (`loop_step`): window messages, input, presenting.
- **The game thread** (`game_thread`, created with a 16 MB stack) runs the game. `uwgame.c`, `uwinv.c` and
  `uwlook.c` take their snapshots on this thread. They lock, or use atomics, so the window thread can read them.
- **Workers:** the waveOut audio thread (`audio_thread`), `uwaudio.c`'s Munt rendering worker (`mt_worker`), and
  `uwmindhost.c`'s language-model worker (`worker`).

## Files

### plat_win32.c (3074 lines)

The Win32 version of Exhume's platform API (`runtime/port/platform/plat.h`). It replaces the SDL3 backend and follows
it point by point. It is the only file that includes the window and sound parts of `windows.h`. The main areas:

| Lines (start) | What |
| --- | --- |
| 93-121 | `plat_pointer_capture`, `plat_screen` (the game's screen state, from underworld-exhumed's `port/sys/screen.c`), the death fade (`plat_death_fade_start` / `plat_death_fade`) |
| 127-232 | Keys: `key_byte` / `game_key` (set-1 scan codes to the game, 0x100 for the E0 prefix), `special_key`, `handle_key` |
| 233-372 | `game_thread`, `plat_game_exit`, the counter, `plat_sleep_ns`, `plat_thread_start`, the waveOut audio (`audio_thread`, `plat_audio_open`) |
| 380-614 | The window's geometry and the pointer: `set_capture` (ClipCursor, the hidden cursor), `pointer_game`, `raw_motion` (WM_INPUT), `key_msg` |
| 615 | `wnd_proc`, the window procedure |
| 731-867 | `plat_max_scale`, `set_fullscreen`, `plat_set_display`, `plat_set_mouse_lock`, `plat_key_byte`, `plat_pointer_event`, `plat_window_click` |
| 868-943 | The folder picker (`browse_folder`, `start_pick`) |
| 944 | `veh_where`: the fault reporter when `UWPV_VEH` is set |
| 968 | `view_start`: loads `uwview.dll` beside the exe and gets its `uwv_*` entry points. It falls back to GDI when the DLL is missing, `UWPORT_GDI` is set, or the build is `UWPORT_PLAIN`. |
| 1027 | `view_size`: starts the D3D12 renderer at the client size. If it fails, the backend uses GDI from then on. |
| 1081-1145 | `display_apply` (our display 0, or the original's display 1), `log_draw`, `full_play` / `full_show` |
| 1146-1556 | Our inventory panel (Tab / I): pointer handling (`inv_pointer`), carrying and throwing (`carry_begin`, `carry_release`, `throw_point`), bag state (`inv_bags_save` / `_load`), `inv_draw`. Commands go to the game thread through `uwinv.c`'s `uwi_cmd`. |
| 1557-1858 | Our Esc menu: volumes (`volumes_load`, `volume_step`), save slots (`slots_read`), the menu rows (`menu_rows_build`, `menu_choose_step`), `menu_event` |
| 1859-1892 | Taps held until the game takes them: `tap_end`, `tap_pump`, `send_key` (NOTES.md, "Our taps are held until the game takes them") |
| 1893-2069 | `first_pick`, `display_init`, `our_action` (the actions of `uwinput.h`'s UWI_A_*), `interface_toggle`, `readout_show` (the crosshair readout), delayed keys |
| 2070-2322 | The input module's handlers: `world_click`, `settings_event`, `panel_event`, `world_event`, `centred_event`, `game_pointer`. Also `input_state` (fills `UwiState`), `look_publish` (mouse-look's want, sent to `port_mouse_look_want`), `capture_sync`, `input_sync`. |
| 2326-2471 | `view_tan`, `keep_last`, `view_world`: the camera, level, light and lens of our 3D view, taken from `uwgame.c` |
| 2472 | `view_present`: the game's picture placed into the interface layer, clear where our 3D view shows through |
| 2688-2728 | `fx_audio_start`, `fx_pump`: drains underworld-exhumed's effect events (`port_fx_drain`) once per picture, to `uwaudio.c` and to the renderer (`uwv_fx_event`). Then `port_fx_control`. |
| 2729 | `present`: palette to RGB, then our renderer or GDI's StretchDIBits, plus screenshots |
| 2810 | `loop_step`: one pass of the main loop (messages, the `UWPORT_FOCUSTEST` hook, input, presenting) |
| 2932 | `log_start`: when the exe was started on its own, stderr and stdout go to `home\logs\uw-new.log` (rotated past 5 MB) |
| 2959 | `plat_run`: makes the window, registers raw input, starts the renderer and the effects audio, starts the game thread, and loops. At exit it stops `uwmindhost.c` and `uwaudio.c` before the DLLs go. |
| 3051-3074 | `plat_message`, `plat_choose_folder`, `plat_base_dir` |

Environment switches read here, for checks and debugging (see NOTES.md for the ones described there):
`UWPORT_GDI`, `UWPORT_OFFSCREEN` (the window is made but never shown), `UWPORT_NO_INPUT`, `UWPORT_LOOKDBG`,
`UWPORT_INPUTTRACE`, `UWPORT_GRABDBG`, `UWPORT_FOCUSTEST`, `UWPORT_MOUSETEST`, `UWPORT_NOCONV`, `UWPORT_TIMING`,
`UWPORT_INVDBG`, `UWPV_VEH`, and others (`UWPORT_FBDUMP`, `UWPORT_NOPICK`, `UWPORT_PICKTEST`, `UWPORT_SHOT_FRAME`,
`UWPORT_SIZE`, `UWPORT_INVTEST_RIGHT`, `UWPORT_WCLICK_RIGHT`).

### uwgame.c / uwgame.h - the game's 3D frame, for the renderer

The link wraps two of the game's routines (`--wrap`, set by `tools\portwin32.py`):

- `__wrap_get_eye` (line 40) keeps the camera exactly as PLAYMOVE.C's `get_eye` placed it (`cPlayer`), before
  VIEW3D.C's `setup_vars` rewrites it.
- `__wrap_cFBtoScreen` (line 53) runs after each copy of the 3D frame buffer to the screen. It keeps a copy of the
  frame buffer, the eye, the level, the light level, the zoom and the view size in a ring of `UWG_KEEP` (4) frames.

`uwgame.h` describes the frame buffer's layout (`UWG_FB_*`, `uwg_fb_at`) and `UwgSnap`. Readers:
`uwg_snapshot` / `uwg_snapshot_back` (line 122) and `uwg_view_info` (line 135).

In uw-new (`UWPORT_GAMEVIEW`), the game reports its own 3D view through underworld-exhumed's `src/port/portview.h`
hooks:

- `gv_notify` (line 83) copies `port_view` into a `UwgView`.
- On each main-loop frame shown, `gv_notify` also takes the inventory and crosshair snapshots (`uwi_capture`,
  `uwl_capture`).
- `uwg_gameview_replace` (line 114) sets `port_view_replace`. With it set, the game leaves out its own rasterization
  and our renderer draws the view.

The file only reads game state.

### uwinput.c / uwinput.h - who owns each key and button

Pure C with no Win32, so it can be unit-tested (`tests\uwinput_test.c`). Design:
`docs\superpowers\specs\2026-11-12-input-rewrite-design.md`; NOTES.md, "Input (2026-11-12)".

Each new press goes to the first owner that applies, in this order: SETTINGS, MENU, then F11 / Esc / Tab, then PANEL,
WORLD (our display's key table), CENTRED, and finally GAME. A release always goes to the owner of its press. A held
press whose owner is no longer valid is released at once.

| Line | Function |
| --- | --- |
| ~25 | `keys[]`: our display's key table (the old `our_keys`) |
| 63 | `handler`: calls the owner's handler |
| 92 | `keeps`: whether an owner may keep a held press in the new state |
| 141 | `uwi_state`: run every frame and on every change; releases presses that are no longer valid |
| 149 | `uwi_release_all`: the focus was lost |
| 156 | `uwi_look_wanted`: mouse-look's want |
| 176 / 223 / 242 | `uwi_key` / `uwi_button` / `uwi_move`: the events in |

`uwinput.h` lists the owners, the screen states (the same values as `screen.h`'s `PS_*`), the key table's actions,
our actions (`UWI_A_*`), `UwiState` and the output callbacks (`UwiOut`).

### uwinv.c / uwinv.h - the inventory, for our panel

Runs on the game thread. It reads the original's inventory data into a `UwiSnap`: the slots, display positions and
rectangles, the bag tree, the held object, the weights, the runes and the character sheet. Our panel
(`src\uwinv_draw.c` in `uwview.dll`) draws from that snapshot. Everything that changes the inventory is a command
(`UWI_OP_*`) queued by the window thread (`uwi_cmd`, line 226, a ring of 32). The commands run on the game thread
between main-loop passes, through `MAINLOOP.C`'s `port_inv_hook = cmds_run` (`cmds_run`, line 579).

The rules are the original's, ported line by line. The file comment at line 214 lists them: INVPANEL.C ItemFitsSlot /
AddToOccupiedSlot, INVDATA.C takeFromSlot / AddToInventory, BAGS.C PutObjectInBag / SwapItemsInBag.

Main functions:

| Line | Function |
| --- | --- |
| 38 | `wear_slot` |
| 65 | `thing` (one `UwiThing`) |
| 130 | `tree_build` |
| 197 | `reveal` |
| 293 | `fits_slot` |
| 331 | `bag_takes` |
| 374 | `take` |
| 410 | `put_in_bag` |
| 455 | `put_on` |
| 530 | `put_pack` |
| 540 | `cmd_use` |
| 655 | `uwi_capture` |
| 748 | `uwi_snapshot` |

`uwinv.h` documents every field of the snapshot and every command's arguments. It includes no game header and no
Windows header, so the backend, the game side and the DLL can all include it.

### uwlook.c / uwlook.h - what the crosshair is on

Runs on the game thread after a 3D frame (`uwl_capture`, line 236). It runs the original's own pick (INTERACT.C's
`pick_3d`) at the view's centre and puts the game's pick globals back afterwards. It then takes names from the game's
own words (`texture_words` line 70, `critter_words` line 82, `object_words` line 117). It does not call LookAt
itself, because LookAt prints. Nothing is printed and no random number is drawn.

The window thread reads `uwl_get` (line 50) and `uwl_info` (line 162, a `UwlInfo`: kind, hostile, reach, take, held
...) for the crosshair picture and the readout. `uwl_set_targets` (line 179) passes our renderer's pick at the
pointer. `uwlook.h` also declares the port's `keys.c` variables the backend sets (`port_interact_take`,
`port_grab_armed`, `port_throw_*`).

### uwconv.c / uwconv.h - conversation, options panel and map in our display

When the game is on its conversation screen (`scrnum == 2`), the window shows pieces cut from the original's 320x200
screen: the conversation area, the right panel and the message scroll (`P[]`, line ~20). They are scaled by a whole
number and centred over our last 3D frame, dimmed. The drawing itself is the renderer's `uwv_conv`, found with
GetProcAddress (`uwc_bind`, line 64).

| Line | Function |
| --- | --- |
| 34 / 52 / 72 | `layout_options` / `layout_map` / `layout`: the layouts for the options panel, the automap and a conversation |
| 84 | `uwc_frame`: builds this frame's layer |
| 135 / 140 | `uwc_frame_window` / `uwc_frame_cut`: the 3D window alone (a cutscene, the death) |
| 155 | `uwc_point`: maps a window point back to the original's screen, so the pointer reaches the game |
| 44-49 | `uwc_talking`, `uwc_options`, `uwc_mapping`: state tests |

### uwlog.c / uwlog.h - the message scroll as text

The port's hook in SCROLLIO.C's `scroll_print` calls `port_scroll_note` (line 29) on the game thread. That function
strips the escapes and wraps at 49 letters, at the last space, as the game does. It keeps a ring of 200 lines.
`uwlog_get` (line 59) gives lines to the window thread for our message box (`src\uwview.c`'s `uwv_log`).
underworld-exhumed's `port/sys/hunt.c` also writes its AI lines here.

### uwaudio.c / uwaudio.h - positional sound effects (fx_audio)

Off unless `fx_audio 1` is in `data\settings.txt`. When on:

- OpenAL Soft (`bin\soft_oal.dll`) is loaded at run time (`load_dll`, line 380) and never linked.
- Each effect event from underworld-exhumed's `port/sys/fxevent.c` plays at the place the game gave
  (`uwaudio_event`, line 602), with the original's falloff: full volume within 1 tile, nothing beyond 6.
- The sound is the Roland's own. Each SOUNDS.DAT effect is rendered once per velocity by a Munt of our own
  (`mt_start` line 283, `mt_worker` line 246, `mt_render` line 208).
- While our copy of an effect exists, the game's copy on the music driver is silenced
  (`uwaudio_taken`, through `port_fx_control`).
- A `data\sfx\NNN.wav` file takes precedence over the Munt rendering (`wav_load`, line 539).

For checks, `uwaudio_init_loopback` / `uwaudio_render` (lines 489 / 508) render through OpenAL Soft's loopback device
into memory (`tests\uwaudio_test.c`), and `UWPORT_FX_LOOPBACK=FILE` writes a WAV during a game run.

### uwmindhost.c - the language model host

Loads `uwmind.dll` (`src\uwmind.c` over llama.cpp, built by `tools\buildmind.cmd`) from `llm\bin`, with a model
from `llm\models`. One worker thread (`worker`, line 359) loads the model and answers the questions that
underworld-exhumed's `hunt.c` / `hunter.c` put in (`port_mind_ask`, line 531). Answers come back through
`port_mind_take` (line 559). The game never waits.

Nothing is loaded when the DLL or the model is missing, when `llm=0`, or when `UWPORT_LLM=0`. `UWPORT_LLM=0` is set
by `run-tests.cmd` and by every off-screen check.

The settings (`uw1port.cfg`: `llm`, `llm_model`, `llm_gpu`, `llm_mode`, `llm_rate`, `llm_stale_ms`, `llm_idle_min`,
`llm_calibrated`) are listed in the file's header. Other main functions: the model list for the Esc menu
(`models_scan` 141, `port_mind_models` 181, `port_mind_pick` 238), `bring_up` 274, `port_mind_ready` 471 and
`port_mind_stop` 505. Every question and answer is logged to `home\logs\ai-<date>.jsonl` (`log_jsonl`, line 91).
Background: `LLM_NPC_AI.md`.

## How the pieces link

```
underworld-exhumed\uw1 (the game, game thread)
  |-- cFBtoScreen / get_eye (wrapped) --------> uwgame.c --> uwg_snapshot / uwg_gameview --+
  |-- portview.h hooks (uw-new) --------------> uwgame.c gv_notify --> uwi_capture, uwl_capture
  |-- MAINLOOP.C port_inv_hook ---------------> uwinv.c cmds_run   (commands from the window)
  |-- SCROLLIO.C scroll_print ----------------> uwlog.c port_scroll_note
  |-- fxevent.c ring (port_fx_event) ---------> plat_win32.c fx_pump --> uwaudio.c, uwv_fx_event
  |-- screen.c port_screen -------------------> plat_win32.c plat_screen
  |-- hunt.c / hunter.c port_mind_ask/take ---> uwmindhost.c (worker thread, uwmind.dll)
plat_win32.c (window thread): wnd_proc --> uwinput.c (owners) --> game keys / pointer, our menu, panel, world
                              present --> uwview.dll (our D3D12 renderer: uwv_frame, uwv_compose, uwv_conv ...) or GDI
```

The files that belong to underworld-exhumed's side of these links are described in `..\underworld-exhumed-additions.md`.

## Running

Use the built program `underworld-exhumed\uw1\build\port\uw1port.exe`, or the packed `dist\UW1` (see NOTES.md, "The
port with our renderer"). The checks for this code are described in `tests\README.md`: `tests\uwinput_test.c` (in
`tools\verify.cmd`) and `tests\uwaudio_test.c` (through `tools\fxcheck.py audio`). Live-game checks of the backend
are `tools\inputcheck.py`, `tools\fxcheck.py`, `tools\deathcheck.py` and `tools\followcheck.py`.
