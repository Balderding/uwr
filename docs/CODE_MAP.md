# uwr code map

Which file does what, how one frame flows, where the rules code is, and how to build.

## 1. What uwr is made of

uwr is four layers. All but the first are ours.

| Layer | Where | Whose |
| --- | --- | --- |
| The game: the rules and the original's code, as C | `underworld-exhumed\uw1` (`src\combat`, `critter`, `motion`, ...) | underworld-exhumed. The rules folders are never edited by us; `src\port\` holds some files of ours (section 4). |
| The platform backend: window, keys, pointer, timing, sound output | `port\win32\plat_win32.c` (Windows), `port\sdl3\plat_sdl3.c` (Linux plan) | ours |
| The front end and the game wrappers: our menus, our panels, input routing, the snapshots the renderer needs, sound, the language model | the rest of `port\win32\` | ours |
| The renderer DLLs: `uwview.dll` (D3D12) and `uwview_vk.dll` (Vulkan) | `src\uwview.c` plus `src\render.c` (D3D12) or `src\render_vk.c` (Vulkan), and the files listed in section 2 | ours |

`port\win32\*` is the source of truth. `tools\portwin32.py` copies it into the exhumed tree at build time, so never
edit the copy there. The front end only reads the game's state (and sends a few commands through `uwinv.c`); it
does not change a rule.

The older files `src\game.c`, `src\main.c` and their neighbours (`check.c`, `core_*.c`, `inventory.c`, ...) are the
old standalone engine (`uw.exe`). It is not part of uwr. Several of its helper files (`level.c`, `world.c`,
`models.c`, `sound.c`, ...) are shared with the renderer DLL, and those are listed below.

Other documents: `port\README.md` (the backend in depth, threads, `plat_win32.c`'s line map), `docs\ESC_MENU.md`,
`docs\KEYBINDS.md`, `docs\LINUX_BUILD.md`, `docs\LINUX_RENDERER.md`.

## 2. Which file does what

### port\win32 (the front end and game wrappers; compiled into uwr.exe)

| File | Job |
| --- | --- |
| `plat_win32.c` | The Win32 backend of Exhume's platform API: window and scaling, event loop on the main thread, keys and pointer, the game thread, timing, waveOut audio output, dialogs, the log file. Loads the renderer DLL. No game logic. |
| `uwfront.c` | Our front end: the screen the game shows, routing of keys and pointer through `uwinput.c`, the Esc menu logic, our inventory panel, mouse-look and throw, hotkeys, the renderer's function table and the picture it is given each pass, effect audio. Reaches the window only through the `uwp_*` services in `uwfront.h`. Each Esc row has its own `MO_` id (the enums near the top and above `menu_rows`); free ids are 129 to 149, and a new row must not take a number another range uses (`MO_ENH` starts at 167). |
| `uwinput.c` | Decides who owns each key or button press (settings, Esc menu, inventory panel, the world, a centred screen, the game) and holds the key table. Pure C, no Win32. |
| `uwgame.c` | Takes a snapshot of what the renderer needs from the running game (the 3D frame buffer, player place and facing, level) where the game copies its 3D frame to the screen. Read only. |
| `uwinv.c` | Snapshot of the player's inventory (slots, weights, names, cursor object) for our panel. Also sends the panel's commands to the game thread (`uwi_cmd`). |
| `uwlook.c` | The name of what the crosshair is on, using the original's own pick and words, for the full-screen readout. Touches no game state. |
| `uwlog.c` | The game's message scroll kept as wrapped text for our own message box. |
| `uwconv.c` | Layout of a conversation in our display. The drawing is in the DLL (`uwconv_draw.c`). |
| `uwconvrec.c` | The record of a conversation, written by hooks on the game thread, read by copy on any thread. |
| `uwaudio.c` | Positional sound effects over OpenAL Soft (`soft_oal.dll`, loaded at run time). Plays what the game chose, with the original's falloff. |
| `uwacmat.c` | The acoustic materials table (which surface absorbs what). |
| `uwacoustic.c` | Our sound rays over the level's tile grid: occlusion and echo estimates. |
| `uwsteam.c` | Steam Audio reflections (`phonon.dll`, loaded at run time) for the "Acoustics: High" level. |
| `rlog.c`, `rlog.h` | The shared logger: one line per message, `[HH:MM:SS.mmm] level tag: text`, to stderr (`uwr.log`); levels error, warn, info, debug. Linked into `uwr.exe`, each renderer DLL (its own copy and level) and the SDL3 menu. |
| `uwmindhost.c` | Runs the language model for creature decisions on a worker thread (loads `uwmind.dll`). When the model is missing or switched off, the rules decide alone. |
| `uwos.h`, `uwlog.h`, ... `*.h` | Interfaces of the files above. `phonon.h`, `phonon_version.h` are Steam Audio's own headers (types only). |

### port\sdl3 (the SDL3 backend; built by `portwin32.py --backend sdl3` on Windows and `tools\portlinux.py` for Linux)

| File | Job |
| --- | --- |
| `plat_sdl3.c` | The SDL3 backend of the platform API. Hands input and picture to the same `uwfront.c` through the same seam. |
| `uwmenu.c` | Our Esc menu drawn into a pixel layer where the renderer DLL is not up. Uses `menu_draw.c` and `ui.c`. |

### The renderer DLLs (src\)

`uwview.c` is shared by both DLLs. The D3D12 renderer is `render.c`; the Vulkan one is `render_vk.c`. Both sit behind
`render.h`.

| File | Job |
| --- | --- |
| `uwview.c` | The DLL's C face (`uwv_*` entry points the backend finds by name). Takes each frame's game snapshot, builds the scene, drives the renderer, draws our menu, panel, message box, effects. |
| `render.c` | D3D12 renderer: scene pass, lights and flames, shadows, ray tracing, DLSS, present. |
| `render_particles.c` | GPU particles (embers, sparks, blood, splashes) for the D3D12 renderer. |
| `render_decals.c` | Decals lying on the floor, for the D3D12 renderer. |
| `ngx.c` | NVIDIA NGX (DLSS) through D3D12. |
| `nrbridge.c` | Builds `uw_nvngx_port.dll` (`portwin32.py` copies it beside the exe as `uw_nvngx.dll`), the bridge to the DLSS 5 neural DLL. A DLL of its own, not part of `uwview.dll`. |
| `render_vk.c` | Vulkan renderer (device, swap chain, present, scene, effects, ray tracing, DLSS). Its helper headers: `render_vk_cpu.h` (render.c's CPU side, copied and checked), `render_vk_fx.h`, `render_vk_rt.h`, `render_vk_dlss.h`, `render_vk_vol.h`. |
| `vk_load.c` | Opens the Vulkan loader at run time and takes the functions from it. |
| `ngx_vk.c` | NGX (DLSS) through Vulkan. |
| `ui.c` | Overlay drawing for our interface: buttons, text, leather. |
| `menu_draw.c` | Our Esc menu's drawing. Also used by the SDL3 backend's `uwmenu.c`. |
| `smoothtext.c` | Smooth text with GDI (Windows). Linux: `smoothtext_stb.c` (stb_truetype); `smoothtext_none.c` for the menu where there is no GDI. |
| `uwinv_draw.c` | Draws our inventory panel. |
| `uwconv_draw.c` | Draws a conversation in our display. |
| `uwrec.c` | Shift+P video recording (Media Foundation). Linux: `uwrec_posix.c`. |
| `normals.c`, `lights.c` | The relief maps (normal/AO/height) for textures and the light list. |
| `base.c`, `png.c` | Paths and small helpers, `fatal` (logged with file and line where the build links rlog.c); PNG reading and writing. |
| `gr.c`, `audio.c`, `conv.c`, `extract.c`, `level.c`, `world.c`, `models.c`, `objdata.c`, `orig.c`, `xmidi.c`, `sound.c`, `sfx.c`, `songs.c`, `collide.c` | Readers of the original's data files and ports of some of its routines (graphics, levels, models, objects, music and sound). Compiled into the DLLs (the `COMMON` list in `tools\buildview.cmd`). |
| `shaders\*.hlsl` | The shaders (scene, probes, present, particles, decal, volume). `buildview.cmd` compiles them to headers (DXIL for D3D12, SPIR-V for Vulkan). |
| `uwmind.c` | `uwmind.dll`, the language model runner over llama.cpp. A DLL of its own, loaded by `uwmindhost.c`. |

Not part of uwr: `game.c`, `main.c` and the rest of the old standalone engine (`uw.exe`).

## 3. How one frame flows

1. The window thread (`plat_win32.c`, `loop_step`) collects window messages, including raw mouse motion.
2. Every key and button goes to `uwfront.c`, which asks `uwinput.c` who owns it: the settings screen, the Esc menu,
   the inventory panel, the world (mouse-look, clicks), a centred screen, or the game. What the game owns is
   passed on to it as the original's own key or mouse event.
3. The game thread (created in `plat_win32.c`) runs the original's code and the rules. Where the game copies its 3D
   frame to the screen, `uwgame.c` takes a snapshot; `uwinv.c` and `uwlook.c` take theirs at the same point.
   They lock, so the window thread can read them.
4. Each pass, `uwfront.c` builds the picture for the renderer: the snapshots, the game's screen as the interface
   layer, and our menu, panel and message box state.
5. `uwview.dll` or `uwview_vk.dll` (`uwview.c` plus `render.c` or `render_vk.c`) draws the 3D scene with our own
   lighting and effects, lays the interface layer over it and presents to the window. If the DLL is missing, the
   backend falls back to GDI and the original's own picture.
6. Sound: the game's effects raise events. `uwaudio.c` plays them positionally through OpenAL Soft. `uwacoustic.c`
   and `uwacmat.c` supply occlusion and echo, and `uwsteam.c` adds reflections. Music and the original's own
   sound go through the game's sound code, to waveOut in `plat_win32.c`.
7. The language model (`uwmindhost.c`, if on) answers the creature questions that `hunt.c` (in `uw1\src\port\sys`, section 4) puts in. It never blocks the
   game.

## 4. The rules code

The game's rules live in the folders of `underworld-exhumed\uw1\src\` named below, byte-matched C translated from the
original's program. We do not edit them. A change there would alter the original's behaviour, and every replay and proof
against the original depends on it staying exact. Everything we add reads the game's state, or replaces only
presentation. The dividing line (the replaced-or-not review):

**Rules, never edited (the original's routines):**
- `combat\` (combat, damage, missiles, runes, spells)
- `critter\` (creature AI, creatures, their timing, path search and door tries)
- `motion\` (collision, motion, object physics, player movement)
- `obj\` (every use, combine, treasure, effect)
- `event\` (triggers, world events, trigger saves)
- `conv\` (the conversation VM, barter: quests and trade)
- `game\` (skills, skill checks, player, player data and time, the save and level code)
- `inv\` (what goes where and weighs what)
- `map\` (the map, addresses, light levels)
- `3d\`: only the grid database's one side effect (GRIDDB marks the tiles seen on the automap); the rest of `3d\` is presentation
- The game clock: the 256 Hz time and the 8-tick frame pace (at most 32 frames a second, which the physics' integer
  rounding needs).

The off-limits part is the rules folders: `combat\`, `critter\`, `motion\`, `obj\`, `event\`, `conv\`, `game\`, `inv\`,
`map\`, plus the clock and frame pace.

**`uw1\src\port\` is the port runtime layer and holds files of ours:**
- `sys\cunning.c`, `hunter.c`, `hunt.c`, `hunternav.c`, `hunterfile.c`, `pack.inc`: the creature AI levels. `hunt.c`
  is where the language-model questions are put in. They are ours, but they decide creature moves, so the
  readability work leaves them alone too.
- `sound\ail.c`, `sound\audio.c`, `sys\pit.c`, `sys\borland.c`: our copies, which `portwin32.py` builds in place of
  Exhume's.
- `platform\win32\`: where `portwin32.py` copies `port\win32\` (edit the source, not this copy).

**Presentation, scheduling, data carrying (may be ours when it helps):** 3D rasterisation, `gfx\` (fonts, bitmaps,
fades, colour cycling, cutscene drawing), `ui\` (automap drawing, panels, scroll, stats, options, main menu, cursor
drawing, input dispatch), the start menu and character creation screens, `sound\` (what plays through what), `sys\`
(file reads, EMS, overlays: DOS emulation), the inventory panel's drawing and pointer handling, the conversation
screen, cutscene framing, save file copying, the mouse driver.

Rule for any replacement: with it off, replays stay byte-identical; with it on, the mechanics' outputs (positions,
hit points, rolls, quest bits) stay the same for the same inputs.

## 5. How to build

Use the Python 3.13 at `O:\appsnotonc\sentinel\uv\python\cpython-3.13.17-windows-x86_64-none\python.exe`.

- **Game and backend:** `python -I tools\portwin32.py [--debug|--release]`
  - Copies `port\win32\` into the exhumed tree, runs `tools\buildview.cmd` (renderers and bridge), then Exhume's
    `portbuild.py` on `underworld-exhumed\uw1\exhume.toml`.
  - `--backend sdl3` builds the SDL3 backend to its own program, `build\port-sdl3` (with `UWR_AREA` set, the code names it `port-sdl3-<name>`); the renderers still go to `build\areas\<name>`. `--original` builds the original's plain display.
  - Result: `underworld-exhumed\uw1\build\port\uw1port.exe`, with the DLLs beside it.
- **UWR_AREA:** set `UWR_AREA=<name>` to build in a private area. The program goes to `build\port-<name>` and the
  renderers to `build\areas\<name>`, so sessions never collide. Unset, the shared `build\port` is used.
- **Renderers alone:** `tools\buildview.cmd` makes `uwview.dll`, `uwview_vk.dll`, `uw_nvngx_port.dll` (copied beside the exe as `uw_nvngx.dll`), and
  compiles the shaders. It checks `render_vk_cpu.h` against `render.c` and the shader struct layouts first.
- **Release folder:** `python -I tools\makeflat.py` assembles `dist\uwr` (the original's files untouched, `uwr.exe`,
  the DLLs, and our own files under `uwr\`). `tools\makedist.py` packs the older `dist\UW1`.
- **Log:** `uwr.log` is in `<home>\logs\` (home is the game's user folder, `dist\uwr\uwr\home` in the release folder).
  It is rotated to `uwr.1.log` once past 5 MB. Not written for off-screen runs (`UWPORT_OFFSCREEN`) or when the
  program's output is already a file or pipe.
Lines written through rlog carry time, level and tag; the Esc menu's Log detail row (Game tab, `uw1port.cfg` `log_detail`) or the environment variable `UWR_LOGLEVEL` sets how much is kept (the variable wins at start). A few lines stay plain (`in:`, `fx:`, `uwview:`, `menu:`) because check scripts match them by their start.
