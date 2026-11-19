# Editor MCP server, slice 2 + watch mode: design (2026-11-18)

The user: "Start building the next part. Also there should be a mode that lets me watch you work while I
am in the app." Background: `2026-11-18-editor-mcp-server-design.md`, slice 1 done (e55076c..1dfa1d4).

## Watch mode
**Approaches** (asked 2026-11-18, the user picked A):
- **A. The server inside the app (picked).** A menu item starts it on the game open in the app; each change
  shows the moment it is made, goes in the app's undo list; the user can undo or stop it.
- B. The app reloads the level on each save (only saves seen, clashes with unsaved edits). Dropped.
- C. The separate server forwards calls into the app over a pipe. Dropped.

**The view stays where the user put it** (asked: "Stay where you put it"); the level redraws in place
(the app already redraws on `LevelEdits.Changed`).

**My calls (the user can correct):**
- Transport: the app listens on a **named pipe** (`uweditor-mcp`, this PC only), not HTTP: Karl's release is
  one self-contained file, and HTTP would add ASP.NET Core to it. Claude Code still runs one stdio command:
  `UwEditor.Mcp.Server --app` passes stdin/stdout through to the pipe. Without the app running it says so
  on stderr and stops.
- Attached, the tools work on the app's own `vm.Session` / `vm.Edits` (following the game the app has open),
  each call run on the UI thread. `open` refuses ("open games in the app"). `save` writes as Ctrl+S does
  for levels.
- A small window (Tools menu, "Let Claude work here (MCP)…"): the game folder it works on, connected or not,
  a running list of what each call did, Stop. Closing it stops the server.
- Karl's files get one menu line in `MainWindow.axaml` and one project reference in the app's csproj.
- Projects: `src\UwEditor.Mcp` becomes a library (McpEditor, EditorTools, the pipe host);
  `src\UwEditor.Mcp.Server` is the program (stdio, `--only`, `--app`).

## Slice 2 tools (from the plan)
- `item_find` (names to item numbers), `object_list` (a tile or the level), `object_add`, `object_move`,
  `object_delete` (via LevelEditor AddObject / MoveObject / DeleteObject, ObjectSpec.Default).
- Recipes, one tool each over TrapRecipes: `recipe_teleport`, `recipe_lever_door`, `recipe_plate`
  (message / damage / door), `recipe_spawner`.
- `picture`: the 2D map (MapPicture) as a PNG image, whole level or a rectangle. **The 3D view picture is
  left for later** (LevelScene needs a GL context); in watch mode the user sees the 3D view anyway.

## Not now
Camera following; fog_set and station_save (slice 3); registering with Claude Code (asked at the end).
