# Editor MCP slice 2 + watch mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** object and trap-recipe tools, a map picture, and a watch mode where the tools work inside the running editor app.

**Architecture:** `UwEditor.Mcp` (library) holds McpEditor (now able to attach to an app's session and run calls through a runner), EditorTools, and `PipeHost` (an MCP server per pipe connection over `StreamServerTransport`). `UwEditor.Mcp.Server` (exe) runs stdio or relays to the pipe. The app's partial `MainWindow.Mcp.cs` + `McpWindow.cs` start PipeHost on the app's game.

**Tech Stack:** as slice 1; Avalonia 12 (the app's).

**Spec:** `O:\uw\uo-new\docs\superpowers\specs\2026-11-18-editor-mcp-watch-design.md`

## Global Constraints
- As slice 1 (env, O: only, no deletes, name scan, no push). Karl's files: only `MainWindow.axaml` (one MenuItem), `UwEditor.App.csproj` (one ProjectReference), `.sln`.
- Pipe name `uweditor-mcp`; local only (`PipeOptions.CurrentUserOnly`).
- Tests: no Avalonia window tests for the watch window (the user checks the app); the pipe host is tested headless.

## Review Focus
1. The app switches to another game, or closes it, while Claude is attached: tools follow the app's current game, or say "No game is open in the app."
2. A second client connects while one is attached: refused or queued, never two servers on one LevelEdits at once.
3. The relay starts with no app running: a plain stderr line and exit code 1 within a few seconds, no hang.
4. A tool call that throws inside the UI-thread runner: the refusal text comes back, the app keeps running.
5. `object_add` by a name that matches several items: lists them and adds nothing.

---

### Task 1: Split library and program
**Files:** Create `src\UwEditor.Mcp.Server\UwEditor.Mcp.Server.csproj` (Exe, refs UwEditor.Mcp), move `Program.cs` there (namespace `UwEditor.Mcp.Server`); `src\UwEditor.Mcp\UwEditor.Mcp.csproj` becomes a library; tests ref both; `.sln` add; `editor_mcp.cmd` runs `UwEditor.Mcp.Server.dll`; README paths.
- [ ] Move, build, run the suite: 30/30 (StdioTests start `UwEditor.Mcp.Server.dll`). Commit.

### Task 2: Attached McpEditor
**Interfaces:** `McpEditor.Attached(Func<GameSession> session, Func<LevelEdits> edits, Func<Func<string>, string> runner)`; `string Run(Func<string> f)` (EditorTools.Answer calls `ed.Run`); `Open` attached -> `McpEditorException("Open games in the app; this server works on the game open there.")`; `Require()` attached with no game -> `"No game is open in the app."`.
- [ ] Tests (`AttachTests.cs`): tools use the provided edits (a fill shows in the provider's LevelEdits); provider switching to another session is followed; null session -> the message; Open refused; the runner is used for every call (counting runner).
- [ ] RED, implement, GREEN, commit.

### Task 3: Objects
**Interfaces:** `ItemFind(string text)` (number and name lines, max 40; exact number `0x1A2` or `418` also); `ObjectList(int level, int? x, int? y)` (index, item number, name, tile, fine x/y, z, quality, owner; tile objects with their contents indented); `ObjectAdd(int level, int x, int y, string item, int? quality, int? owner, int? z, int? heading, int? fineX, int? fineY)` -> new index (via `ObjectSpec.Default`, `LevelEditor.AddObject`); `ObjectMove(int level, int index, int x, int y, int? fineX, int? fineY)`; `ObjectDelete(int level, int index)`. Item text resolves by exact number, else exact name (case-insensitive), else a unique substring; several -> `ArgumentException` listing them.
- [ ] Tests (`ObjectTests.cs`): find "door" lists doors; add by name then list shows it on the tile; ambiguous name adds nothing; move changes tile; delete removes; solid tile refused (the editor's own message).
- [ ] RED, implement + tools, GREEN, commit.

### Task 4: Recipes
**Interfaces:** `RecipeTeleport(level, fromX, fromY, toLevel, toX, toY, bool back)`; `RecipeLeverDoor(level, doorIndex, wallX, wallY, string side)`; `RecipePlate(level, x, y, string kind /*message|damage|door*/, string text, int? damage, int? doorIndex)`; `RecipeSpawner(level, x, y, string creature, double chance)`. Each answers `RecipeResult.Summary` plus the made indexes; wraps the core's `InvalidOperationException`.
- [ ] Tests (`RecipeTests.cs`): a teleport makes a move trigger + teleport trap on the square and the level still passes check; lever/door with a door in level 1 makes a lever; plate message adds a text trap; spawner adds a create trap.
- [ ] RED, implement + tools, GREEN, commit.

### Task 5: Picture
**Interfaces:** `byte[] PicturePng(int level, int? x1, int? y1, int? x2, int? y2, int cell = 8)` (MapPicture.Render cropped); tool `picture` returns `ImageContentBlock.FromBytes(png, "image/png")`.
- [ ] Tests: PNG signature; a 10x10 crop at cell 8 is 80x80 (read IHDR); refusals as text.
- [ ] RED, implement, GREEN, commit.

### Task 6: Pipe host and relay
**Interfaces:** `PipeHost.Start(McpEditor ed, string pipeName = "uweditor-mcp")` -> `PipeHost` (IDisposable; `event Action<string> Called` with "tool: answer's first line"; `bool Connected`); one connection at a time (a second waits until the first ends); per connection `McpServer.Create(new StreamServerTransport(pipe, pipe), options with tools from `McpServerTool.Create(method, editorTools)`)`. Relay: `UwEditor.Mcp.Server --app` connects (2 s timeout) and copies both ways; no app -> stderr "The editor app is not running Let Claude work here." exit 1.
- [ ] Tests (`PipeTests.cs`): PipeHost on a test pipe name, the relay process started with `--app --pipe <name>`: initialize, tools/list names, a fill call changes the hosted McpEditor's edits; relay with no host exits 1 in under 5 s.
- [ ] RED, implement, GREEN, commit.

### Task 7: The app's window
**Files:** Create `src\UwEditor.App\MainWindow.Mcp.cs` (handler `OnMcp`), `src\UwEditor.App\McpWindow.cs` (status, folder, call list, Stop); Modify `MainWindow.axaml` (one MenuItem under the uwr fog item), `UwEditor.App.csproj` (ProjectReference to UwEditor.Mcp).
- Runner: `f => Dispatcher.UIThread.Invoke(f)`; session/edits from `ViewModel.Session` / `ViewModel.Edits`; after a change the app redraws by itself (Edits.Changed).
- [ ] Build the app (0 errors), launch it hidden-check per the user's rules is NOT done (the user tests the app). Commit.
- [ ] README: watch mode, `.mcp.json` example with `--app`.
