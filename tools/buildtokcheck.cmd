@echo off
rem buildtokcheck.cmd: tests\uwmind_tokcheck.c (uwmind.c's choice-token check on llama.cpp's vocabulary-only files)
rem into build\llama\bin, then run on a SentencePiece vocabulary (must fail the check) and GPT-2 style ones (pass).
call O:\appsnotonc\llvm\env.cmd
cd /d "%~dp0.."
set L=build\llama
set M=external\llama.cpp\models
clang %CLANG_FLAGS% -O2 -Wall -Wno-unused-function -I external\llama.cpp\include -I external\llama.cpp\ggml\include ^
  -o %L%\bin\uwmind_tokcheck.exe tests\uwmind_tokcheck.c %L%\src\libllama.dll.a %L%\ggml\src\libggml.dll.a ^
  %L%\ggml\src\libggml-base.dll.a || exit /b 1
echo built %L%\bin\uwmind_tokcheck.exe
%L%\bin\uwmind_tokcheck.exe %M%\ggml-vocab-llama-spm.gguf 0 %M%\ggml-vocab-phi-3.gguf 0 %M%\ggml-vocab-qwen2.gguf 1 ^
  %M%\ggml-vocab-qwen35.gguf 1 %M%\ggml-vocab-gpt-2.gguf 1 %M%\ggml-vocab-llama-bpe.gguf 1
