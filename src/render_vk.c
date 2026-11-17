/* render_vk.c: the Vulkan renderer (docs\superpowers\plans\2026-11-15-vulkan-renderer.md), beside render.c's D3D12
 * one and behind the same render.h, built into uwview_vk.dll. Milestone M1: the device, the swap chain on the game's
 * window and the present pass (present.hlsl's SPIR-V) showing the interface layer. Milestone M2: the scene without
 * ray tracing (scene.hlsl's SPIR-V, the shader that marches its shadows through the tile grid): the level's mesh, the
 * picture cards, the lights and their flames, the textures and their cycling colours, into colour + motion + depth as
 * render.c's scene pass. The CPU side (the constants, the flames, the black body, the matrices) is render.c's own,
 * copied in render_vk_cpu.h. M3 the effects' passes (render_vk_fx.h), M4 ray tracing (render_vk_rt.h), M5 DLSS and Ray
 * Reconstruction (render_vk_dlss.h, ngx_vk.c), M6 HDR output; only render_dlss5_* (DLSS 5, dropped by the user) and
 * the rfx_* entry points are stubs. M7 Linux: the window's surface from the SDL window the backend gives (X11 or
 * Wayland), libuwview.so (tools\buildview_linux.py).
 *
 * As render.c: one frame at a time (the frame waits for the GPU at its end, as wait_gpu); every target at the
 * output size. The present pass draws into an RGBA8 picture of the output's size (render.c's final_tex), which is
 * copied for render_readback and blitted onto the swap chain's image; the window's own size may differ (a resize, the
 * full-screen display: the window stretches the picture, as DXGI's does), the swap chain follows it.
 *
 * UWVK_VALIDATE=1: the Khronos validation layer (its folder in VK_ADD_LAYER_PATH, e.g. the SDK's
 * O:\appsnotonc\vulkan-sdk\1.4.363.1\Bin); its messages to the log, counted, the count at render_shutdown. */
#include "render.h"
#include "vk_load.h"
#include "ngx_vk.h"

#include "normals.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define COBJMACROS
#include <dxgi1_6.h> /* (HDR: the monitor's colour space, hdr_display) */
#else
/* Linux: the window is SDL's (port\sdl3\plat_sdl3.c's uwp_window); its X11 display and window or its Wayland display
   and surface are its properties. The two surface structs as the Vulkan headers have them, with X11's three types
   declared here (the sysroot has no X11 headers; the struct holds only a pointer and two numbers) */
#include <SDL3/SDL.h>
typedef struct _XDisplay Display;
typedef unsigned long Window;
typedef unsigned long VisualID;
#include <vulkan/vulkan_xlib.h>
#include <vulkan/vulkan_wayland.h>
static bool lx_wayland(void) { const char *d = SDL_GetCurrentVideoDriver(); return d && !strcmp(d, "wayland"); }
#endif

/* present.hlsl as SPIR-V (buildview.cmd: DXC -spirv, b0 at binding 0, tN at 16 + N) */
#include "present_ps_spv.h"
#include "present_vs_spv.h"
/* scene.hlsl the same way (the vertex shader with -fvk-invert-y: Vulkan's clip space has y down) */
#include "scene_ps_spv.h"
#include "scene_vs_spv.h"

/* render.c's GetTickCount64, for its copied code */
static unsigned long long rv_ticks(void)
{
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (unsigned long long)t.tv_sec * 1000ull + (unsigned long long)t.tv_nsec / 1000000ull;
#endif
}
#include "render_vk_cpu.h"
void render_fx_attach(const RenderFxPasses *p) { g_fxp = p; }

typedef struct { VkImage img; VkDeviceMemory mem; VkImageView view; VkImageAspectFlags aspect; } Img;
typedef struct { VkBuffer buf; VkDeviceMemory mem; void *ptr; VkDeviceSize size; } Buf;

static struct {
    RenderConfig cfg;
    bool up;
    VkInstance inst;
    VkDebugUtilsMessengerEXT dbg;
    VkSurfaceKHR surface;
    VkPhysicalDevice phys;
    VkPhysicalDeviceMemoryProperties mp;
    VkDevice dev;
    VkQueue queue;
    uint32_t qfam;
    VkCommandPool pool;
    VkCommandBuffer cmd;
    VkFence fence;
    /* the swap chain: its images, the semaphore an image is acquired with, one per image its picture is done with */
    VkSwapchainKHR sc;
    VkFormat sc_fmt;
    VkExtent2D sc_ext;
    uint32_t sc_n;
    VkImage sc_img[8];
    VkSemaphore sem_acq, sem_done[8];
    bool sc_stale;
    /* the targets */
    Img final_, scene, motion, depth, ui;
    Buf ui_up, cbuf, readback;
    bool ui_dirty, final_ready;
    int ui_w, ui_h, ui_s;
    VkDescriptorSetLayout dsl;
    VkPipelineLayout pl;
    VkPipeline present;
    VkDescriptorPool dpool;
    VkDescriptorSet set;
    /* the scene (M2): its own set (b0; t0 atlas, t1 normal atlas, t2 grid, t9, t11, t12 at 16 + N; t6 lights, t7 the
       tiles' lists as storage buffers), its pipeline; a level's mesh and textures (render_upload) */
    VkDescriptorSetLayout sdsl;
    VkPipelineLayout spl;
    VkPipeline scene_pipe;
    VkDescriptorSet sset;
    Img atlas, natlas, snatlas, saoatlas, grid, hatlas;
    int hatlas_f;
    bool hatlas_maps;   /* its own relief maps in its right half (render_set_hires) */
    bool hatlas_colours; /* the upscaled colours in its left half (off: only the maps are its own) */
    Buf vbuf, ibuf, sprite_vb, lights, tiles, atlas_up;
    int vertices, indices, nsprites;
    bool level;                 /* a level is uploaded: the scene's set is whole, the scene can be drawn */
    int atlas_rows;             /* > 0: the atlas's first rows wait in atlas_up for the next frame */
    Light placed[MAX_LIGHTS];
    int nplaced;
    float prev_vp[16];
    bool have_prev;
    unsigned frame;
    char gpu[256], status[512];
} V;

static int g_vk_messages;          /* the validation layer's warnings and errors, this run */
static bool g_ngx;                 /* the driver's NGX is loaded (DLSS): its extensions wanted (render_init) */
static bool g_rt_dev;              /* the device has acceleration structures and ray queries (start_device) */
static bool g_want_shot;

/* ---------------------------------------------------------------- render.h's settings (below) */
static float g_hdr_peak, g_hdr_paper = 200;
static bool g_card_shadows = true, g_hdr_want = true, g_rt_high;
/* HDR output (M6, render.c's hdr_display / hdr_apply / hdr_follow): the swap chain FP16 in scRGB (extended sRGB linear,
   as DXGI's G10 P709) while it is wanted and the monitor runs HDR; the present pass then draws into pic (FP16) and a
   screenshot is drawn again as SDR into final_ with the constants' copy at CB_SHOT_OFF (shot_set) */
#define CB_SHOT_OFF 2048
static struct {
    bool cs_ext;          /* the instance has VK_EXT_swapchain_colorspace */
    bool want;            /* asked of the next swap chain */
    bool on;              /* the swap chain is HDR now */
    bool pic_ready;
    bool refused;         /* the surface offered no FP16 scRGB format: not asked again until the setting or the
                             monitor's answer changes (fable uwr's review, 2026-11-16) */
    bool disp_last;       /* the monitor's last answer */
    int check;            /* frames to the next look at the monitor */
    float display_peak;   /* what the monitor says it reaches (nits) */
    Img pic;
    VkPipeline pipe;
    VkDescriptorSet shot_set;
} H = {.display_peak = 1000};

static VKAPI_ATTR VkBool32 VKAPI_CALL on_message(VkDebugUtilsMessageSeverityFlagBitsEXT sev, VkDebugUtilsMessageTypeFlagsEXT type,
                                                 const VkDebugUtilsMessengerCallbackDataEXT *d, void *user)
{
    (void)user;
    if (sev < VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) return VK_FALSE;
    const char *m = d && d->pMessage ? d->pMessage : "?";
    if (!(type & (VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT))) {
        /* the loader's own (the machine's other layers: an overlay's name, a layer listed twice, one that failed to
           load): not the renderer's use of Vulkan, so not counted */
        logf_("render: Vulkan loader: %s\n", m);
        return VK_FALSE;
    }
    g_vk_messages++;
    logf_("render: Vulkan %s: %s\n", sev >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ? "error" : "warning", m);
    return VK_FALSE;
}

static uint32_t mem_type(uint32_t bits, VkMemoryPropertyFlags want)
{
    for (uint32_t i = 0; i < V.mp.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (V.mp.memoryTypes[i].propertyFlags & want) == want) return i;
    return UINT32_MAX;
}

static bool make_buf(Buf *b, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkMemoryPropertyFlags nice)
{
    VkBufferCreateInfo bi = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    if (vkCreateBuffer(V.dev, &bi, NULL, &b->buf) != VK_SUCCESS) return false;
    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(V.dev, b->buf, &mr);
    uint32_t t = mem_type(mr.memoryTypeBits, props | nice);
    if (t == UINT32_MAX) t = mem_type(mr.memoryTypeBits, props);
    if (t == UINT32_MAX) return false;
    VkMemoryAllocateFlagsInfo af = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT};
    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .pNext = usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT ? &af : NULL,
                               .allocationSize = mr.size, .memoryTypeIndex = t};
    if (vkAllocateMemory(V.dev, &ai, NULL, &b->mem) != VK_SUCCESS) return false;
    vkBindBufferMemory(V.dev, b->buf, b->mem, 0);
    b->size = size;
    if (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) vkMapMemory(V.dev, b->mem, 0, VK_WHOLE_SIZE, 0, &b->ptr);
    return true;
}

static void free_buf(Buf *b)
{
    if (b->ptr) vkUnmapMemory(V.dev, b->mem);
    if (b->buf) vkDestroyBuffer(V.dev, b->buf, NULL);
    if (b->mem) vkFreeMemory(V.dev, b->mem, NULL);
    memset(b, 0, sizeof *b);
}

static bool make_img_a(Img *m, int w, int h, VkFormat fmt, VkImageUsageFlags usage, VkImageAspectFlags aspect)
{
    m->aspect = aspect;
    VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D, .format = fmt,
                            .extent = {(uint32_t)w, (uint32_t)h, 1}, .mipLevels = 1, .arrayLayers = 1,
                            .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL, .usage = usage,
                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
    if (vkCreateImage(V.dev, &ii, NULL, &m->img) != VK_SUCCESS) return false;
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(V.dev, m->img, &mr);
    uint32_t t = mem_type(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (t == UINT32_MAX) return false;
    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = t};
    if (vkAllocateMemory(V.dev, &ai, NULL, &m->mem) != VK_SUCCESS) return false;
    vkBindImageMemory(V.dev, m->img, m->mem, 0);
    VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = m->img, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = fmt,
                                .subresourceRange = {aspect, 0, 1, 0, 1}};
    return vkCreateImageView(V.dev, &vi, NULL, &m->view) == VK_SUCCESS;
}
static bool make_img(Img *m, int w, int h, VkFormat fmt, VkImageUsageFlags usage)
{
    return make_img_a(m, w, h, fmt, usage, VK_IMAGE_ASPECT_COLOR_BIT);
}

static void free_img(Img *m)
{
    if (m->view) vkDestroyImageView(V.dev, m->view, NULL);
    if (m->img) vkDestroyImage(V.dev, m->img, NULL);
    if (m->mem) vkFreeMemory(V.dev, m->mem, NULL);
    memset(m, 0, sizeof *m);
}

/* an image's layout changed (synchronization2: everything before, everything after; one frame's few barriers) */
static void barrier_a(VkImage img, VkImageAspectFlags aspect, VkImageLayout from, VkImageLayout to)
{
    VkImageMemoryBarrier2 b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                               .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                               .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                               .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                               .oldLayout = from, .newLayout = to,
                               .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                               .image = img, .subresourceRange = {aspect, 0, 1, 0, 1}};
    VkDependencyInfo di = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &b};
    vkCmdPipelineBarrier2(V.cmd, &di);
}
static void barrier(VkImage img, VkImageLayout from, VkImageLayout to) { barrier_a(img, VK_IMAGE_ASPECT_COLOR_BIT, from, to); }

static void begin_cmd(void)
{
    vkResetCommandPool(V.dev, V.pool, 0);
    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    vkBeginCommandBuffer(V.cmd, &bi);
}

/* the commands to the GPU, and waited for (render.c's wait_gpu); wait / signal: the swap chain's semaphores or none */
static bool submit_wait(VkSemaphore wait, VkSemaphore signal)
{
    vkEndCommandBuffer(V.cmd);
    VkPipelineStageFlags ws = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO, .waitSemaphoreCount = wait ? 1u : 0u, .pWaitSemaphores = &wait,
                       .pWaitDstStageMask = &ws, .commandBufferCount = 1, .pCommandBuffers = &V.cmd,
                       .signalSemaphoreCount = signal ? 1u : 0u, .pSignalSemaphores = &signal};
    vkResetFences(V.dev, 1, &V.fence);
    VkResult r = vkQueueSubmit(V.queue, 1, &si, V.fence);
    if (r != VK_SUCCESS) return logf_("render: Vulkan submit failed (%d)\n", (int)r), false;
    vkWaitForFences(V.dev, 1, &V.fence, VK_TRUE, UINT64_MAX);
    return true;
}

static VkShaderModule shader(const unsigned char *code, size_t n)
{
    uint32_t *words = malloc(n);   /* (the header's bytes need not be 4-aligned) */
    if (!words) return VK_NULL_HANDLE;
    memcpy(words, code, n);
    VkShaderModuleCreateInfo ci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = n, .pCode = words};
    VkShaderModule m = VK_NULL_HANDLE;
    vkCreateShaderModule(V.dev, &ci, NULL, &m);
    free(words);
    return m;
}

/* ---------------------------------------------------------------- the swap chain */
static void drop_swapchain(VkSwapchainKHR keep_old)
{
    for (uint32_t i = 0; i < V.sc_n; i++)
        if (V.sem_done[i]) vkDestroySemaphore(V.dev, V.sem_done[i], NULL), V.sem_done[i] = VK_NULL_HANDLE;
    if (V.sc && V.sc != keep_old) vkDestroySwapchainKHR(V.dev, V.sc, NULL);
    V.sc = VK_NULL_HANDLE, V.sc_n = 0;
}

/* made (or made again: out of date, the window's size changed) at the window's size now; false: none now (minimised) */
static bool make_swapchain(void)
{
    if (!V.surface) return false;
    vkDeviceWaitIdle(V.dev);
    VkSurfaceCapabilitiesKHR caps;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(V.phys, V.surface, &caps) != VK_SUCCESS) return false;
    VkExtent2D ext = caps.currentExtent;
    if (ext.width == 0xFFFFFFFFu) ext.width = (uint32_t)V.cfg.w, ext.height = (uint32_t)V.cfg.h;
    VkSwapchainKHR old = V.sc;
    if (ext.width == 0 || ext.height == 0) {   /* minimised: none until it comes back */
        drop_swapchain(VK_NULL_HANDLE);
        V.sc_stale = true;
        return false;
    }
    uint32_t nf = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(V.phys, V.surface, &nf, NULL);
    VkSurfaceFormatKHR fm[64];
    if (nf > 64) nf = 64;
    vkGetPhysicalDeviceSurfaceFormatsKHR(V.phys, V.surface, &nf, fm);
    VkSurfaceFormatKHR f = nf ? fm[0] : (VkSurfaceFormatKHR){VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    bool hdr = false;
    for (uint32_t i = 0; i < nf && H.want && H.cs_ext && H.pipe; i++)   /* HDR: FP16 scRGB, as DXGI's R16G16B16A16_FLOAT G10 P709 */
        if (fm[i].format == VK_FORMAT_R16G16B16A16_SFLOAT && fm[i].colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT) { f = fm[i], hdr = true; break; }
    if (H.want && !hdr && !H.refused) logf_("render: HDR: the surface offers no FP16 scRGB format (SDR kept)\n");
    H.refused = H.want && !hdr;
    for (uint32_t i = 0; i < nf && !hdr; i++)   /* 8-bit UNORM as DXGI's R8G8B8A8_UNORM back buffer (no sRGB view: the shader's gamma) */
        if ((fm[i].format == VK_FORMAT_B8G8R8A8_UNORM || fm[i].format == VK_FORMAT_R8G8B8A8_UNORM) &&
            fm[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) { f = fm[i]; break; }
    uint32_t np = 0;
    VkPresentModeKHR pm[16], mode = VK_PRESENT_MODE_FIFO_KHR;
    vkGetPhysicalDeviceSurfacePresentModesKHR(V.phys, V.surface, &np, NULL);
    if (np > 16) np = 16;
    vkGetPhysicalDeviceSurfacePresentModesKHR(V.phys, V.surface, &np, pm);
    if (!V.cfg.vsync)
        for (uint32_t i = 0; i < np; i++) {
            if (pm[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) { mode = pm[i]; break; }
            if (pm[i] == VK_PRESENT_MODE_MAILBOX_KHR) mode = pm[i];
        }
    uint32_t n = caps.minImageCount > 2 ? caps.minImageCount : 2;
    if (caps.maxImageCount && n > caps.maxImageCount) n = caps.maxImageCount;
    if (n > 8) n = 8;   /* (sc_img and sem_done hold 8; a driver may still make more: see below) */
    VkSwapchainCreateInfoKHR ci = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, .surface = V.surface, .minImageCount = n,
                                   .imageFormat = f.format, .imageColorSpace = f.colorSpace, .imageExtent = ext, .imageArrayLayers = 1,
                                   .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                   .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, .preTransform = caps.currentTransform,
                                   .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, .presentMode = mode, .clipped = VK_TRUE,
                                   .oldSwapchain = old};
    if (!(caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)) ci.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    VkSwapchainKHR sc = VK_NULL_HANDLE;
    VkResult r = vkCreateSwapchainKHR(V.dev, &ci, NULL, &sc);
    drop_swapchain(VK_NULL_HANDLE);   /* the old one (retired by the new one's oldSwapchain) */
    if (r != VK_SUCCESS) return logf_("render: Vulkan swap chain failed (%d)\n", (int)r), V.sc_stale = true, false;
    V.sc = sc, V.sc_fmt = f.format, V.sc_ext = ext;
    if (hdr != H.on) logf_("render: HDR output %s\n", hdr ? "on (FP16 scRGB)" : "off");
    H.on = hdr;
    uint32_t made = 0;
    vkGetSwapchainImagesKHR(V.dev, V.sc, &made, NULL);
    if (made > 8) logf_("render: Vulkan: the driver made %u swap chain images; the first 8 are used, a frame given another shows nothing\n", made);
    V.sc_n = 8;
    vkGetSwapchainImagesKHR(V.dev, V.sc, &V.sc_n, V.sc_img);
    VkSemaphoreCreateInfo sci = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (uint32_t i = 0; i < V.sc_n; i++) vkCreateSemaphore(V.dev, &sci, NULL, &V.sem_done[i]);
    V.sc_stale = false;
    logf_("render: Vulkan swap chain %ux%u, %u images (the picture %dx%d%s)\n", ext.width, ext.height, V.sc_n, V.cfg.w, V.cfg.h,
          ext.width == (uint32_t)V.cfg.w && ext.height == (uint32_t)V.cfg.h ? "" : ", stretched");
    return true;
}

/* ---------------------------------------------------------------- start and stop */
static bool has_ext(const VkExtensionProperties *e, uint32_t n, const char *name)
{
    for (uint32_t i = 0; i < n; i++)
        if (!strcmp(e[i].extensionName, name)) return true;
    return false;
}

static bool start_instance(void)
{
    if (!vkl_open()) return logf_("render: no Vulkan (%s)\n", vkl_missing()), false;
    uint32_t api = VK_API_VERSION_1_0;
    vkEnumerateInstanceVersion(&api);
    if (api < VK_API_VERSION_1_3) return logf_("render: Vulkan %u.%u here, 1.3 needed\n", VK_API_VERSION_MAJOR(api), VK_API_VERSION_MINOR(api)), false;
    const char *ext[16];
    uint32_t ne = 0;
    uint32_t navail = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &navail, NULL);
    VkExtensionProperties *avail = calloc(navail ? navail : 1, sizeof *avail);
    if (!avail) return false;
    vkEnumerateInstanceExtensionProperties(NULL, &navail, avail);
    if (V.cfg.hwnd) {
        ext[ne++] = VK_KHR_SURFACE_EXTENSION_NAME;
#ifdef _WIN32
        ext[ne++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#else
        ext[ne++] = lx_wayland() ? VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME : VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
#endif
    }
    const char *validate = getenv("UWVK_VALIDATE");
    bool want_val = validate && *validate && *validate != '0', val = false;
    const char *layer = "VK_LAYER_KHRONOS_validation";
    if (want_val) {
        uint32_t nl = 0;
        vkEnumerateInstanceLayerProperties(&nl, NULL);
        VkLayerProperties *lp = calloc(nl ? nl : 1, sizeof *lp);
        if (lp) {
            vkEnumerateInstanceLayerProperties(&nl, lp);
            for (uint32_t i = 0; i < nl; i++) val = val || !strcmp(lp[i].layerName, layer);
            free(lp);
        }
        if (val && has_ext(avail, navail, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) ext[ne++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        logf_("render: Vulkan validation %s\n", val ? "on (VK_LAYER_KHRONOS_validation)" : "asked for, but the layer is not found (VK_ADD_LAYER_PATH)");
    }
    if (g_ngx) {   /* NGX's (DLSS) instance extensions, each the driver offers */
        const char *const *ni, *const *nd;
        unsigned nn = 0, ndv = 0;
        ngxvk_extensions(&ni, &nn, &nd, &ndv);
        for (unsigned i = 0; i < nn && ne < 16; i++) {
            bool dup = false;
            for (uint32_t k = 0; k < ne; k++) dup = dup || !strcmp(ext[k], ni[i]);
            if (dup) continue;
            if (has_ext(avail, navail, ni[i])) ext[ne++] = ni[i];
            else logf_("render: DLSS asks for the instance extension %s: not offered\n", ni[i]);
        }
    }
    H.cs_ext = has_ext(avail, navail, VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME) && ne < 16;
    if (H.cs_ext) ext[ne++] = VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME;   /* (HDR: scRGB's colour space) */
    for (uint32_t i = 0; i < ne; i++)
        if (!has_ext(avail, navail, ext[i]) && strcmp(ext[i], VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
            return free(avail), logf_("render: Vulkan without %s\n", ext[i]), false;
    free(avail);
    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "uwr", .applicationVersion = 1,
                             .pEngineName = "uwview", .engineVersion = 1, .apiVersion = VK_API_VERSION_1_3};
    VkDebugUtilsMessengerCreateInfoEXT dci = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                                              .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                                              .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                                              .pfnUserCallback = on_message};
    VkInstanceCreateInfo ci = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pNext = val ? &dci : NULL, .pApplicationInfo = &app,
                               .enabledLayerCount = val ? 1u : 0u, .ppEnabledLayerNames = &layer,
                               .enabledExtensionCount = ne, .ppEnabledExtensionNames = ext};
    VkResult r = vkCreateInstance(&ci, NULL, &V.inst);
    if (r != VK_SUCCESS) return logf_("render: Vulkan instance failed (%d)\n", (int)r), false;
    if (!vkl_instance(V.inst)) return logf_("render: Vulkan without %s\n", vkl_missing()), false;
    if (val && vkCreateDebugUtilsMessengerEXT) vkCreateDebugUtilsMessengerEXT(V.inst, &dci, NULL, &V.dbg);
    return true;
}

/* the driver's version as its maker writes it (NVIDIA: 10 bits . 8 . 8 . 6) */
static void driver_text(const VkPhysicalDeviceProperties *p, char *out, size_t n)
{
    uint32_t v = p->driverVersion;
    if (p->vendorID == 0x10DE) snprintf(out, n, "%u.%u", (v >> 22) & 0x3FF, (v >> 14) & 0xFF);
    else snprintf(out, n, "%u.%u.%u", VK_API_VERSION_MAJOR(v), VK_API_VERSION_MINOR(v), VK_API_VERSION_PATCH(v));
}

static bool start_device(void)
{
#ifdef _WIN32
    if (V.cfg.hwnd) {
        VkWin32SurfaceCreateInfoKHR si = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR, .hinstance = GetModuleHandleW(NULL), .hwnd = (HWND)V.cfg.hwnd};
        if (vkCreateWin32SurfaceKHR(V.inst, &si, NULL, &V.surface) != VK_SUCCESS) return logf_("render: Vulkan surface failed\n"), false;
    }
#else
    if (V.cfg.hwnd) {
        SDL_PropertiesID pr = SDL_GetWindowProperties((SDL_Window *)V.cfg.hwnd);
        VkResult r = VK_ERROR_INITIALIZATION_FAILED;
        if (lx_wayland()) {
            PFN_vkCreateWaylandSurfaceKHR mk = (PFN_vkCreateWaylandSurfaceKHR)vkGetInstanceProcAddr(V.inst, "vkCreateWaylandSurfaceKHR");
            VkWaylandSurfaceCreateInfoKHR si = {VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
                                                .display = SDL_GetPointerProperty(pr, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL),
                                                .surface = SDL_GetPointerProperty(pr, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL)};
            if (mk && si.display && si.surface) r = mk(V.inst, &si, NULL, &V.surface);
        } else {
            PFN_vkCreateXlibSurfaceKHR mk = (PFN_vkCreateXlibSurfaceKHR)vkGetInstanceProcAddr(V.inst, "vkCreateXlibSurfaceKHR");
            VkXlibSurfaceCreateInfoKHR si = {VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
                                             .dpy = SDL_GetPointerProperty(pr, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL),
                                             .window = (Window)SDL_GetNumberProperty(pr, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0)};
            if (mk && si.dpy && si.window) r = mk(V.inst, &si, NULL, &V.surface);
        }
        if (r != VK_SUCCESS) return logf_("render: Vulkan surface failed (%s, %d)\n", lx_wayland() ? "Wayland" : "X11", (int)r), false;
    }
#endif
    uint32_t n = 0;
    vkEnumeratePhysicalDevices(V.inst, &n, NULL);
    VkPhysicalDevice pd[16];
    if (n > 16) n = 16;
    vkEnumeratePhysicalDevices(V.inst, &n, pd);
    int best = -1, best_score = -1;
    uint32_t best_q = 0;
    for (uint32_t i = 0; i < n; i++) {
        VkPhysicalDeviceProperties p;
        vkGetPhysicalDeviceProperties(pd[i], &p);
        if (p.apiVersion < VK_API_VERSION_1_3) continue;
        uint32_t nq = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd[i], &nq, NULL);
        VkQueueFamilyProperties qp[32];
        if (nq > 32) nq = 32;
        vkGetPhysicalDeviceQueueFamilyProperties(pd[i], &nq, qp);
        for (uint32_t q = 0; q < nq; q++) {
            VkBool32 pres = VK_TRUE;
            if (V.surface) vkGetPhysicalDeviceSurfaceSupportKHR(pd[i], q, V.surface, &pres);
            if (!(qp[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) || !pres) continue;
            int score = p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 3 : p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2 : 1;
            if (score > best_score) best = (int)i, best_score = score, best_q = q;
            break;
        }
    }
    if (best < 0) return logf_("render: no Vulkan 1.3 GPU that can draw into the window\n"), false;
    V.phys = pd[best], V.qfam = best_q;
    VkPhysicalDeviceProperties p;
    vkGetPhysicalDeviceProperties(V.phys, &p);
    vkGetPhysicalDeviceMemoryProperties(V.phys, &V.mp);
    char drv[64];
    driver_text(&p, drv, sizeof drv);
    snprintf(V.gpu, sizeof V.gpu, "%s", p.deviceName);
    logf_("render: Vulkan: %s (driver %s, Vulkan %u.%u.%u)\n", p.deviceName, drv, VK_API_VERSION_MAJOR(p.apiVersion),
          VK_API_VERSION_MINOR(p.apiVersion), VK_API_VERSION_PATCH(p.apiVersion));
    float prio = 1;
    VkDeviceQueueCreateInfo qi = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = V.qfam, .queueCount = 1, .pQueuePriorities = &prio};
    /* shaderDemoteToHelperInvocation: HLSL's discard (the cards' clear texels) as D3D12 has it, the derivatives kept.
       Reads outside a texture or a buffer: D3D12 gives 0, Vulkan leaves them undefined, so the robust reads are asked
       for where the device has them (robustBufferAccess; VK_EXT_robustness2's robustBufferAccess2 / robustImageAccess2),
       and the two renderers agree at the edges */
    VkPhysicalDeviceRobustness2FeaturesEXT r2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
    VkPhysicalDeviceVulkan13Features have13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceFeatures2 have = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &have13};
    uint32_t nde = 0;
    vkEnumerateDeviceExtensionProperties(V.phys, NULL, &nde, NULL);
    VkExtensionProperties *de = calloc(nde ? nde : 1, sizeof *de);
    bool ext_r2 = false;
    const char *ngx_dext[8];
    unsigned n_ngx_dext = 0;
    if (de) {
        vkEnumerateDeviceExtensionProperties(V.phys, NULL, &nde, de);
        ext_r2 = has_ext(de, nde, VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);
        g_rt_dev = has_ext(de, nde, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) && has_ext(de, nde, VK_KHR_RAY_QUERY_EXTENSION_NAME) &&
                   has_ext(de, nde, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        if (g_ngx) {   /* NGX's (DLSS) device extensions, each the driver offers */
            const char *const *ni, *const *nd;
            unsigned nn = 0, ndv = 0;
            ngxvk_extensions(&ni, &nn, &nd, &ndv);
            for (unsigned i = 0; i < ndv && n_ngx_dext < 8; i++)
                if (has_ext(de, nde, nd[i])) ngx_dext[n_ngx_dext++] = nd[i];
                else logf_("render: DLSS asks for the device extension %s: not offered\n", nd[i]);
        }
        free(de);
    }
    if (ext_r2) have13.pNext = &r2;
    vkGetPhysicalDeviceFeatures2(V.phys, &have);
    if (!have13.synchronization2 || !have13.dynamicRendering || !have13.shaderDemoteToHelperInvocation)
        return logf_("render: this GPU's Vulkan lacks what the renderer needs (synchronization2, dynamic rendering, demote to helper)\n"), false;
    bool rb = have.features.robustBufferAccess, rb2 = ext_r2 && r2.robustBufferAccess2 && rb, ri2 = ext_r2 && r2.robustImageAccess2;
    logf_("render: Vulkan robust reads: buffers %s, images %s\n", rb2 ? "yes (robustBufferAccess2)" : rb ? "yes (robustBufferAccess)" : "NO",
          ri2 ? "yes (robustImageAccess2)" : "NO (a read outside a texture is undefined here, 0 with Direct3D 12)");
    VkPhysicalDeviceRobustness2FeaturesEXT want_r2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
                                                      .robustBufferAccess2 = rb2, .robustImageAccess2 = ri2};
    VkPhysicalDeviceVulkan13Features f13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .pNext = rb2 || ri2 ? &want_r2 : NULL,
                                            .synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE, .shaderDemoteToHelperInvocation = VK_TRUE};
    VkPhysicalDeviceFeatures2 f2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &f13, .features = {.robustBufferAccess = rb}};
    /* ray tracing (M4, render.c's DXR 1.1 inline RayQuery): acceleration structures, ray queries, buffer addresses */
    VkPhysicalDeviceRayQueryFeaturesKHR hrq = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR has = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR, .pNext = &hrq};
    VkPhysicalDeviceVulkan12Features h12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .pNext = &has};
    VkPhysicalDeviceFeatures2 hrt = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &h12};
    vkGetPhysicalDeviceFeatures2(V.phys, &hrt);
    /* scalarBlockLayout: the structured buffers' structs as D3D12 packs them (-fvk-use-dx-layout: Vtx's float3 at 12) */
    g_rt_dev = g_rt_dev && has.accelerationStructure && hrq.rayQuery && h12.bufferDeviceAddress && h12.scalarBlockLayout;
    logf_("render: Vulkan ray tracing: %s\n", g_rt_dev ? "yes (acceleration structures, ray queries)" : "NO (shadows through the tile grid)");
    VkPhysicalDeviceRayQueryFeaturesKHR want_rq = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR, .pNext = f13.pNext, .rayQuery = VK_TRUE};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR want_as = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
                                                                .pNext = &want_rq, .accelerationStructure = VK_TRUE};
    VkPhysicalDeviceVulkan12Features want12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .pNext = g_rt_dev ? (void *)&want_as : f13.pNext,
                                               .bufferDeviceAddress = h12.bufferDeviceAddress, .scalarBlockLayout = h12.scalarBlockLayout};
    if (g_rt_dev || n_ngx_dext) f13.pNext = &want12;   /* (DLSS: buffer addresses as NGX's extension list has them) */
    const char *dext[16];
    uint32_t ndext = 0;
    if (V.surface) dext[ndext++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    if (rb2 || ri2) dext[ndext++] = VK_EXT_ROBUSTNESS_2_EXTENSION_NAME;
    if (g_rt_dev) {
        dext[ndext++] = VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, dext[ndext++] = VK_KHR_RAY_QUERY_EXTENSION_NAME;
        dext[ndext++] = VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME;
    }
    for (unsigned i = 0; i < n_ngx_dext; i++) {
        bool dup = false;
        for (uint32_t k = 0; k < ndext; k++) dup = dup || !strcmp(dext[k], ngx_dext[i]);
        if (!dup) dext[ndext++] = ngx_dext[i];
    }
    VkDeviceCreateInfo di = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .pNext = &f2, .queueCreateInfoCount = 1, .pQueueCreateInfos = &qi,
                             .enabledExtensionCount = ndext, .ppEnabledExtensionNames = dext};
    VkResult r = vkCreateDevice(V.phys, &di, NULL, &V.dev);
    if (r != VK_SUCCESS) return logf_("render: Vulkan device failed (%d)\n", (int)r), false;
    if (!vkl_device(V.dev)) return logf_("render: Vulkan device without %s\n", vkl_missing()), false;
    vkGetDeviceQueue(V.dev, V.qfam, 0, &V.queue);
    VkCommandPoolCreateInfo pi = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .queueFamilyIndex = V.qfam};
    vkCreateCommandPool(V.dev, &pi, NULL, &V.pool);
    VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = V.pool,
                                      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1};
    vkAllocateCommandBuffers(V.dev, &ai, &V.cmd);
    VkFenceCreateInfo fi = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    vkCreateFence(V.dev, &fi, NULL, &V.fence);
    VkSemaphoreCreateInfo sci = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    vkCreateSemaphore(V.dev, &sci, NULL, &V.sem_acq);
    return true;
}

static bool start_present(void)
{
    VkDescriptorSetLayoutBinding b[4] = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
        {16, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
        {17, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
        {18, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
    };
    VkDescriptorSetLayoutCreateInfo li = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 4, .pBindings = b};
    if (vkCreateDescriptorSetLayout(V.dev, &li, NULL, &V.dsl) != VK_SUCCESS) return false;
    VkPipelineLayoutCreateInfo pli = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &V.dsl};
    if (vkCreatePipelineLayout(V.dev, &pli, NULL, &V.pl) != VK_SUCCESS) return false;
    /* (the pool: this set and the scene's, start_scene) */
    /* (the pool: this set, the scene's and the probes', start_scene and rt_start) */
    VkDescriptorPoolSize ps[4] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4}, {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 20}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16},
                                  {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 2}};
    VkDescriptorPoolCreateInfo dpi = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 4, .poolSizeCount = g_rt_dev ? 4u : 3u, .pPoolSizes = ps};
    if (vkCreateDescriptorPool(V.dev, &dpi, NULL, &V.dpool) != VK_SUCCESS) return false;
    VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = V.dpool, .descriptorSetCount = 1, .pSetLayouts = &V.dsl};
    if (vkAllocateDescriptorSets(V.dev, &dai, &V.set) != VK_SUCCESS) return false;
    VkDescriptorBufferInfo cb = {V.cbuf.buf, 0, sizeof(Constants)};
    VkDescriptorImageInfo im[3] = {{VK_NULL_HANDLE, V.scene.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                                   {VK_NULL_HANDLE, V.depth.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                                   {VK_NULL_HANDLE, V.ui.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
    VkWriteDescriptorSet w[4] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = V.set, .dstBinding = 0, .descriptorCount = 1,
                                  .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &cb}};
    for (int i = 0; i < 3; i++)
        w[1 + i] = (VkWriteDescriptorSet){VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = V.set, .dstBinding = 16u + (uint32_t)i,
                                          .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .pImageInfo = &im[i]};
    vkUpdateDescriptorSets(V.dev, 4, w, 0, NULL);

    VkShaderModule vs = shader(PRESENT_VS_SPV, sizeof PRESENT_VS_SPV), fs = shader(PRESENT_PS_SPV, sizeof PRESENT_PS_SPV);
    if (!vs || !fs) return false;
    VkPipelineShaderStageCreateInfo st[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs, .pName = "vs"},
                                             {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs, .pName = "ps"}};
    VkPipelineVertexInputStateCreateInfo vi = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo ia = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkPipelineViewportStateCreateInfo vp = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
    VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .polygonMode = VK_POLYGON_MODE_FILL,
                                                 .cullMode = VK_CULL_MODE_NONE, .frontFace = VK_FRONT_FACE_CLOCKWISE, .lineWidth = 1};
    VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
    VkPipelineColorBlendAttachmentState ba = {.colorWriteMask = 0xF};
    VkPipelineColorBlendStateCreateInfo cbs = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &ba};
    VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo ds = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 2, .pDynamicStates = dyn};
    VkFormat out_fmt = VK_FORMAT_R8G8B8A8_UNORM;
    VkPipelineRenderingCreateInfo ri = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .colorAttachmentCount = 1, .pColorAttachmentFormats = &out_fmt};
    VkGraphicsPipelineCreateInfo gi = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .pNext = &ri, .stageCount = 2, .pStages = st,
                                       .pVertexInputState = &vi, .pInputAssemblyState = &ia, .pViewportState = &vp, .pRasterizationState = &rs,
                                       .pMultisampleState = &ms, .pColorBlendState = &cbs, .pDynamicState = &ds, .layout = V.pl};
    VkResult r = vkCreateGraphicsPipelines(V.dev, VK_NULL_HANDLE, 1, &gi, NULL, &V.present);
    out_fmt = VK_FORMAT_R16G16B16A16_SFLOAT;   /* (HDR: the same pass into the FP16 picture; render.c's present_hdr_pso) */
    if (r == VK_SUCCESS && vkCreateGraphicsPipelines(V.dev, VK_NULL_HANDLE, 1, &gi, NULL, &H.pipe) != VK_SUCCESS) H.pipe = VK_NULL_HANDLE;
    vkDestroyShaderModule(V.dev, vs, NULL);
    vkDestroyShaderModule(V.dev, fs, NULL);
    if (r == VK_SUCCESS && H.pipe && vkAllocateDescriptorSets(V.dev, &dai, &H.shot_set) == VK_SUCCESS) {
        VkDescriptorBufferInfo cb2 = {V.cbuf.buf, CB_SHOT_OFF, sizeof(Constants)};
        for (int i = 0; i < 4; i++) w[i].dstSet = H.shot_set;
        w[0].pBufferInfo = &cb2;
        vkUpdateDescriptorSets(V.dev, 4, w, 0, NULL);
    } else if (H.pipe) vkDestroyPipeline(V.dev, H.pipe, NULL), H.pipe = VK_NULL_HANDLE;
    return r == VK_SUCCESS;
}

/* ---------------------------------------------------------------- the effects' passes (M3) */
#include "render_vk_fx.h"

/* ---------------------------------------------------------------- ray tracing (M4) */
#include "render_vk_rt.h"

/* ---------------------------------------------------------------- DLSS (M5) */
#include "render_vk_dlss.h"

/* ---------------------------------------------------------------- the scene (M2) */
/* a texture from the CPU (bpt bytes a texel), copied in the command buffer now open; stage: its upload buffer, freed
   by the caller once the copy has run. The image is left for the shaders to read. */
static bool upload_tex(Img *m, int w, int h, VkFormat fmt, const u8 *px, int bpt, Buf *stage)
{
    const VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    size_t n = (size_t)w * (size_t)h * (size_t)bpt;
    if (!make_img(m, w, h, fmt, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT) ||
        !make_buf(stage, n, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, host, 0))
        return logf_("render: out of GPU memory\n"), false;
    memcpy(stage->ptr, px, n);
    barrier(m->img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy bc = {.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, .imageExtent = {(uint32_t)w, (uint32_t)h, 1}};
    vkCmdCopyBufferToImage(V.cmd, stage->buf, m->img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bc);
    barrier(m->img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return true;
}

/* a buffer on the GPU filled from the CPU (the level's vertices and their numbers), the same way */
static bool upload_buf(Buf *b, const void *data, size_t n, size_t size, VkBufferUsageFlags usage, Buf *stage)
{
    const VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    if (!make_buf(b, size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0) ||
        !make_buf(stage, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, host, 0))
        return logf_("render: out of GPU memory\n"), false;
    memset(stage->ptr, 0, size);
    memcpy(stage->ptr, data, n);
    VkBufferCopy r = {0, 0, size};
    vkCmdCopyBuffer(V.cmd, stage->buf, b->buf, 1, &r);
    return true;
}

/* the scene's set written whole (a level's textures are all there: render_upload, render_set_hires) */
static void scene_bind(void)
{
    static const uint32_t TB[6] = {16, 17, 18, 25, 27, 28};   /* t0 t1 t2 t9 t11 t12 */
    const Img *ti[6] = {&V.atlas, &V.natlas, &V.grid, &V.snatlas, &V.saoatlas, &V.hatlas};
    VkDescriptorBufferInfo cb = {V.cbuf.buf, 0, sizeof(Constants)}, lb = {V.lights.buf, 0, VK_WHOLE_SIZE}, tb = {V.tiles.buf, 0, VK_WHOLE_SIZE};
    VkDescriptorImageInfo im[6];
    VkWriteDescriptorSet w[9] = {
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = V.sset, .dstBinding = 0, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &cb},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = V.sset, .dstBinding = 22, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &lb},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = V.sset, .dstBinding = 23, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &tb}};
    for (int i = 0; i < 6; i++) {
        im[i] = (VkDescriptorImageInfo){VK_NULL_HANDLE, ti[i]->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        w[3 + i] = (VkWriteDescriptorSet){VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = V.sset, .dstBinding = TB[i], .descriptorCount = 1,
                                          .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .pImageInfo = &im[i]};
    }
    vkUpdateDescriptorSets(V.dev, 9, w, 0, NULL);
    if (T.pset) {   /* (the probes' pass reads the same) */
        for (int i = 0; i < 9; i++) w[i].dstSet = T.pset;
        vkUpdateDescriptorSets(V.dev, 9, w, 0, NULL);
    }
}

/* the scene's set and pipeline: render.c's make_pso(scene 2) and its root signature's part the shader without ray
   tracing uses. Vertex: render.c's layout (pos, prev, uv, rect, normal, colour: 76 bytes); depth tested LESS and
   written; no culling; colour R16G16B16A16F + motion R16G16F, depth D32F. */
static VkPipeline scene_pipe_make(const unsigned char *fsc, size_t fsn, uint32_t targets);
static bool start_scene(void)
{
    static const uint32_t TB[6] = {16, 17, 18, 25, 27, 28};
    const VkShaderStageFlags all = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutBinding b[15] = {
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, all, NULL},
        {22, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, all, NULL},
        {23, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, all, NULL}};
    for (int i = 0; i < 6; i++) b[3 + i] = (VkDescriptorSetLayoutBinding){TB[i], VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, all, NULL};
    /* ray tracing (M4): t3 the top level, t4 the probes, t5 the level's vertices, t8 their numbers, t10 the cards, u0
       the probes written (the probes' pass) */
    static const uint32_t RB[5] = {20, 21, 24, 26, 48};
    b[9] = (VkDescriptorSetLayoutBinding){19, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, all, NULL};
    for (int i = 0; i < 5; i++) b[10 + i] = (VkDescriptorSetLayoutBinding){RB[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, all, NULL};
    VkDescriptorSetLayoutCreateInfo li = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = g_rt_dev ? 15u : 9u, .pBindings = b};
    if (vkCreateDescriptorSetLayout(V.dev, &li, NULL, &V.sdsl) != VK_SUCCESS) return false;
    VkPipelineLayoutCreateInfo pli = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &V.sdsl};
    if (vkCreatePipelineLayout(V.dev, &pli, NULL, &V.spl) != VK_SUCCESS) return false;
    VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = V.dpool, .descriptorSetCount = 1, .pSetLayouts = &V.sdsl};
    if (vkAllocateDescriptorSets(V.dev, &dai, &V.sset) != VK_SUCCESS) return false;
    V.scene_pipe = scene_pipe_make(SCENE_PS_SPV, sizeof SCENE_PS_SPV, 2);
    return V.scene_pipe != VK_NULL_HANDLE;
}

/* a scene pipeline with this fragment shader (the grid's, M2; the ray-traced one, M4) */
static VkPipeline scene_pipe_make(const unsigned char *fsc, size_t fsn, uint32_t targets)
{
    VkShaderModule vs = shader(SCENE_VS_SPV, sizeof SCENE_VS_SPV), fs = shader(fsc, fsn);
    if (!vs || !fs) return VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo st[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs, .pName = "vs"},
                                             {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs, .pName = "ps"}};
    VkVertexInputBindingDescription vb = {0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription va[6] = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},  {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12},
                                               {2, 0, VK_FORMAT_R32G32_SFLOAT, 24},    {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 32},
                                               {4, 0, VK_FORMAT_R32G32B32_SFLOAT, 48}, {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 60}};
    VkPipelineVertexInputStateCreateInfo vi = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, .vertexBindingDescriptionCount = 1,
                                               .pVertexBindingDescriptions = &vb, .vertexAttributeDescriptionCount = 6, .pVertexAttributeDescriptions = va};
    VkPipelineInputAssemblyStateCreateInfo ia = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkPipelineViewportStateCreateInfo vp = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
    /* (depthClampEnable off: clipped at the near and far planes, as D3D12's DepthClipEnable) */
    VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .polygonMode = VK_POLYGON_MODE_FILL,
                                                 .cullMode = VK_CULL_MODE_NONE, .frontFace = VK_FRONT_FACE_CLOCKWISE, .lineWidth = 1};
    VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
    VkPipelineDepthStencilStateCreateInfo dss = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, .depthTestEnable = VK_TRUE,
                                                 .depthWriteEnable = VK_TRUE, .depthCompareOp = VK_COMPARE_OP_LESS};
    VkPipelineColorBlendAttachmentState ba[5] = {{.colorWriteMask = 0xF}, {.colorWriteMask = 0xF}, {.colorWriteMask = 0xF},
                                                 {.colorWriteMask = 0xF}, {.colorWriteMask = 0xF}};
    VkPipelineColorBlendStateCreateInfo cbs = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = targets, .pAttachments = ba};
    VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo ds = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 2, .pDynamicStates = dyn};
    /* colour, motion; and with ray tracing High the guides: albedo, specular, normal + roughness (render.c's formats) */
    VkFormat fmts[5] = {VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R16G16_SFLOAT, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
                        VK_FORMAT_R16G16B16A16_SFLOAT};
    VkPipelineRenderingCreateInfo ri = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .colorAttachmentCount = targets, .pColorAttachmentFormats = fmts,
                                        .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT};
    VkGraphicsPipelineCreateInfo gi = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .pNext = &ri, .stageCount = 2, .pStages = st,
                                       .pVertexInputState = &vi, .pInputAssemblyState = &ia, .pViewportState = &vp, .pRasterizationState = &rs,
                                       .pMultisampleState = &ms, .pDepthStencilState = &dss, .pColorBlendState = &cbs, .pDynamicState = &ds,
                                       .layout = V.spl};
    VkPipeline pipe = VK_NULL_HANDLE;
    VkResult r = vkCreateGraphicsPipelines(V.dev, VK_NULL_HANDLE, 1, &gi, NULL, &pipe);
    vkDestroyShaderModule(V.dev, vs, NULL);
    vkDestroyShaderModule(V.dev, fs, NULL);
    if (r != VK_SUCCESS) logf_("render: the Vulkan scene pipeline failed (%d)\n", (int)r);
    return r == VK_SUCCESS ? pipe : VK_NULL_HANDLE;
}

/* ---------------------------------------------------------------- ray tracing (M4): rt_start (render_vk_rt.h has the rest) */
/* render.c's start_rt: the ray-traced scene pipeline, the probes' pipeline and set, their two buffers; switched off
   by the setting but started all the same, so that it can be switched on */
static void rt_start(void)
{
    T.off = !V.cfg.raytracing;
    if (!g_rt_dev || !vkCreateAccelerationStructureKHR) return;
    T.scratch_align = 256;   /* (minAccelerationStructureScratchOffsetAlignment is at most this on every device seen: 128 on NVIDIA) */
    VkShaderModule m = shader(PROBES_CS_SPV, sizeof PROBES_CS_SPV);
    if (!m) return;
    VkComputePipelineCreateInfo ci = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                      .stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                                                .module = m, .pName = "cs"},
                                      .layout = V.spl};
    VkResult r = vkCreateComputePipelines(V.dev, VK_NULL_HANDLE, 1, &ci, NULL, &T.probe);
    vkDestroyShaderModule(V.dev, m, NULL);
    if (r != VK_SUCCESS) return (void)logf_("render: the probes' pipeline failed (%d)\n", (int)r);
    T.scene_rt = scene_pipe_make(SCENE_PS_RT_SPV, sizeof SCENE_PS_RT_SPV, 2);
    if (!T.scene_rt) return;
    VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = V.dpool, .descriptorSetCount = 1, .pSetLayouts = &V.sdsl};
    if (vkAllocateDescriptorSets(V.dev, &dai, &T.pset) != VK_SUCCESS) return (void)logf_("render: the probes' set failed\n");
    for (int i = 0; i < 2; i++)
        if (!make_buf(&T.probes[i], PROBE_BYTES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0))
            return (void)logf_("render: out of GPU memory (the probes)\n");
    T.ok = true;
}

static void level_free(void)
{
    if (T.ok) level_as_free();
    free_buf(&V.vbuf), free_buf(&V.ibuf);
    free_img(&V.atlas), free_img(&V.natlas), free_img(&V.snatlas), free_img(&V.saoatlas), free_img(&V.grid);
    V.level = false, V.vertices = V.indices = 0, V.atlas_rows = 0;
}

bool render_upload(const Mesh *m)
{
    if (!V.up) return false;
    vkDeviceWaitIdle(V.dev);
    level_free();
    Buf st[7];
    memset(st, 0, sizeof st);
    V.vertices = m->vertices, V.indices = m->indices;
    begin_cmd();
    const VkBufferUsageFlags rtu = T.ok ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                              VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR : 0;
    bool ok = upload_buf(&V.vbuf, m->vert, sizeof(Vertex) * (size_t)m->vertices, sizeof(Vertex) * (size_t)MAX(m->vertices, 3),
                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | rtu, &st[0]) &&
              upload_buf(&V.ibuf, m->index, sizeof(u32) * (size_t)m->indices, sizeof(u32) * (size_t)MAX(m->indices, 3),
                         VK_BUFFER_USAGE_INDEX_BUFFER_BIT | rtu, &st[1]) &&
              upload_tex(&V.atlas, ATLAS_SIZE, ATLAS_H, VK_FORMAT_R8G8B8A8_UNORM, m->atlas, 4, &st[2]) &&
              upload_tex(&V.natlas, NATLAS_SIZE, NATLAS_SIZE, VK_FORMAT_R8G8B8A8_UNORM, m->natlas, 4, &st[3]) &&
              upload_tex(&V.snatlas, ATLAS_SIZE, ATLAS_H, VK_FORMAT_R8G8B8A8_UNORM, m->snatlas, 4, &st[4]) &&
              upload_tex(&V.saoatlas, ATLAS_SIZE, ATLAS_H, VK_FORMAT_R8_UNORM, m->saoatlas, 1, &st[5]) &&
              upload_tex(&V.grid, 64, 64, VK_FORMAT_R8G8B8A8_UINT, m->grid, 4, &st[6]);
    if (ok && T.ok) {   /* the copies done before the structures read the vertices */
        vfx_mem_barrier();
        T.level_ok = build_as(m->vertices);
    }
    ok = submit_wait(VK_NULL_HANDLE, VK_NULL_HANDLE) && ok;
    for (int i = 0; i < 7; i++) free_buf(&st[i]);
    if (ok) scene_bind(), rt_bind_level(), V.level = true;
    else level_free();
    V.have_prev = false;
    return ok;
}

bool render_set_hires(const u8 *rgba, int size, int factor, bool maps, bool colours)
{
    static const u8 white[4] = {255, 255, 255, 255};
    if (!V.up) return false;
    if (!rgba || factor <= 0 || size <= 0) rgba = white, size = 1, factor = 0, maps = false;
    vkDeviceWaitIdle(V.dev);
    Img tex;
    Buf st;
    memset(&tex, 0, sizeof tex), memset(&st, 0, sizeof st);
    begin_cmd();
    bool ok = upload_tex(&tex, maps ? size * 2 : size, size, VK_FORMAT_R8G8B8A8_UNORM, rgba, 4, &st);
    ok = submit_wait(VK_NULL_HANDLE, VK_NULL_HANDLE) && ok;
    free_buf(&st);
    if (!ok) return free_img(&tex), false;   /* (the old one stays) */
    free_img(&V.hatlas);
    V.hatlas = tex, V.hatlas_f = factor, V.hatlas_maps = maps, V.hatlas_colours = colours;
    if (V.level) scene_bind();
    V.have_prev = false;
    return true;
}

void render_set_lights(const Light *l, int n, const u32 *tiles)
{
    if (!V.up) return;
    GpuLight *lp = V.lights.ptr;
    V.nplaced = MAX(0, MIN(n, MAX_LIGHTS));
    memcpy(V.placed, l, sizeof(Light) * (size_t)V.nplaced);
    for (int i = 0; i < V.nplaced; i++) {
        GpuLight *g = &lp[1 + i];
        memcpy(g->pr, l[i].pos, 12), g->pr[3] = l[i].range * RENDER_LIGHT_REACH;
        for (int j = 0; j < 3; j++) g->col[j] = l[i].color[j] * l[i].intensity;
        g->col[3] = 1;
        g->info[0] = 0, g->info[2] = l[i].kind == LIGHT_FLOOD ? 1.0f : 0.0f;   /* (z: it floods its room, common.hlsli light_term) */
    }
    /* the tile lists name placed lights from 0; in the buffer they follow the torch */
    u32 *tp = V.tiles.ptr;
    for (int i = 0; i < 64 * 64 * TILE_LIGHTS; i++) tp[i] = tiles[i] == NO_LIGHT ? NO_LIGHT : tiles[i] + 1;
}

/* the atlas's first rows (the walls' and floors' textures) rewritten from rgba (ATLAS_SIZE wide), at the next frame */
void render_update_atlas(const u8 *rgba, int rows)
{
    if (!V.level || !V.atlas_up.ptr || rows <= 0 || rows > 1024) return;
    memcpy(V.atlas_up.ptr, rgba, (size_t)ATLAS_SIZE * 4 * (size_t)rows);
    V.atlas_rows = rows;
}

void render_set_light_pos(int k, const float pos[3])
{
    if (!V.up || k < 0 || k >= V.nplaced) return;
    memcpy(V.placed[k].pos, pos, 12);
    memcpy(((GpuLight *)V.lights.ptr)[1 + k].pr, pos, 12);
}

void render_set_sprites(const Vertex *v, int n)
{
    if (!V.up) return;
    V.nsprites = MAX(0, MIN(n, RENDER_MAX_SPRITE_VERTS));
    memcpy(V.sprite_vb.ptr, v, sizeof(Vertex) * (size_t)V.nsprites);
}

/* the lights' fade by distance (render.c's twin; common.hlsli light_fade) */
static bool g_light_fade = true;
void render_set_light_fade(bool on) { g_light_fade = on; }
bool render_light_fade(void) { return g_light_fade; }
void render_set_torch(bool on, const float pos[3], const float color[3], float intensity, float range)
{
    if (!V.up) return;
    GpuLight *g = V.lights.ptr;
    memcpy(g->pr, pos, 12), g->pr[3] = range;
    for (int j = 0; j < 3; j++) g->col[j] = color[j] * intensity;
    g->col[3] = on ? 1.0f : 0.0f;
}
/* the player's light as the original's (render.c's render_set_torch_original) */
void render_set_torch_original(bool on, const float pos[3], const float eye[3], int level, float range, float exponent)
{
    if (!V.up) return;
    GpuLight *g = V.lights.ptr;
    memcpy(g->pr, pos, 12), g->pr[3] = range;
    memcpy(g->col, eye, 12);
    g->col[3] = on ? 2.0f + (float)(level & 7) + exponent / 10.0f : 0.0f;
}

/* the images' first state: the scene's targets readable (each frame's scene pass clears them: colour 0 0 0 1, motion
   0, depth the far plane), the interface clear, the upscaled textures' place a white texel (off) */
static bool first_contents(void)
{
    static const u8 white[4] = {255, 255, 255, 255};
    Buf st;
    memset(&st, 0, sizeof st);
    begin_cmd();
    VkImageSubresourceRange all = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkClearColorValue none = {{0, 0, 0, 0}};
    barrier(V.ui.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkCmdClearColorImage(V.cmd, V.ui.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &none, 1, &all);
    barrier(V.ui.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    barrier(V.scene.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    barrier(V.motion.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    barrier_a(V.depth.img, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    bool ok = upload_tex(&V.hatlas, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, white, 4, &st);
    ok = submit_wait(VK_NULL_HANDLE, VK_NULL_HANDLE) && ok;
    free_buf(&st);
    return ok;
}

/* the monitor the window is on: true when Windows runs it in HDR (ST.2084 / Rec. 2020); *peak its MaxLuminance (nits).
   Asked of DXGI for the monitor only (the plan's M6: Vulkan has no such query); off Windows none (X11 has no HDR) */
static bool hdr_display(float *peak)
{
#ifdef _WIN32
    if (!V.cfg.hwnd) return false;
    HMONITOR mon = MonitorFromWindow((HWND)V.cfg.hwnd, MONITOR_DEFAULTTONEAREST);
    IDXGIFactory1 *f = NULL;
    if (FAILED(CreateDXGIFactory1(&IID_IDXGIFactory1, (void **)&f)) || !f) return false;
    bool on = false;
    IDXGIAdapter1 *a = NULL;
    for (UINT i = 0; IDXGIFactory1_EnumAdapters1(f, i, &a) == S_OK; i++) {
        IDXGIOutput *o = NULL;
        for (UINT j = 0; IDXGIAdapter1_EnumOutputs(a, j, &o) == S_OK; j++) {
            DXGI_OUTPUT_DESC d;
            IDXGIOutput6 *o6 = NULL;
            if (SUCCEEDED(IDXGIOutput_GetDesc(o, &d)) && d.Monitor == mon &&
                SUCCEEDED(IDXGIOutput_QueryInterface(o, &IID_IDXGIOutput6, (void **)&o6)) && o6) {
                DXGI_OUTPUT_DESC1 d1;
                if (SUCCEEDED(IDXGIOutput6_GetDesc1(o6, &d1))) {
                    on = d1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
                    if (peak) *peak = d1.MaxLuminance > 80.0f ? d1.MaxLuminance : 1000.0f;
                }
                IDXGIOutput6_Release(o6);
            }
            IDXGIOutput_Release(o);
        }
        IDXGIAdapter1_Release(a);
    }
    IDXGIFactory1_Release(f);
    return on;
#else
    (void)peak;
    return false;
#endif
}
/* each frame: HDR on while it is wanted and the monitor runs HDR (looked at every 30 frames: the window may move to
   another monitor, or HDR be switched in Windows' settings); the swap chain made again when that changes */
static void hdr_follow(void)
{
    if (H.check-- > 0 || !V.sc) return;
    H.check = 30;
    float peak = H.display_peak;
    bool disp = hdr_display(&peak);
    H.display_peak = peak;
    if (disp != H.disp_last) H.refused = false, H.disp_last = disp;
    bool want = g_hdr_want && disp && H.pipe && H.pic.img;
    if (want && H.refused) return;   /* (the surface said no: kept SDR, not made again every 30 frames) */
    if (want == H.on && want == H.want) return;
    H.want = want;
    make_swapchain();
}

bool render_init(const RenderConfig *cfg)
{
    if (V.dev || V.inst) render_shutdown();
    memset(&V, 0, sizeof V);
    memset(&DL, 0, sizeof DL);
    V.cfg = *cfg;
    if (V.cfg.exposure <= 0) V.cfg.exposure = 2;
    int w = cfg->w, h = cfg->h;
    g_ngx = DL.loaded = cfg->dlss_dir && ngxvk_load();   /* (its extensions are wanted before the instance and the device) */
    if (!start_instance() || !start_device()) return render_shutdown(), false;
    V.ui_s = V.cfg.ui_scale > 0 ? V.cfg.ui_scale : (h + 150) / 300 > 1 ? (h + 150) / 300 : 1;
    V.ui_w = w / V.ui_s, V.ui_h = h / V.ui_s;
    const VkMemoryPropertyFlags host = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    bool ok = make_img(&V.final_, w, h, VK_FORMAT_R8G8B8A8_UNORM,
                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT) &&
              make_img(&V.scene, w, h, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) &&
              make_img(&V.motion, w, h, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) &&
              make_img_a(&V.depth, w, h, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                         VK_IMAGE_ASPECT_DEPTH_BIT) &&
              make_img(&V.ui, V.ui_w, V.ui_h, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT) &&
              make_buf(&V.ui_up, (VkDeviceSize)V.ui_w * V.ui_h * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, host, 0) &&
              make_buf(&V.cbuf, 4096, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, host, 0) &&
              make_buf(&V.readback, (VkDeviceSize)w * h * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, host, VK_MEMORY_PROPERTY_HOST_CACHED_BIT) &&
              /* the scene's (render.c's lights_buf, tiles_buf, sprite_vb; the atlas's rows on their way) */
              make_buf(&V.lights, sizeof(GpuLight) * (MAX_LIGHTS + 1), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, host, 0) &&
              make_buf(&V.tiles, sizeof(u32) * 64 * 64 * TILE_LIGHTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, host, 0) &&
              make_buf(&V.sprite_vb, sizeof(Vertex) * RENDER_MAX_SPRITE_VERTS, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                       host, 0) &&
              make_buf(&V.atlas_up, (VkDeviceSize)ATLAS_SIZE * 4 * 1024, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, host, 0);
    if (!ok) return logf_("render: out of GPU memory\n"), render_shutdown(), false;
    H.on = H.want = H.pic_ready = false, H.check = 0;
    if (cfg->hwnd && !make_img(&H.pic, w, h, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
        logf_("render: HDR: no FP16 picture (SDR only)\n");
    memset(V.cbuf.ptr, 0, 4096);
    memset(V.lights.ptr, 0, sizeof(GpuLight) * (MAX_LIGHTS + 1));
    for (int i = 0; i < 64 * 64 * TILE_LIGHTS; i++) ((u32 *)V.tiles.ptr)[i] = NO_LIGHT;
    if (!first_contents() || !start_present()) return logf_("render: the Vulkan present pass failed\n"), render_shutdown(), false;
    if (!start_scene()) return logf_("render: the Vulkan scene pass failed\n"), render_shutdown(), false;
    rt_start();
    dlss_start();   /* always (when the card has it): the settings only say whether DLSS starts on */
    DL.on = DL.ok && cfg->dlss;
    g_rt_high = cfg->rt_high;   /* (render.c's R.high_on) */
    if (DL.ok && T.ok) DL.scene_rth = scene_pipe_make(SCENE_PS_RTH_SPV, sizeof SCENE_PS_RTH_SPV, 5);
    set_dlss_text();
    g_rp_ok = vfx_particles_start();   /* (used only while fx_particles is on) */
    g_rd_ok = vfx_decals_start();      /* (fx_decals) */
    if (cfg->hwnd && !make_swapchain() && !V.sc_stale) return render_shutdown(), false;
    V.up = true;
    /* (render.c's form: the GPU, the size, the shadows' way, DLSS's line) */
    const char *rtw = T.ok && !T.off ? "ray tracing (ray queries) with bounced light" : "shadows through the tile grid";
    if (DL.ok)
        snprintf(V.status, sizeof V.status, "%s, output %dx%d, Vulkan, %s, DLSS %s (render %dx%d)", V.gpu, w, h, rtw,
                 DLSS_QN[MAX(0, MIN(V.cfg.dlss_quality, 4))], DL.rw, DL.rh);
    else snprintf(V.status, sizeof V.status, "%s, %dx%d, Vulkan, %s, %s", V.gpu, w, h, rtw, DL.text);
    return true;
}

void render_shutdown(void)
{
    if (V.dev) {
        vkDeviceWaitIdle(V.dev);
        drop_swapchain(VK_NULL_HANDLE);
        if (V.present) vkDestroyPipeline(V.dev, V.present, NULL);
        if (H.pipe) vkDestroyPipeline(V.dev, H.pipe, NULL);
        free_img(&H.pic);
        H.pipe = VK_NULL_HANDLE, H.shot_set = VK_NULL_HANDLE, H.on = false;
        if (V.pl) vkDestroyPipelineLayout(V.dev, V.pl, NULL);
        if (V.dpool) vkDestroyDescriptorPool(V.dev, V.dpool, NULL);
        if (V.dsl) vkDestroyDescriptorSetLayout(V.dev, V.dsl, NULL);
        dlss_stop();
        vfx_stop();
        level_free();
        rt_stop();
        if (V.scene_pipe) vkDestroyPipeline(V.dev, V.scene_pipe, NULL);
        if (V.spl) vkDestroyPipelineLayout(V.dev, V.spl, NULL);
        if (V.sdsl) vkDestroyDescriptorSetLayout(V.dev, V.sdsl, NULL);
        level_free();
        free_img(&V.hatlas), free_img(&V.motion);
        free_buf(&V.lights), free_buf(&V.tiles), free_buf(&V.sprite_vb), free_buf(&V.atlas_up);
        free_img(&V.final_), free_img(&V.scene), free_img(&V.depth), free_img(&V.ui);
        free_buf(&V.ui_up), free_buf(&V.cbuf), free_buf(&V.readback);
        if (V.sem_acq) vkDestroySemaphore(V.dev, V.sem_acq, NULL);
        if (V.fence) vkDestroyFence(V.dev, V.fence, NULL);
        if (V.pool) vkDestroyCommandPool(V.dev, V.pool, NULL);
        vkDestroyDevice(V.dev, NULL);
    }
    if (V.inst) {
        if (V.surface) vkDestroySurfaceKHR(V.inst, V.surface, NULL);
        if (V.dbg && vkDestroyDebugUtilsMessengerEXT) {
            logf_("render: Vulkan validation: %d warnings and errors this run (the loader's own not counted)\n", g_vk_messages);
            vkDestroyDebugUtilsMessengerEXT(V.inst, V.dbg, NULL);
        }
        vkDestroyInstance(V.inst, NULL);
    }
    memset(&V, 0, sizeof V);
}

/* ---------------------------------------------------------------- a frame */
bool render_frame(const Camera *cam, const Look *look, bool reset)
{
    if (!V.up || !cam || !look) return false;
    /* ---- the frame's constants and the lights: render.c's render_frame, its lines in its order (R. is V. here) ---- */
    bool dlss = DL.on && DL.ok;
    int rw = dlss ? DL.rw : V.cfg.w, rh = dlss ? DL.rh : V.cfg.h;
    reset = reset || !V.have_prev;
    float jx = 0, jy = 0;
    if (dlss) {
        /* the jitter's phases: 8 * (output / render)^2 as NVIDIA asks, 16 at least (what every mode had; DLAA's still) */
        int phases = MAX(16, (int)(8.0f * (float)V.cfg.w * (float)V.cfg.w / ((float)rw * (float)rw) + 0.5f));
        static int jh = -1;   /* UWVIEW_JITTER_HOLD=1 (the checks only: tools\vkcompare.py): one jitter phase, every run alike */
        if (jh < 0) { const char *e = getenv("UWVIEW_JITTER_HOLD"); jh = e && *e && *e != '0'; }
        int ph = (int)((jh ? 0u : V.frame) % (unsigned)phases) + 1;
        jx = halton(ph, 2) - 0.5f;
        jy = halton(ph, 3) - 0.5f;
    }
    Constants *c = V.cbuf.ptr;
    GpuLight *lights_ptr = V.lights.ptr;
    float view[16], proj[16];
    view_proj(cam, rw, rh, 0, 0, c->vp_nojit, view, proj);
    view_proj(cam, rw, rh, jx * 2.0f / (float)rw, -jy * 2.0f / (float)rh, c->vp, NULL, NULL);
    /* ray tracing High: Ray Reconstruction (made the first time it is wanted, between frames) in upscaling's place */
    bool high = false;
    {
        const bool rt_on = T.ok && T.level_ok && !T.off && V.level;
        if (dlss && rt_on && DL.scene_rth && g_rt_high) {
            if (!ngxvk_rr_ready()) {
                begin_cmd();
                if (ngxvk_rr_create(V.dev, V.cmd, DL.rw, DL.rh, V.cfg.w, V.cfg.h, DLSS_NGX_Q[MAX(0, MIN(V.cfg.dlss_quality, 4))])) reset = true;
                submit_wait(VK_NULL_HANDLE, VK_NULL_HANDLE);
            }
            high = ngxvk_rr_ready();
        }
    }
    if (high != DL.high_active) reset = true;
    DL.high_active = high;
    transpose(view, DL.w2v), transpose(proj, DL.v2c);
    if (reset) memcpy(V.prev_vp, c->vp_nojit, 64);
    memcpy(c->vp_prev, V.prev_vp, 64);
    if (!g_hold) g_hold_frame = V.frame;
    memcpy(c->eye, cam->pos, 12), c->eye[3] = (float)g_hold_frame;
    /* the light's size for soft shadows; bounced light; how much of the probes' old light they keep */
    c->look[0] = 0.06f, c->look[1] = look->bounce, c->look[2] = reset ? 0.0f : 0.93f, c->look[3] = look->sheen;
    c->size[0] = (float)rw, c->size[1] = (float)rh, c->size[2] = V.cfg.exposure;
    float gamma = look->gamma > 0.1f ? look->gamma : 1.0f;
    c->post[0] = 1.0f / V.cfg.exposure, c->post[1] = 1.0f, c->post[2] = 1.0f / (2.2f * gamma), c->post[3] = (float)V.ui_s;
    hdr_follow();   /* HDR output (render_set_hdr): the swap chain's format follows the setting and the monitor */
    c->hdr[0] = H.on ? 1.0f : 0.0f, c->hdr[1] = g_hdr_paper / 80.0f;
    c->hdr[2] = render_hdr_peak() / g_hdr_paper, c->hdr[3] = g_light_fade ? 1.0f : 0.0f;   /* (w: the lights' fade, light_fade) */
    if (c->hdr[2] < 1.0f) c->hdr[2] = 1.0f;
    /* the burning torches flicker, each in its own way */
    float secs = g_hold ? g_hold_secs : (float)(rv_ticks() % 100000000ull) / 1000.0f;
    for (int k = 0; k < V.nplaced; k++) {
        const Light *L = &V.placed[k];
        if (L->kind != LIGHT_WALL_TORCH) continue;
        float D = L->flame > 0.005f ? L->flame : 0.05f, fp = 1.5f / sqrtf(D) * 6.2832f, depth = 0.05f + 0.25f * sqrtf(D);
        float ph = k * 2.39996f;
        float f = 1.0f + depth * (0.55f * sinf(secs * fp + ph) + 0.25f * sinf(secs * fp * 1.73f + ph * 3.1f) +
                                  0.12f * sinf(secs * fp * 3.17f + ph * 1.7f) + 0.08f * sinf(secs * fp * 5.9f + ph * 5.3f));
        GpuLight *gl = &lights_ptr[1 + k];
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
    }
    {   /* the player's torch: a pitch torch's flame, 5 cm across, 1.5 kW, 1750 K at its hottest */
        GpuLight *g0 = &lights_ptr[0];
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
    for (int k = 0; k < V.nplaced; k++) {
        const Light *L = &V.placed[k];
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
    c->outp[0] = (float)V.cfg.w, c->outp[1] = (float)V.cfg.h, c->outp[2] = 1.0f / tanf(cam->fov_y * 0.5f);
    c->outp[3] = (float)rw / (float)V.cfg.w;
    memcpy(c->tint, g_tint, sizeof g_tint);
    c->rtlim[0] = look->ray_length > 0 ? look->ray_length : 48.0f, c->rtlim[1] = look->bounces > 0 ? (float)look->bounces : 99.0f;
    c->rtlim[2] = look->normal_maps ? look->sprite_strength : 0;
    memcpy(c->torchc, g_torch_rgb, 12), c->torchc[3] = 1;
    c->sprite[0] = look->normal_maps ? look->sprite_ao : 0, c->sprite[1] = look->normal_maps ? look->sprite_height : 0;
    c->sprite[2] = look->texture_ao, c->sprite[3] = look->texture_shadow;
    c->texres[0] = (float)V.hatlas_f, c->texres[1] = V.hatlas_maps ? 1.0f : 0.0f, c->texres[2] = V.hatlas_colours ? 1.0f : 0.0f, c->texres[3] = 0;
    c->rtlim[3] = g_fullbright ? -1.0f : g_shade_k;   /* the original's depth scale for its shading; < 0: full bright */
    {   /* the effects' water: on, the rings of the splashes younger than two seconds, the time (held in a comparison) */
        int nr = 0;
        double now = g_hold ? g_hold_now : (double)rv_ticks() / 1000.0;
        for (int i = 0; i < 16 && g_fx.water && g_ripples_shown; i++) {
            double age = now - g_ripple[i].born;
            if (g_ripple[i].born <= 0 || age < 0 || age > 2.0) continue;
            c->ripple[nr][0] = g_ripple[i].x, c->ripple[nr][1] = g_ripple[i].z, c->ripple[nr][2] = (float)age, c->ripple[nr][3] = g_ripple[i].strength;
            nr++;
        }
        c->fxw[0] = g_fx.water ? 1.0f : 0.0f, c->fxw[1] = (float)nr, c->fxw[2] = secs, c->fxw[3] = 0;
    }

    /* the swap chain's image for this frame (made again when the window changed) */
    uint32_t si = UINT32_MAX;
    if (V.cfg.hwnd) {
        if (V.sc_stale || !V.sc) make_swapchain();
        if (V.sc) {
            VkResult r = vkAcquireNextImageKHR(V.dev, V.sc, UINT64_MAX, V.sem_acq, VK_NULL_HANDLE, &si);
            if (r == VK_ERROR_OUT_OF_DATE_KHR) {
                si = UINT32_MAX;
                if (make_swapchain() && vkAcquireNextImageKHR(V.dev, V.sc, UINT64_MAX, V.sem_acq, VK_NULL_HANDLE, &si) < 0) si = UINT32_MAX;
            } else if (r < 0) si = UINT32_MAX;
            else if (r == VK_SUBOPTIMAL_KHR) V.sc_stale = true;
            if (si != UINT32_MAX && si >= V.sc_n) si = UINT32_MAX, V.sc_stale = true;   /* (past the 8 held: no image) */
        }
    }

    if (T.ok && T.level_ok && !T.off && V.level) rt_bind_frame((int)(V.frame & 1));   /* (before the frame's commands) */
    begin_cmd();
    if (V.ui_dirty) {   /* this frame's interface */
        barrier(V.ui.img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkBufferImageCopy bc = {.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, .imageExtent = {(uint32_t)V.ui_w, (uint32_t)V.ui_h, 1}};
        vkCmdCopyBufferToImage(V.cmd, V.ui_up.buf, V.ui.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bc);
        barrier(V.ui.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        V.ui_dirty = false;
    }
    if (V.atlas_rows > 0 && V.level) {   /* the textures' cycling colours, as the game's palette has them now */
        barrier(V.atlas.img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkBufferImageCopy bc = {.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, .imageExtent = {ATLAS_SIZE, (uint32_t)V.atlas_rows, 1}};
        vkCmdCopyBufferToImage(V.cmd, V.atlas_up.buf, V.atlas.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bc);
        barrier(V.atlas.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        V.atlas_rows = 0;
    }
    const bool parts = g_fx.particles && g_rp_ok, decals = g_fx.decals && g_rd_ok;
    const bool rt_live = T.ok && T.level_ok && !T.off && V.level;
    if (rt_live) {   /* render.c's order: the pictures' structures, then the probes, then the scene */
        cards_as_build(cam->pos);
        probes_pass();
    }
    if (parts) {   /* the particles moved (render_vk_fx.h), drawn after the level and its pictures: render.c's lines */
        float right[3] = {cosf(cam->yaw), 0, -sinf(cam->yaw)}, f3[3] = {fwd_of(cam, 0), fwd_of(cam, 1), fwd_of(cam, 2)};
        float up[3] = {f3[1] * right[2] - f3[2] * right[1], f3[2] * right[0] - f3[0] * right[2], f3[0] * right[1] - f3[1] * right[0]};
        float torch[4] = {cam->pos[0], cam->pos[1], cam->pos[2], g_fullbright ? 99.0f : 4.0f};
        vfx_particles_frame(c->vp, cam->pos, V.cfg.exposure, right, up, torch, g_torch_rgb);
    }
    if (decals) vfx_decals_frame(c->vp);
    {   /* the scene: colour (cleared 0 0 0 1), motion (0), depth (the far plane); the level indexed, then the cards.
           The viewport's height is negative (y from the bottom up), so clip space is D3D12's (y up) and the picture,
           SV_Position, ddx / ddy and the motion vectors come out as render.c's without a shader switch */
        barrier(V.scene.img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        barrier(V.motion.img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        barrier_a(V.depth.img, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo sa[5] = {
            {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = V.scene.view, .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
             .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE, .clearValue = {.color = {{0, 0, 0, 1}}}},
            {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = V.motion.view, .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
             .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, .storeOp = VK_ATTACHMENT_STORE_OP_STORE, .clearValue = {.color = {{0, 0, 0, 0}}}}};
        Img *guides[3] = {&DL.albedo, &DL.spec, &DL.nrough};
        if (high)   /* where nothing is drawn: the guides' "sky" (albedo 0.5, nothing else) */
            for (int g = 0; g < 3; g++) {
                barrier(guides[g]->img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                sa[2 + g] = (VkRenderingAttachmentInfo){VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = guides[g]->view,
                                                        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                                        .clearValue = {.color = {{g ? 0.0f : 0.5f, g ? 0.0f : 0.5f, g ? 0.0f : 0.5f, 0}}}};
            }
        VkRenderingAttachmentInfo da = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = V.depth.view,
                                        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                        .storeOp = VK_ATTACHMENT_STORE_OP_STORE, .clearValue = {.depthStencil = {1.0f, 0}}};
        VkRenderingInfo sri = {VK_STRUCTURE_TYPE_RENDERING_INFO, .renderArea = {{0, 0}, {(uint32_t)V.cfg.w, (uint32_t)V.cfg.h}}, .layerCount = 1,
                               .colorAttachmentCount = high ? 5u : 2u, .pColorAttachments = sa, .pDepthAttachment = &da};
        vkCmdBeginRendering(V.cmd, &sri);
        if (V.level) {
            VkViewport svp = {0, (float)rh, (float)rw, -(float)rh, 0, 1};
            VkRect2D ssc = {{0, 0}, {(uint32_t)rw, (uint32_t)rh}};
            VkDeviceSize zero = 0;
            vkCmdSetViewport(V.cmd, 0, 1, &svp);
            vkCmdSetScissor(V.cmd, 0, 1, &ssc);
            vkCmdBindPipeline(V.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, high ? DL.scene_rth : rt_live ? T.scene_rt : V.scene_pipe);
            vkCmdBindDescriptorSets(V.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, V.spl, 0, 1, &V.sset, 0, NULL);
            if (V.vertices && V.indices) {
                vkCmdBindVertexBuffers(V.cmd, 0, 1, &V.vbuf.buf, &zero);
                vkCmdBindIndexBuffer(V.cmd, V.ibuf.buf, 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(V.cmd, (uint32_t)V.indices, 1, 0, 0, 0);
            }
            if (V.nsprites) {   /* the picture cards */
                vkCmdBindVertexBuffers(V.cmd, 0, 1, &V.sprite_vb.buf, &zero);
                vkCmdDraw(V.cmd, (uint32_t)V.nsprites, 1, 0, 0);
            }
        }
        vkCmdEndRendering(V.cmd);
        if ((parts || decals) && V.level) {   /* the marks on the floor, then the particles: into the colour alone, the depth tested */
            VkRenderingAttachmentInfo fa = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = V.scene.view,
                                            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                                            .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
            VkRenderingAttachmentInfo fd = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = V.depth.view,
                                            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                                            .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
            VkRenderingInfo fri = {VK_STRUCTURE_TYPE_RENDERING_INFO, .renderArea = {{0, 0}, {(uint32_t)V.cfg.w, (uint32_t)V.cfg.h}}, .layerCount = 1,
                                   .colorAttachmentCount = 1, .pColorAttachments = &fa, .pDepthAttachment = &fd};
            VkViewport fvp = {0, (float)rh, (float)rw, -(float)rh, 0, 1};
            VkRect2D fsc = {{0, 0}, {(uint32_t)rw, (uint32_t)rh}};
            vkCmdBeginRendering(V.cmd, &fri);
            vkCmdSetViewport(V.cmd, 0, 1, &fvp);
            vkCmdSetScissor(V.cmd, 0, 1, &fsc);
            vfx_draw(decals, parts);
            vkCmdEndRendering(V.cmd);
        }
        barrier(V.scene.img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        barrier(V.motion.img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        barrier_a(V.depth.img, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        if (high)
            for (int g = 0; g < 3; g++) barrier(guides[g]->img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    /* DLSS (or Ray Reconstruction); the present pass shows its output, or the scene's colour */
    present_source(dlss && dlss_evaluate(high, jx, jy, rw, rh, reset) ? &DL.out : &V.scene);
    /* the present pass into the output's picture: the 8-bit one, or with HDR on the FP16 one (the window's); a
       screenshot with HDR on: the same frame drawn again as SDR (the constants' copy, hdr.x 0) into the 8-bit one */
    const bool hdr = H.on && H.pipe && H.pic.img && si != UINT32_MAX;
    Img *pic = hdr ? &H.pic : &V.final_;
    for (int pass = 0; pass < (hdr && g_want_shot ? 2 : 1); pass++) {
        bool sdr_copy = pass == 1;
        if (sdr_copy) {
            Constants *c2 = (Constants *)((u8 *)V.cbuf.ptr + CB_SHOT_OFF);
            memcpy(c2, c, sizeof *c2), c2->hdr[0] = 0;
            pic = &V.final_;
        }
        bool *ready = pic == &H.pic ? &H.pic_ready : &V.final_ready;
        barrier(pic->img, *ready ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo ca = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = pic->view,
                                        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                        .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
        VkRenderingInfo ri = {VK_STRUCTURE_TYPE_RENDERING_INFO, .renderArea = {{0, 0}, {(uint32_t)V.cfg.w, (uint32_t)V.cfg.h}}, .layerCount = 1,
                              .colorAttachmentCount = 1, .pColorAttachments = &ca};
        vkCmdBeginRendering(V.cmd, &ri);
        VkViewport vp = {0, 0, (float)V.cfg.w, (float)V.cfg.h, 0, 1};
        VkRect2D sc = {{0, 0}, {(uint32_t)V.cfg.w, (uint32_t)V.cfg.h}};
        vkCmdSetViewport(V.cmd, 0, 1, &vp);
        vkCmdSetScissor(V.cmd, 0, 1, &sc);
        vkCmdBindPipeline(V.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pic == &H.pic ? H.pipe : V.present);
        vkCmdBindDescriptorSets(V.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, V.pl, 0, 1, sdr_copy ? &H.shot_set : &V.set, 0, NULL);
        vkCmdDraw(V.cmd, 3, 1, 0, 0);
        vkCmdEndRendering(V.cmd);
        barrier(pic->img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        *ready = true;
    }
    if (!V.cfg.hwnd || g_want_shot) {   /* headless always; with a window, a screenshot: the picture for render_readback */
        VkBufferImageCopy bc = {.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, .imageExtent = {(uint32_t)V.cfg.w, (uint32_t)V.cfg.h, 1}};
        vkCmdCopyImageToBuffer(V.cmd, V.final_.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, V.readback.buf, 1, &bc);
        g_want_shot = false;
    }
    if (si != UINT32_MAX) {   /* onto the window, stretched to its size when that differs (as DXGI's) */
        VkImage img = V.sc_img[si];
        barrier(img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        bool same = V.sc_ext.width == (uint32_t)V.cfg.w && V.sc_ext.height == (uint32_t)V.cfg.h;
        VkImageBlit bl = {.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, .srcOffsets = {{0, 0, 0}, {V.cfg.w, V.cfg.h, 1}},
                          .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                          .dstOffsets = {{0, 0, 0}, {(int32_t)V.sc_ext.width, (int32_t)V.sc_ext.height, 1}}};
        vkCmdBlitImage(V.cmd, hdr ? H.pic.img : V.final_.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bl,
                       same ? VK_FILTER_NEAREST : VK_FILTER_LINEAR);
        barrier(img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }
    bool ok = submit_wait(si != UINT32_MAX ? V.sem_acq : VK_NULL_HANDLE, si != UINT32_MAX ? V.sem_done[si] : VK_NULL_HANDLE);
    if (ok && si != UINT32_MAX) {
        VkPresentInfoKHR pi = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, .waitSemaphoreCount = 1, .pWaitSemaphores = &V.sem_done[si],
                               .swapchainCount = 1, .pSwapchains = &V.sc, .pImageIndices = &si};
        VkResult r = vkQueuePresentKHR(V.queue, &pi);
        if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) V.sc_stale = true;
        else if (r < 0) logf_("render: Vulkan present failed (%d)\n", (int)r);
        vkQueueWaitIdle(V.queue);   /* (one frame at a time, as render.c: the present's semaphore free again) */
    }
    memcpy(V.prev_vp, c->vp_nojit, 64);
    V.have_prev = true;
    V.frame++;
    return ok;
}

void render_want_screenshot(void) { g_want_shot = true; }

bool render_readback(u8 *rgba)
{
    if (!V.up || !V.readback.ptr) return false;
    memcpy(rgba, V.readback.ptr, (size_t)V.cfg.w * (size_t)V.cfg.h * 4);
    return true;
}

void render_ui_size(int *w, int *h, int *scale) { *w = V.ui_w, *h = V.ui_h, *scale = V.ui_s; }
void render_set_ui(const u8 *rgba)
{
    if (!V.ui_up.ptr) return;
    memcpy(V.ui_up.ptr, rgba, (size_t)V.ui_w * (size_t)V.ui_h * 4);
    V.ui_dirty = true;
}

const char *render_status(void) { return V.status; }

/* ---------------------------------------------------------------- the rest of render.h: the settings */
void render_set_object_shadows(bool on) { g_card_shadows = on; }
bool render_object_shadows(void) { return g_card_shadows; }
void render_set_hdr(bool on) { g_hdr_want = on, H.check = 0, H.refused = false; }
bool render_hdr_wanted(void) { return g_hdr_want; }
bool render_hdr_on(void) { return H.on; }
bool render_hdr_available(void) { float p; return hdr_display(&p); }
void render_set_hdr_levels(float peak, float paper)
{
    g_hdr_peak = peak < 0 ? 0 : peak > 10000 ? 10000 : peak;
    g_hdr_paper = paper < 80 ? 80 : paper > 500 ? 500 : paper;
}
float render_hdr_peak(void) { return g_hdr_peak > 0 ? g_hdr_peak : H.display_peak; }
float render_hdr_peak_setting(void) { return g_hdr_peak; }
float render_hdr_paper(void) { return g_hdr_paper; }
bool render_raytracing(void) { return T.ok && !T.off; }
bool render_raytracing_available(void) { return T.ok; }
void render_set_raytracing(bool on)
{
    T.off = !on;
    V.have_prev = false; /* the probes start again */
}
void render_set_rt_high(bool on) { g_rt_high = on; }
bool render_rt_high_wanted(void) { return g_rt_high; }
bool render_rt_high_active(void) { return DL.high_active; }
bool render_dlss5_available(void) { return false; }
bool render_dlss5_on(void) { return false; }
bool render_set_dlss5(bool on) { (void)on; return false; }
void render_set_dlss5_style(int style) { (void)style; }
void render_set_dlss5_params(float intensity, float structure) { (void)intensity, (void)structure; }

/* the D3D12 passes' entry points (uwview.c's RenderFxPasses names them): not used here, the effects' passes are
   render_vk_fx.h's, started by render_init and called by render_frame */
bool rfx_particles_init(void *dev) { (void)dev; return false; }
void rfx_particles_frame(void *cl, const float vp[16], const float eye[3], float exposure, const float right[3], const float up[3],
                         const float torch[4], const float torchc[3])
{
    (void)cl, (void)vp, (void)eye, (void)exposure, (void)right, (void)up, (void)torch, (void)torchc;
}
void rfx_particles_draw(void *cl, size_t rtv, size_t dsv) { (void)cl, (void)rtv, (void)dsv; }
bool rfx_decals_init(void *dev) { (void)dev; return false; }
void rfx_decals_draw(void *cl, const float vp[16], size_t rtv, size_t dsv) { (void)cl, (void)vp, (void)rtv, (void)dsv; }
