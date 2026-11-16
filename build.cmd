@echo off
rem Build uo-new with clang (plain C17, MinGW target, lld). Output in build\.
rem   build.cmd          shaders (DXC), check.exe, uw.exe and uw_nvngx.dll (the DLSS 5 bridge)
rem   build.cmd debug    the same with undefined-behaviour traps and no optimisation
call O:\appsnotonc\llvm\env.cmd
cd /d "%~dp0"
if not exist build mkdir build
if not exist build\gen mkdir build\gen
rem the shaders: compiled here into headers (build\gen) that uw.exe carries; DXC is only needed to build
set DXC=O:\appsnotonc\dxc\v1.9.2609\bin\x64\dxc.exe
set SH=src\shaders
%DXC% -T vs_6_0 -E vs -Fh build\gen\scene_vs.h -Vn SCENE_VS %SH%\scene.hlsl || exit /b 1
%DXC% -T ps_6_0 -E ps -Fh build\gen\scene_ps.h -Vn SCENE_PS %SH%\scene.hlsl || exit /b 1
%DXC% -T ps_6_5 -E ps -D RT -Fh build\gen\scene_ps_rt.h -Vn SCENE_PS_RT %SH%\scene.hlsl || exit /b 1
%DXC% -T ps_6_5 -E ps -D RT -D RTHIGH -Fh build\gen\scene_ps_rth.h -Vn SCENE_PS_RTH %SH%\scene.hlsl || exit /b 1
%DXC% -T cs_6_5 -E cs -Fh build\gen\probes_cs.h -Vn PROBES_CS %SH%\probes.hlsl || exit /b 1
%DXC% -T vs_6_0 -E vs -Fh build\gen\present_vs.h -Vn PRESENT_VS %SH%\present.hlsl || exit /b 1
%DXC% -T ps_6_0 -E ps -Fh build\gen\present_ps.h -Vn PRESENT_PS %SH%\present.hlsl || exit /b 1
set OPT=-O2
if "%1"=="debug" set OPT=-O0 -g -fsanitize=undefined -fsanitize-trap=undefined
rem build.cmd new: build\uw_new.exe (when uw.exe is running and cannot be replaced; the DLSS 5 bridge is left as it is)
set EXE=uw.exe
if "%1"=="new" set EXE=uw_new.exe
set CF=%CLANG_FLAGS% -std=c17 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers %OPT% -I src -I dlss\include -I build\gen
set COMMON=src\base.c src\png.c src\gr.c src\audio.c src\conv.c src\extract.c src\level.c src\normals.c src\lights.c src\world.c src\models.c src\objdata.c src\orig.c src\xmidi.c src\sound.c src\sfx.c src\songs.c src\collide.c
rem Munt (the Roland emulator) as build\mt32emu.dll, made once
if not exist build\mt32emu.dll call external\mt32emu\build-mt32emu.cmd || exit /b 1
cd /d "%~dp0"
clang %CF% -o build\rtest.exe tests\rtest.c src\orig.c src\sfx.c src\songs.c src\collide.c || exit /b 1
rem the level hunters' pure parts (underworld-exhumed, port only) and their C tests, run by tools\verify.cmd
clang %CLANG_FLAGS% -std=c17 -O2 -Wall -o build\hunternav_test.exe underworld-exhumed\uw1\tests\hunternav_test.c underworld-exhumed\uw1\src\port\sys\hunternav.c || exit /b 1
rem the original's 3D models decoded from game\UW.EXE (src\models.c): every model runs to its end (tests\models_test.c)
clang %CLANG_FLAGS% -std=c17 -O2 -Wall -I src -o build\models_test.exe tests\models_test.c src\models.c src\base.c || exit /b 1
build\models_test.exe game || exit /b 1
clang %CLANG_FLAGS% -std=c17 -O2 -Wall -o build\hunterfile_test.exe underworld-exhumed\uw1\tests\hunterfile_test.c underworld-exhumed\uw1\src\port\sys\hunterfile.c || exit /b 1
rem the keyboard's ring (underworld-exhumed's port\sys\keyqueue.c) with seg063 stubbed, run by tools\verify.cmd;
rem port.h is Exhume's runtime's (port_setw), portgame.h underworld-exhumed's
clang %CLANG_FLAGS% -std=c17 -O2 -Wall -IO:\appsnotonc\exhume\runtime\port -Iunderworld-exhumed\uw1\src\port -o build\keyqueue_test.exe underworld-exhumed\uw1\tests\keyqueue_test.c underworld-exhumed\uw1\src\port\sys\keyqueue.c || exit /b 1
rem the window's input module (port\win32\uwinput.c, pure C) and its test, run by tools\verify.cmd
clang %CLANG_FLAGS% -std=c17 -O2 -Wall -Iport\win32 -o build\uwinput_test.exe tests\uwinput_test.c port\win32\uwinput.c || exit /b 1
rem the rules core: underworld-exhumed's byte-matched C as it is (tools\core.py: build\core\core.a and the stubs), and
rem build\coretest.exe, which the proofs drive (tools\proofs.cmd)
set UV_CACHE_DIR=O:\appsnotonc\sentinel\uv\cache
uv run --no-project python -I tools\core.py || exit /b 1
clang %CLANG_FLAGS% -O1 -w -o build\coretest.exe tests\coretest.c build\gen\core_tab.c build\gen\core_stubs.c build\core\core.a || echo (build\coretest.exe is in use by a proof: left as it was)
clang %CF% -o build\check.exe src\check.c %COMMON% -lwinmm || exit /b 1
clang %CF% -o build\%EXE% src\main.c src\render.c src\ngx.c src\game.c src\inventory.c src\ui.c src\smoothtext.c %COMMON% build\gen\core_stubs.c build\core\core.a -ld3d12 -ldxgi -ldxguid -luser32 -lgdi32 -ladvapi32 -lwinmm || exit /b 1
rem the DLSS 5 bridge: its calls must return into it (no tail calls), see src\nrbridge.c
if not "%1"=="new" clang %CF% -fno-optimize-sibling-calls -shared -o build\uw_nvngx.dll src\nrbridge.c || exit /b 1
echo built
