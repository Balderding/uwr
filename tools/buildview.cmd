@echo off
rem buildview.cmd: our D3D12 renderer as build\uwview.dll for the original game's port (src\uwview.c over
rem src\render.c), and the DLSS 5 bridge build\uw_nvngx.dll (src\nrbridge.c). tools\portwin32.py runs it.
call O:\appsnotonc\llvm\env.cmd
cd /d "%~dp0.."
rem UWR_AREA=NAME (the user, 2026-11-17: every session builds in its own private area): everything this makes goes
rem to build/areas/NAME instead of build, so sessions build at the same time without the build lock
set B=build
if not "%UWR_AREA%"=="" set B=build\areas\%UWR_AREA%
if not exist %B%\gen mkdir %B%\gen
rem the shaders, as build.cmd compiles them (into build\gen headers the renderer carries)
set DXC=O:\appsnotonc\dxc\v1.9.2609\bin\x64\dxc.exe
set SH=src\shaders
%DXC% -T vs_6_0 -E vs -Fh %B%\gen\scene_vs.h -Vn SCENE_VS %SH%\scene.hlsl || exit /b 1
%DXC% -T ps_6_0 -E ps -Fh %B%\gen\scene_ps.h -Vn SCENE_PS %SH%\scene.hlsl || exit /b 1
%DXC% -T ps_6_5 -E ps -D RT -Fh %B%\gen\scene_ps_rt.h -Vn SCENE_PS_RT %SH%\scene.hlsl || exit /b 1
%DXC% -T ps_6_5 -E ps -D RT -D RTHIGH -Fh %B%\gen\scene_ps_rth.h -Vn SCENE_PS_RTH %SH%\scene.hlsl || exit /b 1
%DXC% -T cs_6_5 -E cs -Fh %B%\gen\probes_cs.h -Vn PROBES_CS %SH%\probes.hlsl || exit /b 1
%DXC% -T vs_6_0 -E vs -Fh %B%\gen\present_vs.h -Vn PRESENT_VS %SH%\present.hlsl || exit /b 1
%DXC% -T ps_6_0 -E ps -Fh %B%\gen\present_ps.h -Vn PRESENT_PS %SH%\present.hlsl || exit /b 1
rem the Vulkan renderer's (uwview_vk.dll, src\render_vk.c): the same HLSL as SPIR-V, b0 at binding 0, tN at 16 + N,
rem uN at 48 + N (docs\superpowers\plans\2026-11-15-vulkan-renderer.md)
rem -fvk-use-dx-layout: structured buffers' structs packed as D3D12's (a Vertex 76 bytes, not std430's 96), so the C
rem structs serve both renderers (checked after the build: tools\vklayout.py)
set SPV=-spirv -fspv-target-env=vulkan1.3 -fvk-use-dx-layout -fvk-b-shift 0 0 -fvk-t-shift 16 0 -fvk-u-shift 48 0 -fvk-s-shift 64 0
%DXC% %SPV% -T vs_6_0 -E vs -Fh %B%\gen\present_vs_spv.h -Vn PRESENT_VS_SPV %SH%\present.hlsl || exit /b 1
%DXC% %SPV% -T ps_6_0 -E ps -Fh %B%\gen\present_ps_spv.h -Vn PRESENT_PS_SPV %SH%\present.hlsl || exit /b 1
rem the scene without ray tracing (M2). No -fvk-invert-y: render_vk.c draws it with a negative-height viewport
%DXC% %SPV% -T vs_6_0 -E vs -Fh %B%\gen\scene_vs_spv.h -Vn SCENE_VS_SPV %SH%\scene.hlsl || exit /b 1
%DXC% %SPV% -T ps_6_0 -E ps -Fh %B%\gen\scene_ps_spv.h -Vn SCENE_PS_SPV %SH%\scene.hlsl || exit /b 1
rem ray tracing (M4, src\render_vk_rt.h): the ray-traced scene and the probes (inline RayQuery: SPV_KHR_ray_query)
%DXC% %SPV% -T ps_6_5 -E ps -D RT -Fh %B%\gen\scene_ps_rt_spv.h -Vn SCENE_PS_RT_SPV %SH%\scene.hlsl || exit /b 1
%DXC% %SPV% -T cs_6_5 -E cs -Fh %B%\gen\probes_cs_spv.h -Vn PROBES_CS_SPV %SH%\probes.hlsl || exit /b 1
rem ray tracing High (M5: Ray Reconstruction's guides, src\render_vk_dlss.h)
%DXC% %SPV% -T ps_6_5 -E ps -D RT -D RTHIGH -Fh %B%\gen\scene_ps_rth_spv.h -Vn SCENE_PS_RTH_SPV %SH%\scene.hlsl || exit /b 1
rem the effects' passes for the Vulkan renderer (M3, src\render_vk_fx.h): particles.hlsl and decal.hlsl the same way
%DXC% %SPV% -T cs_6_0 -E cs_spawn -D CS -Fh %B%\gen\particles_cs_spawn_spv.h -Vn PARTICLES_CS_SPAWN_SPV %SH%\particles.hlsl || exit /b 1
%DXC% %SPV% -T cs_6_0 -E cs_update -D CS -Fh %B%\gen\particles_cs_update_spv.h -Vn PARTICLES_CS_UPDATE_SPV %SH%\particles.hlsl || exit /b 1
%DXC% %SPV% -T vs_6_0 -E vs -Fh %B%\gen\particles_vs_spv.h -Vn PARTICLES_VS_SPV %SH%\particles.hlsl || exit /b 1
%DXC% %SPV% -T ps_6_0 -E ps -Fh %B%\gen\particles_ps_spv.h -Vn PARTICLES_PS_SPV %SH%\particles.hlsl || exit /b 1
%DXC% %SPV% -T vs_6_0 -E vs -Fh %B%\gen\decal_vs_spv.h -Vn DECAL_VS_SPV %SH%\decal.hlsl || exit /b 1
%DXC% %SPV% -T ps_6_0 -E ps -Fh %B%\gen\decal_ps_spv.h -Vn DECAL_PS_SPV %SH%\decal.hlsl || exit /b 1
rem the fog and smoke (src\render_vk_vol.h, the Vulkan renderer's alone): volume.hlsl's passes; set 1 by its own vk::binding
%DXC% %SPV% -T cs_6_0 -E cs_advect -Fh %B%\gen\volume_cs_advect_spv.h -Vn VOLUME_CS_ADVECT_SPV %SH%\volume.hlsl || exit /b 1
%DXC% %SPV% -T cs_6_0 -E cs_curl -Fh %B%\gen\volume_cs_curl_spv.h -Vn VOLUME_CS_CURL_SPV %SH%\volume.hlsl || exit /b 1
%DXC% %SPV% -T cs_6_0 -E cs_mac -Fh %B%\gen\volume_cs_mac_spv.h -Vn VOLUME_CS_MAC_SPV %SH%\volume.hlsl || exit /b 1
%DXC% %SPV% -T cs_6_0 -E cs_div -Fh %B%\gen\volume_cs_div_spv.h -Vn VOLUME_CS_DIV_SPV %SH%\volume.hlsl || exit /b 1
%DXC% %SPV% -T cs_6_0 -E cs_jac_ab -Fh %B%\gen\volume_cs_jac_ab_spv.h -Vn VOLUME_CS_JAC_AB_SPV %SH%\volume.hlsl || exit /b 1
%DXC% %SPV% -T cs_6_0 -E cs_jac_ba -Fh %B%\gen\volume_cs_jac_ba_spv.h -Vn VOLUME_CS_JAC_BA_SPV %SH%\volume.hlsl || exit /b 1
%DXC% %SPV% -T cs_6_0 -E cs_project -Fh %B%\gen\volume_cs_project_spv.h -Vn VOLUME_CS_PROJECT_SPV %SH%\volume.hlsl || exit /b 1
%DXC% %SPV% -T cs_6_0 -E cs_march -Fh %B%\gen\volume_cs_march_spv.h -Vn VOLUME_CS_MARCH_SPV %SH%\volume.hlsl || exit /b 1
%DXC% %SPV% -T cs_6_0 -E cs_light -Fh %B%\gen\volume_cs_light_grid_spv.h -Vn VOLUME_CS_LIGHT_GRID_SPV %SH%\volume.hlsl || exit /b 1
%DXC% %SPV% -T cs_6_5 -E cs_light -D RT -Fh %B%\gen\volume_cs_light_rt_spv.h -Vn VOLUME_CS_LIGHT_RT_SPV %SH%\volume.hlsl || exit /b 1
%DXC% %SPV% -T vs_6_0 -E vs -Fh %B%\gen\volume_vs_spv.h -Vn VOLUME_VS_SPV %SH%\volume.hlsl || exit /b 1
%DXC% %SPV% -T ps_6_0 -E ps -Fh %B%\gen\volume_ps_spv.h -Vn VOLUME_PS_SPV %SH%\volume.hlsl || exit /b 1
rem the effects (docs\superpowers\plans\2026-11-12-fx-audio-and-visual.md), each a pass of its own: FXSRC their C
%DXC% -T cs_6_0 -E cs_spawn -D CS -Fh %B%\gen\particles_cs_spawn.h -Vn PARTICLES_CS_SPAWN %SH%\particles.hlsl || exit /b 1
%DXC% -T cs_6_0 -E cs_update -D CS -Fh %B%\gen\particles_cs_update.h -Vn PARTICLES_CS_UPDATE %SH%\particles.hlsl || exit /b 1
%DXC% -T vs_6_0 -E vs -Fh %B%\gen\particles_vs.h -Vn PARTICLES_VS %SH%\particles.hlsl || exit /b 1
%DXC% -T ps_6_0 -E ps -Fh %B%\gen\particles_ps.h -Vn PARTICLES_PS %SH%\particles.hlsl || exit /b 1
%DXC% -T vs_6_0 -E vs -Fh %B%\gen\decal_vs.h -Vn DECAL_VS %SH%\decal.hlsl || exit /b 1
%DXC% -T ps_6_0 -E ps -Fh %B%\gen\decal_ps.h -Vn DECAL_PS %SH%\decal.hlsl || exit /b 1
set FXSRC=src\render_particles.c src\render_decals.c
set CF=%CLANG_FLAGS% -std=c17 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -O2 -I src -I dlss\include -I %B%\gen
set COMMON=src\base.c src\png.c src\gr.c src\audio.c src\conv.c src\extract.c src\level.c src\normals.c src\lights.c src\world.c src\models.c src\objdata.c src\orig.c src\xmidi.c src\sound.c src\sfx.c src\songs.c src\collide.c
clang %CF% -shared -o %B%\uwview.dll src\uwview.c src\render.c %FXSRC% src\ngx.c src\ui.c src\menu_draw.c src\smoothtext.c src\uwinv_draw.c src\uwconv_draw.c src\uwrec.c %COMMON% -ld3d12 -ldxgi -ldxguid -luser32 -lgdi32 -ladvapi32 -lwinmm -lmfplat -lmfreadwrite -lmfuuid -lole32 -lshell32 || exit /b 1
clang %CF% -fno-optimize-sibling-calls -shared -o %B%\uw_nvngx_port.dll src\nrbridge.c || exit /b 1
echo built uwview.dll
rem the same with the Vulkan renderer (render_vk.c for render.c; no D3D12, NGX or the D3D12 passes; the loader is opened
rem at run time, so no Vulkan library is linked): settings.txt renderer vulkan, or UWPORT_RENDERER=vulkan
rem Two checks first, each stopping the build: render_vk_cpu.h (render.c's CPU side, copied) is still what render.c
rem gives; and the shaders' SPIR-V reads every struct where the C side writes it (Vertex, GpuLight, Constants, RParticle)
set VKPY=O:\appsnotonc\sentinel\uv\python\cpython-3.13.17-windows-x86_64-none\python.exe
%VKPY% -I tools\render_vk_cpu.py --check || exit /b 1
%VKPY% -I tools\vklayout.py || exit /b 1
clang %CF% -I O:\appsnotonc\vulkan-sdk\1.4.363.1\Include -shared -o %B%\uwview_vk.dll src\uwview.c src\render_vk.c src\vk_load.c src\ngx_vk.c src\ui.c src\menu_draw.c src\smoothtext.c src\uwinv_draw.c src\uwconv_draw.c src\uwrec.c %COMMON% -ldxgi -ldxguid -luser32 -lgdi32 -ladvapi32 -lwinmm -lmfplat -lmfreadwrite -lmfuuid -lole32 -lshell32 || exit /b 1
echo built uwview_vk.dll
rem uwr-vulkan.exe: uwr.exe started with the Vulkan renderer for one run (makedist.py puts it beside uwr.exe)
clang %CLANG_FLAGS% -O2 -municode -mwindows tools\uw_vulkan_launch.c -o %B%\uwr-vulkan.exe || exit /b 1
rem the tour: the renderer with no game, every level and pictures of its items (tests\uwtour.c)
clang %CF% -o %B%\uwtour.exe tests\uwtour.c src\uwview.c src\render.c %FXSRC% src\ngx.c src\ui.c src\menu_draw.c src\smoothtext.c src\uwinv_draw.c src\uwrec.c %COMMON% -ld3d12 -ldxgi -ldxguid -luser32 -lgdi32 -ladvapi32 -lwinmm -lmfplat -lmfreadwrite -lmfuuid -lole32 -lshell32 || exit /b 1
echo built uwtour.exe
