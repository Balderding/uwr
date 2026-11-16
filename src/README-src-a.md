# src guide, part A

This guide covers the following files in `src\`: audio, base, check, collide, conv, core_dos, core_glue, core_world, extract, game, gr,
imath, inventory, level and lights. It is a map for reading the code. It does not replace the comments in the code, and
it does not change any rule. Line numbers are those at commit time (git HEAD of uo-new, 2026-11-13).

## How these files fit together

The port has two layers. This guide keeps them apart:

- **The rules core.** It is underworld-exhumed's byte-matched C, compiled unchanged by `tools\core.py` into
  `build\core\core.a`. These files belong to the core's side and are built the same way, with compat.h
  force-included: `core_glue.c`, `imath.c`, `core_dos.c` and `core_world.c`. (`tools\core.py` line 37:
  `GLUE = ['core_glue.c', 'imath.c', 'core_world.c', 'core_dos.c']`.) They supply what the core needs from outside:
  - Borland's C library
  - the integer maths
  - DOS memory quirks
  - a plain C face for the game (`core_world.h`, prefix `cw_`)
- **The front end (our engine).** Everything else here is plain C on our own types (`base.h`). It includes:
  - the loose-file extractor (`extract`, `gr`, `audio`, `conv`)
  - the level as text and as triangles (`level`)
  - lights (`lights`)
  - the game layer: HUD, panels, and our stand-in rules where the core is not used (`game`, `inventory`)
  - a proved port of UW.EXE's collision segment (`collide`)
  - the self-check (`check`)

When the rules core runs (`cw_running()`), `game.c` and `inventory.c` hand the rules to it. They only mirror its state
for drawing and pointing. When it does not run, they use their own logic, which is older and partly estimated. Many
functions have both paths, and the guide says which is which.

Build: `build.cmd` line 25 puts base, gr, audio, conv, extract, level, lights and collide in `COMMON`. `check.exe` is
`src\check.c` + `COMMON` (line 46). `tests\rtest.c` also links `collide.c` for its emulator proof (line 29).

---

## base.c / base.h: the engine's basics

**Purpose:** the shared ground for every front-end file:
- fixed-size integer types (`u8` .. `i64`)
- `COUNT` / `MIN` / `MAX`
- little-endian readers `rd16` / `rd16s` / `rd32` / `rd32s` (base.h:23-26)
- one memory arena
- whole-file reading and writing
- logging
- small string helpers
- FNV-1a hashing for checks against the Python tools (base.h:58-70)

**Main functions:**
- `arena_init` (base.c:11): `malloc`s the one arena. Calls `fatal` if that fails.
- `arena_alloc` (base.c:18): bump allocator. Aligns to 16 bytes, zeroes what it returns, and calls `fatal` when the
  arena is full. Nothing is freed one block at a time.
- `arena_mark` / `arena_reset` / `arena_used` (base.c:26-28): the pattern used everywhere is
  `mark = arena_mark(); ... arena_reset(mark);`. Temporary file reads are dropped this way.
- `file_read` (base.c:30): reads the whole file into the arena, with a spare byte (`n + 1`). Returns `size 0` when the
  file is missing or empty.
- `file_write` (base.c:50): if `g_write_hook` is set, the bytes go to the hook instead of disk. `check.c` uses this to
  compare the extractor's output without writing it.
- `file_exists` (base.c:58), `logf_` (base.c:64: stdout, flushed every call), `fatal` (base.c:72: prints, then
  `exit(1)`).
- `str_ieq` (base.c:83): ASCII case-insensitive compare. `str_copy` (base.c:93): bounded copy that always ends with a
  NUL.

**Declared here, defined in `png.c` (not in this guide):** `root_dir`, `path_join`, `make_dirs_for`, `png_write`,
`png_write_indexed`, `png_read` and the `Image` struct (base.h:72-85).

**Depends on:** the C library only.

**Note:** `core_glue.c` also defines a function called `str_copy`, with a different signature (MODEX.ASM's). The two are
never linked together: base.c is front end, core_glue.c is core.

---

## audio.c / audio.h: music and speech

**Purpose:**
- converts the game's `.XMI` songs to standard MIDI and `.VOC` speech to WAV (the extractor uses both)
- plays the playlist (`data\music\playlist.txt`) on its own thread through the Windows MIDI mapper

The reference converter is `tools\xmi.py`.

**Main functions:**
- `vlq` / `put_vlq` (audio.c:18, 27): read and write MIDI variable-length numbers.
- `xmi_to_mid` (audio.c:40):
  - finds the first `EVNT` chunk and decodes its events
  - XMI timing is fixed: tempo meta events (0x51) are dropped (line 67), and one tempo of 500,000 us per quarter at 60
    ticks per quarter (120 ticks a second) is written (lines 100-107)
  - an XMI note-on carries its own duration, so a matching note-off is made at `t + dur` (lines 91-96)
  - events are stable-sorted by time, with `seq` keeping file order for equal times (`ev_cmp`, line 35)
  - output is a format-0 file with one track
  - only the first EVNT chunk is converted
- `voc_to_wav` (audio.c:125):
  - checks the "Creative Voice File" header
  - copies only block type 1 (sound data) into an 8-bit mono WAV
  - the rate is `1000000 / (256 - time constant)`, taken from the last type-1 block seen
- `load_song` (audio.c:168): parses our own format-0 `.mid` into `{tick, short message}` pairs. It handles running
  status and skips meta and sysex events.
- `music_thread` (audio.c:208):
  - plays the songs in turn on a `QueryPerformanceCounter` clock
  - sleeps until each event is due
  - volume is applied by scaling note-on velocity (at least 1) (lines 229-232)
  - sends "all notes off" after each song and waits about 3 seconds between songs
  - `M.next` skips to the next song
- `music_start` (audio.c:241): writes a default playlist (UW02..UW13) if none exists, reads up to 32 names (`#` lines
  skipped), opens `MIDI_MAPPER` and starts the thread. Returns false when there is no music.
- `music_volume` / `music_next` / `music_stop` (audio.c:267-277).

**Depends on:** `base` and winmm (`midiOut*`).

**Used by:** `extract.c` (conversion) and `main.c` (playback).

---

## check.c: check.exe, the loaders against the Python reference

**Purpose:** a stand-alone self-test (`main`, check.c:40). It needs `extracted\data-ref`, made by
`python -I tools\loose.py write`, and writes nothing. It runs five checks:

1. **Extractor = reference** (lines 50-61): `g_write_hook = compare_hook` (line 17), so every file `extract_core`
   would write is compared byte for byte with the reference instead. Paths are re-rooted from the fake folder
   `build\check-nowhere`. At most five differences are printed.
2. **Level text round trip** (lines 63-98): `level_load` reads each of the 9 `levelN.txt` files. Every tile's two words,
   each object's four words plus the 19 mobile bytes, and the 48 wall, 10 floor and 6 door texture-map entries are
   re-packed and compared with `LEV.ARK`. Blocks 0-8 are the levels and blocks 18-26 the texture maps.
3. **Textures** (lines 100-121): every `W64.TR` / `F32.TR` texture PNG reads back to the original palette indices. The
   expected count of 262 (210 walls + 52 floors) is fixed in the code.
4. **PNG reader** (lines 123-131): reads a zlib-compressed RGB PNG, `extracted\levels\level1.png`, if it is present.
5. **Meshes** (lines 133-143): every level builds (`level_mesh`) and must have more than 3000 vertices.

**Depends on:** `base`, `png`, `extract`, `level`.

---

## collide.c / collide.h: UW.EXE's terrain and object collision (proved)

**Purpose:** a routine-by-routine port of UW.EXE segment 0x2529, adapted from underworld-exhumed
`motion/COLLIDE.C` (MIT). It is proved against UW.EXE by `tools\collide_verify.py` (and `tools\proof_collide.py`) through
`tests\rtest.c`. The types are the original's where they change a result: `char` is signed (`i8`) and `int` is 16-bit
(`i16`).

All checks work on a `MotionCalc` record (collide.h:9-20), which is the original's `curP`:
- the caller fills x and y (1/8 tiles), z (object units), heading, radius and height
- the checks fill the hit bits, floor and top heights, the slope and open directions, and the list of objects in the way

The world (`CollWorld`, collide.h:26-30) holds:
- the raw LEV.ARK level block, 0x7c08 bytes: 64x64 tiles of 2 words; the 256 mobile objects of 27 bytes at 0x4000;
  the 768 static objects of 8 bytes at 0x5b00
- the 10 floor terrain words (`TxmTerr`)
- `COMOBJ.DAT`'s 11-byte records

**Tables** (collide.c):
- `TILE_OFF` (line 23): the 3x3 neighbourhood as map offsets.
- `TILE_WALLS` (line 26): read from UW.EXE DS 0x1d8a. It gives which sides of each tile type are wall.

**Routines and their UW.EXE addresses:**
- `tile_terr` (line 38), the original's `TILE_TERR`: `type + height << 4 + (the floor's terrain byte) << 4`. In UW1 the
  whole terrain byte is used.
- `coll_get_hgt`, **GetHgt 0x28494** (line 44): the floor height at a corner point. A solid tile and the wall half of a
  diagonal give 0x80 ("no floor"). Diagonals set `*steep`. A slope adds 0-7 by the point's position.
- `coll_get_slope_hgt`, **GetSlopeHgt 0x28635** (line 73): the fine height on the centre tile's slope.
- `solve_pnt`, **SolvePnt 0x2869d** (line 87): classifies one corner. 0x200 means wall, 0x100 means above reach, 0x800
  means below. Otherwise it sets the tile's bit `8 << (bits 8-9)`. It also raises `top`.
- `solve_center`, **SolveCenter 0x28754** (line 100): the same for the centre. It sets `hits0` and `floor`, adds 4 when
  standing, and adds 0x2000 on a slope.
- `coll_terrain_check`, **TerrainCheck 0x28809** (line 115):
  1. sets the 9 tile cache entries to the "unread" marker 0x1111
  2. solves the centre
  3. walks the footprint's four corners out by `radius` across tile edges (`off` moves by +-1 in a row, +-3 between
     rows)
  4. reads each corner's tile word
  5. a corner on a diagonal or slope that is not blocked checks the two wall sides (`WALLS[5]`) toward its neighbour
     corners, and can become a wall (0x200, `top = 0x80`)
  6. `firstsolve` stays 1 only if no corner went through that path
- `coll_compute_heading`, **ComputeHeading 0x28c86** (line 154): from the corners' flags it computes:
  - `slope`: the direction of the blocking side (0-7, 9 none)
  - `open`: the direction of the open side
  - the `DIRS` 3x3 table maps the sign sums to a direction
  - a single blocking corner on an odd (diagonal) direction, on the first solve, is rotated by the mover's heading
    (cases 0-7 at lines 167-187)
- `obj_coll_check` (**0x28ef0**, line 201): adds one object to `coll_objects` when its square overlaps the mover's.
  - radius 4 means the whole tile
  - two creatures shrink each other's radius by one (line 212)
  - a zero-height object still gets a top one higher than its bottom
  - bit 0x10 of `link` means the mover's centre is inside it
- `coll_object_check`, **ObjectCheck 0x29086** (line 225):
  - walks the object lists of the tiles round the mover
  - skips: the mover itself; for an NPC, objects with COMOBJ byte 3 bit 2; flat static objects; non-creature mobiles
    with byte 0x15 bit 7
  - with `useflag` set, only objects with COMOBJ byte 6 bit 0
  - a tile list longer than 0x40 stops the whole scan (line 251)
- `ocswap` (**oCswap 0x292ea**, line 255) and `coll_process_objlist` (**process_objlist 0x2934f**, line 261):
  - bubble-sort by top until the first object above the mover's z
  - then sort the rest by bottom
  - `first` = the index of the first object the mover is not standing on
  - `count` = how many of those it overlaps vertically
- `coll_can_place`, **can_place 0x29498** (line 277): whether an object of kind `item` fits at x, y, z.
  - sets `coll_place_height` (`nvokHgt`) and `coll_place_terrain` (`nvokTerr`: the footing bit, 0x10 for air,
    1 for floor)
  - refuses on wall hits, objects in the way, a non-solid object as the highest support (COMOBJ byte 3 bit 1), and
    (for a non-flier) hanging above the floor

**Quirks kept:**
- `coll_objects` has 9 entries where the original has 8 (collide.h:41). The original's ninth write spills into the
  memory that follows the array.

**Depends on:** `base` only. It reads the block given by `coll_world`.

**Used by:** `tests\rtest.c` (the proof). It is also in `COMMON`.

---

## conv.c / conv.h: our own conversation machine (the non-core path)

**Purpose:** runs a conversation program from `data\conversations\NNN.txt` on a small stack machine, and keeps each
conversation's variables between talks. When the rules core runs and `UW_CORE_TALK` is set, conversations are the
original's instead (CONVERSE.C / BABL.C on the core's fiber, see `core_world.c`). `conv.c` is the older path:
`game.c`'s `talk`, the Garamon bones, and `game_converse` (for the checks) use it.

**Main functions:**
- `conv_extract` (conv.c:9): `CNV.ARK` to one text file per conversation. The file holds the 8 header words, one
  `import NAME id unk type ret` line per import, then `code N` and the code words in hex, 16 per line. Existing files
  are kept.
- Stack helpers (lines 71-85):
  - `push` / `pop` / `mem` / `set_mem` bounds-check the `STACK` (4096) array
  - memory addresses *are* stack indices (FETCHM and STO address `V.st`)
  - `add_string` adds a runtime string, such as a name, after the block's strings
- `expand` (line 88): replaces `@GSn` / `@GIn` (global), `@SSn` / `@SIn` (`bp + n`) and `@PSn` / `@PIn` (through a
  pointer at `bp + n`) with a string or a number.
- `builtin` (line 116): the built-in functions, by name from the import table:
  - `babl_menu` / `babl_fmenu`: build the options; fmenu filters by a flag array and answers with the string id
  - `print`, `compare`, `contains`, `random`, `get_quest` / `set_quest` (64 entries), `length`
  - `sex`: always the male form
  - `babl_ask`: always the empty string
  - every other built-in returns 0 (the comment at line 148)
- `run` (line 153): the interpreter.
  - opcodes 0x00-0x29 (enum at line 47)
  - JMP is absolute; BEQ / BNE / BRA are relative to the operand
  - CALL pushes the return address; RET with no frame left ends the conversation
  - stops when waiting for a menu choice, on EXIT, on an unknown opcode, or after 200,000 steps
  - `UW_CONV_STEP` and `UW_CONV_TRACE` turn on tracing to stdout
- `load_block` (line 229): the conversation's strings from `data\strings\blockNNNN.txt`, turning `\n` and `\\` back
  into characters.
- `conv_start` (line 313):
  1. parses the text file
  2. sets the stack pointer and base pointer above the `nvars` variables (header word 6)
  3. loads the string block (header word 5)
  4. restores what this conversation remembers (`g_glob`)
  5. sets the imported variables it knows: `play_name`, `npc_name`, `npc_attitude` (whose address is kept for
     `conv_attitude`). It also sets `play_hp` and `play_health` to 30, and `play_level` and `dungeon_level` to 1.
     These are fixed values: the code does not read the real numbers.
  6. runs until the first menu
- `conv_choose` (line 382): echoes `> option`, puts the menu id in the result register and goes on.
- `conv_save` / `conv_load` / `conv_forget` (lines 272-308): the remembered variables, at most 128 for each of 512
  conversations, in `dir\conversations.txt` (`conv N v0 v1 ...`). They stand in for the original's BGLOBALS.DAT.
- `conv_set_attitude` / `conv_attitude` (lines 311-312): attitude in (0 hostile .. 3 friendly) and out after the talk.

**Depends on:** `base`. It reads `data\strings` (written by `extract.c`).

---

## core_glue.c: what the rules core needs from outside, made exact

**Purpose:** the outside names the compiled underworld-exhumed C needs, where the exact behaviour matters. Every other
outside call is a generated weak stub in `build\gen\core_stubs.c` (written by `tools\core.py`) until the front end
provides it. It is compiled with compat.h force-included.

**Contents:**
- **Borland rand** (lines 11-15), UW.EXE **0x12c37 rand / 0x12c26 srand**: `seed = seed * 0x015A4E35 + 1`, returning
  `(seed >> 16) & 0x7FFF`. `srand` keeps only 16 bits of the seed.
- **Game clock** (lines 17-22): `rp_time` returns `core_clock_fn()` when the game has set it with `cw_clock`, else
  `core_time`. `rp_walltime` and `rp_checkpoint` are empty.
- **Call log for the proofs** (lines 24-62):
  - `core_stub_log` appends each outside call's name to `core_calls` (NUL-separated)
  - `UW_STUB_TRACE` also prints each call to stderr
  - `core_stub_logv` formats arguments as `tools\core.py`'s SINKS describe them: F or N a string (with `\hh` escapes),
    w a word, b a byte, X a far pointer
- **Null pointers** (lines 64-76): `port_null_near` / `port_null_far` return blocks filled by `core_null_init`
  (line 312).
  - `null_block` holds UW.EXE's load image from offset 0
  - `null_near` holds DGROUP from paragraph 0x5AAC
  - this is what the emulator (`tools\emu.py`) has at those addresses, so a path that reads through a null pointer
    (for example a critter whose target is 0) gives the same result on both sides
  - `enhance_on` = 0 (every enhancement off)
- **`port_far_copy` and `movedata`** (lines 78, 292): forward byte copies through the far-pointer map.
- **Borland `itoa` / `ltoa` / `ultoa` / `strupr` / `strlwr`** (lines 86-111). `itoa` truncates to a 16-bit int first.
- **Borland file calls** (lines 113-200):
  - DOS paths have backslashes changed to slashes and are rooted at `core_game_dir`, which is only ever read
  - writes, and files already written, go to `core_write_dir`
  - `open_either` (line 140) tries the write folder first, then the game folder
  - all files are opened in binary
  - handles are numbered 5-39, as Borland numbers them (0-4 are the standard devices)
  - `bc_open` (line 167) maps the O_* bits to fopen modes
- **Far pointers** (lines 202-219): every host pointer handed to DOS becomes a fake "segment" (0x100 + index) with
  offset 0. `port_mk_fp` turns it back into the pointer. The table is reused round-robin after 4096 entries, and lookups
  search only the last 64.
- **`intdosx`** (line 223): only DOS 3Fh (read) and 40h (write) through `bc_read` / `bc_write`. An error sets carry
  and returns AX = 6. `int86` copies the inputs to the outputs.
- **Far heap** (lines 236-242): the host's `malloc`. `farcoreleft` / `coreleft` report fixed sizes.
- **MODEX.ASM string helpers** (lines 244-295), reproduced instruction for instruction:
  - lengths and counts are 16-bit
  - `str_ncopy` uses a signed compare (`jle`)
  - `FindStringDelimiter` also searches the terminator, so `c = 0` finds the end
  - `str_str` never finds an empty string
  - `str_cmp` returns the difference of the last bytes compared
  - `mem_set` writes `count / 2` words and leaves an odd count's last byte untouched
- **Mouse** (lines 297-333): the front end sets `core_mouse_x` / `core_mouse_y` (320x200 screen pixels),
  `core_mouse_buttons` and `core_mouse_dragged`. `mouse_get_input` returns -1 (no queued input).
- **Workspace** (lines 302-307): TMPALLOC.C's 64 KB scratch segment as a static block.
- **`core_death_fn`** (line 329): the `tools\core.py` PATCHES send `real_death` here when it is set. The death screen
  is the front end's. The rules before it (`player_is_dead`) are the original's.

**Depends on:** exhume's portability headers (`uw2.h` and compat.h).

**Used by:** the rules core (all of it), `imath.c` (`core_null_init`), `core_world.c`.

---

## core_dos.c: what DOS memory held past an array's end

**Purpose:** reproduces an out-of-bounds read the original makes. `res_to_terr` (MOTION.C, DS:03FA, 18 bytes) is
indexed by `1 << terrain code`. `OBJPHYS.C` reads it with 32 (the player's shore bit, or a creature of terrain code 5),
and with 64 and 128 (terrain codes 6 and 7, which some shipped creatures have). DOS reads on into:
- `MP` (DS:040C): struct MotionParams as DOS lays it out, including two near pointers
- then the 3D view's constant tables: chgtable DS:0444, headmod 045C, trans_grid 0464

The `tools\core.py` PATCHES send such reads to `core_res_to_terr`.

**Functions:**
- `core_dos_init` (line 15): copies 0x500 bytes of UW.EXE's DGROUP from paragraph 0x5AAC of the load image. It is
  called from `core_math_init`.
- `phys_ds` (line 22): a physics record's DOS address. `PN` is at DS:2780, followed by `CN1` to `CN4`, 0x28 bytes
  each.
- `mp_byte` (line 32): rebuilds `MP`'s bytes in DOS layout:
  - `vel` as its DS address
  - `pos` as 0x2768 when it points at `Ppd.x`
  - then the words in order
- `core_res_to_terr` (line 49): index < 18 returns the real table; DS 0x40C-0x443 returns `MP`'s byte; anything else
  returns DGROUP's byte.

**Depends on:** the core's `uw2.h`, `map.h` and `motion.h` (`PN`, `CN1`-`CN4`, `MP`, `Ppd`, `res_to_terr`).

---

## core_world.c / core_world.h: the game's way into the rules core

**Purpose:** the plain C face (`cw_*`) the front end calls to run the original's world. It:
- starts the core (UWEDIT.C's `init_world` without screen, sound, input or title, then MAINMENU.C's "Begin")
- runs one pass of MAINLOOP.C's `do_changes` each frame, which includes PLAYMOVE.C's `check_physics` (the player,
  critters, effects)
- feeds the player's input as the original's keyboard handler sets it
- carries clicks into the original's inventory, magic and conversation routines

It is compiled like the core.

**Start and step:**
- `cw_start` (line 47):
  1. reads `UW.EXE` (through Borland's `fopen`, so it comes from the game folder) for the maths tables
  2. runs `init_strings`, `Map_Init`, `init_objects`, `init_txtlib`, `init_player`, `init_ai`, `init_char(0)`,
     `init_lighting`, `init_combinables`
  3. copies `DATA\lev.ark` to `SAVE0\lev.ark`
  4. runs `init_babl`, `GetLevel(1)`, `player_setup(0x20, 2, 1)`, `do_level_hacks(1, 0)`
  5. sets up an all-released keyboard table (SYSENTRY.ASM's `key_on` and shift states)
  6. installs `on_death`, sets `scrnum = 0` (the 3D view) and marks `change_state[0]`
- `cw_step` (line 93):
  - advances `core_time` by `dt` in 1/256 s, carrying the fraction in `frac`, unless `cw_clock` set a real clock
  - if the core's fiber is waiting for the player, resumes it and returns. Its own loop runs the world in the
    meantime.
  - otherwise sets `PlayerFacing` / `PlayerHeading` from our mouse-look yaw. This is ours, a creature comfort; the
    original turns at a rate.
  - maps `CW_*` to `PIN_*`. Forward speed is 0x70 running and 0x32 walking (`do_player_keyboard`'s rates).
  - turning while standing still sends "forward at rate 0", so the physics turns the player's object, which is the
    heading a blow goes by (lines 117-124)
  - runs `step_job` on the fiber
- `step_job` (line 128): clears `changed` bit 9 (RedispInv, the inventory redraw, which the front end does), then
  calls `do_changes` if anything is set.
- `cw_player` (line 134): `PN.x / 256`, `PN.y / 256` (our z) and `PN.z / 256` as our units, plus `PlayerLevel`.
- `cw_block` (line 147): the core's `mapdata` (LEV.ARK layout), for drawing.
- `cw_teleport` (line 153): sets `NewPlayerX` / `Y` / `Level` and `editchng(8)` (UWEDIT.C's `new_player_pos` on the
  next pass).
- `cw_vitals` (line 164): HP from the player object, max HP from `playerdat->avghit`, and mana and max mana from the
  player record.
- `cw_fight` / `cw_attack` (lines 180-196): `toggle_fightmode` when the drawn state differs. A swing calls
  `player_attack(1..9)`. "Held" is the right button (`core_mouse_buttons = 2`), which `display_scr`'s
  `player_attack(0)` reads to charge.
- `cw_dead` (line 199): set by `on_death`, the core's `real_death` sink.

**Inventory and world objects** (lines 201-321):
- `cw_inv_slot` and `cw_held` read the core's `Inventory[]` and `CursorObjPtr`.
- `view_mode` (line 229): input mode 1, right panel = inventory, `mlowptr = mapdata`.
- `cw_inv_click` (line 239): puts the mouse at the centre of the slot's `InvDisplay` rectangle (and `inplist` relative
  to the panel at 0xF0, 0x52), sets `core_mouse_dragged`, then calls `DoInventoryMouse(how)`. A drag picks up or puts
  down; `how` is 0 for use and -2 for look.
- `pick` (line 261): sets the core's pick state (`newPlObj`, `PickMap`, `releasePtr`, `releaseable`) as `pick_3d`
  would.
- `cw_take` (`player_3dget`), `cw_use_world` (`player_3duse` on the fiber: a shrine may talk) and `cw_look_world`
  (`player_3dlook`).
- `cw_drop` (line 285): `DoSpecialActions(0x17)` with the pointer at (0x8A, 0x50) to drop, or (0x8A, 0xA0) to throw.
- `cw_bag_reach` (line 310): reaches a thing nested in bags the way the original's panel does:
  1. `CloseAllBags`
  2. `OpenTheBag` each bag outward-in
  3. `ScrollItemsDown` until the next one shows (`visible`, line 301, at most 64 scrolls)
  4. returns the thing's hole, 20-27

**Magic** (lines 323-372), through RUNES.C:
- `rune_panel` (line 329) fakes a click in the rune bag panel through `mous_in_rune`
- `cw_rune_add` turns rune r into the click position (comment at line 339:
  `rune = 0x14 - ((y - 0x12) / 0xF << 2) + (x - 3) / 0x12`)
- `cw_shelf_clear` clicks the top strip
- `cw_cast` calls `try_cast(1)`
- `cw_aiming` is true while `GameInputMode == 3` (a missile spell waiting)
- `cw_release_spell` calls `BlastFunction`
- `cw_debug_give_rune` is for the checks only

**Other:**
- `cw_light_level` (line 375): bits 4-7 of the player record's light byte.
- `cw_give_torch` (line 381): our setting "start with the torch lit". It is ours; the original starts with nothing.
  It does `CreateObj(0x95)`, quality 40, `AddToInventory(.., 7)`, `FixPlayerEquips`.

**The fiber** (lines 391-446): the original waits in loops of its own for the player:
- `converse_event_loop`'s menu
- `scroll_wait`'s pauses

The core therefore runs on a Win32 fiber (`fib_call`, line 416). Where the original reads input, `core_yield` (line
439) switches back to the game's fiber, and the next `cw_step` resumes it (`fib_resume`, line 431). Details:
- The fiber is used only when the environment variable `UW_CORE_TALK` is set. Otherwise, and if fibers cannot be
  made, the job runs directly (line 421) and a wait cannot hand control back.
- `fib_busy` means a job is under way. `on_core` means code is running on the core's fiber now.

**Conversation** (lines 448-524). CONVERSE.C, BABL.C and BARTER.C run as they are; the screen is ours. These functions
replace the core's stubs:
- `do_main_scroll` / `do_npc_scroll` / `do_play_scroll` set where `scroll_print` writes (`scroll_to`).
- `mouse_get_input_sp` (line 468): yields a frame, then passes the player's pick to `conv_play_menu`. Returns -1.
- `scroll_wait` (line 483): yields until a click or key (`cw_conv_skip`) or until the time runs out. Our scale is
  2 ms per tick.
- `gronk_gr` (line 494): reports the conversation art as loaded (our screen draws it) and counts conversations in
  `conv_serial`.
- `cw_talk_world`: `player_3dtalk` on the fiber.
- `cw_conv_active`: `scrnum == 2`.
- `cw_conv_waiting`, `cw_conv_options` (`core_conv_nopts` / `core_conv_opt`), `cw_conv_choose`.
- `cw_conv_name`: `get_name` of `talking_to`.

**Depends on:** the core's headers (`uw2.h`, `map.h`, `motion.h`, `object.h`, `player.h`, `critter.h`, `ui.h`, `inv.h`)
and `core_glue.c` (clock, mouse, death sink, game and write folders). `core_math_init` is in `imath.c`.

**Used by:** `main.c`, `game.c`, `inventory.c`.

---

## extract.c / extract.h: the loose files from the originals

**Purpose:** the engine reads only loose files in `data\`. This writes any that are missing from the original files in
`game\`, and never overwrites a file that is already there (edited files stay). The core part's output is
byte-identical to `tools\loose.py` (checked by `check.exe`).

**Entry points:**
- `extract_list` (line 17): the core part's file list, in order: 210 wall PNGs, 52 floor PNGs, `levels\level1..9.txt`.
- `extract_core` (line 722): the part that is checked. If anything on the list is missing, it:
  - loads PALS / W64 / F32 / LEV.ARK (`load_originals`, line 30)
  - writes the textures as indexed PNGs with palette 0 (`palette0`, line 48: VGA 6-bit to 8-bit `c << 2 | c >> 4`)
  - writes each level with `write_level`
- `extract_missing` (line 715): `extract_more` + `extract_core`.

**Helpers:**
- `tr_texture` (line 56): texture i of a .TR file.
- `ark_block` (line 67): a LEV.ARK block. Its size is the gap to the next stored block.
- `write_level` (line 101): the level text, exactly as `loose.py`'s `level_text` writes it:
  - `walls`, `floors`, `doors` from the texture-map block (block n + 17)
  - one `tile` line per tile (decoded from the two words)
  - one `object` line per object; the 256 mobiles also carry their 19 extra bytes in hex
  - `rest` lines with the undecoded tail of the block from 0x7300
  - `overlay` lines with block n + 8 (384 bytes when present)
- Strings (lines 149-231): `extract_strings`:
  - skips the work if `strings\blocks.txt` and every block it lists are present
  - otherwise decodes `STRINGS.PAK` (`strings_each` in `gr.c`) into `strings\blockNNNN.txt` files (`index: text`,
    with `\n` and `\\` escapes, `\r` dropped)
  - writes `blocks.txt`
- `extract_gr` (line 234): a .GR file into RGBA PNGs (palette 0, index 0 transparent).
  - Special case: UW1's `PANELS.GR` pictures have no headers. Each is 83x114 bytes of palette 0, opaque (lines
    244-265).

**`extract_more` (line 295) writes, each only when missing:**
- the 33 .GR sets in the table at line 309 (objects, wall objects, doors, every UI set, TMFLAT switches)
- `magic\spells.txt`:
  - UW.EXE's spell table: 48 x 4 bytes in the segment of overlay-table entry 73
  - the table is found through the `FBOV` signature; the base is `0x3200 + seg * 16`
  - byte 0 is category << 3 | aim; a word holds three 5-bit runes; byte 3 is strength; circle = n / 6 + 1
  - the names come from the manual (line 349)
- `levels\terrain.txt`: the floor textures that are water (type >> 4 == 1) or lava (2), from `TERRAIN.DAT`'s floor
  half (offset 512)
- `character\classes.txt`: `SKILLS.DAT` (read by UW.EXE 0x6dd3e). For each of 8 classes: str, dex, int and bonus
  points, then 5 skill sets.
- `ui\weapons\offsets.txt`: `WEAPONS.DAT` (read by 0x32fb1). y is converted to `111 - y` because the file counts up
  from the view's bottom row.
- creatures: `sprites\creatures\NN\anim.txt` and `pP_FFF.png` for the 64 kinds:
  - from `CRIT\ASSOC.ANM` (the animation number and palette for each kind) and `CRIT\CRnnPAGE.N00` / `.N01`
  - an `anim.txt` written by the first version (header `# creature`, which made black boxes) is remade
  - a "v2" or hand-made one is kept (lines 458-471)
  - transparency is the game palette's colour 0 (line 500)
- music and speech: `SOUND\*.XMI` to `music\*.mid` and `*.VOC` to `sound\speech\*.wav` (through `audio.c`)
- the raw `.XMI`, `UW.MT`, `UW.AD` and `SOUNDS.DAT`, copied for `xmidi.c`'s driver
- conversations (`conv_extract`)
- the full-screen `.BYT` pictures (320x200, palette 0) to `ui\screens`
- `objects\lights.txt`: `OBJECTS.DAT` at 0xd62, duration and brightness for objects 0x90-0x9F
- `objects\objects.txt`, remade unless its first line is `# uo-new objects v7`. It holds:
  - every COMOBJ record decoded
  - armour (0xb2), containers (0xd32), weapons (offset 2), critters (0x132, 48 bytes), launchers (0x82) and food
    (0xd82)
  - names from strings block 4
  - format comment at lines 636-655

**Depends on:** `base`, `png`, `gr`, `audio`, `conv` and Win32 `FindFirstFileA`.

**Used by:** `main.c`, `uwview.c`, `check.c`.

---

## gr.c / gr.h: .GR pictures, creature frames, STRINGS.PAK

**Purpose:** the decoders the extractor uses. The engine itself never reads the originals. The references are
`tools\gr.py`, `tools\crit.py` and `tools\strings_pak.py`.

**Functions:**
- `gr_count` (line 5): the picture count at byte 1.
- `nib` / `nib_count` (lines 11, 17): read nibbles, high nibble first. A count is one nibble; if that is 0, two more;
  if those are 0, three more.
- `gr_picture` (line 26): picture k into palette indices. The type is at byte 0 of the picture:
  - 4: 8-bit raw
  - 10: 4-bit raw through an ALLPALS.DAT small palette (byte 3 & 31)
  - 8: 4-bit run-length, alternating repeat records and run records. Count 1 switches to a run, and count 2 sets a
    repeat count (lines 45-67).
  - `guard` limits the loop for broken data
- `bit_get` / `bit_count` (lines 78, 86): the same for creature frames in 4- or 5-bit units. Counts of more than one
  unit combine 4 bits per unit even in 5-bit data (comment at line 71).
- `crit_page` (line 95): a `CRxxPAGE.Nyy` header: slots, segments (8 frames each, 255 for none), up to 16 small
  palettes of 32 entries, and frame offsets.
- `crit_frame` (line 116): frame k as w x h small-palette indices with its hotspot. Byte 4 == 6 means 5-bit data.
- `strings_each` (line 149): Huffman-decodes every string in STRINGS.PAK:
  - walks the node table from the root (the last node) until a leaf (byte 2 == 0xFF)
  - `|` ends a string
  - calls `each(block number, index, text, length)`

**Depends on:** `base`.

**Used by:** `extract.c`.

---

## imath.c: UW.EXE's integer trigonometry and square root

**Purpose:** IMATH.ASM (seg019 module 14) with C3DENTRY.ASM's C entries (underworld-exhumed
`uw1/src/sys/IMATH.ASM`), ported instruction for instruction to C:
- angles are words, 0x10000 to a full turn
- a sine is a 1.15 fixed-point value
- the tables are the original's, read from the user's UW.EXE

It is compiled like the core.

**Functions:**
- `core_math_init` (line 12): copies 0xAB0 table bytes from paragraph 0x5624 of the load image (seg063):
  - 0x4E0: the sine at each 1/256 turn, 257 words (0x560 is therefore the cosine)
  - 0x762: the arcsine
  - 0x864: the arccosine (0 to 1.0 in 128 steps)

  It then calls `core_dos_init` and `core_null_init`, which makes it the one start-up for all three tables.
- `lerp` (line 25): `imul` with the 8.8 result taken from the middle of DX:AX.
- `a38` / `cSinCos` (lines 32, 39): `_A38`, sine and cosine interpolated by the angle's low byte.
- `cFstSinCos` (line 48): `_A69`, the table entries for the high byte only.
- `cSqRt` (line 58): `_A78`, five Newton rounds from a guess chosen by the highest non-zero byte.
  - Quirk: DOS faults on a divide overflow, which only the largest inputs reach. Here the quotient is cut to 16 bits
    instead (`round16`, line 57).
- `b78` / `ba2` (lines 87, 94): `_B78`, arcsine with ax's sign; `_BA2`, arccosine, into the other half for a negative
  input.
- `cAtan2` (line 103): `_BD4`. It uses the arcsine while |x| <= 0x5A82 (sin 45 degrees), else the arccosine with x's
  sign.

**Depends on:** `uw2.h`, `core_dos.c`, `core_glue.c`.

**Used by:** the rules core, and `core_world.c` (`cw_start`).

---

## level.c / level.h: the level as text, as triangles, and for movement

**Purpose:**
- reads and writes `levelN.txt`
- answers floor height and solid-rock questions for our movement and lights
- builds the renderer's mesh and atlases
- decodes the core's raw level block into our `Level`

World space: x east, z north, y up; one tile = 1.0; a height step is `STEP` = 0.25, and the ceiling is at `CEIL_STEPS`
= 16 steps (unconfirmed from UW.EXE, level.h:5).

**Types** (level.h):
- `Tile` and `Object`: the LEV.ARK fields decoded.
- `Level`: textures, 64x64 tiles `[y * 64 + x]` with y 0 = south, and 1024 objects.
- `Vertex`: the renderer's layout, including `prev` for DLSS.
- `Mesh`:
  - vertices and indices
  - the colour atlas (1024 x 8192)
  - the normal atlas (4096^2)
  - the pictures' normal and height atlas and AO atlas
  - the shadow-ray grid
  - picture sizes
  - each creature kind's frames, slots and segments (pages 0 and 1)
- Atlas layout constants (level.h:44-85):
  - walls: 64-pixel cells, 16 per row, from the top
  - floors: 32-pixel cells from row 896
  - object pictures: 32-pixel cells from row 1024
  - doors from row 1536 (`DOOR_ROW`)
  - ANIMO pictures from cell 576
  - creatures from row 1664 (`CRIT_ROW`)
  - the particle dot at cell 463, the TMFLAT switches from cell 464, the TMOBJ decorations from cell 480

**Text:**
- `level_set_dir` / `level_dir` (line 19): the save's folder of levels, which `level_load` tries first.
- `level_save` (line 22): the level as it is now, written in the same text form.
- `level_load` (line 56): parses `walls`, `floors`, `doors`, `tile` and `object` lines.
  - Out-of-range tile values are an error.
  - Fewer than 4096 tiles is an error.
  - `rest` and `overlay` lines are ignored.

**Shape queries:**
- `corner_h` (line 126): a slope rises one step toward the side it is named after (unconfirmed).
- `open_on_edge` (line 138) and `open_at` (line 150): the diagonals' open halves.
- `level_solid_at` (line 163), `level_floor_y` (line 169: a solid tile reports the ceiling), `level_ceil_y`
  (line 161).

**Mesh:** `level_mesh` (line 596) works in this order:
1. Atlases: each wall and floor texture is scaled to its cell. A missing texture is drawn as a magenta check.
2. Normal maps: the user's Laigter `data\laigter\walls|floors\NNN_n.png` first, else
   `textures\*_normal\NNN.png` (lines 624-633).
3. Object pictures, switches, wall decorations, ANIMO frames and door pictures into their cells, each with
   `sprite_normal` (line 566: the Laigter `_n` / `_p` / `_o` maps into the picture atlases). For doors it also finds
   the opaque box (lines 832-838).
4. Creatures present on the level (lines 691-794):
   - parses `anim.txt` page 0 slots, segments and frames, and page 1 (slot 0x20 + ...), using `%n` care (comments at
     lines 708 and 729)
   - packs each frame at its own size in shelves from `CRIT_ROW`
   - a kind that does not fit is left out
5. `terrain_load` (line 204): water and lava floors.
6. The particle dot (lines 796-807).
7. The faces, for every non-solid tile:
   - floor and ceiling via `flat` (line 293). Its u/v follow TMAPOPS.ASM's `do_tmap` / GRIDDB.C's `qudecode`:
     u 0 at the west edge, v 0 at the south edge for a floor and the north edge for a ceiling. The ceiling uses floor
     slot 9 (unconfirmed, line 853).
   - the diagonal wall
   - the four edges: a full wall where the neighbour is closed, a riser where the neighbour's floor is higher
8. Wall texture mapping, `wall_quad` (line 279), as GRIDDB.C's `txtwal` / TMAPOPS.ASM's `do_compact_wtmap` do it:
   - the whole texture spans the face, u 0..63
   - v runs from 0 at the top edge to `sh * 16` texels at the bottom edge
   - so the stones hang from the top, and a slope's bottom edge squeezes them
   - faces whose ends differ in height are cut into 8 strips so the bilinear map is followed
   - a diagonal wall also gets 64 texels, not 90 (line 886)
   - `wall_strip` (line 258) turns a degenerate end into a triangle
9. Relief, in our renderer only:
   - `set_tapers` (line 492) hashes edges. An edge shared with a coplanar face of the same texture keeps its relief;
     the other edges ease it to the base.
   - `emit_face` (line 380) tiles the texture's relief mesh (`data\textures\*_mesh\NNN.obj`, read by `relief_load`,
     line 322) over the face. Each mesh triangle is clipped to the outline (`clip_half`, line 361), split at the taper
     lines, and raised along the normal by `RELIEF_DEPTH` x height x smoothstep ease.
   - vertices are de-duplicated through a generation-stamped hash
   - water and lava faces stay flat and carry the material in colour alpha: 1.2 water, 1.3 lava (line 929)
   - `level_relief = false` gives the original's flat faces
   - if too many triangles are made, the rest are drawn flat (line 933)
10. Orbs for lights (`orb`, line 527: colour alpha 2 = emissive), then the grid: type, height, and terrain in byte 2.

**From the core:** `level_from_block` (line 956) and `level_obj_from_block` (line 976) decode the raw 0x7c08-byte
block (the core's `mapdata`) into `Level` / `Object`, with the same bit fields as the extractor uses. The wall, floor
and door texture lists are left unchanged.

**Depends on:** `base`, `png`.

**Used by:** `main.c`, `uwview.c`, `game.c`, `inventory.c`, `lights.c`, `check.c`.

---

## lights.c / lights.h: the world's light sources

**Purpose:** reads the hand-placed lights and works out which lights shade each tile, for the renderer. The player's
torch (light 0) is set elsewhere (`render_set_torch`).

**Functions:**
- `read_file` (line 18): parses `light LEVEL X Z HEIGHT R G B INTENSITY RANGE` (an orb) and
  `walltorch ... FX FZ` (a wall torch) for one level. HEIGHT is in steps (`* STEP`), and the range is at least 0.5.
- `lights_load` (line 59): writes the default `data\lights.txt` if it is missing. The default says there are no lights,
  as in the original. It then reads that file.
- `clear_line` (line 48): samples a segment every 0.1 tiles against rock, floor and ceiling.
- `lights_tiles` (line 71): for every open tile, up to `TILE_LIGHTS` = 8 lights within `range + 0.75`, scored by
  `intensity / (d^2 + 0.5)`. The score is x100 when the light can see one of three test points in the tile (diagonal
  tiles use the centre of their open half). The list is kept sorted best first; `NO_LIGHT` ends it.

**Depends on:** `base`, `level` (`level_solid_at`, `level_floor_y`, `level_ceil_y`).

**Used by:** `main.c`, `uwview.c`. `render.h` and `world.h` include the header.

---

## inventory.c / inventory.h: the player's things, the panel, light and body

**Purpose:** the player's inventory on our side:
- a pool of 512 objects (`I.pool`) with the original's slot layout:
  - 0-10 body: helm, chest, gloves, leggings, boots, both shoulders, both hands, both rings
  - 11-18 the pack's 8 holes
- the right-hand panel, drawn as a combined grid with a bag bar and tooltips (this design is ours)
- the burning of lights, and the body's hunger, fatigue, drink and poison when the core is not running

When the core runs, the pool is rebuilt every frame as a mirror of the core's `Inventory[]` (`inv_core_sync`), and
clicks go to the original's routines (`inv_update_core`).

**Pool and tree helpers** (lines 44-91):
- `alloc`, `free_tree`, `tree_mass`, `contents_mass`
- `holds` (line 54): a container that is not a stack (flags bit 0x40 means the link is a count)
- `lv_carries` / `pool_carries` (lines 58, 63): what travels with a thing: a container's contents, or an enchanted
  thing's spell object (0x120-0x12F). A spell object's own link is its number, not a chain.

**Burning and the player tick** (lines 93-174), when the core is not running:
- `load_light_table` (line 94): durations and brightness from `objects\lights.txt`, with built-in defaults.
- `burn_tick` (line 114): UW.EXE file 0x2acbd and 0x2af36. A lit light in slots 5-8 loses a point when
  `tick % duration == 0`. At the last point it goes out and stays where it is, with "Thy lantern has run out of oil."
  or "Thy X has burned out."
- `player_tick` (line 141), every 21 s (`inv_tick`, line 164: more than 20 whole seconds):
  - the counter goes 1-24, back to 0 at 24
  - poison -1 every 3rd tick
  - at 24: hunger -(3..6), drunk -1, fatigue +1
  - calls `game_player_tick` for healing, mana and poison harm
- `inv_status` (line 159): "You are X and Y." from block 1, `104 + hunger / 30` and `118 - fatigue / 23` (0x26122).

**Light:** `inv_light` (line 176): the brightest lit light in slots 5-8, or a light spell if brighter.
- The reach comes from SHADES.DAT's 4th word (UW.EXE 0x810f8): 3 3 4 4 5 6 7 7 tiles by level.
- The intensity is ours: `0.5 + 0.4 x brightness`, dimmed below quality 8.

**Enchantments** (lines 203-286), UW.EXE 0x38b5c:
- `enchant_src` (line 210):
  - triggers and traps (class 6) never have one
  - a linked object's source is the spell object in its chain; one with no charges is missed 4 times in 10 unless
    only looked at (DS 0x9f8)
  - a quantity object with flag 0x08 (and not furniture) is its own source
- `enchant_read` (line 222): major (bit 0x04) means group `l >> 6` (0 = the spell numbered `l & 63`), else category
  `l >> 4`, value `l & 15`.
- `inv_enchant_name` (line 237): block 6 names. Armour's Protection and Toughness are at 464+; a major enchantment or a
  weapon's category 12 is at 256 + l.
- `worn_counts` (line 248), UW.EXE 0x79c12: slots 0-4 and 9-10 always count; slot 8 only for a shield.
- `inv_worn_enchants` (line 254), UW.EXE 0x7e6e5: the normal forms of categories 0-3 and 11.
- `inv_enchant_armour` (line 264), UW.EXE 0x7e15f: by body part through DS 0x1af8. Bit 8 of the value means
  Toughness, else Protection, each worth `(v & 7) + 1`. A ring or shield counts on both body and arms.
- `inv_weapon_enchant` (line 278): category 12 in the right hand gives accuracy (or damage when bit 8 is set),
  `(v & 7) + 1`.

**Queries and stock:**
- `inv_slot_item`, `inv_consume_one` (line 319), `inv_held_item` / `inv_held_consume` (lines 331-346)
- `inv_give`, `inv_wield`, `carried`
- `inv_count` (line 459): stacks counted
- `inv_has_key` (line 490): keys 0x100-0x10E except 0x101 (the lockpick), matched by owner & 63
- `inv_weapon`
- `inv_armour` (line 478), UW.EXE 0x7e3ed: `protection x (quality & 63) / 64 + 1`. Nothing for items below 0x20.
  Slot 8 counts only a shield 0x3B-0x3F.
- `inv_protection`, `inv_set_capacity` / `inv_capacity`

**Saves:** `inv_save` / `inv_load` (lines 378-432): `dir\inventory.txt` with a `state` line (body, tick, clock, rng),
`slot`, `held` and `object` lines (and `hidden`).

**Level slots** (lines 501-533):
- `mark` and `level_used` find every object slot reachable from the tiles. Links are followed even when they are
  counts, because marking an unused slot costs nothing while freeing a trap would break the level (comment at line
  506).
- `inv_level_free`, `inv_level_alloc` (static slots 256+), `inv_level_alloc_mobile` (2-255; 1 is the player).

**Moving things** (lines 535-702):
- `copy_in` / `copy_out_rec`: a thing with its contents between the level and the pool
- `slot_ok`, `put_in` (to the front of a container, if `obj_fits`), `unlink_from`, `place_new` (a free hole, else the
  first container that takes it)
- `inv_take` (line 639): with the core, `cw_take`. Otherwise:
  - "You cannot pick that up."
  - pool space
  - the carry limit, strength x 20 tenths (block 1:95)
  - room in the pack
  - out of the tile chain
- `drop_held` (line 683): half a tile ahead, or at the feet if that is rock. With no space: the message and effect
  0xF (MISSILE.C).

**Using things** (lines 704-869):
- `consume` (line 712).
- `eat` (line 732), UW.EXE file 0x374ee:
  - food adds nutrition and is refused past 255
  - the taste line is block 1, 172 + up to 4
  - special items have their own strings (229-240), listed in the comment at lines 726-731
  - a potion applies its enchantment
  - a toadstool poisons
  - a drink below -1 makes the player drunk
  - wine cannot be opened
- `read_thing` (line 774), UW.EXE 0x37fa0:
  - the map: not done yet
  - an enchanted scroll casts once
  - otherwise the text is block 3 [link & 0x1ff]; blank pages for flag 0x02, index >= 0x100 or an empty string
- `use_thing` (line 798): lights only in slots 5-8; a container shows or hides; reading; bedroll (0x37cd2); wand charges
  (0x38d20, cracks 4 in 10 when empty); fishing pole; instruments; leeches; silver seed; eating.
- `inv_toggle_light` (line 851): order right hand, left hand, right shoulder, left shoulder. Otherwise a light from the
  pack goes to a free one.

**Panel** (lines 871-1206, our design):
- `cells_build` (line 950): body holes round the figure, the grid (`grid_build`, line 925: the pack's own holes, then
  each shown bag's section, each from a new row with room to drop more), the bag bar, scroll arrows and the
  "Character" checkbox
- `body_slot_for` (line 998), `equip_held`, `wear_from_pack` (line 1039: the replaced thing goes exactly where the
  new one was)
- `take_out`
- `right_click` (line 1072): never drops; use, wear or bag as the comment says
- `stack_onto` (line 1111): same kind, stackable, same quality, total < 0x200
- `inv_update` (line 1123): with the core it goes to `inv_update_core`. Otherwise it handles look, the figure (wear,
  or `game_status`), scrolling, shift + right click to drop, the bag bar, pick-up, use-on (`game_use_on_item`: 1 back
  to the pack, 2 used up, 3 the target destroyed), into a bag, stacking, and swapping.
- `inv_draw` (line 1395) and the tooltips (lines 1227-1393: `tip_build` shows the original's attack score, armour
  and durability numbers for this character, and green or red differences against what is worn).

**Core mirror** (lines 1467-1583):
- `mirror_tree` (line 1472): a core object and its contents into the pool, keeping which bags are hidden by core index
- `inv_core_sync` (line 1499)
- `inv_core_of`
- `core_click_thing` (line 1528): a top-level slot is clicked directly; a thing inside bags is reached with
  `cw_bag_reach` and then clicked
- `inv_update_core` (line 1543): our panel's clicks translated to `cw_inv_click` / `cw_drop`

**Depends on:**
- `level`, `core_world`, `game` (messages, `game_*` hooks), `objdata` (`obj_info`, `obj_name`, `obj_fits`, ...)
- `orig` (`uw_rand_mod`, the original's `rand() % n`), `sound`, `ui`, `str_get`

---

## game.c / game.h: the game layer (HUD, panels, our stand-in rules)

**Purpose:** everything between `main.c` and the rules:
- input to actions
- messages and the reading panel
- doors, creatures, combat, magic, traps and triggers on our side (used when the rules core is not running)
- character creation and the character sheet
- saves of the game state
- the HUD
- the core's outside calls for messages and sounds

Inline comments cite UW.EXE file offsets for each rule. With `cw_running()`, most of these hand over to `core_world.c`,
as listed under "Core paths" below.

**State:**
- `G` (line 26): level, mesh, panels, the 256 Hz clock (a new character starts at 0x010b3000 = 19:00 day 1), hp and
  mana, doors, messages, the hovered thing, lasting spells, the shelf, the silver tree, swimming and footsteps.
- `W` (line 417): the weapon picture's state.
- `g_cr[256]` (line 406): creatures.
- `g_ms[32]` (line 462): missiles.
- `P` (line 1465): the character.
- `CG` (line 1710): the creation steps.
- `g_quest` / `g_var` (lines 407-408): the quest bits and game variables. They are declared again at lines 2413-2414.
  These are tentative definitions in C, so both are the same objects.

**Start and level:**
- `game_start` (line 116): `objdata_load`, `inv_start`, `character_load`, clock, `uw_srand(time)` (UW.EXE 0x6a947).
- `game_level` (line 154): finds the doors (0x140-0x147 shut, 0x148-0x14F open), loads the creatures, builds the sprite
  cards.
- `rebuild_sprites` / `upload_sprites` (lines 139-146): item cards (`world_sprites`) plus door quads, creature cards and
  missile cards, every frame.

**Doors** (lines 178-383):
- `game_blocked` (line 186): the frame and a mostly shut door as segments.
- `door_lock` / `locked` (lines 204, 208), UW.EXE 0x3831b: the lock object 0x10F, locked when flags bit 0 is set.
- `pick_door` (line 214).
- `lock_try` (line 237): the key and lockpick rules, including a Picklock check against z x 3, z 15 never, z 14 only
  with 31.
- `spiked` (line 283): quality bit 1 on a closed door (0x36c42, 0x7c6c5).
- `use_held_on_world` (line 295): the "use X on Y" cases (0x36b9a):
  - pole on a switch (0x36e62)
  - rock hammer on a boulder (0x3797e)
  - bones on Garamon's grave (0x36cf9)
  - key (0x36afd)
  - lockpick (0x36a78)
  - spike (0x36c42)
- `use_door` (line 363): E on a locked door uses a key carried. This one-press shortcut is ours; the comment at line
  368 says how the original does it.

**Terrain** (line 258): `terrain_at` reads `levels\terrain.txt` once.

**Weapon picture** (lines 411-583), UW.EXE 0x33ffd draws it and 0x330c9 steps it:
- 28 pictures per kind
- `weapon_step` (line 555): steps every 1/16 s (estimated). f 0-3 draws back and holds at 3 while charging; the blow
  lands at f 6 (0x257a8); f 9 ends.
- `weapon_draw` (line 574): scaled from the 176x112 view.

**Missiles** (lines 451-552), UW.EXE 0x2b2bd:
- the `LAUNCH` table (line 464)
- `player_shoot` (line 478): speed = Missile x 8 + 0xC0, a check against 10 adjusts it, damage x speed / 256
- `missiles_update` (line 498): our flight model; a miss lies where it fell

**Creatures (non-core)** (lines 386-410, 586-642, 1346-1427, 2066-2140):
- `creature_add` (line 588): hp from mobile byte 0, else the kind's. Attitude from mobile byte 6 bits 6-7 (UW.EXE
  0x7120c).
- `creature_remove` (line 608): drops what it carried on its tile.
- `hostiles_within` (line 634), UW.EXE 0x711e1.
- `creature_frame` / `creature_walk_pos`.
- `creature_walk_sound` (line 1389): `critter_mv`'s sounds by record byte 0x10.
- `creature_death_sound` (line 1407).
- `creatures_update` (line 2066):
  - with the core, rebuilds `g_cr` from the core's block, only for pointing at creatures
  - otherwise runs our placeholder chase and strike AI, with noticing distances scaled from visibility and noise (our
    estimate, comment at line 2119)

**Combat** (lines 1428-1455, 1976-2063, 2142-2264):
- `skill_check` (0x3419c) and `dice` (0x38eee) through `orig.c`
- `roll_damage` (0x24cb5), `hit_part` (0x2441a), `angle_bonus` (0x25160)
- `player_armour` (line 1980): 0x7e444 and 0x7e39f
- `creature_strikes` (line 1995): 0x259ee, 0x24b68, 0x24dc1, easy halves (0x24e43)
- `blow_sound` (line 2051), `do_miss` 0x25039. It reproduces the original's slip: a creature target's armour kind is
  read from the *attacker's* record (comment at lines 2039-2046).
- `attack_type` (line 2150): the crosshair's height on the creature picks the row: thrust, slash or bash.
- `player_attack` (line 2168): 0x25406 / 0x24b68 / 0x24cb5, the weapon's enchantment, and a wall clang on a miss
  (`resolve_attack` / `find_wall_coll`).

**Sleep and death** (lines 643-764):
- `restore_hp` / `restore_mana` (0x346c2, 0x34656)
- `game_sleep` (line 654), UW.EXE 0x81f8f: refused on level 9 and with hostiles within 2; hours and interruption
  (0x71670)
- `player_dies` (line 729), 0x82880: the talismans rule, an eighth of the experience lost, the bones, the silver tree
- `game_plant_seed` (line 757), 0x37d45

**Items' uses** (lines 766-867):
- `repair_item` (line 771), 0x747ac / 0x748c4: two clicks, ask and then try
- `game_use_on_item` (line 801): the anvil, and oil (0x37b60)
- `game_fish` (0x73f34), `game_leeches` (0x37cfe), `game_play` (0x37ce9), `track` (line 852, 0x356ee)

**Magic (non-core)** (lines 869-1298):
- `spells_load` (`magic\spells.txt`)
- `spell_last` (line 913), 0x2b109: at most 3, durations by strength bits
- `effects` / `spell_has`, and the derived values `game_spell_light`, `game_motion` (0x7e0a8), `spell_armour`,
  `player_protect`, `player_resist`, `player_visibility` / `player_noise` (0x7dea4 / 0x7e31d)
- `spells_tick`
- the 5x5 target square (`spell_square`, line 1006: 0x38e1d), `random_tiles`, `creature_hurt` / `creature_harm`
  (0x25f0a) / `tile_harm` (0x35bca)
- `spell_effect` (line 1077), 0x34418 by category 0-13. Some values are marked as our estimates in the code (for
  example tremor's harm).
- `cast_shelf` (line 1238), 0x78ec8:
  - the wait after a cast (0x78dd9 / 0x78f9c)
  - "Not a spell"
  - the circle and mana checks
  - the Casting check: a backfire is the harm of circle / 2 d8
- `game_enchant_effect` (line 1272), `game_set_shelf` (for the checks), `has_rune`

**Character** (lines 1456-1670):
- the `P` record in UW.EXE's skill order
- `character_load` / `character_read` / `character_save_to` (`character.txt`)
- `character_derive` (line 1582), 0x81305: max HP, max mana, carry limit
- `swim_tick` (line 1498), 0x2b02e
- `game_player_tick` (line 1509), 0x2adee and 0x2af03. Regeneration's +1 per tick is an estimate.
- `skill_advance` (line 1616), 0x8141f
- `shrine_advance` (line 1628), 0x814e1
- `chant` (line 1641), 0x816fd mantras
- `gain_exp` (line 1960): `uw_player_get_exp` (0x341d8, proved in `orig.c`)
- `kill_exp` (line 1971): `uw_kill_exp` (0x25b35, proved)

**Character creation** (lines 1672-1956), overlay 5991:
- `classes_load`
- `init_char` (line 1794): UW.EXE `init_char`, underworld-exhumed CHARGEN.C:88. It keeps the original's roll order.
- `cg_class` (0x6cdb2), `cg_next_set` (0x6ca88), `cg_choose`, `cg_options`
- `chargen_update` / `chargen_draw`

**Saves:**
- `game_save` / `game_load` (lines 1719-1780): `character.txt` + `game.txt` (hp, mana, fight, clock, silver tree,
  quest, vars, shelf, spells) + inventory + conversations
- `game_new` (line 1782)
- `game_store_level` (line 1337): creatures' hp back into mobile byte 0

**Conversations** (lines 2266-2336):
- `talk` (line 2317): an NPC's own number (mobile byte 18), else 256 + kind
- `talk_end` (line 2300): writes the final attitude back
- `talk_state` / `talk_choose` / `core_talk_open`: the core's conversation shown on our screen (`g_ccv`)

**Core creature drawing:**
- `core_creature_frame` (line 2344), as DRAWOBJ.C chooses the frame (`DIRTAB`, line 2343)
- `core_creature_cards` (line 2368): position from the home word at 0x16

**Traps and triggers** (lines 2410-2667):
- `run_trap` (line 2444), the dispatcher at 0x83d96:
  - set variable (0x83ea4), check variable (0x83fc5), inventory (0x8463b)
  - change terrain (0x83e17 / 0x73b68), door (0x843dd), create object (0x8418f), delete object (0x845dc)
  - text string (0x847bb), arrow (0x83dc3, our aim), teleport, damage (0x840bd)
- `chain_on` (line 2426): 0x84803
- `fire_trigger`
- `trigger_event` (line 2632), 0x83b3b: the event table from OBJECTS.DAT 0xd92; the player needs flags bit 2; a look
  with z needs a search check
- `enter_tile`, `use_switch` (line 2648), `game_travel`

**Pointing and looking** (lines 2669-2861):
- `pick` (line 2671): items and wall decorations
- `game_light` (line 2708): with the core, the player's light level 0-7 scaled to our torch
- `look_world` (line 2731): through the core's `player_3dlook` when it runs. Otherwise our descriptions and walls,
  floors and ceilings by ray, including the 0x16E / 0x16F look triggers on a wall.
- `hover_world` (line 2794)
- `use_hovered` (line 2809): the core's take or use; the core's talk with `UW_CORE_TALK`; else ours

**Frame:**
- `game_escape` (line 2865): one step back out of whatever is open
- `core_world_sync` (line 2883): when the core's block changes, decodes it into our level. A change to a tile's shape
  asks for a remesh; the first sync is skipped.
- `game_update` (line 2900): the per-frame driver for reading, talking, death, the clock and music turn, terrain
  (swimming and footsteps, UW.EXE 0x312d3; lava damage is our estimate), mantra, instrument, runes panel, wards
  (0x846e3), fight mode, the core's swing (`cw_attack`) or our charge (0x2574b), missiles, the inventory panel or the
  world, doors, and syncing with the core (shelf, vitals, death)

**HUD** (lines 3147-3571):
- `flask` (line 3149)
- `sheet_draw` / `sheet_tip` (lines 3185, 3233): the character panel and its tooltips
- `game_draw_ui` (line 3371): chargen, the sleep fade, the rune panel, lasting spell names, chant, the death screen,
  weapon, sheet, reading, conversation, the hover name, the crosshair (cursor 10 look tag, 9 hostile, 0 otherwise),
  the compass, the power gem (POWER.GR, `charge / 12 + 1`), the Fight icon, flasks, and fading messages

**The core's outside calls** (lines 3573-3629). They replace the weak stubs in `build\gen\core_stubs.c`:
- `scroll_print` (line 3588): gathers pieces into lines and strips `\digit` colour escapes and `\P`.
- `scroll_line` (line 3577): sends a line to the messages or the conversation (`cw_scroll_target`).
- `play_effect` / `play_effect_here` / `play_effect_on_mobile` / `kill_effect`: SOUND.C's calls, through `sound.c`.
- `wdialog` (line 3621): answers AskHowMany's "Move how many?" with `cw_howmany()` (999 means everything). A typed
  answer during a conversation (`babl_ask`) is still to do.
- `game_core_conv` (line 3629): for the checks.

**Depends on:**
- `level`, `lights`, `inventory`, `conv`, `core_world`
- `objdata`, `orig` (the proved rules: `uw_skill_check`, `uw_dice`, `uw_resist`, `uw_player_get_exp`, `uw_kill_exp`,
  `uw_rand*`)
- `render`, `sound`, `ui`, `world`

**Used by:** `main.c`.

---

## UNSURE / points to check

These are places where the code and its own comments disagree, or where the intent is not clear from the code:

- `lights.h:5-8` describes per-level automatic lights in `data\lights\levelN.txt`, and `main.c:519`'s message names
  that file. However, `lights_load` (`lights.c:59`) only reads `data\lights.txt`, and nothing in `src` writes or reads
  `lights\levelN.txt`. Was this feature removed, or is it still to come?
- `lights.c:18` `read_file` never sets `Light.flame` or `Light.kelvin` (`lights.h:28-31`). They are whatever the
  caller's array held. Does the caller zero it?
- `level.h:62` says `Mesh.grid` holds "type, floor height (steps), 0, 0". `level.c:948` writes the terrain (1 water,
  2 lava) into byte 2.
- `game.c:387-390` says a creature is "Hostile unless it has a conversation". `creature_add` (`game.c:601`) decides by
  the attitude bits, mobile byte 6 >> 6, instead.
- `inventory.c:27`: the `open[8]` / `nopen` fields of `I` are not used anywhere in `inventory.c`. Are they left over
  from the older one-bag-at-a-time panel that `inventory.h:3-5` still describes?
- `inventory.c:1439-1444`: the panel's "Carrying" weight leaves out the thing on the pointer, while `carried()` (2026-11-16: put right, see the root README)
  (`inventory.c:370`, used for the limit) counts it. Is the difference meant?
- `game.c:2932`: the comment "at nothing, the player wakes at the entrance of the Abyss" does not match the code that
  follows (the death choice).
