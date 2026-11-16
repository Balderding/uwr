@echo off
rem Normal maps for UW's world art with laigter-cli (Laigter 1.14.0's maths), into data\laigter.
rem Walls and floors tile (Laigter's "Tile" on); sprites (creatures, objects, wall/flat details, doors) do not.
rem The engine's own maps (data\textures\walls_normal, floors_normal) are not touched.
rem   make-normals.cmd          only remakes maps older than their picture
rem   make-normals.cmd all      remakes every map
cd /d "%~dp0..\.."
if not exist build\laigter-cli.exe call tools\laigter-cli\build.cmd || exit /b 1
set CC=--check-changes
if "%1"=="all" set CC=
rem walls and floors: tools\laigter-cli\texture.preset (halfway between Laigter's defaults and pixelart.preset, the
rem blurs 25% under halfway; the user chose it 2026-11-10)
rem plus (the user, 2026-11-14) occlusion in the joints (NNN_o: 3 px, soft; the threshold per texture, half its median
rem grey + 1: --occlusion-thresh-median) and the height map the renderer casts the relief's shadows from (NNN_p)
build\laigter-cli.exe -q -n -o -p %CC% --tile --occlusion-thresh-median -r tools\laigter-cli\texture.preset -d data\textures\walls  -l data\laigter\walls  || exit /b 1
build\laigter-cli.exe -q -n -o -p %CC% --tile --occlusion-thresh-median -r tools\laigter-cli\texture.preset -d data\textures\floors -l data\laigter\floors || exit /b 1
rem sprites (16 px items, creatures): tools\laigter-cli\sprite_op.preset: pixelart.preset (short bevel, little blur, more surface; the
rem user chose it 2026-11-10 over Laigter's defaults, which make every small picture one smooth dome) plus occlusion (a soft
rem rim toward the silhouette, distance 6) and height (HeightMap, contrast 2.5, +40) maps, NAME_o and NAME_p beside NAME_n
rem (the user chose them 2026-11-12 from work\laigter\try\sheet_op_F.png); the renderer's loader takes them up (uwr's task)
build\laigter-cli.exe -q -n -o -p %CC% -r tools\laigter-cli\sprite_op.preset -d data\sprites -l data\laigter\sprites || exit /b 1
rem the effects (ANIMO.GR: blood, sparks, dust, splashes, spell effects): the items' preset (the user, 2026-11-11)
build\laigter-cli.exe -q -n -o -p %CC% -r tools\laigter-cli\sprite_op.preset -d data\ui\animo -l data\laigter\ui\animo || exit /b 1
echo normal maps in data\laigter
