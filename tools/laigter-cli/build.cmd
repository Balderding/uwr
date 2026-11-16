@echo off
rem laigter-cli: Laigter 1.14.0's map maker without Qt (GPL v3, see LICENSE-GPL3.txt). Builds build\laigter-cli.exe.
rem Its own program: the engine (uw.exe) never links it, it only reads the PNGs it writes.
call O:\appsnotonc\llvm\env.cmd
cd /d "%~dp0"
if not exist ..\..\build mkdir ..\..\build
if not exist ..\..\build\laigter-obj mkdir ..\..\build\laigter-obj
clang %CLANG_FLAGS% -std=c17 -O2 -w -I ..\..\src -c ..\..\src\base.c -o ..\..\build\laigter-obj\base.o || exit /b 1
clang %CLANG_FLAGS% -std=c17 -O2 -w -I ..\..\src -c ..\..\src\png.c -o ..\..\build\laigter-obj\png.o || exit /b 1
rem -O3 like Laigter's release; no fused multiply-add, so the float maths rounds as Laigter's does
clang++ %CLANG_FLAGS% -std=c++17 -O3 -ffp-contract=off -w -c laigter_cli.cpp -o ..\..\build\laigter-obj\laigter_cli.o || exit /b 1
clang++ %CLANG_FLAGS% -static -o ..\..\build\laigter-cli.exe ..\..\build\laigter-obj\laigter_cli.o ..\..\build\laigter-obj\base.o ..\..\build\laigter-obj\png.o -lpthread || exit /b 1
echo built build\laigter-cli.exe
