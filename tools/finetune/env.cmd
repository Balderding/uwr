@echo off
rem env.cmd: the fine-tune environment. Every cache stays inside this folder; python\ is this folder's own Python
rem (see README.md, "Set up").
set FT=%~dp0
if "%FT:~-1%"=="\" set FT=%FT:~0,-1%
set HF_HOME=%FT%\cache\hf
set HF_HUB_CACHE=%FT%\cache\hf\hub
set TORCH_HOME=%FT%\cache\torch
set TRITON_CACHE_DIR=%FT%\cache\triton
set XDG_CACHE_HOME=%FT%\cache
if not exist "%FT%\cache\tmp" mkdir "%FT%\cache\tmp"
set TEMP=%FT%\cache\tmp
set TMP=%FT%\cache\tmp
set PATH=%FT%\python;%FT%\python\Scripts;%PATH%
