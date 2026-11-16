@echo off
rem the rules core's proofs (tools\coreproof.py): UW.EXE's routines in the emulator beside underworld-exhumed's C as
rem we build it (tools\core.py, build\coretest.exe). Each prints its counts and "all the same", or where it differs.
rem   tools\proofs.cmd            every proof          tools\proofs.cmd objects     one (tools\proof_objects.py)
set PY=%~dp0..\work\venv\Scripts\python.exe
if not exist "%PY%" (
  set UV_CACHE_DIR=O:\appsnotonc\sentinel\uv\cache
  uv venv -q --python 3.10 "%~dp0..\work\venv" || exit /b 1
  uv pip install -q --python "%PY%" unicorn==2.0.1.post1 capstone "setuptools<70" || exit /b 1
)
set G=%~dp0..\game
set T=%~dp0..\build\coretest.exe
if not "%1"=="" ( "%PY%" -u "%~dp0proof_%1.py" "%G%" "%T%" %2 & exit /b )
for %%f in ("%~dp0proof_*.py") do ( "%PY%" -u "%%f" "%G%" "%T%" || exit /b 1 )
