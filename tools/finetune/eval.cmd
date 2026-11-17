@echo off
rem eval.cmd MODEL DATA.jsonl [--gold GOLD.jsonl]: the first-token score of a stock or merged model, no training
call "%~dp0env.cmd"
set M=%1
if "%M%"=="1.2B" set M=%~dp0models\LFM2.5-1.2B-Instruct
if "%M%"=="350M" set M=%~dp0models\LFM2.5-350M
shift /1
"%~dp0python\python.exe" "%~dp0train.py" "%M%" --eval %1 %2 %3 %4 %5
