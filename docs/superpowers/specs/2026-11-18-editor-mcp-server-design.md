# Editor MCP server: design (2026-11-18, approved by the user)

Intent: Karl's Underworld Editor as an MCP server so Claude can make maps; first use is a test level with
stations so checks need no teleport. Built into his solution so it can be shared with him. Full background:
`docs\superpowers\plans\2026-11-18-editor-mcp-test-level.md`.

## Approaches
1. **Own project in his solution over the official C# MCP SDK (picked)**: `src\UwEditor.Mcp`, stdio,
   `ModelContextProtocol` 2.4.0, references `UwEditor.Core`. Shareable as is, merges with his updates.
2. A Python server driving uwcheck-like dll calls: not shareable with Karl, two languages. Dropped.
3. MCP inside the Avalonia app (live window): touches his files, needs the window. Dropped.

## Decided
- Console program, stdio, net10.0, packages in `O:\appsnotonc\dotnet10\home\.nuget\packages`.
- `--only <folder>`: `open` refuses any other folder. Our folder: `O:\uw\uo-new\work\testlevel\game`, a copy
  of the original UW1 files.
- His files: only the `.sln` gets the two new projects.
- Slices: (1) open, level_map, tile_get, tile_set, fill, check, save + tests; (2) objects, trap_recipe,
  picture; (3) fog_set (uwr-only, kept apart), station_save.
- Not now: Claude Code registration, sending to Karl, commits before a slice works.

## Left to the builder
Tool argument shapes, text formats of the answers, test layout.
