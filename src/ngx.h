/* NVIDIA NGX (DLSS) from plain C, without NVIDIA's Microsoft-built loader library: the engine
 * loads the driver's NGXCore (_nvngx.dll) itself and calls it the way that loader does (read from
 * nvsdk_ngx_s.lib, SDK 310.9.16; see NOTES.md). */
#pragma once
#define COBJMACROS
#define WIDL_C_INLINE_WRAPPERS
#include <d3d12.h>
#include <dxgi1_4.h>
#include <stdbool.h>
#include <wchar.h>

/* Starts NGX on the device. search: folders holding feature DLLs (nvngx_dlss.dll ...). data_dir:
 * where NGX writes its own logs (on O:). Returns false (and prints why) when NGX is unavailable. */
bool ngx_start(ID3D12Device *dev, const wchar_t *data_dir, const wchar_t *const *search, int nsearch);
void ngx_stop(ID3D12Device *dev);
bool ngx_dlss_available(void);

enum { DLSS_PERF = 0, DLSS_BALANCED = 1, DLSS_QUALITY = 2, DLSS_ULTRAPERF = 3, DLSS_DLAA = 5 };   /* NGX's PerfQuality values */
/* Creates DLSS Super Resolution for render size -> output size (recorded on cl). */
bool ngx_dlss_create(ID3D12GraphicsCommandList *cl, int render_w, int render_h, int out_w, int out_h, int quality);
/* Upscales color (render size, RGBA16F) into output (out size, a UAV texture). jitter_x/y: the
 * sub-pixel offset of this frame's projection, in render pixels; mv: render-size RG16F motion
 * vectors in render pixels (current -> previous); depth: render-size D32F. reset: a camera cut. */
bool ngx_dlss_evaluate(ID3D12GraphicsCommandList *cl, ID3D12Resource *color, ID3D12Resource *depth,
                       ID3D12Resource *mv, ID3D12Resource *output, float jitter_x, float jitter_y,
                       int render_w, int render_h, bool reset);
void ngx_dlss_release(void);

/* --- DLSS Ray Reconstruction (NGX feature 13, nvngx_dlssd.dll) ---------------------------------
 * Denoises and upscales a noisy ray-traced picture in one go (it takes DLSS upscaling's place while
 * on). Its guides, all render size: diffuse and specular albedo (linear RGB), world-space normals with
 * the linear roughness in w. w2v / v2c: world to view and view to clip, row-major for row vectors. */
bool ngx_rr_create(ID3D12GraphicsCommandList *cl, int render_w, int render_h, int out_w, int out_h, int quality);
bool ngx_rr_ready(void);
bool ngx_rr_evaluate(ID3D12GraphicsCommandList *cl, ID3D12Resource *color, ID3D12Resource *depth, ID3D12Resource *mv,
                     ID3D12Resource *diffuse, ID3D12Resource *specular, ID3D12Resource *normal_rough,
                     ID3D12Resource *output, float jitter_x, float jitter_y, int render_w, int render_h,
                     float *w2v, float *v2c, bool reset);
void ngx_rr_release(void);

/* --- DLSS 5 neural rendering (NGX feature 18) ------------------------------------------------
 * The user's neural DLL (nvngx_dlssnr.dll, the RTX 40 build) fails the driver's signature check, so
 * NGX will not load it. The engine loads it itself, as OptiScaler's forwarder does (decoded with REA:
 * LoadLibraryExW, then the DLL's own NVSDK_NGX_D3D12_Init_Ext / CreateFeature / EvaluateFeature /
 * ReleaseFeature, with the DLSSNR.* parameters). Needs ngx_start first (for a parameter object). */
typedef struct {
    float intensity, local_structure, local_tone, skin_structure;
    int style, preset, auto_mask;
} NrSettings;
bool ngx_nr_start(ID3D12Device *dev, ID3D12GraphicsCommandList *cl, const wchar_t *dll, const wchar_t *data_dir,
                  int width, int height, const NrSettings *s, int passes);
/* Runs the neural model on color (width x height) into output (same size, a UAV texture). depth and
 * mv are render-size (rw x rh); mv in render pixels, scaled by mv_scale to color pixels. */
void ngx_nr_subrect(int x, int y, int w, int h);
bool ngx_nr_evaluate(ID3D12GraphicsCommandList *cl, ID3D12Resource *color, ID3D12Resource *depth, ID3D12Resource *mv,
                     ID3D12Resource *output, int width, int height, int rw, int rh, float mv_scale, bool reset, int pass);
/* the passes made at start (1-3), and the style (0 A - Default, 1 B - Natural, 2 C - Cinematic), changeable any frame */
int ngx_nr_passes(void);
void ngx_nr_set_params(float intensity, float structure, float tone);
void ngx_nr_set_style(int style);
int ngx_nr_style(void);
void ngx_nr_stop(void);
const char *ngx_nr_error(void);
