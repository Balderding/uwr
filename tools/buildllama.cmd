@echo off
rem buildllama.cmd: llama.cpp (external\llama.cpp, b12131, MIT) built with our clang into build\llama, for our own
rem model runner (LLM_NPC_AI.md, "Our own Strata"). Out of source: external\llama.cpp is never written to.
rem   The core as DLLs (llama.dll, ggml.dll, ggml-base.dll) and every CPU variant as its own DLL
rem   (GGML_BACKEND_DL + GGML_CPU_ALL_VARIANTS): at start ggml loads the best one this PC's processor has, so one
rem   build runs on any x64 PC. A GPU backend (ggml-vulkan.dll) is another DLL beside them, loaded when present.
rem   tools\buildllama.cmd          CPU
rem   tools\buildllama.cmd vulkan   CPU + Vulkan (the Vulkan SDK in O:\appsnotonc\vulkan-sdk: glslc + headers; only
rem                                 for building, players need just their graphics driver)
call O:\appsnotonc\llvm\env.cmd
set PATH=%PATH%;O:\appsnotonc\gcc\mingw64\bin
cd /d "%~dp0.."
set VK=-DGGML_VULKAN=OFF
if /i not "%1"=="vulkan" goto novk
set VULKAN_SDK=O:\appsnotonc\vulkan-sdk\1.4.363.1
set PATH=%VULKAN_SDK%\Bin;%PATH%
rem the shader generator (vulkan-shaders-gen, run only while building) is its own CMake project that does not get
rem the -D settings below; llama.cpp hands it a toolchain file only when "cross-compiling", which naming the
rem system (Windows, as it is) turns on
set VK=-DGGML_VULKAN=ON -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=AMD64 -DGGML_VULKAN_SHADERS_GEN_TOOLCHAIN=O:/uw/uo-new/tools/clang-mingw-host.cmake
:novk
cmake --fresh -G Ninja -S external\llama.cpp -B build\llama -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER_TARGET=x86_64-w64-windows-gnu ^
  -DCMAKE_CXX_COMPILER_TARGET=x86_64-w64-windows-gnu -DCMAKE_SYSROOT=O:/appsnotonc/gcc/mingw64 ^
  "-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld -static-libgcc -static-libstdc++" ^
  "-DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld -static-libgcc -static-libstdc++" ^
  "-DCMAKE_MODULE_LINKER_FLAGS=-fuse-ld=lld -static-libgcc -static-libstdc++" ^
  "-DCMAKE_C_STANDARD_LIBRARIES=-Wl,-Bstatic -lwinpthread -Wl,-Bdynamic" ^
  "-DCMAKE_CXX_STANDARD_LIBRARIES=-Wl,-Bstatic -lwinpthread -Wl,-Bdynamic" ^
  -DBUILD_SHARED_LIBS=ON -DGGML_BACKEND_DL=ON -DGGML_CPU_ALL_VARIANTS=ON -DGGML_NATIVE=OFF -DGGML_OPENMP=OFF %VK% ^
  -DLLAMA_CURL=OFF -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_SERVER=OFF -DLLAMA_BUILD_TOOLS=OFF -DLLAMA_BUILD_APP=OFF ^
  -DLLAMA_BUILD_COMMON=OFF || exit /b 1
cmake --build build\llama -j 16 || exit /b 1
echo built llama.cpp into build\llama\bin
