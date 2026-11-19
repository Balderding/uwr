# Editor MCP server, slice 1 (tiles) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** a stdio MCP server in Karl's editor solution that opens a UW game folder and reads, edits, checks and saves level tiles.

**Architecture:** `src\UwEditor.Mcp` (console, net10.0) holds `McpEditor` (plain C#, all the logic, returns text) and `EditorTools` (thin `[McpServerToolType]` wrappers, `McpEditor` injected as a singleton). Tests call `McpEditor` directly on a temp copy of the game, plus one stdio smoke test.

**Tech Stack:** .NET 10 SDK at `O:\appsnotonc\dotnet10`, `ModelContextProtocol` 2.4.0 + `Microsoft.Extensions.Hosting`, xunit 2.9.3 (as his tests).

**Spec:** `O:\uw\uo-new\docs\superpowers\specs\2026-11-18-editor-mcp-server-design.md` (background: `2026-11-18-editor-mcp-test-level.md`).

## Global Constraints
- Repo `O:\UltimaUnderworldEditorAvalonia`, branch `uwr-fog`. Pull Karl first: `python -I O:\uw\uo-new\tools\editor_update.py`.
- Only new files; his files change only in `UltimaUnderworldEditorAvalonia.sln` (two projects added).
- His code style: `Nullable disable`, `ImplicitUsings disable`, braces style of `UwEditor.Core`, plain-words doc comments.
- Every dotnet call with `DOTNET_CLI_HOME=O:\appsnotonc\dotnet10\home`, `NUGET_PACKAGES=O:\appsnotonc\dotnet10\home\.nuget\packages`, `DOTNET_CLI_TELEMETRY_OPTOUT=1`. Run built programs via `dotnet.exe <dll>` (ASR blocks new exes).
- Nothing on C:; temp files in `O:\uw\uo-new\work\testlevel`. Never delete anything without asking.
- No real name in any file or commit; author "balderding".
- Not registered with Claude Code; nothing pushed or sent.
- Levels are numbered 1..`Profile.LevelSlots`; tiles 0..63 each way.

## Review Focus
1. `--only` with a different spelling of the same folder (trailing slash, case, `/` vs `\`): accepted; a subfolder or a sibling: refused.
2. A tool called before `open`: a plain "Open a game folder first." answer, not an exception trace.
3. Coordinates or values out of range (x 64, height 16, type 10, texture beyond the level's list): the editor's own message returned, nothing changed.
4. `fill` with corners given in either order: same rectangle; one bad value: nothing in the rectangle changed (one undo step).
5. `save` of a level that fails the game's check: refused with the editor's message, LEV.ARK unchanged on disk.

---

### Task 1: Test copy of the game and the project skeleton

**Files:**
- Create: `O:\uw\uo-new\work\testlevel\game\` (copy of `O:\uw\` `DATA`, `CRIT`, `SOUND`, `CUTS`, `UW.EXE`) and `O:\uw\uo-new\work\testlevel\gamedata\UW\` (a second copy, for the tests' `UW_GAMEDATA`)
- Create: `src\UwEditor.Mcp\UwEditor.Mcp.csproj`, `src\UwEditor.Mcp\Program.cs`
- Create: `tests\UwEditor.Mcp.Tests\UwEditor.Mcp.Tests.csproj`, `tests\UwEditor.Mcp.Tests\GameCopy.cs`
- Create: `O:\uw\uo-new\tools\editor_mcp.cmd` (build + run through dotnet.exe with the env above)
- Modify: `UltimaUnderworldEditorAvalonia.sln` (`dotnet sln add` both)

**Interfaces:**
- Produces: `GameCopy.Make()` -> `string` (a fresh temp copy of `UW_GAMEDATA\UW` under `work\testlevel\tmp\<guid>`, or null when there is no game data: the test then returns, as his `GameData.Game` does).

- [ ] Step 1: copy the two game folders (cp -r, nothing deleted).
- [ ] Step 2: csproj: `OutputType Exe`, net10.0, refs `../UwEditor.Core/UwEditor.Core.csproj`, packages `ModelContextProtocol` 2.4.0 and `Microsoft.Extensions.Hosting` (newest 10.x at least two weeks old). `InternalsVisibleTo UwEditor.Mcp.Tests`. Program.cs: host with `AddMcpServer().WithStdioServerTransport().WithToolsFromAssembly()`, logging to stderr only.
- [ ] Step 3: build: `dotnet build src\UwEditor.Mcp -c Release` -> 0 errors.

### Task 2: `McpEditor.Open` and the `--only` guard

**Files:** Create `src\UwEditor.Mcp\McpEditor.cs`, `src\UwEditor.Mcp\EditorTools.cs`; Test `tests\UwEditor.Mcp.Tests\OpenTests.cs`

**Interfaces:**
- Produces: `sealed class McpEditor { McpEditor(string onlyFolder); string Open(string folder); GameSession Session; LevelEdits Edits; }`; Program reads `--only <folder>` into it. Every later method first calls `Require()` which throws `McpEditorException("Open a game folder first.")`; `EditorTools` turns `McpEditorException`, `ArgumentException`, `InvalidOperationException` into the message as the tool's text.
- `Open` answer: game kind, folder, then one line a slot: `level 1: in use` / `level 9: empty` / `level N: damaged (why)`.

- [ ] Step 1: tests:
  - `Open_lists_levels`: `Open(copy)` contains `"level 1: in use"`, `"UW1"`.
  - `Only_refuses_other_folders`: `new McpEditor(copy).Open(Path.GetDirectoryName(copy))` throws `McpEditorException` whose message contains `"only"`.
  - `Only_accepts_other_spellings`: `copy + "\\"`, upper-cased, `/` slashes: all open.
  - `Tools_before_open_say_so`: `new McpEditor(null).LevelMap(1)` throws, message `"Open a game folder first."`.
- [ ] Step 2: run `dotnet test tests\UwEditor.Mcp.Tests` -> FAIL (no McpEditor).
- [ ] Step 3: implement; compare folders with `Path.GetFullPath(...).TrimEnd('\\','/')` case-insensitive.
- [ ] Step 4: run tests -> PASS.
- [ ] Step 5: commit `src\UwEditor.Mcp tests\UwEditor.Mcp.Tests UltimaUnderworldEditorAvalonia.sln` ("MCP server: project, open, --only guard").

### Task 3: `level_map`, `tile_get`, `tile_set`

**Files:** Modify `McpEditor.cs`, `EditorTools.cs`; Test `tests\UwEditor.Mcp.Tests\TileTests.cs`

**Interfaces:**
- `string LevelMap(int level)`: header line, then 64 rows y=0 at the top as the editor's 2D map draws (check `MapPicture` and match it; say which in the header), each row 64 chars of type: `#` solid, `.` open, `1`-`4` diagonals SE SW NE NW, `n s e w` slopes rising N S E W; a blank line; then 64 rows of floor height as hex `0`-`f` (`#` for solid).
- `string TileGet(int level, int x, int y)`: `Type`, `Floor height`, `Light`, `Floor texture`, `No magic`, `Door`, `Wall texture` one a line as `name: value`, plus the type's name (`TileInfo.TypeName`), the texture numbers (`Model.FloorMap[i]`, `Model.WallMap[i]`) and the object count on the tile.
- `string TileSet(int level, int x, int y, string field, int value)`: via `LevelEditor.SetTileField` (for `Floor height`, via `RaiseFloor(x, y, value - current)` so objects follow); answers the new `TileGet`. x/y outside 0-63 -> `ArgumentOutOfRangeException("x", "Tiles are 0-63.")`.

- [ ] Step 1: tests: `Map_has_64_rows_of_64` (both grids); `Map_matches_model` (for every tile, the char equals the model's type); `Tile_set_changes_one_field` (set `Floor texture` 2 on an open tile, `TileGet` shows 2, other fields as before, `Edits.HasChanges`); `Tile_set_refuses_out_of_range` (height 16, type 10, x 64: throws, `Edits.HasChanges` false); `Raise_carries_objects` (a tile with an object on it: its Z goes up 8 a step).
- [ ] Step 2: run -> FAIL.  Step 3: implement.  Step 4: run -> PASS.
- [ ] Step 5: commit ("MCP server: level map and tiles").

### Task 4: `fill`

**Files:** Modify `McpEditor.cs`, `EditorTools.cs`; Test `tests\UwEditor.Mcp.Tests\FillTests.cs`

**Interfaces:** `string Fill(int level, int x1, int y1, int x2, int y2, string type, int? height, int? floorTexture, int? wallTexture)`; `type` is a map char from Task 3 or a name (`solid`, `open`); all in one `LevelEditor.Batch("Fill (x1,y1)-(x2,y2)", ...)`; values checked before the batch starts so a bad one changes nothing. Answer: count of tiles changed and the rectangle.

- [ ] Step 1: tests: `Fill_makes_a_room` (open 5x4 at height 3: all 20 tiles open, height 3); `Fill_corners_any_order` (same tiles for (10,10)-(14,13) and (14,13)-(10,10)); `Fill_bad_value_changes_nothing` (wall texture 99: throws, the tiles as before); `Fill_is_one_undo` (after a fill, `Edits.For(l).UndoLabels.Count` went up by 1).
- [ ] Step 2: FAIL. Step 3: implement. Step 4: PASS. Step 5: commit ("MCP server: fill").

### Task 5: `check` and `save`

**Files:** Modify `McpEditor.cs`, `EditorTools.cs`; Test `tests\UwEditor.Mcp.Tests\SaveTests.cs`

**Interfaces:** `string Check(int level)`: `LevelEditor.Check(out ok)` -> `"ok"` or `"fails: " + report`. `string Save()`: `LevelEdits.Save()` -> `"saved levels 1, 2"` / `"nothing to save"`; its `InvalidOperationException` passes through as the answer.

- [ ] Step 1: tests: `Save_writes_and_reopens` (fill, save, new `McpEditor` opens the copy: the tiles are as filled, a `LEV.ARK.BKP` exists); `Save_nothing` ("nothing to save"); `Check_shipped_level_ok` (level 1 "ok").
- [ ] Step 2: FAIL. Step 3: implement. Step 4: PASS. Step 5: commit ("MCP server: check and save").

### Task 6: stdio smoke test and README

**Files:** Test `tests\UwEditor.Mcp.Tests\StdioTests.cs`; Create `src\UwEditor.Mcp\README.md`; Modify `O:\uw\uo-new\tools\editor_update.py` (run `UwEditor.Mcp.Tests` too, if it does not already)

- [ ] Step 1: test `Server_lists_its_tools`: start `dotnet <UwEditor.Mcp.dll> --only <copy>`, write `initialize`, `notifications/initialized`, `tools/list` as JSON-RPC lines; the reply names `open`, `level_map`, `tile_get`, `tile_set`, `fill`, `check`, `save`; then `tools/call open` returns the level list. Process killed at the end, 20 s timeout.
- [ ] Step 2: run -> PASS (code exists already; a FAIL here is a wiring bug).
- [ ] Step 3: README: what it is, the tools, `--only`, how to register it with an MCP client (text only, not done for us).
- [ ] Step 4: whole `dotnet test tests\UwEditor.Mcp.Tests`; grep output for ` FAIL` and check the exit code.
- [ ] Step 5: real-name scan (`git grep -i` the name, from the vault-free check used for uwr pushes) and commit ("MCP server: stdio test, README").
