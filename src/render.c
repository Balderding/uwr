#include "render.h"

#include "normals.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ngx.h" /* d3d12.h, dxgi1_4.h with COBJMACROS and WIDL_C_INLINE_WRAPPERS */
#include <dxgi1_6.h> /* IDXGIOutput6: the monitor's HDR (render_hdr_*) */
#include <windows.h>

/* the shaders, compiled by DXC at build time (build.cmd; sources in src\shaders) */
#include "present_ps.h"
#include "present_vs.h"
#include "probes_cs.h"
#include "scene_ps.h"
#include "scene_ps_rt.h"
#include "scene_ps_rth.h"
#include "scene_vs.h"

#define FRAMES 2

/* the effects' passes (docs\superpowers\plans\2026-11-12-fx-audio-and-visual.md): attached by the program that has them
 * (uwview.dll: render_particles.c, Task 3), so a program built without them (build.cmd's uw.exe) draws as before */
static const RenderFxPasses *g_fxp;
void render_fx_attach(const RenderFxPasses *p) { g_fxp = p; }
static struct { int particles, decals, water, fog; } g_fx;   /* render_set_fx: all off unless the settings say */
void render_set_fx(int particles, int decals, int water, int fog) { g_fx.particles = particles, g_fx.decals = decals, g_fx.water = water, g_fx.fog = fog; }
static bool g_rp_ok, g_rd_ok;   /* the particles' and the decals' passes started */
/* the effects' water (fx_water; the scene's shader): the splashes' rings, up to 16, two seconds each */
static struct { float x, z, strength; double born; } g_ripple[16];
static int g_ripples_shown = 1;   /* (the checks: 0 hides them, the water's waves kept) */
void render_water_ripple(float x, float z, float strength) {
    int k = 0;
    for (int i = 1; i < 16; i++) if (g_ripple[i].born < g_ripple[k].born) k = i;
    g_ripple[k].x = x, g_ripple[k].z = z, g_ripple[k].strength = strength;
    g_ripple[k].born = (double)GetTickCount64() / 1000.0;
}
void render_water_ripples_shown(bool on) { g_ripples_shown = on; }
/* the checks' comparisons of an effect on and off (uwview.c, UWVIEW_COMPARE_FX): the flames' time and the frame count
 * (the water's and the lava's motion) held, so that only the effect differs between the two pictures */
static bool g_hold;
static float g_hold_secs;
static double g_hold_now;
static u32 g_hold_frame;
void render_hold_time(bool on) {
    if (on && !g_hold) g_hold_secs = (float)(GetTickCount64() % 100000000ull) / 1000.0f, g_hold_now = (double)GetTickCount64() / 1000.0;
    if (on && !g_hold) {   /* UWVIEW_HOLD_SECS=N (the checks only: tools\vkcompare.py): the held time is N seconds, not
                              the clock's, so that two runs (D3D12 and Vulkan) hold the same flames and water */
        static int got;
        static float fixed = -1;
        if (!got) { const char *e = getenv("UWVIEW_HOLD_SECS"); got = 1; if (e && *e) fixed = (float)atof(e); }
        if (fixed >= 0) g_hold_secs = fixed, g_hold_now = (double)fixed;
    }
    g_hold = on;
}
#define PROBE_LAYERS 6
#define PROBES (64 * 64 * PROBE_LAYERS)
#define PROBE_BYTES ((size_t)PROBES * 6 * 16)

/* the frame's constants: src\shaders\cbuf.hlsli, the same order */
typedef struct {
    float vp[16], vp_nojit[16], vp_prev[16];
    float eye[4], look[4], size[4], post[4];
    float lpos[16][4], lcol[16][4], linfo[4], outp[4];
    float tint[4];
    float rtlim[4];
    float torchc[4];
    float sprite[4];    /* the pictures' Laigter maps: x = occlusion strength, y = height (parallax, texels) */
    float fxw[4];       /* the effects' water: x on, y the ripples, z the time (s) */
    float ripple[16][4];/* the splashes' rings: x, z, age (s), strength */
    float texres[4];    /* the walls' and floors' high-res copy: x its scale (0: off) */
    float hdr[4];       /* HDR output: x on, y paper white / 80 (scRGB's 1.0 is 80 nits), z peak / paper white */
} Constants;
#define CB_SHOT_OFF 2048   /* the screenshot's SDR copy of the constants in the 4096-byte constant buffer (HDR on) */

/* a light as the shaders take it (common.hlsli GLight) */
typedef struct {
    float pr[4], col[4];
    float info[4];      /* x: puffs (0: a point light), y: their emission's sum this frame */
    float puff[4][4];   /* the flame's puffs: position, w its emission (render_flame) */
    float pcol[4][4];   /* their colours (unit luminance, the black body at each one's temperature) */
} GpuLight;

/* ---- the flame (ours, from fire physics; the user's): a column of NPUFF glowing puffs. A pool fire puffs at
 * f = 1.5 / sqrt(D) a second (D its width in metres: Cetegen & Ahmed); its visible height is Heskestad's
 * L = 0.235 Q^0.4 - 1.02 D (Q its heat release in kW). A puff is born at the base each 1/f s and rises through the
 * height in that time (phase phi 0..1); it glows by its soot's incandescence, sin(pi phi)^1.5 x (T / Tmax)^4
 * (Stefan-Boltzmann), the temperature falling toward the tip, its colour the black body's (render_blackbody); it
 * meanders sideways (the flame's sway, wider higher up) and the whole column leans back from the way the flame moves,
 * tan(theta) = 0.7 sqrt(U^2 / g L) (a Froude number). The emissions are scaled so their mean sum is 1: the flicker is
 * the puffs' own coming and going. Positions in tiles: TILE_M metres a tile. ---- */
#define NPUFF 4
#define TILE_M 1.2f
static float bb_table[201][3];   /* the black body's colour, 1000-3000 K in 10 K steps */
static bool bb_ready;   /* (false again when the method or the adaptation changes) */
static void bb_at(float kelvin, float out[3]) {
    if (!bb_ready) {
        for (int i = 0; i <= 200; i++) render_blackbody(1000.0f + 10.0f * (float)i, bb_table[i]);
        bb_ready = true;
    }
    float x = (kelvin - 1000.0f) / 10.0f;
    if (x < 0) x = 0;
    if (x > 199.999f) x = 199.999f;
    int i = (int)x;
    float t = x - (float)i;
    for (int j = 0; j < 3; j++) out[j] = bb_table[i][j] * (1 - t) + bb_table[i + 1][j] * t;
}
static float puff_glow(float phi, float T, float Tmax) {
    float s = sinf(3.14159265f * phi);
    float r = T / Tmax;
    return (s > 0 ? powf(s, 1.5f) : 0) * r * r * r * r;
}
/* the puffs of a flame of width D (m) and heat Q (kW), hottest Tmax (K), at base (tiles), moving at vel (tiles a
   second, x and z), at time t (s), seed its own; the emissions times scale */
static float g_puff_depth = 0.15f;   /* the flame's coherent swell (UWVIEW_PUFF for checks, settings flame_swell) */
void render_set_puff_depth(float d) { g_puff_depth = d < 0 ? 0 : d > 0.5f ? 0.5f : d; }
/* a campfire's own swell (the user, 2026-11-14: deeper pulses, a more dramatic fire): the flames 0.4 m across and
 * wider (a campfire is 0.5) swell and neck this deep instead (settings campfire_swell, UWVIEW_CAMPFIRE for checks;
 * 0.35: the light about half as bright again at a puff's crest and half at its neck); the torches keep flame_swell */
static float g_camp_depth = 0.35f;
void render_set_campfire_depth(float d) { g_camp_depth = d < 0 ? 0 : d > 0.6f ? 0.6f : d; }
/* the flame's slow breath (the user, 2026-11-16: "a deeper slow breathing ... once in a while"). What is measured
 * of real flames: besides the puffing (f = 1.5 / sqrt(D), Cetegen and Ahmed 1993) their light carries slow, aperiodic
 * changes below about 1 Hz, from the flame's shape changing with the air round it and the fuel reaching it (a lull
 * starves it: shorter, dimmer, cooler; a fresh pocket of fuel flares). So not a steady pulse: now and then, each flame
 * at its own times, one swell 3 to 6 s long, two in three a sag and one a flare; about half of every 9 s has none.
 * Returns -1 (the deepest sag) .. 0.6 (a flare); render_flame takes g_breath_depth of it (settings flame_breath, the
 * Esc menu's Fire breathing row; UWVIEW_BREATH for checks), a campfire half as much again, a candle's or a lantern's
 * flame a quarter (sheltered, small) */
static float g_breath_depth = 0.20f;
void render_set_breath_depth(float d) { g_breath_depth = d < 0 ? 0 : d > 0.4f ? 0.4f : d; }
float render_breath_depth(void) { return g_breath_depth; }
static float flame_breath(float t, float seed) {
    float u = t / 9.0f + seed * 17.0f, b = 0;
    for (int k = -1; k <= 0; k++) {   /* (a swell begun in the window before can still be running) */
        float w = floorf(u) + (float)k;
        unsigned h = (unsigned)(long long)w * 2654435761u ^ (unsigned)(seed * 1e6f) * 40503u;
        h ^= h >> 15, h *= 2246822519u, h ^= h >> 13, h *= 3266489917u, h ^= h >> 16;
        if ((h & 0xFF) >= 128) continue;
        float start = w + (float)((h >> 8) & 0xFF) / 255.0f, len = (3.0f + 3.0f * (float)((h >> 16) & 0xFF) / 255.0f) / 9.0f;
        float x = (u - start) / len;
        if (x <= 0 || x >= 1) continue;
        float s = sinf(3.14159265f * x);
        b += (((h >> 24) % 3) ? -1.0f : 0.6f) * s * s;
    }
    return b < -1 ? -1 : b > 0.6f ? 0.6f : b;
}
static void render_flame(GpuLight *g, const float base[3], float D, float Q, float Tmax, float seed, const float vel[2], float t,
                         float scale) {
    float f = 1.5f / sqrtf(D), L = 0.235f * powf(Q, 0.4f) - 1.02f * D;
    if (L < 2.0f * D) L = 2.0f * D;
    /* its slow breath (flame_breath): the flame shorter, dimmer and cooler in a sag, the other way in a flare */
    float br = g_breath_depth * (D >= 0.4f ? 1.5f : D < 0.03f ? 0.25f : 1.0f) * flame_breath(t, seed);
    L *= 1.0f + 0.5f * br, Tmax += 150.0f * br, scale *= 1.0f + br;
    /* the mean sum of the emissions over a period (the puffs evenly spaced in phase) */
    float mean = 0;
    for (int s = 0; s < 64; s++) {
        float phi = (float)s / 64.0f;
        mean += puff_glow(phi, Tmax - 400.0f * powf(phi, 1.3f), Tmax);
    }
    mean = mean / 64.0f * NPUFF;
    if (mean < 1e-6f) mean = 1;
    /* the lean */
    float U = sqrtf(vel[0] * vel[0] + vel[1] * vel[1]) * TILE_M, tl = 0.7f * sqrtf(U * U / (9.81f * L)), ux = 0, uz = 0;
    if (tl > 1.5f) tl = 1.5f;
    if (U > 1e-3f) ux = -vel[0] * TILE_M / U, uz = -vel[1] * TILE_M / U;
    float small = D < 0.03f ? 0.25f : 1.0f;   /* a candle's or a lantern's flame hardly sways in still air */
    float sum = 0, csum[3] = {0, 0, 0};
    for (int j = 0; j < NPUFF; j++) {
        float phase = t * f + (float)j / NPUFF + seed, phi = phase - floorf(phase);
        float T = Tmax - 400.0f * powf(phi, 1.3f);
        float turb = 1.0f + 0.25f * small * sinf(6.2832f * (t * f * 2.3f + (float)j * 0.37f + seed));
        /* each shed puff its own size (the fuel it took in: real fires' light varies 10-20%): a strength 0.55-1.45 for
           this puff's life, from its birth's number; a small flame's less */
        unsigned hb = (unsigned)(long long)floorf(phase) * 2654435761u ^ (unsigned)(j * 40503) ^ (unsigned)(seed * 1e6f);
        hb ^= hb >> 15, hb *= 2246822519u, hb ^= hb >> 13;
        float strength = 1.0f + small * 0.9f * ((float)(hb & 0xFFFF) / 65535.0f - 0.5f);
        /* the whole flame swells and necks once a puff (its luminous area at the puffing frequency), coherent across the
           puffs, so it does not average out: g_puff_depth deep (the user: 12-16% on the screen) */
        float swell = 1.0f + small * (D >= 0.4f ? g_camp_depth : g_puff_depth) * (sinf(6.2832f * (t * f + seed)) + 0.4f * sinf(6.2832f * (t * f * 2.7f + seed * 1.7f)));
        float w = puff_glow(phi, T, Tmax) * turb * strength * swell / mean * scale;
        float y = phi * L, A = small * (0.18f * D + 0.06f * L * phi);
        float ox = A * sinf(6.2832f * (t * f * 0.31f + seed * 7.1f + phi * 0.9f));
        float oz = A * sinf(6.2832f * (t * f * 0.27f + seed * 3.7f + phi * 1.3f) + 1.1f);
        g->puff[j][0] = base[0] + (ox + ux * y * tl) / TILE_M;
        g->puff[j][1] = base[1] + y / TILE_M;
        g->puff[j][2] = base[2] + (oz + uz * y * tl) / TILE_M;
        g->puff[j][3] = w;
        bb_at(T, g->pcol[j]);
        g->pcol[j][3] = T;
        sum += w;
        for (int c = 0; c < 3; c++) csum[c] += w * g->pcol[j][c];
    }
    g->info[0] = NPUFF, g->info[1] = sum;
    for (int c = 0; c < 3; c++) g->col[c] = csum[c];   /* (the sum, for anything that takes the light as a point) */
}
static float g_torch_vel[2];
void render_set_torch_motion(float vx, float vz) { g_torch_vel[0] = vx, g_torch_vel[1] = vz; }

static bool g_want_shot; /* the next frame's picture is copied for render_readback (window) */

/* HDR output (the user, 2026-11-15: real HDR for an HDR monitor): the swap chain FP16 in scRGB (linear, Rec. 709
 * primaries, 1.0 = 80 nits) while Windows runs the window's monitor in HDR and the setting is on. The picture is the
 * same as in SDR up to paper white; above it the highlights (fire, a torch-lit wall up close) go on up to the
 * monitor's peak instead of being eased into white. The interface sits at paper white. */
static bool g_hdr_want = true;          /* settings hdr (on: HDR whenever the monitor runs it) */
static float g_hdr_peak, g_hdr_paper = 200.0f;   /* nits; peak 0: the monitor's own (its MaxLuminance) */
static float g_hdr_display_peak = 1000.0f;       /* what the monitor says it reaches */

static struct {
    RenderConfig cfg;
    char status[320], dlss_text[64];
    IDXGIFactory4 *factory;
    ID3D12Device *dev;
    ID3D12Device5 *dev5;
    ID3D12CommandQueue *queue;
    ID3D12CommandAllocator *alloc;
    ID3D12GraphicsCommandList *cl;
    ID3D12GraphicsCommandList4 *cl4;
    ID3D12Fence *fence;
    HANDLE fence_event;
    u64 fence_value;
    IDXGISwapChain3 *swap;
    ID3D12Resource *back[FRAMES];
    ID3D12DescriptorHeap *rtv_heap, *dsv_heap, *srv_heap;
    UINT rtv_step, srv_step;
    ID3D12RootSignature *root, *croot;
    ID3D12PipelineState *scene_pso, *present_pso, *probe_pso, *high_pso;
    ID3D12PipelineState *present_hdr_pso;   /* the present pass into the HDR (FP16 scRGB) back buffers */
    bool hdr_on;                            /* the swap chain is HDR now (render_set_hdr) */
    int hdr_check;                          /* frames to the next look at the monitor's HDR */
    ID3D12PipelineState *grid_pso; /* with ray tracing started: the shader without it (ray tracing switched off) */
    ID3D12Resource *albedo, *spec, *nrough; /* Ray Reconstruction's guides (ray tracing High) */
    ID3D12Resource *color, *motion, *depth, *dlss_out, *nr_out, *final_tex, *readback, *cbuf, *vbuf, *ibuf, *atlas, *natlas, *snatlas, *grid;
    ID3D12Resource *saoatlas;   /* the pictures' occlusion maps (R8, the atlas's places) */
    ID3D12Resource *hatlas;     /* the walls' and floors' upscaled textures (render_set_hires; 1 x 1 when off) */
    int hatlas_f;               /* its scale (0: off) */
    bool hatlas_maps;           /* its own relief maps in its right half (render_set_hires) */
    bool hatlas_colours;        /* the upscaled colours in its left half (off: only the maps are its own) */
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT readback_fp;
    Constants *cb;
    int vertices, indices;
    float prev_vp[16];
    bool have_prev;
    /* ray tracing: the level's acceleration structures and the probes (two, one written while the other is read) */
    bool rt, rt_off; /* started on this GPU; switched off (any frame) */
    ID3D12Resource *blas, *tlas, *probes[2];
    /* the pictures in the ray tracing (ours): their bottom level (the cards, rebuilt each frame), the top level with
       the level and them (mask 1 the level, 2 the cards), its scratch, its two instances (mapped) */
    ID3D12Resource *cblas, *ctlas, *cscratch, *cinst;
    ID3D12Resource *cpos;   /* the cards' corners as drawn (3 floats each, mapped: cpos_ptr), the bottom level's input */
    float *cpos_ptr;
    D3D12_RAYTRACING_INSTANCE_DESC *cinst_ptr;
    /* the lights: [0] the torch, then the placed ones; and each tile's list */
    ID3D12Resource *lights_buf, *tiles_buf;
    GpuLight *lights_ptr;
    /* the picture cards (objects, wall torches, later creatures): rewritten when they change */
    ID3D12Resource *sprite_vb;
    /* the interface overlay (ui.c): a whole factor smaller than the window, uploaded every frame */
    ID3D12Resource *ui_tex, *ui_up;
    u8 *ui_ptr;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT ui_fp;
    int ui_w, ui_h, ui_s;
    bool ui_dirty;
    Vertex *sprite_ptr;
    int nsprites;
    Light placed[MAX_LIGHTS];
    int nplaced;
    /* DLSS: started (dlss_ok, nr_ok), and on (the player's choice) */
    bool dlss_ok, nr_ok, on, nr_on; /* nr_on: DLSS 5 wanted (the options) */
    char gpu[128];
    int rw, rh;
    u32 frame;
    int nr_frames_ok, nr_frames_failed;
    bool high_on, high_active; /* ray tracing High wanted; and what the last frame got */
    int rr_frames_ok, rr_frames_failed;
    float w2v[16], v2c[16];    /* this frame's matrices for Ray Reconstruction (row vectors) */
} R;

/* --- helpers -------------------------------------------------------------------------------------- */
static void wait_gpu(void) {
    R.fence_value++;
    ID3D12CommandQueue_Signal(R.queue, R.fence, R.fence_value);
    if (ID3D12Fence_GetCompletedValue(R.fence) < R.fence_value) {
        ID3D12Fence_SetEventOnCompletion(R.fence, R.fence_value, R.fence_event);
        WaitForSingleObject(R.fence_event, INFINITE);
    }
}

static void submit_and_wait(void) {
    ID3D12GraphicsCommandList_Close(R.cl);
    ID3D12CommandList *lists[1] = {(ID3D12CommandList *)R.cl};
    ID3D12CommandQueue_ExecuteCommandLists(R.queue, 1, lists);
    wait_gpu();
    ID3D12CommandAllocator_Reset(R.alloc);
    ID3D12GraphicsCommandList_Reset(R.cl, R.alloc, NULL);
}

static void barrier(ID3D12Resource *res, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
    if (from == to) return;
    D3D12_RESOURCE_BARRIER b;
    memset(&b, 0, sizeof b);
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = res;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = from;
    b.Transition.StateAfter = to;
    ID3D12GraphicsCommandList_ResourceBarrier(R.cl, 1, &b);
}
static void uav_barrier(ID3D12Resource *res) {
    D3D12_RESOURCE_BARRIER b;
    memset(&b, 0, sizeof b);
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    b.UAV.pResource = res;
    ID3D12GraphicsCommandList_ResourceBarrier(R.cl, 1, &b);
}

static ID3D12Resource *make_buffer_ex(size_t size, D3D12_HEAP_TYPE heap, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_FLAGS flags) {
    D3D12_HEAP_PROPERTIES hp = {heap, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1};
    D3D12_RESOURCE_DESC d;
    memset(&d, 0, sizeof d);
    d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width = size;
    d.Height = d.DepthOrArraySize = d.MipLevels = 1;
    d.SampleDesc.Count = 1;
    d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    d.Flags = flags;
    ID3D12Resource *r = NULL;
    if (FAILED(ID3D12Device_CreateCommittedResource(R.dev, &hp, D3D12_HEAP_FLAG_NONE, &d, state, NULL, &IID_ID3D12Resource, (void **)&r)))
        return NULL;
    return r;
}
static ID3D12Resource *make_buffer(size_t size, D3D12_HEAP_TYPE heap, D3D12_RESOURCE_STATES state) {
    return make_buffer_ex(size, heap, state, D3D12_RESOURCE_FLAG_NONE);
}

static ID3D12Resource *make_texture(int w, int h, DXGI_FORMAT fmt, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state,
                                    const D3D12_CLEAR_VALUE *clear) {
    D3D12_HEAP_PROPERTIES hp = {D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1};
    D3D12_RESOURCE_DESC d;
    memset(&d, 0, sizeof d);
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d.Width = (UINT64)w;
    d.Height = (UINT)h;
    d.DepthOrArraySize = d.MipLevels = 1;
    d.Format = fmt;
    d.SampleDesc.Count = 1;
    d.Flags = flags;
    ID3D12Resource *r = NULL;
    if (FAILED(ID3D12Device_CreateCommittedResource(R.dev, &hp, D3D12_HEAP_FLAG_NONE, &d, state, clear, &IID_ID3D12Resource, (void **)&r)))
        return NULL;
    return r;
}

static D3D12_CPU_DESCRIPTOR_HANDLE cpu_at(ID3D12DescriptorHeap *h, UINT step, int i) {
    D3D12_CPU_DESCRIPTOR_HANDLE c = ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(h);
    c.ptr += (SIZE_T)step * (SIZE_T)i;
    return c;
}

static D3D12_GPU_DESCRIPTOR_HANDLE gpu_at(int i) {
    D3D12_GPU_DESCRIPTOR_HANDLE g = ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(R.srv_heap);
    g.ptr += (UINT64)R.srv_step * (UINT64)i;
    return g;
}

static void srv(ID3D12Resource *res, DXGI_FORMAT fmt, int slot) {
    D3D12_SHADER_RESOURCE_VIEW_DESC d;
    memset(&d, 0, sizeof d);
    d.Format = fmt;
    d.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    d.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    d.Texture2D.MipLevels = 1;
    ID3D12Device_CreateShaderResourceView(R.dev, res, &d, cpu_at(R.srv_heap, R.srv_step, slot));
}

static D3D12_SHADER_BYTECODE code(const void *p, size_t n) {
    D3D12_SHADER_BYTECODE b = {p, n};
    return b;
}

/* scene: 0 the present pass (one 8-bit target); 2 the scene (colour, motion); 5 with Ray Reconstruction's guides */
static ID3D12PipelineState *make_pso(D3D12_SHADER_BYTECODE vs, D3D12_SHADER_BYTECODE ps, int scene) {
    static const D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"POSITION", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 60, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d;
    memset(&d, 0, sizeof d);
    d.pRootSignature = R.root;
    d.VS = vs, d.PS = ps;
    d.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    for (int t = 1; t < 5; t++) d.BlendState.RenderTarget[t].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    d.SampleMask = 0xFFFFFFFFu;
    d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    d.RasterizerState.DepthClipEnable = TRUE;
    d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.SampleDesc.Count = 1;
    if (scene > 0) {
        d.InputLayout.pInputElementDescs = layout;
        d.InputLayout.NumElements = COUNT(layout);
        d.DepthStencilState.DepthEnable = TRUE;
        d.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        d.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        d.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        d.NumRenderTargets = (UINT)scene;
        d.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
        d.RTVFormats[1] = DXGI_FORMAT_R16G16_FLOAT;
        if (scene == 5) {
            d.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;     /* diffuse albedo */
            d.RTVFormats[3] = DXGI_FORMAT_R8G8B8A8_UNORM;     /* specular albedo */
            d.RTVFormats[4] = DXGI_FORMAT_R16G16B16A16_FLOAT; /* normal + roughness */
        }
    } else {
        d.NumRenderTargets = 1;
        d.RTVFormats[0] = scene < 0 ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;   /* -1: the HDR present */
    }
    ID3D12PipelineState *pso = NULL;
    HRESULT hr = ID3D12Device_CreateGraphicsPipelineState(R.dev, &d, &IID_ID3D12PipelineState, (void **)&pso);
    if (FAILED(hr)) logf_("render: pipeline state failed 0x%08lX\n", (unsigned long)hr);
    return pso;
}

static ID3D12RootSignature *serialize(const D3D12_ROOT_SIGNATURE_DESC *rd) {
    ID3DBlob *blob = NULL, *err = NULL;
    ID3D12RootSignature *rs = NULL;
    if (FAILED(D3D12SerializeRootSignature(rd, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err))) {
        logf_("render: root signature: %s\n", err ? (const char *)ID3D10Blob_GetBufferPointer(err) : "?");
        return NULL;
    }
    if (FAILED(ID3D12Device_CreateRootSignature(R.dev, 0, ID3D10Blob_GetBufferPointer(blob), ID3D10Blob_GetBufferSize(blob),
                                                &IID_ID3D12RootSignature, (void **)&rs)))
        return NULL;
    return rs;
}

/* graphics: b0, table t0-t2, t3 the scene's acceleration structure, t4 probes, t6 lights, t7 tile lists, t5 vertices,
 * t8 vertex numbers (ray tracing High's bounce rays)
 * compute (probes): b0, table t0-t2, t3, t4 last frame's probes, t5 vertices, t6, t7, u0 this frame's probes */
enum { GP_CB, GP_TABLE, GP_AS, GP_PROBES, GP_LIGHTS, GP_TILES, GP_VERTS, GP_INDEX, GP_SVERTS, GP_COUNT };
enum { CP_CB, CP_TABLE, CP_AS, CP_PREV, CP_VERTS, CP_LIGHTS, CP_TILES, CP_INDEX, CP_OUT, CP_COUNT };
static bool make_roots(void) {
    /* the table: atlas, normal atlas, grid (t0-t2), then the pictures' normal maps (t9, the table's fourth), and
     * their occlusion maps (t11, slot SLOT_SAOATLAS = 13: after the present pass's tables) */
    D3D12_DESCRIPTOR_RANGE ranges[4] = {{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 0, 0}, {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 9, 0, 3},
                                        {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 11, 0, 13},
                                        {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 12, 0, 15}};   /* t12: the upscaled textures */
    D3D12_ROOT_PARAMETER p[CP_COUNT];
    memset(p, 0, sizeof p);
    p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    p[1].DescriptorTable.NumDescriptorRanges = 4;
    p[1].DescriptorTable.pDescriptorRanges = ranges;
    static const UINT GREG[7] = {3, 4, 6, 7, 5, 8, 10};   /* t10: the cards' vertices (their shadows) */
    for (int i = 0; i < 7; i++) p[2 + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV, p[2 + i].Descriptor.ShaderRegister = GREG[i];
    D3D12_ROOT_SIGNATURE_DESC rd = {GP_COUNT, p, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
    R.root = serialize(&rd);
    static const UINT CREG[6] = {3, 4, 5, 6, 7, 8};
    for (int i = 0; i < 6; i++) p[2 + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV, p[2 + i].Descriptor.ShaderRegister = CREG[i];
    p[CP_OUT].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV, p[CP_OUT].Descriptor.ShaderRegister = 0;
    D3D12_ROOT_SIGNATURE_DESC cd = {CP_COUNT, p, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_NONE};
    R.croot = serialize(&cd);
    return R.root && R.croot;
}

/* descriptor slots: the scene's table (atlas, normal atlas, grid); the present pass's tables (the picture shown,
 * the depth, one unused) */
enum { SLOT_ATLAS = 0, SLOT_NATLAS = 1, SLOT_GRID = 2, SLOT_SNATLAS = 3, SLOT_COLOR = 4, SLOT_DLSS = 7, SLOT_NR = 10, SLOT_SAOATLAS = 13, SLOT_HATLAS = 15, HEAP_SRV = 16 };
enum { RTV_COLOR = 0, RTV_MOTION = 1, RTV_ALBEDO = 2, RTV_SPEC = 3, RTV_NROUGH = 4, RTV_FINAL = 5, RTV_BACK = 6, RTVS = RTV_BACK + FRAMES };

/* ---- HDR output (g_hdr_*, above) ---- */
/* the monitor the window is on: true when Windows runs it in HDR (its colour space ST.2084 / Rec. 2020); *peak its
   MaxLuminance (nits) */
static bool hdr_display(float *peak) {
    if (!R.swap) return false;
    IDXGIOutput *o = NULL;
    if (FAILED(IDXGISwapChain3_GetContainingOutput(R.swap, &o)) || !o) return false;
    IDXGIOutput6 *o6 = NULL;
    bool on = false;
    if (SUCCEEDED(IDXGIOutput_QueryInterface(o, &IID_IDXGIOutput6, (void **)&o6)) && o6) {
        DXGI_OUTPUT_DESC1 d;
        if (SUCCEEDED(IDXGIOutput6_GetDesc1(o6, &d))) {
            on = d.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
            if (peak) *peak = d.MaxLuminance > 80.0f ? d.MaxLuminance : 1000.0f;
        }
        IDXGIOutput6_Release(o6);
    }
    IDXGIOutput_Release(o);
    return on;
}
/* the swap chain's buffers made again in the format for on (FP16 scRGB) or off (8-bit sRGB), the GPU idle first */
static void hdr_apply(bool on) {
    if (!R.swap || on == R.hdr_on || (on && !R.present_hdr_pso)) return;
    wait_gpu();
    for (int i = 0; i < FRAMES; i++) if (R.back[i]) ID3D12Resource_Release(R.back[i]), R.back[i] = NULL;
    DXGI_FORMAT f = on ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
    HRESULT hr = IDXGISwapChain3_ResizeBuffers(R.swap, FRAMES, (UINT)R.cfg.w, (UINT)R.cfg.h, f, 0);
    if (FAILED(hr) && on) {   /* back to SDR as it was */
        logf_("render: HDR: the swap chain would not take FP16 (0x%08lX)\n", (unsigned long)hr);
        on = false, f = DXGI_FORMAT_R8G8B8A8_UNORM;
        IDXGISwapChain3_ResizeBuffers(R.swap, FRAMES, (UINT)R.cfg.w, (UINT)R.cfg.h, f, 0);
    }
    IDXGISwapChain3_SetColorSpace1(R.swap, on ? DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709 : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
    for (int i = 0; i < FRAMES; i++) {
        IDXGISwapChain3_GetBuffer(R.swap, (UINT)i, &IID_ID3D12Resource, (void **)&R.back[i]);
        ID3D12Device_CreateRenderTargetView(R.dev, R.back[i], NULL, cpu_at(R.rtv_heap, R.rtv_step, RTV_BACK + i));
    }
    R.hdr_on = on;
    logf_("render: HDR output %s\n", on ? "on (FP16 scRGB)" : "off");
}
/* each frame: HDR on while it is wanted and the monitor runs HDR (looked at twice a second: the window may move to
   another monitor, or HDR be switched in Windows' settings) */
static void hdr_follow(void) {
    if (R.hdr_check-- > 0) return;
    R.hdr_check = 30;
    float peak = g_hdr_display_peak;
    bool disp = hdr_display(&peak);
    g_hdr_display_peak = peak;
    hdr_apply(g_hdr_want && disp);
}
void render_set_hdr(bool on) { g_hdr_want = on, R.hdr_check = 0; }
bool render_hdr_wanted(void) { return g_hdr_want; }
bool render_hdr_on(void) { return R.hdr_on; }
bool render_hdr_available(void) { float p; return hdr_display(&p); }
void render_set_hdr_levels(float peak, float paper) {
    g_hdr_peak = peak < 0 ? 0 : peak > 10000 ? 10000 : peak;
    g_hdr_paper = paper < 80 ? 80 : paper > 500 ? 500 : paper;
}
float render_hdr_peak(void) { return g_hdr_peak > 0 ? g_hdr_peak : g_hdr_display_peak; }
float render_hdr_peak_setting(void) { return g_hdr_peak; }
float render_hdr_paper(void) { return g_hdr_paper; }

static void set_dlss_text(void) {
    snprintf(R.dlss_text, sizeof R.dlss_text, "%s", !R.dlss_ok ? "DLSS unavailable"
                                                    : !R.on    ? "DLSS off"
                                                    : R.nr_ok && R.nr_on ? "DLSS 5 on"
                                                    : R.nr_ok  ? "DLSS on (DLSS 5 off)"
                                                               : "DLSS on (no DLSS 5 on this card)");
}

/* the render size for each quality (NVIDIA's ratios): performance 1/2, balanced 0.58, quality 2/3, DLAA 1, ultra
 * performance 1/3; and NGX's name for each */
static const float DLSS_SCALE[5] = {0.5f, 0.58f, 0.6667f, 1.0f, 1.0f / 3.0f};
static const int DLSS_NGX_Q[5] = {DLSS_PERF, DLSS_BALANCED, DLSS_QUALITY, DLSS_DLAA, DLSS_ULTRAPERF};
static const char *const DLSS_QN[5] = {"performance", "balanced", "quality", "DLAA", "ultra performance"};
void render_size_for(int q, int *rw, int *rh) {
    q = MAX(0, MIN(q, 4));
    *rw = MAX(64, (int)((float)R.cfg.w * DLSS_SCALE[q] + 0.5f));
    *rh = MAX(64, (int)((float)R.cfg.h * DLSS_SCALE[q] + 0.5f));
}
static void start_dlss(void) {
    int q = R.cfg.dlss_quality = MAX(0, MIN(R.cfg.dlss_quality, 4));
    const int *NGX_Q = DLSS_NGX_Q;
    render_size_for(q, &R.rw, &R.rh);
    const wchar_t *search[1] = {R.cfg.dlss_dir};
    if (R.cfg.log_dir) CreateDirectoryW(R.cfg.log_dir, NULL);
    if (!ngx_start(R.dev, R.cfg.log_dir, search, 1)) return;
    if (!ngx_dlss_create(R.cl, R.rw, R.rh, R.cfg.w, R.cfg.h, NGX_Q[q])) {
        submit_and_wait();
        return;
    }
    submit_and_wait();
    R.dlss_ok = true;
    /* DLSS 5 is not tried on an RTX 20-series card or older (DLSS upscaling works there; DLSS 5 is taken not to) */
    bool old_card = strstr(R.gpu, "RTX 20") || strstr(R.gpu, "GTX") || strstr(R.gpu, "Quadro RTX");
    /* started whatever the settings say (they only say whether it starts on), so ' can switch it any time */
    if (old_card) logf_("render: %s: DLSS 5 is not tried on this card (DLSS only)\n", R.gpu);
    if (!old_card) {
        wchar_t nr_dll[MAX_PATH];
        swprintf(nr_dll, MAX_PATH, L"%ls\\nvngx_dlssnr.dll", R.cfg.dlss_dir);
        NrSettings ns = {R.cfg.nr_intensity, R.cfg.nr_structure, R.cfg.nr_tone, 1.0f, R.cfg.nr_style, 0, 0};
        {   /* the checks: DLSS 5's skin structure (UWVIEW_D5SKIN) and auto mask (UWVIEW_D5AUTOMASK) */
            const char *e = getenv("UWVIEW_D5SKIN");
            if (e && *e) ns.skin_structure = (float)atof(e);
            e = getenv("UWVIEW_D5AUTOMASK");
            if (e && *e) ns.auto_mask = atoi(e);
        }
        R.nr_ok = ngx_nr_start(R.dev, R.cl, nr_dll, R.cfg.log_dir, R.cfg.w, R.cfg.h, &ns, 1);
        submit_and_wait();
    }
}

/* hardware ray tracing: DXR 1.1 (inline ray queries) on this GPU, and the shaders that use it */
static void start_rt(void) {
    R.rt_off = !R.cfg.raytracing; /* started all the same, so the ; key can switch it on */
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 o5;
    memset(&o5, 0, sizeof o5);
    if (FAILED(ID3D12Device_CheckFeatureSupport(R.dev, D3D12_FEATURE_D3D12_OPTIONS5, &o5, sizeof o5)) ||
        o5.RaytracingTier < D3D12_RAYTRACING_TIER_1_1) {
        logf_("render: this GPU has no DXR 1.1: shadows are traced through the tile grid instead\n");
        return;
    }
    if (FAILED(ID3D12Device_QueryInterface(R.dev, &IID_ID3D12Device5, (void **)&R.dev5)) ||
        FAILED(ID3D12GraphicsCommandList_QueryInterface(R.cl, &IID_ID3D12GraphicsCommandList4, (void **)&R.cl4)))
        return;
    D3D12_COMPUTE_PIPELINE_STATE_DESC cd;
    memset(&cd, 0, sizeof cd);
    cd.pRootSignature = R.croot;
    cd.CS = code(PROBES_CS, sizeof PROBES_CS);
    if (FAILED(ID3D12Device_CreateComputePipelineState(R.dev, &cd, &IID_ID3D12PipelineState, (void **)&R.probe_pso))) return;
    ID3D12PipelineState *ps = make_pso(code(SCENE_VS, sizeof SCENE_VS), code(SCENE_PS_RT, sizeof SCENE_PS_RT), 2);
    R.high_pso = make_pso(code(SCENE_VS, sizeof SCENE_VS), code(SCENE_PS_RTH, sizeof SCENE_PS_RTH), 5);
    if (!ps) return;
    for (int i = 0; i < 2; i++) {
        R.probes[i] = make_buffer_ex(PROBE_BYTES, D3D12_HEAP_TYPE_DEFAULT,
                                     D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                     D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        if (!R.probes[i]) return;
    }
    R.grid_pso = R.scene_pso;
    R.scene_pso = ps;
    R.rt = true;
}

bool render_init(const RenderConfig *cfg) {
    memset(&R, 0, sizeof R);
    R.cfg = *cfg;
    if (R.cfg.exposure <= 0) R.cfg.exposure = 2;
    if (FAILED(CreateDXGIFactory1(&IID_IDXGIFactory4, (void **)&R.factory))) return logf_("render: no DXGI\n"), false;
    IDXGIAdapter1 *adapter = NULL;
    char gpu[128] = "?";
    for (UINT i = 0; IDXGIFactory4_EnumAdapters1(R.factory, i, &adapter) == S_OK; i++) {
        DXGI_ADAPTER_DESC1 d;
        IDXGIAdapter1_GetDesc1(adapter, &d);
        if (!(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) &&
            SUCCEEDED(D3D12CreateDevice((IUnknown *)adapter, D3D_FEATURE_LEVEL_12_0, &IID_ID3D12Device, (void **)&R.dev))) {
            WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, gpu, sizeof gpu, NULL, NULL);
            memcpy(R.gpu, gpu, sizeof R.gpu);
            IDXGIAdapter1_Release(adapter);
            break;
        }
        IDXGIAdapter1_Release(adapter);
    }
    if (!R.dev) return logf_("render: no D3D12 GPU\n"), false;
    D3D12_COMMAND_QUEUE_DESC qd = {D3D12_COMMAND_LIST_TYPE_DIRECT, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0};
    ID3D12Device_CreateCommandQueue(R.dev, &qd, &IID_ID3D12CommandQueue, (void **)&R.queue);
    ID3D12Device_CreateCommandAllocator(R.dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&R.alloc);
    ID3D12Device_CreateCommandList(R.dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, R.alloc, NULL, &IID_ID3D12GraphicsCommandList, (void **)&R.cl);
    ID3D12Device_CreateFence(R.dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&R.fence);
    R.fence_event = CreateEventW(NULL, FALSE, FALSE, NULL);

    D3D12_DESCRIPTOR_HEAP_DESC hd = {D3D12_DESCRIPTOR_HEAP_TYPE_RTV, RTVS, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0};
    ID3D12Device_CreateDescriptorHeap(R.dev, &hd, &IID_ID3D12DescriptorHeap, (void **)&R.rtv_heap);
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV, hd.NumDescriptors = 1;
    ID3D12Device_CreateDescriptorHeap(R.dev, &hd, &IID_ID3D12DescriptorHeap, (void **)&R.dsv_heap);
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, hd.NumDescriptors = HEAP_SRV, hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ID3D12Device_CreateDescriptorHeap(R.dev, &hd, &IID_ID3D12DescriptorHeap, (void **)&R.srv_heap);
    R.rtv_step = ID3D12Device_GetDescriptorHandleIncrementSize(R.dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    R.srv_step = ID3D12Device_GetDescriptorHandleIncrementSize(R.dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    /* every target at the output size; with DLSS on, the scene is drawn into its top-left render-size part */
    int w = cfg->w, h = cfg->h;
    D3D12_CLEAR_VALUE cc = {DXGI_FORMAT_R16G16B16A16_FLOAT, {{0, 0, 0, 1}}};
    R.color = make_texture(w, h, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &cc);
    D3D12_CLEAR_VALUE mc = {DXGI_FORMAT_R16G16_FLOAT, {{0, 0, 0, 0}}};
    R.motion = make_texture(w, h, DXGI_FORMAT_R16G16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &mc);
    D3D12_CLEAR_VALUE gc = {DXGI_FORMAT_R8G8B8A8_UNORM, {{0, 0, 0, 0}}}, nc = {DXGI_FORMAT_R16G16B16A16_FLOAT, {{0, 0, 0, 0}}};
    R.albedo = make_texture(w, h, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &gc);
    R.spec = make_texture(w, h, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &gc);
    R.nrough = make_texture(w, h, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &nc);
    D3D12_CLEAR_VALUE dc;
    memset(&dc, 0, sizeof dc);
    dc.Format = DXGI_FORMAT_D32_FLOAT, dc.DepthStencil.Depth = 1.0f;
    R.depth = make_texture(w, h, DXGI_FORMAT_R32_TYPELESS, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &dc);
    R.dlss_out = make_texture(w, h, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, NULL);
    R.nr_out = make_texture(w, h, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, NULL);
    D3D12_CLEAR_VALUE fc = {DXGI_FORMAT_R8G8B8A8_UNORM, {{0, 0, 0, 1}}};
    R.final_tex = make_texture(w, h, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE, &fc);
    R.cbuf = make_buffer(4096, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    R.lights_buf = make_buffer(sizeof(GpuLight) * (MAX_LIGHTS + 1), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    R.tiles_buf = make_buffer(sizeof(u32) * 64 * 64 * TILE_LIGHTS, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    R.sprite_vb = make_buffer(sizeof(Vertex) * RENDER_MAX_SPRITE_VERTS, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    R.ui_s = R.cfg.ui_scale > 0 ? R.cfg.ui_scale : MAX(1, (h + 150) / 300), R.ui_w = w / R.ui_s, R.ui_h = h / R.ui_s;
    R.ui_tex = make_texture(R.ui_w, R.ui_h, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, NULL);
    if (R.ui_tex) {
        D3D12_RESOURCE_DESC ud = ID3D12Resource_GetDesc(R.ui_tex);
        UINT64 ut = 0;
        ID3D12Device_GetCopyableFootprints(R.dev, &ud, 0, 1, 0, &R.ui_fp, NULL, NULL, &ut);
        R.ui_up = make_buffer((size_t)ut, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    }
    if (!R.color || !R.motion || !R.albedo || !R.spec || !R.nrough || !R.depth || !R.dlss_out || !R.nr_out || !R.final_tex || !R.cbuf || !R.lights_buf || !R.tiles_buf)
        return logf_("render: out of GPU memory\n"), false;
    D3D12_RANGE none = {0, 0};
    ID3D12Resource_Map(R.cbuf, 0, &none, (void **)&R.cb);
    ID3D12Resource_Map(R.lights_buf, 0, &none, (void **)&R.lights_ptr);
    if (!R.sprite_vb) return logf_("render: out of GPU memory (sprites)\n"), false;
    ID3D12Resource_Map(R.sprite_vb, 0, &none, (void **)&R.sprite_ptr);
    memset(R.lights_ptr, 0, sizeof(GpuLight) * (MAX_LIGHTS + 1));
    u32 *tp = NULL;
    ID3D12Resource_Map(R.tiles_buf, 0, &none, (void **)&tp);
    for (int i = 0; i < 64 * 64 * TILE_LIGHTS; i++) tp[i] = NO_LIGHT;
    ID3D12Resource_Unmap(R.tiles_buf, 0, NULL);
    D3D12_RESOURCE_DESC fd = ID3D12Resource_GetDesc(R.final_tex);
    UINT64 total = 0;
    ID3D12Device_GetCopyableFootprints(R.dev, &fd, 0, 1, 0, &R.readback_fp, NULL, NULL, &total);
    R.readback = make_buffer((size_t)total, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);

    ID3D12Device_CreateRenderTargetView(R.dev, R.color, NULL, cpu_at(R.rtv_heap, R.rtv_step, RTV_COLOR));
    ID3D12Device_CreateRenderTargetView(R.dev, R.motion, NULL, cpu_at(R.rtv_heap, R.rtv_step, RTV_MOTION));
    ID3D12Device_CreateRenderTargetView(R.dev, R.final_tex, NULL, cpu_at(R.rtv_heap, R.rtv_step, RTV_FINAL));
    ID3D12Device_CreateRenderTargetView(R.dev, R.albedo, NULL, cpu_at(R.rtv_heap, R.rtv_step, RTV_ALBEDO));
    ID3D12Device_CreateRenderTargetView(R.dev, R.spec, NULL, cpu_at(R.rtv_heap, R.rtv_step, RTV_SPEC));
    ID3D12Device_CreateRenderTargetView(R.dev, R.nrough, NULL, cpu_at(R.rtv_heap, R.rtv_step, RTV_NROUGH));
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv;
    memset(&dsv, 0, sizeof dsv);
    dsv.Format = DXGI_FORMAT_D32_FLOAT, dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    ID3D12Device_CreateDepthStencilView(R.dev, R.depth, &dsv, cpu_at(R.dsv_heap, 0, 0));
    /* the present tables: the picture shown, then the depth (for the orbs) */
    ID3D12Resource *shown[3] = {R.color, R.dlss_out, R.nr_out};
    const int slot[3] = {SLOT_COLOR, SLOT_DLSS, SLOT_NR};
    for (int i = 0; i < 3; i++) {
        srv(shown[i], DXGI_FORMAT_R16G16B16A16_FLOAT, slot[i]);
        srv(R.depth, DXGI_FORMAT_R32_FLOAT, slot[i] + 1);
        srv(R.ui_tex, DXGI_FORMAT_R8G8B8A8_UNORM, slot[i] + 2);
    }

    if (cfg->hwnd) {
        DXGI_SWAP_CHAIN_DESC1 sd;
        memset(&sd, 0, sizeof sd);
        sd.Width = (UINT)w, sd.Height = (UINT)h, sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM, sd.SampleDesc.Count = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT, sd.BufferCount = FRAMES, sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        IDXGISwapChain1 *s1 = NULL;
        if (FAILED(IDXGIFactory4_CreateSwapChainForHwnd(R.factory, (IUnknown *)R.queue, (HWND)cfg->hwnd, &sd, NULL, NULL, &s1)))
            return logf_("render: swap chain failed\n"), false;
        IDXGIFactory4_MakeWindowAssociation(R.factory, (HWND)cfg->hwnd, DXGI_MWA_NO_ALT_ENTER);
        IDXGISwapChain1_QueryInterface(s1, &IID_IDXGISwapChain3, (void **)&R.swap);
        IDXGISwapChain1_Release(s1);
        for (int i = 0; i < FRAMES; i++) {
            IDXGISwapChain3_GetBuffer(R.swap, (UINT)i, &IID_ID3D12Resource, (void **)&R.back[i]);
            ID3D12Device_CreateRenderTargetView(R.dev, R.back[i], NULL, cpu_at(R.rtv_heap, R.rtv_step, RTV_BACK + i));
        }
    }
    if (!make_roots()) return logf_("render: root signature failed\n"), false;
    R.scene_pso = make_pso(code(SCENE_VS, sizeof SCENE_VS), code(SCENE_PS, sizeof SCENE_PS), 2);
    R.present_pso = make_pso(code(PRESENT_VS, sizeof PRESENT_VS), code(PRESENT_PS, sizeof PRESENT_PS), 0);
    R.present_hdr_pso = make_pso(code(PRESENT_VS, sizeof PRESENT_VS), code(PRESENT_PS, sizeof PRESENT_PS), -1);   /* (none: no HDR) */
    if (!R.scene_pso || !R.present_pso) return false;
    start_rt();
    g_rp_ok = g_fxp && g_fxp->particles_init && g_fxp->particles_init(R.dev);   /* (used only while fx_particles is on) */
    g_rd_ok = g_fxp && g_fxp->decals_init && g_fxp->decals_init(R.dev);         /* (fx_decals) */

    start_dlss(); /* always (when the card has it): the settings only say whether DLSS / DLSS 5 start on */
    R.on = R.dlss_ok && cfg->dlss, R.nr_on = cfg->dlss5, R.high_on = cfg->rt_high;
    set_dlss_text();
    const char *const *QN = DLSS_QN;
    const char *rt = R.rt && !R.rt_off ? "ray tracing (DXR) with bounced light" : "shadows through the tile grid";
    if (R.dlss_ok)
        snprintf(R.status, sizeof R.status, "%s, output %dx%d, %s, DLSS %s (render %dx%d), DLSS 5 %s", gpu, w, h, rt,
                 QN[MAX(0, MIN(R.cfg.dlss_quality, 4))], R.rw, R.rh, R.nr_ok ? (cfg->dlss5 ? "on" : "ready (off)") : "unavailable");
    else snprintf(R.status, sizeof R.status, "%s, %dx%d, %s, %s", gpu, w, h, rt, R.dlss_text);
    return true;
}

const char *render_status(void) { return R.status; }
const char *render_dlss_text(void) { return R.dlss_text; }
bool render_dlss_on(void) { return R.on; }
bool render_raytracing(void) { return R.rt && !R.rt_off; }
bool render_raytracing_available(void) { return R.rt; }
void render_set_raytracing(bool on) {
    R.rt_off = !on;
    R.have_prev = false; /* the probes start again */
}
void render_set_rt_high(bool on) {
    R.high_on = on;
    R.have_prev = false;
}
bool render_rt_high_wanted(void) { return R.high_on; }
bool render_rt_high_active(void) { return R.high_active; }
bool render_dlss_available(void) { return R.dlss_ok; }
bool render_dlss5_available(void) { return R.nr_ok; }
bool render_dlss5_on(void) { return R.nr_ok && R.nr_on; }
void render_set_dlss5_style(int style) { ngx_nr_set_style(style); }
bool render_set_dlss5(bool on) {
    R.nr_on = on;
    R.have_prev = false;
    set_dlss_text();
    return R.nr_ok && R.nr_on;
}
int render_dlss_quality(void) { return R.cfg.dlss_quality; }
void render_sizes(int *rw, int *rh, int *ow, int *oh) {
    bool dlss = R.on && R.dlss_ok;
    *rw = dlss ? R.rw : R.cfg.w, *rh = dlss ? R.rh : R.cfg.h, *ow = R.cfg.w, *oh = R.cfg.h;
}
void render_set_dlss5_params(float intensity, float structure) {
    R.cfg.nr_intensity = intensity, R.cfg.nr_structure = structure;
    if (R.nr_ok) ngx_nr_set_params(intensity, structure, R.cfg.nr_tone);
}
/* DLSS's mode during play. Every target is the output's size and the scene is drawn into its top-left render-size
 * part (render_init), so only the two NGX features know the render size: between frames (the GPU idle, the command
 * list open: render_frame's end) Super Resolution and Ray Reconstruction are let go, Super Resolution is made again
 * at the new size (Ray Reconstruction by the next frame that wants it), and the history starts again (one frame's
 * cut). When it cannot be made the old mode is made again. */
static int g_dlss_made;   /* (the checks: Super Resolution features made since the start) */
bool render_set_dlss_quality(int q) {
    int old = R.cfg.dlss_quality;
    q = MAX(0, MIN(q, 4));
    if (!R.dlss_ok) return false;
    if (q == old) return true;
    wait_gpu();
    ngx_rr_release();
    ngx_dlss_release();
    R.cfg.dlss_quality = q;
    render_size_for(q, &R.rw, &R.rh);
    bool ok = ngx_dlss_create(R.cl, R.rw, R.rh, R.cfg.w, R.cfg.h, DLSS_NGX_Q[q]);
    submit_and_wait();
    if (!ok) {
        R.cfg.dlss_quality = old;
        render_size_for(old, &R.rw, &R.rh);
        if (!ngx_dlss_create(R.cl, R.rw, R.rh, R.cfg.w, R.cfg.h, DLSS_NGX_Q[old])) R.dlss_ok = false, R.on = false;
        submit_and_wait();
        logf_("render: DLSS %s could not be made: %s again\n", DLSS_QN[q], DLSS_QN[old]);
    }
    g_dlss_made++;
    R.have_prev = false;
    R.high_active = false;
    set_dlss_text();
    logf_("render: DLSS mode %s: %dx%d -> %dx%d (made %d)\n", DLSS_QN[R.cfg.dlss_quality], R.rw, R.rh, R.cfg.w, R.cfg.h, g_dlss_made);
    return ok;
}
bool render_set_dlss(bool on) {
    R.on = on && R.dlss_ok;
    R.have_prev = false; /* the history starts again */
    set_dlss_text();
    return R.on;
}

/* --- a level ---------------------------------------------------------------------------------------- */
/* a 4-byte-a-texel texture from the CPU, its view in slot; up: the upload buffer, freed by the caller after
 * the copy has run */
static ID3D12Resource *upload_texture(int w, int h, DXGI_FORMAT fmt, const u8 *px, int slot, ID3D12Resource **up) {
    ID3D12Resource *tex = make_texture(w, h, fmt, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, NULL);
    if (!tex) return logf_("render: out of GPU memory\n"), NULL;
    D3D12_RESOURCE_DESC ad = ID3D12Resource_GetDesc(tex);
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp;
    UINT64 total = 0;
    ID3D12Device_GetCopyableFootprints(R.dev, &ad, 0, 1, 0, &fp, NULL, NULL, &total);
    *up = make_buffer((size_t)total, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!*up) return logf_("render: out of GPU memory\n"), NULL;
    u8 *ap = NULL;
    D3D12_RANGE none = {0, 0};
    ID3D12Resource_Map(*up, 0, &none, (void **)&ap);
    size_t bpt = fmt == DXGI_FORMAT_R8_UNORM ? 1 : 4;   /* bytes a texel: R8 (the occlusion maps) or RGBA8 */
    for (int y = 0; y < h; y++) memcpy(ap + fp.Offset + (size_t)y * fp.Footprint.RowPitch, px + (size_t)y * w * bpt, (size_t)w * bpt);
    ID3D12Resource_Unmap(*up, 0, NULL);
    D3D12_TEXTURE_COPY_LOCATION dst = {tex, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {.SubresourceIndex = 0}};
    D3D12_TEXTURE_COPY_LOCATION src = {*up, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, {.PlacedFootprint = fp}};
    ID3D12GraphicsCommandList_CopyTextureRegion(R.cl, &dst, 0, 0, 0, &src, NULL);
    barrier(tex, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    srv(tex, fmt, slot);
    return tex;
}

static bool cards_as_make(void);
static void release(ID3D12Resource **r) {
    if (*r) ID3D12Resource_Release(*r);
    *r = NULL;
}

/* the pictures' structures (ours): sized for the most cards, their bottom level not opaque (the shadow rays look at
 * each picture's alpha), a top level of two instances; nothing when they cannot be made (the level alone then) */
static bool cards_as_make(void) {
    if (!R.cpos) {
        D3D12_RANGE none0 = {0, 0};
        R.cpos = make_buffer(12 * (size_t)RENDER_MAX_SPRITE_VERTS, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        if (!R.cpos) return logf_("render: the pictures' shadows: out of GPU memory (the level's alone)\n"), false;
        ID3D12Resource_Map(R.cpos, 0, &none0, (void **)&R.cpos_ptr);
    }
    D3D12_RAYTRACING_GEOMETRY_DESC g;
    memset(&g, 0, sizeof g);
    g.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    g.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
    g.Triangles.VertexBuffer.StartAddress = ID3D12Resource_GetGPUVirtualAddress(R.cpos);
    g.Triangles.VertexBuffer.StrideInBytes = 12;
    g.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    g.Triangles.VertexCount = RENDER_MAX_SPRITE_VERTS / 3 * 3;
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bi;
    memset(&bi, 0, sizeof bi);
    bi.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    bi.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
    bi.NumDescs = 1, bi.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY, bi.pGeometryDescs = &g;
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bp, tp;
    ID3D12Device5_GetRaytracingAccelerationStructurePrebuildInfo(R.dev5, &bi, &bp);
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ti;
    memset(&ti, 0, sizeof ti);
    ti.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    ti.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
    ti.NumDescs = 2, ti.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    ID3D12Device5_GetRaytracingAccelerationStructurePrebuildInfo(R.dev5, &ti, &tp);
    R.cblas = make_buffer_ex((size_t)bp.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
                             D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    R.ctlas = make_buffer_ex((size_t)tp.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
                             D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    R.cscratch = make_buffer_ex((size_t)MAX(bp.ScratchDataSizeInBytes, tp.ScratchDataSizeInBytes), D3D12_HEAP_TYPE_DEFAULT,
                                D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    R.cinst = make_buffer(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * 2, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!R.cblas || !R.ctlas || !R.cscratch || !R.cinst) {
        release(&R.cblas), release(&R.ctlas), release(&R.cscratch), release(&R.cinst);
        return logf_("render: the pictures' shadows: out of GPU memory (the level's alone)\n"), false;
    }
    D3D12_RANGE none = {0, 0};
    ID3D12Resource_Map(R.cinst, 0, &none, (void **)&R.cinst_ptr);
    memset(R.cinst_ptr, 0, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * 2);
    for (int k = 0; k < 2; k++) {
        D3D12_RAYTRACING_INSTANCE_DESC *d = &R.cinst_ptr[k];
        d->Transform[0][0] = d->Transform[1][1] = d->Transform[2][2] = 1;
        d->InstanceID = (UINT)k, d->InstanceMask = k ? 0x02 : 0x01;
        d->AccelerationStructure = ID3D12Resource_GetGPUVirtualAddress(k ? R.cblas : R.blas);
    }
    return true;
}
/* each frame (ray tracing): the cards' bottom level from this frame's cards, the top level with the level and them */
static void cards_as_build(const float eye[3]) {
    if (!R.ctlas || !R.cinst_ptr || !R.cpos_ptr) return;
    int n = R.nsprites / 3 * 3;
    /* the corners: a card (kind 2.5-4.5) stands upright at its bottom middle facing the eye across the floor, its
       corner offset in nrm (right, up) as scene.hlsl's vs turns it; anything else (a door, its frame) is where it is */
    for (int i = 0; i < n; i++) {
        const Vertex *v = &R.sprite_ptr[i];
        float *p = R.cpos_ptr + (size_t)i * 3;
        if (v->color[3] > 2.5f && v->color[3] < 4.5f) {
            float dx = eye[0] - v->pos[0] + 1e-4f, dz = eye[2] - v->pos[2], l = sqrtf(dx * dx + dz * dz);
            if (l < 1e-6f) l = 1e-6f;
            dx /= l, dz /= l;
            p[0] = v->pos[0] - dz * v->nrm[0], p[1] = v->pos[1] + v->nrm[1], p[2] = v->pos[2] + dx * v->nrm[0];
        } else
            p[0] = v->pos[0], p[1] = v->pos[1], p[2] = v->pos[2];
    }
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC b;
    memset(&b, 0, sizeof b);
    if (n >= 3) {
        D3D12_RAYTRACING_GEOMETRY_DESC g;
        memset(&g, 0, sizeof g);
        g.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        g.Triangles.VertexBuffer.StartAddress = ID3D12Resource_GetGPUVirtualAddress(R.cpos);
        g.Triangles.VertexBuffer.StrideInBytes = 12;
        g.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        g.Triangles.VertexCount = (UINT)n;
        b.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        b.Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
        b.Inputs.NumDescs = 1, b.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY, b.Inputs.pGeometryDescs = &g;
        b.DestAccelerationStructureData = ID3D12Resource_GetGPUVirtualAddress(R.cblas);
        b.ScratchAccelerationStructureData = ID3D12Resource_GetGPUVirtualAddress(R.cscratch);
        ID3D12GraphicsCommandList4_BuildRaytracingAccelerationStructure(R.cl4, &b, 0, NULL);
        uav_barrier(R.cblas);
        uav_barrier(R.cscratch);
    }
    memset(&b, 0, sizeof b);
    b.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    b.Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
    b.Inputs.NumDescs = n >= 3 ? 2 : 1, b.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    b.Inputs.InstanceDescs = ID3D12Resource_GetGPUVirtualAddress(R.cinst);
    b.DestAccelerationStructureData = ID3D12Resource_GetGPUVirtualAddress(R.ctlas);
    b.ScratchAccelerationStructureData = ID3D12Resource_GetGPUVirtualAddress(R.cscratch);
    ID3D12GraphicsCommandList4_BuildRaytracingAccelerationStructure(R.cl4, &b, 0, NULL);
    uav_barrier(R.ctlas);
    uav_barrier(R.cscratch);
}
/* the top level the frame's rays use: the level and the cards when they could be made, else the level's own */
static bool g_card_shadows = true;   /* the objects' shadows (render_set_object_shadows: '.', the menu) */
void render_set_object_shadows(bool on) { g_card_shadows = on; }
bool render_object_shadows(void) { return g_card_shadows; }
static D3D12_GPU_VIRTUAL_ADDRESS scene_tlas(void)
{
    static int off = -1;   /* UWVIEW_CARDSHADOW=0 (a check's): the level's alone */
    if (off < 0) { const char *e = getenv("UWVIEW_CARDSHADOW"); off = e && *e == '0'; }
    return ID3D12Resource_GetGPUVirtualAddress(R.ctlas && R.cinst_ptr && !off && g_card_shadows ? R.ctlas : R.tlas);
}

/* the level's acceleration structures: one bottom level of all its triangles, one top level with it once */
static bool build_as(int vertices, ID3D12Resource **scratch_out, ID3D12Resource **inst_out) {
    D3D12_RAYTRACING_GEOMETRY_DESC g;
    memset(&g, 0, sizeof g);
    g.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    g.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
    g.Triangles.VertexBuffer.StartAddress = ID3D12Resource_GetGPUVirtualAddress(R.vbuf);
    g.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
    g.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    g.Triangles.VertexCount = (UINT)vertices;
    g.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
    g.Triangles.IndexCount = (UINT)R.indices;
    g.Triangles.IndexBuffer = ID3D12Resource_GetGPUVirtualAddress(R.ibuf);
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bi;
    memset(&bi, 0, sizeof bi);
    bi.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    bi.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    bi.NumDescs = 1, bi.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY, bi.pGeometryDescs = &g;
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bp, tp;
    ID3D12Device5_GetRaytracingAccelerationStructurePrebuildInfo(R.dev5, &bi, &bp);

    D3D12_RAYTRACING_INSTANCE_DESC inst;
    memset(&inst, 0, sizeof inst);
    inst.Transform[0][0] = inst.Transform[1][1] = inst.Transform[2][2] = 1;
    inst.InstanceMask = 0xFF;
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ti;
    memset(&ti, 0, sizeof ti);
    ti.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    ti.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    ti.NumDescs = 1, ti.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    ID3D12Device5_GetRaytracingAccelerationStructurePrebuildInfo(R.dev5, &ti, &tp);

    size_t scratch = (size_t)MAX(bp.ScratchDataSizeInBytes, tp.ScratchDataSizeInBytes);
    R.blas = make_buffer_ex((size_t)bp.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
                            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    R.tlas = make_buffer_ex((size_t)tp.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
                            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    *scratch_out = make_buffer_ex(scratch, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    *inst_out = make_buffer(sizeof inst, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!R.blas || !R.tlas || !*scratch_out || !*inst_out) return logf_("render: out of GPU memory (ray tracing)\n"), false;
    inst.AccelerationStructure = ID3D12Resource_GetGPUVirtualAddress(R.blas);
    void *ip = NULL;
    D3D12_RANGE none = {0, 0};
    ID3D12Resource_Map(*inst_out, 0, &none, &ip);
    memcpy(ip, &inst, sizeof inst);
    ID3D12Resource_Unmap(*inst_out, 0, NULL);
    ti.InstanceDescs = ID3D12Resource_GetGPUVirtualAddress(*inst_out);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC b;
    memset(&b, 0, sizeof b);
    b.DestAccelerationStructureData = ID3D12Resource_GetGPUVirtualAddress(R.blas);
    b.Inputs = bi;
    b.ScratchAccelerationStructureData = ID3D12Resource_GetGPUVirtualAddress(*scratch_out);
    ID3D12GraphicsCommandList4_BuildRaytracingAccelerationStructure(R.cl4, &b, 0, NULL);
    uav_barrier(R.blas);
    uav_barrier(*scratch_out);
    b.DestAccelerationStructureData = ID3D12Resource_GetGPUVirtualAddress(R.tlas);
    b.Inputs = ti;
    ID3D12GraphicsCommandList4_BuildRaytracingAccelerationStructure(R.cl4, &b, 0, NULL);
    uav_barrier(R.tlas);
    cards_as_make();
    return true;
}

bool render_upload(const Mesh *m) {
    wait_gpu();
    release(&R.vbuf), release(&R.ibuf), release(&R.atlas), release(&R.natlas), release(&R.snatlas), release(&R.saoatlas), release(&R.grid), release(&R.blas), release(&R.tlas);
    if (R.cinst && R.cinst_ptr) ID3D12Resource_Unmap(R.cinst, 0, NULL);
    R.cinst_ptr = NULL;
    release(&R.cblas), release(&R.ctlas), release(&R.cscratch), release(&R.cinst);
    R.vertices = m->vertices;
    size_t vsize = sizeof(Vertex) * (size_t)MAX(m->vertices, 3);
    ID3D12Resource *up = make_buffer(vsize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    R.vbuf = make_buffer(vsize, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);
    if (!up || !R.vbuf) return logf_("render: out of GPU memory\n"), false;
    void *p = NULL;
    D3D12_RANGE none = {0, 0};
    ID3D12Resource_Map(up, 0, &none, &p);
    memcpy(p, m->vert, sizeof(Vertex) * (size_t)m->vertices);
    ID3D12Resource_Unmap(up, 0, NULL);
    ID3D12GraphicsCommandList_CopyBufferRegion(R.cl, R.vbuf, 0, up, 0, vsize);
    /* drawn from, and read by the ray tracing (the build, and the probes' hit surfaces) */
    barrier(R.vbuf, D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    /* the triangles' vertex numbers */
    R.indices = m->indices;
    size_t isize = sizeof(u32) * (size_t)MAX(m->indices, 3);
    ID3D12Resource *iup = make_buffer(isize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    R.ibuf = make_buffer(isize, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST);
    if (!iup || !R.ibuf) return logf_("render: out of GPU memory (indices)\n"), false;
    ID3D12Resource_Map(iup, 0, &none, &p);
    memcpy(p, m->index, sizeof(u32) * (size_t)m->indices);
    ID3D12Resource_Unmap(iup, 0, NULL);
    ID3D12GraphicsCommandList_CopyBufferRegion(R.cl, R.ibuf, 0, iup, 0, isize);
    barrier(R.ibuf, D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_INDEX_BUFFER | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    ID3D12Resource *ups[6] = {0}, *scratch = NULL, *inst = NULL, *zero = NULL;
    static const u8 white[4] = {255, 255, 255, 255};
    if (!R.hatlas) R.hatlas = upload_texture(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, white, SLOT_HATLAS, &ups[5]);   /* off */
    R.atlas = upload_texture(ATLAS_SIZE, ATLAS_H, DXGI_FORMAT_R8G8B8A8_UNORM, m->atlas, SLOT_ATLAS, &ups[0]);
    R.natlas = upload_texture(NATLAS_SIZE, NATLAS_SIZE, DXGI_FORMAT_R8G8B8A8_UNORM, m->natlas, SLOT_NATLAS, &ups[1]);
    R.snatlas = upload_texture(ATLAS_SIZE, ATLAS_H, DXGI_FORMAT_R8G8B8A8_UNORM, m->snatlas, SLOT_SNATLAS, &ups[3]);
    R.saoatlas = upload_texture(ATLAS_SIZE, ATLAS_H, DXGI_FORMAT_R8_UNORM, m->saoatlas, SLOT_SAOATLAS, &ups[4]);
    R.grid = upload_texture(64, 64, DXGI_FORMAT_R8G8B8A8_UINT, m->grid, SLOT_GRID, &ups[2]);
    bool ok = R.atlas && R.natlas && R.snatlas && R.saoatlas && R.grid;
    if (R.rt && ok) {
        ok = build_as(m->vertices, &scratch, &inst);
        /* the probes start dark */
        zero = make_buffer(PROBE_BYTES, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        if (zero) {
            void *zp = NULL;
            ID3D12Resource_Map(zero, 0, &none, &zp);
            memset(zp, 0, PROBE_BYTES);
            ID3D12Resource_Unmap(zero, 0, NULL);
            for (int i = 0; i < 2; i++) {
                barrier(R.probes[i], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                        D3D12_RESOURCE_STATE_COPY_DEST);
                ID3D12GraphicsCommandList_CopyBufferRegion(R.cl, R.probes[i], 0, zero, 0, PROBE_BYTES);
                barrier(R.probes[i], D3D12_RESOURCE_STATE_COPY_DEST,
                        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            }
        }
    }
    submit_and_wait();
    release(&up), release(&iup), release(&scratch), release(&inst), release(&zero);
    for (int i = 0; i < 6; i++) release(&ups[i]);
    R.have_prev = false;
    return ok;
}

bool render_set_hires(const u8 *rgba, int size, int factor, bool maps, bool colours) {
    static const u8 white[4] = {255, 255, 255, 255};
    if (!R.cl) return false;
    if (!rgba || factor <= 0 || size <= 0) rgba = white, size = 1, factor = 0, maps = false;
    wait_gpu();
    ID3D12Resource *up = NULL;
    ID3D12Resource *tex = upload_texture(maps ? size * 2 : size, size, DXGI_FORMAT_R8G8B8A8_UNORM, rgba, SLOT_HATLAS, &up);
    submit_and_wait();
    release(&up);
    if (!tex) return false;   /* (the old one's view stays in the slot) */
    release(&R.hatlas);
    R.hatlas = tex, R.hatlas_f = factor, R.hatlas_maps = maps, R.hatlas_colours = colours;
    R.have_prev = false;
    return true;
}

void render_set_lights(const Light *l, int n, const u32 *tiles) {
    R.nplaced = MAX(0, MIN(n, MAX_LIGHTS));
    memcpy(R.placed, l, sizeof(Light) * (size_t)R.nplaced);
    for (int i = 0; i < R.nplaced; i++) {
        GpuLight *g = &R.lights_ptr[1 + i];
        memcpy(g->pr, l[i].pos, 12), g->pr[3] = l[i].range * RENDER_LIGHT_REACH;
        for (int j = 0; j < 3; j++) g->col[j] = l[i].color[j] * l[i].intensity;
        g->col[3] = 1;
        g->info[0] = 0;
    }
    u32 *tp = NULL;
    D3D12_RANGE none = {0, 0};
    ID3D12Resource_Map(R.tiles_buf, 0, &none, (void **)&tp);
    /* the tile lists name placed lights from 0; in the buffer they follow the torch */
    for (int i = 0; i < 64 * 64 * TILE_LIGHTS; i++) tp[i] = tiles[i] == NO_LIGHT ? NO_LIGHT : tiles[i] + 1;
    ID3D12Resource_Unmap(R.tiles_buf, 0, NULL);
}

static ID3D12Resource *g_atlas_up[2];
static int g_atlas_rows, g_atlas_pending, g_atlas_flip;
/* the atlas's first rows (the walls' and floors' textures) rewritten from rgba (ATLAS_SIZE wide), at the next frame */
void render_update_atlas(const u8 *rgba, int rows) {
    if (!R.atlas || rows <= 0 || rows > 1024) return;
    int k = g_atlas_flip ^= 1;
    if (!g_atlas_up[k]) g_atlas_up[k] = make_buffer((size_t)ATLAS_SIZE * 4 * 1024, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!g_atlas_up[k]) return;
    u8 *p = NULL;
    D3D12_RANGE none = {0, 0};
    if (FAILED(ID3D12Resource_Map(g_atlas_up[k], 0, &none, (void **)&p)) || !p) return;
    memcpy(p, rgba, (size_t)ATLAS_SIZE * 4 * (size_t)rows);
    ID3D12Resource_Unmap(g_atlas_up[k], 0, NULL);
    g_atlas_rows = rows, g_atlas_pending = k;
}

/* placed light k moved to pos (a light that travels; its tile lists stay as render_set_lights made them) */
void render_set_light_pos(int k, const float pos[3]) {
    if (k < 0 || k >= R.nplaced) return;
    memcpy(R.placed[k].pos, pos, 12);
    memcpy(R.lights_ptr[1 + k].pr, pos, 12);
}

void render_ui_size(int *w, int *h, int *scale) { *w = R.ui_w, *h = R.ui_h, *scale = R.ui_s; }
void render_set_ui(const u8 *rgba) {
    if (!R.ui_up) return;
    if (!R.ui_ptr) {
        D3D12_RANGE none = {0, 0};
        ID3D12Resource_Map(R.ui_up, 0, &none, (void **)&R.ui_ptr);
    }
    for (int y = 0; y < R.ui_h; y++) memcpy(R.ui_ptr + R.ui_fp.Offset + (size_t)y * R.ui_fp.Footprint.RowPitch, rgba + (size_t)y * R.ui_w * 4, (size_t)R.ui_w * 4);
    R.ui_dirty = true;
}

void render_set_sprites(const Vertex *v, int n) {
    R.nsprites = MAX(0, MIN(n, RENDER_MAX_SPRITE_VERTS));
    memcpy(R.sprite_ptr, v, sizeof(Vertex) * (size_t)R.nsprites);
}

void render_set_torch(bool on, const float pos[3], const float color[3], float intensity, float range) {
    GpuLight *g = &R.lights_ptr[0];
    memcpy(g->pr, pos, 12), g->pr[3] = range;
    for (int j = 0; j < 3; j++) g->col[j] = color[j] * intensity;
    g->col[3] = on ? 1.0f : 0.0f;
}
/* the player's light as the original's: by the distance to the eye, the light level's shades (common.hlsli
 * light_term); exponent: the output's (2.2 x the gamma), so the screen shows LIGHT.DAT's brightness */
void render_set_torch_original(bool on, const float pos[3], const float eye[3], int level, float range, float exponent) {
    GpuLight *g = &R.lights_ptr[0];
    memcpy(g->pr, pos, 12), g->pr[3] = range;   /* (the shader reads pr.w as the flame's flicker, a multiplier: uwview.c) */
    memcpy(g->col, eye, 12);
    g->col[3] = on ? 2.0f + (float)(level & 7) + exponent / 10.0f : 0.0f;
}

/* --- matrices (row-major, used as mul(M, v)) --------------------------------------------------- */
static void mat_mul(const float *a, const float *b, float *o) {
    float t[16];
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) t[r * 4 + c] = a[r * 4] * b[c] + a[r * 4 + 1] * b[4 + c] + a[r * 4 + 2] * b[8 + c] + a[r * 4 + 3] * b[12 + c];
    memcpy(o, t, sizeof t);
}

/* world: x east, y up, z north. view: right, up, forward rows; depth = distance along forward.
 * jx, jy: a shift of the whole picture in clip space (the DLSS jitter) */
/* the part of the output the camera's view fills (fractions of it, from the top left): the whole by default; the
 * original game's port puts the view in its 3D window, the interface layer covering the rest */
static float g_vr[4] = {0, 0, 1, 1};
/* INSTANCE.ASM's scale_matrix: the original's view depth scaled by its zoom / 8000h before SMOOTH.ASM measures a
 * corner's distance for its shade (0.76 at the usual zoom); 1 until the game says */
static float g_shade_k = 0.7629f;
void render_set_shade_scale(float k) { if (k > 0.1f && k < 4) g_shade_k = k; }
/* full bright (Shift+L, the user): every surface lit fully, at any distance; the shader is told by rtlim.w < 0 */
static bool g_fullbright;
void render_set_fullbright(bool on) { g_fullbright = on; }
/* the lights' fade by distance (common.hlsli light_fade; the user, 2026-11-16): 50% at 2 tiles, 30% at 3, gone at 4,
   for every light; off: the original's shading by distance and the inverse square, as before */
static bool g_light_fade = true;
void render_set_light_fade(bool on) { g_light_fade = on; }
bool render_light_fade(void) { return g_light_fade; }

/* A flame's colour from its temperature (the physics: its light is mostly soot glowing as a black body): Planck's
 * law through the CIE 1931 functions to XYZ; the eye's adaptation by a chromatic adaptation transform (CAT16, CAM16's,
 * the default; or Bradford), a degree D of the way from a wood fire's white (1800 K) to D65's; then linear sRGB, a
 * colour outside it brought in by lowering its OKLCh chroma (its lightness and hue kept); unit luminance.
 * D: CAM16's D = F (1 - exp((-La - 42) / 92) / 3.6) for a torch-lit dungeon (La about 5 cd/m2, a dim surround F 0.9:
 * 0.75), or the settings' fixed share. Until 2026-11-15 the negative sRGB blue was clipped to 0 first and the
 * adaptation done channel by channel after: firelight had no blue at any adaptation (removed, the user). */
static float g_adapt = -1.0f;   /* the eye's adaptation, 0..1; < 0 automatic (settings light_adaptation, "auto") */
static int g_cat;                /* 0 CAT16, 1 Bradford (settings light_colour) */
void render_set_adaptation(float a) { g_adapt = a < 0 ? -1.0f : a > 1 ? 1 : a; bb_ready = false; }
float render_adaptation(void) { return g_adapt; }
void render_set_light_cat(int cat) { g_cat = cat ? 1 : 0; bb_ready = false; }
int render_light_cat(void) { return g_cat; }
static double cam16_degree(double La, double F) {
    double d = F * (1.0 - exp((-La - 42.0) / 92.0) / 3.6);
    return d < 0 ? 0 : d > 1 ? 1 : d;
}
float render_adaptation_auto(void) { return (float)cam16_degree(5.0, 0.9); }
static float g3(float x, float mu, float s1, float s2) { float t = (x - mu) / (x < mu ? s1 : s2); return expf(-0.5f * t * t); }
/* the black body at kelvin as CIE XYZ, scaled to Y = 1 (Planck's law through Wyman, Sloan & Shirley's fit) */
static void bb_xyz(double kelvin, double out[3])
{
    double X = 0, Y = 0, Z = 0;
    for (int nm = 380; nm <= 780; nm += 5) {
        float l = (float)nm;
        double lm = nm * 1e-9, b = 1.0 / (lm * lm * lm * lm * lm * (exp(0.0143877735 / (lm * kelvin)) - 1.0));
        X += b * (1.056 * g3(l, 599.8f, 37.9f, 31.0f) + 0.362 * g3(l, 442.0f, 16.0f, 26.7f) - 0.065 * g3(l, 501.1f, 20.4f, 26.2f));
        Y += b * (0.821 * g3(l, 568.8f, 46.9f, 40.5f) + 0.286 * g3(l, 530.9f, 16.3f, 31.1f));
        Z += b * (1.217 * g3(l, 437.0f, 11.8f, 36.0f) + 0.681 * g3(l, 459.0f, 26.0f, 13.8f));
    }
    out[0] = X / Y, out[1] = 1.0, out[2] = Z / Y;
}
static void mat3_inv(const double m[3][3], double o[3][3])
{
    double det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                 m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            int r0 = (j + 1) % 3, r1 = (j + 2) % 3, c0 = (i + 1) % 3, c1 = (i + 2) % 3;   /* the cofactor, transposed */
            o[i][j] = (m[r0][c0] * m[r1][c1] - m[r0][c1] * m[r1][c0]) / det;
        }
}
/* OKLab (Ottosson 2020) from linear sRGB and back */
static void oklab_from(const double c[3], double o[3])
{
    double l = 0.4122214708 * c[0] + 0.5363325363 * c[1] + 0.0514459929 * c[2];
    double m = 0.2119034982 * c[0] + 0.6806995451 * c[1] + 0.1073969566 * c[2];
    double s = 0.0883024619 * c[0] + 0.2817188376 * c[1] + 0.6299787005 * c[2];
    l = cbrt(l), m = cbrt(m), s = cbrt(s);
    o[0] = 0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s;
    o[1] = 1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s;
    o[2] = 0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s;
}
static void oklab_to(double L, double A, double B, double c[3])
{
    double l = L + 0.3963377774 * A + 0.2158037573 * B, m = L - 0.1055613458 * A - 0.0638541728 * B,
           s = L - 0.0894841775 * A - 1.2914855480 * B;
    l = l * l * l, m = m * m * m, s = s * s * s;
    c[0] = 4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s;
    c[1] = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s;
    c[2] = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s;
}
static void blackbody_adapted(float kelvin, float rgb[3])
{
    static const double MS[2][3][3] = {
        {{0.401288, 0.650173, -0.051461}, {-0.250268, 1.204414, 0.045854}, {-0.002079, 0.048952, 0.953127}},   /* CAT16 */
        {{0.8951, 0.2664, -0.1614}, {-0.7502, 1.7135, 0.0367}, {0.0389, -0.0685, 1.0296}}};                   /* Bradford */
    static const double D65[3] = {0.95047, 1.0, 1.08883};
    static double MI[2][3][3], wsrc[2][3], wdst[2][3];
    static bool ready;
    const double (*M)[3] = MS[g_cat];
    double c[3], l[3], x[3], D = g_adapt < 0 ? cam16_degree(5.0, 0.9) : g_adapt;
    if (!ready) {
        double w[3];
        bb_xyz(1800.0, w);
        for (int k = 0; k < 2; k++) {
            mat3_inv(MS[k], MI[k]);
            for (int i = 0; i < 3; i++) wsrc[k][i] = MS[k][i][0] * w[0] + MS[k][i][1] * w[1] + MS[k][i][2] * w[2],
                                        wdst[k][i] = MS[k][i][0] * D65[0] + MS[k][i][1] * D65[1] + MS[k][i][2] * D65[2];
        }
        ready = true;
    }
    bb_xyz(kelvin, c);
    for (int i = 0; i < 3; i++) l[i] = (M[i][0] * c[0] + M[i][1] * c[1] + M[i][2] * c[2]) * pow(wdst[g_cat][i] / wsrc[g_cat][i], D);
    for (int i = 0; i < 3; i++) x[i] = MI[g_cat][i][0] * l[0] + MI[g_cat][i][1] * l[1] + MI[g_cat][i][2] * l[2];
    double v[3] = {3.2406 * x[0] - 1.5372 * x[1] - 0.4986 * x[2], -0.9689 * x[0] + 1.8758 * x[1] + 0.0415 * x[2],
                   0.0557 * x[0] - 0.2040 * x[1] + 1.0570 * x[2]};
    if (v[0] < 0 || v[1] < 0 || v[2] < 0) {   /* outside sRGB: the most chroma (OKLCh) that is inside, hue and lightness kept */
        double lab[3], t[3], lo = 0, hi = 1;
        oklab_from(v, lab);
        for (int it = 0; it < 30; it++) {
            double k = 0.5 * (lo + hi);
            oklab_to(lab[0], lab[1] * k, lab[2] * k, t);
            if (t[0] >= 0 && t[1] >= 0 && t[2] >= 0) lo = k; else hi = k;
        }
        oklab_to(lab[0], lab[1] * lo, lab[2] * lo, v);
        for (int i = 0; i < 3; i++) if (v[i] < 0) v[i] = 0;   /* (rounding) */
    }
    double y = 0.2126 * v[0] + 0.7152 * v[1] + 0.0722 * v[2];
    if (y <= 0) { rgb[0] = rgb[1] = rgb[2] = 1; return; }
    rgb[0] = (float)(v[0] / y), rgb[1] = (float)(v[1] / y), rgb[2] = (float)(v[2] / y);
}
void render_blackbody(float kelvin, float rgb[3]) { blackbody_adapted(kelvin, rgb); }
static float g_torch_rgb[3] = {1, 1, 1};
void render_set_torch_colour(const float rgb[3]) { memcpy(g_torch_rgb, rgb, 12); }
static bool g_torch_steady;
void render_set_torch_steady(bool on) { g_torch_steady = on; }
bool render_fullbright(void) { return g_fullbright; }
/* the lens as the tangents of the half angles across and up (0: the camera's fov_y and the rectangle's shape) */
static float g_lens[2];
static int g_debug; /* the checks: 1 the shading normals as colours */
/* the scene's tint (render_set_tint), and the atlas's top rows to rewrite (render_update_atlas: two upload buffers
 * in turn, the copy made at the next frame's start) */
static float g_tint[4];
void render_set_tint(float r, float g, float b, bool on) { g_tint[0] = r, g_tint[1] = g, g_tint[2] = b, g_tint[3] = on ? 1.0f : 0.0f; }
static int g_atlas_pending = -1;
void render_set_debug(int what) { g_debug = what; }
void render_set_lens(float tan_x, float tan_y) { g_lens[0] = tan_x, g_lens[1] = tan_y; }
void render_set_view_rect(float x0, float y0, float x1, float y1) {
    if (x1 - x0 < 0.01f || y1 - y0 < 0.01f) x0 = y0 = 0, x1 = y1 = 1;
    g_vr[0] = x0, g_vr[1] = y0, g_vr[2] = x1, g_vr[3] = y1;
}
static void view_proj(const Camera *cam, int w, int h, float jx, float jy, float *out, float *view_out, float *proj_out) {
    float cy = cosf(cam->yaw), sy = sinf(cam->yaw), cp = cosf(cam->pitch), sp = sinf(cam->pitch);
    float f[3] = {sy * cp, sp, cy * cp};
    float r[3] = {cy, 0, -sy};
    float u[3] = {f[1] * r[2] - f[2] * r[1], f[2] * r[0] - f[0] * r[2], f[0] * r[1] - f[1] * r[0]};
    if (cam->roll != 0) { /* rolled about the view direction */
        float c = cosf(cam->roll), s = sinf(cam->roll), r2[3], u2[3];
        for (int k = 0; k < 3; k++) r2[k] = r[k] * c + u[k] * s, u2[k] = u[k] * c - r[k] * s;
        memcpy(r, r2, sizeof r2), memcpy(u, u2, sizeof u2);
    }
    const float *p = cam->pos;
    float v[16] = {r[0], r[1], r[2], -(r[0] * p[0] + r[1] * p[1] + r[2] * p[2]),
                   u[0], u[1], u[2], -(u[0] * p[0] + u[1] * p[1] + u[2] * p[2]),
                   f[0], f[1], f[2], -(f[0] * p[0] + f[1] * p[1] + f[2] * p[2]),
                   0, 0, 0, 1};
    /* the view's rectangle: the frustum fitted to it (its shape), its centre moved there */
    float rw = g_vr[2] - g_vr[0], rh = g_vr[3] - g_vr[1], cx = g_vr[0] + g_vr[2] - 1.0f, cyc = 1.0f - (g_vr[1] + g_vr[3]);
    float aspect = (float)w * rw / ((float)h * rh), n = 0.02f, fa = 200.0f;
    float ys = 1.0f / tanf(cam->fov_y * 0.5f), xs = ys / aspect;
    if (g_lens[0] > 0 && g_lens[1] > 0) xs = 1.0f / g_lens[0], ys = 1.0f / g_lens[1];
    float pr[16] = {xs * rw, 0, jx + cx, 0, 0, ys * rh, jy + cyc, 0, 0, 0, fa / (fa - n), -n * fa / (fa - n), 0, 0, 1, 0};
    mat_mul(pr, v, out);
    if (view_out) memcpy(view_out, v, sizeof v);
    if (proj_out) memcpy(proj_out, pr, sizeof pr);
}
static float fwd_of(const Camera *cam, int k) {
    return k == 0 ? sinf(cam->yaw) * cosf(cam->pitch) : k == 1 ? sinf(cam->pitch) : cosf(cam->yaw) * cosf(cam->pitch);
}
static void transpose(const float *a, float *o) {
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) o[c * 4 + r] = a[r * 4 + c];
}

static float halton(int i, int base) {
    float f = 1, r = 0;
    for (; i > 0; i /= base) {
        f /= (float)base;
        r += f * (float)(i % base);
    }
    return r;
}

bool render_frame(const Camera *cam, const Look *look, bool reset) {
    bool dlss = R.on && R.dlss_ok;
    int rw = dlss ? R.rw : R.cfg.w, rh = dlss ? R.rh : R.cfg.h;
    reset = reset || !R.have_prev;
    float jx = 0, jy = 0;
    if (dlss) {
        /* the jitter's phases: 8 * (output / render)^2 as NVIDIA asks, 16 at least (what every mode had; DLAA's still) */
        int phases = MAX(16, (int)(8.0f * (float)R.cfg.w * (float)R.cfg.w / ((float)rw * (float)rw) + 0.5f));
        static int jh = -1;   /* UWVIEW_JITTER_HOLD=1 (the checks only: tools\vkcompare.py): one jitter phase, every run alike */
        if (jh < 0) { const char *e = getenv("UWVIEW_JITTER_HOLD"); jh = e && *e && *e != '0'; }
        int ph = (int)((jh ? 0u : R.frame) % (unsigned)phases) + 1;
        jx = halton(ph, 2) - 0.5f;
        jy = halton(ph, 3) - 0.5f;
    }
    Constants *c = R.cb;
    float view[16], proj[16];
    view_proj(cam, rw, rh, 0, 0, c->vp_nojit, view, proj);
    view_proj(cam, rw, rh, jx * 2.0f / (float)rw, -jy * 2.0f / (float)rh, c->vp, NULL, NULL);
    /* ray tracing High: Ray Reconstruction (made the first time it is wanted) in DLSS upscaling's place */
    bool high = false;
    bool rt_live = R.rt && R.tlas && !R.rt_off;
    if (dlss && rt_live && R.high_pso && R.high_on) {
        if (!ngx_rr_ready() && ngx_rr_create(R.cl, R.rw, R.rh, R.cfg.w, R.cfg.h, DLSS_NGX_Q[MAX(0, MIN(R.cfg.dlss_quality, 4))])) reset = true;
        high = ngx_rr_ready();
    }
    if (high != R.high_active) reset = true;
    R.high_active = high;
    transpose(view, R.w2v), transpose(proj, R.v2c);
    if (reset) memcpy(R.prev_vp, c->vp_nojit, 64);
    memcpy(c->vp_prev, R.prev_vp, 64);
    if (!g_hold) g_hold_frame = R.frame;
    memcpy(c->eye, cam->pos, 12), c->eye[3] = (float)g_hold_frame;
    /* the light's size for soft shadows; bounced light; how much of the probes' old light they keep */
    c->look[0] = 0.06f, c->look[1] = look->bounce, c->look[2] = reset ? 0.0f : 0.93f, c->look[3] = look->sheen;
    c->size[0] = (float)rw, c->size[1] = (float)rh, c->size[2] = R.cfg.exposure;
    float gamma = look->gamma > 0.1f ? look->gamma : 1.0f;
    c->post[0] = 1.0f / R.cfg.exposure, c->post[1] = 1.0f, c->post[2] = 1.0f / (2.2f * gamma), c->post[3] = (float)R.ui_s;
    hdr_follow();   /* HDR output (render_set_hdr): the swap chain's format follows the setting and the monitor */
    c->hdr[0] = R.hdr_on ? 1.0f : 0.0f, c->hdr[1] = g_hdr_paper / 80.0f;
    c->hdr[2] = render_hdr_peak() / g_hdr_paper, c->hdr[3] = g_light_fade ? 1.0f : 0.0f;   /* (w: the lights' fade, light_fade) */
    if (c->hdr[2] < 1.0f) c->hdr[2] = 1.0f;
    /* the burning torches flicker, each in its own way */
    float secs = g_hold ? g_hold_secs : (float)(GetTickCount64() % 100000000ull) / 1000.0f;
    for (int k = 0; k < R.nplaced; k++) {
        const Light *L = &R.placed[k];
        if (L->kind != LIGHT_WALL_TORCH) continue;
        /* a flame's flicker (the physics of fire, the user's): it puffs at its buoyant frequency, about 1.5 / sqrt(width)
           a second (a torch 5 cm across about 7 a second, a campfire 50 cm about 2), with the turbulence's faster wisps
           over it; the bigger the flame the deeper (no wind down here) */
        float D = L->flame > 0.005f ? L->flame : 0.05f, fp = 1.5f / sqrtf(D) * 6.2832f, depth = 0.05f + 0.25f * sqrtf(D);
        float ph = k * 2.39996f;
        float f = 1.0f + depth * (0.55f * sinf(secs * fp + ph) + 0.25f * sinf(secs * fp * 1.73f + ph * 3.1f) +
                                  0.12f * sinf(secs * fp * 3.17f + ph * 1.7f) + 0.08f * sinf(secs * fp * 5.9f + ph * 5.3f));
        GpuLight *gl = &R.lights_ptr[1 + k];
        memcpy(gl->pr, L->pos, 12);
        if (L->kelvin > 0 && L->flame > 0.011f) {   /* a flame: its puffs (render_flame), its heat from its width */
            static const float still[2] = {0, 0};
            float Q = 1.5f * powf(L->flame / 0.05f, 1.4f);
            render_flame(gl, L->pos, L->flame, Q, L->kelvin + 150.0f, (float)k * 0.6180339f, still, secs, L->intensity);
        } else if (L->kelvin > 0) {   /* a spark: a point, its colour from its temperature */
            float rgb[3];
            bb_at(L->kelvin + (f - 1.0f) * 900.0f, rgb);
            for (int j = 0; j < 3; j++) gl->col[j] = rgb[j] * L->intensity * f;
            gl->info[0] = 0;
        } else {
            for (int j = 0; j < 3; j++) gl->col[j] = L->color[j] * L->intensity * f;
            gl->info[0] = 0;
        }
        (void)ph;
    }
    {   /* the player's torch: a pitch torch's flame, 5 cm across, 1.5 kW, 1750 K at its hottest */
        GpuLight *g0 = &R.lights_ptr[0];
        if (g0->col[3] > 1.5f && !g_fullbright && !g_torch_steady) {
            float eye_keep[3];   /* (light 0's col.xyz is the eye, the original's falloff measures from it: kept) */
            memcpy(eye_keep, g0->col, 12);
            render_flame(g0, g0->pr, 0.05f, 1.5f, 1750.0f, 0.37f, g_torch_vel, secs, 1.0f);
            memcpy(g0->col, eye_keep, 12);
        }
        else g0->info[0] = 0;
    }
    /* the orbs: the placed lights in front of the camera and within 30 tiles, nearest first (16 at most) */
    int no = 0;
    float od[16];
    float fwd[3] = {sinf(cam->yaw) * cosf(cam->pitch), sinf(cam->pitch), cosf(cam->yaw) * cosf(cam->pitch)};
    for (int k = 0; k < R.nplaced; k++) {
        const Light *L = &R.placed[k];
        float d[3] = {L->pos[0] - cam->pos[0], L->pos[1] - cam->pos[1], L->pos[2] - cam->pos[2]};
        float dd = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
        if (L->kind != LIGHT_ORB || dd > 900 || d[0] * fwd[0] + d[1] * fwd[1] + d[2] * fwd[2] < -0.2f) continue;
        int at = no < 16 ? no++ : 15;
        if (at == 15 && no == 16 && dd >= od[15]) continue;
        while (at > 0 && od[at - 1] > dd) {
            od[at] = od[at - 1];
            memcpy(c->lpos[at], c->lpos[at - 1], 16), memcpy(c->lcol[at], c->lcol[at - 1], 16);
            at--;
        }
        od[at] = dd;
        memcpy(c->lpos[at], L->pos, 12), c->lpos[at][3] = L->range;
        for (int j = 0; j < 3; j++) c->lcol[at][j] = L->color[j] * L->intensity;
        c->lcol[at][3] = 0;
    }
    c->linfo[0] = (float)no, c->linfo[1] = look->normal_maps ? look->normal_strength : 0;
    c->linfo[2] = NORMAL_DEPTH_TILES; /* the relief's depth, as the normal maps were made (normals.h) */
    c->linfo[3] = (float)g_debug;
    c->outp[0] = (float)R.cfg.w, c->outp[1] = (float)R.cfg.h, c->outp[2] = 1.0f / tanf(cam->fov_y * 0.5f);
    c->outp[3] = (float)rw / (float)R.cfg.w;
    memcpy(c->tint, g_tint, sizeof g_tint);
    /* the bounce rays' reach and the bounces (0: as uw.exe had them, 48 tiles and the probes' feedback) */
    c->rtlim[0] = look->ray_length > 0 ? look->ray_length : 48.0f, c->rtlim[1] = look->bounces > 0 ? (float)look->bounces : 99.0f;
    c->rtlim[2] = look->normal_maps ? look->sprite_strength : 0;
    memcpy(c->torchc, g_torch_rgb, 12), c->torchc[3] = 1;
    /* the pictures' occlusion and height maps (with the normal maps; 0 and 0: the pictures as before them) */
    c->sprite[0] = look->normal_maps ? look->sprite_ao : 0, c->sprite[1] = look->normal_maps ? look->sprite_height : 0;
    /* the walls' and floors' (NNN_o, NNN_p): z their occlusion's strength, w their relief's depth in texels of a 64
     * texel texture, which the lights' shadows across the relief are cast from (0: none). Their own Esc rows, not
     * tied to the normal maps' (the user, 2026-11-14: each its own switch) */
    c->sprite[2] = look->texture_ao, c->sprite[3] = look->texture_shadow;
    c->texres[0] = (float)R.hatlas_f, c->texres[1] = R.hatlas_maps ? 1.0f : 0.0f, c->texres[2] = R.hatlas_colours ? 1.0f : 0.0f, c->texres[3] = 0;
    c->rtlim[3] = g_fullbright ? -1.0f : g_shade_k;   /* the original's depth scale for its shading; < 0: full bright */
    {   /* the effects' water: on, the rings of the splashes younger than two seconds, the time (held in a comparison) */
        int nr = 0;
        double now = g_hold ? g_hold_now : (double)GetTickCount64() / 1000.0;
        for (int i = 0; i < 16 && g_fx.water && g_ripples_shown; i++) {
            double age = now - g_ripple[i].born;
            if (g_ripple[i].born <= 0 || age < 0 || age > 2.0) continue;
            c->ripple[nr][0] = g_ripple[i].x, c->ripple[nr][1] = g_ripple[i].z, c->ripple[nr][2] = (float)age, c->ripple[nr][3] = g_ripple[i].strength;
            nr++;
        }
        c->fxw[0] = g_fx.water ? 1.0f : 0.0f, c->fxw[1] = (float)nr, c->fxw[2] = secs, c->fxw[3] = 0;
    }

    if (g_atlas_pending >= 0 && R.atlas) { /* the textures' cycling colours, as the game's palette has them now */
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp = {0, {DXGI_FORMAT_R8G8B8A8_UNORM, ATLAS_SIZE, (UINT)g_atlas_rows, 1, ATLAS_SIZE * 4}};
        D3D12_TEXTURE_COPY_LOCATION ad = {R.atlas, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {.SubresourceIndex = 0}};
        D3D12_TEXTURE_COPY_LOCATION as = {g_atlas_up[g_atlas_pending], D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, {.PlacedFootprint = fp}};
        barrier(R.atlas, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
        ID3D12GraphicsCommandList_CopyTextureRegion(R.cl, &ad, 0, 0, 0, &as, NULL);
        barrier(R.atlas, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        g_atlas_pending = -1;
    }
    if (R.ui_dirty && R.ui_tex) { /* this frame's interface */
        barrier(R.ui_tex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
        D3D12_TEXTURE_COPY_LOCATION ud = {R.ui_tex, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {.SubresourceIndex = 0}};
        D3D12_TEXTURE_COPY_LOCATION us = {R.ui_up, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, {.PlacedFootprint = R.ui_fp}};
        ID3D12GraphicsCommandList_CopyTextureRegion(R.cl, &ud, 0, 0, 0, &us, NULL);
        barrier(R.ui_tex, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        R.ui_dirty = false;
    }
    ID3D12DescriptorHeap *heaps[1] = {R.srv_heap};
    ID3D12GraphicsCommandList_SetDescriptorHeaps(R.cl, 1, heaps);
    D3D12_GPU_VIRTUAL_ADDRESS cbva = ID3D12Resource_GetGPUVirtualAddress(R.cbuf);
    D3D12_GPU_VIRTUAL_ADDRESS lva = ID3D12Resource_GetGPUVirtualAddress(R.lights_buf), tva = ID3D12Resource_GetGPUVirtualAddress(R.tiles_buf);
    D3D12_RESOURCE_STATES readable = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    int wp = (int)(R.frame & 1); /* the probes written this frame; the other holds last frame's */
    const bool parts = g_fx.particles && g_rp_ok;
    if (parts) {   /* the particles moved (render_particles.c), drawn after the level and its pictures */
        float right[3] = {cosf(cam->yaw), 0, -sinf(cam->yaw)}, f3[3] = {fwd_of(cam, 0), fwd_of(cam, 1), fwd_of(cam, 2)};
        float up[3] = {f3[1] * right[2] - f3[2] * right[1], f3[2] * right[0] - f3[0] * right[2], f3[0] * right[1] - f3[1] * right[0]};
        float torch[4] = {cam->pos[0], cam->pos[1], cam->pos[2], g_fullbright ? 99.0f : 4.0f};
        g_fxp->particles_frame(R.cl, c->vp, cam->pos, R.cfg.exposure, right, up, torch, g_torch_rgb);
    }

    if (rt_live) cards_as_build(cam->pos);   /* the pictures' shadows: this frame's cards in the ray tracing */
    /* the light probes: each fires its rays and keeps the light arriving */
    if (rt_live) {
        barrier(R.probes[wp], readable, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        ID3D12GraphicsCommandList_SetComputeRootSignature(R.cl, R.croot);
        ID3D12GraphicsCommandList_SetPipelineState(R.cl, R.probe_pso);
        ID3D12GraphicsCommandList_SetComputeRootConstantBufferView(R.cl, CP_CB, cbva);
        ID3D12GraphicsCommandList_SetComputeRootDescriptorTable(R.cl, CP_TABLE, gpu_at(SLOT_ATLAS));
        ID3D12GraphicsCommandList_SetComputeRootShaderResourceView(R.cl, CP_AS, scene_tlas());
        ID3D12GraphicsCommandList_SetComputeRootShaderResourceView(R.cl, CP_PREV, ID3D12Resource_GetGPUVirtualAddress(R.probes[wp ^ 1]));
        ID3D12GraphicsCommandList_SetComputeRootShaderResourceView(R.cl, CP_VERTS, ID3D12Resource_GetGPUVirtualAddress(R.vbuf));
        ID3D12GraphicsCommandList_SetComputeRootShaderResourceView(R.cl, CP_LIGHTS, lva);
        ID3D12GraphicsCommandList_SetComputeRootShaderResourceView(R.cl, CP_TILES, tva);
        ID3D12GraphicsCommandList_SetComputeRootShaderResourceView(R.cl, CP_INDEX, ID3D12Resource_GetGPUVirtualAddress(R.ibuf));
        ID3D12GraphicsCommandList_SetComputeRootUnorderedAccessView(R.cl, CP_OUT, ID3D12Resource_GetGPUVirtualAddress(R.probes[wp]));
        ID3D12GraphicsCommandList_Dispatch(R.cl, (PROBES + 63) / 64, 1, 1);
        barrier(R.probes[wp], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, readable);
    }

    ID3D12GraphicsCommandList_SetGraphicsRootSignature(R.cl, R.root);
    ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(R.cl, GP_CB, cbva);
    ID3D12GraphicsCommandList_SetGraphicsRootShaderResourceView(R.cl, GP_LIGHTS, lva);
    ID3D12GraphicsCommandList_SetGraphicsRootShaderResourceView(R.cl, GP_TILES, tva);
    if (rt_live) {
        ID3D12GraphicsCommandList_SetGraphicsRootShaderResourceView(R.cl, GP_AS, scene_tlas());
        ID3D12GraphicsCommandList_SetGraphicsRootShaderResourceView(R.cl, GP_SVERTS, ID3D12Resource_GetGPUVirtualAddress(R.sprite_vb));
        ID3D12GraphicsCommandList_SetGraphicsRootShaderResourceView(R.cl, GP_PROBES, ID3D12Resource_GetGPUVirtualAddress(R.probes[wp]));
        ID3D12GraphicsCommandList_SetGraphicsRootShaderResourceView(R.cl, GP_VERTS, ID3D12Resource_GetGPUVirtualAddress(R.vbuf));
        ID3D12GraphicsCommandList_SetGraphicsRootShaderResourceView(R.cl, GP_INDEX, ID3D12Resource_GetGPUVirtualAddress(R.ibuf));
    }
    barrier(R.color, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    barrier(R.motion, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    barrier(R.depth, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    ID3D12Resource *guides[3] = {R.albedo, R.spec, R.nrough};
    if (high)
        for (int g = 0; g < 3; g++) barrier(guides[g], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE rtv[5] = {cpu_at(R.rtv_heap, R.rtv_step, RTV_COLOR), cpu_at(R.rtv_heap, R.rtv_step, RTV_MOTION),
                                          cpu_at(R.rtv_heap, R.rtv_step, RTV_ALBEDO), cpu_at(R.rtv_heap, R.rtv_step, RTV_SPEC),
                                          cpu_at(R.rtv_heap, R.rtv_step, RTV_NROUGH)};
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = cpu_at(R.dsv_heap, 0, 0);
    const float black[4] = {0, 0, 0, 1}, zero[4] = {0, 0, 0, 0};
    ID3D12GraphicsCommandList_ClearRenderTargetView(R.cl, rtv[0], black, 0, NULL);
    ID3D12GraphicsCommandList_ClearRenderTargetView(R.cl, rtv[1], zero, 0, NULL);
    if (high) { /* where nothing is drawn: the guides' "sky" (albedo 0.5, nothing else) */
        const float sky[4] = {0.5f, 0.5f, 0.5f, 0};
        for (int g = 2; g < 5; g++) ID3D12GraphicsCommandList_ClearRenderTargetView(R.cl, rtv[g], g == 2 ? sky : zero, 0, NULL);
    }
    ID3D12GraphicsCommandList_ClearDepthStencilView(R.cl, dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
    ID3D12GraphicsCommandList_OMSetRenderTargets(R.cl, high ? 5 : 2, rtv, FALSE, &dsv);
    D3D12_VIEWPORT vp = {0, 0, (float)rw, (float)rh, 0, 1};
    D3D12_RECT sc = {0, 0, rw, rh};
    ID3D12GraphicsCommandList_RSSetViewports(R.cl, 1, &vp);
    ID3D12GraphicsCommandList_RSSetScissorRects(R.cl, 1, &sc);
    if (R.vbuf && R.vertices) {
        ID3D12GraphicsCommandList_SetPipelineState(R.cl, high ? R.high_pso : rt_live || !R.grid_pso ? R.scene_pso : R.grid_pso);
        ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(R.cl, GP_TABLE, gpu_at(SLOT_ATLAS));
        ID3D12GraphicsCommandList_IASetPrimitiveTopology(R.cl, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        D3D12_VERTEX_BUFFER_VIEW vbv = {ID3D12Resource_GetGPUVirtualAddress(R.vbuf), (UINT)(sizeof(Vertex) * (size_t)R.vertices), sizeof(Vertex)};
        ID3D12GraphicsCommandList_IASetVertexBuffers(R.cl, 0, 1, &vbv);
        D3D12_INDEX_BUFFER_VIEW ibv = {ID3D12Resource_GetGPUVirtualAddress(R.ibuf), (UINT)(sizeof(u32) * (size_t)R.indices), DXGI_FORMAT_R32_UINT};
        ID3D12GraphicsCommandList_IASetIndexBuffer(R.cl, &ibv);
        ID3D12GraphicsCommandList_DrawIndexedInstanced(R.cl, (UINT)R.indices, 1, 0, 0, 0);
    }
    if (R.nsprites) { /* the picture cards */
        D3D12_VERTEX_BUFFER_VIEW sv = {ID3D12Resource_GetGPUVirtualAddress(R.sprite_vb), (UINT)(sizeof(Vertex) * (size_t)R.nsprites), sizeof(Vertex)};
        ID3D12GraphicsCommandList_IASetVertexBuffers(R.cl, 0, 1, &sv);
        ID3D12GraphicsCommandList_DrawInstanced(R.cl, (UINT)R.nsprites, 1, 0, 0);
    }
    const bool decals = g_fx.decals && g_rd_ok;
    if (decals) g_fxp->decals_draw(R.cl, c->vp, rtv[0].ptr, dsv.ptr);   /* the marks on the floor, under the particles */
    if (parts) g_fxp->particles_draw(R.cl, rtv[0].ptr, dsv.ptr);   /* the particles into the colour, the depth tested */
    if (parts || decals) {   /* the scene's root state set again after them */
        ID3D12GraphicsCommandList_SetGraphicsRootSignature(R.cl, R.root);
        ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(R.cl, GP_CB, cbva);
        ID3D12GraphicsCommandList_SetGraphicsRootShaderResourceView(R.cl, GP_LIGHTS, lva);
        ID3D12GraphicsCommandList_SetGraphicsRootShaderResourceView(R.cl, GP_TILES, tva);
    }
    barrier(R.color, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    barrier(R.motion, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    barrier(R.depth, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    if (high)
        for (int g = 0; g < 3; g++) barrier(guides[g], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    /* DLSS (or Ray Reconstruction), then DLSS 5 on its picture */
    int show = SLOT_COLOR;
    if (dlss) {
        barrier(R.dlss_out, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        bool up;
        if (high) {
            up = ngx_rr_evaluate(R.cl, R.color, R.depth, R.motion, R.albedo, R.spec, R.nrough, R.dlss_out, jx, jy, rw, rh, R.w2v, R.v2c, reset);
            if (up) R.rr_frames_ok++;
            else R.rr_frames_failed++, up = ngx_dlss_evaluate(R.cl, R.color, R.depth, R.motion, R.dlss_out, jx, jy, rw, rh, reset);
        } else up = ngx_dlss_evaluate(R.cl, R.color, R.depth, R.motion, R.dlss_out, jx, jy, rw, rh, reset);
        barrier(R.dlss_out, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        if (up) {
            show = SLOT_DLSS;
            if (R.nr_ok && R.nr_on) {
                barrier(R.nr_out, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                {   /* DLSS 5 on the view's rectangle only (the original's 3D window), the whole frame otherwise */
                    int sx = (int)(g_vr[0] * R.cfg.w + 0.5f), sy = (int)(g_vr[1] * R.cfg.h + 0.5f);
                    ngx_nr_subrect(sx, sy, (int)(g_vr[2] * R.cfg.w + 0.5f) - sx, (int)(g_vr[3] * R.cfg.h + 0.5f) - sy);
                }
                bool nr = ngx_nr_evaluate(R.cl, R.dlss_out, R.depth, R.motion, R.nr_out, R.cfg.w, R.cfg.h, rw, rh, (float)R.cfg.w / (float)rw,
                                          reset, 0);
                barrier(R.nr_out, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                if (nr) show = SLOT_NR, R.nr_frames_ok++, c->post[1] = R.cfg.nr_saturation;
                else R.nr_frames_failed++;
            }
        }
        /* NGX changes the bound heaps and root state */
        ID3D12GraphicsCommandList_SetDescriptorHeaps(R.cl, 1, heaps);
        ID3D12GraphicsCommandList_SetGraphicsRootSignature(R.cl, R.root);
        ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(R.cl, GP_CB, cbva);
    }

    UINT bi = R.swap ? IDXGISwapChain3_GetCurrentBackBufferIndex(R.swap) : 0;
    ID3D12Resource *target = R.swap ? R.back[bi] : R.final_tex;
    D3D12_RESOURCE_STATES before = R.swap ? D3D12_RESOURCE_STATE_PRESENT : D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier(target, before, D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE out = cpu_at(R.rtv_heap, R.rtv_step, R.swap ? RTV_BACK + (int)bi : RTV_FINAL);
    ID3D12GraphicsCommandList_OMSetRenderTargets(R.cl, 1, &out, FALSE, NULL);
    D3D12_VIEWPORT ovp = {0, 0, (float)R.cfg.w, (float)R.cfg.h, 0, 1};
    D3D12_RECT osc = {0, 0, R.cfg.w, R.cfg.h};
    ID3D12GraphicsCommandList_RSSetViewports(R.cl, 1, &ovp);
    ID3D12GraphicsCommandList_RSSetScissorRects(R.cl, 1, &osc);
    const bool hdr = R.swap && R.hdr_on;
    ID3D12GraphicsCommandList_SetPipelineState(R.cl, hdr ? R.present_hdr_pso : R.present_pso);
    ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(R.cl, GP_TABLE, gpu_at(show));
    ID3D12GraphicsCommandList_IASetPrimitiveTopology(R.cl, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D12GraphicsCommandList_DrawInstanced(R.cl, 3, 1, 0, 0);
    if (hdr && g_want_shot) {
        /* a screenshot with HDR on: the window's FP16 picture does not fit the 8-bit readback, so the same frame is
           drawn once more as SDR (the constants' copy at CB_SHOT_OFF, hdr.x 0) into the 8-bit final texture, copied */
        barrier(target, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        Constants *c2 = (Constants *)((u8 *)R.cb + CB_SHOT_OFF);
        memcpy(c2, c, sizeof *c2), c2->hdr[0] = 0;
        barrier(R.final_tex, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        D3D12_CPU_DESCRIPTOR_HANDLE fin = cpu_at(R.rtv_heap, R.rtv_step, RTV_FINAL);
        ID3D12GraphicsCommandList_OMSetRenderTargets(R.cl, 1, &fin, FALSE, NULL);
        ID3D12GraphicsCommandList_SetPipelineState(R.cl, R.present_pso);
        ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(R.cl, GP_CB, cbva + CB_SHOT_OFF);
        ID3D12GraphicsCommandList_DrawInstanced(R.cl, 3, 1, 0, 0);
        barrier(R.final_tex, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION dst = {R.readback, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, {.PlacedFootprint = R.readback_fp}};
        D3D12_TEXTURE_COPY_LOCATION src = {R.final_tex, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {.SubresourceIndex = 0}};
        ID3D12GraphicsCommandList_CopyTextureRegion(R.cl, &dst, 0, 0, 0, &src, NULL);
        g_want_shot = false;
    } else if (R.swap && !g_want_shot) {
        barrier(target, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    } else if (R.swap) { /* a screenshot: what the window shows, copied for render_readback */
        barrier(target, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION dst = {R.readback, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, {.PlacedFootprint = R.readback_fp}};
        D3D12_TEXTURE_COPY_LOCATION src = {target, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {.SubresourceIndex = 0}};
        ID3D12GraphicsCommandList_CopyTextureRegion(R.cl, &dst, 0, 0, 0, &src, NULL);
        barrier(target, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
        g_want_shot = false;
    } else {
        barrier(target, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION dst = {R.readback, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT, {.PlacedFootprint = R.readback_fp}};
        D3D12_TEXTURE_COPY_LOCATION src = {R.final_tex, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX, {.SubresourceIndex = 0}};
        ID3D12GraphicsCommandList_CopyTextureRegion(R.cl, &dst, 0, 0, 0, &src, NULL);
    }
    ID3D12GraphicsCommandList_Close(R.cl);
    ID3D12CommandList *lists[1] = {(ID3D12CommandList *)R.cl};
    ID3D12CommandQueue_ExecuteCommandLists(R.queue, 1, lists);
    if (R.swap) IDXGISwapChain3_Present(R.swap, R.cfg.vsync ? 1 : 0, 0);
    wait_gpu();
    ID3D12CommandAllocator_Reset(R.alloc);
    ID3D12GraphicsCommandList_Reset(R.cl, R.alloc, NULL);
    memcpy(R.prev_vp, c->vp_nojit, 64);
    R.have_prev = true;
    R.frame++;
    return true;
}

void render_want_screenshot(void) { g_want_shot = true; }

bool render_readback(u8 *rgba) {
    if (!R.readback) return false;
    u8 *p = NULL;
    /* the last row is not padded to the pitch */
    D3D12_RANGE all = {0, (SIZE_T)R.readback_fp.Offset + (SIZE_T)R.readback_fp.Footprint.RowPitch * (SIZE_T)(R.cfg.h - 1) + (SIZE_T)R.cfg.w * 4};
    HRESULT mh = ID3D12Resource_Map(R.readback, 0, &all, (void **)&p);
    if (FAILED(mh)) {
        D3D12_RESOURCE_DESC rd = ID3D12Resource_GetDesc(R.readback);
        logf_("render: the picture cannot be read back (0x%08lX, device 0x%08lX; pitch %u x %d rows, buffer %llu, offset %llu)\n",
              (unsigned long)mh, (unsigned long)ID3D12Device_GetDeviceRemovedReason(R.dev), R.readback_fp.Footprint.RowPitch, R.cfg.h,
              (unsigned long long)rd.Width, (unsigned long long)R.readback_fp.Offset);
        return false;
    }
    for (int y = 0; y < R.cfg.h; y++)
        memcpy(rgba + (size_t)y * (size_t)R.cfg.w * 4, p + R.readback_fp.Offset + (size_t)y * R.readback_fp.Footprint.RowPitch, (size_t)R.cfg.w * 4);
    D3D12_RANGE none = {0, 0};
    ID3D12Resource_Unmap(R.readback, 0, &none);
    return true;
}

void render_shutdown(void) {
    if (!R.dev) return;
    wait_gpu();
    if (R.rr_frames_ok || R.rr_frames_failed)
        logf_("Ray Reconstruction: %d frames, %d failed (shown with DLSS upscaling)\n", R.rr_frames_ok, R.rr_frames_failed);
    if (R.nr_ok) logf_("DLSS 5: %d frames through the neural model, %d failed (shown with DLSS alone)\n", R.nr_frames_ok, R.nr_frames_failed);
    ngx_stop(R.dev);
}
