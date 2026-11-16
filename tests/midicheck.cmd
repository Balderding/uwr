@echo off
call O:\appsnotonc\llvm\env.cmd
cd /d "%~dp0.."
clang %CLANG_FLAGS% -O2 -o build\midicheck.exe tests\midicheck.c -lwinmm || exit /b 1
build\midicheck.exe
