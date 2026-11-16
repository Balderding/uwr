# laigter-cli - Laigter's map maker on the command line, without Qt

`laigter_cli.cpp` ports Laigter 1.14.0's map generator to C++ with no Qt. Laigter is by Pablo Ivan Fonovich (azagaya,
https://github.com/azagaya/laigter, GPL v3). The port makes the same normal, specular, occlusion and parallax maps.
We use it to make the normal maps (plus occlusion and height maps for sprites) that our renderer uses on UW's walls,
floors and sprites. It is a separate program: the game never links it, it only reads the PNG files it writes.

Licence: GPL v3 (`LICENSE-GPL3.txt`, beside this file). `thirdparty\CImg.h` is CImg (CeCILL-C), the same image
library Laigter itself uses.

History, the proof and the user's choices of settings: NOTES.md, "Laigter normal maps (2026-11-10)".

## Files

| File | What |
| --- | --- |
| `laigter_cli.cpp` | The whole program (776 lines). |
| `build.cmd` | Builds `build\laigter-cli.exe` at the repository root. |
| `make-normals.cmd` | Makes the maps for the game's art into `data\laigter\`. |
| `texture.preset` | Settings for walls and floors. |
| `sprite_op.preset` | Settings for sprites and effects: `pixelart.preset` plus occlusion and height. |
| `pixelart.preset` | The pixel-art settings on their own. `make-normals.cmd` does not use this file now; it uses `sprite_op.preset`, which contains these same six lines. |
| `LICENSE-GPL3.txt` | The GPL v3 text. |
| `thirdparty\CImg.h` | CImg. |

## Building

```
tools\laigter-cli\build.cmd
```

It loads `O:\appsnotonc\llvm\env.cmd`, compiles our `src\base.c` and `src\png.c` (the engine's own PNG reader) and
`src\gr.c`, `audio.c`, `conv.c`, `extract.c` (the engine's extractor, for `--uwr-game`) into `build\laigter-obj\`, and compiles `laigter_cli.cpp` with `clang++ -std=c++17 -O3 -ffp-contract=off`. The flag turns
off fused multiply-add, so the floating-point math rounds as Laigter's does. The program is linked statically into
`build\laigter-cli.exe`.

## Running

```
build\laigter-cli.exe -d <file|folder|a,b,c> [-n] [-c] [-o] [-p] [-r X.preset] [-l OUT] [options]
```

| Option | Meaning |
| --- | --- |
| `-d`, `--diffuse` | The pictures: a file, a folder (every picture under it), or a comma-separated list. |
| `-n` / `-c` / `-o` / `-p` | Make normal / specular / occlusion / parallax maps (`NAME_n.png`, `_s`, `_o`, `_p`). |
| `-r`, `--preset` | A Laigter `.preset` file (the same format the Laigter window saves). |
| `-l`, `--location` | Where the maps go. The default is beside each picture. Files inside the output folder are never taken as input. |
| `--normal-suffix`, `--specular-suffix`, `--occlusion-suffix`, `--paralax-suffix` (or `--parallax-suffix`) | Change the suffixes. |
| `--not-recursive` | A folder's own pictures only. |
| `--flatten` | No sub-folders in the output. |
| `--check-changes` | Only remake maps that are older than their picture. |
| `--tile` | Ours: tileable, the same as `Tile` set to 1 in a preset. It is applied after the preset. |
| `--flat-zero-x` | Ours: newer Laigter's rule, a pixel whose x slope is exactly 0 is drawn flat. |
| `-j`, `--jobs N` | Ours: how many pictures are made at once. The default is all cores. |
| `-q`, `--quiet` | Ours: no line per picture. |
| `-g`, `--no-gui`, `-s`, `--software-opengl` | Accepted and ignored, for compatibility with Laigter's command line. |

Only `.png` files are read. A picture under 3 pixels across is skipped with a message, because Laigter's slopes would
read past its edge. The program ends with "N pictures made, N skipped, N failed, T s", and the exit code is 1 if any
picture failed.

The Linux program is cross-built by `tools\build_laigter_linux.py` into `build\linux\laigter-cli` (the same
sources and switches; libstdc++ linked in).

`thirdparty\CImg.h` (CImg 2.9.1, CeCILL-C) is not in the public repository: take it from Laigter 1.14.0's own
source, https://github.com/azagaya/laigter (`thirdparty/CImg.h`).

## Every map in one run (uwr_maps.inc)

Ours, not Laigter's. Two more ways to start the program, which make every map uwr's renderer reads:

```
laigter-cli --uwr DATA [PRESETS] [-j N]            from loose pictures (DATA\textures, sprites, ui) into DATA\laigter
laigter-cli --uwr-game GAME OUT [PRESETS] [-j N]   from the player's own game files into OUT\laigter
```

- `--uwr-game` is what the game runs on its first start when it has no maps (`src\mapsmake.h`, called from
  uwview.c's level_build). It unpacks the game's pictures with the engine's own extractor into
  `OUT\laigter\unpack-tmp`, makes the maps, and removes the unpacked pictures: only the maps stay on the disk.
  `OUT\laigter\made.txt` is written last, so a run cut short is made again.
- PRESETS is the folder of the `.preset` files; the default is the program's own folder.
- The steps are make-normals.cmd's below. For the walls and floors, the two Python steps that followed it
  (`tools\upsize_normals.py`: the normal map enlarged 4 times by Pillow's bicubic curve wrapped round the tile;
  `tools\make_soft_maps.py` variant A: the soft height and occlusion maps) are ported into the program, so a
  player needs no Python.
- Proved 2026-11-16 with `tools\cmp_map_pixels.py A B` (every PNG decoded and compared): all 13,227 maps made by
  `--uwr`, by `--uwr-game`, by the Linux program under WSL, and by the game's own first run are the same, pixel
  for pixel, as the maps in `data\laigter` made by make-normals.cmd and the two Python scripts (0 pixels
  different). The files themselves differ (this program writes PNGs without compression).
- About 10 seconds on the machine it was written on (36 threads).

## make-normals.cmd

```
tools\laigter-cli\make-normals.cmd          remake only the maps older than their picture (--check-changes)
tools\laigter-cli\make-normals.cmd all      remake every map
```

It runs from the repository root, builds the program first if it is missing, and then makes these maps:

| Input | Output | Maps | Preset |
| --- | --- | --- | --- |
| `data\textures\walls` | `data\laigter\walls` | normal, `--tile` | `texture.preset` |
| `data\textures\floors` | `data\laigter\floors` | normal, `--tile` | `texture.preset` |
| `data\sprites` | `data\laigter\sprites` | normal, occlusion, parallax (height) | `sprite_op.preset` |
| `data\ui\animo` (ANIMO.GR effects) | `data\laigter\ui\animo` | normal, occlusion, parallax | `sprite_op.preset` |

The engine's own maps (`data\textures\walls_normal`, `floors_normal`) are not touched. NOTES.md says that after
remaking maps, `tools\makedist.py` (or a copy of `data\laigter`) brings them into `dist\UW1`.

## The .preset format

This is Laigter's own format, read by `apply_preset_file` (line 132): one `Name<TAB>value` per line. A name includes
its trailing space where Laigter's does (for example `EnhanceSoft ` then a tab, but `BumpDistance` then a tab with no
space). Lines with an unknown name are ignored, and so are Laigter's `LightSource` lines. The format has no comment
syntax: any other line is just an unknown name, so notes about the presets live here and in NOTES.md.

The names this program reads, and the settings they set (`Settings`, line 68):

| Preset name | Setting |
| --- | --- |
| `EnhanceHeight`, `EnhanceSoft` | Normal depth, normal blur radius |
| `BumpHeight`, `BumpDistance`, `BumpSoft`, `BumpCut` | Bevel depth, distance, blur radius, soft |
| `Tile`, `InvertX`, `InvertY` | Tileable; invert the normal's x / y |
| `ParallaxType`, `BinaryThreshold`, `BinaryFocus`, `ParallaxSoft`, `BinaryMinHeight`, `BinaryErodeDilate`, `HeightMapBrightness`, `HeightMapContrast`, `InvertParallax` | Parallax (type 0 Binary, 1 HeightMap, 2 Quantization, 3 Intervals) |
| `SpecularBlur`, `SpecularBright`, `SpecularContrast`, `SpecularThresh`, `SpecularInvert` | Specular |
| `OcclusionBlur`, `OcclusionBright`, `OcclusionInvert`, `OcclusionThresh`, `OcclusionContrast`, `OcclusionDistance`, `OcclusionDistanceMode` | Occlusion |
| `Normal Use Alpha`, `Specular Use Alpha`, `Occlussion Use Alpha` (Laigter's spelling), `Parallax Use Alpha` | Copy the picture's alpha into the map |

Values are whole numbers, as Laigter's `QByteArray::toInt` reads them. The contrasts are given in thousandths
(`HeightMapContrast 2500` means 2.5). There is one addition of ours: `EnhanceSoft` and `BumpSoft` may be decimals,
such as `2.6` (`qt_to_double`, line 119). A whole number gives exactly Laigter's result.

The three presets:

| Preset | Contents | Used for |
| --- | --- | --- |
| `texture.preset` | EnhanceHeight 275, EnhanceSoft 2.6, BumpHeight 80, BumpDistance 32, BumpSoft 4.1, BumpCut 1 | Walls and floors. Halfway between Laigter's defaults and pixelart, the blurs 25% under halfway (chosen by the user, 2026-11-10). |
| `pixelart.preset` | EnhanceHeight 300, EnhanceSoft 1, BumpHeight 60, BumpDistance 3, BumpSoft 1, BumpCut 1 | A short bevel and little blur, for 16 px pictures. Laigter's defaults made each one a smooth dome. |
| `sprite_op.preset` | pixelart's six lines, plus OcclusionDistanceMode 1, OcclusionDistance 6, OcclusionBlur 1, ParallaxType 1 (HeightMap), HeightMapContrast 2500, HeightMapBrightness 40, ParallaxSoft 1 | Doors, wall and flat objects, effects (chosen by the user, 2026-11-12). Creatures and items used it until 2026-11-16. |
| `item.preset` | `creature.preset` with EnhanceHeight 400 (N4), OcclusionContrast 1000 and OcclusionBright 60 (A4), HeightMapContrast 3000, HeightMapBrightness 0, ParallaxSoft 1 (row 10); run with `--occlusion-thresh-median`. Sheets: `work\laigter\items16b`, `items16c` | Items (`data\sprites\objects`), chosen by the user 2026-11-16 as row 10 of the sheets in `work\laigter\items16b`. They had `creature.preset` for an hour before that. |
| `creature.preset` | EnhanceHeight 250, EnhanceSoft 2, BumpHeight 80, BumpDistance 6, BumpSoft 2, BumpCut 1, OcclusionThresh 30, OcclusionDistance 4, OcclusionContrast 600, OcclusionBright 85, OcclusionBlur 1, HeightMap contrast 1500, brightness 40, ParallaxSoft 2; run with `--occlusion-thresh-median` | Creatures (`data\sprites\creatures`) and, since the same day, items (`data\sprites\objects`; old maps in `work\laigter_backup_2026-11-16_object_maps`, sheets in `work\laigter\items16`); chosen by the user 2026-11-16 as "B" from the sheets in `work\laigter\npc16`. The old maps are in `work\laigter_backup_2026-11-16_creature_maps`. |

## The code (laigter_cli.cpp)

| Line | What |
| --- | --- |
| 1-25 | Header: what is ported, routine by routine, from 1.14.0's `src/image_processor.cpp` and `gui/presets_manager.cpp`, and how Qt's pixel conversions are reproduced. |
| 68 | `Settings`: ImageProcessor's constructor defaults. |
| 106 / 119 | `qt_to_int` (QByteArray::toInt), `qt_to_double` (ours). |
| 132 | `apply_preset_file`. |
| ~200-250 | Qt's conversions: `qt_premul`, `qt_unpremul`, `qt_gray`; `Picture`, `rgba_cimg`, `gray_cimg`. |
| 251 | `Processor`: ImageProcessor's command-line path. It includes `load` (fill_neighbours, which lays the picture out 3x3 for tiling), `calculate`, `calculate_distance`, `modify_distance`, `calculate_normal`, `generate_normal_map`, `modify_parallax` / `calculate_parallax`, `modify_specular` / `calculate_specular`, and `modify_occlusion` / `calculate_occlusion`. |
| 522-574 | A small PNG writer (`crc_init`, `chunk`, `png_save`). |
| 575 | `save_map` (with the use-alpha copy). |
| 591 | `load_picture`, through our `png_read`, made premultiplied as QImage would be. |
| 615 | `usage`. |
| 640 | `main`: options, file collection as Laigter's main.cpp does it, and a thread pool. |

## Proving a change

NOTES.md describes the proof. The real `laigter.exe` 1.14.0 runs headless
(`O:\appsnotonc\laigter-release\unpacked\laigter-windows\laigter.exe --no-gui -d IN -n -c -o -p [-r P] -l OUT`), and
`work\laigter\compare.py REF OURS` compares the outputs. The references are in `work\laigter\ref`, `ref_tile` and
`few_ref`. The recorded result: Qt's grey matched on all 16.7M colours and 65,536 alpha samples, and 17,388 maps
were pixel-identical. The user wants only a few such checks, not runs over the whole set.
