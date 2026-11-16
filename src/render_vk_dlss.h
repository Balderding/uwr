/* render_vk_dlss.h: DLSS on the Vulkan renderer (milestone M5): render.c's start_dlss, render_size_for,
 * render_set_dlss(_quality), set_dlss_text and the frame's evaluate, through fable uwr's src\ngx_vk.c (NGX's Vulkan
 * interface). Super Resolution, and Ray Reconstruction for ray tracing High (scene.hlsl -D RT -D RTHIGH: five targets,
 * the guides). No DLSS 5 on Vulkan (the user, 2026-11-15). Included by render_vk.c after render_vk_rt.h.
 *
 * As render.c: every target is the output's size and the scene is drawn into its top-left render-size part; NGX's
 * inputs are read in SHADER_READ_ONLY_OPTIMAL (where the scene pass leaves them), the output written in GENERAL. */
#include "ngx_vk.h"
#include "scene_ps_rth_spv.h"

static struct {
    bool loaded;         /* the driver's NGX (ngxvk_load, before the instance) */
    bool started;        /* ngxvk_start */
    bool ok;             /* Super Resolution made: DLSS can be on */
    bool on;             /* on (render_set_dlss) */
    bool high_active;    /* this frame drew ray tracing High (Ray Reconstruction) */
    bool out_ready;      /* the output image has been through a layout change */
    int rw, rh;          /* the render size */
    Img out, albedo, spec, nrough;
    VkPipeline scene_rth;
    unsigned rr_ok, rr_failed;
    float w2v[16], v2c[16];
    char text[64];
} DL;

static const float DLSS_SCALE[5] = {0.5f, 0.58f, 0.6667f, 1.0f, 1.0f / 3.0f};
static const int DLSS_NGX_Q[5] = {NGXVK_PERF, NGXVK_BALANCED, NGXVK_QUALITY, NGXVK_DLAA, NGXVK_ULTRAPERF};
static const char *const DLSS_QN[5] = {"performance", "balanced", "quality", "DLAA", "ultra performance"};

void render_size_for(int q, int *rw, int *rh)
{
    q = MAX(0, MIN(q, 4));
    *rw = MAX(64, (int)((float)V.cfg.w * DLSS_SCALE[q] + 0.5f));
    *rh = MAX(64, (int)((float)V.cfg.h * DLSS_SCALE[q] + 0.5f));
}

static void set_dlss_text(void)
{
    snprintf(DL.text, sizeof DL.text, "%s", !DL.ok ? "DLSS unavailable" : !DL.on ? "DLSS off" : "DLSS on (no DLSS 5 with Vulkan)");
}

static NgxImage ngx_img(const Img *m, VkFormat fmt)
{
    NgxImage n = {m->img, m->view, fmt, m->aspect, V.cfg.w, V.cfg.h};
    return n;
}

/* render.c's start_dlss without DLSS 5: NGX started, Super Resolution made at the quality's render size, the output
   and the guides made (output size) */
static void dlss_start(void)
{
    if (!DL.loaded || !V.cfg.dlss_dir) return;
    int q = V.cfg.dlss_quality = MAX(0, MIN(V.cfg.dlss_quality, 4));
    render_size_for(q, &DL.rw, &DL.rh);
    const wchar_t *search[1] = {V.cfg.dlss_dir};
#ifdef _WIN32
    if (V.cfg.log_dir) CreateDirectoryW(V.cfg.log_dir, NULL);
#endif
    if (!(DL.started = ngxvk_start(V.inst, V.phys, V.dev, vkGetInstanceProcAddr, vkGetDeviceProcAddr, V.cfg.log_dir, search, 1))) return;
    int w = V.cfg.w, h = V.cfg.h;
    const VkImageUsageFlags rt = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (!make_img(&DL.out, w, h, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT) ||
        !make_img(&DL.albedo, w, h, VK_FORMAT_R8G8B8A8_UNORM, rt) || !make_img(&DL.spec, w, h, VK_FORMAT_R8G8B8A8_UNORM, rt) ||
        !make_img(&DL.nrough, w, h, VK_FORMAT_R16G16B16A16_SFLOAT, rt))
        return (void)logf_("render: out of GPU memory (DLSS)\n");
    begin_cmd();
    /* the guides readable from the start (each RT High frame clears them); the output's first layout */
    Img *g[3] = {&DL.albedo, &DL.spec, &DL.nrough};
    for (int i = 0; i < 3; i++) barrier(g[i]->img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    barrier(DL.out.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    DL.ok = ngxvk_dlss_create(V.dev, V.cmd, DL.rw, DL.rh, w, h, DLSS_NGX_Q[q]);
    submit_wait(VK_NULL_HANDLE, VK_NULL_HANDLE);
}

static void dlss_stop(void)
{
    if (DL.started) {
        if (DL.rr_ok || DL.rr_failed)   /* (render.c's line, the checks read it) */
            logf_("Ray Reconstruction: %d frames, %d failed (shown with DLSS upscaling)\n", (int)DL.rr_ok, (int)DL.rr_failed);
        vkDeviceWaitIdle(V.dev);
        ngxvk_rr_release();
        ngxvk_dlss_release();
        ngxvk_stop(V.dev);
    }
    free_img(&DL.out), free_img(&DL.albedo), free_img(&DL.spec), free_img(&DL.nrough);
    if (DL.scene_rth) vkDestroyPipeline(V.dev, DL.scene_rth, NULL);
    bool loaded = DL.loaded;
    memset(&DL, 0, sizeof DL);
    DL.loaded = loaded;
}

bool render_dlss_available(void) { return DL.ok; }
bool render_dlss_on(void) { return DL.on; }
const char *render_dlss_text(void) { return DL.text; }
int render_dlss_quality(void) { return V.cfg.dlss_quality; }
void render_sizes(int *rw, int *rh, int *ow, int *oh)
{
    bool dlss = DL.on && DL.ok;
    *rw = dlss ? DL.rw : V.cfg.w, *rh = dlss ? DL.rh : V.cfg.h, *ow = V.cfg.w, *oh = V.cfg.h;
}
bool render_set_dlss(bool on)
{
    DL.on = on && DL.ok;
    V.have_prev = false; /* the history starts again */
    set_dlss_text();
    return DL.on;
}
/* render.c's render_set_dlss_quality: between frames both features let go, Super Resolution made again at the new
   size (Ray Reconstruction by the next frame that wants it); the old mode again when the new cannot be made */
static int g_dlss_made;
bool render_set_dlss_quality(int q)
{
    int old = V.cfg.dlss_quality;
    q = MAX(0, MIN(q, 4));
    if (!DL.ok) return false;
    if (q == old) return true;
    vkDeviceWaitIdle(V.dev);
    ngxvk_rr_release();
    ngxvk_dlss_release();
    V.cfg.dlss_quality = q;
    render_size_for(q, &DL.rw, &DL.rh);
    begin_cmd();
    bool ok = ngxvk_dlss_create(V.dev, V.cmd, DL.rw, DL.rh, V.cfg.w, V.cfg.h, DLSS_NGX_Q[q]);
    submit_wait(VK_NULL_HANDLE, VK_NULL_HANDLE);
    if (!ok) {
        V.cfg.dlss_quality = old;
        render_size_for(old, &DL.rw, &DL.rh);
        begin_cmd();
        if (!ngxvk_dlss_create(V.dev, V.cmd, DL.rw, DL.rh, V.cfg.w, V.cfg.h, DLSS_NGX_Q[old])) DL.ok = false, DL.on = false;
        submit_wait(VK_NULL_HANDLE, VK_NULL_HANDLE);
        logf_("render: DLSS %s could not be made: %s again\n", DLSS_QN[q], DLSS_QN[old]);
    }
    g_dlss_made++;
    V.have_prev = false;
    DL.high_active = false;
    set_dlss_text();
    logf_("render: DLSS mode %s: %dx%d -> %dx%d (made %d)\n", DLSS_QN[V.cfg.dlss_quality], DL.rw, DL.rh, V.cfg.w, V.cfg.h, g_dlss_made);
    return ok;
}

/* the frame's upscaling (after the scene and its effects, the inputs readable): Ray Reconstruction when high, else
   (or when it fails) Super Resolution; true when DL.out holds the frame */
static bool dlss_evaluate(bool high, float jx, float jy, int rw, int rh, bool reset)
{
    barrier(DL.out.img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
    NgxImage col = ngx_img(&V.scene, VK_FORMAT_R16G16B16A16_SFLOAT), dep = ngx_img(&V.depth, VK_FORMAT_D32_SFLOAT);
    NgxImage mv = ngx_img(&V.motion, VK_FORMAT_R16G16_SFLOAT), out = ngx_img(&DL.out, VK_FORMAT_R16G16B16A16_SFLOAT);
    bool up;
    if (high) {
        NgxImage al = ngx_img(&DL.albedo, VK_FORMAT_R8G8B8A8_UNORM), sp = ngx_img(&DL.spec, VK_FORMAT_R8G8B8A8_UNORM);
        NgxImage nr = ngx_img(&DL.nrough, VK_FORMAT_R16G16B16A16_SFLOAT);
        up = ngxvk_rr_evaluate(V.cmd, &col, &dep, &mv, &al, &sp, &nr, &out, jx, jy, rw, rh, DL.w2v, DL.v2c, reset);
        if (up) DL.rr_ok++;
        else DL.rr_failed++, up = ngxvk_dlss_evaluate(V.cmd, &col, &dep, &mv, &out, jx, jy, rw, rh, reset);
    } else up = ngxvk_dlss_evaluate(V.cmd, &col, &dep, &mv, &out, jx, jy, rw, rh, reset);
    barrier(DL.out.img, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return up;
}

/* the present set's picture: the scene's colour or DLSS's output (written before the present pass binds the set) */
static void present_source(const Img *src)
{
    VkDescriptorImageInfo im = {VK_NULL_HANDLE, src->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet w = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = V.set, .dstBinding = 16, .descriptorCount = 1,
                              .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .pImageInfo = &im};
    vkUpdateDescriptorSets(V.dev, 1, &w, 0, NULL);
    if (H.shot_set) w.dstSet = H.shot_set, vkUpdateDescriptorSets(V.dev, 1, &w, 0, NULL);   /* (HDR's SDR screenshot) */
}
