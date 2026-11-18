# The editor as an MCP server, and a test level (plan, 2026-11-18)

The user, 2026-11-18: "the plan is to make it into a mcp server so you can make maps, and we will use that to
make a test level, so you can test everything without teleporting". The editor is Karl Clinckspoor's Underworld
Editor (MIT), our copy at `O:\UltimaUnderworldEditorAvalonia`, kept **on O: only** (no fork on GitHub). The
work on it starts at a later date; this is the plan for then.

## Keeping up with Karl, and giving it back (the user, later on 2026-11-18)

"When he updates his editor we pull that into our codebase"; "we will build it into the codebase and share it
with him"; "make sure his version doesn't have a mcp server yet".

- Checked 2026-11-18 against his newest (b8f5d52, v0.1.53-beta): **no MCP server**, no MCP / ModelContextProtocol
  / JSON-RPC code, one branch (`main`), no issue or pull request about MCP. Check again before starting.
- Our copy: `main` = his, untouched; `uwr-fog` = ours on top (fog window committed 55a2fa9, his v0.1.53 merged in).
- **`tools\editor_update.py`** (in uo-new) pulls his newest: fetch, merge into `uwr-fog`, build, our tests
  (`UwrFog`, `Mcp`). It stops and lists the files on a conflict; it never pushes.
- So that his updates merge cleanly and he can take ours: our work is in **new files** (new projects, partial
  classes like `MainWindow.UwrFog.cs`); his files get one-line hooks at most (today: one menu item, one
  `AddUwrFogMarkers(lines);` call).
- The MCP server goes **inside his solution** as its own project, `src\UwEditor.Mcp` (with
  `tests\UwEditor.Mcp.Tests`), not in uo-new, so it can be handed to him as it is. Written for any game folder
  he or his users open (UW1 and UW2, his code style, his README section), with the uwr-only parts (`fog_set`,
  the test copy guard) kept apart so he can leave them out.
- **Sharing with him** needs the user's yes first, and one of: a pull request (needs a fork of his repo on the
  user's GitHub, which is public), or a patch / bundle (`git format-patch main..uwr-fog`) sent to him another
  way, or an issue on his repo offering it. Scan for the user's real name before anything leaves O:.

## Why

Today every check moves the player with `cheat goto` (the game's own teleport) and turns the view with
`cheat turn`, into whatever the shipped levels happen to have there. A picture often faces a wall, and a feature
can only be checked where the original map has it (water at level 1 13,24, a campfire at 6,42). A level of our own,
with each feature laid out where a check expects it, and saved games that start the player at each test
station, make the checks repeatable and quick.

## What is there already (the editor's core, `src\UwEditor.Core`, usable without its window: `tools\uwcheck` does)

- `GameSession.Open(folder)`: a game folder, its levels, names, strings, graphics.
- `LevelEdits` / `LevelEditor` (Editing\LevelEditor.cs): tile fields (`SetTileField`, `RaiseFloor`, light bit),
  objects (`AddObject` from an `ObjectSpec`, `MoveObject`, `DeleteObject`, `PutInside`, `Copy` / `Paste`),
  undo, `Check` (the game's own consistency checks), `Save` (the `.ORIG` / `.BKP` copies).
- `NewLevels` (Make this slot a level, Clear the level), `TrapRecipes` (teleports, triggers made in one step),
  `Triggers`, `Keys`, `Schedules`.
- `SpotTest` / `ExhumedCheck`: saved games that hold a tester on chosen squares, run in a port.
- `UwrFog` (ours, branch `uwr-fog`): each level's fog and smoke for uwr, `DATA\UWRFOG.TXT`.
- `MapPicture`, `LevelScene`: a level drawn as a picture (2D map, 3D view) without a window.

## Stage 1: the MCP server (`src\UwEditor.Mcp` in the editor's own solution)

A C# console program over stdio on the official MCP C# SDK (the `ModelContextProtocol` NuGet package; check
its current version and API before writing), referencing `UwEditor.Core` as a project in his solution. Run
through `O:\appsnotonc\dotnet10\dotnet.exe` (Defender's ASR rule blocks a new exe). Registered with Claude Code
only after the user says so (the MCP servers are off to save tokens: [[mcp-servers-disabled]]).

For our use it only ever works on **a copy of the game** in `O:\uw\uo-new\work\testlevel\game` (never the
user's `dist\uwr`, never the shipped originals): a `--only <folder>` start option makes `open` refuse any other
folder (for Karl's users it opens any folder, saving with his `.ORIG` / `.BKP` copies as the window does).

Tools (each small, each answering in plain words and numbers):

| Tool | What it does |
|---|---|
| `open` | opens the test copy; lists the levels and which are in use |
| `level_map` | a level as text: a 64x64 grid of tile types and floor heights (and a PNG via MapPicture) |
| `tile_get` / `tile_set` | one tile's fields: type, floor height, wall and floor textures, light |
| `fill` | a rectangle of tiles at once (a room: open floor, a height, its textures; or solid rock) |
| `object_list` / `object_add` / `object_move` / `object_delete` | the objects, by item name or number, with their fields |
| `trap_recipe` | the editor's trap recipes (a teleport, a lever and its door, a trigger) |
| `fog_set` | `UwrFog`: a level's haze, smoke, draught, temperature, spots |
| `check` | the game's own checks on the level; refuses to save a level the game could not load |
| `save` | writes the levels (the editor's safe write) |
| `picture` | the level seen from a square and a heading (LevelScene), to look before running the game |
| `station_save` | a saved game with the player at a station (square, height, heading), via SpotTest's saves |

## Stage 2: the test level

Made with the server, in a free slot of the copy (or level 1 of the copy replaced, if the slot route does not
work with uwr). One station a feature, each a small room joined by corridors, laid out so a check can face it:

- **Water room**: still water with a cold floor around it, a step down beside it (fog running down steps).
- **Lava room**: lava with a high ceiling (steam rising).
- **Fire room**: a campfire under a low ceiling, a torch on a wall, a lantern (smoke rising, gathering, puffing).
- **Draught corridor**: a long passage with fog spots and the level's own draught (wisps stretching).
- **Doors and creatures**: a door that a lever opens, a few slow creatures (pushing the air).
- **Light**: a dark room with one torch and pillars (shafts through the fog).
- **Sound**: rooms of different sizes and materials (the acoustics), a stair to level 2 (the level change gate).
- **Mechanics**: slopes, a pit, a ledge to jump, a locked door and its key (movement, the rules).

Its `DATA\UWRFOG.TXT` block sets the fog spots and draught for the stations.

## Stage 3: the checks use it

- `station_save` makes one saved game a station (the game's four slots a round, as SpotTest does).
- The hidden checks (`work\soundfix\vol\vprobe*.py`, `zone.py`) load a station's save with `--data` on the test
  copy instead of `cheat goto` / `cheat turn`: the player starts there, the right way round, every time.
- A picture a station, beside the same station with the feature's Esc row off (the before/after pairs).

## Then: the fog in the editor's 3D view (part 2 of 2026-11-18's ask)

The editor draws with OpenGL 3.3 / GLES 3.0 (`src\UwEditor.Render\GlRenderer.cs`, Shaders.cs): no compute
shaders. Two ways, to decide then:

1. **A light preview** (recommended first): the level's fog and smoke as uwr would place them, drawn without
   moving air: a ray-march in the fragment shader through a small 3D texture the editor fills on the CPU from
   the level's water, lava, fires and spots (haze by the water's height, smoke columns over fires, the spots).
   Shows where the fog will be, not how it moves.
2. **The real simulation**: the air stepped on the CPU (the same steps as volume.hlsl: advect, buoyancy,
   pressure) at a coarse grid, or with GL 4.3 compute where the driver has it. Shows how it moves; much more work.

## Open questions for the user (when the work starts)

- Test level in a free slot, or level 1 of the test copy replaced?
- Register the MCP server with Claude Code (it costs tokens each session while on)?
- The preview (1) or the real simulation (2) in the editor's view?
- How to hand it to Karl: a pull request from a fork, a patch, or an issue first asking if he wants it?
