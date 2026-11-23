@echo off
rem buildmind.cmd: tests\uwmind_bench.c (our own Strata's first step) against build\llama (tools\buildllama.cmd first),
rem into build\llama\bin beside the llama.cpp DLLs.
call "%~dp0uwr_env.cmd"
cd /d "%~dp0.."
set L=build\llama
clang %CLANG_FLAGS% -O2 -Wall -I external\llama.cpp\include -I external\llama.cpp\ggml\include ^
  -o %L%\bin\uwmind_bench.exe tests\uwmind_bench.c %L%\src\libllama.dll.a %L%\ggml\src\libggml.dll.a ^
  %L%\ggml\src\libggml-base.dll.a -ldxgi -ldxguid || exit /b 1
echo built %L%\bin\uwmind_bench.exe
rem uwmind.dll (src\uwmind.c): the runner the game loads at run time (port\win32\uwmindhost.c), beside the llama DLLs
clang %CLANG_FLAGS% -O2 -Wall -shared -I external\llama.cpp\include -I external\llama.cpp\ggml\include ^
  -o %L%\bin\uwmind.dll src\uwmind.c %L%\src\libllama.dll.a %L%\ggml\src\libggml.dll.a ^
  %L%\ggml\src\libggml-base.dll.a || exit /b 1
echo built %L%\bin\uwmind.dll
rem uwmind_score.exe (tests\uwmind_score.c): uwmind.dll timed as the game uses it, old way against new
clang %CLANG_FLAGS% -O2 -Wall -o %L%\bin\uwmind_score.exe tests\uwmind_score.c || exit /b 1
echo built %L%\bin\uwmind_score.exe
