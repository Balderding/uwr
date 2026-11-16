@echo off
rem builds and runs tests\mt32test.c (Munt + the CM-32L ROMs -> build\mt32test.wav)
call O:\appsnotonc\llvm\env.cmd
cd /d "%~dp0.."
clang %CLANG_FLAGS% -O2 -o build\mt32test.exe tests\mt32test.c || exit /b 1
build\mt32test.exe
