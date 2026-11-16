# One Vulkan Renderer for Windows and Linux: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task by task.
> Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** a Vulkan renderer with everything our D3D12 renderer does, from one source for Windows and Linux:

- the scene, its textures, normal maps, AO and relief shadows, the cards, water, fog, decals, particles;
- the ray-traced light (ray queries), bounced light and probes;
- DLSS upscaling and Ray Reconstruction through NGX's official Vulkan API;
- HDR output, screenshots, and the interface layer (menu, HUD, text).

DLSS 5 is dropped from the Vulkan renderer (the user, 2026-11-15: "switching over to vulkan and dropping dlss5 is the
path").

**Decision:** docs\LINUX_RENDERER.md, route 2, chosen by the user over vkd3d-proton. That is not re-argued here.

**Architecture:**

- New files behind the existing interface: src\render.h's 74 render_* functions, plus the rfx_* passes.
  - src\render_vk.c: device, swap chain, targets, the scene, ray tracing, present.
  - src\render_vk_particles.c, src\render_vk_decals.c: the same interface as render_particles.c / render_decals.c.
  - src\ngx_vk.c: NGX's Vulkan API, the same functions to render_vk.c as ngx.c is to render.c.
  - src\vk_load.c / vk_load.h: the Vulkan loader opened at run time (vulkan-1.dll or libvulkan.so.1) and the
    function table: VK_NO_PROTOTYPES, as vkd3d-proton does; no link-time dependency.
- **uwview_vk.dll, a second build of the same uwview.c with render_vk.c in render.c's place**, rather than one DLL with
  a backend switch. Why:
  - a switch inside one DLL needs a dispatch layer over all 74 functions, a second copy of the interface to keep in
    step;
  - two builds keep the D3D12 DLL byte-for-byte what it is today, so the default game cannot change;
  - the front end already loads the renderer by file name (uwfront.c view_start), so choosing one is one line;
  - on Linux the same Vulkan files make libuwview.so.
- **The switch:**
  - data\settings.txt `renderer d3d12|vulkan` (default d3d12);
  - an Esc-menu row "Renderer" under Graphics, taking effect at the next start, with the program started again as the
    New game row does, and the row's hint saying so;
  - the environment override UWPORT_RENDERER=vulkan|d3d12 for the checks.
  - uwfront.c's view_start loads uwview_vk.dll for vulkan. If it is missing or fails to start, it falls back to
    uwview.dll, with one log line.

**Tech stack:**

- C17 with our clang (MinGW target on Windows, the Debian sysroot for Linux).
- Vulkan 1.3 headers from the Vulkan SDK 1.4.363.1 (O:\appsnotonc\vulkan-sdk). Its validation layer is used for the
  checks.
- DXC v1.9.2609 (O:\appsnotonc\dxc), already ours, for DXIL and SPIR-V.
- NGX from O:\appsnotonc\dlss-sdk (310.9.16): nvsdk_ngx_s.lib on Windows, libnvsdk_ngx.a on Linux, nvsdk_ngx_vk.h.
- **No VMA, no volk.** The renderer makes a few dozen resources, each a dedicated allocation from a memory type chosen
  by flags (about 60 lines); the loader table is ours (vk_load.c). Nothing new is added, so no new licence.

## Global constraints

1. **Beside, not instead.**
   - render.c, ngx.c, nrbridge.c, render_particles.c, render_decals.c, uwview.dll, uw_nvngx.dll and DLSS 5's menu rows
     are not edited, disabled or deleted by this project.
   - The default stays `renderer d3d12` until the user says otherwise (M8).
   - Both Windows builds (Win32, SDL3) keep building and launching at every commit.
2. **One HLSL source.**
   - src\shaders\*.hlsl and *.hlsli are compiled twice by tools\buildview.cmd: DXIL as today, and SPIR-V
     (`-spirv -fspv-target-env=vulkan1.3`) into build\gen\*_spv.h.
   - **Bindings by DXC's shift options, not `[[vk::binding]]` attributes:** `-fvk-b-shift 0 0 -fvk-t-shift 16 0
     -fvk-u-shift 48 0 -fvk-s-shift 64 0`. Every register becomes binding (class base + register) in set 0, so b0 is 0,
     t0..t12 are 16..28 and u0 is 48.
   - Why shifts: attributes would put Vulkan text into every declaration of the shared HLSL, which fable dlss5 edits
     daily, and the shifts need no source change at all.
   - Proved before this plan: all nine scene, probes, present and decal variants compile to SPIR-V unchanged
     (work\vk, 2026-11-15). The particles' cs_spawn / cs_update are to be checked in M3.
   - No forked copy of any shader. A Vulkan-only difference, if one is ever needed, is an `#ifdef __spirv__` in the
     shared file, listed in the ledger.
3. **Parity by pictures.**
   - The D3D12 renderer is the reference.
   - tools\vkcompare.py (M1) renders the same held view with both backends in hidden runs: UWVIEW_COMPARE's held camera
     and time, the same window size, `--window-shot`.
   - It counts the pixels differing by more than a threshold, and measures a noise floor first: D3D12 against D3D12,
     two runs.
   - A milestone passes when every compared case is at or under the floor, or each difference is explained in the
     ledger: the jitter sequence, float order, a driver's rounding.
   - Hidden runs (UWPORT_OFFSCREEN). The user allows visible runs; they are used for judging by eye at M4, M5 and M6,
     asked for in the report.
4. **Validation.** Every proof runs once with the SDK's validation layer, with zero errors:
   - `UWVK_VALIDATE=1` makes render_vk.c enable VK_LAYER_KHRONOS_validation and a debug messenger that logs every
     message as `vk: VALIDATION ...` and counts them;
   - VK_LAYER_PATH points at the SDK's Bin;
   - the proof greps for the count line `vk: validation: N messages (E errors)`.
5. **The ported-features ledger.**
   - render.c keeps changing (HDR, firelight colour and lintels on 2026-11-15 alone).
   - The ledger at the end of this plan lists every D3D12-side feature with the render.c / shader commit it came in,
     and its Vulkan state: ported (commit), planned (milestone), or N/A (DLSS 5).
   - It starts at render.c's HEAD deaf295.
   - At each milestone, `git log <last-ledger-hash>..HEAD -- src/render.c src/render_*.c src/ngx.c src/shaders` is read
     and every new D3D12-side change gets a ledger line before the milestone is reported.
   - fable dlss5 is told the ledger exists. A change to shared HLSL needs no port, since one source serves both; the
     ledger records it as such.
6. **The usual rules.**
   - Builds through work\locked.sh; scratch in work\vk; installs and SDKs only under O:\appsnotonc (nothing new
     planned).
   - Nothing deleted; no agents; commits by explicit path with only our hunks.
   - Every switch an Esc row (the user's standing rule): the Renderer row in M1, and the Vulkan renderer's own
     options are the same rows the D3D12 one has (uwv_option).

## Inventory of the D3D12 renderer: the parity checklist

From src\render.c (deaf295), render_particles.c, render_decals.c, ngx.c and src\shaders. Each line is checked off in
the milestone named.

**Device and frame (M1):**

- [ ] The adapter: the first hardware DXGI adapter at feature level 12_0. The GPU's name goes into render_status.
- [ ] One direct queue, one allocator and command list, one fence. Each frame is recorded, submitted, presented and
  waited for before the next: FRAMES 2 back buffers, no overlap (wait_gpu).
- [ ] The swap chain: FLIP_DISCARD, 2 buffers, R8G8B8A8_UNORM (SDR) or R16G16B16A16_FLOAT scRGB (HDR, M6), vsync as
  cfg.vsync, Alt+Enter not DXGI's.
- [ ] Headless: no swap chain; final_tex R8G8B8A8 is the target, copied to a readback buffer.
- [ ] render_status / render_dlss_text strings: the same formats (the logs and checks read them).

**Targets, at the output size** (with DLSS, the scene is drawn into the top-left render-size part):

- [ ] color R16G16B16A16_FLOAT, cleared (0,0,0,1) (M2)
- [ ] motion R16G16_FLOAT, cleared 0 (M2; DLSS's motion vectors, M5)
- [ ] depth D32 (R32 typeless; read as R32_FLOAT by present for the orbs) (M2)
- [ ] albedo R8G8B8A8, spec R8G8B8A8, nrough R16G16B16A16_FLOAT: Ray Reconstruction's guides, written only with ray
  tracing High, cleared to sky (0.5 albedo) (M5)
- [ ] dlss_out R16G16B16A16_FLOAT UAV (M5); nr_out (DLSS 5): **N/A**
- [ ] final_tex R8G8B8A8 (headless and the HDR screenshot's SDR copy) (M1 / M6)
- [ ] ui_tex R8G8B8A8 at the interface's size (output / ui_scale, ui_scale (h+150)/300 by default), uploaded when
  render_set_ui marks it dirty (M1)

**Buffers:**

- [ ] cbuf: 4096 bytes, mapped, `Constants` (the scene's) at 0 and the screenshot's SDR copy at CB_SHOT_OFF 2048
  (M1: present's fields; M2: the rest)
- [ ] lights_buf: GpuLight × (MAX_LIGHTS+1), mapped (torch [0], then the placed lights); tiles_buf: u32 per tile ×
  TILE_LIGHTS (M2)
- [ ] vbuf / ibuf: the level's mesh (render_upload), Vertex 76 bytes (pos, pos2, uv, uv2/rect, normal, color), u32
  indices (M2)
- [ ] sprite_vb: RENDER_MAX_SPRITE_VERTS Vertex, mapped (the cards) (M2)
- [ ] probes[2]: PROBE_BYTES each, written and read alternately (M4)

**Textures (render_upload, render_set_hires, render_update_atlas):**

- [ ] atlas ATLAS_SIZE × ATLAS_H RGBA8, with its cycling colours' rows rewritten by render_update_atlas (the palette)
  (M2)
- [ ] natlas NATLAS_SIZE² RGBA8 (normal + height) (M3); snatlas RGBA8 and saoatlas R8 (the pictures' normal and AO
  maps) (M3)
- [ ] grid 64×64 R8G8B8A8_UINT (tile type, floor height) (M2)
- [ ] hatlas (upscaled walls and floors, 1×1 when off; render_set_hires) (M3)

**Pipelines and passes per frame, in order:**

1. [ ] The atlas's cycling rows and the interface layer uploaded when dirty (copies + barriers) (M1 / M2)
2. [ ] Particles' compute (spawn, update) (render_particles.c) when fx_particles (M3)
3. [ ] Ray tracing: the cards' BLAS and TLAS rebuilt each frame (cards_as_build); the level's BLAS/TLAS at
   render_upload (build_as) (M4)
4. [ ] The probes' compute (probes.hlsl, cs_6_5, PROBES/64 groups): last frame's probes in, this frame's out (M4)
5. [ ] The scene: scene.hlsl VS + PS, in three variants: grid (no RT, ps_6_0), RT (ps_6_5), RT High (ps_6_5,
   5 targets). Triangle lists, the level indexed, then the cards unindexed (M2 / M4 / M5)
6. [ ] Decals (decal.hlsl), then particles' draw, into color with the depth tested (M3)
7. [ ] DLSS Super Resolution, or Ray Reconstruction (High) with a fallback to SR, into dlss_out (M5)
8. [ ] Present (present.hlsl): the shown picture (color or dlss_out), the depth (the orbs' glow), the interface, the
   exposure undone, gamma, tint; into the back buffer (SDR) or FP16 scRGB (HDR) (M1 / M6)
9. [ ] Screenshot: the back buffer copied to readback (SDR); with HDR, the frame drawn again SDR into final_tex and
   copied (M6)

**The frame's constants and state** (render_frame): the jitter (Halton 2/3, the phases rule), the matrices (vp, vp_nojit,
vp_prev), eye and held frame, look, size, post (exposure, gamma, ui scale), hdr, the torch flames (render_flame), the
wall torches' flicker, the orbs (16 nearest in front), linfo, outp, tint, rtlim, torchc, sprite, texres, the water's
ripples and time, `render_hold_time` for comparisons.

- [ ] The CPU side is copied, not reinvented. The constants struct is shared through a header made from render.c's
  `Constants` / `GpuLight` (M2).

**Ray tracing (M4):**

- [ ] The level's BLAS (all triangles, opaque) + a TLAS of one instance, built at render_upload.
- [ ] The cards' BLAS (not opaque: alpha tested in the shader), rebuilt each frame from cpos, and a TLAS of two
  instances: mask 1 the level, 2 the cards.
- [ ] scene_tlas(): the cards' TLAS when on (render_set_object_shadows, UWVIEW_CARDSHADOW=0), else the level's.
- [ ] Inline RayQuery in scene.hlsl (RT, RTH) and probes.hlsl. DXR TIER_1_1 → VK_KHR_ray_query.
- [ ] render_set_raytracing on and off any frame (the grid variant); rt_high wanted / active.

**DLSS (M5)** (ngx.c: ngx_start, dlss_create / evaluate / release, rr_create / evaluate / release, ngx_stop):

- [ ] Inputs: color, depth, motion (MV scale), jitter (jx, jy), render size, reset, exposure (cfg.exposure; HDR scale),
  the quality (DLSS_Q_* → NGX's perf/quality values, as in DLSS_NGX_Q).
- [ ] Ray Reconstruction: plus albedo, spec, nrough, and the matrices w2v / v2c (row vectors).
- [ ] render_set_dlss / _quality / _sizes / _size_for; the counts ("Ray Reconstruction: N frames, M failed").

**HDR (M6):** hdr_display (the monitor's colour space and MaxLuminance), hdr_apply (the swap chain's format and colour
space), hdr_follow (every few frames), the levels (peak, paper white), present's HDR variant.

**Other (M6):** render_readback; render_want_screenshot; resize (today the renderer keeps its first size, and the window
stretches it: render.c has no ResizeBuffers path on resize, and V.ok "a restart cannot be made"); device lost (today
nothing beyond logging DeviceRemovedReason at readback); the comparison modes (UWVIEW_COMPARE, UWVIEW_COMPARE_FX,
render_hold_time, render_set_debug's normal view).

**Effects:**

- [ ] particles: RP_MAX ring, spawn and update compute, billboards drawn with depth test, kinds 1-3 (M3)
- [ ] decals: up to 512, a quad each, decal.hlsl, kinds blood, scorch, ice, acid (M3)
- [ ] water: in the scene shader (fx_water, ripples) (M3)
- [ ] fog: fx_fog (in the shader) (M3)

**Interface:** the layer from uwview.c (ui.c, the menu, HUD, text, the conversation's pieces) is RGBA uploaded each frame
and composed by present: one path for every 2D thing (M1).

## The Vulkan design

**Instance and device:**

- Vulkan 1.3. On Windows the instance extensions are VK_KHR_surface and VK_KHR_win32_surface; on Linux, VK_KHR_surface
  plus SDL's (SDL_Vulkan_GetInstanceExtensions).
- Plus NGX's: NVSDK_NGX_VULKAN_RequiredExtensions, or GetFeatureInstanceExtensionRequirements for DLSS and DLSS-RR,
  called before the instance is made.
- The physical device: the first discrete GPU, else the first; the name goes into render_status.
- Device extensions and features:

| Extension or feature | Why |
|---|---|
| VK_KHR_swapchain | presenting |
| VK_KHR_acceleration_structure, VK_KHR_ray_query, VK_KHR_deferred_host_operations (the first's requirement) | DXR 1.1's inline rays |
| bufferDeviceAddress (1.2 core feature) | acceleration structure inputs |
| VK_EXT_swapchain_colorspace (instance), VK_EXT_hdr_metadata | HDR output (M6) |
| dynamicRendering + synchronization2 (1.3 core) | no render pass objects, simpler barriers |
| NGX's device extensions, as NVSDK_NGX_VULKAN_RequiredExtensions reports (on today's SDK: VK_NVX_binary_import, VK_NVX_image_view_handle, VK_KHR_push_descriptor, among others) | DLSS (M5) |

- Missing ray tracing works as D3D12 does: "shadows are traced through the tile grid instead". Missing NGX: DLSS
  unavailable.

**The rest of the design:**

- **Memory:** one dedicated allocation per buffer or image: device-local for targets and textures, host-visible and
  coherent for the mapped buffers (cbuf, lights, tiles, sprites, ui upload, readback, cards' inputs). The uploads go
  through a staging buffer and one-time command buffers, as render.c's upload_texture does.
- **Descriptors:** one descriptor set layout per pipeline kind, mirroring the root signatures.
  - Graphics: binding 0 the constants (the uniform buffer); 16+n the t-registers (sampled images for textures,
    storage buffers for StructuredBuffers, acceleration structure for t3); 48 u0 for the probes' compute.
  - The D3D12 root CBV and SRVs (changing per frame: probes[wp], cbuf offset) become descriptor sets updated before
    recording, two per kind (a frame's and the next's).
  - Present: its own small set. There are no samplers at all (every shader uses Load).
- **Synchronisation:**
  - as render.c does: record, submit, present, wait on a fence before the next frame (a single frame in flight, as
    today);
  - image barriers with synchronization2 at the same places as D3D12's;
  - layouts: color, motion, guides are COLOR_ATTACHMENT while drawn and SHADER_READ_ONLY after; depth is
    DEPTH_ATTACHMENT then DEPTH_READ_ONLY; dlss_out is GENERAL (NGX's UAV); back buffers are PRESENT_SRC_KHR.
- **Swap chain:**
  - surface from the HWND the backends pass to uwv_init (vkCreateWin32SurfaceKHR); on Linux from the SDL window
    (SDL_Vulkan_CreateSurface).
  - The seam: uwp_window() already returns the SDL window off Windows (port\sdl3\plat_sdl3.c), and uwv_init's void
    *hwnd carries it.
  - FIFO when vsync, else MAILBOX or IMMEDIATE.
  - Formats: B8G8R8A8_UNORM or R8G8B8A8_UNORM (SDR); R16G16B16A16_SFLOAT with EXTENDED_SRGB_LINEAR (scRGB) for HDR.
  - On out-of-date or suboptimal, it is recreated at the same size.
- **Shaders:** build\gen\*_spv.h from DXC (constant arrays as today's *_vs.h / *_ps.h), entry points as today
  (vs, ps, cs; particles' cs_spawn, cs_update and their draw).
- **Linux (M7):** the same render_vk.c with vk_load.c opening libvulkan.so.1. uwview.c's Windows parts (53 errors in
  the study), extract.c, smoothtext.c on stb_truetype with the free fonts, uwconv_draw.c and libuwview.so are built by
  portlinux; NGX's libnvsdk_ngx.a plus libnvidia-ngx-dlss / dlssd .so ship beside the game.

## Milestones

Each leaves both Windows builds working, with the default renderer d3d12 unchanged. Sizes are working days at this
pace, not calendar promises.

| | Milestone | Proof | Size | Main risk |
|---|---|---|---|---|
| M1 | Device, swap chain, the present pass with the interface layer; the Renderer switch | the title screen and the open Esc menu: Vulkan vs D3D12 window shots under the noise floor; the device, driver and Vulkan version in the log; a clean exit; resize and full screen survive; validation 0 errors | 2-3 | the SDL3 build's HWND and MinGW-built Vulkan code (first contact) |
| M2 | The scene: raster, textured, lit, no RT, DLSS off; the constants; the cards; atlas cycling | held views (level 1 start, a torch-lit corridor, a card-heavy room) at raytracing 0, DLSS off | 3-4 | the vertex layout and the constants' packing rules (std140 vs HLSL cbuffer) |
| M3 | Normal maps, AO, relief shadows, sprites' maps, hatlas, water, fog, decals, particles | fxcheck's own compare cases with UWPORT_RENDERER=vulkan, plus held views each on and off | 3-5 | the particles' compute and ring under Vulkan's barriers |
| M4 | Ray tracing: AS builds, ray queries, probes, cards' shadows, RT on and off at run time | held views at rt_quality each, bounce on and off; the cards' shadows | 4-6 | AS build sizes and per-frame rebuilds; probe feedback stability |
| M5 | DLSS SR via ngx_vk.c; then Ray Reconstruction with its guides | DLSS on per mode vs D3D12 (temporal: the same held frame after N frames); RR on; the counts lines | 4-6 | NGX Vulkan init and its extensions; the jitter and MV sign conventions |
| M6 | HDR, screenshots (the SDR copy), resize, device loss, the compare modes | HDR on an HDR monitor by eye (the user); window shots SDR equal; a resize mid-play | 2-3 | Windows' HDR detection without DXGI (queried through DXGI anyway, for the monitor only) |
| M7 | Linux: libuwview.so, the SDL surface, fonts, NGX's Linux libraries; the Mint test | portlinux builds; WSL hidden run (llvmpipe or no GPU: render_init's failure path); the Mint machine: RT and DLSS | 5-8 | NGX on the Mint card and driver; fonts |
| M8 | The user's decision: Vulkan the default on Windows | the user's own play | — | what happens then to the D3D12 renderer and DLSS 5: the user's call, nothing removed before |

Total before M8: about 23 to 35 working days.

**For M7's list (fable uwr, 2026-11-15):** our map (src\uwconv_draw.c uwv_map) reads its parchment with
uwv_game_file("DATA\\BLNKMAP.BYT"), and uwview.c's uwv_game_file builds "%s\\%s" and fopen()s it. On Linux, with
dist's names lower-cased and '/' separators, that fails: map_bg stays NULL, uwv_map returns 0 and the game silently
shows the original's blocky map. When libuwview.so is built, uwv_game_file (and any other path the DLL builds itself)
must use '/' and lower case off Windows; add "the map opens with our sharper drawing" to M7's Mint checks.

## What needs the user

- **Fonts on Linux: ANSWERED YES (2026-11-15, the user through fable uwr):** Carlito for Calibri (the UI font you
  chose), Selawik for Segoe UI, DejaVu Sans for Tahoma and Verdana, Gelasio for Georgia, Caladea for Cambria,
  Inconsolata for Consolas. All free (OFL / Bitstream-Vera). Used at M7.
- **The Mint machine with its NVIDIA card and driver at M7**, and an hour earlier if possible to try the Windows exe
  under Proton (a hardware probe).
- **Looking sessions at M4 (ray tracing), M5 (DLSS, RR) and M6 (HDR):** a visible run each, the Vulkan renderer beside
  the D3D12 one.
- **M8:** the decision.
- **makedist.py** (the lower-case rename plus the Linux files) still waits for the user's yes; unrelated to this plan
  but pending.

## What can go wrong, most likely first

1. **Packing differences between HLSL cbuffers and SPIR-V:** DXC's default std140-style layout for Vulkan versus D3D12's
   cbuffer packing. Mitigation: `-fvk-use-dx-layout` (DXC keeps D3D's layout) for the constants; check `Constants`' size
   and offsets with a reflection dump in M2.
   **Done in M2 (fable uwr's review, 2026-11-15):** without the flag the structured buffers' structs were std430 (Vtx
   and SVtx 96 bytes with offsets 0/16/32/48/64/80 against the C Vertex's 76 and 0/12/24/32/48/60: the ray-traced paths
   would have read wrong vertices). `-fvk-use-dx-layout` is in buildview.cmd's SPV flags, and `tools\vklayout.py` runs
   at every build: a clang-built probe's offsetof / sizeof of Constants, GpuLight, Vertex and RParticle against the
   SPIR-V's OpMemberDecorate offsets and ArrayStride (cbuffer C, GLight, Vtx, SVtx, particles.hlsl's P). A difference
   stops the build.
2. **NGX on Vulkan:** the extensions must be known before the instance and device exist, and its resource description
   (NVSDK_NGX_Resource_VK) needs image views and formats; mistakes show as a black DLSS output. Mitigation: NGX's own
   logging (log_dir), its helpers_vk.h exactly as given, and SR before RR.
3. **The cards' TLAS rebuilt each frame:** a scratch buffer reused while in flight, which is why there is one frame in
   flight as today.
4. **Validation messages from DXC's SPIR-V:** e.g. ray query capability, or storage buffer alignment. Mitigation:
   spirv-val in the build; the SDK's layer.
5. **The interface layer's colour handling** (UNORM vs SRGB views) shifting the 2D picture slightly. Mitigation: M1's
   comparison of the title screen is exactly this test.
6. **Pace:** render.c keeps moving. Mitigation: the ledger, read at every milestone.
   **And since M2 a check:** render.c's CPU side is copied for render_vk.c in src\render_vk_cpu.h, made by
   `tools\render_vk_cpu.py` from render.c by markers (the text each part starts and ends with, not line numbers);
   buildview.cmd runs it with `--check` and stops when the committed header is no longer what render.c gives, naming
   the first line that differs. A real shared header (render.c including it too) is a question for the user at M8.
7. **Reads outside a resource:** D3D12 returns 0 for a Load or a StructuredBuffer read out of range, Vulkan leaves it
   undefined. **Done in M2:** robustBufferAccess and VK_EXT_robustness2's robustBufferAccess2 / robustImageAccess2 are
   enabled where the device has them, with a log line either way (`render: Vulkan robust reads: buffers ..., images
   ...`). On the RTX 4090: both yes.
8. **Orientation:** the scene is drawn with a negative-height viewport (y = h, height = -h), not -fvk-invert-y, so clip
   space is D3D12's and SV_Position, ddx / ddy and the motion vectors need no shader switch.

## Milestone M1 tasks

- [x] **T1 Loader and instance:** src\vk_load.c/.h (vulkan-1.dll / libvulkan.so.1, vkGetInstanceProcAddr, the
  global, instance and device function tables). Instance 1.3 with surface extensions; optional validation
  (UWVK_VALIDATE) with a counting messenger.
- [x] **T2 Device and queue:** the physical device (discrete first), a graphics queue that can present, the
  log line `render: Vulkan: <GPU> (driver <version>, Vulkan <api version>)`.
- [x] **T3 Swap chain:** a Win32 surface from cfg.hwnd, B8G8R8A8 or R8G8B8A8_UNORM, 2 images, FIFO for vsync; recreated
  when out of date; headless (no hwnd) renders into an offscreen image.
- [x] **T4 The present pass:** present.hlsl's SPIR-V (built by buildview.cmd beside the DXIL), with the interface image,
  a black colour image and a depth image of 1.0 bound as the scene's picture until M2. The constants: post (exposure,
  gamma, ui scale), hdr off, the rest zero.
- [x] **T5 The interface upload:** render_set_ui into a mapped staging buffer, copied each dirty frame.
- [x] **T6 render_readback / render_want_screenshot:** the presented image copied to a host-visible buffer (for
  --window-shot and the compare harness).
- [x] **T7 The rest of render.h:** for M1 every other render_* function exists in render_vk.c as a safe stub (no scene
  yet; DLSS and RT unavailable), so uwview.c links unchanged.
- [x] **T8 uwview_vk.dll:** tools\buildview.cmd builds it from uwview.c + render_vk.c + vk_load.c + the same common
  files (without ngx.c, nrbridge.c, render.c and the D3D12 passes). portwin32.py deploys it beside uwview.dll.
- [x] **T9 The switch:** uwfront.c reads `renderer` (settings, UWPORT_RENDERER) in view_start and loads uwview_vk.dll
  for vulkan, falling back with a log line. The Esc row "Renderer" (Direct3D 12 / Vulkan, next start).
- [x] **T10 tools\vkcompare.py:** hidden runs of the title screen and the open Esc menu with each backend, the window
  shots compared (count of pixels differing by more than 8 of 255 in any channel, and the largest difference), the
  noise floor from two D3D12 runs; the validation run.
- [x] **T11 The proof:** the numbers; a clean exit; a resize (the window stretched) and a full-screen switch (the Esc
  row) in a run; commit; report.

### M1 result (2026-11-15)

Done. `tools\vkcompare.py` (off-screen runs; the switch case also with `--visible`), on the RTX 4090:

| Picture | D3D12 vs D3D12 (noise floor) | Vulkan vs D3D12 |
|---|---|---|
| Title screen | 0 pixels over 8/255, largest 0 | 0 pixels over 8/255, largest 0 |
| Esc menu open (F1, saved game, scene tinted black in both) | 0, largest 0 | 1892, largest 255, all in x 1222-1394, y 363-581 |

The menu's 1892 are explained: the Renderer row's own value ("Vulkan" / "Direct3D 12"), and the scroll bar's thumb,
which is longer because the Vulkan renderer offers fewer rows (no DLSS, ray tracing or HDR rows before their
milestones). The rest of the menu is identical.

- Log: `render: Vulkan: NVIDIA GeForce RTX 4090 (driver 617.42, Vulkan 1.4.351)`.
- Validation (`UWVK_VALIDATE=1`, the SDK's VK_LAYER_KHRONOS_validation): 0 warnings and errors in every run (title,
  menu, switch). The loader's own messages about the machine's other layers (GOG Galaxy's overlay names, ReShade's
  implicit layer failing to load with error 1114) are logged as `render: Vulkan loader:` and not counted.
- Resize and full screen (visible run): swap chain 1920x1080 -> 1264x681 -> 1684x961 (the window stretched by
  SetWindowPos) -> 2560x1440 (the Esc menu's Full screen on) -> 1920x1080 (off). Exit 0, no errors.
- The default is unchanged (no `renderer` setting and no UWPORT_RENDERER: uwview.dll, D3D12).

What differed from the plan:
- The setting lives in the port's settings (uw1port.cfg, `renderer=vulkan|d3d12`, which the front end reads before any
  renderer DLL is loaded), not the renderer's settings.txt.
- When uwview_vk.dll is missing, or Vulkan does not start (no loader, no 1.3 GPU), the front end falls back to
  D3D12 with a log line, not to GDI.
- The present pass draws into an RGBA8 picture of the output's size (render.c's final_tex), which is blitted onto the
  swap chain image. The swap chain follows the window's size; the picture keeps its size and is stretched (DXGI does
  the same for the D3D12 renderer, which also keeps its size).
- One check-only switch was added to uwview.c: `UWVIEW_NOSCENE=1` tints the scene black (render_set_tint), so the
  menu is compared without the D3D12 scene behind it.
- The validation layer is found through the SDK installer's registration; VK_ADD_LAYER_PATH (the SDK's Bin) is needed
  only where it is not registered (vkcompare.py: `VKCOMPARE_LAYER_PATH=1`).

### M2 result (2026-11-15)

Done (e7bec2e, decd7c2 and this commit). `tools\vkcompare.py --m2`, hidden runs from a private copy of the program
(work\vk\port), on the RTX 4090; both renderers with ray tracing, DLSS, DLSS 5, HDR, particles, decals, fog and the
water's effects off (UWVIEW_SETTINGS, never the user's own file) and the player's flame held steady (UWVIEW_STEADY).
Counted: pixels differing by more than 8 of 255 in any channel, and the largest difference.

| View | D3D12 vs D3D12 (noise floor) | Vulkan vs D3D12 | Where both differ |
|---|---|---|---|
| start (the saved game as restored) | 658, largest 60 | 692, largest 60 | the near wall's edge, x 886-919 |
| corridor (26, 21) | 0, largest 0 | 0, largest 1 | nothing |
| things on the floor (27, 12) | 1046, largest 122 | 1040, largest 122 | the poison flask (832), a column at x 1328-1359 |
| hall (46, 44) | 2783, largest 125 | 2241, largest 98 | the flask, a region x 864-1071, y 160-1023 |
| the original's display (the view in its 3D window) | 275, largest 60 | 248, largest 60 | the near wall's edge, x 812-832 |
| corridor, full bright | 864, largest 122 | 864, largest 122 | the flask only |
| by a creature (55, 6) | 8831, largest 72 | 8675, largest 122 | the creature (x 1632-1887, y 752-879), a column at x 1024-1039, the flask |
| the water's edge (49, 2) | 71, largest 20 | 58, largest 23 | the top left, x 19-43, y 4-142 |

- In every view the Vulkan picture differs from D3D12's only in the boxes where two D3D12 runs differ from each other,
  and by about as many pixels (work\vk\m2\stable.py lists the boxes of both; work\vk\m2\stable.txt). What moves between
  runs of the same renderer: the poison flask's bubbles (x 20-103, y 1016-1031), a creature's frame, and edges that
  land a pixel apart (the runs are timed by the clock, not by frames). The hall's region and the water's top-left box
  were not looked at one by one: they are in the D3D12 pair's list too.
- Exact where nothing moves: the corridor (largest difference 1 of 255), and the full-bright corridor outside the flask.
- Validation: 0 warnings and errors in all eight Vulkan runs. `render: Vulkan robust reads: buffers yes
  (robustBufferAccess2), images yes (robustImageAccess2)`.
- The build's two checks ran in a full build (portwin32.py): render_vk_cpu.h is render.c's (326 lines); vklayout: the
  constants (19 members), GpuLight (176 bytes), Vertex (76 bytes) and RParticle (64 bytes) the same in C and SPIR-V.

What differed from the plan, and what is not covered:
- **No view with orbs:** data\lights.txt has its one light commented out, so the orbs' code in render_vk.c ran in no
  comparison. Unproved until a view has one (a copy of the data folder through UWVIEW_DATA).
- **The "interface" view first pressed R,** which now opens our rune page: the view inside the original's 3D window
  was not shown. The case now sets `display=original` in the run's own settings; the numbers above are that run.
- **The user's mouse reaches hidden runs:** with the game being played beside them, one run caught a tooltip under
  the pointer (176703 pixels between two D3D12 runs). Comparisons are run with the game closed.
- The maps' textures (normal, AO, height) are uploaded and the water's constants filled already, but with their
  options off in every comparison: unproved until M3.
- Particles, decals and fog: not ported (M3).
- `-fvk-use-dx-layout`, the robust reads and the negative-height viewport came from fable uwr's review (risks 1, 7, 8).

### M2 again, on held views (2026-11-15 night)

fable uwr's review: a noise floor of hundreds of pixels can hide a real difference. What moved between two D3D12 runs:
the shadow rays' seed is the frame count (scene.hlsl eye.w), the flames' and water's time the clock, the panel's
liquids and the halos the clock, a creature's frame the game's clock, the poisoned flask the game's own screen.
UWVIEW_STEADY now holds all of ours (render_hold_time from the first frame: the frame count stays 0), UWVIEW_CRIT_FORCE
holds the creatures' frame, UWVIEW_VIT_TEST the flasks' levels; the flasks' boxes are masked (they are cut from the
original game's screen and bubble by its clock): x 16-111 y 920-1063 in our display, x 1356-1463 y 675-869 in the
original's. Then, D3D12 vs D3D12 and Vulkan vs D3D12, pixels over 8/255: start, corridor, things, hall, the original's
display, full bright, creature, water's edge: **0 and 0, largest difference 1** in every view (the creature: against
the D3D12 run with the same frame, the second try). Validation 0.

### M3 result (2026-11-15 night)

The effects' passes: src\render_vk_fx.h (render_particles.c's and render_decals.c's CPU side line for line; the ring's
compute passes with a memory barrier between update and spawn; decals then particles drawn into the scene's colour with
its depth tested, a second rendering after the scene's). particles.hlsl and decal.hlsl as SPIR-V (buildview.cmd). The
maps (normal, AO, height, the sprites' maps) and the upscaled textures were uploaded in M2 and are compared here.

Held views (`tools\vkcompare.py --m3`, `--orbs`; flask masked), D3D12 vs D3D12 then Vulkan vs D3D12, pixels over 8/255:

| View | Feature | Floor | Vulkan | Note |
|---|---|---|---|---|
| orbs (8 lights through UWVIEW_DATA=work\vk\orbdata) | the orbs (present's depth path): one before a wall, one cut by a corner | 0 | 3, largest 10 | three lone floor pixels |
| orbs-maps-off | the maps off in placed light (on/off differ: 116 pixels) | 0 | 3, largest 10 | the same three |
| corridor-tex256 / -tex512 | upscaled textures (hatlas) | 0 / 0 | 0 / 0 (largest 1) | |
| pool-on (11, 28) | water (waves, caustics; on/off differ 3105) | 0 | 0, largest 6 | |
| torches (8 wall torches, work\vk\torchdata) | flames, held at 100 s | 0 | 3, largest 19 | three lone pixels |
| embers (6, 42, the campfire) | particles, 64 frames of 16 ms from nothing | 0 | 0, largest 1 | a goblin walking at the left edge masked |
| corridor-decal | a decal | 0 | 1215, largest 24 | see below |

- **The decal:** the difference is a smooth band along the mark's ragged edge. decal.hlsl's noise hashes with
  frac(sin(x) * 43758.5453) for x up to some thousands; the DXIL and SPIR-V paths' sin differ in their last bits there,
  so a lattice cell's value differs and the edge moves smoothly within it. Likely, not proved: the mark's yaw and seed are
  the same (the turn's rand() seeded under UWVIEW_STEADY). Left as is (changing the hash changes D3D12's picture).
- **The maps** shade only the placed lights (the player's own light is the original's, flat): in the corridor maps on
  and off are the same picture; they are compared in the orb ring's light.
- **Fog:** render.c stores fx_fog and draws nothing with it: nothing to port.
- **Open (cannot be held):** the splash rings' birth time (render_water_ripple by the clock): judged by eye later.
- Validation 0 in every Vulkan run. The Linux cross-build links (portlinux.py; the renderer DLL is M7's).
- Check-only switches added: UWVIEW_HOLD_SECS (render.c, 407cfb6, by fable uwr under the user's yes) and
  UWVIEW_PART_DT (render_particles.c, the same commit; render_vk_fx.h's copy); in uwview.c UWVIEW_STEADY's holds
  (the emitters' 16 ms step, rand() seeded at a comparison's start and before the test decal). Unset, the default is
  unchanged: start, corridor, hall, full bright D3D12 before 407cfb6 against after: 0 pixels, largest 0.

### M4 result (2026-11-15 night)

Ray tracing in src\render_vk_rt.h (render.c's start_rt, build_as, cards_as_make, cards_as_build, scene_tlas and the
probes' pass): VK_KHR_acceleration_structure + VK_KHR_ray_query + buffer device addresses + scalarBlockLayout (the
structured buffers' structs in D3D12's packing, -fvk-use-dx-layout: without it spirv-val refused Vtx / SVtx, a float3 at
offset 12). scene.hlsl -D RT (ps_6_5) and probes.hlsl (cs_6_5) as SPIR-V. One descriptor set layout for the scene and
the probes' pass (t3 the top level at 19, t4 probes 20, t5 vertices 21, t8 their numbers 24, t10 the cards 26, u0 48);
the top level and the probes' pair written per frame before the frame's commands. The level's structures are built in
the upload's command buffer; the cards' each frame (fast build), the scratch's address aligned to 256.

Held views with UWVIEW_RT=1 (`vkcompare.py --m4`), D3D12 vs D3D12 then Vulkan vs D3D12, pixels over 8/255:

| View | Floor | Vulkan | RT on vs off (D3D12) |
|---|---|---|---|
| rt-corridor | 0 | 0 (largest 1) | 1640 |
| rt-orbs (8 placed lights) | 0 | 1 (largest 24) | 41601 |
| rt-torches (8 wall torches, flames held) | 0 | 1 (largest 9) | 35212 |
| rt-things (the cards' shadows) | 0 | 0 (largest 1) | 1888 |
| rt-things-nocards (UWVIEW_CARDSHADOW=0) | 0 | 0 (largest 1) | cards on vs off: 1604 |
| rt-pool (water on) | 0 | 0 (largest 5) | |
| rt-creature (frame held) | 0 | 0 (largest 1), second try | |

- Validation 0 in all seven. The probes converge (their seed is the held frame count), so the views are exact.
- Not in M4: ray tracing High (Ray Reconstruction's guides, scene_ps_rth): M5, with NGX.
- For the user's eyes (fable uwr schedules it): uw-vulkan.exe beside uw-new.exe with ray tracing on (the Esc menu's
  Ray tracing row, or settings.txt raytracing 1): the shadows of placed lights and wall torches, the objects' shadows
  ('.' toggles them), the bounced light in a torch-lit hall; then the same with uw-new.exe (D3D12).
- makedist.py lists uwview_vk.dll and uw-vulkan.exe (buildview.cmd builds the starter into build\); not run.

### M5 result (2026-11-15 night)

DLSS through fable uwr's src\ngx_vk.c (43816dc), wired in src\render_vk_dlss.h: NGX loaded before the instance and its
instance / device extensions enabled where offered; Super Resolution at the quality's render size (the scene in the
targets' top-left part, as render.c); the Halton jitter; ray tracing High with scene.hlsl -D RT -D RTHIGH (five
targets: the guides albedo, specular, normal + roughness) and Ray Reconstruction; the present pass reads DLSS's output
(its set's t0 written per frame). Mode changes as render_set_dlss_quality. The output image needs TRANSFER_DST (NGX
clears it). No DLSS 5 on Vulkan (the user's).

Held views (`vkcompare.py --m5`, UWVIEW_JITTER_HOLD=1: eb8f692, the user's yes; default unchanged: four still views
before / after 0 pixels), D3D12 vs D3D12 then Vulkan vs D3D12, pixels over 8/255:

| View | Floor | Vulkan |
|---|---|---|
| DLAA, corridor | 37 (largest 34) | 37 (largest 34), the same pixels |
| DLAA, the orb ring | 1 (largest 9) | 257 (largest 58) |
| DLSS quality (2/3), the orb ring | 1 (largest 9) | 1178 (largest 79) |
| Ray Reconstruction (RT High), the orb ring | 6129 (largest 116) | 6186 (largest 125) |

- The DLSS differences are single texels in the orb-lit grain (the soft shadows' random rays), spread evenly, with no
  edge or object out of place (work\vk\m5\q2-crop.png): the jitter's sign and the motion vectors agree. Taken as
  NGX's own numerics on the two APIs (likely, not proved). Ray Reconstruction is noisy between two D3D12 runs too.
- `Ray Reconstruction: 596 frames, 0 failed` in the Vulkan run. Validation 0. Linux cross-build links.
- For the user's eyes: uw-vulkan.exe with DLSS on (the Esc menu: DLSS, its quality row) and ray tracing High; the same
  in uw-new.exe. dist needs another makedist for these (fable ran it at 20:35, before M5's fixes).
- The decal's edge: accepted by the user ("i don't care if they draw it differently"); not raised again.

### M6 result (2026-11-15 late)

HDR output, the screenshot's SDR copy, resize, full screen (render.c's hdr_display / hdr_apply / hdr_follow and its
present's HDR path):
- The monitor's HDR is asked of DXGI (IDXGIOutput6::GetDesc1 of the output under MonitorFromWindow(cfg.hwnd)), as the
  plan said; off Windows false (X11 has none). Every 30 frames; a change makes the swap chain again.
- HDR on: VK_EXT_swapchain_colorspace, the swap chain R16G16B16A16_SFLOAT in VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT
  (scRGB, DXGI's G10 P709); the present pass draws into an FP16 picture (the same present.hlsl with hdr.x 1), blitted
  onto it. A screenshot with HDR on: the frame drawn again as SDR into the 8-bit picture with the constants' copy at
  2048 (its own descriptor set), read back. The constants' hdr[] as render.c's lines.
- Device loss: a failed submit or present is logged with its code (VK_ERROR_DEVICE_LOST is -4) and the frame returns
  false; render.c only logs too.

Proof:
- Hidden, HDR asked for (settings hdr 1; this machine's monitor runs HDR): both renderers log `render: HDR output on
  (FP16 scRGB)`; the window shots (the SDR copy) D3D12 vs D3D12 / Vulkan vs D3D12: corridor 0 / 0 (largest 1), the orb
  ring 0 / 3 (largest 10, the three floor pixels of M3). SDR start and corridor again 0 / 0. Validation 0.
- Visible (the user allowed it, the game closed): `vkcompare.py --only switch --visible`: HDR on, the swap chain
  1920x1080 -> 1264x681 -> 1684x961 (the window stretched) -> 2560x1440 (full screen) -> 1920x1080, exit 0,
  validation 0.
- For the user's eyes: uw-vulkan.exe on the HDR monitor beside uw-new.exe: torch-lit and orb-lit places (the
  highlights past paper white), the Esc menu's HDR peak and paper white rows moving the picture the same way.

### M7 result, the Windows side (2026-11-16)

libuwview.so (tools\buildview_linux.py), the Linux surface, the fonts, NGX for Linux, the program loading it; the
details in NOTES.md's 2026-11-16 entry. What M7 asked of this side, each:
- libuwview.so in the cross-build: done (a script of its own beside portlinux.py, which builds the program).
- uwview.c's Windows parts, extract.c, smoothtext.c, uwconv_draw.c, the paths: os_posix.h (force-included on Linux
  only), smoothtext_stb.c, uwrec_posix.c; the Windows sources keep their code, guarded where they include windows.h.
- SDL_Vulkan_CreateSurface: not used (it needs the window made with SDL_WINDOW_VULKAN); the X11 or Wayland surface
  from the SDL window's own properties instead.
- uwv_game_file and every other path: '/' and any case, through uwos_fopen (the BLNKMAP.BYT item).
- ngx_vk.c's Linux path against libnvsdk_ngx.a: links; DLSS's own libraries in dlss\ beside the game.
- makelinuxdist / makedist lists: makelinuxdist.py stages the new files; makedist.py already places its staging.
- The WSL hidden run: WSL's Mesa offers llvmpipe (software Vulkan
  1.4, with ray queries), so the renderer started there: the X11 surface, the swap chain (1264x921, 3 images), NGX
  loaded and said "DLSS NOT available" (no NVIDIA: the three NVX / buffer-address extensions not offered), level 1
  built with the same counts as on Windows (16236 triangles, 16090 texture pixels in the cycling colours: the data's
  paths resolve), 1219 presents, exit 0; the window shot (work: O:\wsl\test\run-vk\window.png) the title screen as on
  Windows. With NOVK=1 (the loader given no driver file): "render: Vulkan without VK_KHR_surface", "the Vulkan
  renderer did not start: the original's picture", the game on through SDL's renderer, exit 0.

Not proved here (the Mint machine, M8's eve): the NVIDIA driver's Vulkan, DLSS through NGX on Linux, X11's surface on
real hardware, the free fonts by eye, the map opening with our drawing (the plan's M7 note).

## The ported-features ledger

Base: src/render.c at deaf295 (2026-11-15); read again at M2: no commit to render.c or the shaders since. A new
D3D12-side commit gets a line here at each milestone.

| D3D12 feature | Came in | Vulkan |
|---|---|---|
| Scene raster, textures, lights, torch flames, flicker, orbs | before 2026-11-14 | **done M2** (compared; the orbs in the code but in no view) |
| Normal maps, AO, relief shadows, sprites' maps, upscaled textures | 536b9f3, 81797fa and earlier | **done M3** |
| Water (waves, rings, caustics) | b6cb0bc | **done M3** (rings unproved: their birth time cannot be held) |
| Particles, decals, fog | 2026-11-12 fx plan | **done M3** (fog: render.c draws none; decal's edge, see M3) |
| Check-only holds UWVIEW_HOLD_SECS, UWVIEW_PART_DT | 407cfb6 | render_vk_cpu.h / render_vk_fx.h carry them |
| Check-only UWVIEW_JITTER_HOLD (render_frame) | eb8f692 | render_vk.c the same line |
| Ray tracing, bounce, probes, cards' shadows | earlier | **done M4** (RT High with M5) |
| DLSS SR, Ray Reconstruction | earlier | **done M5** (ngx_vk.c, fable uwr) |
| DLSS 5 (nr_*) | earlier | **N/A** (dropped by the user) |
| Firelight colour CAT16 / Bradford, CAM16 adaptation (CPU side, render_blackbody) | 6622e06 | **done M2** (render.c's CPU code, copied by tools\render_vk_cpu.py and checked at every build) |
| Boulders' Gouraud shade from the palette ramp (models.c) | uncommitted, another session's (NOTES 2026-11-15, levelcheck) | models.c is shared by both DLLs: nothing to port |
| A placed light's strength 0.3 -> 0.24 (scene.hlsl ps) | uncommitted, another session's (seen 2026-11-15 18:33, after M2's comparisons) | scene.hlsl is the one source of both: nothing to port; M2's pictures are from before it |
| Door frame lintel texture | 6622e06 | in the mesh (level.c): nothing to port |
| HDR output (scRGB swap chain, levels, present's HDR) | deaf295 | **done M6** |
