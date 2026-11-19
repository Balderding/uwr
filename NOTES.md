# uo-new: Ultima Underworld rebuilt in C (clang)

Everything for this project lives in this folder, so it can be moved anywhere as one piece.

| Folder | What |
|---|---|
| game\ | exact copy of the user's original files (O:\uw, 297 files, md5-checked 2026-11-07). Never modified. |
| tools\ | Python reference decoders (`python -I tools\assets.py check` / `export`). Paths are relative. |
| extracted\ | PNGs etc. written by the Python tools, for checking by eye. |
| (later) src\, build\, data\ | the C engine, its build output, and the loose files it reads. |

## Rules agreed with the user
- No DOSBox. Rebuild in plain C with LLVM clang (O:\appsnotonc\llvm).
- Everything goes in uo-new. Anything needed from the original is copied into uo-new\game.
- **The engine reads no archives.** Everything it uses is a loose extracted file (pictures, sounds, music, levels, text).
  **If an extracted file is missing, the engine extracts it itself** from the original files in game\
  (so the extractor is C code inside the engine; the Python tools are the reference it is checked against).
- Goals: mouse look, rebindable keys, any resolution, better sound, D3D12 renderer with DLSS and DLSS 5
  (same approach as shock-engine).

## UW.EXE
547,248 bytes, md5 8afc3c29ef5667a3ab6667e8cbc8d63a, dated 16 Jun 1993. 16-bit DOS MZ.
Header: Borland TLINK 3.0 signature `01 00 FB 30 "jr"` at 0x1C; entry CS:IP = 0EC5:0000; SS:SP = 6248:0080;
3065 relocations at 0x3E; header 800 paragraphs (0x3200); load image 812 pages, so ~130 KB of overlay
data follows the load image (probably Borland VROOMM overlays; unconfirmed).

## Data formats (verified on the user's copy by tools\assets.py check)
- **PALS.DAT** 6144 bytes = 8 palettes x 256 x RGB, VGA 6-bit. Palette 0 is the game palette.
- **.TR textures**: u8 2, u8 size, u16 count, u32 offsets[count], then size*size palette indices each.
  W64.TR 210 walls 64x64, W16.TR 210 walls 16x16 (distant), F32.TR 52 floors 32x32, F16.TR 52 floors 16x16.
- **LEV.ARK**: u16 count (135), u32 offsets (0 = empty). Blocks in groups of 9 (one per level):
  0-8 level data (31,752 bytes each), 9-17 384 bytes (animation overlays, unconfirmed),
  18-26 texture mapping (122 bytes), 27+ empty in a fresh game (automap / notes are made in saves).
- **Level data**: 64x64 tiles x 2 u16, row 0 = south.
  w0: bits 0-3 type (0 solid, 1 open, 2-5 diagonal open SE/SW/NE/NW, 6-9 slope N/S/E/W), 4-7 floor height,
  10-13 floor texture slot, 14 no-magic, 15 door. w1: bits 0-5 wall texture slot, 6-15 first object.
  At 0x4000: 1024 objects, 256 mobile x 27 bytes then 768 static x 8 bytes. General part (4 u16):
  w0 0-8 item id, 9-15 flags; w1 0-6 z, 7-9 heading, 10-12 y in tile, 13-15 x in tile;
  w2 0-5 quality, 6-15 next; w3 0-5 owner, 6-15 quantity/link. Items 64-127 are creatures.
  The rest of the block after the objects (free lists) is not decoded yet.
- **Texture mapping**: 48 u16 wall texture numbers (into W64), 10 u16 floor numbers (into F32), 6 u8 doors.
- Check (2026-11-07): all 9 levels decode; tile types 0-9 only; all texture slots in range; no object
  chain loops; 220-597 objects per level. extracted\levels\level1.png shows the Abyss level 1 correctly
  (ankh room, rivers, checkerboard floors, 10 doors).

## Engine (src\, built by build.cmd with clang into build\)
- `play.cmd` (or build\uw.exe) plays. `run-tests.cmd` builds, runs build\check.exe, draws pictures with no window
  into build\shots. Exes run from this folder (Defender ASR exclusion for O:\uw\uo-new added by the user 2026-11-07).
- **data\** holds every file the engine reads (loose, no archives). Missing files are written on start by
  src\extract.c from game\ (only missing ones: edited files are never overwritten). check.exe proves the C
  extractor = tools\loose.py byte for byte, level texts read back to LEV.ARK exactly, PNGs read back to the
  original palette indices, and the PNG reader handles compressed PNGs (edited textures from any program load).
  Textures of any size load (scaled to their atlas cell, nearest texel).
- data\settings.txt (resolution, fullscreen borderless, fov, mouse, light radius, ambient, gamma, start level) and
  data\keys.txt (rebindable, up to 4 keys/mouse buttons per action) are written with defaults on first run.
- src: base.c (arena, files), png.c (paths, PNG write/read), extract.c, level.c (level text -> tiles/objects,
  triangles + 1024 atlas: walls 64px from the top, floors 32px from y 896), render.c (D3D12: colour + motion +
  depth targets ready for DLSS, the player's light fading to dark), main.c (window, raw mouse look, WASD, run,
  jump, collision with walls/steps/slopes, F2/F3 level change, F4 fly, --shot headless pictures), check.c.
- Geometry, confirmed by eye 2026-11-07: one height step = 1/4 tile, ceiling at 16 steps, each open tile draws the
  walls around it with ITS OWN wall texture, textures hang from the ceiling at 64 texels a tile: the entrance's
  double doors (south wall of the start room, level 1 tile 32,1) fill floor-to-ceiling exactly.
  Still unconfirmed: slope direction (rises toward the side it is named after), ceiling texture = floor slot 9.
  Level 9 (the Void) centre: floor F32 #26 and wall W64 #64 are pure black in the original (correct, not a bug).

## DLSS and DLSS 5 (2026-11-07)
- dlss\ holds nvngx_dlss.dll (from the DLSS SDK, O:\appsnotonc\dlss-sdk), nvngx_dlssnr.dll (the user's DLSS 5 neural
  DLL from O:\appsnotonc\DLSS 5 MANAGER\mod files\dlss 5; md5-checked copies), the SDK headers (dlss\include) and
  its licence. Not in git (the DLLs are 225 MB).
- src\ngx.c + src\nrbridge.c from shock-engine (NGX from plain C; the neural DLL only accepts a caller whose path
  contains "nvngx.dll", so its calls go through build\uw_nvngx.dll, built with -fno-optimize-sibling-calls).
- render.c: all targets at output size; with DLSS on the scene is drawn into the top-left render-size part with
  Halton jitter, DLSS (subrect) upscales into dlss_out, DLSS 5 runs on that into nr_out. Scene in linear light x
  exposure 4 (as shock-engine measured), the present pass undoes it, so DLSS off = the same picture as before.
- **' key (keys.txt "dlss") switches DLSS + DLSS 5 on/off any frame**; the window title says which is on.
  settings.txt: dlss, dlss_quality (dlaa/quality/balanced/performance), dlss5, dlss5_style (default 2 Cinematic,
  the user's choice in shock-engine), intensity/structure/tone, saturation 1.15, exposure 4.
- settings.txt / keys.txt: settings or actions a file lacks are appended with their defaults (never rewritten).
- Measured (level 1 entrance, 1600x900, quality = render 1067x600): DLSS vs native 39.7 dB, DLSS 5 vs DLSS 31.5 dB
  (floor gets a wet sheen, doors look metallic), brightness 49.8 / 49.9 / 52.1. Switched off mid-run = native
  exactly (99 dB); switched back on = fresh DLSS 5 at 50 dB; 0 neural frames failed. Headless: --dlss=0/1/2,
  --frames=N, --toggle-every=N.

## Relief and light (2026-11-07)
- **Normal maps** (src\normals.c): made from each texture as the artists drew it: mortar = pixels clearly darker
  than the 9x9 round them (< 0.62 x local, < 0.45), in lines (patches under max(6, n/150) pixels dropped: specks
  are not mortar); stone height = distance to mortar over a 4.5-texel bevel (cushion: 1-(1-t)^3), softened, plus
  the painted speckle; made at 4x (256 per wall, 128 per floor), relief 2.2 texels deep. RGBA: normal (OpenGL,
  green up) + height. data\textures\walls_normal, floors_normal; made from the loose textures when missing.
  data\textures\normal_maps.txt lists the engine's own maps with their hash: those (unchanged) are made again when
  NORMALS_VERSION goes up; any edited map is kept. Version 2.
- Shader: tangent frame from screen derivatives (T, B = texels per tile), parallax (8 steps + midpoint) and
  self-shadowing on the height map (6 steps toward each light), the normal map bending the face normal.
  F6 (keys.txt normal_maps) switches relief on/off; settings normal_maps, normal_strength.
- **Light = light sources only** (user, 2026-11-07): no ambient, no light round the player. data\lights.txt
  (written when missing): "light LEVEL X Z HEIGHT R G B INTENSITY RANGE", HEIGHT in floor steps. First light:
  level 1 (32.0, 3.0) at step 14, warm (1, 0.78, 0.52), intensity 1.6, range 7, where the first hallway leaves the
  entrance room. Each light has a glowing orb (emissive, colour alpha 2). Inverse-square falloff, windowed to the range.
- **Shadows now:** a shadow ray per light per pixel marched through the tile grid on the GPU (t2 = type + floor
  height per tile; blocked by rock, a diagonal's solid half, a floor above the ray, the ceiling): exact for the
  level's walls/floors/steps (slopes by their formula). Works with d3dcompiler (SM 5).
- **DONE 2026-11-07 (user allowed DXC):** shaders are files in src\shaders compiled by DXC v1.9.2609
  (O:\appsnotonc\dxc, build-time only) into build\gen headers. Hardware ray tracing (DXR 1.1 inline RayQuery):
  BLAS of the level + TLAS; scene PS (ps_6_5 -D RT) shoots a shadow ray per light at a random point of the light
  (size 0.06: soft edges once DLSS gathers frames). Probe grid (probes.hlsl, cs_6_5): one column per tile, 6 layers
  from 0.35 above the floor every 0.7 (diagonal tiles: the middle of the open half), 6-direction light each, 32
  Fibonacci rays a frame turned at random, hits shaded with the surface colour x (lights with shadow rays + last
  frame's probes) = many bounces over time, blended 0.93; the scene reads 2x2 columns x 2 layers, rock skipped,
  probes behind the surface down-weighted. No DXR 1.1 or raytracing 0: ps_6_0 variant with shadow rays marched
  through the tile grid (tested with --rt=0).
- **The orb artifact (user's screenshot: blue ring + green fringe)**: the orb was a tiny 24x-bright dot inside the
  picture DLSS / DLSS 5 process. Now orbs are drawn in present.hlsl over the finished picture at output size
  (disc + soft glow, hidden by walls via the scene depth), never seen by DLSS. Near-field falloff d^2 + 0.15 and a
  luminance shoulder above 0.6 so hot spots roll off to white.
- **Many lights / torch (user):** the player's torch is light 0 (right hand, flickering, T lights / snuffs, settings
  torch / torch_intensity / torch_range). Placed lights: data\lights.txt (hand) + data\lights\levelN.txt (automatic,
  written when missing: only room tiles with >= 17 of the 5x5 open, 6 tiles apart: landmarks, corridors dark,
  49 on level 1). Up to 1024 a level; lights_tiles() gives each tile its 8 strongest reaching lights, the ones
  it can see (line through the grid) x100. User's direction: torches required for exploring (inventory torches
  that burn down, candles, lanterns, as the original) once items exist.
- Speed: 300 frames at 1920x1080 with RT + probes + DLSS + DLSS 5 in 5.5 s including start-up.
- **Floors (user's screenshot 2026-11-07: cobbles looked sunk under the walls, worst looking UP a slope):**
  (1) slope triangles had a flat-floor normal (fixed: each floor/ceiling triangle gets its own normal);
  (2) parallax on floors fails by nature (grazing angles, no silhouettes, invisible to ray tracing); now walls
  only (stone faces at the wall plane, mortar carved in, adaptive 10-28 steps + linear refinement, faded at
  grazing); floors keep normal map + self-shadow. Relief depth is NORMAL_DEPTH_TILES (0.035) on every surface
  (normal maps v3: floors were twice as deep). Research (sources in the conversation: Tatarchuk 2006 POM,
  PlayCanvas, Dark Mod wiki, Unreal/Unigine pixel depth offset, Hoetzlein 2025): the real fix for floors is
  DISPLACED GEOMETRY (proposed: 16x16 cells a floor tile, indexed, ~1M triangles a level, height map raised from
  the floor, in the BLAS so cobbles cast RT shadows); walls keep POM, later + pixel depth offset (SV_Depth) for
  clean corners. Waiting for the user's go-ahead.
- **Relief as geometry (2026-11-07, user: "generate the detail then use a lower level detail in game"):**
  normals.c relief_mesh_write: full detail = a point per texel corner from the stones' SHAPES (cushion, no
  painted speckle); simplified by edge collapses with quadric error (Garland-Heckbert), borders first as lines
  (Douglas-Peucker, the same points on opposite borders: repeats meet), then to a budget of w*w/14 triangles
  (292 a wall texture, 73 a floor). data\textures\walls_mesh, floors_mesh\NNN.obj ("v u v h"; header
  "# uo-new relief v7": older engine files are remade, others kept). level.c: every face (wall, floor,
  ceiling, slope, diagonal) = Face (polygon + uv + texture); set_tapers: edges shared with a coplanar face of the
  same texture keep relief, others ease to the base over 1.5 texels; emit_face clips the repeated relief mesh to
  the face (Sutherland-Hodgman), splits at taper lines, raises along the normal by h x 0.035 tiles. Indexed
  triangles (renderer, BLAS, probes read t8). Level 1: 2.46 M triangles, loads in ~1.8 s.
- **Walls were mirrored** (user: the entrance doors faced wrong): u now grows to the right seen from the room
  (S edge u = -64x, E edge u = -64z, N u = 64x, W u = 64z; diagonals NW/SW reversed). Doors now join.
- **DLSS 5 default style 1 (B - Natural)** (user: "neutral"); the user's settings.txt line changed to 1.
- **Strings** (tools\strings_pak.py = reference, src\gr.c strings_each): data\strings\blockNNNN.txt "index: text"
  (\n escaped), blocks.txt lists them. Block 4 = item names (144 lantern, 145 torch, 146 candle, 147 taper,
  148-151 lit versions), 3 = books/scrolls, 6 = light levels, 7 = conversation words, 1 = messages, 2 = chargen.
- **Pictures** (tools\gr.py = reference, src\gr.c gr_picture): .GR = u8 1, u16 n, u32 offsets; picture u8 type
  (4 8-bit, 8 4-bit RLE, 10 4-bit), w, h, [aux pal], u16 size; RLE: counts 1/2/3 nibbles, repeat/run records,
  count 1 -> run, count 2 -> multiple repeats. All 33 .GR files extracted to data\sprites\* and data\ui\*
  (RGBA PNG, palette 0, index 0 transparent); the 9 .BYT screens to data\ui\screens.
- **Objects in the world** (world.c): items from the tiles' object chains (not 0x40-0x7F creatures, not >=0x140,
  not invisible flag bit 5) as upright cards facing the eye (VS expands: colour alpha 3; 4 = glowing), 1/48 tile a
  pixel, position x = tx + (x+0.5)/8, z = tz + (y+0.5)/8, y = z/32. Atlas now 1024 x 2048, pictures in 32x32
  cells from row 1024. Separate upload buffer (render_set_sprites), not in the BLAS.
- **Wall torches: REMOVED 2026-11-07** (user: "lets get rid of them as it wasn't in the original game ... the
  only way to see was to have torches yourself"). The automatic data\lights\levelN.txt files are no longer made
  or read (the old ones are still on disk, unused: ask before deleting). The floating light of data\lights.txt is
  commented out in the user's file and gone from the default. "walltorch"/"light" lines a person adds to
  data\lights.txt still work (drawn with the lit torch picture 0x95, flickering).
- **Interface** (ui.c): RGBA overlay at window/s (s = max(1, (h+150)/300)), drawn on the CPU, composited last in
  present.hlsl (t2 of the present tables, post.w = s). Fonts: .SYS copied loose to data\ui\fonts (u16 1, bytes a
  char, space width, height, bytes a row, widest; glyphs from CHARACTER 0, each bitmap + width byte). Pictures from
  data\ui\* and data\sprites\* (kept in malloc, not the arena). HUD (game.c): crosshair (cursors 0), name of what
  it is on + "E - TAKE/OPEN/CLOSE", compass (compass 0-3 by quarter), flasks (empty glass 76 + liquid slices
  0-12 red / 25-37 blue stacked 2 px apart), messages (4, fade at 6-8 s), the pack panel (I / Tab: 5 x 8 cells,
  mouse free, cursor 10; left = use (light / put out), right = drop). PANELS.GR has no picture headers (TODO).
- **Game** (game.c): inventory = 40 Object copies; names from strings block 4 ("article_name&plural"); take = unlink
  from the tile chain; drop = a free static slot (256-1023) at the feet. Light sources 144-147 (+4 lit): one lit at
  a time = the carried light; settings torch 1 = start with a lit torch, quality 40 (the dungeon is dark). T =
  light / put out the first light in the pack.
- **Light sources, from UW.EXE** (2026-11-07; tools\dis16.py / scan16.py = capstone 16-bit disassembly by file
  offset; DGROUP = paragraph 0x5AAC, file 0x5DCC0 (corrected: 0x5E2C0 was a slip)):
  - OBJECTS.DAT at 0xd62: 16 x (duration, brightness) for 0x90-0x9F, in DS at 0x5B3A. Lit lantern 10/4, torch
    3/2, candle 12/1, taper 0/3 (never burns out); unlit ones 0/0. Extracted to data\objects\lights.txt.
  - Clock: AIL timer at 256 Hz (file 0x1ef9d registers 0x1ef8b, which adds 1 to the 32-bit clock at [0x2364]);
    clock >> 8 = real seconds (60 x 256 a game minute). Game time = real time.
  - file 0x26587: whole seconds added to a byte at [0x28d]; above 20 it is zeroed and the player tick runs
    (0x2acbd: spell effects, then burn(1, ++[0x3f9])).
  - burn (0x2af36): the 4 light slots (DS 0x171e: inventory slots 5-8, hands and shoulders); a lit light (class 9,
    index 4-7) with duration d loses n = (counter % d == 0) + (span > 1 ? span / d : 0) points; when quality <= n:
    quality 0 and item - 4 (goes out, stays). So torch 63 s a point (fresh 40 = ~42 min), lantern 210 s (~2 h 20),
    candle 252 s (~2 h 48), taper never.
  - use (0x37267): "Lights may only be used if equipped." (strings 1:123) when not in a light slot; "That light
    is already used up." (1:124) at quality 0; lighting = item + 4, putting out = item - 4.
  - Brightness in our light: intensity 0.5 + 0.4 b, range 3 + 1.5 b tiles (torch b 2 = 1.3 / 6 as before).
  - TODO with the paperdoll: lights only in the hand/shoulder slots, up to 4 lit; oil refuels (strings 1:178-184).
- **Close to walls** (user report 2026-11-07: walls blown white, smears and pink haze up close): (1) the torch light
  was 0.25 ahead / 0.22 right of the eyes, so against a wall it sat on or inside the stone (blown out, or the room
  dark with sparkles from the relief). main.c place_torch now keeps it 0.2 from any wall, pulling the hand back
  toward the eyes. (2) light_term: the torch (light 0) has size 0.6 in the falloff (others 0.15), scaled to give
  the same light 1 tile away: at most ~2.5x that right against a wall instead of ~8x. (3) exposure 4 -> 2 (default
  and the user's settings.txt): at 4 DLSS 5 turns a bright near wall milky/pink and greys whole rooms; 2 matches
  native colours best (compared 1, 2, 3, 4, 8; DLSS 5 intensity / style did not help). Headless --set=key=value
  tries a setting for one run without touching settings.txt.
- **Doors** (world.c world_door_quads, game.c): 0x140-0x147 shut, 0x148-0x14F open; across the tile's middle,
  heading 0/4 spans x, 2/6 spans z; 0.5 tile wide (DOORS.GR 32 x 64 at DOOR_ROW 1536 of the atlas, picture =
  level doors[item & 7], 0x146 portcullis = 12, 0x147 secret = the wall texture), frames of the tile's wall
  texture beside / above; swings 90 degrees on the left hinge in 0.5 s (E); locked when its link is a lock 0x10F
  (flag bit 0 = unlocked, unconfirmed); frames + a mostly-shut door block (game_blocked from main.c fits()).
- **Move triggers** 0x1A0 (q, owner = their own tile) -> link: 0x181 teleport trap (dest tile q, owner; level z, 0 =
  this one: level 1 stairs have z 2), 0x180 damage trap (quality), others (0x182 arrow, 0x183 do, 0x185 change
  terrain, 0x187 create object, 0x188 door, 0x190 text string) not yet. Fired when the player walks into the tile.
- **Sound** (audio.c): .XMI -> .mid (= tools\xmi.py byte for byte: UW02, UW07, AW01 checked), data\music,
  playlist.txt (UW02..UW13), played on a thread through MIDI_MAPPER (velocity x volume), settings music,
  music_volume. UW*.XMI and AW*.XMI use MT-32 channels 2-9 (programs played as GM numbers: unverified by ear).
  The .VOC files are the intro SPEECH (1-7 s, 12,048 Hz) -> data\sound\speech\NN.wav. In-game effects are MIDI:
  SOUNDS.DAT (count 24, 5 bytes each: layout not decoded yet).
- **Creatures** (tools\crit.py = reference; gr.c crit_page / crit_frame): CRIT\ASSOC.ANM = 32 animation names x 8
  bytes, then 64 x (animation, palette). Pages CRnnPAGE.N0p with nn the animation in OCTAL (CR00..CR37). A page:
  u8 slot base, u8 slot count, slots (segment or 255), u8 segment count, segments of 8 frames, u8 palette count,
  palettes of 32 game-palette indices, u8 frame count, u8 compression, u16 offsets; a frame: w, h, hotspot x/y (the
  feet), type (6 = 5-bit units, 8 = 4-bit), u16 length in units, run-length data as .GR BUT counts of more than
  one unit combine 4 bits a unit even in 5-bit data (found by eye: otherwise long runs smear). Transparent = the
  small palette entry that maps to game colour 0 (not small index 0). Extracted per kind (object 0x40 + k):
  data\sprites\creatures\NN\pP_FFF.png + anim.txt (header "# uo-new creature v2"; v1 files are remade).
  Engine: the level's kinds' page-0 frames in 80 x 80 atlas cells from CRIT_ROW 1664 (atlas 1024 x 4096); cards
  stand on the hotspot, 1/60 tile a pixel. Slots used: 0 idle, 5 walk, 1 attack, 7 hit, 12 death (by how the
  frames look; unconfirmed). game.c: hp = mobile byte 0 & 0x7F (else 8), friendly when mobile byte 18 (npc
  number) is set; hostile ones that see the player within 7 tiles walk at 0.9 tiles/s, strike at 0.85 (1-3, every
  1.6 s); left click swings (2-6, 1 in 5 misses, 0.7 s); dead ones stay (last death frame). Health comes back
  0.08/s; at 0 the player wakes at the entrance (level 1 32, 1.5). Level 1: 51 creatures of 19 kinds.
- **Conversations** (conv.c): CNV.ARK (320 slots, 97 present) -> data\conversations\NNN.txt (header 8 words,
  "import NAME ID 1 TYPE RET" (0x111 functions by number, 0x10F variables at an address), "code N" + hex words).
  The machine (uwadv-documented UW1 VM): stack of words with the variables at the bottom (sp starts at vars-1),
  push = ++sp; JMP absolute, BEQ/BNE/BRA add the operand to the operand's address (+1 after), CALL pushes the
  operand's address, RET pops (calls < 0 = the end), PUSHI_EFF bp + offset, ADDSP pushes n zeros, STO (address
  under value), OFFSET a + b - 1, CALLI = a built-in by number: the argument count on top, the argument
  ADDRESSES under it. Done: babl_menu (answer = place 1-based), babl_fmenu (flags array; answer = string id),
  print, compare, contains, random, get/set_quest, sex (male), length, babl_ask (""); the rest answer 0 (barter,
  inventory, traps, skills, clock...). Text "@GS8" = global string at 8 etc. String block = header word 5
  (data\strings\blockNNNN.txt). BUG FOUND: strtol skips newlines (read the next line's first word: one word a
  line doubled) -> the loader stops at the line end. Checked: conversation 1 (the ghost of Cabirus' dream) runs
  through 5 menus correctly (--talk=1 --choose=1,1,1,1). Talking: E on a friendly creature: its own conversation
  = its npc number (mobile byte 18), else 256 + kind; panel with the words (wrapped) and numbered replies (1-9 or
  click; E / Esc to leave when it is over). Friendly = has an npc number (the attitude bits at word 0x0C were 0
  for talking NPCs: the layout is not the documented one; to find in UW.EXE).
- Readback fix: the map range ends at the last row's width (the last row is not padded): shots of widths not a
  multiple of 64 failed with E_INVALIDARG.
- Headless test flags: --ui=1 (draw the interface), --use=N (E on frame N).
- build.cmd new -> build\uw_new.exe when uw.exe is running (the user plays while we work; never kill it).
- (Old note) **Waiting (DXC):** hardware ray tracing (DXR inline RayQuery, SM 6.5) for objects/creatures casting shadows and
  bounced light (probe grid at tile centres, 4 heights, 32 rays each a frame, blended over time). Needs DXC:
  v1.9.2609 (29 Sep 2026) downloaded to O:\appsnotonc\dxc\download (sha256 ad31b1fc... = GitHub's digest) and
  unpacked to O:\appsnotonc\dxc\v1.9.2609, NOT yet run: running it was refused by the permission check (code
  downloaded from outside); the user decides.

## Sound: where it stands (2026-11-08, end of the session)

The user wants sound effects and music (both at 80%: `music_volume` / `sound_volume` in settings.txt, both in the
Esc menu) through the original's own sound code, exactly. Chosen device: Roland CM-32L through Munt with the
user's ROMs (`roms\cm32l_ctrl_1_02.rom` + `roms\cm32l_pcm.rom`, not in git); AdLib (OPL + TVFX) when there are no
ROMs. Then port every sound routine and every place UW.EXE plays a sound, routine by routine (ROUTINES.md).

### 2026-11-13: the effects came out on the wrong side; positional sound is the Roland's own

- **The mirror.** play_effect (SOUND.C 601ff) pans an effect General-MIDI-wise: 0x7F for a source east of a
  north-facing player (deltatotheta: heading 0 is +y, clockwise). The MT-32, and Munt after it, read the pan
  controller the other way round (Munt Part.cpp setPan: "Panning is inverted compared to GM"; measured with
  work\pantest.c on our mt32emu.dll: CC10 127 -> left rms 3075, right 259). So every effect sat on the wrong side,
  on the real unit in 1992 as well as here (the music is composed by ear on the unit and is not affected). Now
  fx_play's pan goes through port_fx_pan (port only, portgame.h port_fx_control; fxevent.c): mirrored when
  data\settings.txt `fx_pan_fix` is 1 (the default when absent; UWPORT_FX_PANFIX for the checks), the original's
  byte when 0. Proof (tools\fxcheck.py sides): level 1's door 1012 at 22,5 from 21,5, the save facing south-east
  (heading 23450), the door ahead-left (pan 0x1A); the Roland mix recorded by --audio-wav with music_volume 0:
  pan fix on left 289 / right 162 ("comes out LEFT PASS"); off left 61 / right 316 ("comes out RIGHT PASS").
- **Positional sound** (uwaudio.c, fx_audio 1, the Esc menu's "Positional sound") used to look for data\sfx\NNN.wav
  only, and none exist, so it played nothing. Now each SOUNDS.DAT effect is rendered once at start, on a worker
  thread, by a Munt of uwaudio.c's own (mt32emu.dll beside the game, the CM-32L ROM pair in roms\; set up as
  MT32MPU.ADV sets the unit: channel map, partial reserve, reverb 0/3/2, master volume 90; the effect's bank-1
  timbre from SOUND\UW.MT installed as the driver installs it, the note held for its length, the release rendered
  until quiet) into a mono OpenAL buffer; OpenAL places it (the original's falloff, HRTF when OpenAL Soft chooses it).
  While an effect is rendered and the module on, uwaudio_taken's bit reaches the game each frame (port_fx_control)
  and fx_play sends that effect's Roland copy at volume 0, so nothing plays twice; before it is rendered, or
  without Munt / ROMs / UW.MT ("fx: Roland effects off (why)"), the effect plays as before. A data\sfx\NNN.wav still
  takes precedence. The Sound volume applies to it (uwaudio_volume). Proof: the same door with UWPORT_FX_AUDIO=1 and
  UWPORT_FX_LOOPBACK=FILE (the loopback device rendered into a WAV by uwaudio_pump in a hidden run, nothing heard):
  the door's 1.5 s in OpenAL's output left 324 / right 55 ("LEFT PASS"), the Roland mix of that run left 30 /
  right 21 against 515 for the pan run ("silent while OpenAL plays it PASS"); tests\uwaudio_test.c "roland
  rendered / roland east / roland off PASS" (24 effects ready 100 ms after start). The user reported the door sound
  "from the switch" on 2026-11-12: the mirror explains it.
- Not done: a menu row for fx_pan_fix (settings.txt only). (The game side is underworld-exhumed f2c1249; da67d62's
  message names it wrongly.)

### 2026-11-13 later: the water sound that would not stop; the blows too quiet (the user's reports)

- **The water played on after leaving it.** PLAYMOVE.C make_noise starts effect 0 (SOUNDS.DAT: timbre 8, note 56,
  velocity 96, 6400/256 = 25 s) when swimming and kill_effect's it when the player is out of the water (or every
  0x1800 ticks to start it again). kill_effect only released the music driver's channel, which was already silent
  (the Roland copy at volume 0 while OpenAL plays it), and nothing told uwaudio.c: its 25 s copy played to the end.
  Now (port only): fx_play notes which effect each of the four slots holds (port_fx_slot_fx), kill_effect and
  kill_all_effects (death) send FX_STOP for it (portgame.h, fxevent.c: "fx: stop N here"), and uwaudio_event stops
  every source playing that effect (src_fx). Proof: tests\uwaudio_test.c "roland water plays / roland stop PASS";
  tools\fxcheck.py audio: into the water at 13,23 of level 1 and out onto 12,24 ("fx: sound 0 here" then "fx: stop
  0 here"), the loopback's second after the stop under a fifth of the second before it.
- **The blows (and every effect) at their SOUNDS.DAT velocity only.** play_effect / play_effect_here add the caller's
  vol to the effect's (COMBAT.C: sounds 3 and 4 at damage * 4 louder, up to 0x7F; the footsteps softer by the
  speed), and fx_play's note carries it; our event did not, so OpenAL played the base rendering (velocity 72 for a
  hit, where a hard hit is 0x7F: on Munt 6 x the RMS, 609 against 3603 in the test). Now the event carries the
  velocity (port_fx_velocity: effects[fx].vol + vol clamped, -1 when muted / not loaded), and uwaudio.c keeps a
  rendering per (effect, velocity): the base set at start as before, then the blows' velocities (3 and 4 at base +
  4k and 0x7F), the rest as the game asks for them (a worker queue; the first play of a new velocity uses the base
  rendering scaled by (v / base)^1.5 as a guess, the exact rendering from then on). A data\sfx wav gets the same
  gain guess. Proof: uwaudio_test "roland velocity PASS" (4 at 72: 609, at 0x7F: 3603). fxcheck audio runs the
  Roland cases now too (11 PASS). The level itself was already right: the door measured the same in the Roland
  mix (left 289 / right 162) and in OpenAL (324 / 55), so nothing else was turned up.
- **The plan for the rest of the sound** (the user's acoustics outline, 2026-11-13): docs\superpowers\plans\
  2026-11-13-sound-and-acoustics.md (the caller's vol, acoustic materials by texture name in data\acoustics.txt, a
  64-ray room probe driving EFX reverb, occlusion by the blocking material, Steam Audio as the alternative).
- **Steam Audio shelved, our own acoustics planned** (the user, 2026-11-14): docs\superpowers\plans\
  2026-11-14-acoustics-ours.md is the plan in force (rays over the tile grid + EFX; every setting a row of the Esc
  menu); 2026-11-13-steam-audio.md is kept, marked shelved. Its Tasks 0 and 0B (the menu) are built, 1-6 are not.
- **Acoustics, Tasks 1 to 4 built** (2026-11-14; plan 2026-11-14-acoustics-ours.md). Off unless the Esc menu's
  "Acoustics" row is On (`fx_acoustics 1`); with it off the sound is sample for sample what it was (uwaudio_test's
  `off` case against tests\data\uwaudio_east_ref.raw, rendered by the code before this work).
  - Task 1 (791bccc, 624706e): port\win32\uwacmat.c, what each wall, floor, ceiling and door texture is made of
    (stone, wood, carpet, metal, water, dirt, hanging), shipped in the code and written as data\acoustics.txt when
    missing. `transmit` is three bands (low, mid, high: the user): the share of the sound's ENERGY that passes a
    shut door (Steam Audio's "wood" and "rock" numbers; metal our estimate), so the gain on the sound is its
    square root (uwacoustic.c path_ray).
  - Task 2 (e6f1b9c): uwview.dll exports uwv_acoustic_grid / _doors / _epoch (the tiles, the doors with their
    picture, a counter that moves whenever level_build made a level); plat_win32.c fx_acoustic_level turns the
    textures into materials and hands the level to uwaudio_acoustics when the epoch moves, the doors whenever one
    opens or shuts.
  - Task 3 (feea5aa): port\win32\uwacoustic.c, rays over the tile grid (uwac_ray) and the room probe (uwac_room:
    64 rays, 16 tiles). One probe on level 1's grid: 0.004 ms.
  - Task 4: uwac_path (9 rays from the listener: 1 clear, the doors' transmit through shut doors, 0 through rock;
    the mean, never under 0.15 / 0.10 / 0.03) and uwaudio.c's EFX: one EAX reverb for the room round the listener
    (asked again every 100 ms, eased), one band-pass filter a source. Menu rows shown while Acoustics is On: Reverb
    level (acoustics_wet, 0 = no reverb), Room size (acoustics_tile_m), Muffling by walls (acoustics_occlusion,
    0 = no filter); each taken at once (uwaudio_tune), so each part can be turned off alone.
  - Departures from the plan, each for a reason: the tail is measured 0.6 to 0.9 s after the sound (right after it a
    dead room is as loud as a live one); a jump of the listener by more than a tile probes the new room at once;
    at Reverb level 0 the reverb is taken off its slot (OpenAL's slot gain alone left a tail already ringing to
    ring on); a cube corner of the source that is in rock is not a ray, and the source is lifted 0.3 off its floor
    (a thing's place is its foot); a shut door does not muffle a sound made in its own tile (its own creak was
    behind it from one side); the bounds sanitizer (trapping) in place of the address sanitizer (no runtime for
    our MinGW target); the "great hall" is the level's roomiest place by measurement (7, 5), not (32, 2).
  - Numbers from the hidden game run (fxcheck acoustics, level 1, acoustics on): the hall at 7,5 rt60 10.00 / 9.31 /
    6.92 s (low / mid / high), mean free path 6.7 m; a corridor at 37,13 rt60 3.38 / 2.62 / 1.99 s, 2.2 m; 17
    sounds placed, the 13 with rock on the straight line all at the floor values, 4 clear at 1.
  - **Open, for the user's ear:** the stone preset (absorb 0.02 0.03 0.04, dressed stone) makes every stone room
    ring 3 to 10 s, a cathedral's time. Rough-hewn rock is nearer Steam Audio's "rock" (0.13 0.20 0.24), which would
    give about 0.5 to 1.5 s. The table is data\acoustics.txt; the Room size row scales every time. Not changed
    without the user's say.
  - Not ours, seen in fxcheck's game runs with the acoustics off: "our copy of the water stops" and "the Roland
    copy silent while OpenAL plays it" fail when a creature walks beside the player in that run (its footsteps in
    the measured second); the second failed the same way on the build before Task 4.
  - Task 5, sound round corners (2026-11-14; row "Sound round corners", acoustics_detour, On by default, shown
    while Acoustics is On). uwacoustic.c: the walked way from the listener's tile to every tile within 12 (one
    Dijkstra a listener's tile and level, kept), uwac_detour (walked less straight), uwac_path_round (when the mid
    band is under 0.5 and there is a way: each band at least {1, 0.8, 0.5} * (1 - detour / 4)). **Added to the plan
    for the user's "I can hear things and cannot find them":** uwac_waypoint, the last place on the way that the
    listener sees; uwaudio.c places the source toward it, at the sound's own distance (the falloff stays the
    original's), so a sound round a corner is heard from the opening it comes through, not through the rock.
    A step up of more than 4 height steps is a wall to the sound that comes from below it. In the fixture: round
    an L's corner 0.71 / 0.57 / 0.35 against the blocked 0.15 / 0.10 / 0.03, both ears alike (the corner ahead)
    where the straight line gave left 270, right 426.
  - Task 6, footsteps by the floor (2026-11-14; row "Footsteps by floor", fx_footsteps, On by default, shown while
    Positional sound is on; needs no Acoustics). The table's `step N` (-24 to 24 semitones; shipped: wood 3,
    metal 7, water -5, dirt -2, carpet -2, stone none: the original's note). Effects 1 and 2: a
    data\sfx\NNN-<preset>.wav of the user's when there is one, else the Roland note moved, rendered once a preset
    and velocity in rend[32..63] (mt_render_as); a floor taking more than half the high band (carpet) at half the
    velocity. The materials are read at every start of the audio: data\acoustics.txt when there, else the table in
    the code (uwacmat_load_default; nothing written). To be judged by ear: the five notes.
  - Task 7, Acoustics High (2026-11-14, the user: Steam Audio back in, on top of ours; the Acoustics row is now
    Off / On / High, fx_acoustics 0 / 1 / 2). port\win32\uwsteam.c: phonon.dll (Steam Audio 4.8.1, Apache 2.0)
    loaded at run time from bin\ (never linked; port\win32\phonon.h gives the types). It makes the
    level's triangles itself from the tiles the acoustics already get (floors, the ceiling, walls, risers, diagonal
    walls, shut doors; level 1: 12,018), with uwacmat.c's materials (scattering 0.25 for all: a guess), simulates
    the reflections of up to 8 sounds at once (convolution, first-order ambisonics, 2 s) and mixes them for the
    listener's facing; uwaudio.c plays that mix through one stereo source kept fed block by block. The muffling by
    walls and the way round corners stay ours; the EFX reverb is silent in High. Every sound's samples are kept by
    its OpenAL buffer (pcm_of) so its own echoes can be made. A sound card run has two threads (rays every 50 ms,
    feeding every 10); a hidden run simulates and feeds in step with the frames rendered (repeatable checks).
    Without phonon.dll, or when Steam Audio does not start: one line "fx: acoustics high off (...)", and the
    acoustics go on as On. Measured on level 1: one simulation with one voice 20 ms, one block mixed 0.4 ms (a
    block lasts 21 ms). What it gives over On: each sound rings with the room it is in and its early echoes come
    from the walls near it; what it does not: anything for the muffling or for finding a sound (those are ours).
    To be judged by ear: its level against the direct sound (Reverb level scales it), and the stone preset's
    absorption (as for On: dressed stone rings long).
  - The sound review of 2026-11-14 (REVIEW-SOUND, after the user's first listen: "the walking sound is quite long";
    work\acoustics\stepcompare.py records a walk the game's way and ours, stepenv.py prints a step's decay):
    - A footstep IS about a second long on the Roland: effects 1 and 2 (the two feet, alternating; patch 1, notes
      29 and 24) are held 0.25 s and the timbre's own release takes the rest. The game's own Roland mix
      (--audio-wav, positional off) and our copy (loopback, acoustics off) fall alike: -10 dB at 0.6 s, -20 dB at
      1.0 s. The unit's reverb is not what makes it long (rendered without it the step is the same to -23 dB).
    - What the user heard was Acoustics On with dressed stone: the first room rang 8.9 / 6.0 / 4.5 s and a step was
      still at -20 dB three seconds on. The stone preset is now rough rock (absorb 0.13 0.20 0.24: that room
      1.4 / 0.9 / 0.7 s, the great hall 2.2 / 1.4 / 1.2 s, a corridor 0.7 / 0.5 / 0.4 s; a step at -20 dB after
      1.0 s). An acoustics.txt already written keeps its old line until edited: the three we have (data\,
      dist\UW1\data\, the test build's) were changed by hand, the old ones kept in work\acoustics\*.before-rock.
    - A sound's way into the reverb now carries the same filter as the sound (ac_filter_apply): unfiltered, a sound
      hidden by rock rang in the listener's room at full strength (uwaudio_test "occluded tail": 1223 from within
      the room, 155 from behind the rock).
    - Acoustics High: its echoes were 15 dB OVER the sound itself (HI_LEVEL 0.12 now: a few dB under it); a voice
      given to a new sound was still ended by the source that had it before (hi_voice clears the old holder); with
      all eight voices busy the one that has rung out longest gives way (a walk's steps held six of them).
    - The check "sides: the Roland copy silent" failed every run for a reason of its own: the music at
      music_volume 0 is not wholly silent in Munt (a bed of some 25 a channel from the title on). It now compares
      the door's three seconds with that bed.
    - Known and left: with Headphone 3D sound on, a sound at the listener (the player's own steps) comes out some
      7 dB quieter than with it off, and our copy is about 6 dB under the game's own mix even then; the original
      plays four effects at once at most and drops the fifth, ours plays all (32).
  - Three of the known-and-left put right (2026-11-15, the user; handoff work\HANDOFF-2026-11-15-sound-fixes.md):
    - The Roland copy's leak: it plays on Exhume's MT-32, not our Munt, and volume 0 there still leaked. SOUND.C
      fx_play's port-only block now returns before the note-on when port_fx_taken(fx) (slot, timer, fx_mask, the
      channel lock and fx_note stay the original's). fxcheck sides: the door's 3 s 30 / 18 against the bed's
      32 / 18 (was 45 against 29).
    - The ring times leaning short: uwac_room now bounces each of its 64 rays 12 times with cosine-law scatter
      off the hit's normal (UwaHit.n) and takes the mean leg from the third on; the reflections' delay uses the
      mean first hit (UwaRoom.first_m). The bias that was left (more bounces, shorter numbers) was the scatter's
      R2 sequence over i * 12 + k: neighbouring rays scattered nearly alike leg by leg, 16 % short in a closed
      box. A hash of (ray, leg) instead (hash01): work\rt60fix\probe.c's closed boxes 4x4 2.61 / 2.67 tiles,
      8x8 4.08 / 4.00, 12x12 5.03 / 4.80, 1x12 1.44 / 1.50 (random scatter, 112 000 legs: exact). uwacoustic_test
      "room mfp" PASS (A's middle 6.20 m, near its corner 6.40, 4V/S 6.40). In the game: the hall 2.45 / 1.60 /
      1.33 s (was 2.2 / 1.4 / 1.2), the corridor 0.82 / 0.55 / 0.46 s (was 0.7 / 0.5 / 0.4).
    - High's late echoes: the delay was the queue (a new sound's echoes went in behind three 1024-frame blocks).
      UWST_FRAME 512 (10.7 ms), HI_AHEAD 2: at most some 21 to 32 ms (worked out, not measured); one block mixes
      in 0.33 ms. And the rays' thread is woken at once by a new sound (HI.sim_wake), not after up to 50 ms
      (a voice's echoes stay silent until its first run). To be judged by ear: any crackle (the feed late).
    - Then the user: "so quiet, like the volume at 10%". With the Roland copy gone our copy was heard alone, and it
      had never been brought to the game's level (the 6 dB / 7 dB of "known and left" above). Measured on the same
      walk's footsteps (work\acoustics\levelcmp.py, levelsum.py: the mean of the loudest 30 10-ms windows): the
      game's own mix 750; ours 341 plain stereo, 138 with Headphone 3D. Now the listener's gain carries a makeup
      (uwaudio.c MAKEUP_STEREO 2.15, MAKEUP_HRTF 5.2, set again by hrtf_read once the device says; f8a990c): 735
      and 718; the loudest sample 2002 of 32767. tests\uwaudio_test.c runs at UWPORT_FX_MAKEUP=1 (its reference and
      levels were taken before). fxcheck steam materials grid acoustics audio sides: 124 PASS, exit 0.
  - Checks: `python -I tools\fxcheck.py steam materials grid acoustics audio sides`, `python -I tools\menucheck.py acoustics`.
- **The Esc menu made legible** (2026-11-14, the user: the font was hard to read, the model's name was cut; plan
  Task 0). src\smoothtext.c: Segoe UI through GDI (Tahoma where missing), glyph coverage kept per face and size,
  drawn at the window's resolution into the interface layer after the overlay's art (uwv_menu_rows2; the cursor
  blended again over the text). Mixed case; headings (a label beginning with '#': Game, Display, Graphics, Effects,
  Sound, Controls, AI; the keys skip them: plat_win32.c menu_sel_fix); a value too long is drawn at three quarters,
  then with its middle left out; the foot gives the chosen row's value in full and a line saying what it does
  (MENU_ROWS' hint). The panel 300 wide (216). The model's name is no longer cut in plat_win32.c (value buffers 128).
  New rows: Menu text (menu_font smooth / original: the old letters), Headphone 3D sound (fx_hrtf 1 / 0 / 2),
  Effects' sides (fx_pan_fix). The rows are one table in display order (MENU_ROWS), the renderer's option by the
  table (menu_opt), no longer by the enum's order. uwconv_draw.c's own smooth text was left as it is (not moved onto
  smoothtext.c: no risk to the conversation). The checks' options go to UWVIEW_SETTINGS=FILE, never data\settings.txt.
  tools\menucheck.py smooth / original / keys / kept: 47 PASS (smooth is in verify.cmd; the rest by hand, ~10 min).
- **The DLSS rows by name, changed during play** (2026-11-14, the user; plan Task 0B). Every render target is the
  output's size and the scene is drawn into its top-left part, so only NGX's two features know the render size:
  render_set_dlss_quality (render.c) lets Super Resolution and Ray Reconstruction go between frames, makes Super
  Resolution again at the new size (Ray Reconstruction by the next frame that wants it) and resets the history; the
  old mode is made again if it fails. Ultra Performance added (1/3, NGX value 3). The jitter's phases are now
  8 * (output / render)^2, 16 at least (DLAA, the default, unchanged). Rows under Graphics: Resolution (drawn ->
  shown, information), DLSS Super Resolution, DLSS mode (name and size; kept as port_dlss_quality dlaa / quality /
  balanced / performance / ultra), Ray tracing (On / Off now), DLSS Ray Reconstruction (what "High" was; kept as
  rt_quality high / normal; "Needs ray tracing" / "Needs DLSS" when it cannot run), DLSS 5, its style, intensity and
  structure. At 2560 x 1440: DLAA 2560 x 1440, Quality 1707 x 960, Balanced 1485 x 835, Performance 1280 x 720,
  Ultra Performance 853 x 480. menucheck.py dlss (hidden, 1920 x 1080): 24 PASS: each mode made by NGX at its size
  (1280x720, 1114x626, 960x540, 640x360), 30 changes in a row clean, Ray Reconstruction made at 1280x720 in Quality,
  the settings kept and read back. Frame Generation is not in the renderer: no row.
- **The power gem in our HUD** (2026-11-13, the user: the charge could not be judged): PANELS.C's adjust_power draws
  POWER.GR at 4, 60 counted from the bottom, i.e. rows 139-150 of the original's picture (31 x 12), and the game does
  draw it in our full-screen play (measured: 286 pixels change between rest and a charge); uwv_hud never copied it.
  Now copied in fight mode at the compass's left (src\uwview.c). Proof: tools\fxcheck.py gem (C and the left button
  held through the window's own input, UWPORT_MOUSETEST): "the original's picture shows the gem charging PASS",
  "our HUD shows the gem charging, at the compass's left PASS" (5347 pixels at 1920x1080).

Done:
- `build\mt32emu.dll` (external\mt32emu, Munt 2.8.6, LGPL, built by external\mt32emu\build-mt32emu.cmd; C API via
  LoadLibrary); tests\mt32test.cmd renders a chord with the CM-32L ROMs (works, 32 kHz).
- external\nuked-opl3 (the OPL chip) for the AdLib path.
- tools\adv.py runs UW's own SOUND\*.ADV drivers in Unicorn (MPU-401 and VGA retrace modelled), recording every
  port write per 120 Hz service tick; tools\adv_music.py plays a song as the game does. MT32MPU.ADV with UW02.XMI:
  FF 3F reset, sysex 7F0000 reset, 10000D channel map, 100004 partial reserve, 100001 reverb, per-channel
  controllers, then the notes -- the byte-exact reference.

UW.EXE's sound code (seg 1bd8 for the game's side, seg 2042 = the linked Miles AIL API):
- device: DS 0x13a (0 none .. 6 mt32mpu.adv; names at DS 0x14e; digital drivers at 0x15c), config DATA\UW.CFG
  ("0 -1 -1 -1 sound", "0 -1 -1 -1 speech", "0 cuts" in the user's copy: no device chosen).
- 0x1efe1: start-up (loads the driver, describes it, init, timbre cache, SOUNDS.DAT via 0x1fd8e).
- 0x1fa9b play effect (id, pan, volume change): SOUNDS.DAT entry (5 bytes: timbre, note, velocity, word flags)
  -> 0x1fee9 (the effect channels, AIL send-channel-voice through the driver).
- 0x1fb1c play effect at an object -> 0x1f82e (volume and pan by distance; hankmorgan's seg014_8AE).
- AIL thunks in seg 2042 (file 0x23620 + offset): 0xbf5 register seq (151), 0xbfb release (152), 0xc07 define
  cache (154), 0xc13 install timbre (156), 0xc31 stop seq (171), 0xc7f send CV message, 0xca9 ...
- The driver's function table: ADLIB.ADV / MT32MPU.ADV (both UW's own builds, differ from AIL 2.14).

Next, in order:
1. DONE 2026-11-09: src\xmidi.c = MT32MPU.ADV ported routine by routine (XMIDI player, MT-32 interpreter, MPU-401).
   Proof (tests\xmidi_test.cmd, in run-tests.cmd): all 24 songs identical to UW's own driver byte for byte, every
   retrace wait included, over 30000 ticks (4+ minutes, loops included); driver-call scripts tests\xmidi_scripts\*.txt
   (fades, tempo, stop/resume, channel locking with UW.MT timbres, branches, two songs, Roland controllers, display,
   sysex, timbre protection, shutdown) identical, return values included. Found: UW.MT = 64 bank-1 timbres, used by
   no song (the sound effects'); songs use the Roland's own timbres. A released handle's status reads the stale
   pointer in the original (0); ours returns 0 the same.
2. DONE 2026-11-09: src\sound.c (Munt x2: music / effects, notes routed by locked channel; waveOut 32 kHz; the driver
   served 120 Hz and the effects' timer 16 Hz on the audio thread; music_volume / sound_volume). --sound-wav=F,S[,FX].
3. IN PROGRESS: src\sfx.c = seg 1bd8's effects (0x1fa9b play, 0x1f82e at a place, 0x1fee9 start, 0x1fc95 timer,
   0x1fb7d / 0x1fbfc stop, isqrt 0x23018, sin table 0x23009) proved identical (tools\sfx_verify.py, run by
   tools\verify.cmd since 2026-11-09).
   Triggers done: combat (0x25039 miss 10 / hit 7 or 8; 0x24cb5 player hurt 3, creature hurt 4), doors (0x387c6 /
   0x388dd: 11, portcullis 20), swimming (0x312d3: effect 0, restarted every 0x1800 clock), spells (0x78dd9: the cast
   wait, effect 21 if too soon; 0x78ea5 fizzle 22 with messages 210-213; 16 when it takes; "Not a spell"), lockpick 19.
   Still to do (call sites of 0x1bd8:0xb1b / 0xb9c / 0x8ae): footsteps (0x312d3 second half: needs the movement
   physics' speed DS 0x2794 -> port the movement first), the swing at 0x246e2 (7 at the strike point), missiles
   (fire 9 at 0x277ff, hit 10 at 0x27ef1, 0x258cf hits 3 / 4), landing thud 15 (0x1cedd), objects landing 5 / 15
   (0x2d223), dropped items 15 (0x27ab5, 0x2b82a), tremor 18 (0x35ae4), creature sounds (0x1b8fa 14, 0x1c27b 6),
   0x38115 (19), 0x3a359 (12), 0x384e4 (21), 0x7225b (17). Also: the intro/cutscene music.
3b. DONE 2026-11-09: the music choice. src\songs.c = seg 1bd8 0x1f39c play (song n = file UW<n/8><n%8>.XMI, octal;
   AW.. for the AdLib; tempo 100, volume 96; a missing file / timbre turns the music off: DS 0x138), 0x1f58d resume,
   0x1f6e8 on/off, 0x1f75f toggle, 0x2053a ask, 0x20545 wandering (rand % 3 + 2), 0x2055c again-if-over (1 -> 4),
   0x2058d the turn, 0x20741 ended; proved identical by tools\songs_verify.py (in verify.cmd). The playlist is gone
   (data\music\playlist.txt left as it was; audio.c's GM fallback still uses it). Songs: 1 title, 2-4 wandering, 5-7
   a fight (5 the foe below 1/4 life, 7 the player below 1/4, else 6; changes at most every 8 s, each try restarts
   the 8 s, DS 0x2634; ends 10 s after the last blow / engagement, DS 0x2482), 8 Fight mode (player 0x5f bit 1),
   9 a kill, 10 death, 11 (not asked for anywhere found), 13 map / conversation / sleep. REPEATS table 561b:0000.
   DS 0x565e = the screen (1 the main game screen: a song that ends is followed by a wandering one; elsewhere it
   repeats). Hooks in game.c at the original's places: title at start (0x74d53), the turn each clock move
   (0x265c7), Fight mode (0x27621 / 0x274c5), blows the target lived through (0x1c44d / 0x1c48d, also our missiles
   and spells, which the original routes through the same damage routine 0x1c2d2), kills (0x25b56), conversations
   (0x6e68b, 0x6ea81, 0x6ee17), sleep (0x81fea, 0x820d8), death (0x8289c), silver tree (0x82858), the end of
   character creation (0x6d5c7). ONE TRIGGER ON OUR AI: the engagement (0x1b652: a creature record's goal target
   is the player) is fired by our AI's "hostile and has noticed the player" until the creature AI is ported.
   Not wired yet: the map screen (0x66074, no map yet), resurrection proper, 0x72e42's song 4 (si 1-3: unidentified
   view), the start-up's preinstall of bank 1 timbres (0x1f304: the effects install theirs on demand meanwhile).
4. AdLib (no ROMs): ADLIB.ADV (YAMAHA + TVFX) ported the same way, Nuked OPL3 for the chip.
5. Then ROUTINES.md on: creature AI; seg 1f3a is NOT all hardware (its math library 0x22fd8.. is game math): re-mark.
(old step 2 text:) Audio out: a mixing thread (waveOut, 32 kHz stereo from Munt; the driver served 120 times a second); music and
   effects at their volumes.
3. Port seg 1bd8 (start-up, music choice, effects, positional volume) and then every caller of 0x1fa9b / 0x1fb1c /
   0x1f82e (doors, blows, water, traps, spells...).
4. AdLib: ADLIB.ADV (YAMAHA + TVFX) the same way, Nuked OPL3 for the chip.
5. Then on through ROUTINES.md: creature AI next.

## Next (rewritten 2026-11-14; the first day's list, formats to engine, is done)
The latest handoff (HANDOFF-2026-11-14-screens-sound.md and any newer) is the detailed state; in short:
1. Sound and acoustics: docs\superpowers\plans\2026-11-14-acoustics-ours.md (rays over the tile grid + EFX; every
   setting an Esc menu row), being worked by its own session. Steam Audio shelved.
2. FORMAT 2: kind 1 and the hunters' "looking" sentence not yet seen live; the user judges the fine-tuned
   LFM2.5-350M-UW in play.
3. Hunters (HANDOFF-2026-11-13-routes.md): danger budget per leg as flood_path has it; plan1 flaky; a cheat goto to
   the same level logs no route; levels 4-9 routes; the Shift-click drop "huge pause" (not reproduced).
4. Sweep the original's screen for anything else our full-screen display dropped.
5. Closed 2026-11-14: the gargoyle's eyes blink (seen, see "Drowning checked; ..."); "far null pointer read at
   INVDATA.C:130" is the original's own read: FindSlot compares a slot with OpenBag->obj while no bag is open (DOS
   reads the interrupt vector table there); the port reproduces it on purpose (FARNULLREC) and logs it, like
   COMBAT.C:454 and AI.C's set_up_target. Nothing to fix.

## UW.EXE map and the object rules (2026-11-07)
- **The exe's layout**: load image at file 0x3200 (segment S offset O = 0x3200 + S*16 + O); DGROUP paragraph 0x5AAC
  = **file 0x5DCC0** (the light slots table at DS 0x171e = 05 06 07 08, the Look strings at 0x18ff...). Borland
  VROOMM overlays: "FBOV" at 0x65700, segment table at file 0x5c1c0 (161 x {seg, size, type, ?}, type 3 = an
  overlay stub); a stub: CD 3F, +4 dword -> overlay code at file 0x65710 + it, +8 code size, +0xA fixups, +0xC
  entries; entries from +0x20, 5 bytes (CD 3F, word offset). Overlay code names far segments by selector = table
  index * 8 (0x150 -> 0x32A8). tools\exe_map.py writes re\uw_map.txt: 1909 routines, callers/calls, the block 1
  messages each prints (printer 0x32A8:0x494 takes the number), object ids compared. tools\callsites.py shows
  the pushes before calls to a routine; string ids are block << 9 | index (0x150:0x8d gets one).
- **OBJECTS.DAT** in DS at 0x4dd8 (whole file): containers 0x5b0a (3 bytes), lights 0x5b3a. **COMOBJ.DAT** at DS
  0x5b6e (512 x 11). Byte 3: bit 5 = can be picked up (blood stains 0xDA-0xDF: no; the exe tests it as the last
  rule of "can it go in a container"), bits 6-7 = kind (0 a thing, 1 weapon/armour, 3 container; NOT stackable).
  Byte 6 bits 2-3 quality class; byte 10 bits 0-3 quality type, bit 4 "look" (else a decoration's own text);
  byte 7 bit 7 can have an owner. Extracted (all fields + armour slot/protection/durability + container
  capacity/accepts) to data\objects\objects.txt ("# uo-new objects v2"; the engine remakes its own older copy).
- **Look** (file 0x7ba10): "You see " + count or a/an (by the next word's first letter; quality type 13: none) +
  quality word (block 5 [qtype*6 + 0 at quality 0, 5 for qclass 3, else quality/16 + 1]) + name (plural for a
  count) + " belonging to" + block 1 [370 + owner] (owner 1-27, kinds that can have one) + ".". Decorations
  (file 0x7c0d1, jump table by item & 15): 0x164 bridge "You see a bridge."; 0x165 gravestone / 0x166 writing:
  block 8 lead-in [0x160 / 0x170 + (flags & 15)] ("The sign reads: ") then block 8 [link & 0x1ff when a count,
  else quality]; a gravestone with a picture reads GRAVE.DAT. Walls: block 10 [wall texture] (0-209), floors
  block 10 [510 - floor texture]. In game: L, or left click when no creature is in reach; E on what cannot be
  picked up looks at it.
- **Containers** (file 0x7aaeb): contents + item mass <= capacity (0 = none) else "The sack is too full.";
  accepts -1 anything pickable; an object number; 0x200 runes 0xE8-0xFF ("You can only put runes in the rune
  bag."), 0x201 ammunition 0x10-0x12, 0x202 maps/scrolls 0x138-0x13F, 0x203 food 0xB0-0xBF + plants 0xce/0xcf,
  candle 0x92, stew 0x11b, leeches 0x125, dead rotworm 0xd9; else "That item does not fit.".
- **Inventory** (inventory.c): pool of 512 objects, 19 slots: 0 helm 1 chest 2 gloves 3 leggings 4 boots 5/6
  shoulders 7/8 hands 9/10 rings 11-18 pack; armour by OBJECTS.DAT's armour table (byte 3: 1 chest, 3 leggings,
  4 gloves, 5 boots, 8 helm/crowns, 9 ring, 0 shield = a hand). Containers keep contents chained from link (as
  the levels do); taking/dropping moves the whole tree (level slots: free = not reachable from any tile through
  next/link). Lights only burn and light in 5-8 ("Lights may only be used if equipped."). Panel: grey board,
  sunk holes, the figure (BODIES.GR 0); left click takes/places/swaps/stacks (things with a plural name), on a
  container puts in; right click uses (light, open a container into the pack holes; "<" closes); L looks;
  click outside drops in front. Not yet: carry weight limit ("That is too heavy to take.").
- **The player tick** (file 0x2acbd, every 21 s): counter [0x3f9] + 1; spell effects; the lights burn (counter
  % duration); counter % 3 == 0: poison (player 0x5f bits 2-5) - 1 with damage; counter % 24 == 0: hunger
  (player byte 0x39) += -(3 + random 0-3) (via 0x82377, clamped at 0), drunkenness (0x61 bits 4-9) - 1,
  fatigue and two more bytes (0x3a-0x3c) + 1, and the **counter is reset to 0**. So the lights' durations run
  inside a 24-tick cycle: torch 3 -> every 3 (fresh 40 = ~42 min), candle 12 -> 12 and 24 (~2 h 48), **lantern
  10 -> 10 and 20 only = every 12 on average (~2 h 48, not 2 h 20)**, taper never. Hunger falls ~4.5 every
  8.4 min: a new character (0xC0 = 192, file 0x6c9c1) empties in ~6 hours.
- **Eating** (file 0x374ee): see inventory.c eat(). Food table OBJECTS.DAT 0xd82 (signed: meat 64, fish 48,
  corn 25, bread 16/12, cheese 12, apple 6, popcorn 2, toadstool 6, mushroom 0; drinks: water -1, ale -3,
  port -8, potions -127, wine 0) -> objects.txt v3 "food" lines. Adding past 255 is refused (msg 126); taste
  = block 1 [172 + min(4, (random 0-19 + quality) >> 4)]; specials 229-240; ale/port add drunkenness (then a
  check against the player that may knock them out: msg 241, TODO); toadstool poisons (4, or +2 up to 15).
  Sleep (file 0x82130) heals only when hunger > 0x40; at 0: "You are starving." (msg 17).
- **Status** (file 0x26122): block 1 [104 + hunger / 30] ("starving" .. "satiated") + " and " + block 1
  [118 - min(5, fatigue / 23)] ("wide awake" .. "fatigued"). In game: click the figure in the panel.
- **Command modes** (file 0x26fef: the 3D view's click handler; table of far routines at DS 0x28f, 5 entries in
  segment 0x22ef): talk, get, look, fight, use; with no mode chosen the click is index 2 = **look** (the
  original's default). Here: no modes, look on left click / L, use on E / right click.
- **Reading** (file 0x37fa0): 0x130-0x13F books/scrolls; 0x13B the map (opens the map); enchanted (item word bit
  12 = our flags bit 3): a spell scroll; bit 10 (flags bit 1): its own routine (0x59b8:0x89 with link + 0x100);
  else "You read the " + name + "..." then strings block 3 [link & 0x1ff] (below 0x100; else 0x5998:0x34).
  In game: right click it in the pack -> the reading panel (click pages, Esc closes).
- **Keys and locks** (file 0x36b55 use a key: 0x101 lockpick -> pick (0x333e:0x498), 0x100/0x102-0x10E -> "Use
  key on..." then 0x3831b): a door's link -> the lock (0x10F); **locked = item word bit 9 = our flags bit 0 SET**
  (our first door code had it inverted); the lock's number = its link & 0x1ff, a key's = its owner (byte 6 &
  0x3f). Result + 2 = the message: 0 "The key does not fit.", 1 "There is no lock on that.", 2 "The key locks
  the lock." (unlocked, closed), 3 "The key unlocks the lock."; an open door "That is already open.". A locked
  door: "The " + name + " is locked." (DS 0xa2f, file 0x381b8). Here E on a locked door tries the keys carried.
- **Switches and traps**: 0x170-0x17F buttons (0x170-0x172), switches (0x173-0x174), lever (0x175), pull chains
  (0x176-0x177), +8 = used: pictures TMFLAT.GR 0-15 (sprite cells 464+, TMFLAT_CELL), flat on their wall. TMOBJ 4-19
  are dials (8 angles), not switches. Using one flips it (item ^ 8) and fires its link: a use trigger (0x1A2;
  quality, owner = the target tile; link = the trap). Trap dispatcher file 0x83d96: by item & 0x3f, table at
  overlay 5a9c + 0xda0: 0x180 damage 0x840bd, 0x181 teleport 0x83dda, 0x182 arrow 0x83dc3, 0x183 do 0x84136,
  0x185 change terrain 0x83e17, 0x186 spell 0x84150, 0x187 create object 0x8418f, **0x188 door 0x843dd**, 0x189/
  0x18a ward/tell 0x846e3, 0x18b delete object 0x845dc, 0x18c inventory 0x8463b, 0x18d set variable 0x83ea4,
  0x18e check variable 0x83fc5, **0x190 text string 0x847bb** (block 9 [(quality << 5) | owner]); 0x184 pit and
  0x18f combination do nothing there. Door trap: the door on the target tile loses its lock, gets a copy of the
  lock the trap links to (if its link is an object), then quality 1 open / 2 close / 3 toggle. On level 1 all 7
  switches are use trigger -> door trap (q 3) -> a locked lock. Triggers have flags bit 6 set yet link to
  objects: the level "free slot" marking now follows every link.
- **Create object trap** (0x8418f): roll 0-62 < quality -> nothing; else copy the linked template (27 bytes for a
  creature, else 8; its own contents too) onto the target tile at the template's in-tile x/y and z (0xe8:0x122b).
  **Delete object trap** (0x845dc): unlink and free the linked object from the tile (trap quality, owner).
  **Set variable** (0x83ea4): heading/z bits reused as parameters: variable 0 = the quest bits (player 0x65/0x67,
  bit = ((quality << 5 | owner) << 3) | y bits; op = word 2 bits 7-9: 1 clear, 5 toggle, else set), others: TODO.
  **Change terrain** (0x83e17 -> 0x358:0x66): TODO. Level 1: move trigger on (3,36) creates 0x4D at (3,33).
  Object word 2 (bytes 2-3) = z (bits 0-6), heading (7-9), y (10-12), x (13-15): traps reuse them as parameters.
- **2026-11-07 stopping point** (user feedback): doors/frames/wall pictures were full bright (scene.hlsl treated
  colour alpha 5 as a glowing torch: now alpha 4 only); doors left see-through gaps (DOORS.GR pictures are 32x64
  with the top 12-13 rows and a side column transparent: Mesh door_top/door_l/door_r, the frame fits the opaque
  part). Attacks: left button held charges (CHARGE_TIME 0.8 s, damage x 0.4-1.0), release strikes; the crosshair
  hides while charging; the power gem (POWER.GR 0 grey, 1-8 rising, 9-13 green sparkle) above the compass. Look
  tag: on a thing in reach (item, writing, switch, door, friendly creature) the cursor becomes the blue pointer
  (CURSORS 10) and a click looks; a hostile in reach: the red ring (9). Hints moved to the bottom right. Inventory
  board 176 x 196 with labelled holes; a thing dropped on the figure goes to its slot (armour table, weapons the
  right hand, shields the left, lights a shoulder, rings a ring), what was there comes onto the pointer.
  NEXT: the combat rules from UW.EXE (weapon damage/skills/attack types by charge, creature AI and combat).
- **Tooltips and right click** (2026-11-08, user's WoW example): hovering a thing in the panel (nothing held) shows a
  tooltip: name (purple if enchanted), condition (block 5 word), where it goes, damage (slash/bash/thrust) and skill,
  protection, durability (quality x max / 63), container capacity, light brightness and burn time left, food,
  weight, value; "If you equip this:" gains (green) / losses (red) against the worn thing in its slot; Shift: the
  worn one's tooltip beside it ("Currently equipped"), each with its differences. Right click never drops: worn ->
  pack; bag -> opens; light/food/drink/book -> used; else -> into the first bag that takes it. Shift + right click
  drops into the world. objects.txt v4: "weapon N slash bash stab skill durability" (OBJECTS.DAT 2, 16 x 8; bytes
  3-5 not decoded) and "launcher N ammo" (OBJECTS.DAT 0x82 byte 2). Skill names block 2: 33 Unarmed 34 Sword 35 Axe
  36 Mace 37 Missile (table skill 3 sword, 4 axe, 5 mace, 6 unarmed). Headless: --shift=N, --mouse2, --rclick2.
- **Inventory as combined bags + options menu** (2026-11-08): the board (PW 188, the screen's height above the
  flasks): body holes 32 px with the slot's name in the background (5x6 font, faint under a thing; items drawn
  2x); one grid (24 px holes, 2 px gaps, 7 a row) = the pack's own holes (8 less the bags) then each shown bag's
  contents (from a new row, a row of room to drop in; bags in bags after); the bag bar at the bottom (left click
  shows/hides a bag, shift+left picks the bag up, a held thing clicked on a bag goes in); scroll arrows when it
  overflows. While it is open the world under the mouse pointer works (main.c casts the view ray through the
  pointer: GameInput mouse_dir): left click / L looks, right click / E uses, takes, talks; the name shows by the
  pointer. Esc (keys.txt release_mouse) backs out one step (game_escape: reading, talk, a held thing back into
  the pack, the inventory), else opens the options menu (paused; DLSS, DLSS 5 + style only when started, ray
  tracing (restart), bounce, relief, music, volume, mouse, field of view, quit; each change saved to
  settings.txt keeping its comment). render: DLSS 5 has its own switch (render_set_dlss5); an RTX 20-series /
  GTX / Quadro RTX card does not try DLSS 5 (plain DLSS). Headless: --menu.
- **Combat from UW.EXE** (2026-11-08; the full list and status: MECHANICS.md): weapon table DS 0x5972 (8 bytes:
  slash bash stab power_min rate power_max skill durability), critter table DS 0x4a52 (48 bytes: 0-3 armour by body
  part, 4 hp, 5 strength, 0x11 attack, 0x12 defense, 0x13/0x16/0x19 attacks (bonus, damage, chance%), 0x28 xp).
  Player record [0x7270] from 0x1e: str dex int + 20 skills; 0xb4 easy. Combat state DS 0x2654 score, 0x2670
  damage, 0x2675 angle, 0x2680 power, 0x2662 charge, 0x2668 target (1 = player), 0x2672 body part.
- **Wall decorations drawn**: TMOBJ.GR pictures in sprite cells 480+ (TMOBJ_CELL): writing 20 + flags & 7,
  gravestone 28 + flags & 1, flat on the tile edge their x/y touches (0 or 7), 0.045 out of the wall, 1/64 tile a
  pixel. TMOBJ 4-19 are switches/levers/buttons (0x170-0x17F; the exact mapping waits for the switch code).
- **Ray tracing High + DLSS Ray Reconstruction** (2026-11-08, user: "raytracing yes do that"): settings.txt
  `rt_quality normal|high`; the options' Ray tracing row cycles Off / Normal / High (High switches live; Off needs a
  restart). High = scene.hlsl built with RT + RTHIGH (scene_ps_rth.h): every pixel fires one cosine-weighted bounce
  ray (src\shaders\rthit.hlsli ray_light, shared with the probes: the hit surface's colour x the torch + one of its
  tile's lights at random, through shadow rays, + the probes' bounced light there) instead of reading the probes,
  and writes Ray Reconstruction's guides (MRT 2-4: diffuse albedo RGBA8, specular albedo 0, world normal + roughness
  1 in RGBA16F; cleared to the guide's "sky" values). ngx.c ngx_rr_create / ngx_rr_evaluate: NGX feature 13
  (dlss\nvngx_dlssd.dll from the DLSS SDK), own parameter object, roughness packed, HW depth, matrices transposed
  for row vectors; made the first time High is wanted with DLSS on, then it replaces DLSS upscaling (DLSS 5 still
  runs on its output). High needs ray tracing + DLSS on (the row says "(needs DLSS)" otherwise; then Normal is
  drawn). RR works on every RTX card (the 2070 laptop too). Verified headless on the 4090: --set=rt_quality=high,
  60 frames still + 6 frames after a turn clean, with DLSS 5 too, 0 failed frames; run-tests has rt_high.png.
- **Keys ' and ;** (2026-11-08, user): ' = DLSS 5 on / off (turns DLSS on with it if it was off; on a card without
  DLSS 5, DLSS itself); saved to settings.txt. ; (keys.txt "raytracing") cycles ray tracing High -> Normal -> Off,
  live: render.c now starts DXR whenever the GPU has it (settings raytracing 0 = start switched off) and keeps the
  grid-shadow pipeline (grid_pso) for Off; render_set_raytracing / render_raytracing_available. The options row
  is live too ("(no DXR)" on a card without it). Headless --rt-every=N cycles like ; (verified: each mode matches
  its own reference shot, Ray Reconstruction 0 failed frames).
- **No restarts for DLSS / DLSS 5 / style** (2026-11-08, user): NGX, DLSS and DLSS 5 are always started when the card
  has them; settings dlss / dlss5 only say whether they start on (before, dlss5 0 saved by the ' key meant DLSS 5
  was never loaded, so it could not come back without a restart). The DLSS 5 style is sent every frame
  (render_set_dlss5_style): the options row switches it live. Headless --style-at=frame,style (verified: switched
  0 -> 2 at frame 30 matches style 2 from the start, 0.69 mean difference, vs 6.2 between the styles).
- **Bounced light kept to the lights' reach** (user: the hallway was lit all the way down with ray tracing on): the
  probes carried bounced light on down corridors frame after frame, though direct light stops at the reach
  (SHADES.DAT). common.hlsli reach_window(p) = the falloff of the nearest light's own reach (1 - (d/range)^4);
  bounced light (Normal, High, and at the probes' hit points) is multiplied by it. The hallway past the torch is
  black now, as in the original and with ray tracing off.
- **Weapon art and swings** (2026-11-08, user): WEAPONS.GR pictures at the bottom right, placed by WEAPONS.DAT (the
  extractor writes data\ui\weapons\offsets.txt: picture, left, top in the 176 x 112 view), scaled (overlay height
  + 56) / 112 and anchored to the bottom right. The swing follows UW.EXE (MECHANICS.md): wind-up 0-3, held while
  charging (nothing in sight, crosshair hidden), the swing 4-8 after letting go, the blow at f 6 (player_attack
  now takes the type chosen when the swing began). --wield=item puts a weapon in the right hand (tests).
- **Right click equips** (user): a wearable thing in the pack or a bag is worn; what it replaces goes into the very
  hole / place in the bag it came from (wear_from_pack). Music volume default 0.8.
- **Command modes decoded; C = Fight mode** (2026-11-08, user): the six icons and what each makes a click in the view do
  are in MECHANICS.md (UW.EXE 0x26fef, table DS 0x28f). As in the original, only Fight mode attacks: with it off a
  click looks (the default with no icon); C toggles it (keys.txt "fight"), a message says which, and the lit Fight
  icon (LFTI.GR 9) shows beside the compass. Hints follow the mode (C - FIGHT on a foe; L - LOOK in fight mode).
  Headless --fight=N presses C on frame N.
- **Fight mode shows the weapon; shields; keys** (2026-11-08): the weapon is raised (pictures 18-20, three 1/16 s steps)
  when fight mode comes on, lowered when it goes off, and only drawn in fight mode (UW.EXE's raise / lower states).
  Armour as UW.EXE adds it (inv_armour: protection x condition / 64 + 1; a shield 0x3B-0x3F in the left hand
  counts on body and arms). E at a locked door with its key carried: unlocks and opens in one press. Conversation
  answers take a left click (verified headless: --converse=N opens conversation N; --mouse + --click picks).
- **Character panel (,) and tooltips with the character's numbers** (2026-11-08, user): the panel lists level,
  experience, skill points, Str/Dex/Int, Vit and Mana (now/max), hunger and fatigue (the original's words), carried
  weight of the limit, armour on body/arms/legs/head (UW.EXE's sum), the attack score with the weapon in hand and
  defense, fight mode, and all 20 skills in the game's own names (strings block 2, 31-50). Esc closes it. Weapon
  tooltips: its skill and the character's value, attack score, strength's damage bonus, power range and charge
  rate; armour: what it is worth now (condition) and where; shields: body and arms; lockpick: Picklock; anvil:
  Repair; launchers: Missile. game_skill / game_attr / game_easy give inventory.c the numbers. Headless --sheet=N.
- **Tab opens the character panel too** (user): a "Character" checkbox on the pack's weight line (C_CHECK); ticked
  (the default), Tab shows the character panel on the left and the pack and bags on the right, the game between.
  Kept in settings.txt character_with_pack (main.c saves it when the box changes). , still opens the panel alone.
- **Saves** (2026-11-08): data\saves\run-N is the game being played (current.txt names it; level_set_dir: level_load
  reads a level from there first, load_level writes the one left behind there, creatures' hit points stored first,
  dead creatures removed from the level with their belongings dropped). Slots data\saves\slot1-4, quick (F5 / F9),
  autosave (written when the window closes, loaded when it opens): level*.txt + character.txt + game.txt (hp, mana,
  fight, quest bits, the 128 variables) + inventory.txt (pool, slots, hunger, fatigue, drink, poison, the tick) +
  conversations.txt (each conversation's own variables, kept between talks now) + where.txt. Loading starts a new
  run folder; nothing is deleted. Esc menu: Save slot, Save game, Load game, New game. Headless --saves=DIR
  --save-at=N --load-at=N (run-tests: build\savetest).

## 2026-11-09 later: the ruleset, the roadmap, SKILLCHK.C done (5b05980)

- Direction (CLAUDE.md): finish before adding; work ROADMAP.md file by file; only emulator-proved routines count
  (re/proved.txt). underworld-exhumed (same UW.EXE MD5) is the specification and its MIT code may be adapted.
- Sounds compared with exhumed (87c3c14): blow sounds were reversed (fixed); footsteps, swings' whoosh / wall clang,
  creature walk / death, missile, landing, tremor, door-shut, switch sounds added. Not yet: instruments, Cup of Wonder,
  objects landing (needs object physics), wand "too soon" 0x15 (needs its 0x2FD wait), cutscene 0x11, map music.
  Ride on ours until ported: footstep speed (movement), creature walk/death timing (AI).
- SKILLCHK.C finished: player_get_exp, player_killed_a proved (verify.py). Experience is in tenths, shown / 10.
- init_char (CHARGEN.C:88) ported (unproved: no verifier yet); every start is a new game with a random character.
- Next three: motion/PLAYMOVE.C make_noise + set_player_phys_params (needs the world-state harness: the player
  record and PN in the emulator), motion/PHYSICS.C newFPS / do_player_input, combat/COMBAT.C frp_check + do_miss +
  player_killed_a's callers (the critical is x1 or x2 at even odds: ours x1.5-2.5, wrong).

## 2026-11-09: the rules core; Movement, Combat, Creatures, Objects proved

- The rules are now underworld-exhumed's C compiled as it is (tools\core.py -> build\core\core.a), proved against
  UW.EXE by tools\proof_*.py (16 scripts, tools\coreproof.py; run all with tools\proofs.cmd). re\proved.txt: 296.
- Core fixes (tools\core.py PATCHES and its unsigned rule; never in underworld-exhumed): Turbo C's 16-bit unsigned
  everywhere; tiles[] after oCollisions as in DGROUP; MOTION.C's 16-bit step; mts_doanim's null st; res_to_terr read
  past its end as DOS did (src\core_dos.c); null pointers read what the emulator has at 0 (the load image) and DS:0;
  sp_true_sight through area_spells; three uninitialised locals zeroed (the proofs zero the stack).
- Tools: Pair.trace with the CORE_INSTR=1 build finds the first routine where the two sides part; coverage says which
  routines a proof ran.
- The game: original_movement 1 (default) runs the core: the player moves by its physics (one main-loop pass a frame,
  check_physics), its clock is real time, its creatures are drawn from its level block with DRAWOBJ.C's frame choice
  (page 1 loaded; frames packed). Our creature code stands down while it runs.
- Open: AI with negative target offsets, seed 2 (proof_ai PROOF_NEG=1 PROOF_SEED=2): flood_path ends sooner in ours
  (a hyp_move result); combat PROOF_SEED=11 (a wall spark's height); player_simple_move (a clock wait).
- Next three: the player's attacks through COMBAT.C's player_attack (the core's hits, our swing animation); the
  player's vitals and death from the core (PlayerDat); the core's messages and sounds (scroll_print, play_effect)
  into our message log and sound.

### Next: the inventory and the world's objects on the core (design, 2026-11-09)
- Our panel stays the picture; its data comes from the core each frame: slots 0-18 are Inventory[0..18] (the same
  numbering), the held thing is CursorObjPtr, the pool is filled from the core's object store (cw_block).
- Actions follow INVPANEL.C's DoInventoryMouse step by step without its blocking mouse loops: pick up SetCursorObj,
  put down RearrangeInventory (fits, stacks, combines, swaps: ItemFitsSlot, AddTogether, COMBINE.C), the weapon hand
  toggle_fightmode, bag arrows / dropping in the world DoSpecialActions(DISP_UP / DISP_DOWN / DISP_WORLD); a stack's
  "how many" is our prompt, then the split as AskHowMany does it.
- The world: pick up = player_3dget's checks and steps (InPickRange, BlockingTerrain, EncumCheck, player_grabbed,
  release_3d) with newPlObj / PickDist / PickMap / releaseable from our crosshair; use = UseObj; look = LookAt.
- Then our level (G.lv) is refreshed from the core's block whenever it changes (level_from_block; doors from their
  items 0x140-0x14F; tiles changed by traps remesh), and our own object code stands down.

## Laigter normal maps (2026-11-10)
- tools\laigter-cli: Laigter 1.14.0's normal/specular/occlusion/parallax maker as a command-line program with no Qt
  (C++, CImg, our png.c reader); build: tools\laigter-cli\build.cmd -> build\laigter-cli.exe. Same options as
  Laigter's --no-gui (-d -n -c -o -p -r -l, suffixes, --check-changes) plus --tile, --flat-zero-x, -j.
- Proved against the real laigter.exe 1.14.0 (O:\appsnotonc\laigter-release): Qt's grey = qGray(unpremultiplied)
  matched on all 16.7M colours + 65,536 alpha samples; 17,388 maps (all walls, floors, sprites, 4 map kinds) and
  2 presets touching every setting: all pixel-identical. Pictures under 3 px across are skipped (Laigter reads past them).
- tools\laigter-cli\make-normals.cmd -> data\laigter\{walls,floors,sprites\...}\NAME_n.png (4,347 maps; UI left out,
  user: UI doesn't need it). The port loads them (level.c: sprite normal atlas t9, walls/floors before textures\*_normal).
  - walls + floors, tiled: tools\laigter-cli\texture.preset (EnhanceHeight 275, EnhanceSoft 2.6, BumpHeight 80,
    BumpDistance 32, BumpSoft 4.1): halfway between Laigter's defaults and pixelart.preset, blurs 25% under halfway.
  - sprites (creatures, objects, wall/flat objects, doors), not tiled: tools\laigter-cli\pixelart.preset
    (EnhanceHeight 300, EnhanceSoft 1, BumpHeight 60, BumpDistance 3, BumpSoft 1); Laigter's defaults made every
    16 px picture one smooth dome.
  - Both chosen by the user 2026-11-10 from comparison sheets (work\laigter\*sheet*.png).
  - Our one addition to Laigter's preset format: EnhanceSoft and BumpSoft may be decimals (whole numbers = Laigter).
  - Walls + floors, 2026-11-14 (the user: keep the normals, add AO in the joints, 3 px, soft, and relief shadows; the
    pixel-art preset on walls was tried and turned down): texture.preset also makes NNN_o (occlusion: distance mode,
    3 px, contrast 0.6, +85, blur 1; the threshold per texture = half its median grey + 1, our option
    --occlusion-thresh-median, because one fixed grey speckled the dark textures and missed light mortar) and NNN_p
    (height: contrast 2.5, -70, soft 1). The normal maps came out byte-identical (md5 of all 262). level.c puts NNN_o
    into the pictures' AO atlas at the texture's own place and NNN_p into the normal atlas's alpha (smooth, 4x);
    scene.hlsl: the occlusion on all the light a wall/floor takes, and relief_shadow, each light's ray marched 6
    texels through the height map. Settings (data\settings.txt): texture_ao_strength (1; 0 none), texture_shadow (3:
    the relief's depth in texels; 0 none); checks: UWVIEW_TEXTURE_AO / UWVIEW_TEXTURE_SHADOW. Seen off / on in
    uwtour and in an off-screen game run (work\laigter\walls14: sheets lit_t*.png, maps_t*.png, game_hall.png;
    make.py makes the sheets, gamerun.py the game pictures). The effect in the game is gentle (the joints are
    already black in the textures; the torch sits at the eye, so the relief's shadows show on walls seen at a
    slant and under the placed lights).
- Campfires' pulse (2026-11-14, the user: "deeper pulses, a more dramatic fire"): flames 0.4 m and wider swell by
  their own depth, render.c g_camp_depth 0.35 (settings campfire_swell 0-0.6, UWVIEW_CAMPFIRE for checks); the
  torches keep flame_swell 0.15. Measured at level 1's campfire (work\laigter\walls14\camprun.py, six pictures a
  setting at different moments): the whole picture's brightness spread 10% at 0.15, 21% at 0.35.
- In the port today (uwview.dll's renderer, level.c); tools\makedist.py copies data\laigter into dist\UW1\data\laigter
  (checked 2026-11-11: dist holds the current maps). After remaking maps, run makedist again (or copy data\laigter).
- To change the look: edit the .preset (Laigter's format: "Name<TAB>value" per line, names with their trailing
  spaces, e.g. "EnhanceSoft \t2.6"), then `tools\laigter-cli\make-normals.cmd all`. Try settings in work\ first:
  `build\laigter-cli.exe -q -n [--tile] -r X.preset -d data\textures\walls -l work\laigter\try\walls`, and show the
  user a sheet before replacing anything (work\laigter\sheet.py items, sheet_tex3.py/sheet_tex5.py walls+floors).
- Proof tools if laigter_cli.cpp is ever changed: the real Laigter runs headless:
  `O:\appsnotonc\laigter-release\unpacked\laigter-windows\laigter.exe --no-gui -d IN -n -c -o -p [-r P] -l OUT`;
  compare with `work\laigter\compare.py REF OURS` (references kept in work\laigter\ref, ref_tile, few_ref; inputs
  in, few). The user wants only a few checks, not whole-set runs.
- Laigter source (read only, GPL v3): O:\appsnotonc\laigter (HEAD) with tag 1.14.0 fetched; we ported 1.14.0's math
  (newer HEAD only adds speed-ups plus an `if (nr)` flat-pixel check, which is our --flat-zero-x).
- Separate idea, not built: LLM-driven creatures, see LLM_NPC_AI.md.
- (2026-11-13, since built) Level hunters: LLM_NPC_AI.md "Hunters" (routes by the original walker's terrain rule:
  lava, bridges, drops, the danger range; checked over all nine levels by tools\huntercheck.py routes in verify.cmd;
  the side file keeps what each hunter noticed and its clock, so a load neither re-notices nor ends plans at once).

## The port with our renderer (2026-11-10)

The game is now underworld-exhumed's whole UW1 port (the original's code, all of it), built with our LLVM clang on our
own Win32 backend, with our D3D12 renderer drawing the 3D view. The rules core bridge in src\core_*.c (cw_*) is no
longer used by it.

- Build: `O:\appsnotonc\sentinel\uv\python\cpython-3.13.17-windows-x86_64-none\python.exe -I tools\portwin32.py`
  (runs Exhume's portbuild.py unchanged; backend `port\win32\` copied in; tools\buildview.cmd builds uwview.dll,
  the DLSS 5 bridge and uwtour.exe). Program: underworld-exhumed\uw1\build\port\uw1port.exe.
- `tools\makedist.py`: dist\UW1, one folder for any PC (the program, renderer, DLSS, game\, the user's
  data\laigter normal maps; the rest of data\ is extracted on the first run). Play.cmd starts it.
- Backend (port\win32\plat_win32.c): Win32 window, raw input, waveOut, high-resolution timers; loads uwview.dll
  and falls back to GDI. port\win32\uwgame.c wraps the game's cFBtoScreen (each 3D frame: its frame buffer, the
  level, the light level, the zoom) and get_eye (the camera as the original places it).
- Renderer (src\uwview.c over src\render.c): the level from the game's live block (flat faces, the user's Laigter
  normal maps), objects, creatures (DRAWOBJ.C's frames), doors (ITEM_MOVING_DOOR), animated objects (ANIMO.GR by
  the owner frame), burning things as lights, the palette's colour cycling and fades, our settings
  (data\settings.txt: ray tracing, rt_quality, rt_ray_length, rt_bounces, DLSS / port_dlss_quality (DLAA),
  DLSS 5, sheen, sprite_normal_strength). The original's 3D window pixels show through only where the game drew
  over its frame (the cursor); a conversation or the map shows as the original's.
- 3D models (doors, the shrine's ankh, benches, boulders, tables, chests, barrels, chairs, pillars, the bridge...) drawn from UW.EXE's own model bytecode: docs\MODELS.md (src\models.c, world_model_quads / world_door_model_quads; UWVIEW_OLD_DOORS=1 keeps the old door slab).
- Checks (no exhumed test suites, the user's word): build\uwtour.exe (every level loads, pictures of items),
  windowed replays with UWPORT_NO_INPUT=1, UWVIEW_COMPARE, UWVIEW_SHOT_CRIT, UWPORT_TIMING; UWVIEW_CRIT_LOG=1 (a
  line per creature within 8 tiles whenever its sequence / frame / view / picture changes, with the camera) and
  UWVIEW_CRIT_FORCE=SEQ,FR (every creature drawn in that sequence and frame; hex SEQ: 20 standing, 2c walking).
- Things under the floor (2026-11-13, the user: "the slugs on the ground disappear for a second or two when they
  move"). The game sets a creature down at its tile's base height (AI.C move_mobile SET_Z(to->height << 3) when it
  heads for a far target; CRITTIME.C's placements) and lays items there, which on a slope is up to a step under the
  surface; the original's renderer has no depth test, so the picture shows all the same, where our floor buried it
  (level 1's two flesh slugs at 26,33 / 27,34 live between the ramps 24-27 x 31-32 and 24-26 x 34; the slug at
  37,60 starts on a slope; the three plants at 58,18 showed their tips only). world_object_pos now stands a thing
  on the floor's surface when its z is under it (presentation only; work\slug\cibefore.png / ciafter.png). Two more
  things of the creatures made as the original: an empty animation entry (0xFF) draws the page's frame 0 and the
  entry is segment * 8 + the object's frame 0-15 (pgcache_x.c parse_cpg L7D04-L7D11; we drew nothing and masked the
  frame to 0-7), and a creature's card carries where it stood last frame as its motion vectors (DLSS followed a
  moving creature with none). Not reproduced headless: a slug's walk (seq 0x2C, four frames, under a second) drew
  its pictures in every hidden run (crit log, full bright and lit shots, rt_quality high); the slope is the one
  mechanism found that hides a whole slug.
- Mouse in our display (2026-11-10, as the old version): the original's left button in its 3D view walks, so in
  full-screen play the buttons are ours (plat_win32.c world_click): left looks (the port's q, port_key_interact(0) at
  the game's pointer, which mouse-look keeps at the view's centre), right / E uses (its e: talk, use; uw-new's
  port_interact_take: a thing that can be taken is taken with player_3dget and put in the first empty pack hole as a
  click there would, RearrangeInventory; none empty: it stays on the cursor and our panel opens), C is the
  original's fight icon (its F5, deal_with_icons(1); fight_mode follows player->drawn), in fight mode left = Space
  (held charges). An object on the cursor: a click puts it down (the original's right click while carrying). The
  Tab panel's world clicks do the same at the pointer. Crosshair: CURSORS.GR 0 / 10 on a thing / 9 on a hostile
  creature in reach / none while charging; readout bottom right with its hint line (uwlook.c's UwlInfo: kind,
  attitude, InPickRange + BlockingTerrain, releaseable, pQatt, CursorObjPtr). Mouse-look follows the want (see
  "Input (2026-11-12)" below; check switch UWPORT_FOCUSTEST=T1,T2[,T3,T4]). wide-pitch on in our display with its bound
  0x38E3 (80 degrees; uw-new's port_pitch_wide). Test homes need uw1port.cfg (enhance=mouse-look,...): the ` key is
  bound at init_player only when mouse-look is on then (it is the toggle in the original's display only). Seen: at a steep pitch the original's own picture and ours
  place a floor object a few of its pixels apart, so the pick (the original's) can miss what our crosshair shows.

## Input (2026-11-12)

The window's input was rewritten (docs\superpowers\specs\2026-11-12-input-rewrite-design.md; plan and reports in
.superpowers\sdd\2026-11-12-input-rewrite\).
- **The module**: port\win32\uwinput.c / .h (pure C, unit test tests\uwinput_test.c in tools\verify.cmd). plat_win32.c
  hands it every key and button with the state it happens in; it picks the owner of each press: SETTINGS, MENU (our Esc
  menu), F11 / Esc / Tab as ours, PANEL, WORLD (our display's key table, the old our_keys), CENTRED (a conversation,
  the map, typed text), else GAME.
- **The owner rule**: a release goes to the owner of its press, whatever changed since; a held WORLD or GAME press
  that its owner may no longer keep (the screen left the 3D view, our menu or the settings opened, the focus lost)
  is released at once, and its later physical release sends nothing. This fixes Z's sleep loop, Space left held,
  the map's Esc held for good, and keys held through death. A GAME press of a key in the key table (W pressed over a
  conversation) is released at once when the world is on again (back in PS_GAME, full play): keeps() (final review,
  finding 5; uwinput_test "game_key_released_back_in_play", FAIL on the old code). A release the focus loss gives early
  (uwi_release_all at WM_KILLFOCUS) always takes the let-go path: world_event's `live` and panel_event's release need
  `focused` (full_play does not), so a pending take sends no look, a carried thing is neither thrown nor packed, a
  panel throw is not made; what is on the cursor stays there (finding 1; `inputcheck.py focusheld`). The one exception is raw mouse motion (WM_INPUT): it goes
  straight to the game while captured, and in our display only while mouse-look is wanted.
- **Mouse-look by want**: the backend works out what it wants (uwi_look_wanted: our display, PS_GAME, nothing of ours
  over it, no options panel, focus, the enhancement on, no replay) and publishes it (port_mouse_look_want, ui\MOUSE.C).
  The game applies it on its own thread at do_keyboard_input's key() poll as the reserved results 0xFE00 / 0xFE01
  (port\sys\keyqueue.c), so a recording replays it at the same call. So a recording of live play now carries key()
  results 0xFE00 / 0xFE01 (the want's mouse-look on / off): such a recording is ours only, not to be fed to the
  original binary or the emulator (the original has no such results). The capture follows the game's applied state:
  ClipCursor and the hidden cursor exactly while `focused && !settings && !menu && (lock || port_mouse_look())`. The
  backend never types ` (the old follower is gone).
  - In our display (display_mode 0, the R / , / Tab interface too), ` is swallowed by the module. The swallow comes
    after the SETTINGS, MENU, F11, Esc and Tab owners, and before PANEL, WORLD, CENTRED and GAME. So ` is also never
    typed into text in our display (a deliberate limit).
  - In the original's display the backend publishes "no wish" (-1), so ` is the game's own toggle again. Before, a
    standing 0 there would have undone the toggle at the next poll; display_apply's clearing of ENH_MOUSE_LOOK in that
    display hid it.
  - A switch from ours to the original's at run time (Esc menu, Display) publishes 0 first, until the game's
    mouse-look reads off, and only then -1. MOUSE.C applies an off even with ENH_MOUSE_LOOK cleared (still at key(),
    recorded), as b9ba607's follower turned it off there.
- **The display rule**: in our display PS_GAME always shows our 3D view. A frame that cannot be matched keeps our
  last frame ("display: kept our last frame (why)"). A new game (PS_MENU / PS_CHARGEN / PS_DEATH) clears it and shows
  black until our first view. The original's 3D-screen picture is never composed there ("display: original picture
  in PS_GAME" would say so). A level whose scene cannot be built (uwview's level_build gives up the scene it had
  before it fails) keeps the last frame's camera and level, and is tried again at most once a second, with no V.level
  call between the tries while the old level's scene is still there ("display: level N has no scene: level M's kept
  (tried again in 1 s)" under UWPORT_LOOKDBG); before, both levels were built again every frame (final review,
  finding 2; `inputcheck.py levelfail`).
- **The key ring cannot overrun** (Task 6, port\sys\keyqueue.c): KBDINT's 64-byte ring (seg063 0300h-033Fh) used to
  be lapped when the game stopped reading (our menu, the settings, a long load) while bytes kept coming, losing the
  whole ring at once and, with a lost break, leaving key_on counting a key as held. Now int9 stores into the ring only
  while it has room (one slot kept free), else into a host-side queue (4096 bytes) behind it; whole() (key()'s) moves
  the queue into the ring as reads make room, under the spinlock; a typematic repeat is dropped while the ring is full
  or bytes wait (an E0 key's pair whole or not at all). No make or break is lost. A sequence that fits in the ring
  gives the same key() results and the same seg063 bytes as before (recorded from 5da8a34). Replays: key() is not
  run, nothing changes. Verdicts: keyqueue_test "results_unchanged PASS", "overrun_keeps_pairs PASS",
  "repeat_dropped_when_full PASS" (the last two FAIL on the old code): keyqueue_test alone proves the ring fix.
  `inputcheck.py ring` (W held, our Esc menu 20 s, closed, W let go: no overrun line, no "key_on: 0x11 held", the
  player stopped) is a smoke check of the live game only: it passes on the old code too (the final review), so it is
  no evidence for the fix. A test cheat `where` (cheats.c) prints PN's position for the check.
- **Diagnostics** (log only): "keyqueue: ring full, N bytes queued" (once per occurrence, when the queue has gone into
  the ring; ", M lost" if the 4096-byte queue itself was full; it replaces Task 3's "keyqueue: ring overrun"),
  "key_on: 0xNN held" (> 5 s counted held while not physically held: the key the typematic has as held now is
  skipped, so a key held down is never said; final review, finding 3, keyqueue_test "key_on_only_lost", FAIL on the
  old code). Under UWPORT_LOOKDBG: "input:
  mouse-look wanted / not wanted / no wish", "mouse-look on|off (want)", "mouse-look's capture on|off", "screen N at M
  ms". There is also "clip: set N: clip L,T,R,B client L,T,R,B screen L,T,R,B cursor C at M ms" at each set_capture,
  with the real GetClipCursor and C whether the cursor shows (GetCursorInfo's CURSOR_SHOWING: 1 shown, 0 hidden, -1
  not read; before finding 4 it was ShowCursor's count, read by calling ShowCursor twice, which the probe must not
  touch), and "focus: lost to window ...; the foreground window ... at M ms"
  (ending "(focus test)" when UWPORT_FOCUSTEST made it). UWPORT_INPUTTRACE=1
  gives "in: key XX" / "in: ptr ...".
- **Our taps are held until the game takes them** (2026-11-13; the user: C while walking forward "sometimes doesn't ...
  i have to spam the button", a scroll that could not be picked up with the left button held). The game's key
  handlers never act on key()'s result: INPUT.C's input_dispatch reads MOUSE.C's do_keyboard_input(1), which replaces it
  with do_keyarray_input's (a scan of key_on, the keys held, at most every 30 ticks = 117 ms since the last key it gave,
  round from the last one). A tap whose make and break came together (send_key: C's F5, a click's q / e, the left
  button's n / o / u) was read whole by key() between two scans, so it counted only when no key had been given for 30
  ticks: while W walked (the walk's X given every 30 ticks), PageDown or a slide was held, it was lost. Now send_key
  sends the make and keeps the key down until MOUSE.C's port_key_taken (port only: each key do_keyboard_input gives,
  by scan code) moves, or 400 ms (before keyqueue's typematic at 500 ms); tap_pump lets it go at the window's next
  step (measured: 31-125 ms while walking). UWPORT_GRABDBG prints "tap XX held N ms". Replays: unchanged (key() and the
  scans are the recording's). `inputcheck.py fightwalk`: C six times while W walks, on/off alternate (FAIL on the old
  code: on, on, on, off, off, on). Seen while looking (work\inputbugs): a left press held on a thing at one's feet and let
  go still throws it with the lowest toss (y 0x25); looking steeply down that toss and ReturnObject's drop in front can
  both fail ("There is no space to drop that.", the original's verdict), so the thing stays on the cursor and our panel
  opens 600 ms later. The user's "huge pause" before a Shift+click drop was not reproduced off-screen (the panel's
  Shift+right: pick and drop in the same game frame, frame to frame worst 31 ms).
- **Checks**:
  - tools\inputcheck.py look / trace / charge / sleep / display / original / ring / focusheld / levelfail / fightwalk (hidden;
    original includes the run-time switch to the original's display; focusheld: the left button held on a sack when
    the focus goes, a take pending, a sack carried, a throw from our panel; levelfail: our renderer's data with
    level2.txt empty, cheat goto 2).
  - `inputcheck.py visible [play menu panel conv death newchar alttab sleep]`: shown runs on the user's screen, the
    real clip and cursor; it refuses while uw-new.exe runs.
  - tools\deathcheck.py and tools\followcheck.py.
  - Test hooks: UWPORT_MOUSETEST (the window's own events), UWPORT_FOCUSTEST=T1,T2[,T3,T4].

### 2026-11-13 evening: HRTF asked for outright

- **The gap.** uwaudio.c made its OpenAL context with no attributes, so HRTF was OpenAL Soft's own choice, and OpenAL
  Soft turns it on by itself only when Windows reports headphones: on most set-ups the effects were plain pan (front
  and back alike). Now the context asks for it (ALC_SOFT_HRTF attribute; data\settings.txt `fx_hrtf` 1 on, the
  default and written into dist\UW1\data\settings.txt; 0 off; 2 OpenAL's choice; UWPORT_FX_HRTF for the checks), and
  the log says what the device made of it: "fx: HRTF on (Built-In HRTF)" / off / denied / unsupported format.
  uwaudio_hrtf() and uwaudio_hrtf_name() for the checks. Loopback contexts ask the same way.
- **Proof.** tests\uwaudio_test.c: "hrtf on" (status 1, Built-In HRTF), "hrtf front/back differ" (effect 7 three
  tiles ahead vs three behind, both dead centre: difference RMS 1039 against 3094), "hrtf off" (UWPORT_FX_HRTF=0:
  status 0). tools\fxcheck.py audio: 14 PASS. Dropped on the way: an assertion that plain pan renders front and back
  the same; OpenAL Soft's stereo decode still differs by a little (971), so that was my assumption, not a fact.
- **HRTF is NOT 12 dB quieter (measured; the guess above it was wrong).** work\fxhrtf\measure.c: the same effect at 8
  directions x 1 and 3 tiles and at the listener, loopback 48 kHz, HRTF off vs on: a 440 Hz sine RMS 5318 / 5490
  (HRTF 0.3 dB LOUDER), white noise 3830 / 3182 (HRTF 1.6 dB quieter), the same at 3 tiles and "here". HEAD's
  uwaudio.c (no context attributes) built into the same harness gives 5317.9 / 3830.0: exactly the new code with
  fx_hrtf 0, so plain pan's level is unchanged. No gain compensation added (a ~4x one would make HRTF ~10 dB louder
  than before).
- **fxcheck sides "the Roland copy silent while OpenAL plays it" is flaky, with or without HRTF.** It compares the
  Roland mix (constant: 26-33 left / 15 right) against a quarter of OpenAL's WHOLE-run output, and that swings with
  the creature that wanders by (sounds 1 and 2): fx_hrtf 0 twice gave 107 (FAIL) and 200 (PASS) with the door's own
  window the same (147 / 25 and 146 / 25); HRTF on 115 (FAIL), door window 117 / 43. The earlier "pan 476 / door
  324 / 55" was such a run, not the pan level. The check is not in tools\verify.cmd. Not changed (rule 1; changing a
  verifier is the user's call): measuring the Roland leak against the door's own window would remove the creature
  from it. fxcheck audio "our copy of the water stops with the game's" is flaky the same way (creatures' sounds 15
  and 23 at 3-5 tiles inside its after-stop second): fx_hrtf 0 FAIL (54 / 17 after), fx_hrtf 1 PASS, then FAIL
  (29 / 43). run-tests.cmd (tools\verify.cmd): all tests passed with fx_hrtf 1.

### 2026-11-13 later: mouse-look on every rendered frame; the route grid's walls and slopes (second uwr session)
Two leftovers from the routes handoff, the user's pick ("focus on these two"); the LLM fine-tune work tabled.
- **Mouse-look on every frame we draw.** The game turns PlayerFacing by 64 units a mouse pixel and the pitch by 128
  only when its loop reads the mouse (MOUSE.C mouse_look_step, every 8 ticks), so with Smooth motion the turn came up
  to 31 ms after the hand moved. Now the count on both sides: mousedrv.c mouse_look_sent (the pixels given the game
  with mouse-look on, after look-speed; the mickeys set aside for a cursor correction taken off again), MOUSE.C
  port_look_moved (the pixels the game read and the facing / pitch units they turned it by, the pitch's bound
  applied), taken into the frame's snapshot as get_eye placed the camera (uwgame.c eye_look: UwgSnap / UwgView look[4]).
  plat_win32.c's camera gives the renderer the facing LESS the mouse's turns (eased by smooth_pose as before: keyboard
  turns, jolts) and, through the new uwv_look, the mouse's turns plus the pixels not read yet, added after the easing
  at once, the pitch clamped to port_pitch_bound. What is shown then depends on the mouse alone, not on when the game
  reads it: no backstep when the game's step lands. Mouse-look off, or the counts disagreeing (pending < 0): the
  pending part is reset (base). Presentation only: the game's facing, the crosshair's picking, nothing else changes.
  Not proved by a check (a hand's motion against the clock); the arithmetic is in the comment at the camera call.
  A fixed camera (a cutscene's camang) still gets the pending pixels for a frame or two: accepted.
- **Routes: the walls between tiles and the slope rule** (PATHFIND.C hyp_move's first tests and its h23++). HunterNav
  gets side[64][64]: TRIGGER.C tile_walls[type]'s TW_WEST..NORTH bits (a diagonal's two half walls; event.h's values,
  the same bits) and HNAV_SLOPE with the side the slope rises toward. step_danger refuses a step when either tile has
  a wall facing the other (every mover, fliers too, as hyp_move tests them before the 0x1000 flag), and counts a slope
  entered from any side but its rise one unit higher before the climb and drop tests (a Bresenham diagonal step of
  clear_line is never along the rise: path_turns has no entry for it). hunter.c build_nav fills side[] from the tile
  type. tools\huntercheck.py's reference walker has the same two rules (walker_cells keeps the tile type).
  hunternav_test: test_diagonal_half_walls, test_slope_entered_sideways (13 PASS; my first map had the detour row
  north of the diagonal, where its wall is: the test's mistake, rows run from y = 0 south). On the real levels (the
  reference's walker flood from the check's open square): level 6 loses 64 reachable cells and 199 cost more danger,
  level 1 loses 2, levels 5 and 8 have 5 costlier each, the rest unchanged. tools\huntercheck.py routes: levels
  logged 9 of 9, every walker's verdict the reference's, run 2 reached on 7 levels: PASS (the first run stopped after
  level 4 in run 2, the known fight-ends-the-run flake; the rerun passed).
- Files: underworld-exhumed uw1\src\ui\MOUSE.C, port\sys\mousedrv.c, hunter.c, hunternav.c, hunternav.h,
  tests\hunternav_test.c; uo-new src\uwview.c, port\win32\plat_win32.c, uwgame.c, uwgame.h, tools\huntercheck.py,
  LLM_NPC_AI.md, HANDOFF-2026-11-13-routes.md. run-tests.cmd: see the line below.
- **Reverted the same evening (the user: "revert the mouse look render stuff go back to the way it was"):** MOUSE.C,
  mousedrv.c, uwview.c, plat_win32.c, uwgame.c, uwgame.h back to their versions before 41d84df / 65673bd. The route
  grid's walls and slopes stay. Mouse-look turns the view at the game's steps again, eased by smooth_pose.

## Our conversation screen, step A1: the record (2026-11-13)
Plan: docs\superpowers\plans\2026-11-13-conversation-and-map-screens.md (the user: the conversation first; the
original's look and its proportions, centred). A1 done: read-only hooks copy what the original prints and chooses.
- underworld-exhumed CONVERSE.C (strt_converse: port_conv_player / port_conv_npc with the portrait gronk_gr loaded;
  babl_menu / babl_fmenu: port_conv_option, port_conv_menu; conv_play_menu: port_conv_chosen; babl_ask: port_conv_ask;
  free_converse: port_conv_close) and SCROLLIO.C (scroll_print in MODE_CONV: port_conv_text with scroll_mode;
  scroll_clear, scroll_wait). Each under #ifndef __TURBOC__; no call or value of the original changed (the NPC's name
  is reported after the original's own get_name, not by a second call).
- port\win32\uwconvrec.c / .h: the record (history of 400 pieces, the menu, waits, typed text, names, portraits,
  cnv_id), a spin lock, uwcr_snapshot for the window thread. UWPORT_CONVLOG=1 writes "conv: ..." lines.
- Found: an NPC line is not limited to 0xA0 (npc_say builds it in conv_buffer, the screen buffer): the Outcast's
  "Banner of Cabirus" line is over 300 characters, so a piece holds 1024.
- tools\convcheck.py (in tools\verify.cmd): off-screen `cheat talk N`, key 1 every 1.5 s; reference STRINGS.PAK
  block 0x0E00 + cnv_id by tools\strings_pak.py. Verdicts: talk 243 (Hagbard, conversation 19): 8 NPC lines of the
  block, 8 menus and 8 choices with their echoes PASS; talk 239 (the Outcast, 282): 7 lines, 6 menus / 6 choices PASS.
  The check fails when it should: Hagbard's lines against blocks 3604 and 3584 match 0 of 8. Creature 205 does not
  talk (no conversation opened).
- Not yet in the record: the trade slots (BARTER.C), for step A4.
- run-tests.cmd with the hooks built in: all tests passed (huntercheck 0 failures).
- Next: A2, our screen drawing from the record (the original's art and layout, centred, sharper text), the original
  still taking the input.

## Our conversation screen, step A2: our text (2026-11-13)
The user: the original's look and proportions, centred; text: a smooth font as the default, the original's letters an
option (settings.txt conv_font smooth / original, the Esc menu's "Conversation text", UWVIEW_CONVFONT for checks).
- The original's screen pieces stay (uwconv.c / uwv_conv); its four text places are ours (uwconv.c draw_text, the DLL's
  uwv_conv_text in src\uwconv_draw.c): the NPC's scroll (npc_scroll: x 0x38..0xDB, rows 51..131, text from 0x3B), the
  message scroll (main_scroll: x 0x0F..0x131, rows 169..198) and the two names (rows 2..8 at x 0x30 / 0x90). There the
  original's letters (its inks \0..\6: 0x2E 0x26 0xF1 0x60 0xB4 0xC4 0xD4; the names' 0x65) take the colour of the pixel
  left of them; our text is drawn from the record at the window's resolution in the same inks; pixels neither paper
  (0x2A) nor ink (the game's pointer) are drawn again over it.
- Smooth: Tahoma through GDI (anti-aliased, a glyph cache per size), line height 6 rows x the scale, the em 4/5 of it.
  Original: FONT5X6P from ui.c (ui_glyph), the pen moving by the glyph's width byte (not + 1 as ui_text): it wraps
  line for line as the original's screen (work\convshot\orig5.png against before.png).
- The NPC's scroll keeps the whole conversation (the record's history): it fills from the top as the original's, then
  the newest line on the bottom; the wheel (WM_MOUSEWHEEL, uwc_wheel) goes back two lines a notch; new text: newest.
  [MORE] (scroll_more's key wait) shows at the end in \3's ink. The message scroll shows what came after its last clear.
- Answers: our lines wrap otherwise than the original's, so a left press on our answer gives the game its number key
  (send_key 02..0A, conv_play_menu(option) as typing it) instead of the original's click on its row (plat_win32.c
  pointer_game, uwc_answer_at). babl_ask's typing: the message scroll is left to the original's letters (until A3).
- Found: in the test build (build\port) the DLL's data folder has no fonts and ui.c's fallback path did not exist, so
  the original's letters were blank there; ui_game_dir (from data_dirs) gives ui_start game\DATA as the fallback.
- Checks: tools\convcheck.py click (in verify.cmd): the key 2 and a click on our answer 2 (UWPORT_MOUSETEST, its new
  wh@N/0 wheel event too) give the same conversation, 91 events each, with each font: PASS.
- Next: A3, the rest of our input (typed answers in our own box), then A4 (trade).

## Our conversation screen, step A3: typed answers (2026-11-13)
- The original's wdialog (SCROLLIO.C) still takes the keys and applies its own rules (50 at most, its width limit in
  the original's font, the cursor keys, Ctrl+K and so on); a read-only hook, port_conv_typing (MODE_CONV only), gives
  the record its text and cursor after each key. Our message scroll shows ">" (wdialog's scroll_print) and the text
  with a bar at the cursor, in our font. The answer given goes into the record as UWCR_ASKED: in the message scroll as
  the original left it, and (ours) in the NPC's scroll in the chosen answers' ink, as play_say echoes a chosen one.
- Found: the conversations that call babl_ask (CALLI 0x14 with its import id, work\findask.py): 15, 18, 21, 24, 27,
  113, 144, 161, 187, 212, 281. Steeltoe (level 2, object 250 on tile 9 14, conversation 21) asks after 1, 1.
- tools\convcheck.py ask (in verify.cmd): d w a r f, Left, Left, x: the answer the game got is "dwaxrf", the NPC
  replies: PASS.
- Next: A4, the trade. (Later the same day: not needed with the original's layout; see the plan.)

## Our map (Part B, 2026-11-13)
The user: the original's look, only sharper; no zoom or pan; the notes in the original's font.
- The original (AUTOMAP.C) still draws its map: ShowDungeonMap / DoTile / ShadeSide / PixelDarken call rand(), so
  skipping them would change every later roll. Our map is drawn over the map's part of the screen (x 8..200, y up
  5..197) by the DLL's uwv_map (src\uwconv_draw.c) from the same data: BLNKMAP.BYT's parchment, PlayersMap by
  DoTile's patterns, terrain shades and kinds, DoDoorTile, ShadeSide's walls and ShowDungeonMap's corners (its quirk
  kept: east and south darken the top right corner), all in palette indices added up as the original does, then the
  palette now (the map's). Sharper: diagonals as lines across the tile (|a - c| < 0.5 of the original's pixels), the
  darkening's grain per half of the scale (a hash of the place, never rand()). Notes (ATM_Strings, FONT4X5P, ink 0x2D),
  the note being typed, the player's marker (BUTTONS.GR 0x3F) and the pointer (CURSORS.GR): the PNG's shape, the
  colours from the original's screen there (the PNGs are in the game's palette, the map uses its own).
- Read-only hooks (underworld-exhumed): MOUSE.C port_mouse_drawn (the cursor's rectangle and picture while drawn),
  AUTOMAP.C port_map_state (the level shown, the notes' count, the marker) and port_map_typing (a note being typed).
  UWPORT_ORIGMAP=1: the original's map alone (comparisons).
- A presentation difference: erasing a note in the original puts back the bare parchment under it (rectangle in colour
  0x106), tiles included, until the map is drawn again; ours keeps the tiles.
- Input is the original's (its buttons, notes, eraser, level arrows through the window's point mapping, as before).
- tools\convcheck.py map (in verify.cmd): M, a click, HALL, Enter, Esc, with ours and with UWPORT_ORIGMAP=1:
  SAVE0\LEV.ARK the same bytes (ceb784c1), the note in it: PASS. Pictures: work\convshot\map0/map1, note0/1/2.

## Drowning checked; the rune shelf and active spells on our HUD (2026-11-13)
The user (from the player's guide): "you can drown, why isn't this working" and "a rune shelf ... bottom left".
- Drowning works as the original's (PLAYTIME.C sink_sink_sink on the timed updates while swim_count > 0x50; water_set
  gives 0x60; a failed swimming check against the load (weight * 32 / max) adds rollem(3 - result, 4) up to 0x8C;
  above 0x78 rollem(2, hurt + 2) damage). Measured (level 1 tile 3 11, swimming 0, weight 3000): swim_count 96, 105,
  114, 124, 135, 143 and hit points 30, 21, 12, 7 once it passed 0x78. With some skill and a light pack every check
  passes and the player floats: the original's way, slow. Earlier fast deaths in tests were the test save's own
  state (hp 6, poison 4) and a creature at tile 15 25, not drowning.
- Test cheats (EXHUME_TEST_CHEATS; underworld-exhumed port\sys\cheats.c): `where` also prints hp, poison, swim_count
  and motion_state; `shelf R1 [R2 [R3]]` (runes 0-23 on the shelf, then the game's set_runes); `spell CLASS SUB` (the
  game's set_curmagic with stability 0x80, then FixPlayerEquips).
- Our full-screen HUD now shows the shelf's runes and the active spells' icons beside the compass as the original's
  screen has them (runes right, spells left, the first spell nearest; left of the power gem in fight mode): read-only
  hooks in PANELS.C set_runes / active_spells (port_hud_runes / port_hud_spells, plat_win32.c), drawn by the DLL from
  the game's own pictures (sprites\objects 0xE8 + rune, ui\spells) (uwv_hud_magic). Cutting them from the screen
  picture let the frame's animated dragons in. Picture: work\convshot\runes2.png.
- The gargoyle's face on our HUD (the user: its eyes show a creature's health), top middle: cut from the original's
  screen (x 127-149, rows 0-17, its black keyed out; the compass's border keying ate the face, which shares the
  frame's colours). Its eyes are PANELS.C's sprite (0x80, row 4); COMBAT.C sets set_screen_frame(7, 3 - fx) when the
  player hits a creature, fx = hp * 3 / its average hp (0-2), so adjust_eyes blinks set 1 (healthy), 2 or 3 (near
  death), five frames, then closes. Seen 2026-11-14 (work\eyes\eyescheck.py: fxcheck's goblin fight run 46 times
  hidden, the window shot and the original's picture of the same frame at 300 ms steps): the blow at 19.3 s opens
  yellow eyes (set 1) in the original's picture and in our HUD's face alike (work\eyes\crops.png). In that run
  the player dies at ~21 s; the death scene's green / red eyes are not shown by us (our skull covers the screen).
- A portcullis lifted its whole wall (the user's slot 1 save, level 1, opened by a goblin): world.c drew the door's
  frame (model 1) at the portcullis object's height, which rises 0x30 a step as it opens. DRAWOBJ.C do_door draws the
  leaf there but the frame at oldz = objyloc - steps * 0x30 with h = 0x400 - oldz - 0xD0: now the same (fpos).
  Checked on the user's save against uw-original.exe's own picture: work\convshot\port2.png (ours), port0big.png.
- Open bags: our panel's list (16, open_bags) keeps the order of opening and drops the oldest when full; it had filled
  with bags of older saves and games, so a new bag did not open. A display setting only.

## Quick fades (2026-11-14)
- quick_fades (uw1port.cfg, 1 the default; the user: seconds wasted leaving the map): UWEDIT.C strt_demscr's fade out
  and in (2 x 8 steps of 8 ticks) run with GRFX.C's port_fade_nowait: every step computed and set as the original's,
  no wait between steps. Measured: our view back 532 ms after the screen change, 1125 ms with the fades.
- A mistake on the way, recorded: the first version faded with a count of 0 and was committed (5e64f39, 01968c3)
  with "all tests passed" though convcheck map had FAILED (the commit read the last grep line wrong). The map check's
  SAVE0\LEV.ARK differed only in the 45 bytes past the note's text, memory the note code never writes (str_copy to the
  0): zeros with the fades, 0xE6 without (work\mapvary.py: each setting deterministic, twice each). Skipping only the
  waits did not change it: it is the timing (the map opens a second sooner, the script's keys land elsewhere), not the
  fade's memory. So the checks run with quick_fades=0 (a replacement is off in every proof): convcheck's runs, and
  format2check; the map check's byte comparison is unchanged and passes (ceb784c1 both).

## FORMAT 2: the creature questions (2026-11-14)
The user (through the fine-tune session): the fine-tuned LFM2.5-350M-UW (dist\UW1\llm\models) was trained on FORMAT 2.
- SYSTEM (port\win32\uwmindhost.c): the nine-rule text, generated from O:\appsnotonc\finetune\mind_format.py
  (work\format2_system.py). Every model gets it; its effect on the stock models is not measured.
- hunt.c: player_words after the "where" sentence: kind 4 (sees) "The adventurer looks <look>, <weapon>, <light>.",
  kind 1 (lost sight) "When you last saw them the adventurer looked ..."; look from player health / maxhealth
  (port_player_look: >= 3/4 unhurt, >= 1/3 hurt, >= 1/8 badly hurt, else near death), weapon player->drawn, light
  the reach (brightness + 1: candle up to 2, torch 3, bright lantern 4+). Not for kinds 2 and 3 (no sight).
- hunter.c: HunterMem.look (in the padding; 0xFF not known), set by port_hunter_lost; facts_words "you knew where the
  adventurer was: ..., looking <look>." when known. The side file's mem line has it as a 7th number (older files: not
  known). tests\uwmind_score.c and tools\mind_format.py: the FORMAT 2 copies.
- tools\format2check.py (loads the model; not in verify.cmd): goblin #218 beside the player (level 1, 10,38), hostile:
  its question "sees the player" equals mind_format's sees_player string character for character: PASS (1 question;
  "The adventurer looks badly hurt, no weapon in hand, carrying a ..."). Kind 1 and the hunters' "looking" were not
  asked in that run: not yet seen live.

## The full character sheet (2026-11-14)
- The user: "use the character stats information page from the game we were REA'ing, it had way more information,
  but keep the one we have now as an option in the Esc menu". Taken to mean our first engine's sheet (src\game.c
  sheet_draw / sheet_tip, the "," panel built from the UW.EXE decoding), which shows more than the game's statistics
  page: now the default sheet of the inventory panel (Tab, its SHEET box), parchment, 240 wide, the face (HEADS.GR).
- Rows: name, class, level; experience, skill points; strength, dexterity, intelligence; vitality and mana;
  "You are <hunger> and <fatigue>[, poisoned]."; carrying of the most; armour body / arms / legs / head; attack
  score (with its skill) and defense; damage slash / bash / stab (fists one number; a launcher its own); fight mode;
  the level, the day of imprisonment and the time of day; the twenty skills in two columns. Each piece has a
  tooltip (the old sheet's texts, shared with the game's page: sheet_tip_k).
- The numbers are the game's own, read on the game's thread (uwinv.c sheet_full, nothing written): armour
  playerdat->armour[0..3] and defence playerdat->defence as FixPlayerEquips (PLAYDATA.C) sets them; the attack and
  damage as DoPlayerWeapon (COMBAT.C) works them out (Attack / 2 + the weapon's skill + Dex / 7, + 7 easy, a 0xC
  enchantment), without its stores; the words as print_info (INTERACT.C, a click on the compass) picks them: hunger
  string 0x68 + hunger / 0x1E, fatigue 0x76 - min(5, fatigue / 0x17), level 0x19A + PlayerLevel, day 0x19B + day
  (none past 100), time 0x47 + the period.
- The game's page (the previous sheet) is the other choice: Esc menu, Display, "Character sheet" Full / The game's
  page (uwview.c option 40, settings.txt char_sheet full|original, UWVIEW_CHARSHEET for checks).
- Seen in hidden runs (work\eyes\sheetshot.py, the in2-4 save, 1920 x 1080): work\eyes\sheet-away.png (full),
  sheet-orig.png (the game's page), sheet-tip.png (the attack score's tooltip). No check of its own yet.
- The crash the user met (2026-11-14 ~01:35, dragging a fresh ear of corn; the sheet's skill points following the
  mouse): a makedist at 01:29 copied a uwview.dll built with this work's larger UwiSnap beside the 01:04 uw-new.exe
  (makedist copies a file only when its size differs, and the port build had failed, so the exe stayed old). The
  DLL read past the exe's snapshot: the exe's next globals (the mouse) as numbers, unterminated bytes as words.
  Guard: uwview.dll exports uwv_snap_size; plat_win32.c turns our inventory panel off (one stderr line, the
  original's interface instead) when it differs from the exe's sizeof(UwiSnap).

## Our message box: smaller, smoother, nine lines (2026-11-14)
- The user: the box where Ctrl+L's AI lines show "a better font and easier to read with a few more lines ... make
  the font easier to read and smaller". uwv_log draws the lines in smoothtext.c's Segoe UI at the window's
  resolution (4 x the overlay scale + 3 pixels: 19 at 1080p, the game's letters were 28), into a layer of the box's
  size laid over the parchment with the box's fade. UWLOG_ROWS (uwlog.h) 9 lines, was 5: the backend's log_draw,
  the wheel's limit and the DLL take it from there. The game's letters stay a choice: Esc menu, Display, "Message
  text" (option 41, settings.txt log_font smooth|original, UWVIEW_LOGFONT for checks). UWPORT_LOGTEST=1 (checks)
  fills the box once with sample lines. Seen: work\eyes\c-log-smooth.png, c-log-orig.png (work\eyes\logshot.py).
  The lines still wrap at the game's 49 letters (uwlog.c), so the smooth text leaves the box's right part empty.

## Creature AI: rules v4 (2026-11-14)
- The fine-tune session ("second uwr", for the user): rule 8 of SYSTEM rewritten to what its tools\hunter_policy.py
  labels (model r5: LFM2.5-350M-UW-r5-Q8_0.gguf / -r5-f16.gguf, 294/311 held out, 50/50 gold). uwmindhost.c SYSTEM and
  tests\uwmind_score.c's copy made from O:\appsnotonc\finetune\data\SYSTEM.txt (work\eyes\system_v4.py: the joined
  literal equals the file, 3590 characters); tools\mind_format.py = the fine-tune folder's again. The questions are
  unchanged. The stock models get the new rule 8 too.
- Found with it: uwmind.c's cached_system was 2048 bytes, the FORMAT 2 text 3058 (now 3590): the copy was cut, the
  strcmp in prefix_ready never matched, and the instructions (~900 tokens) were decoded again for every question.
  cached_system and prefix_text are 8192 now.

## The attack's kind in mouse-look: the flick (2026-11-14)
- The user: "while in combat with mouse look on I cannot do an overhead attack or a stab due to the mouse being in
  the center of the screen". Cause: the fight-mode left click sent the port's Space (keys.c port_key_attack(0)), which
  repeats its last kind, the slash until . ; or p choose another, and our key table has those three for other things.
- The user's pick (of flick, movement keys, extra buttons, the wheel): the mouse's motion in the 100 ms before the
  press (plat_win32.c attack_key, motion_speed): up at ATTACK_FLICK (0.4) window heights a second or more, and more up
  than sideways, the bash (the port's p, swing 9), down the stab (its ., swing 3), else the slash (its Space while the
  last kind sent was the slash, else its ;, swing 6). Only the key sent differs; the blow is player_attack's.
- Check: inputcheck.py flick (3 PASS; before the change every click's key was 39). charge and fightwalk pass as before.
  inputcheck.py trace has one FAIL (R, comma, Z, Esc bytes; no attack key in it) that the build before this change
  gives identically (work\trace-prev.txt): older, not looked into here.
- Not committed (the user did not ask); run-tests.cmd not run for it. ATTACK_FLICK is a first value: not felt by hand yet.

## Our HUD art: the gargoyle face, the hint line, the swing indicator (2026-11-14)
- The user's pictures (tools\art\*-source.png) cut by tools\hud_art.py into src\hud_gargoyle.h / src\hud_charge.h
  (the black surround cut away by a flood from the edges; planes for the glow). uwview.c hud_gargoyle: the face at the
  top's middle, 26 * s rows; its eye sockets and a halo glow with the game's eyes value (PANELS.C setting[SCR_EYES],
  read by uwlook.c uwl_combat, passed by plat_win32.c to uwv_hud_combat): 1 green, 2 amber, 3 red, held 2.5 s, fading
  1.5 s. hud_charge: the pedestal left of the compass, its fire spreading from the basin's centre with play_pow (while
  pQatt is -1..-9), pulsing at 100; cut from tools\art\charge-source.jpg (`hud_art.py charge`); a header with CHG_W 0
  falls back to the original's power gem. hud_art.py closes outline gaps up to CLOSE (13) source pixels (the tiers'
  seams had been cut with the black surround).
- The hint line: the smooth font (st_draw, em 3 * scale + 3) with a dark outline, not FONT_SMALL blown up.
- UWVIEW_HUD_TEST=EYES,POW forces both for a check; work\hud\shot.py takes the off-screen window shot.
- The bottom panel (the user's picture, tools\art\panel-source.jpg -> src\hud_panel.h, `hud_art.py panel`): 224 * s
  wide, flush with the bottom (uwview.c pnl_place / pnl_draw; parts by the source's 1024 x 282 pixels, PNL_X/PNL_Y).
  Compass in the middle plaque (hud_piece_fit, scaled to fit), active spells in the left, runes in the right
  (pnl_icons: the largest whole scale that fits). Message lines on the parchment always shown, never faded
  (log_panel; the fade alpha from plat_win32.c is ignored there), sliding up a line in 220 ms when the newest line
  changes. The first-person weapon is drawn from the panel's top; the pedestal sits left of the panel; the name and
  hint readout just above it. Checks: UWVIEW_MAGIC_TEST=R0,R1,R2,S0,S1,S2 forces runes and spells;
  `MAGIC=... python -I work\hud\shot.py NAME EYES,POW`.
- inputcheck.py display: "our last frame kept ... FAIL" (0 "kept our last frame" lines, expected at least one) on
  the build with the panel, twice. keep_last (plat_win32.c) is not touched by this work; not compared against a build
  without it.
- The beakers (the user, 2026-11-14: the flasks as end caps of the panel, square beakers showing health and mana):
  uwview.c hud_beakers / hud_beaker, drawn from shapes (riveted bevelled brass frame with cut corners, brass neck,
  glass with highlight and quarter marks, rippling surface, rising bubbles), strapped to the panel's ends, 1/12 taller
  than it. Health left (red; green while poisoned), mana right (blue), level = value / max from uwinv.c uwi_vitals
  (player->health, maxhealth, play_mana, max_mana, poison) via uwv_hud_vitals; UWVIEW_VIT_TEST forces them. The
  original's corner flasks are no longer drawn. pnl_place keeps the panel to 4/5 of the width for them.
- The pedestal now stands on the panel's top over the middle plaque, 22 * s tall, always shown (dark at rest).
- 2026-11-14 later, the user's second panel and gargoyle (tools\art\panel2-source.png, gargoyle2-source.png;
  `hud_art.py panel2` / `gargoyle2`, full size, the black surround cut by surround_mask): the panel now carries the
  tanks (pnl_tank: the dark of their glass, recoloured to the liquid below the level: tank_liquid, in pnl_draw; the
  shape-drawn beakers are gone), the swing light (hud_swing_light: a lens in the top housing, x 430-602 rows 42-82,
  ember -> orange -> pulsing yellow-white with play_pow, its glow spilling round), and in the middle plaque the
  heading strip in place of the original's compass (hud_heading: cam.yaw, eight points and 15-degree marks over 120
  degrees, north red). Spells x 172-312, runes 722-862 (rows 112-165), parchment 160-878 x 192-302. 232 * s wide.
  The gargoyle bar: 168 * s wide (1:1 at 1080), flush with the screen's top, its eye holes (rows 85-112) glowing in
  the struck creature's health. The pedestal (hud_charge) is gone; tools\art\charge-source.jpg and src\hud_charge.h
  stay unused.
- 2026-11-14 evening, the user: "go back to the way we had it before I started giving you graphics ... leave the
  paper scroll like area at the bottom for text, just above the compass we used from the original game". uwv_hud
  draws the original's pieces again (compass, flasks, gargoyle eyes, rune shelf, spells, power gem); our message box
  above the compass as before (its fade the Esc menu's Message fade; the user's dist home has message_fade=2). Our
  art functions are kept, marked unused, not drawn (pnl_w stays 0). Kept from the day: the attack flick, the smooth
  outlined hint line, and now the readout drawn again over the message box (it covered the hint's right end).
- The full character sheet in the smooth font (the user, 2026-11-14 evening: "about the same as the inventory sheet
  on the right ... a smaller font and clearer text"): uwinv_draw.c sheet_size makes it as tall as the inventory panel
  (L.ph), its 21 rows filling it, em the largest that fits a row, as wide as its longest rows; the text is queued in
  window pixels (sm_text) and drawn after the overlay is laid (uwi_panel_smooth, called by uwview.c uwv_inventory).
  The sheet's tooltips likewise (sm_tip), their paper queued too (sm_parchment) so it covers the sheet's text.
  Seen: work\eyes\sheetshot.py -> sheet-full.png, sheet-tip.png, sheet-orig.png (1920 x 1080).
- Then (the user: "a wider version of sheet-orig.png ... build the tooltip out of the original stats page as well"):
  the full sheet and its tooltips are boxes of PANELS.GR picture 2's art at any size (uwinv_draw.c page_at /
  page_box / sm_page_box: its 3-pixel frame, the wordless leather x 36-78 rows 5-58, the inset's leather x 8-74 rows
  66-100, repeated mirrored), the skills in the darker inset as the original's, the inks light with a dark shadow
  (SK_*). The tooltip box is queued so it covers the sheet's smooth text.
- 2026-11-14 night: Tab froze the game (1-2 fps, the mouse stuck): sheet_size tried font sizes 40 down every frame and
  hit test; smoothtext.c keeps 12 sizes and renders a new one's 224 glyphs through GDI, so it re-rendered ~20 sizes
  a frame. Now one interface text size from the window's height (smoothtext.c st_game_em: 19 at 1080, in
  proportion; the user: "the font size needs to be universal ... the game's resolution"), used by the hint line,
  the message box and the full sheet with its tooltips; the sheet's rows are one line of it each (21 rows + 18).
- One interface font (the user, 2026-11-14 night: "change the font to something that is easy to read and apply it to
  everything ... font size based on the resolution ... options in the esc menu"): ui.c's ui_text draws, when
  ui_font (settings.txt ui_font: 0 the game's letters, 1.. smoothtext.c's faces Segoe UI, Verdana, Georgia, Tahoma,
  Palatino Linotype; 1 the default) is smooth, into a text layer at the window's pixels (T), laid on after the
  overlay at every composite (ui_text_apply: 5 places in uwview.c); opaque overlay drawing clears the text under
  it (blend), so later things cover earlier text. Widths and heights are the face's, so layouts follow. Size
  st_game_em (19 px at 1080) x settings.txt ui_font_size % (Esc menu "Font size": 80..150). Esc menu rows Font
  (option 42) and Font size (43). The stats sheet and tooltips are the committed ones (the art version kept at
  work\hud\uwinv_draw-artsheet.c). build.cmd links smoothtext.c into uw.exe (ui.c needs it; uw.exe keeps font 0).
- More fonts (the user: "could we include other fonts?"): the Font row offers Windows' readable faces this PC has
  (smoothtext.c WIN_FACES: Segoe UI, Verdana, Georgia, Tahoma, Palatino Linotype, Calibri, Cambria, Candara,
  Constantia, Corbel, Trebuchet MS, Book Antiqua, Garamond, Consolas) and every .ttf / .otf in <game>\fonts
  (dist\UW1\fonts; st_load_font_dir: AddFontResourceEx FR_PRIVATE, the family name read from the file's name table),
  kept by name (settings.txt ui_font 0/1 + ui_font_name, spaces as _). Checked: work\hud\fontdir_test.c with
  segoesc.ttf copied in: offered as "Segoe Script", drawn.
- Tighter menus (the user, 2026-11-14 night): ui_parchment / ui_leather frames at half thickness (frame_half: one
  overlay pixel in all, drawn at the window's pixels into ui.c's text layer); tooltips (ui_tip, uwinv_draw tip_of)
  with margins of one overlay pixel inside the frame and ui_tip's body justified (just_line, at window pixels);
  wrapped lines one overlay pixel apart; the full sheet half as wide (FW 120) with tight rows; the message box
  without blank lines, as tall as its lines and as wide as a 49-letter line (faded with ui_text_apply_a). No red
  text (the user can't see red well): UI_INK_HEAD navy, UI_INK_RED now teal, poisoned green, skill values navy.
  Font size: 5 % steps down to the face's smallest (6 px) and up to 60 px (Esc menu, option 43).
- The mandolin / flute (the user: "when I play the mandolin I cannot get out of playing it"): SOUND.C
  play_instrument loops until mouse_get_input gives Esc, and our Esc menu took Esc in full-screen play. uwlog.c
  sets a flag at string 0xFA "You play the instrument..." and clears it at 0xFB "You put the instrument down." (and
  at the Esc passed on: the cup tune path prints no 0xFB); input_state's esc_menu is off while it is set, so Esc
  goes to the game. The other mouse_get_input loops (SCROLLIO typing, SKILLS/MAINMENU waits, CUTS, AUTOMAP,
  OPTIONS, CHARGEN) end on Enter/a click/any key or are not under our menu: none else traps. Not played in a check.

## 2026-11-15: plat_win32.c split into the Win32 backend and our front end (the Linux / SDL plan, step 2)

port\win32\plat_win32.c (3406 lines) is now two files, with the seam in port\win32\uwfront.h. The code was moved,
not rewritten; work\split\mk.py regenerates the split from the original by line ranges and exact replacements.
- plat_win32.c (834 lines): wnd_proc turning messages into uwf_* events, the window, full screen, display scale,
  raw input, the game thread, timers, threads, waveOut, GDI present, the folder picker, the dialogs, plat_run, the
  check-only UWPORT_FOCUSTEST / UWPORT_MOUSETEST injections, log_start, and the uwp_* services.
- uwfront.c (2718 lines, no windows.h): the screen state, the key and pointer routing through uwinput.c, the Esc
  menu, the inventory panel and clicks on things, mouse-look's want and capture, the attack flick, the hotkeys,
  the uwview.dll table (V, loaded through uwp_lib_open/sym), view_world / view_present, the fx audio, and the
  picture each pass (uwf_frame; the backend only does GDI's blit).
- Events: uwf_key, uwf_key_byte, uwf_pointer, uwf_raw_motion, uwf_hover, uwf_pointer_free, uwf_wheel, uwf_focus,
  uwf_set_focused, uwf_resized, uwf_mouse_unlock, uwf_mouse_title, and the ticks uwf_tick_begin / _input / _scan.
  Startup and shutdown: uwf_view_start, uwf_fx_audio_start, uwf_input_init, uwf_shutdown, uwf_frame, plus small
  queries. Services: uwp_ms, uwp_sleep_ms, uwp_xchg, uwp_inc (the game thread's flags), uwp_window,
  uwp_window_size, uwp_resize_client, uwp_title, uwp_capture, uwp_cursor_hidden / _hide, uwp_pointer_pos,
  uwp_clip_hold / _free, uwp_ctrl_down, uwp_close, uwp_new_game, uwp_repaint, uwp_lib_open / _sym / _close.
- Shared state: L (the loop's picture; its dst is now UwfRect, long fields like RECT's), hooks, live.
- Two differences, neither observable: uwp_clip_hold records its last rectangle before the warp's pointer
  move, not after (that move cannot re-enter it); uwp_window_size reads 0 where a failing GetClientRect used to
  leave the RECT unset.
- Proof: the before checks on 5d565c5 were menucheck 86 PASS / 0 FAIL, inputcheck 58 PASS / 5 FAIL (trace,
  display, 3x switch: they were already there before the split), followcheck 3 / 0, fxcheck audio 50 / 0,
  deathcheck 8 / 0, convcheck 6 / 0, run-tests.cmd "all tests passed" (in work\split\before). The after run was
  stopped at the user's word (11-15: only compile and launch now; the user checks the rest). After: clang
  -fsyntax-only gives the original's warnings, no more, in the GAMEVIEW, default and PLAIN builds; portwin32.py
  builds (5943808 bytes); an off-screen launch goes title -> start menu -> play (screen 2), D3D12 up, 1862
  presents, exit 0.
- Still Windows-only and needed again by an SDL backend: the message-to-event translation, Print Screen's and
  Pause's byte sequences (special_key), the Ctrl+F10 lock key, the window, full screen, max scale and big-window
  sizing, set_capture (ClipCursor + ShowCursor), the folder picker, the GDI present and its window shot, the
  UWPORT_FOCUSTEST / UWPORT_MOUSETEST test injections, log_start. uwfront.c also still uses the MS CRT's _stat,
  _stricmp and MAX_PATH's 260 (UWF_PATH); uwp_new_game uses the wide command line.

## 2026-11-15: an SDL3 backend beside the Win32 one (the Linux / SDL plan, step 3)

- SDL3 3.4.18 (2026-10-02; 3.4.20 was under two weeks old), the official devel-mingw archive, unpacked at
  O:\appsnotonc\sdl3\SDL3-3.4.18 (x86_64-w64-mingw32: include\SDL3, lib\libSDL3.dll.a, bin\SDL3.dll).
- port\sdl3\plat_sdl3.c (778 lines), made from Exhume's runtime/port/platform/sdl3/plat_sdl3.c (its key table, threads,
  audio stream, folder picker, display scale, icon and dialogs as they are there). Its input and picture go through
  our front end as plat_win32.c's do: SDL events become uwf_* events in plat_win32.c's order (uwf_tick_begin,
  uwf_tick_tests, the events, uwf_tick_input, the folder picker's answer, uwf_tick_scan, the picture), and every uwp_*
  service is SDL's. The window has no SDL renderer while our D3D12 renderer (uwview.dll, given the HWND from
  SDL_PROP_WINDOW_WIN32_HWND_POINTER) presents on it; SDL's renderer is made at the first picture uwf_frame hands
  back (no renderer, UWPORT_GDI=1, or the renderer failed). UWPORT_OFFSCREEN: SDL_WINDOW_HIDDEN (a real HWND for D3D12).
  The capture is SDL's relative mode, whose motion stands in for Win32's raw input. Exhume's web parts are left out.
- Build: `tools\portwin32.py --backend sdl3`, which writes underworld-exhumed\uw1\build\port-sdl3\uw1port.exe with
  SDL3.dll beside it. Our plat_sdl3.c replaces the runtime's: it is listed in runtime_exclude. The shared files stay
  in port\win32 (least churn) and are copied with it, without plat_win32.c. The default (win32, build\port), dist and
  makedist are as they were.
- Checked as the user asked (compile and launch only): both backends build. Hidden launches of each go title -> menu
  -> play (screen 2) with D3D12 up, the focus test and mouse test events running, exit 0. SDL3 with UWPORT_GDI=1
  shows the picture through SDL's direct3d11 renderer. build\port-sdl3 has no llm\ (build\port's was put there by
  hand; deploy does not copy it), so smart hunting is off there.
- Still Windows-only in plat_sdl3.c: the HWND property (our renderer takes an HWND), uwp_new_game's command line
  (GetCommandLineW / CreateProcessW; /proc/self/cmdline elsewhere), _putenv in log_start, "uwview.dll" and "\\"
  in paths, and the Win32 frame size (16 x 39) big_window_size allows for. The checks' window-system log lines are
  given in the same form, but SDL cannot name the window that took the focus, and the "clip:" line describes
  relative mode.

## 2026-11-15: a Linux cross-build from this Windows machine (the Linux / SDL plan, step 4)

- Our clang 23.1.4 has the x86-64 Linux target and lld. The sysroot, O:\appsnotonc\linux-sysroot, is made by
  tools\linux_sysroot.py from Debian 13 trixie's own .debs:
  - the packages: libc6, libc6-dev, linux-libc-dev, libgcc-s1, libgcc-14-dev, libstdc++6, libstdc++-14-dev, libsdl3-0
    and libsdl3-dev;
  - the checks: InRelease's signature by gpg, then SHA256s down to each .deb;
  - the result: 106 MB, plus 25 MB of .debs; glibc 2.41, SDL3 3.2.10.
  Munt is not in trixie's main, so the Linux programs have no MT-32. docs\LINUX_BUILD.md has the steps.
- Proof 1, work\linux\hello.c: ELF64 x86-64 PIE, NEEDED libSDL3.so.0 and libc.so.6, interpreter
  /lib64/ld-linux-x86-64.so.2, GLIBC_2.34 at most. No --allow-shlib-undefined is needed.
- Proof 2, tools\portlinux.py --original: upstream's UW1 port (exhumed-original, the runtime's own plat_sdl3.c)
  compiles 89 game and 91 port sources with 2 warnings, both unused variables when built without Munt. It links
  exhumed-original\uw1\build\port-linux\uw1port, 5,032,224 bytes: ELF64 x86-64 PIE, NEEDED libSDL3.so.0 and libc.so.6,
  the same interpreter, GLIBC 2.2.5 / 2.3 / 2.7 / 2.14 / 2.33 / 2.34. It was not run: Windows cannot run it.
- tools\portlinux.py on underworld-exhumed (uw-new): every source compiles after one fix, main.c's S_IFDIR changed to
  S_ISDIR (glibc's strict C has no S_IFDIR; committed in underworld-exhumed). It does not link: 25 functions of our
  backend's files are missing:
  - uwfront.c: plat_screen, plat_death_fade, plat_death_fade_start, plat_death_ours, port_hud_runes, port_hud_spells;
  - uwconvrec.c: port_conv_ask, _chosen, _clear, _close, _menu, _npc, _option, _player, _text, _typing, _wait, and
    port_map_typing;
  - uwlog.c: port_scroll_note;
  - uwmindhost.c: port_mind_ask, _models, _ready, _take.
  Step 5 brings them in.

## 2026-11-15: uw-new itself builds for Linux (the Linux / SDL plan, step 5)

- port\win32\uwos.h: the operating system calls of the shared files that run their own threads or load libraries.
  It is header only, so the standalone tests' file lists stay as they were.
  - On Windows it includes windows.h, and the files make exactly the calls they made before.
  - Elsewhere it supplies the same Win32 subset made of POSIX: thread, event and semaphore HANDLEs with
    WaitForSingleObject and CloseHandle, CRITICAL_SECTION, SRWLOCK, the Interlocked calls, GetTickCount64,
    QueryPerformanceCounter, Sleep, LoadLibraryExA (the given path, then the bare name), GetProcAddress and
    GetFileAttributesA.
  - UWOS_SEP and UWOS_LIB_OPENAL / _PHONON / _MT32EMU expand to the same string literals on Windows.
  - uwos_lib_findable is the old GetFileAttributesA check on Windows; elsewhere the file, or the library by its bare
    name.
- The files changed:
  - uwlog.c, uwconv.c: their own kernel32 declarations became uwos.h.
  - uwgame.c, uwsteam.c, uwaudio.c: windows.h became uwos.h; their library and data paths are spelt with UWOS_SEP and
    UWOS_LIB_*; the two library checks use uwos_lib_findable.
  - uwmindhost.c: unchanged under _WIN32, and a stub elsewhere: no model, port_mind_ready 0, as with no uwmind.dll,
    because uwmind (llama.cpp) has no Linux build. A full port needs uwos.h's calls plus a directory listing for
    llm\models and libuwmind.so.
- tools\portlinux.py, without --original, builds uw-new with port\sdl3\plat_sdl3.c and the shared files.
  - Result: underworld-exhumed\uw1\build\port-linux\uw1port, 5,777,944 bytes, from 89 game and 113 port sources with
    only the 5 old warnings.
  - The file: ELF64 x86-64 PIE, NEEDED libSDL3.so.0, libm.so.6 and libc.so.6, RUNPATH $ORIGIN, at most GLIBC_2.34,
    which suits Mint 21 (2.35) and Mint 22 (2.39). Only weak _ITM and gmon names are left unresolved besides glibc's
    and SDL's.
  - Debian 13's libSDL3.so.0 needs GLIBC_2.38, so it can ship for Mint 22 only; Mint 21 needs an SDL3 built for its
    base.
  - libphonon.so from the Steam Audio SDK's lib\linux-x64 (now unpacked beside windows-x64) needs GLIBC_2.14.
  - OpenAL comes from Mint's libopenal1. There is no Munt yet. Not run: Windows cannot run it.
- Windows, as the user asked (compile and launch only):
  - Both backends build; Win32 is still 5,943,808 bytes.
  - Hidden launches reach play with the focus and mouse tests and exit 0.
  - With our audio on (loopback WAV, nothing heard), both backends print "fx: Roland effects rendered by Munt (24
    effects, 32000 Hz)", "fx: audio on (OpenAL Soft loopback ...)", "fx: HRTF on (Built-In HRTF)", "fx: acoustics on
    (EFX)" and "fx: acoustics high on (Steam Audio 4.8.1)".
  - The standalone tests uwaudio_test, uwacmat_test, uwacoustic_test, uwsteam_test and uwinput_test build with no
    warnings.
  - build\port-sdl3's data\settings.txt (the user's) is unchanged by a rebuild.

## 2026-11-15: step 6: the message box on SDL, our Esc menu without the renderer, the Linux files in the game's folder

- The SDL build's message box never showed: plat_sdl3.c's uwp_ms and L.start were SDL_GetTicks, which counts from
  SDL's start, while uwlog.c stamps with GetTickCount64, which counts from the boot. The age underflowed, so the alpha
  was 0. Both now use GetTickCount64 (6d768ea). Window shots with UWPORT_LOGTEST=1 show the box on both SDL3 and
  Win32. No other place compares times from two clocks.
- uwos.h review (6d768ea):
  - Timed waits now take their deadline on the condition's own clock. winpthreads refuses CLOCK_MONOTONIC with
    EINVAL, so every timed wait had returned at once there; glibc accepts it.
  - ReleaseSemaphore past the maximum now fails, as on Windows.
  - CloseHandle joins and frees an ended thread, and detaches a running one, which then frees its own handle.
  - tests\uwos_test.c passes 20 of 20, both with the POSIX side forced on Windows (UWOS_FORCE_POSIX, three runs) and
    against Windows itself. It builds for Linux.
- Found on the way to Linux: uwacmat.c named "data\acoustics.txt" as one file name off Windows; uwfront.c's save
  slots looked for SAVEn/DESC, while the game writes SAVE0\desc, so off Windows they now go through plat_resolve.
- The Esc menu's drawing moved out of uwview.c into src\menu_draw.c unchanged (53bd34d). The SDL3 backend draws it
  when the renderer is not up (784a6b4): port\sdl3\uwmenu.c, the game's letters, laid over the picture. A hidden
  SDL3 run with UWPORT_GDI=1 opens it with Esc.
- With the renderer up, the SDL3 build's Esc menu shows the Language model row (window shot, AI section).
- tools\makelinuxdist.py stages the Linux files. tools\makedist.py places them in dist\UW1 and makes every name lower
  case. (Later the same day: makedist.py HAS been run, by another session, with the user's yes. dist\UW1 has lower-case
  names and the seven Linux files, byte-identical to release\linux. From now on the user allows makedist on dist\UW1
  whenever uw-new.exe is not running; say so in reports, since it replaces the exe the user plays.)

## 2026-11-15: step 7: our sound on Linux (Munt's MT-32 music and our Roland effects)

- The user's Linux run had sound, but not ours. Their log (WSL ~/uwplay/uw1/home/logs/uw-new.log) said "mt32: no ROMs
  (built without libmt32emu)", and there were no fx lines at all. That play folder also had no data\settings.txt
  (fx_audio stayed at its default, off) and an empty roms\, and the distro has no libopenal1.
- tools\buildmunt_linux.py builds build\munt-linux\libmt32emu.so.2 from Munt 2.8.6, the Windows DLL's own source.
  portlinux.py answers pkg-config "mt32emu" with it, so the runtime's MT-32 music is compiled in; the "rom_dir /
  music_kind unused" warnings are gone, and the ELF NEEDS libmt32emu.so.2. makelinuxdist.py ships it beside uw-new;
  the readme now says plainly that libopenal1 is required for our effects, and the "No MT-32" line is gone.
- Proof on Ubuntu 24.04 (WSL), work\linux\mint\test_audio.sh, with Debian's libopenal.so.1 put into the test folder
  only (nothing installed in the distro):
  - the MT-32 music: the UW.CFG it writes says music 6, and music.wav is 30 s of sound;
  - "fx: Roland effects rendered by Munt (24 effects, 32000 Hz)", OpenAL on, HRTF on, acoustics on (EFX);
  - fx.wav is 30 s with the effects in it.

## 2026-11-15: renderer features for the Vulkan port's ledger (fable dlss5 session)
Three renderer changes from today that the Vulkan renderer must carry (commits 6622e06, deaf295):
- **Firelight colour** (6622e06): render.c `render_blackbody` -> `blackbody_adapted` (with `bb_xyz`, `mat3_inv`,
  `oklab_from` / `oklab_to`, `cam16_degree`), the cached table `bb_at` / `bb_table` (rebuilt when the method or the
  adaptation changes: `bb_ready`). CPU only: the shaders get the colours through the lights (`GpuLight.col`, `pcol`) and
  `Constants.torchc`, as before. Settings `light_colour` cat16|bradford and `light_adaptation` auto|0..1; uwview.c
  options 31 and 32.
- **Door frame lintel texture** (6622e06): models.c `Ctx.vscale`, `mi_vscale`, the 0x0064/66/68 jnorm cases (reset
  per side), opcode 0x00BE (`BE Clk(7) -> *Clk(6)` restores the full height), `face()`'s uv[1] times vscale;
  models.h `ModelInst.tex_vscale`; world.c `world_door_model_quads` sets it to `delta_y`. CPU only (vertex uvs); no
  shader change.
- **HDR output** (deaf295): render.c `hdr_display` (IDXGIOutput6::GetDesc1, ColorSpace G2084/P2020, MaxLuminance),
  `hdr_apply` (ResizeBuffers FP16 + SetColorSpace1 G10/P709 or back to 8-bit G22, the back buffers' RTVs),
  `hdr_follow` (every 30 frames), the `render_set_hdr*` / `render_hdr_*` API, `R.present_hdr_pso` (`make_pso` scene -1:
  FP16 RTV), `Constants.hdr` + `CB_SHOT_OFF` (the screenshot's SDR copy of the constants, drawn into `final_tex`).
  Shaders: cbuf.hlsli `hdrout`; present.hlsl's shoulder `top` (SDR 1, HDR (peak/paper)^gamma), no clamp past 1 in
  HDR (the orbs' saturate too), and the scRGB output `pow(c, 2.2) * hdrout.y`. Settings `hdr`, `hdr_peak`,
  `hdr_paper_white`; uwview.c options 33-35; uwfront.c rows MO_HDR / MO_HDRPEAK / MO_HDRPAPER. On Vulkan: an
  `VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT` FP16 swap chain (or HDR10 ST.2084 with the PQ curve in the shader).
- Not to carry: render.c's `UWVIEW_D5SKIN` / `UWVIEW_D5AUTOMASK` check switches (DLSS 5, dropped).

## 2026-11-15: Vulkan renderer M1 (the device, the swap chain, the present pass with the interface)
- New files beside the D3D12 renderer: src\vk_load.c/.h (the loader opened at run time, VK_NO_PROTOTYPES: nothing
  linked against Vulkan), src\render_vk.c (render.h over Vulkan 1.3: dynamic rendering + synchronization2; M1 draws
  the present pass with the interface over a black scene; the rest of render.h are stubs until their milestones).
  tools\buildview.cmd builds present.hlsl as SPIR-V too (DXC -spirv, b0 at binding 0, tN at 16 + N) and
  build\uwview_vk.dll (uwview.c + render_vk.c, no D3D12 / NGX); portwin32.py deploys it beside uwview.dll.
- The switch: UWPORT_RENDERER=vulkan, or uw1port.cfg `renderer=vulkan` (the Esc menu's Display > Renderer row, from
  the next start). The default stays D3D12. Vulkan missing or not starting: D3D12, with a log line.
- Validation: UWVK_VALIDATE=1 (the SDK's layer is registered by its installer; else VK_ADD_LAYER_PATH=
  O:\appsnotonc\vulkan-sdk\1.4.363.1\Bin). Validation messages are logged and counted (the count at shutdown); the
  loader's own about this machine's other layers (GOG Galaxy's overlay, ReShade's implicit layer) are logged as
  `render: Vulkan loader:` and not counted.
- Proof: tools\vkcompare.py (results in the plan's "M1 result"): the title screen identical (0 pixels over 8/255);
  the Esc menu differs only in the Renderer row's value and the scroll bar's thumb (fewer rows); 0 validation
  messages; the resize and the full screen go through (the swap chain follows the window, the picture is stretched).
- uwview.c: UWVIEW_NOSCENE=1 (checks only): the scene tinted black, for comparing the interface alone.
- Builds collided with another session's test loop on build\port\uw1port.exe (the link is refused while it runs):
  wait for it, or copy a rebuilt build\uwview_vk.dll into build\port when only the DLL changed.

## 2026-11-15: Vulkan renderer M2 (the scene without ray tracing)
- src\render_vk.c draws the scene: scene.hlsl's SPIR-V (the same source as the DXIL), the level, the cards, the
  lights and flames, the textures and their cycling colours. Ray tracing, DLSS, HDR, particles, decals and fog are
  not in it yet (M3 to M6). The default renderer stays D3D12; render.c, ngx.c and nrbridge.c were not touched.
- Two checks in every build (tools\buildview.cmd; by themselves work\vk\m2\checks.cmd):
  `tools\render_vk_cpu.py --check` (src\render_vk_cpu.h is render.c's CPU side, copied by markers; a change to
  render.c's copied parts stops the build until the header is made again) and `tools\vklayout.py` (the C structs'
  offsets against the SPIR-V's; needs `-fvk-use-dx-layout`, without which the vertex structs were 96 bytes for C's 76).
- Proof: `tools\vkcompare.py --m2`, eight views, numbers in the plan's "M2 result". Vulkan differs from D3D12 only
  where two D3D12 runs differ from each other (the flask's bubbles, a creature's frame, edges a pixel apart); the
  still corridor is exact (largest difference 1 of 255); 0 validation messages.
- Unproved: the orbs (data\lights.txt has no light, so no view had one); the maps' textures and the water's
  constants (uploaded and filled, their options off in every comparison).
- Run comparisons with the game closed: the user's mouse reaches hidden runs (a tooltip under the pointer made two
  D3D12 runs differ by 176703 pixels). vkcompare's views are timed by the clock, so moving things never match exactly.
- uwview.c: UWVIEW_STEADY=1 (checks only): the player's light held steady. vkcompare cases take `cfg` (settings for
  the run's own uw1port.cfg; the "interface" view uses display=original, since R now opens our rune page).

## 2026-11-15: sound, the distance in the note's velocity and the two feet apart

The user: "it is not playing both feet sounds when i move forward" and "i could hear the rat moving behind the door at
the start well before i got near it". Measured with work\sound\walk.py (a hidden walk, our effects into a loopback
WAV) and steps.py (each effect's level left and right):

- **Distance.** play_effect lowers the NOTE'S VELOCITY from one tile to six (v * (0x30 - dist) / 0x28), fixed when the
  note starts; uwaudio.c had OpenAL fade the level by that line instead. The Roland's level falls about as the cube of
  the velocity (effect 4: 72 -> 609, 0x7F -> 3603), so far sounds were much too loud: a rat's steps five tiles off
  measured 190 against the player's own 620 (10 dB under), where the original plays them at velocity 12 of 64. Now
  uwaudio_event works out the original's velocity from the distance (eighths of a tile, as play_effect), plays the
  rendering of that velocity (made on first use; until then the base one by the cube, VEL_GAIN) and turns OpenAL's own
  fall off for that source (AL_ROLLOFF_FACTOR 0). After: the same steps measure 3. Without a velocity (no SOUNDS.DAT)
  OpenAL's line as before. Left as it was: port_fx_velocity clamps to 0x7F before the distance, the original after
  (only a blow over 0x7F differs).
- **The feet.** Both effects (1 and 2, notes 29 and 24) did play in turn, but both dead centre: the original's
  play_effect_here(1, 0x48) and (2, 0x38) pan them a little right and left, and our event dropped the pan. Now the pan
  rides in the event's kind (portgame.h FX_PAN, SOUND.C play_effect_here) and uwaudio.c places the sound to that side.
  The game's own pan is an eighth of the way; ours the same since the user's "play the sounds like the original does" (first 4 x that; data\settings.txt fx_feet_spread, percent:
  100 the game's own, 0 the middle as before; UWPORT_FX_FEET for the checks). After: foot 1 left 568-642 right 653-722,
  foot 2 left 589-650 right 511-581 (before: equal). No Esc row yet.

## 2026-11-15: the archways at the level stairs (special wall panels 0x16E / 0x16F)

The user at level 1's 7,34: "it teleports me to the second level but there is no door or anything here ... there has
to be something that represents the way down". There is: a special wall panel object (0x16E, owner 47 = the level's
wall texture 137, a dark archway) in the trigger's tile, here and at 27,20 (work\sound\objs.py lists a tile's objects,
tele.py a level's triggers to other levels). DRAWOBJ.C do_obj draws it as type 3: txtwal(0, sqmod, 4, owner) and
do_rect(0x16, o, -1, owner). rect_sub has no model for items 0x16E / 0x16F, so world_model_quads skipped them. Now
model 0x16 with the wall texture `owner`, set off the wall as the lever plates are. Seen: work\sound\l2-ours.png
against the game's own picture ledge-g-game.png (UWPORT_GDI=1), the arch on the same wall. Not done: do_obj's hilite
for texture types 3, 4, 8, 0xB.

## 2026-11-15: what the original draws and we did not (work\sound\audit.py)

The user: "find things we are missing to render". audit.py walks every object in LEV.ARK's nine levels, takes how
DRAWOBJ.C do_obj draws its item (COMOBJ.DAT's render type, rect_sub, the animation objects' owner, the rune stones)
and compares with world.c's rules. Four differences, all put right in world.c:
- **Item 0, the hand axe** (3 in the game; level 1's at 26,12): world_item_shown refused id 0. Seen: ax1500-ours.png.
- **Arrows and bolts (0x11, 0x12; 7 lying about, and every one shot)**: render type 2, so the original draws the 3D
  model 8 (rect_sub[item & 0x3F - 0x10], as 0x151 / 0x152); we drew the inventory's picture. Seen: ar0-ours.png.
- **Rune stones 0xE8-0xFF on the ground (13)**: do_obj draws them all as the one rune stone (0xE0); we drew each
  rune's own letter picture (the inventory's).
- **Wall panels 0x16D (112, all on level 9)**: render type 3 like 0x16E / 0x16F; now the same model 0x16.
Not covered by the audit: things only made in play (missiles other than these, spell effects), do_obj's hilite for
texture types 3, 4, 8, 0xB, the animated wall and floor textures, the picking (uwview.c's hover still takes an
arrow's picture size).

## 2026-11-15: all nine levels, ours beside the original's (work\levelcheck\levelcheck.py)

The user: "go through the levels and sprites and make sure they're showing up in the game correctly ... only one path
back to the old renderer, in the options, or if the engine fails". levelcheck.py: per level the 4 open tiles with the
most drawn objects round them, two views each, our window and the original's own picture (UWPORT_GDI=1); sheets
work\levelcheck\level-N.png (the script's "look" does not turn the original's display: only the as-landed pairs
compare). Walls, floors, water, lava, doors, creatures, items, corpses, pillars and wall panels agree on all nine.
- **Boulders (models 5-7)** were pale and flat: models.c's Gouraud surface now takes the palette entry base + shade
  as the original (0x64 + 0..0xE, the grey-blue ramp) instead of darkening the base. Still lighter than the
  original's beside our walls (our light on a light grey surface); not looked into further. For the Vulkan ledger:
  models.c only, no shader touched.
- **UWPORT_GDI** (uwfront.c view_start, plat_win32.c big_window_size) is taken only with EXHUME_TEST_CHEATS=1: a
  check's switch, not a player's. A player's ways to the original's picture: the Esc menu's Display row, and the
  renderer not starting (view_size). uw-original.exe / uw-original.cmd in dist are a separate program, left as is.
- **The lava's light 20% less** (the user, 2026-11-15): scene.hlsl's lava glow on what is round it, 0.3 -> 0.24. The
  lava's own picture (emit) is as it was. For the Vulkan ledger: scene.hlsl only (shared source), the one constant.

## The automap and its experience under wide-pitch (2026-11-15)
The user: "look at the automap and make sure it is sound"; then "yes fix it".
- Found (read, then measured): beyond the original's pitch (0x1000) the port draws the view in two passes with opened
  rays (VIEW3D.C port_view_passes), and GRIDDB.C grdb_elem wrote PlayersMap in both. So a steep look mapped squares
  behind and beside the player that the original's one pass never walks, a door behind lost its mark (0x41 -> 0x01,
  its objects are not sorted in the back pass), and pipeexp (the newly mapped squares: experience, pipeexp *
  PlayerLevel / 10 a frame, rounded down) differed. Measured before the fix: 1970 squares in 35 of 37 steep runs,
  experience e.g. 326 against 342.
- A second writer: a pick frame (do_3d_pickup, what is under the pointer) walks the last frame's grid again and maps
  as it goes; after a steep frame that grid is the pass ahead's.
- Fix (port only, the original's lines untouched): at a steep look the original's own pass (setup_vars, do_2dclip)
  runs first, for PlayersMap and pipeexp alone, and its queue is thrown away; the two passes, and pick frames while
  the grid is theirs (port_grid_wide, 0 again at do_2dclip), write neither (GRIDDB.C port_map_off). The drawn view is
  as before. UW1PORT_MAP_AS_DRAWN=1 (a check's): the old way.
- Proof, tools\mapcheck.py (hidden runs; test cheats pitch, turn, mapdump in port\sys\cheats.c): five places (level 2
  28,28 and 9,14; level 1 21,5 by a door, 6,42, 36,7), six headings each, pitch +-0x2800 against pitch 0 from the same
  start: PlayersMap the same 4096 bytes and the experience the same in all 60; pitch 0 twice the same (the control).
  Within the original's range (pitch 16, +-0x1000) nothing differed before or after.
- Not exact, said plainly: the original cannot look this steeply, so "as the original" means as its pass maps from
  the same place and heading; and at a steep look the original's pass is one more walk of the grid a frame (its
  objects sorted once more; no rand() in that path).
- Our map drawing (src\uwconv_draw.c uwv_map) was read against AUTOMAP.C line by line: patterns, shades, doors,
  sides, corners and order agree. On Linux uwv_game_file's "DATA\BLNKMAP.BYT" path will not open (M7's list).

## 2026-11-16: Vulkan renderer M7, the Windows side (libuwview.so, the Linux surface, fonts, NGX for Linux)
- tools\buildview_linux.py builds build\linux\libuwview.so: uwview_vk.dll's sources cross-built for x86-64 Linux (our
  clang + lld, the Debian sysroot) after tools\buildview.cmd (its build\gen shaders). -z defs (nothing unresolved),
  --exclude-libs (only the 52 uwv_* entry points exported), glibc 2.34 at most (as the program), libstdc++ 3.4.21.
  Left out: the standalone engine's sound (sound.c, sfx.c, xmidi.c, songs.c: the renderer calls none of them).
- src\os_posix.h, force-included by the Linux build only: Windows' names (MAX_PATH, _stricmp, GetTickCount64,
  CreateDirectoryA, extract.c's FindFirstFileA listing) and fopen through uwos_fopen, which turns '\' into '/' and
  finds each name of a path whatever its case (dist's names are lower case, the game's may not be: this is also the
  plan's uwv_game_file / BLNKMAP.BYT item). _DEFAULT_SOURCE, not _GNU_SOURCE (glibc 2.38's headers would send sscanf
  and strtol to __isoc23_*, which Mint 21 lacks); fmod / fmodf bound to their first symbol version for the same
  reason; dladdr declared in os_posix.c (uwos_self_dir).
- src\smoothtext_stb.c: smoothtext.h on Linux with stb_truetype (src\stb_truetype.h 1.28, public domain): the same
  glyph cells, metrics (em as GDI's CreateFont(-em), the line's height from OS/2 usWinAscent + usWinDescent) and
  blending. The faces keep their Windows names in the Esc menu and settings.txt (one home\ for both systems), each
  drawn from its free stand-in in freefonts\ (the user's yes 2026-11-15): Segoe UI Selawik, Verdana and Tahoma DejaVu
  Sans, Georgia Gelasio, Calibri Carlito, Cambria Caladea, Consolas Inconsolata (OFL / Bitstream Vera; licences
  beside them); else the system's DejaVu / Carlito / Caladea. uwconv_draw.c's conversation letters use it too
  (st_conv_glyph). Windows' smoothtext.c is unchanged.
- src\uwrec_posix.c: P's screenshot onto the clipboard as a PNG through SDL3; Shift+P's video is not on Linux yet
  (logged).
- render_vk.c: the window's surface on Linux from the SDL window the backend gives: X11 (vkCreateXlibSurfaceKHR with
  the window's X11 display and window) or Wayland, by SDL's video driver; X11's three types declared locally (the
  sysroot has no X11 headers). ngx_vk.c's Linux path links the DLSS SDK's libnvsdk_ngx.a; its Parameter Reset() is
  called through the Itanium vtable (slot 16: the C headers have no reset call). uwview.c's find_dirs on Linux: up
  from the library's folder to the one with dlss\ (NGX's search path, wide strings as on Windows).
- The program (port\win32\uwfront.c, port\sdl3\plat_sdl3.c): on Linux it loads libuwview.so beside it (the Vulkan
  renderer only); when it is missing or does not start, the original's picture through SDL (not D3D12).
- fable uwr's review of M1-M6, folded in: the HDR rebuild loop when the surface has no FP16 scRGB (remembered, logged
  once, cleared by render_set_hdr or a change of the monitor's answer); more than 8 swap chain images (clamped asked,
  an image index past 8 shows nothing); the stale "stubs" comments.
- makelinuxdist.py stages libuwview.so, freefonts\ and dlss\libnvidia-ngx-dlss(d).so.310.9.16 (the SDK's release
  builds, 108 MB) beside the Linux program; its readme says what works now.
- Proof: the Windows builds (buildview.cmd, portwin32.py) and the Linux ones (buildview_linux.py, portlinux.py) build;
  vkcompare --only title PASS (Vulkan vs D3D12 as the noise floor, validation 0, exit 0). WSL (Ubuntu 24.04, no NVIDIA
  Vulkan; nothing installed; O:\wsl\test\launch-vk.sh): WSL's Mesa offers llvmpipe (software Vulkan
  1.4, with ray queries), so the renderer started there: the X11 surface, the swap chain (1264x921, 3 images), NGX
  loaded and said "DLSS NOT available" (no NVIDIA: the three NVX / buffer-address extensions not offered), level 1
  built with the same counts as on Windows (16236 triangles, 16090 texture pixels in the cycling colours: the data's
  paths resolve), 1219 presents, exit 0; the window shot (work: O:\wsl\test\run-vk\window.png) the title screen as on
  Windows. With NOVK=1 (the loader given no driver file): "render: Vulkan without VK_KHR_surface", "the Vulkan
  renderer did not start: the original's picture", the game on through SDL's renderer, exit 0.
- For the ledger: no D3D12-side change (render.c untouched by M7).

## 2026-11-16: the game is named uwr
The user: "Let's rename the game to uwr" (the long joke name, Ultima Underworld Rekonfigmasturfication, is for an ANSI
splash screen shown for 2 seconds at the start, still to be made). The rename was made (46c9f05), taken back on a
misread "the filename can stay the same" (8b3c19e), and made again on "switch the game to uwr like we had it ... you
can get rid of the old files".
- The names a player sees: dist\UW1\uwr.exe (was uw-new.exe), uwr and uwr.sh (Linux), uwr.cmd, uwr-vulkan.exe, the
  log home\logs\uwr.log (the backends' own and the start scripts'), the window's title "uwr" (underworld-exhumed's
  port\sys\main.c), the Vulkan application name, the readmes' first lines. play.cmd keeps its name and starts uwr.exe.
- Kept: the folders (uo-new, dist\UW1), the git branch uw-new, uw-original, the sources' and DLLs' names, the log's
  "uw1port:" prefix. vkcompare's "is the game running" looks for both names.
- Removed with the user's word: the old-named files in dist\UW1 (uw-new.exe, uw-new.cmd, uw-new, uw-new.sh,
  uw-vulkan.exe), release\linux\uw-new and uw-new.sh, build\uw-vulkan.exe. The old log home\logs\uw-new.log is kept.
- Checked: all four builds; makedist; uwr.exe starts, its window's title reads "uwr", the log is uwr.log.

## 2026-11-16: for the release, our textures in zips (the user's plan; NOT started, waits for the user's test)
The user: "I'd rather our textures be in some sort of zip and it is opened by the exe and read at the start. So that
the user doesn't have thousands of files laying around. The game can read the original files then the zips. Also we
need the zips to be like texture256 and texture512 with the right textures in them with their ao and depth." And:
"for when we release we will move to this once I test the higher res" textures.
- When: at the release, after the user has tested the higher-resolution textures. Until then the loose files stay.
- The exe reads the ORIGINAL game files (game\) first, then the zips, at the start. No folder of thousands of loose
  files for the player (data\ is about 20,000 files today: about 5,700 decoded copies of game\'s own data, 44 MB; our
  upscaled textures 524 files, 50 MB; our relief maps 13,754 files, 166 MB).
- One zip a texture size, named like texture256 and texture512, each holding that size's wall and floor textures
  WITH their own maps: the occlusion (AO) and the depth (height), and the normal maps that go with them
  (data\upscaled\walls|floors\256|512 and data\laigter\upscaled\walls|floors\256|512 today).
- Still to settle when it is built: where the 64-texel maps and the sprites' and interface's maps (data\laigter\walls,
  floors, sprites, ui: 13,000 files) go (a zip of their own is the plain answer); whether the decoded copies of the
  game's own data are decoded in memory at each start or kept in one cache file (measure the start time first);
  the zip reader (a small one of ours over png.c's inflate, stored or deflated entries, nothing to install).
- This replaces the idea of the same day of writing them as files in the game's own formats (.TR): a zip it is.
- The user, the same day: "I need access to the raw files until we are done." So the loose files (data\upscaled,
  data\laigter and the rest) stay as they are, readable and editable, for all the work before the release; the
  zips are made only as the last step, from those files, and the loose files are not removed from uo-new\data.

## Creature AI levels: Original, Devious, Monstrous (2026-11-16)

Spec docs\superpowers\specs\2026-11-16-creature-ai-levels-design.md, plan docs\superpowers\plans\2026-11-16-creature-ai-levels.md.
The user set the fine-tuned model aside: the rulebook the model was being taught is now code, so no model is needed.
- Setting creature_ai = 0 Original, 1 Devious, 2 Monstrous (Esc menu "Creature AI"; port_ai_level in cunning.c). It
  drives ENH_SMART_HUNTING (on for 1 and 2), which every hook still tests. main.c's start no longer needs model files
  for the flag; with no creature_ai line the older smart-hunting setting decides (absent = on = Devious).
- port\sys\cunning.h / cunning.c (new): the settings, each creature type's style (cun_trait: pack_need, nerve8, calls,
  kites, relays), and rules 1 to 9 as pure functions (cun_seen, cun_lost, cun_sensed, cun_friend, cun_plan,
  cun_reconsider). hunt.c and hunter.c call them where they used to toss a coin or wait for the model.
- "Decisions by" (ai_by_model, default 0 = Rules): on Language model the model is loaded and asked as before and its
  answer replaces the rules' when it comes. With Rules the model is never loaded (uwfront.c's start, hunt.c's mind_on).
- Level hunters run without a model (hunter.c plan_rules: the plan-step policy of O:\appsnotonc\finetune\tools\hunter_policy.py).
- port\sys\pack.inc (new, included by hunt.c): Monstrous's parts, each with an Esc row (ai_part_0..6): hit and run,
  calls for help, pack courage, fall back when hurt, strike together, calls passed on, AI messages. Sub-modes of goals
  13 to 15 in HuntState (mode, PM_*); no new goal value, no new hook in AI.C, nothing saved.
- The call for help has no sound yet (UW1 has no creature voices): a message line only. Our own Roland timbres for
  the calls are the next part (spec, "Later part"), each to be heard by the user first.
- Proof (the user's rule of 11-15): builds, and the port starts and exits clean in a hidden run. The behaviour is
  NOT checked: the user judges it in play with Ctrl+L (lines "the rules choose ... (rule N)", "calls for help",
  "pack courage", "hit and run", "strike together"). The trait numbers are first guesses to tune.
- Backups of the files as they were: work\*.before-ai-levels-2026-11-16.

## 2026-11-16: one folder, the game's own files not extracted

The user: "make the game work out of one directory with the original game files not extracted".

- **Nothing extracted onto the disk.** src\base.c keeps files in memory: `file_memory_root(dir)` names a root;
  `file_write` under it keeps the bytes (a hash table, heap), `file_read` / `file_exists` look on the disk first (a
  file there is someone's own and wins) and then in memory, `make_dirs_for` makes no folder under it. uwview.c's
  level_build names our data folder as the root before `extract_missing`, so the 5,746 loose files (31 MB: textures,
  sprites, levels, strings, the interface) are made in memory at every start (about 2 s) and never written.
  `UWVIEW_EXTRACT_TO_DISK=1` writes them out as before (make-normals.cmd and the sheets need the loose PNGs; the
  project's own data\ still has them). Two readers that opened files themselves now go through file_read:
  level.c terrain_load, uwview.c read_light_table. The program's own audio (port\win32\uwaudio.c) read
  data\sound\sounds.dat and uw.mt: it now falls back to the game's SOUND\ (uwaudio_game_dir).
- **A flat folder.** When the program is run bare beside the game's own UW.EXE and there is no game\ folder
  (underworld-exhumed uw1\src\port\sys\main.c), that folder is the game and the home is uwr\home. Our own files
  (settings.txt, acoustics.txt, laigter\, upscaled\) are then in uwr\ and not data\, so that nothing of ours lands
  in the game's DATA\: uwfront.c `uwf_our_data` (the program, the Linux menu) and uwview.c `data_dirs` (the
  renderer) make the same choice. The old layout (game\ + data\ + home\, dist\UW1) works as before.
- `tools\makeflat.py` builds dist\uwr (copies only; dist\UW1 untouched; llm\ only with --llm).
- Proof: builds (Windows program, both renderers; the Linux program and libuwview.so cross-built); a hidden bare run
  of dist\uwr\uwr.exe loaded the saved game with the Vulkan renderer and wrote only under uwr\home
  (work\onedir\flatrun.py, mem.py). NOT run: the audio fallback (a hidden run has the effects' audio off), Linux, the
  D3D12 renderer in the flat folder. Known gap: the Linux menu without the renderer (uwmenu.c) has no extracted
  interface pictures unless data\ui is on the disk. dlss\, bin\, roms\, freefonts\ are still folders of the top level.

## 2026-11-16: the relief maps made on the first run; the public package

The user: "We cannot include the upscaled textures" in a public download, and: can the game make the normal, AO and
depth maps on the first run "with the laigter tool, with the exact settings we used", for Linux and Windows.

- **tools\laigter-cli --uwr / --uwr-game** (uwr_maps.inc, part of the GPL program): every map in one run. The walls'
  and floors' two Python steps (upsize_normals.py, make_soft_maps.py variant A) are ported into it. --uwr-game reads
  the player's own game files with src\extract.c (unpacked into OUT\laigter\unpack-tmp, removed afterwards).
  Windows: tools\laigter-cli\build.cmd; Linux: tools\build_laigter_linux.py (build\linux\laigter-cli).
- **src\mapsmake.h** (uwview.c level_build, after extract_missing): when <data>\laigter has neither made.txt nor
  walls\000_n.png and <root>\laigter\laigter-cli(.exe) is there, the game runs it and waits. Our own dist\UW1
  (maps on the disk, no made.txt) is left alone. In a flat folder the maps go to uwr\laigter (data_dirs).
- **Proof** (tools\cmp_map_pixels.py): 13,227 maps, 0 pixels different from data\laigter, by --uwr, --uwr-game, the
  Linux program under WSL, and the game's own first run on Windows (hidden: "first run: making the relief maps",
  "made", level 2 loaded; a second start makes nothing), and the Linux game's own first run under WSL Ubuntu
  24.04 (work\announce\linux_firstrun.sh: the package on the distro's own disk, the same two lines, level 2
  loaded, llvmpipe). Not run on a real Linux Mint machine.
- **tools\makepublic.py NAME** -> release\NAME\uwr: the public download from dist\UW1 by named files only, with
  laigter\, the texts of tools\public\ and VERSION.txt. No game\, roms\, home\, llm\, data\ (so no maps, no
  upscaled textures, nothing from the game's art), no nvngx_dlssnr.dll. It refuses uncommitted changes and anything
  that looks like a game file, a ROM, a model or a picture. About 330 MB, of which DLSS 206 and Steam Audio 93.
- Nothing is uploaded: a GitHub release needs the user's yes each time (a draft made 2026-11-16 was deleted on
  their word). Announcement drafts: work\announce\ANNOUNCE-DRAFTS.md.
- **The clean build of 2026-11-16** (uo-new 6fe669e, underworld-exhumed 45a30dd; the working tree clean): every
  program made again from the committed source (portwin32.py, laigter-cli's build.cmd, build_laigter_linux.py,
  portlinux.py, buildview_linux.py, makelinuxdist.py, makedist.py: logs in work\announce\clean), dist\UW1 refreshed,
  release\uwr-preview-2026-11-16d\uwr made by makepublic.py. Checked: dist\UW1\uwr.exe starts and loads a saved
  game (hidden, a home of its own; its own maps left alone); the package's first run on Windows and under WSL; no
  file of the package is the same as any file of game\, roms\, dist's data\ or home\ (work\announce\scan_package.py:
  16,649 files hashed, 0 the same), and it holds no picture.
- **uw-original.exe no longer links** (`portwin32.py --original`: port_hunter_setting, port_sound_volumes and other
  port_* of uw-new are undefined in the untouched tree): dist's is still the build of 2026-11-10, and the public
  package leaves it out. Open.

## 2026-11-16: the sound, "this twinge at the end", every number a row, Steam Audio for the sound itself

The user: the acoustics "do not seem right" and "have this twinge at the end"; then, quoting Steam Audio's own list
("real-time and baked ray-traced sound propagation, physics-based occlusion, transmission, reflections, pathing,
and HRTF binaural rendering"): "all of this needs to be in the game ... one of the biggest selling points ... real
and realistic with sounds changing their form based on whats in the room". Probes and logs: work\acoustics\twinge.

- **The late echoes (commit 2270e77).** In Acoustics High a voice's echoes were silent until the rays had run once
  for it, and the head of the sound was thrown away meanwhile. One run by live voices 1 / 6 / 8: Echo detail 1
  18 / 57 / 70 ms, detail 2 52 / 144 / 175 ms, detail 3 169 / 434 / 526 ms; a voice stayed live for the sound plus
  2 s, so a walk kept six live. Echo start after the sound (onset.c, the game's two threads): detail 2 with five
  others 203 ms, detail 3 651 ms. Now: a ringing voice leaves the simulation, and a response for the listener's own
  place is kept ready (uwsteam.c S.ear; made again after a quarter tile, a level change, or while a sound is live)
  which a new sound uses until its own is made: 3 ms and 18 ms. No clicks at the change (hf.py on a 100 Hz burst).
  Not the cause: IR_SECONDS (the ring ends when the bounces run out: the hall's at 0.45 s with 16, 0.9 s with 32,
  1.1 s with 64), the voice steal (nine sounds in 3 s).
- **From the uwr session's review (work\review\agents\tail), taken in, not measured again here:** the echo stream
  as float (it was 16 bits after the 0.12 level: echoes 12 to 93 steps tall, fizz as they died), four blocks ahead
  (two ran dry; hi_stop logs "the echo stream ran dry N times"), AL_DIRECT_CHANNELS_SOFT on the stream (it went
  through OpenAL's HRTF a second time; echoes about 2.6 dB up since), the HI.idle race (HI.kick).
- **Every fixed number a row** ("Sound tuning" in the Esc menu; uwfront.c SOUND_DIALS, ids 200+, ours, written to
  settings.txt by fx_setting_save; uwaudio.c's comment above TU lists every key with its default; tune_read reads
  them). Each default is the old number. "Room size" is "Map square size" (2.4 m is right: the eye at 0xA4/256 of
  a tile is 1.54 m, the start corridor's headroom of 4 steps is one tile; render.c's TILE_M 1.2 is the wrong one,
  the uwr session's to deal with). "Room measured here" (read-only): uwaudio_room, uwac_room at the listener.
  MENU_MAX 160 (and src\menu_draw.c's M[]).
- **Stage 1 of Steam Audio for everything: the sound itself** (uwsteam.c D, direct_run, block()). With Acoustics
  High and HRTF on, each voice's samples go through Steam Audio's direct effect and binaural effect into the
  stream beside the echoes; our OpenAL source of that sound plays at gain 0 (its bookkeeping stays). A second
  simulator (direct only, its sources always added) runs every block on the audio's side under S.scene_lock
  (scene_build takes it for the mesh swap). Occlusion volumetric (source 0.5 m, 16 samples), transmission 8 rays,
  by the materials of acoustics.txt. Our way round a corner stays: when it raised a sound's gains they are a floor
  under Steam Audio's and the sound is heard from the opening (uwst_direct, hi_push). A sound's place is lifted
  0.3 tile off its floor (uwac_lift); the player's two feet are a third of a tile to each side. A footstep's echo
  share is uwst_echo now, not in the samples. Rows: Sound by Steam Audio, Muffling by Steam Audio, Sound level
  (Steam Audio) 30 %, Sound source size 50 cm.
  Measured (direct.c, uwsteam.c alone): Steam Audio's HRTF at unity (at the listener 0.0 dB, ahead -1.6, east
  +1.4 with left 0.03 / right 0.24, behind -3.2), a sound behind the rock silent. Worked out, NOT measured: the 30 %
  (unity HRTF x the stream's 5.2 makeup against the game's mix at 1.55 of a unity sound). Compiled and launched
  only: all of it inside the game (fxcheck steam acoustics 42 PASS, 0 FAIL: a hidden run in High, clean).
  Known: sounds past the 8 voices stay OpenAL's (another HRTF, another level); a sound cut by the game (FX_STOP)
  stops between two blocks (a possible click); about 45 ms of stream delay on the sound itself; speakers (no HRTF)
  keep OpenAL's sound.
- **Next stages (the user's list):** (2) the room's contents in the scene: the door's real opening (frame and
  lintel always, the leaf when shut; uwacoustic.c 141-148, uwsteam.c level_mesh; src\world.c 123-125 has the
  sizes), pillars, furniture and other models as boxes with materials, so a room's sound changes with what is in
  it; (3) baked: probes per level (iplProbeArrayGenerateProbes), baked reflections and Steam Audio's pathing
  (iplPathBakerBake, iplPathEffect), made in the background on a level's first visit and kept in our folder;
  (4) the EFX mode's reverb level by room (the uwr session's item), Eyring for Sabine.
- menucheck acoustics: 2 FAIL of its own ("off by default"): build\port\data\settings.txt has fx_acoustics 1.

## 2026-11-16 (evening): the flat folder stands alone

fable uwr's review of dist\uwr found it reading the ROMs from dist\UW1 (the home copied over carried
`mt32-roms=O:\...\dist\UW1\roms` and `data=...\dist\UW1\game` in uw1port.cfg). Fixed:

- underworld-exhumed `uw1\src\port\sys\main.c`: `flat_bare` (run bare in the flat folder). Then the game is the
  program's folder and the ROMs its `roms\` (given, so they win over a remembered folder), and neither `data` nor
  `mt32-roms` is written to uw1port.cfg: the folder can be moved or copied. One new log line at every start:
  `uw1port: the game ..., the home ..., the ROMs ... (given|remembered|found)`.
- `src\uwview.c`: one log line `uwview: the game's own files in ...; ours in ...: settings.txt there, ...`, and
  the time of the in-memory making on the "files read" line.
- `tools\makeflat.py`: the player's own (uwr\settings.txt, keys.txt, acoustics.txt, lights.txt, uwr\home) are given
  once and never written over; the home is taken from dist\UW1 only when the flat folder has none; uw1port.cfg goes
  without `data=` / `mt32-roms=` (and they are taken out of an existing flat one).
- A one-file cache of the in-memory files (uwr\cache.bin) was built, measured and taken out: making the 5,746 files
  costs 0.38-0.42 s at a start; the cache saved 0.2 s (65 files are made at every start whatever is there) and had
  to be rewritten for each renderer's build. Not worth a file that can go stale.

Proof (work\onedir\alone.py): dist\uwr copied to work\onedir\alone\uwr, run hidden and bare from there on Vulkan
(alone6.err) and D3D12 (alone7.err): exit 0, the save loaded on level 2, every path in the log inside the copy, no
line naming dist\UW1 or dist\uwr, the only files written under uwr\home (save0, a recording).

- The original's installer files (UINSTALL.EXE, UWSOUND.EXE, LHA.EXE, LHA.DOC, INSTALL.DAT, INSTALL.OLB) are read by nothing of ours
  (no source names them) and makeflat.py no longer copies them (the user: not needed in our package); UW.EXE stays, its
  models and tables are read. A folder made without them (makeflat.py --out work\onedir\noinst\uwr) runs on Vulkan and
  D3D12: work\onedir\noinst1.err, noinst2.err.

## 2026-11-16 (night): Steam Audio's pathing; where baked propagation would fit

The user: "we want pathing from the steam audio", "and figure out where Baked propagation would work".

- **Pathing (port\win32\uwsteam.c, the comment above P; uwsteam.h uwst_pathing).** A probe in every open tile
  (probes_place: over the middle of its floor, 0.6 tile up, reach 1 tile), the ways between them baked by
  iplPathBakerBake in a thread of its own (bake_worker) with every door open (only the frames stand in the baked
  scene), once a level: path_service on the simulation's thread starts it when level_key (tiles' shapes and heights,
  the doors' places, the metres a tile, the frames' row) is not the one in use, and puts the result in the direct
  simulator under S.scene_lock. The direct simulator and its sources carry IPL_SIMULATIONFLAGS_PATHING;
  direct_run runs iplSimulatorRunPathing with it every third block (32 ms) and at once for a new sound, with
  enableValidation and findAlternatePaths against the scene as it is (a shut door's leaf); block() plays each
  sounding voice through iplPathEffect (binaural, first order) beside its direct effect.
  - Our own way round a corner (uwac_path_round / uwac_waypoint: v->heard and the floor under the gains) stands
    back for a voice that has Steam Audio's paths; it stays while a level's bake runs, with the row off, on
    speakers, and for the sounds past the eight voices.
  - The paths' share is 1 - Steam Audio's occlusion, so a sound in plain sight is not heard twice.
  - The level: Steam Audio's coefficients carry 1 / the way's metres (measured: the first is 0.2821 / metres);
    the samples already carry the game's falloff by the straight line, so the paths play at straight / way of that
    (never above it). Eased over a block.
  - Found: iplPathBakerBake 4.8.1 crashes on a NULL progress callback (bake_progress); a band's gain of 0 into
    iplPathEffect gave NaN from then on (never under 0.001); `far` is a macro of windows.h.
  - Rows (Sound tuning, shown with Acoustics High): "Sound paths by Steam Audio" (acoustics_steam_pathing 1) and
    "Sound paths level" (acoustics_path_level 100). Needs Sound by Steam Audio and Muffling by Steam Audio, so
    headphones.
  - Measured, uwsteam.c alone (tests\uwsteam_path_test.c; work\acoustics\pathing\build.sh builds and runs it, 4 PASS):
    the fixture's 161 probes baked in 0.04 s, level 1's 2036 in 0.4 s (so nothing is kept on disk; a file cache was
    written and taken out). A sound in room A, the listener in B out of sight, the door open: left 0.143 / right
    0.113 with the paths (the doorway is to the west), 0.0003 without; bands' gains 0.74 0.41 0.24. In plain sight
    through the doorway: 0.342 / 0.200 with and without. Further along B (25,14 / 25,17 / 25,20): 0.24, 0.20, 0.16.
    The door shut: no way (0.0003). Level 1, seven sounds within 6 tiles: a block 2.51 ms with the paths, 2.26
    without (a block lasts 10.7 ms).
  - Compiled and launched only, in the game: Windows hidden on Vulkan with the Roland effects and the loopback
    (work\acoustics\pathing\run1): exit 0, "fx: sound paths ready (Steam Audio's pathing: 1995 probes, baked in
    0.3 s)" on level 2, 71 sounds, no echo stream run dry. Linux: compiles and links, starts under WSL and exits 0
    (work\acoustics\pathing\linux-run.err), but that WSL has no OpenAL ("fx: audio off"), so no sound went through
    the paths there. NOT heard by anyone yet: the level against the straight sound and the 32 ms step are worked
    out, for the user's ear.
  - That build held another session's uncommitted door work in underworld-exhumed (COLLIDE.C, EFFECT.C,
    USEITEMS.C): dist\UW1 has it too.
- **Where baked propagation would fit (an agent's read of the code and phonon.h, nothing built):**
  - Pathing is the part of Steam Audio's baking that pays here, and it is in.
  - Baked reflections: every sound in the game is a one-shot at a moving or momentary place (no fixed emitters:
    uwaudio.c starts each from play_effect), so the STATICSOURCE and STATICLISTENER variations have no use. The one
    fit is a REVERB layer (source and listener at the same probe) for the listener's own place: the response a new
    sound rings with until its own run is made (S.ear), the player's own sounds, and a ring for the sounds past the
    eight voices, which are dry today. Our pipeline is convolution through a mixer, so it would be
    IPL_REFLECTIONSBAKEFLAGS_BAKECONVOLUTION into the pathing's probe batch, in.baked on the ear source alone.
  - What a bake of echoes cannot follow: doors (each open or shut one changes the room), tiles changed by
    triggers, and the rows that change the scene (Map square size, scatter, materials, echo length, Echo detail):
    a key over all of those, the doors taken as open.
  - Cost, worked out from the measured runs (18 / 52 / 169 ms a place at Echo detail 1 / 2 / 3), not measured:
    about half a minute to four minutes a level of 2000 probes, in the background on a first visit, and a file a
    level (Steam Audio keeps energy fields, in the tens of megabytes).
  - Real-time stays for every placed sound. The smallest step: the REVERB layer for the ear, behind a row, heard
    against the real-time one.

## 2026-11-16 (late): the Esc menu's descriptions, the menu guide, the music's volume after off and on

- **Descriptions.** The foot of the menu had ONE line for what a row does, and a hint too long for it lost its
  middle ("..."): 29 of the 117 rows were cut on screen. Two changes: src/menu_draw.c's menu_wrap breaks the hint
  at its spaces over up to three lines (FOOT 27 -> 46 overlay pixels, both fonts; a last line still too long ends
  in "..."), and all 116 hints of uwfront.c's MENU_ROWS were written again (an agent's work, read against the
  code): what the row does, its values and range, what it needs, the default. Labels, ids and order unchanged.
  The dump's foot2 line now joins the hint's lines with " | ".
- **docs/ESC_MENU.md** (new, linked from the README): every row of the menu for a reader who has not seen the
  code, heading by heading; the three levels of the sound (Positional sound, Acoustics On, Acoustics High) and
  which tuning rows belong to which. The Fog row is left out (not built).
- **Music off and on again played at full, and the Music volume row did nothing in the menu** (the user's
  report). Two causes. (1) SOUND.C's turn_music starts the sequence again (AIL_start_sequence), which rewinds it:
  the relative volume back at the driver's default and each channel's volume forgotten, and the original sets
  none there. Port only: port_mus_dirty set, so the menu's volume is applied again. (2) The volume is applied by
  the effects' 16 Hz timer, and with "Music in the menu" on the game's timers stand still while the menu is open
  (ail.c's music_only), so a step of the row was only heard after the menu closed. port_music_volume_poll
  (SOUND.C) is now also called from ail.c's api_timer while music_only (the same thread as the timers).
  Compiled and started only: NOT heard by anyone yet.
- tools/menucheck.py: its list of headings had no "Sound tuning" (two FAILs that were the check's own); added.
  `menucheck.py smooth original`: no FAIL after. A long hint seen wrapped in both fonts
  (work/esc_hints/wrapshot.py; pictures work/menucheck/wrap-*-w.png).
- Built: buildview.cmd, portwin32.py, buildview_linux.py, portlinux.py; makedist and makeflat run.
- **Menu background** (the user: "a solid color so it is easier to see"): the menu's panel is one dark brown with a
  gold line round it (menu_draw.c md_solid, the default); the row Menu background (opt 63, settings.txt
  menu_background solid / leather) gives the leather back. Seen in a hidden run (work/menucheck/wrap-smooth-w.png);
  all four builds compile.
- **The log says what a start is doing** (the user: "on first launch the log should show what it is doing ... like
  making the textures or loading things ... a logline for everything", and: no window of its own, "the log should
  go to the console the rest of the logs go to"). All in the one log (home/logs/uwr.log, or the terminal / a check's
  pipe): a line BEFORE each slow step (starting the positional sound, starting the renderer, settings read,
  reading the game's files, reading the 3D models, building level N, working out the sound paths), and the map
  maker's own lines: laigter-cli --uwr / --uwr-game now names each stage and gives a running count every tenth
  (uwr_maps.inc, laigter_cli.cpp's uwr_progress), and mapsmake.h hands it our stderr (Windows: the handle
  inherited; Linux: dup2), with the seconds at the end. The maps are now made at the renderer's start (uwv_init,
  before the title) and not at the first level; level_build still asks (one run a start: `tried`).
  Seen: a hidden first start of a flat trial folder without maps (work/esc_hints/firstrun.py -> firstrun.err):
  100 "uwr maps:" lines, "the relief maps: made, 11 s", made.txt there, level 2 built, exit 0. The maps' pixels
  were not compared again (only printing was added). A console window of its own was drafted and dropped on the
  user's word. All four game builds and both laigter-cli builds compile.
- **The log's lines in the terminal too** (the user: started from a terminal, a few lines show there and then
  nothing; the start's lines should go there). plat_win32.c log_start: when stderr is a console, its handle is
  kept (duplicated before freopen), and a thread copies what is added to the log file to it every 50 ms, with a
  last copy at exit (term_start, term_copy). The file is still written straight. Windows only (elsewhere the
  lines already go to the terminal). Compiled; NOT seen by us: a hidden run has no console and skips log_start,
  so it needs the user's own start from cmd / PowerShell / Windows Terminal.
- **The same on Linux** (2026-11-17, the user: "this works in Linux too?"). It did not: plat_sdl3.c's log_start
  also sends everything to the file (the note above, "elsewhere the lines already go to the terminal", was wrong).
  Now, when stderr is a terminal (isatty), it is kept (dup before freopen) and a thread copies the file's growth
  to it (term_start, term_copy; SDL thread and mutex). Seen in WSL: started through `script` (a pseudo terminal)
  under Xvfb, the terminal got all 24 lines the log file got (work\esc_hints\termlinux.sh). That trial start
  ended after 2 s with the game's own "Error code D004", with and without a terminal (termlinux_pipe.sh), so not
  from this; not looked into (the trial's home and missing ROMs are the first suspects).
- **The Linux game checked as a player gets it; error D004 at start found and fixed** (2026-11-17, the user: "make
  sure the Linux client works"). Trial: the public package (makepublic, release\linuxcheck-2026-11-17) copied onto
  ext4 in the WSL Ubuntu, the original's files in game/ under their upper-case names, hidden under Xvfb
  (work\esc_hints\linuxplayer.sh). First start: relief maps made (2.3 s), title; second start: saved game loaded,
  level 2 built; with Debian's libopenal.so.1 beside it the positional sound, Steam Audio and HRTF come on.
  - **D004** ("a .GR file failed") about one start in four, on both file systems. Cause: the runtime's sys\borland.c
    keeps a text-mode mark per handle NUMBER, set only by open_host and never cleared at close; bc_fopen never set
    it. A picture file opened as a stream on a number that had been a text-mode handle was read through bc_read
    (LOADGR.C reads fileno(grfp)) with CR LF and Ctrl-Z translated, came up short. Which number a file gets depends
    on what the renderer's and sound's threads have open, hence now and then (never under strace or -v, which slow
    the game thread). Fix: our own copy underworld-exhumed\uw1\src\port\sys\borland.c (as ail.c, pit.c; excluded in
    portwin32.py and portlinux.py), marks cleared in bc_close and in bc_fopen. Proof: 20 of 20 starts pass where
    1 of 5 (ext4, sound on: linuxrepeat.sh) and 2 of 4 (drvfs: d004c.sh) failed. The same code is in the Windows
    game (rebuilt, launches). LOADGR.C now logs "gr: <file>: ..." when a picture file fails (port only).
  - Quitting while the pathing bake still ran crashed in a libphonon bake thread (linuxexit.sh: 3 of 4 under
    strace). Fixed by the sound session the same day (bake_stop waits the bake out, never cancels); with it 6 of 6
    quits are clean. That change is the sound session's to commit.
  - Not run: a new game from a fresh home (the old key script no longer reaches it), a real Mint machine, a real
    graphics card (llvmpipe here). strace was installed in the WSL Ubuntu for this.
- **The Display row is the one switch between the original and everything of ours** (2026-11-17, the user: the
  original "changes everything back to it and turns off our enhancements and the key binding changes to the old way
  too", ours "changes to everything we've done"). Before, the original's display took back only the keys and
  mouse-look; our renderer still drew its 3D window and the sound, creature AI, door helpers and quick fades stayed.
  Now (uwfront.c, display_apply and its neighbours) the original's display means: enhance_on 0 (smart-hunting with
  it, so Creature AI reads Original and hunters and friends are off), the game's own 3D picture
  (uwg_gameview_replace(0); view_present shows the picture whole, full_live kept so the menu still opens), the
  positional sound shut down (fx_audio_start returns; SOUND.C plays its own, effects interrupt music), quick fades
  off, the three door helpers off. What was on is kept in memory and comes back with ours; nothing is written to
  the settings but "display". A replay keeps its enhancements. In the original's display the menu shows only the
  rows that still do something. Not turned off: the renderer still runs behind the original's picture (it presents
  the screen and draws the menu). Hidden runs (work\esc_hints\origcheck.py, my build area): to the original's and
  the picture is the 1992 screen with its own 3D view; back to ours and the full-screen view and the sound return;
  a start with display=original starts so. Not tried by hand: the original's keys in play.
  Linux the same day (the user: "make sure this works on both"): portlinux, makelinuxdist, makedist; in the WSL
  Ubuntu, the trial package on ext4 with the new program (linuxnew.sh, then linuxorig2.sh, hidden under Xvfb, the
  keys by UWPORT_MOUSETEST with UWPORT_FOCUSTEST for the focus): to the original's, the 1992 screen with its own
  3D picture and the positional sound stopped; and back, our full-screen view and the sound started again. Both
  exit 0. (From dist/UW1 on drvfs with the user's settings the game never left the title under llvmpipe, with or
  without this change's keys: three wasted runs, linuxorig.sh; use the ext4 package.)

## 2026-11-19: the code made readable; the AI log row's own id
- Our own uwr code (never the rules) was made easier to follow: long functions split into named steps, plain
  comments, docs\CODE_MAP.md. Plan and spec in docs\superpowers. Every log line of ours goes through
  port\win32\rlog.c (`[time] level tag: text`); the Esc menu's Game tab has a Log detail row (uw1port.cfg
  log_detail), UWR_LOGLEVEL wins at start. Lines check scripts match by their start stay plain.
- Fixed: the "AI log in the log file" row had the id 167 (it came after MO_AIXEND), the same as the first
  enhancement's (MO_ENH + ENH_SKIP_INTRO), so stepping it toggled Skip the introduction and ai_log_file could not
  be changed from the menu. It is now MO_AILOGFILE = 129 (uwfront.c). Checked: Windows and SDL3 build, hidden launch.
