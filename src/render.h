/* The D3D12 renderer: the level's triangles into colour + depth + motion vectors, then (when on)
 * DLSS Super Resolution and DLSS 5 neural rendering, then onto the window's back buffer, or headless
 * into a picture that can be read back. */
#pragma once
#include <wchar.h>

#include "level.h"
#include "lights.h"

typedef struct {
    float pos[3];
    float yaw, pitch;   /* radians; yaw 0 looks north (+z), yaw > 0 turns east; pitch > 0 looks up */
    float fov_y;        /* radians */
    float roll;         /* radians, about the view direction (0 level) */
} Camera;

enum { DLSS_Q_PERF, DLSS_Q_BALANCED, DLSS_Q_QUALITY, DLSS_Q_DLAA, DLSS_Q_ULTRA };   /* (ULTRA: Ultra Performance, 2026-11-14) */

typedef struct {
    void *hwnd;         /* NULL = headless */
    int w, h;           /* the output size */
    bool vsync;
    /* DLSS: started at init when dlss is set (it can then be turned on and off any frame) */
    bool dlss;
    int dlss_quality;   /* DLSS_Q_* */
    bool dlss5;         /* DLSS 5 neural rendering on DLSS's picture */
    int nr_style;       /* 0 A - Default, 1 B - Natural, 2 C - Cinematic */
    float nr_intensity, nr_structure, nr_tone, nr_saturation;
    float exposure;     /* the HDR scale DLSS / DLSS 5 see (undone before the screen) */
    const wchar_t *dlss_dir;  /* the folder with nvngx_dlss.dll and nvngx_dlssnr.dll */
    const wchar_t *log_dir;   /* where NGX may write its logs */
    bool raytracing;    /* hardware ray tracing (DXR 1.1) when the GPU has it; else shadows through the tile grid */
    bool rt_high;       /* ray tracing High at start: a bounce ray per pixel, denoised by DLSS Ray Reconstruction */
    int ui_scale;       /* the interface layer's size: the window divided by this (0: by the window's height) */
} RenderConfig;

typedef struct {
    float gamma;           /* 1 = as the textures are */
    bool normal_maps;
    float normal_strength; /* 1 = as made; more = deeper */
    float bounce;          /* light bounced off surfaces (ray tracing): 1 = as it would, 0 = none */
    float sheen;           /* the stone's sheen: a soft highlight that follows the normal maps (0 = none, as uw.exe) */
    float ray_length;      /* how far a bounce ray goes, tiles (0 = 48) */
    int bounces;           /* 1: one bounce; 2 and more: the probes' light fed back (as many as build up); 0 = that */
    float sprite_strength; /* the pictures' normal maps (objects, creatures, doors): 0 = as the walls' */
    float sprite_ao;       /* the pictures' occlusion maps (NAME_o.png) on their light: 0 none, 1 full */
    float sprite_height;   /* the pictures' height maps (NAME_p.png): their normals' parallax, texels (0 none) */
    float texture_ao;      /* the walls' and floors' occlusion maps (NNN_o.png: the joints): 0 none, 1 as made */
    float texture_shadow;  /* their height maps (NNN_p.png): the relief's depth the lights' shadows are cast from,
                            * texels of a 64 texel texture (0 none) */
} Look;

/* the placed lights of a level and each tile's list of them (lights_tiles), at a level's load */
/* every light reaches 10% less far (the user, 2026-11-16): a placed or burning light's range times this as the
   renderer takes it; the player's light in common.hlsli (LIGHT_REACH, the same number) */
#define RENDER_LIGHT_REACH 0.9f
void render_set_lights(const Light *l, int n, const u32 *tiles);
/* placed light k moved (it keeps the tiles render_set_lights gave it) */
void render_set_light_pos(int k, const float pos[3]);
/* the picture cards (world.c): up to RENDER_MAX_SPRITE_VERTS vertices, whenever they change */
#define RENDER_MAX_SPRITE_VERTS (6 * 4096)
void render_set_sprites(const Vertex *v, int n);
/* the interface overlay: its size (a whole factor, scale, smaller than the window) and this frame's pixels */
void render_ui_size(int *w, int *h, int *scale);
void render_set_ui(const u8 *rgba);
/* our panel on the GPU for this frame (Vulkan: render_vk.c; D3D12 false, the caller blends it into the layer) */
bool render_set_panel(const u8 *rgba, int w, int h, int scale, const unsigned *text, int stride, int x0, int y0, int x1, int y1);
bool render_has_panel(void);   /* render_set_panel can take a panel here (Vulkan, started) */
unsigned render_panel_gen(void);   /* how many panels were uploaded: a caller's own one is still there while unchanged */
bool render_keep_panel(void);      /* the panel uploaded last, over this frame again (nothing sent) */
/* one frame in flight (Vulkan; the row Frame overlap): false waits for each frame at its submit, as before */
void render_set_overlap(bool on);
bool render_overlap(void);
/* the player's torch, every frame */
void render_set_torch(bool on, const float pos[3], const float color[3], float intensity, float range);
void render_set_shade_scale(float k);
void render_set_fullbright(bool on);
void render_set_light_fade(bool on);   /* the lights' fade by distance: 50% at 2 tiles, 30% at 3, gone at 4 (on) */
bool render_light_fade(void);
void render_set_object_shadows(bool on);
bool render_object_shadows(void);
void render_blackbody(float kelvin, float rgb[3]);
void render_set_adaptation(float a);
float render_adaptation(void);          /* < 0: automatic (CAM16's degree, render_adaptation_auto) */
float render_adaptation_auto(void);
void render_set_light_cat(int cat);     /* the light's colour: 0 CAT16, 1 Bradford (render.c) */
int render_light_cat(void);
/* HDR output (render.c): wanted on / off; on while Windows runs the window's monitor in HDR. Levels in nits: peak 0
   is the monitor's own; paper white the interface's white and the SDR picture's */
void render_set_hdr(bool on);
bool render_hdr_wanted(void);
bool render_hdr_on(void);
bool render_hdr_available(void);
void render_set_hdr_levels(float peak, float paper);
float render_hdr_peak(void);
float render_hdr_peak_setting(void);
float render_hdr_paper(void);
void render_set_torch_motion(float vx, float vz);
void render_set_puff_depth(float d);
void render_set_breath_depth(float d);     /* the fires' slow breath now and then, 0-0.4 (0.20; 0 none) */
float render_breath_depth(void);
void render_set_campfire_depth(float d);   /* the campfires' own swell, 0-0.6 (0.35) */
void render_set_torch_colour(const float rgb[3]);
/* no light carried (the light level 0): the player's light steady, a point at the eye with no flame (the user) */
void render_set_torch_steady(bool on);
bool render_fullbright(void);
void render_set_torch_original(bool on, const float pos[3], const float eye[3], int level, float range, float exponent);
bool render_raytracing(void); /* on now */
/* started on this GPU (DXR 1.1): then it can be switched off and on any frame */
bool render_raytracing_available(void);
void render_set_raytracing(bool on);
/* ray tracing High (a bounce ray from every pixel + DLSS Ray Reconstruction, in DLSS upscaling's place): wanted,
 * and whether it is what the frames get (it needs ray tracing and DLSS on, and Ray Reconstruction to start) */
void render_set_rt_high(bool on);
bool render_rt_high_wanted(void);
bool render_rt_high_active(void);

bool render_init(const RenderConfig *cfg);
/* the part of the output the view fills (fractions, from the top left; 0,0,1,1 the whole): the frustum is fitted to it */
void render_set_view_rect(float x0, float y0, float x1, float y1);
/* the lens as the tangents of the half angles across and up; 0, 0 for the camera's fov_y and the view's shape */
void render_set_lens(float tan_x, float tan_y);
/* the checks: 1 draws the shading normals as colours, 0 the picture */
void render_set_debug(int what);
/* the scene tinted after its gamma (the original's palette fades and flashes); on false: none */
void render_set_tint(float r, float g, float b, bool on);
/* the atlas's first rows (the textures; ATLAS_SIZE wide RGBA) rewritten at the next frame */
void render_update_atlas(const u8 *rgba, int rows);
/* the walls' and floors' upscaled textures (the Esc menu's Textures): rgba, size x size, the atlas's rows 0-959 at
 * factor times the size (alpha below 128: a cycling colour's palette entry, common.hlsli); NULL or factor 0: off.
 * Kept across levels. false: not made (out of GPU memory; off). maps (the user, 2026-11-15: the upscaled textures'
 * own relief maps): rgba is 2 x size wide, size high, and its right half holds each texture's maps at the same
 * place: r, g the normal's x, y; b the occlusion; a the height (common.hlsli hmaps_at) */
bool render_set_hires(const u8 *rgba, int size, int factor, bool maps, bool colours);
bool render_upload(const Mesh *m);
bool render_frame(const Camera *cam, const Look *look, bool reset);
/* DLSS (and DLSS 5 with it, when it started) on or off; returns whether it is now on */
bool render_set_dlss(bool on);
/* DLSS / DLSS 5 started on this card (false: the options do not offer it); DLSS 5 on / off by itself */
bool render_dlss_available(void);
bool render_dlss5_available(void);
bool render_dlss5_on(void);
bool render_set_dlss5(bool on);
/* DLSS 5 style (0 A - Default, 1 B - Natural, 2 C - Cinematic), any frame */
void render_set_dlss5_style(int style);
bool render_dlss_on(void);
/* DLSS's mode (DLSS_Q_*) changed during play (the user, 2026-11-14: the Esc menu's "DLSS mode"): the size the scene
 * is drawn at before DLSS enlarges it to the output. false: it could not be made, the old mode is back. */
bool render_set_dlss_quality(int q);
int render_dlss_quality(void);
/* the size drawn now (the output's when DLSS is off) and the output's; and what mode q would draw at this output */
void render_sizes(int *rw, int *rh, int *ow, int *oh);
void render_size_for(int q, int *rw, int *rh);
/* DLSS 5's strength and local structure, 0..1, during play */
void render_set_dlss5_params(float intensity, float structure);
/* a short line: "DLSS 5 on" / "DLSS on" / "DLSS off" / "DLSS unavailable" */
const char *render_dlss_text(void);
/* the last frame (w*h RGBA8): always headless; with a window, after render_want_screenshot and one frame */
bool render_readback(u8 *rgba);
void render_want_screenshot(void);
void render_shutdown(void);
/* the game quits and the process ends at once (the row Quick quit): the GPU's work finished, nothing taken down (Vulkan:
   NVIDIA's NGX shutdown faults at the quit since the 2026-11-22 update; D3D12: render_shutdown, as before) */
void render_quit_fast(void);
const char *render_status(void);

/* --- the effects (docs\superpowers\plans\2026-11-12-fx-audio-and-visual.md): each pass runs only while its setting is
 * on; off, the frames are as without them --- */
void render_set_fx(int particles, int decals, int water, int fog);
/* the passes, attached before render_init by the program that has them (uwview.c: render_particles.c's); D3D12's
 * device, command list and descriptor handles as void * and size_t, so this header needs no D3D12 */
typedef struct {
    bool (*particles_init)(void *dev);
    void (*particles_frame)(void *cl, const float vp[16], const float eye[3], float exposure, const float right[3], const float up[3],
                            const float torch[4], const float torchc[3]);
    void (*particles_draw)(void *cl, size_t rtv, size_t dsv);
    bool (*decals_init)(void *dev);
    void (*decals_draw)(void *cl, const float vp[16], size_t rtv, size_t dsv);
} RenderFxPasses;
void render_fx_attach(const RenderFxPasses *p);
void render_hold_time(bool on);   /* the checks: the flames' time and the frame count held (an effect compared on / off) */
/* the passes' own (render_particles.c, render_decals.c): uwview.c puts them in its RenderFxPasses */
bool rfx_particles_init(void *dev);
void rfx_particles_frame(void *cl, const float vp[16], const float eye[3], float exposure, const float right[3], const float up[3],
                         const float torch[4], const float torchc[3]);
void rfx_particles_draw(void *cl, size_t rtv, size_t dsv);
bool rfx_decals_init(void *dev);
void rfx_decals_draw(void *cl, const float vp[16], size_t rtv, size_t dsv);
/* the particles (render_particles.c): a ring of RP_MAX on the GPU; kind 1 glowing (bounces off the floor), 2 lit (gone
 * on the floor), 3 smoke (lit, growing, through the floor); grav the fall's acceleration (tiles/s^2, < 0 rises), drag
 * the speed's decay (1/s); floor_y the floor under the spawn */
#define RP_MAX 65536
typedef struct { float pos[3], life, vel[3], age, col[3], size, floor_y, kind, drag, grav; } RParticle;
void render_particles_spawn(const RParticle *p, int n);
void render_particles_clear(void);   /* every particle gone (a level change) */
int render_particles_live(void);     /* alive now (from their lives as spawned) */
/* the decals (render_decals.c): marks on the floor at x, y (the floor's height), z, size tiles across, turned by yaw;
 * rgb their colour (NULL: the kind's); up to 512 a level, the oldest replaced */
enum { RD_BLOOD, RD_SCORCH, RD_ICE, RD_ACID, RD_KINDS };
void render_decals_add(float x, float y, float z, int kind, float size, float yaw, const float rgb[3]);
void render_decals_clear(void);
int render_decals_count(void);
/* the water (fx_water: the scene's shader): a splash's ring at x, z (tiles), strength 1 a splash */
void render_water_ripple(float x, float z, float strength);
void render_water_ripples_shown(bool on);   /* (the checks) */

/* --- the fog and smoke (the user, 2026-11-17: "physical simulation of smoke and water haze fog ... displaced by you
 * when you move through it"; src\render_vk_vol.h, shaders\volume.hlsl; the Vulkan renderer's, the D3D12 one has none):
 * a box of air cells round the eye stepped on the GPU each frame, lit by shadow rays, drawn by stepping through it.
 * uwview.c (volfog.inc) gives each frame the dials (the Esc menu's Fog and smoke page) as the shader's own constants
 * and what gives off, heats and pushes the air (fires, blasts, the player's and the creatures' bodies) --- */
#define RV_EMIT_MAX 96
/* pr: the place (a body's feet), w the radius; vh: its velocity, w its height (0: a ball); q: fog, smoke and heat
   given off a second, w the push outward (tiles a second); k: x how far the air is carried along with it (0..1) */
typedef struct { float pr[4], vh[4], q[4], k[4]; } RvEmit;
enum { RVP_AIR, RVP_FOGA, RVP_FOGB, RVP_SMKA, RVP_SMKB, RVP_FOGL, RVP_FOGM, RVP_SMKL, RVP_SMKM, RVP_LIGHTP, RVP_LIGHTQ, RVP_FOGC, RVP_FOGP, RVP_EXTRA, RVP_MORE, RVP_SHAPE, RVP_SCREEN, RVP_N };   /* (RVP_SCREEN: the renderer's own) */
typedef struct {
    int on;
    int cells, box;         /* cells to a tile; the box's width in tiles */
    int pressure;           /* the pressure's passes a step */
    int view_steps, light_share;
    int half;               /* 1: the picture drawn at half size (cs_march), full size at edges */
    float rate;             /* steps a second; 0: one a frame */
    float par[RVP_N][4];    /* volume.hlsl's VC from air to lightq */
    int nemit;
    RvEmit emit[RV_EMIT_MAX];
} RenderVolume;
void render_volume(const RenderVolume *v);   /* this frame's; NULL or on 0: none */
bool render_volume_available(void);
