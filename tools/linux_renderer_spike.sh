#!/bin/bash
# linux_renderer_spike.sh: the compile spike of docs\LINUX_RENDERER.md. Our D3D12 renderer's files compiled for x86-64
# Linux (syntax only) against vkd3d-proton's headers (X:\tools\vkd3d-proton, v3.1, its .idl turned into C headers
# by MinGW's widl), and the rest of uwview.dll against port\win32\uwos.h; the errors counted by kind.
# Run from Git Bash: bash tools/linux_renderer_spike.sh   (output in work\linux\spike)
set -u
R=/o/uw/uo-new
S=$R/work/linux/spike
V=/x/tools/vkd3d-proton
CC=/x/tools/llvm/23.1.4/bin/clang.exe
mkdir -p "$S/inc" "$S/shim" "$S/shim2"
cd "$V/include" || exit 1
for f in vkd3d_d3d12 vkd3d_d3d12sdklayers vkd3d_d3dcommon vkd3d_dxcapi vkd3d_dxgi vkd3d_dxgi1_2 vkd3d_dxgi1_3 vkd3d_dxgi1_4 \
         vkd3d_dxgi1_5 vkd3d_dxgibase vkd3d_dxgiformat vkd3d_dxgitype vkd3d_swapchain_factory vkd3d_command_list_vkd3d_ext \
         vkd3d_command_queue_vkd3d_ext vkd3d_device_vkd3d_ext vkd3d_core_interface; do
    /x/tools/gcc/mingw64/bin/widl.exe -h -o "$S/inc/$f.h" "$f.idl"
done
# the Windows SDK's names onto vkd3d-proton's (the renderer's files); windows.h onto uwos.h (the others)
printf '#pragma once\n#include <vkd3d_windows.h>\n' > "$S/shim/windows.h"
printf '#pragma once\n#include <vkd3d_windows.h>\n#include <vkd3d_d3d12.h>\n' > "$S/shim/d3d12.h"
printf '#pragma once\n#include <vkd3d_dxgi1_4.h>\n' > "$S/shim/dxgi1_4.h"
printf '#pragma once\n#include <vkd3d_dxgi1_5.h>\n' > "$S/shim/dxgi1_6.h"
printf '#pragma once\n#include "uwos.h"\n' > "$S/shim2/windows.h"
cd "$R" || exit 1
FLAGS="--target=x86_64-linux-gnu --sysroot=X:/tools/linux-sysroot -std=c17 -D_GNU_SOURCE -fsyntax-only -ferror-limit=0 -Wno-everything"
INC="-I$S/shim -I$S/inc -I$V/include -I$V/khronos/Vulkan-Headers/include -Isrc -Idlss/include -Ibuild/gen"
for f in render render_decals render_particles ngx nrbridge; do
    $CC $FLAGS $INC src/$f.c > "$S/$f.txt" 2>&1
    echo "$f.c: $(grep -c 'error:' "$S/$f.txt") errors"
done
for f in uwview level extract uwrec uwconv_draw smoothtext uwmind audio sound; do
    $CC $FLAGS -I$S/shim2 -Iport/win32 $INC src/$f.c > "$S/$f.txt" 2>&1
    echo "$f.c: $(grep -c 'error:' "$S/$f.txt") errors"
done
