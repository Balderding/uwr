@echo off
rem export.cmd MERGED_DIR NAME: the merged model to GGUF through our own llama.cpp source (the runtime the game
rem ships), then quantised: NAME-f16.gguf, NAME-Q8_0.gguf, NAME-Q4_K_M.gguf in %~dp0gguf\.
rem llama-quantize.exe comes from uo-new work\buildquant.cmd (its own CPU-only llama.cpp tree, build\llama-tools; built 2026-11-13, runs; libwinpthread-1.dll beside it).
call "%~dp0env.cmd"
set LL=%~dp0llama-tools
set Q=%~dp0llama-tools\bin\llama-quantize.exe
if not exist "%~dp0gguf" mkdir "%~dp0gguf"
"%~dp0python\python.exe" "%LL%\convert_hf_to_gguf.py" "%1" --outtype f16 --outfile "%~dp0gguf\%2-f16.gguf" || exit /b 1
if not exist "%Q%" ( echo export.cmd: no %Q% - build it first; the f16 GGUF is written & exit /b 1 )
"%Q%" "%~dp0gguf\%2-f16.gguf" "%~dp0gguf\%2-Q8_0.gguf" Q8_0 || exit /b 1
"%Q%" "%~dp0gguf\%2-f16.gguf" "%~dp0gguf\%2-Q4_K_M.gguf" Q4_K_M || exit /b 1
echo exported %~dp0gguf\%2-*.gguf
