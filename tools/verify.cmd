@echo off
rem the ported routines against UW.EXE's own (run in an emulator): tools\verify.py
set UV_CACHE_DIR=O:\appsnotonc\sentinel\uv\cache
uv run --no-project --with unicorn==2.0.1.post1 --with capstone python -I "%~dp0verify.py" "%~dp0..\game" "%~dp0..\build\rtest.exe" 1000
if errorlevel 1 exit /b 1
rem the sound effects (seg 1bd8) and the music choice (seg 1bd8)
uv run --no-project --with unicorn==2.0.1.post1 python -I "%~dp0sfx_verify.py" "%~dp0..\game" "%~dp0..\build\rtest.exe" 2000
if errorlevel 1 exit /b 1
uv run --no-project --with unicorn==2.0.1.post1 python -I "%~dp0songs_verify.py" "%~dp0..\game" "%~dp0..\build\rtest.exe" 4000
if errorlevel 1 exit /b 1
rem the terrain collision (seg 2529) on the nine real levels
uv run --no-project --with unicorn==2.0.1.post1 python -I "%~dp0collide_verify.py" "%~dp0..\game" "%~dp0..\build\rtest.exe" 600
if errorlevel 1 exit /b 1
rem the level hunters' navigation (hunternav.c) on ASCII maps
"%~dp0..\build\hunternav_test.exe"
if errorlevel 1 exit /b 1
rem the level hunters' side file (hunterfile.c): records in and out (its files in build\)
cd /d "%~dp0.." && "%~dp0..\build\hunterfile_test.exe"
if errorlevel 1 exit /b 1
rem the level hunters' routes on the nine levels (hunter.c's grid and hnav against the original walker's rule from the
rem level file, two off-screen runs of the built port, about two and a half minutes): tools\huntercheck.py routes
set HUNTERCHECK_EXE=%~dp0..\underworld-exhumed\uw1\build\port\uw1port.exe
uv run --no-project python -I "%~dp0huntercheck.py" routes
if errorlevel 1 exit /b 1
rem the conversation's record (port\win32\uwconvrec.c, the hooks in CONVERSE.C / SCROLLIO.C) against STRINGS.PAK:
rem Hagbard (conversation 19) and the Outcast (282), two off-screen runs of the built port, about two minutes
uv run --no-project python -I "%~dp0convcheck.py" 243 239
if errorlevel 1 exit /b 1
rem our conversation screen's answers (uwconv.c): a click on our answer 2 gives the same conversation as the key 2,
rem with the smooth font and the original's (four off-screen runs, about four minutes)
uv run --no-project python -I "%~dp0convcheck.py" click
if errorlevel 1 exit /b 1
rem a typed answer (babl_ask) through our box: Steeltoe's question, "dwarf" edited to "dwaxrf" by the original's wdialog
uv run --no-project python -I "%~dp0convcheck.py" ask
if errorlevel 1 exit /b 1
rem our map (uwconv.c draw_map, the DLL's uwv_map) only draws: a note made on it is saved as on the original's map
rem (SAVE0\LEV.ARK the same bytes with UWPORT_ORIGMAP=1)
uv run --no-project python -I "%~dp0convcheck.py" map
if errorlevel 1 exit /b 1
rem the keyboard's ring (keyqueue.c): no make or break lost when the game stops reading, results as before
"%~dp0..\build\keyqueue_test.exe"
if errorlevel 1 exit /b 1
rem the window's input module (port\win32\uwinput.c): owners of each press, the key table, mouse-look's want
"%~dp0..\build\uwinput_test.exe"
if errorlevel 1 exit /b 1
rem our Esc menu (tools\menucheck.py: legible text, headings, the model's whole name, every setting a row; its other
rem modes, keys / kept / dlss, are run by hand: about ten minutes of off-screen runs)
uv run --no-project python -I "%~dp0menucheck.py" smooth
if errorlevel 1 exit /b 1
