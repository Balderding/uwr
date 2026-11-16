@echo off
rem our MT32MPU.ADV port (src\xmidi.c) against UW's own driver run in the emulator: every song (the first %1 ticks,
rem default 3000) and every driver call script in tests\xmidi_scripts; any difference fails
call O:\appsnotonc\llvm\env.cmd
cd /d "%~dp0.."
clang %CLANG_FLAGS% -std=c17 -O2 -Wall -Wextra -Wno-unused-parameter -I src -o build\xmidi_test.exe tests\xmidi_test.c src\xmidi.c || exit /b 1
set UV_CACHE_DIR=O:\appsnotonc\sentinel\uv\cache
set TICKS=%1
if "%TICKS%"=="" set TICKS=3000
uv run --no-project --with unicorn==2.0.1.post1 python -I tools\xmidi_verify.py %TICKS% || exit /b 1
uv run --no-project --with unicorn==2.0.1.post1 python -I tools\xmidi_script.py tests\xmidi_scripts || exit /b 1
