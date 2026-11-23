@echo off
rem uwr_env.cmd: the compilers' environment for uwr's builds (2026-11-22, the move to X:). One variable: UWR_TOOLS, the
rem folder of the compilers and SDKs (default X:\tools: llvm, gcc, dxc, vulkan-sdk, dlss-sdk, sdl3, Exhume ...).
rem Everything of uwr's own is found from this file's folder. clang targets MinGW (x86_64-w64-windows-gnu), its
rem headers and libraries from %UWR_TOOLS%\gcc\mingw64, the linker LLVM's lld; temporary files in build\tmp.
rem   clang   %CLANG_FLAGS% -o x.exe x.c
if not defined UWR_TOOLS set UWR_TOOLS=X:\tools
set UWR_TMP=%~dp0..\build\tmp
if not exist "%UWR_TMP%" mkdir "%UWR_TMP%"
set PATH=%UWR_TOOLS%\llvm\23.1.4\bin;%PATH%
set TMP=%UWR_TMP%
set TEMP=%UWR_TMP%
set TMPDIR=%UWR_TMP%
set CLANG_FLAGS=--target=x86_64-w64-windows-gnu --sysroot=%UWR_TOOLS:\=/%/gcc/mingw64 -fuse-ld=lld
