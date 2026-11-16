/* ngx_vk.h: NVIDIA NGX (DLSS upscaling and DLSS Ray Reconstruction) through NGX's Vulkan interface, for the Vulkan
 * renderer (render_vk.c) as ngx.h is for the D3D12 one. DLSS 5 is not here (dropped for the Vulkan renderer).
 *
 * The order of use:
 *   ngxvk_load()                 before the Vulkan instance exists: finds NGX; false = no DLSS on this machine
 *   ngxvk_extensions()           the instance and device extensions NGX needs; add them when making both
 *   ngxvk_start()                once the device exists
 *   ngxvk_dlss_* / ngxvk_rr_*    create (records on the command buffer), evaluate each frame, release
 *   ngxvk_stop()                 before the device is destroyed
 *
 * The pictures given to evaluate: colour, depth, motion and the guides in VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
 * (the depth's view with the depth aspect), the output in VK_IMAGE_LAYOUT_GENERAL with storage usage. NGX records its
 * own pipelines and descriptor sets on the command buffer: bind the renderer's again after an evaluate. */
#pragma once
#include <stdbool.h>
#include <wchar.h>

#include "vk_load.h"

typedef struct {
    VkImage image;
    VkImageView view;
    VkFormat format;
    VkImageAspectFlags aspect;   /* VK_IMAGE_ASPECT_COLOR_BIT, or _DEPTH_BIT for the depth */
    int w, h;                    /* the whole image's size */
} NgxImage;

bool ngxvk_load(void);
void ngxvk_extensions(const char *const **inst, unsigned *ninst, const char *const **dev, unsigned *ndev);
/* data_dir: where NGX writes its own logs. search: folders holding the feature libraries (nvngx_dlss.dll and
 * nvngx_dlssd.dll; libnvidia-ngx-dlss.so.* and -dlssd on Linux). gipa / gdpa: the loader's own two functions. */
bool ngxvk_start(VkInstance inst, VkPhysicalDevice phys, VkDevice dev, PFN_vkGetInstanceProcAddr gipa,
                 PFN_vkGetDeviceProcAddr gdpa, const wchar_t *data_dir, const wchar_t *const *search, int nsearch);
void ngxvk_stop(VkDevice dev);
bool ngxvk_dlss_available(void);

/* NGX's PerfQuality values (ngx.h's DLSS_PERF ... DLSS_DLAA are the same numbers) */
enum { NGXVK_PERF = 0, NGXVK_BALANCED = 1, NGXVK_QUALITY = 2, NGXVK_ULTRAPERF = 3, NGXVK_DLAA = 5 };

/* DLSS Super Resolution for render size -> output size. As ngx.h's: jitter in render pixels; motion vectors render
 * size, in render pixels, current -> previous; depth as drawn (not inverted); reset: a camera cut. The scene may be
 * drawn into the top left rw x rh part of larger images (the subrect is given). */
bool ngxvk_dlss_create(VkDevice dev, VkCommandBuffer cmd, int render_w, int render_h, int out_w, int out_h, int quality);
bool ngxvk_dlss_evaluate(VkCommandBuffer cmd, const NgxImage *color, const NgxImage *depth, const NgxImage *mv,
                         const NgxImage *output, float jitter_x, float jitter_y, int render_w, int render_h, bool reset);
void ngxvk_dlss_release(void);

/* DLSS Ray Reconstruction, in DLSS upscaling's place while on. Guides at render size: diffuse and specular albedo,
 * world-space normals with the linear roughness in w. w2v / v2c: 16 floats each, row-major for row vectors. */
bool ngxvk_rr_create(VkDevice dev, VkCommandBuffer cmd, int render_w, int render_h, int out_w, int out_h, int quality);
bool ngxvk_rr_ready(void);
bool ngxvk_rr_evaluate(VkCommandBuffer cmd, const NgxImage *color, const NgxImage *depth, const NgxImage *mv,
                       const NgxImage *diffuse, const NgxImage *specular, const NgxImage *normal_rough,
                       const NgxImage *output, float jitter_x, float jitter_y, int render_w, int render_h, float *w2v,
                       float *v2c, bool reset);
void ngxvk_rr_release(void);
