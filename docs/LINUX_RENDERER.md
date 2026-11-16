# Our renderer on Linux: the route (study, 2026-11-15)

## In short

**Recommended route:** keep the one renderer we have (src\render.c and its files, Direct3D 12) and run it on Linux
through **vkd3d-proton**. vkd3d-proton is Valve's Direct3D-12-on-Vulkan, the same layer Proton uses, built as a native
Linux library. On Linux:

- The scene, the ray-traced light and our shaders stay as they are.
- What changes is the window plumbing (how a frame reaches the screen) and the NVIDIA DLSS calls. DLSS runs through
  NVIDIA's own Linux DLSS library and Vulkan, reached through vkd3d-proton's interop interfaces.
- One renderer source then serves both systems, so a change made for Windows (the HDR output last hour) reaches Linux
  without being written twice.

**What the compile test shows:**

- The renderer's five files compile for Linux against vkd3d-proton's headers with **29 errors in all**. Every one is a
  Windows constant or a piece of DXGI, the Windows window and display layer; none is in the scene or the ray tracing.
- render_decals.c and render_particles.c compile with **no errors**.
- The real work is in four places:
  - presenting to the window (about 20 DXGI calls in one part of render.c);
  - an `ngx_vk.c` beside `ngx.c` (8 NGX functions, in Vulkan form);
  - the smooth fonts (GDI to be replaced);
  - the language model's Linux build.

**What it needs from you:**

1. A yes to building vkd3d-proton and llama.cpp for Linux. Both can be tried from this Windows machine first; the
   WSL distro would only be needed if that fails, and I would ask before installing anything in it.
2. The Linux Mint machine with its NVIDIA card, for the first real run. DLSS and ray tracing can only be proved on
   real NVIDIA hardware and a recent NVIDIA driver (WSL's GPU does not show NGX).
3. A choice of fonts for Linux (below), since Calibri and Segoe UI are Microsoft's and cannot ship.

**Risks:**

- **DLSS through the interop (the largest):** sound on paper, with every handle NGX needs available, but it has never
  been run here.
- **Driver features:** vkd3d-proton's ray tracing and DLSS paths depend on the NVIDIA driver's Vulkan features.
- **Ours to maintain:** a native vkd3d-proton build is less travelled than its Wine one.

**Order of work:**

1. Build vkd3d-proton natively.
2. Present a plain frame through it from our SDL window.
3. The renderer without DLSS: scene, ray-traced light, HUD, menu.
4. DLSS Super Resolution through `ngx_vk.c`.
5. Ray Reconstruction.
6. HDR output.
7. Smooth fonts.
8. The language model.

Each step is a working game on its own. DLSS 5 stays out, as you said.

**The four routes, ranked:**

| Rank | Route | Cost | Risk | What you get |
|---|---|---|---|---|
| 1 | vkd3d-proton native (recommended) | Moderate: the window plumbing, `ngx_vk.c`, the fonts, about 1,000 to 1,500 new lines; the D3D12 renderer untouched | DLSS through the interop is unrun; the native vkd3d-proton build is ours to keep building | A native Linux game with ray tracing, DLSS and Ray Reconstruction, from the one renderer Windows uses |
| 2 | One Vulkan renderer for both systems (replacing D3D12) | High: render.c, decals, particles and ngx.c rewritten in Vulkan (about 2,500 dense lines), the shaders to SPIR-V, the Windows game re-proved picture by picture | Windows regressions for months; the DLSS 5 bridge to be redone | One native API on both systems, no translation layer; DLSS and Ray Reconstruction through NGX's Vulkan API on both (its entry points exist on both) |
| 3 | The Windows uw-new.exe under Proton on Mint | Minimal: no porting | Wine's, outside our control; not a Linux program | Ray tracing, DLSS and maybe DLSS 5 at once, but through Wine, with Wine's sound and paths |
| 4 | A separate Vulkan renderer for Linux only | High, then forever: every renderer change made twice | The two drift apart | Nothing route 2 does not give, at a higher lasting cost |

**The decision rule:**

1. Spend an hour of the Mint machine on route 3 first. It is a cheap probe of that card and driver: ray tracing and
   DLSS under vkd3d-proton.
2. Then build route 1.
3. If NGX's Vulkan API will not run on vkd3d-proton's device (the interop's handles and in-order command buffers),
   or vkd3d-proton's native present or ray queries fail on the NVIDIA driver, **go to route 2, never route 4**.

**The one finding that would change the ranking:** whether `NVSDK_NGX_VULKAN_Init` and `EvaluateFeature` work on
vkd3d-proton's VkDevice, recorded through BeginVkCommandBufferInterop. If they do, route 1 stands. If they do not,
route 2 is next.

---

## The answers, with their evidence

vkd3d-proton v3.1 (2026-10-28) is cloned with its submodules at O:\appsnotonc\vkd3d-proton: Vulkan-Headers,
SPIRV-Headers, dxil-spirv. File:line references are to that tree, unless they name ours.

### 1. A native build: what it makes, what it needs, where to build it

**What it makes:**

- The native build is plain Meson without a cross file ("If you want to build natively (ie. for
  `libvkd3d-proton.so`), pass `--native`": README.md:84; README.md:103-107).
- It produces `libvkd3d-proton-d3d12.so` (libs/d3d12/meson.build:10 `d3d12_name_prefix = 'libvkd3d-proton-'`,
  shared_library at :22) and `libvkd3d-proton-d3d12core.so` (include/vkd3d_sonames.h).
- It installs a pkg-config file `libvkd3d-proton-d3d12` and the headers under `vkd3d-proton/` (meson.build:194-196):
  vkd3d_windows.h, vkd3d_types.h, and the IDL-generated vkd3d_d3d12.h, vkd3d_dxgi*.h and vkd3d_swapchain_factory.h
  (include/meson.build).
- It loads the Vulkan loader itself at run time (SONAME_LIBVULKAN, include/vkd3d_sonames.h), so nothing links against
  libvulkan.

**What it needs:**

- widl, to turn its .idl into headers (meson.build:73-81; README.md:69-70).
- glslang, for its own internal shaders (meson.build:83).
- Meson and Ninja.
- A C and C++ compiler: dxil-spirv is C++.

**Where to build it:** every tool is already on this machine.

- widl.exe 11.5 and ninja.exe are in O:\appsnotonc\gcc\mingw64\bin; glslang.exe is in the Vulkan SDK; Meson comes
  through uv.
- Our clang and the Debian sysroot build Linux C and C++. Munt was built this way in step 7; libstdc++'s headers have
  to be named by hand.
- So a cross-build from Windows is plausible: a Meson cross file for x86_64-linux-gnu with our clang. **Not attempted
  in this study.**
- If it fails, the WSL distro could build it natively. That would need `meson ninja-build glslang-tools wine-stable`
  (or `mingw-w64-tools`, for widl) plus `g++`. Nothing is installed without your yes.

### 2. Direct3D 12 from plain C on Linux

- Yes. widl's headers carry the same C macros (COBJMACROS, WIDL_C_INLINE_WRAPPERS) that our ngx.h sets.
- vkd3d_windows.h stands in for windows.h's types (HRESULT, GUID, IUnknown, UINT, and the like).
- **Spike:** render_decals.c and render_particles.c compile against these headers for x86_64-linux-gnu with 0 errors.
  render.c compiles with 15 errors, none in the D3D12 code (see the table).

### 3. Presenting without DXGI

- The native library exports only the D3D12 entry points: D3D12CreateDevice, the root-signature (de)serialisers,
  D3D12GetDebugInterface, EnableExperimentalFeatures and GetInterface (libs/d3d12/main.c:143-316;
  libs/d3d12/d3d12.def). **There is no DXGI factory natively**: no CreateDXGIFactory1 and no CreateSwapChainForHwnd.
- A native application presents through vkd3d-proton's own swap chain:
  1. The command queue gives an `IDXGIVkSwapChainFactory` by QueryInterface (demos/demo_xcb.h:418-440; implemented in
     libs/vkd3d/swapchain.c).
  2. Its `CreateSwapChain` takes an `IDXGIVkSurfaceFactory` that we implement: one method, `CreateSurface(instance,
     physical_device, &surface)` (include/vkd3d_swapchain_factory.idl:45). For us its body is `SDL_Vulkan_CreateSurface`
     on the SDL window.
  3. `IDXGIVkSwapChain` has GetImage, GetImageIndex, ChangeProperties (resize), Present, CheckColorSpaceSupport,
     SetColorSpace and SetHDRMetaData (vkd3d_swapchain_factory.idl:57-106).
- So the HDR output that just landed maps onto SetColorSpace and SetHDRMetaData. Only the monitor query
  (IDXGIOutput6::GetDesc1, render.c:446-452) has no native counterpart; the HDR capability would come from SDL3's
  display properties instead.
- **What changes in render.c:**
  - the device's creation (no DXGI factory or adapter enumeration: D3D12CreateDevice on the default adapter, and the
    adapter's name from the interop or the Vulkan device);
  - the swap chain's creation, resize, back buffers and present (the about 20 DXGI calls counted below, all in one part
    of the file);
  - the HDR monitor query.
- That part can sit behind one `#ifdef _WIN32` with a native counterpart beside it; the rest of render.c is shared.
  The HWND parameter of the renderer's start becomes the SDL window, which plat_sdl3.c already hands out.

### 4. Ray tracing

- render.c uses **inline ray queries only** (DXR 1.1):
  - ID3D12Device5 (6 uses), ID3D12GraphicsCommandList4 (6);
  - BuildRaytracingAccelerationStructure (4) and GetRaytracingAccelerationStructurePrebuildInfo (4);
  - it requires `RaytracingTier >= D3D12_RAYTRACING_TIER_1_1` (render.c:557);
  - **no** CreateStateObject and **no** DispatchRays.
- The shaders' rays are `RayQuery<...>` in pixel and compute shaders (src\shaders\rthit.hlsli:12 and :47,
  scene.hlsl:55), built as ps_6_5 and cs_6_5 (tools\buildview.cmd:12-14).
- vkd3d-proton reports TIER_1_1 when the driver has VK_KHR_ray_query (libs/vkd3d/device.c:74 enables the extension;
  :9098-9110 raise the tier). On a current NVIDIA Linux driver that is expected to hold: **to be proved on the Mint
  machine.**

### 5. Shaders

- Ours stay DXIL from DXC at build time. vkd3d-proton turns DXBC and DXIL into SPIR-V itself through dxil-spirv, a
  built-in submodule (README.md:249: the cache "of DXBC/DXIL -> SPIR-V conversion"), so the same compiled shader headers
  serve Linux.
- They use SM 6.0 and 6.5 with RayQuery: no bindless heap indexing, no wave intrinsics, no view instancing, and no
  16-bit types (grepped).
- No unsupported feature was found. That cannot be certain until the pipelines are made on the NVIDIA machine.

### 6. NVIDIA DLSS on Linux

**The libraries are here:** O:\appsnotonc\dlss-sdk has `lib\Linux_x86_64\libnvsdk_ngx.a`, plus
`rel\libnvidia-ngx-dlss.so.310.9.16` (Super Resolution) and `libnvidia-ngx-dlssd.so.310.9.16` (Ray Reconstruction).
On Linux NGX has a Vulkan API only. Our ngx.c calls its D3D12 API: NVSDK_NGX_D3D12_Init_Ext, GetCapabilityParameters,
AllocateParameters, CreateFeature, EvaluateFeature, ReleaseFeature, DestroyParameters and Shutdown1, eight functions,
plus the parameter setters, which are the same on both APIs.

**a. Through vkd3d-proton's interop: everything NGX's Vulkan API needs is reachable.**
`ID3D12DXVKInteropDevice1..3` (include/vkd3d_device_vkd3d_ext.idl:50-110) gives:

- the VkInstance, VkPhysicalDevice and VkDevice (GetVulkanHandles), for NVSDK_NGX_VULKAN_Init;
- the queue (GetVulkanQueueInfo);
- each resource's VkImage and VkFormat (GetVulkanResourceInfo1), and its layout per D3D12 state (GetVulkanImageLayout).
  These cover colour, depth, motion vectors, the Ray Reconstruction guides and the output. We create the VkImageViews
  NGX takes with vkCreateImageView on that device.
- **a VkCommandBuffer recorded in order inside our D3D12 command list** (BeginVkCommandBufferInterop /
  EndVkCommandBufferInterop): EvaluateFeature is recorded there, between our D3D12 commands.

The device is QueryInterface'd for it in device.c:3751 with no `_WIN32` guard, so it is in the native build. NGX's
Vulkan device extensions are enabled by vkd3d-proton itself when the driver has them (VK_NVX_binary_import and
VK_NVX_image_view_handle: device.c:158-159; VK_KHR_push_descriptor: :69).

**How Proton does DLSS, for comparison (dxvk-nvapi not cloned; vkd3d-proton's side read):** a Windows game under Proton
loads NVIDIA's *Windows* nvngx.dll, whose D3D12 path runs its CUDA binaries through nvapi (dxvk-nvapi). That reaches
vkd3d-proton's `ID3D12DeviceExt` CUBIN functions and `ID3D12GraphicsCommandListExt::LaunchCubinShader`
(vkd3d_command_list_vkd3d_ext.idl:27-41), over VK_NVX_binary_import. Natively there is no Windows nvngx.dll to load, so
for us the route is NGX's Linux Vulkan API over the interop, as above.

**To write:** `ngx_vk.c` beside `ngx.c` with the same functions to the rest of the renderer, about 300 to 450 lines.
It does the interop queries, image views per resource, the layouts, and the eight calls in their Vulkan form. Plus a
VkSurface hook (above), and the DLSS libraries shipped beside the program.

**What cannot be known here:**

- whether NGX's Vulkan init accepts a device it did not create (it should: NVSDK_NGX_VULKAN_Init takes the handles);
- the driver's NGX feature support on the Mint machine;
- that Ray Reconstruction's guides map one for one in their Vulkan form.

All three need the NVIDIA machine.

**b. Anything simpler:** none found. NGX has no D3D12 API on Linux, and Proton's route needs the Windows DLL.

### 7. The rest of uwview.dll on Linux

The spike compiled these against port\win32\uwos.h, with windows.h mapped onto it.

| File | What it uses from Windows | What it needs |
|---|---|---|
| level.c | nothing | compiles as is (0 errors) |
| uwview.c | the DLL exports (`__declspec(dllexport)`, 46 of its 53 errors), the window handle, CP_UTF8, GetModuleHandleEx | an export macro, the handle the backend gives, small portable helpers |
| extract.c | FindFirstFile / WIN32_FIND_DATA (listing folders) | opendir / readdir behind one helper |
| smoothtext.c, uwconv_draw.c | GDI: CreateFont, TextOut, bitmaps (57 and 37 errors) | glyphs from a font file, e.g. stb_truetype (one public-domain header) |
| uwrec.c | Media Foundation (the Shift+P video) | Windows only; on Linux leave out, or use the screenshot alone |
| audio.c, sound.c | the engine's own players (not the port's) | not needed: the port has its own sound |

**Fonts on Linux:** Windows' faces cannot ship. Freely licensed substitutes, each to ship in data\fonts:

| Windows face | Free substitute | Notes |
|---|---|---|
| Calibri (your UI font) | Carlito | OFL, metric-compatible with Calibri |
| Segoe UI | Selawik | Microsoft's own open, OFL, metric-compatible fallback |
| Tahoma, Verdana | DejaVu Sans | free |
| Georgia | Gelasio | OFL |
| Cambria | Caladea | OFL |
| Consolas | Inconsolata | OFL |

st_load_font_dir already loads .ttf files from a folder, so on Linux that folder becomes the font list.

### 8. The language model (uwmind) on Linux

- llama.cpp b12131 is in external\llama.cpp (224 MB of source). Our Windows build (tools\buildllama.cmd) uses CMake +
  Ninja, with CPU variants and a Vulkan backend; llm\bin is about 76 MB.
- **For Linux:** the same CMake with our clang, the sysroot and a toolchain file. A cmake.exe is in
  O:\appsnotonc\gcc\mingw64\bin.
- **Backends:**
  - Vulkan works on NVIDIA Linux; its shader generator needs glslc, which the Vulkan SDK on this machine has;
  - CPU works everywhere;
  - CUDA would need NVIDIA's Linux CUDA toolkit, which is not here and is not needed.
- uwmind.c's own windows.h use is small: the DLL exports and loading. It would join uwos.h.
- The models (2.5 GB under llm\models) are the same files on both systems.
- Size on Linux: about the same as on Windows, 70 to 90 MB of libraries.
- **Study only; nothing built.**

---

## The compile spike's table

Reproduce with `bash tools/linux_renderer_spike.sh`; the output goes to work\linux\spike. Syntax only, for
x86_64-linux-gnu: the renderer's files against vkd3d-proton v3.1's headers (made with widl 11.5), the others against
uwos.h.

| File | Errors | What they are |
|---|---|---|
| render.c | 15 | INFINITE, MAX_PATH ×2, CP_UTF8 (Windows constants); IDXGIOutput6 / DXGI_OUTPUT_DESC1 (the HDR monitor query, 7); DXGI_ADAPTER_DESC1 + Flags + DXGI_ADAPTER_FLAG_SOFTWARE (adapter choice, 3); DXGI_MWA_NO_ALT_ENTER |
| render_decals.c | 0 | |
| render_particles.c | 0 | |
| ngx.c | 6 | MAX_PATH ×3; a registry read (HKEY_LOCAL_MACHINE, RRF_RT_REG_SZ, ERROR_SUCCESS) |
| nrbridge.c | 8 | the DLSS 5 bridge (HMODULE, `__declspec`): stays out |
| uwview.c | 53 | `__declspec` ×46; HWND, CP_UTF8, GetModuleHandleEx flags |
| level.c | 0 | |
| extract.c | 10 | the folder listing (WIN32_FIND_DATAA) |
| smoothtext.c | 57 | GDI |
| uwconv_draw.c | 37 | GDI (33) and `__declspec` (4) |
| uwrec.c | stops | mfapi.h: Media Foundation |
| uwmind.c | stops | llama.h: needs llama.cpp's Linux build |

A clean syntax check is not a link. Beyond the table:

- the DXGI calls above have no native implementation (question 3);
- ngx.c's D3D12 NGX functions have no Linux library (question 6).

Those two are the work. The DXGI calls render.c makes are:

- CreateDXGIFactory1, IDXGIFactory4_EnumAdapters, IDXGIAdapter1_GetDesc;
- IDXGIFactory4_CreateSwapChainForHwnd, MakeWindowAssociation;
- IDXGISwapChain3: GetBuffer ×2, ResizeBuffers ×2, Present, GetCurrentBackBufferIndex, SetColorSpace,
  GetContainingOutput;
- IDXGIOutput6_GetDesc;
- CreateEventW and WaitForSingleObject (the frame fence; uwos.h already has both).

---

## The alternatives

**Route 2: one Vulkan renderer for both systems, replacing the D3D12 one.**

- **DLSS:** NGX has Vulkan entry points on both systems, for both features.
  - The SDK's headers: nvsdk_ngx_vk.h, with 36 NVSDK_NGX_VULKAN_ functions including RequiredExtensions (:114),
    Init (:172-184), CreateFeature (:536) and EvaluateFeature (:753); nvsdk_ngx_helpers_vk.h for Super Resolution;
    nvsdk_ngx_helpers_dlssd_vk.h for Ray Reconstruction.
  - The libraries: Windows' lib\Windows_x86_64\x64\nvsdk_ngx_d.lib and _s.lib carry the VULKAN functions beside the
    D3D12 ones; Linux's libnvsdk_ngx.a carries them too.
  - So DLSS and Ray Reconstruction would need no interop glue: the same `ngx_vk.c` on both systems.
- **DLSS 5 on Windows:**
  - Today it is the D3D12 path: nrbridge.c's uw_nvngx.dll calls nvngx_dlssnr.dll's NVSDK_NGX_D3D12_* functions, because
    that DLL checks its caller's name.
  - nvngx_dlssnr.dll also exports NVSDK_NGX_VULKAN_Init, Init_Ext, Init_Ext2 and EvaluateFeature (llvm-objdump of
    uo-new\dlss\nvngx_dlssnr.dll), so the bridge is reworkable for Vulkan.
  - Unproven: DLSS 5 is NVIDIA's newest feature and may expect D3D12. A risk, not a certain loss.
- **HDR:** a Vulkan swap chain on Windows offers the same colour spaces (VK_EXT_swapchain_colorspace: scRGB as
  EXTENDED_SRGB_LINEAR, HDR10 as HDR10_ST2084) and VK_EXT_hdr_metadata. The monitor's peak brightness comes from
  DXGI or Windows' display API either way.
- **The rewrite:**
  - render.c (1,705 dense lines, 493 D3D12 uses), render_decals.c (47), render_particles.c (75) and ngx.c (25):
    pipelines, descriptor sets in place of the root signatures, barriers, acceleration structures, the swap chain.
  - The shaders stay HLSL, built by DXC with `-spirv`. RayQuery maps to SPV_KHR_ray_query. The root signatures'
    registers need `[[vk::binding]]` placements, and the scene's texture arrays and samplers need care.
- **Re-proving the Windows game:**
  - every picture check again: fxcheck's compare cases (particles, decals, water, the acoustics' grid dump), menucheck's
    drawn rows, inputcheck's display cases, run-tests' shots;
  - DLSS, Ray Reconstruction, ray tracing and HDR by eye on the 4090.
  - Expect months before it matches today's renderer, and every change fable dlss5 makes meanwhile lands on a moving
    target.

**Route 4: a hand-written Vulkan renderer for Linux only.**

- It means writing render.c, the particles, the decals and the ray-traced light again in Vulkan: pipelines,
  descriptors, acceleration structures, and the shaders rebuilt as SPIR-V.
- That is weeks of work before it matches what we have.
- After that, every change made to the D3D12 renderer has to be made twice, forever. The renderer changes almost daily,
  so this is the dearest route over time.
- Its one gain: no dependency on vkd3d-proton.

**Route 3: the Windows uw-new.exe under Proton or Wine on Mint.**

- Proton already carries vkd3d-proton and dxvk-nvapi, so DXR and DLSS (even DLSS 5, if its bridge works there) could
  run with no porting at all.
- **What you would lose:**
  - it is not a Linux program;
  - Wine has to be installed and kept working;
  - the sound goes through Wine's Windows audio emulation (waveOut, OpenAL Soft's Windows DLL) rather than the native
    paths we just built;
  - file paths are Wine's;
  - the language model's Vulkan backend runs through Wine too;
  - problems become Wine problems, which are harder to diagnose.
- **What would have to be tested:** the renderer under vkd3d-proton (the same layer as the native route), DLSS through
  dxvk-nvapi, sound, the mouse grab, full screen, and the save paths.
- **As a stop-gap it is cheap:** an hour on the Mint machine would show whether the whole game, renderer included,
  already works this way. It is worth trying first in parallel, because it answers the hardware questions (ray tracing,
  DLSS on that card and driver) before the native work spends weeks.
