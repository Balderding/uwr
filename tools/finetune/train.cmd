@echo off
rem train.cmd MODEL DATA.jsonl OUT [train.py options]: one LoRA run on the 4090 (the user starts it; see README.md)
rem   train.cmd 1.2B data\labelled.jsonl runs\lfm12b-1
rem   train.cmd 350M data\labelled.jsonl runs\lfm350m-1 --lr 5e-4
call "%~dp0env.cmd"
set M=%1
if "%M%"=="1.2B" set M=%~dp0models\LFM2.5-1.2B-Instruct
if "%M%"=="350M" set M=%~dp0models\LFM2.5-350M
shift /1
"%~dp0python\python.exe" "%~dp0train.py" "%M%" %1 %2 %3 %4 %5 %6 %7 %8 %9
