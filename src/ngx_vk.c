/* ngx_vk.c: NGX through its Vulkan interface (ngx_vk.h), the Vulkan renderer's ngx.c.
 *
 * Windows: as ngx.c, without NVIDIA's Microsoft-built loader library (our clang cannot link it): the driver's NGXCore
 * (_nvngx.dll, found by the registry) is loaded here and its NVSDK_NGX_VULKAN_* exports called as that loader calls
 * them. Read from nvsdk_ngx_s.lib (SDK 310.9.16, nvsdk_ngx_vulkan_lib.obj; work\ngxvk\lib.dis):
 *   - NVSDK_NGX_VULKAN_Init loads the core, then calls the core's NVSDK_NGX_VULKAN_Init_Ext2 with
 *     (app id, data path, instance, physical device, device, vkGetInstanceProcAddr, vkGetDeviceProcAddr, SDK version,
 *     feature info): the version before the info, the reverse of the header's Init;
 *   - CreateFeature1 (device, command buffer, feature, parameters, handle), EvaluateFeature, ReleaseFeature,
 *     Shutdown1 (device), the parameter calls and RequiredExtensions go to the core's exports of the same names with
 *     the header's arguments.
 *   The parameter object is the same MSVC class ngx.c uses (its virtual functions by slot); a Vulkan picture is given
 *   as a pointer to an NVSDK_NGX_Resource_VK through the void pointer setter, as nvsdk_ngx_helpers_vk.h does.
 * Elsewhere (Linux): NVIDIA's own static library, libnvsdk_ngx.a, is linked and called by the header's names.
 *
 * The parameters set are ngx.c's, name for name, so both renderers ask DLSS for the same thing. */
#include "ngx_vk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include "nvsdk_ngx_defs.h"
#include "nvsdk_ngx_defs_vk.h"
#else
#include "nvsdk_ngx_vk.h"
#endif

static const unsigned long long APP_ID = 231313132ull; /* the SDK samples' development id (ngx.c's) */

#ifdef _WIN32
typedef NVSDK_NGX_Result (*PFN_InitExt2)(unsigned long long, const wchar_t *, VkInstance, VkPhysicalDevice, VkDevice,
                                         PFN_vkGetInstanceProcAddr, PFN_vkGetDeviceProcAddr, NVSDK_NGX_Version,
                                         const NVSDK_NGX_FeatureCommonInfo *);
typedef NVSDK_NGX_Result (*PFN_Shutdown1)(VkDevice);
typedef NVSDK_NGX_Result (*PFN_Params)(NVSDK_NGX_Parameter **);
typedef NVSDK_NGX_Result (*PFN_DestroyParams)(NVSDK_NGX_Parameter *);
typedef NVSDK_NGX_Result (*PFN_Create1)(VkDevice, VkCommandBuffer, NVSDK_NGX_Feature, NVSDK_NGX_Parameter *, NVSDK_NGX_Handle **);
typedef NVSDK_NGX_Result (*PFN_Evaluate)(VkCommandBuffer, const NVSDK_NGX_Handle *, const NVSDK_NGX_Parameter *, void *);
typedef NVSDK_NGX_Result (*PFN_Release)(NVSDK_NGX_Handle *);
typedef NVSDK_NGX_Result (*PFN_ReqExt)(unsigned *, const char ***, unsigned *, const char ***);

/* the parameter class's virtual functions, MSVC's slot order (ngx.c) */
typedef struct {
    void (*SetVoid)(NVSDK_NGX_Parameter *, const char *, void *);
    void (*SetD3D12)(NVSDK_NGX_Parameter *, const char *, void *);
    void (*SetD3D11)(NVSDK_NGX_Parameter *, const char *, void *);
    void (*SetI)(NVSDK_NGX_Parameter *, const char *, int);
    void (*SetUI)(NVSDK_NGX_Parameter *, const char *, unsigned);
    void (*SetD)(NVSDK_NGX_Parameter *, const char *, double);
    void (*SetF)(NVSDK_NGX_Parameter *, const char *, float);
    void (*SetULL)(NVSDK_NGX_Parameter *, const char *, unsigned long long);
    NVSDK_NGX_Result (*GetVoid)(NVSDK_NGX_Parameter *, const char *, void **);
    NVSDK_NGX_Result (*GetD3D12)(NVSDK_NGX_Parameter *, const char *, void **);
    NVSDK_NGX_Result (*GetD3D11)(NVSDK_NGX_Parameter *, const char *, void **);
    NVSDK_NGX_Result (*GetI)(NVSDK_NGX_Parameter *, const char *, int *);
    NVSDK_NGX_Result (*GetUI)(NVSDK_NGX_Parameter *, const char *, unsigned *);
    NVSDK_NGX_Result (*GetD)(NVSDK_NGX_Parameter *, const char *, double *);
    NVSDK_NGX_Result (*GetF)(NVSDK_NGX_Parameter *, const char *, float *);
    NVSDK_NGX_Result (*GetULL)(NVSDK_NGX_Parameter *, const char *, unsigned long long *);
    void (*Reset)(NVSDK_NGX_Parameter *);
} ParamVtbl;
#define VT(p) (*(ParamVtbl **)(p))
#define P_VOID(p, n, v) VT(p)->SetVoid(p, n, v)
#define P_I(p, n, v) VT(p)->SetI(p, n, v)
#define P_UI(p, n, v) VT(p)->SetUI(p, n, v)
#define P_F(p, n, v) VT(p)->SetF(p, n, v)
#define P_GETI(p, n, v) VT(p)->GetI(p, n, v)
#define P_RESET(p) VT(p)->Reset(p)
#else
#define P_VOID(p, n, v) NVSDK_NGX_Parameter_SetVoidPointer(p, n, v)
#define P_I(p, n, v) NVSDK_NGX_Parameter_SetI(p, n, v)
#define P_UI(p, n, v) NVSDK_NGX_Parameter_SetUI(p, n, v)
#define P_F(p, n, v) NVSDK_NGX_Parameter_SetF(p, n, v)
#define P_GETI(p, n, v) NVSDK_NGX_Parameter_GetI(p, n, v)
/* (NVSDK_NGX_Parameter_Reset is only the name "Reset" in the C headers: the class's Reset() is called through its
   table, as on Windows. The GCC / Itanium ABI keeps nvsdk_ngx_params.h's order (8 Set, 8 Get, then Reset; no
   destructor), so it is slot 16) */
#define P_RESET(p) ((*(void (***)(NVSDK_NGX_Parameter *))(p))[16](p))
#endif

static struct {
#ifdef _WIN32
    HMODULE core;
    PFN_InitExt2 init;
    PFN_Shutdown1 shutdown;
    PFN_Params caps, alloc;
    PFN_DestroyParams destroy;
    PFN_Create1 create;
    PFN_Evaluate evaluate;
    PFN_Release release;
    PFN_ReqExt reqext;
#endif
    bool loaded, started, dlss_ok;
    VkDevice dev;
    NVSDK_NGX_Parameter *params;
    NVSDK_NGX_Handle *dlss;
    unsigned char internal[16];
    NVSDK_NGX_FeatureCommonInfo info;
} N;

/* the calls, one name for both systems */
#ifdef _WIN32
#define NGX_CAPS(pp) N.caps(pp)
#define NGX_ALLOC(pp) N.alloc(pp)
#define NGX_DESTROY(p) N.destroy(p)
#define NGX_CREATE(dev, cmd, f, p, h) N.create(dev, cmd, f, p, h)
#define NGX_EVALUATE(cmd, h, p) N.evaluate(cmd, h, p, NULL)
#define NGX_RELEASE(h) N.release(h)
#else
#define NGX_CAPS(pp) NVSDK_NGX_VULKAN_GetCapabilityParameters(pp)
#define NGX_ALLOC(pp) NVSDK_NGX_VULKAN_AllocateParameters(pp)
#define NGX_DESTROY(p) NVSDK_NGX_VULKAN_DestroyParameters(p)
#define NGX_CREATE(dev, cmd, f, p, h) NVSDK_NGX_VULKAN_CreateFeature1(dev, cmd, f, p, h)
#define NGX_EVALUATE(cmd, h, p) NVSDK_NGX_VULKAN_EvaluateFeature_C(cmd, h, p, NULL)
#define NGX_RELEASE(h) NVSDK_NGX_VULKAN_ReleaseFeature(h)
#endif

bool ngxvk_load(void)
{
    if (N.loaded) return true;
#ifdef _WIN32
    wchar_t core[MAX_PATH] = {0};
    DWORD sz = sizeof core;
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\NVIDIA Corporation\\Global\\NGXCore", L"FullPath", RRF_RT_REG_SZ, NULL,
                     core, &sz) != ERROR_SUCCESS) {
        printf("ngx: no NGXCore registry key (no NVIDIA driver with NGX)\n");
        return false;
    }
    wcscat(core, L"\\_nvngx.dll");
    N.core = LoadLibraryW(core);
    if (!N.core) {
        printf("ngx: cannot load the driver's _nvngx.dll\n");
        return false;
    }
#define GET(field, type, name) N.field = (type)(void *)GetProcAddress(N.core, name)
    GET(init, PFN_InitExt2, "NVSDK_NGX_VULKAN_Init_Ext2");
    GET(shutdown, PFN_Shutdown1, "NVSDK_NGX_VULKAN_Shutdown1");
    GET(caps, PFN_Params, "NVSDK_NGX_VULKAN_GetCapabilityParameters");
    GET(alloc, PFN_Params, "NVSDK_NGX_VULKAN_AllocateParameters");
    GET(destroy, PFN_DestroyParams, "NVSDK_NGX_VULKAN_DestroyParameters");
    GET(create, PFN_Create1, "NVSDK_NGX_VULKAN_CreateFeature1");
    GET(evaluate, PFN_Evaluate, "NVSDK_NGX_VULKAN_EvaluateFeature");
    GET(release, PFN_Release, "NVSDK_NGX_VULKAN_ReleaseFeature");
    GET(reqext, PFN_ReqExt, "NVSDK_NGX_VULKAN_RequiredExtensions");
#undef GET
    if (!N.init || !N.shutdown || !N.caps || !N.alloc || !N.destroy || !N.create || !N.evaluate || !N.release || !N.reqext) {
        printf("ngx: an NGXCore Vulkan export is missing (the driver is too old for NGX on Vulkan)\n");
        FreeLibrary(N.core), N.core = NULL;
        return false;
    }
#endif
    N.loaded = true;
    return true;
}

void ngxvk_extensions(const char *const **inst, unsigned *ninst, const char *const **dev, unsigned *ndev)
{
    const char **ie = NULL, **de = NULL;
    unsigned ni = 0, nd = 0;
    *inst = *dev = NULL, *ninst = *ndev = 0;
    if (!N.loaded) return;
#ifdef _WIN32
    NVSDK_NGX_Result r = N.reqext(&ni, &ie, &nd, &de);
#else
    NVSDK_NGX_Result r = NVSDK_NGX_VULKAN_RequiredExtensions(&ni, &ie, &nd, &de);
#endif
    if (r != NVSDK_NGX_Result_Success) {
        printf("ngx: the Vulkan extensions NGX needs could not be read (0x%08X)\n", (unsigned)r);
        return;
    }
    *inst = (const char *const *)ie, *ninst = ni, *dev = (const char *const *)de, *ndev = nd;
}

bool ngxvk_start(VkInstance inst, VkPhysicalDevice phys, VkDevice dev, PFN_vkGetInstanceProcAddr gipa,
                 PFN_vkGetDeviceProcAddr gdpa, const wchar_t *data_dir, const wchar_t *const *search, int nsearch)
{
    if (!N.loaded || N.started) return N.started;
    memset(&N.info, 0, sizeof N.info);
    N.info.PathListInfo.Path = search;
    N.info.PathListInfo.Length = (unsigned)nsearch;
    N.info.InternalData = (NVSDK_NGX_FeatureCommonInfo_Internal *)N.internal;
    /* NGX's own log (to its file in data_dir, and the console) only when SHOCK_NGX_LOG is set, as ngx.c */
    N.info.LoggingInfo.MinimumLoggingLevel = getenv("SHOCK_NGX_LOG") ? NVSDK_NGX_LOGGING_LEVEL_VERBOSE : NVSDK_NGX_LOGGING_LEVEL_OFF;
#ifdef _WIN32
    NVSDK_NGX_Result r = N.init(APP_ID, data_dir, inst, phys, dev, gipa, gdpa, NVSDK_NGX_Version_API, &N.info);
#else
    NVSDK_NGX_Result r = NVSDK_NGX_VULKAN_Init(APP_ID, data_dir, inst, phys, dev, gipa, gdpa, &N.info, NVSDK_NGX_Version_API);
#endif
    if (r != NVSDK_NGX_Result_Success) {
        printf("ngx: the Vulkan Init failed 0x%08X\n", (unsigned)r);
        return false;
    }
    N.started = true, N.dev = dev;
    NVSDK_NGX_Parameter *caps = NULL;
    int available = 0;
    if (NGX_CAPS(&caps) == NVSDK_NGX_Result_Success && caps) P_GETI(caps, "SuperSampling.Available", &available);
    N.dlss_ok = available == 1;
    if (NGX_ALLOC(&N.params) != NVSDK_NGX_Result_Success) N.params = NULL;
    printf("ngx: started on Vulkan, DLSS %s\n", N.dlss_ok ? "available" : "NOT available");
    return true;
}

bool ngxvk_dlss_available(void) { return N.started && N.dlss_ok && N.params; }

/* a picture as NGX takes it; rw: the output, which NGX writes */
static void res_of(NVSDK_NGX_Resource_VK *r, const NgxImage *m, bool rw)
{
    memset(r, 0, sizeof *r);
    r->Type = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW;
    r->Resource.ImageViewInfo.ImageView = m->view;
    r->Resource.ImageViewInfo.Image = m->image;
    r->Resource.ImageViewInfo.SubresourceRange = (VkImageSubresourceRange){m->aspect, 0, 1, 0, 1};
    r->Resource.ImageViewInfo.Format = m->format;
    r->Resource.ImageViewInfo.Width = (unsigned)m->w;
    r->Resource.ImageViewInfo.Height = (unsigned)m->h;
    r->ReadWrite = rw;
}

/* what every feature's creation is given (ngx.c's) */
static void create_common(NVSDK_NGX_Parameter *p, int rw, int rh, int ow, int oh, int quality)
{
    P_RESET(p);
    P_UI(p, "CreationNodeMask", 1);
    P_UI(p, "VisibilityNodeMask", 1);
    P_UI(p, "Width", (unsigned)rw);
    P_UI(p, "Height", (unsigned)rh);
    P_UI(p, "OutWidth", (unsigned)ow);
    P_UI(p, "OutHeight", (unsigned)oh);
    P_I(p, "PerfQualityValue", quality);
    P_I(p, "DLSS.Feature.Create.Flags",
        NVSDK_NGX_DLSS_Feature_Flags_MVLowRes | NVSDK_NGX_DLSS_Feature_Flags_IsHDR | NVSDK_NGX_DLSS_Feature_Flags_AutoExposure);
}

bool ngxvk_dlss_create(VkDevice dev, VkCommandBuffer cmd, int rw, int rh, int ow, int oh, int quality)
{
    if (!ngxvk_dlss_available()) return false;
    NVSDK_NGX_Parameter *p = N.params;
    create_common(p, rw, rh, ow, oh, quality);
    /* what NVIDIA's loader fills in when unset (Populate_Unspecified_Defaults): the default presets */
    static const char *presets[] = {"DLSS.Hint.Render.Preset.DLAA", "DLSS.Hint.Render.Preset.Quality",
                                    "DLSS.Hint.Render.Preset.Balanced", "DLSS.Hint.Render.Preset.Performance",
                                    "DLSS.Hint.Render.Preset.UltraPerformance", "DLSS.Hint.Render.Preset.UltraQuality"};
    for (int i = 0; i < 6; i++) P_UI(p, presets[i], 0);
    NVSDK_NGX_Result r = NGX_CREATE(dev, cmd, NVSDK_NGX_Feature_SuperSampling, p, &N.dlss);
    if (r != NVSDK_NGX_Result_Success) {
        printf("ngx: DLSS create failed 0x%08X\n", (unsigned)r);
        N.dlss = NULL;
        return false;
    }
    printf("ngx: DLSS created %dx%d -> %dx%d (quality %d)\n", rw, rh, ow, oh, quality);
    return true;
}

bool ngxvk_dlss_evaluate(VkCommandBuffer cmd, const NgxImage *color, const NgxImage *depth, const NgxImage *mv,
                         const NgxImage *output, float jx, float jy, int rw, int rh, bool reset)
{
    if (!N.dlss) return false;
    NVSDK_NGX_Parameter *p = N.params;
    static NVSDK_NGX_Resource_VK rc, ro, rd, rm;   /* (kept: NGX reads them at the evaluate) */
    res_of(&rc, color, false), res_of(&ro, output, true), res_of(&rd, depth, false), res_of(&rm, mv, false);
    P_VOID(p, "Color", &rc);
    P_VOID(p, "Output", &ro);
    P_VOID(p, "Depth", &rd);
    P_VOID(p, "MotionVectors", &rm);
    P_F(p, "Jitter.Offset.X", jx);
    P_F(p, "Jitter.Offset.Y", jy);
    /* UW_MV_SCALE (a test, ngx.c's): scales the motion vectors DLSS is given; 0 = as if nothing moved */
    static float mv_scale = -1;
    if (mv_scale < 0) {
        const char *v = getenv("UW_MV_SCALE");
        mv_scale = v && *v ? (float)atof(v) : 1.0f;
    }
    P_F(p, "MV.Scale.X", mv_scale);
    P_F(p, "MV.Scale.Y", mv_scale);
    P_I(p, "Reset", reset ? 1 : 0);
    P_UI(p, "DLSS.Render.Subrect.Dimensions.Width", (unsigned)rw);
    P_UI(p, "DLSS.Render.Subrect.Dimensions.Height", (unsigned)rh);
    NVSDK_NGX_Result r = NGX_EVALUATE(cmd, N.dlss, p);
    if (r != NVSDK_NGX_Result_Success) {
        static int told;
        if (!told++) printf("ngx: DLSS evaluate failed 0x%08X\n", (unsigned)r);
        return false;
    }
    return true;
}

void ngxvk_dlss_release(void)
{
    if (N.dlss) NGX_RELEASE(N.dlss);
    N.dlss = NULL;
}

/* --- DLSS Ray Reconstruction ---------------------------------------------------------------------------------- */
static struct {
    NVSDK_NGX_Parameter *params; /* its own: the guides it is given are not DLSS upscaling's */
    NVSDK_NGX_Handle *handle;
    bool failed;
} RR;

bool ngxvk_rr_ready(void) { return RR.handle != NULL; }

bool ngxvk_rr_create(VkDevice dev, VkCommandBuffer cmd, int rw, int rh, int ow, int oh, int quality)
{
    if (RR.handle) return true;
    if (!N.started || RR.failed) return false;
    if (!RR.params && NGX_ALLOC(&RR.params) != NVSDK_NGX_Result_Success) RR.params = NULL;
    if (!RR.params) return RR.failed = true, false;
    NVSDK_NGX_Parameter *p = RR.params;
    create_common(p, rw, rh, ow, oh, quality);
    P_I(p, "DLSS.Enable.Output.Subrects", 0);
    P_I(p, "DLSS.Denoise.Mode", 1);    /* DL unified */
    P_UI(p, "DLSS.Roughness.Mode", 1); /* packed in the normals' w */
    P_UI(p, "DLSS.Use.HW.Depth", 1);   /* the depth buffer as drawn */
    static const char *presets[] = {"RayReconstruction.Hint.Render.Preset.DLAA", "RayReconstruction.Hint.Render.Preset.Quality",
                                    "RayReconstruction.Hint.Render.Preset.Balanced", "RayReconstruction.Hint.Render.Preset.Performance",
                                    "RayReconstruction.Hint.Render.Preset.UltraPerformance", "RayReconstruction.Hint.Render.Preset.UltraQuality"};
    for (int i = 0; i < 6; i++) P_UI(p, presets[i], 0);
    NVSDK_NGX_Result r = NGX_CREATE(dev, cmd, NVSDK_NGX_Feature_RayReconstruction, p, &RR.handle);
    if (r != NVSDK_NGX_Result_Success) {
        printf("ngx: Ray Reconstruction create failed 0x%08X (its library missing, or the driver is too old)\n", (unsigned)r);
        RR.handle = NULL, RR.failed = true;
        return false;
    }
    printf("ngx: Ray Reconstruction created %dx%d -> %dx%d (quality %d)\n", rw, rh, ow, oh, quality);
    return true;
}

bool ngxvk_rr_evaluate(VkCommandBuffer cmd, const NgxImage *color, const NgxImage *depth, const NgxImage *mv,
                       const NgxImage *diffuse, const NgxImage *specular, const NgxImage *normal_rough,
                       const NgxImage *output, float jx, float jy, int rw, int rh, float *w2v, float *v2c, bool reset)
{
    if (!RR.handle) return false;
    NVSDK_NGX_Parameter *p = RR.params;
    static NVSDK_NGX_Resource_VK rc, ro, rd, rm, ra, rs, rn;
    res_of(&rc, color, false), res_of(&ro, output, true), res_of(&rd, depth, false), res_of(&rm, mv, false);
    res_of(&ra, diffuse, false), res_of(&rs, specular, false), res_of(&rn, normal_rough, false);
    P_VOID(p, "Color", &rc);
    P_VOID(p, "Output", &ro);
    P_VOID(p, "Depth", &rd);
    P_VOID(p, "MotionVectors", &rm);
    P_VOID(p, "DLSS.Input.DiffuseAlbedo", &ra);
    P_VOID(p, "DLSS.Input.SpecularAlbedo", &rs);
    P_VOID(p, "GBuffer.Normals", &rn);
    P_VOID(p, "GBuffer.Roughness", &rn);
    P_VOID(p, "WorldToViewMatrix", w2v);
    P_VOID(p, "ViewToClipMatrix", v2c);
    P_F(p, "Jitter.Offset.X", jx);
    P_F(p, "Jitter.Offset.Y", jy);
    P_F(p, "MV.Scale.X", 1.0f);
    P_F(p, "MV.Scale.Y", 1.0f);
    P_I(p, "Reset", reset ? 1 : 0);
    P_F(p, "DLSS.Pre.Exposure", 1.0f);
    P_F(p, "DLSS.Exposure.Scale", 1.0f);
    P_UI(p, "DLSS.Render.Subrect.Dimensions.Width", (unsigned)rw);
    P_UI(p, "DLSS.Render.Subrect.Dimensions.Height", (unsigned)rh);
    NVSDK_NGX_Result r = NGX_EVALUATE(cmd, RR.handle, p);
    if (r != NVSDK_NGX_Result_Success) {
        static int told;
        if (!told++) printf("ngx: Ray Reconstruction evaluate failed 0x%08X\n", (unsigned)r);
        return false;
    }
    return true;
}

void ngxvk_rr_release(void)
{
    if (RR.handle) NGX_RELEASE(RR.handle);
    RR.handle = NULL;
    if (RR.params) NGX_DESTROY(RR.params);
    RR.params = NULL, RR.failed = false;
}

void ngxvk_stop(VkDevice dev)
{
    if (!N.started) return;
    ngxvk_rr_release();
    ngxvk_dlss_release();
    if (N.params) NGX_DESTROY(N.params);
    N.params = NULL;
#ifdef _WIN32
    N.shutdown(dev);
#else
    NVSDK_NGX_VULKAN_Shutdown1(dev);
#endif
    N.started = N.dlss_ok = false;
}
