#include "ngx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "nvsdk_ngx_defs.h"

/* The driver's exports, with the signatures NVIDIA's loader uses (some differ from the header). */
typedef NVSDK_NGX_Result (*PFN_InitExt)(unsigned long long, const wchar_t *, ID3D12Device *, NVSDK_NGX_Version,
                                        const NVSDK_NGX_FeatureCommonInfo *);
typedef NVSDK_NGX_Result (*PFN_Shutdown1)(ID3D12Device *, int *);
typedef NVSDK_NGX_Result (*PFN_GetCaps)(NVSDK_NGX_Parameter **);
typedef NVSDK_NGX_Result (*PFN_AllocParams)(NVSDK_NGX_Parameter **);
typedef NVSDK_NGX_Result (*PFN_DestroyParams)(NVSDK_NGX_Parameter *);
typedef NVSDK_NGX_Result (*PFN_Create)(ID3D12GraphicsCommandList *, NVSDK_NGX_Feature, NVSDK_NGX_Parameter *, NVSDK_NGX_Handle **);
typedef NVSDK_NGX_Result (*PFN_Evaluate)(ID3D12GraphicsCommandList *, const NVSDK_NGX_Handle *, const NVSDK_NGX_Parameter *, void *);
typedef NVSDK_NGX_Result (*PFN_Release)(NVSDK_NGX_Handle *);

/* The parameter object is an MSVC C++ class; its 17 virtual functions, in MSVC's slot order
 * (overloads reversed), read from the loader's NVSDK_NGX_Parameter_* wrappers. */
typedef struct {
    void (*SetVoid)(NVSDK_NGX_Parameter *, const char *, void *);
    void (*SetD3D12)(NVSDK_NGX_Parameter *, const char *, ID3D12Resource *);
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

static const unsigned long long APP_ID = 231313132ull; /* the SDK samples' development id */

static struct {
    HMODULE core;
    PFN_InitExt init;
    PFN_Shutdown1 shutdown;
    PFN_GetCaps caps;
    PFN_AllocParams alloc;
    PFN_DestroyParams destroy;
    PFN_Create create;
    PFN_Evaluate evaluate;
    PFN_Release release;
    NVSDK_NGX_Parameter *params;
    NVSDK_NGX_Handle *dlss;
    bool started, dlss_ok;
    unsigned char internal[16];
    NVSDK_NGX_FeatureCommonInfo info;
} N;

bool ngx_start(ID3D12Device *dev, const wchar_t *data_dir, const wchar_t *const *search, int nsearch) {
    wchar_t core[MAX_PATH] = {0};
    DWORD sz = sizeof core;
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\NVIDIA Corporation\\Global\\NGXCore", L"FullPath",
                     RRF_RT_REG_SZ, NULL, core, &sz) != ERROR_SUCCESS) {
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
    GET(init, PFN_InitExt, "NVSDK_NGX_D3D12_Init_Ext");
    GET(shutdown, PFN_Shutdown1, "NVSDK_NGX_D3D12_Shutdown1");
    GET(caps, PFN_GetCaps, "NVSDK_NGX_D3D12_GetCapabilityParameters");
    GET(alloc, PFN_AllocParams, "NVSDK_NGX_D3D12_AllocateParameters");
    GET(destroy, PFN_DestroyParams, "NVSDK_NGX_D3D12_DestroyParameters");
    GET(create, PFN_Create, "NVSDK_NGX_D3D12_CreateFeature");
    GET(evaluate, PFN_Evaluate, "NVSDK_NGX_D3D12_EvaluateFeature");
    GET(release, PFN_Release, "NVSDK_NGX_D3D12_ReleaseFeature");
#undef GET
    if (!N.init || !N.shutdown || !N.caps || !N.alloc || !N.destroy || !N.create || !N.evaluate || !N.release) {
        printf("ngx: an NGXCore export is missing\n");
        return false;
    }
    memset(&N.info, 0, sizeof N.info);
    N.info.PathListInfo.Path = search;
    N.info.PathListInfo.Length = (unsigned)nsearch;
    N.info.InternalData = (NVSDK_NGX_FeatureCommonInfo_Internal *)N.internal;
    /* NGX's own log (to its file in data_dir, and the console) only when SHOCK_NGX_LOG is set */
    N.info.LoggingInfo.MinimumLoggingLevel =
        GetEnvironmentVariableA("SHOCK_NGX_LOG", NULL, 0) ? NVSDK_NGX_LOGGING_LEVEL_VERBOSE : NVSDK_NGX_LOGGING_LEVEL_OFF;
    NVSDK_NGX_Result r = N.init(APP_ID, data_dir, dev, NVSDK_NGX_Version_API, &N.info);
    if (r != NVSDK_NGX_Result_Success) {
        printf("ngx: Init_Ext failed 0x%08X\n", (unsigned)r);
        return false;
    }
    N.started = true;
    NVSDK_NGX_Parameter *caps = NULL;
    int available = 0;
    if (N.caps(&caps) == NVSDK_NGX_Result_Success && caps)
        VT(caps)->GetI(caps, "SuperSampling.Available", &available);
    N.dlss_ok = available == 1;
    if (N.alloc(&N.params) != NVSDK_NGX_Result_Success) N.params = NULL;
    printf("ngx: started, DLSS %s\n", N.dlss_ok ? "available" : "NOT available");
    return true;
}

bool ngx_dlss_available(void) { return N.started && N.dlss_ok && N.params; }

bool ngx_dlss_create(ID3D12GraphicsCommandList *cl, int rw, int rh, int ow, int oh, int quality) {
    if (!ngx_dlss_available()) return false;
    NVSDK_NGX_Parameter *p = N.params;
    VT(p)->Reset(p);
    VT(p)->SetUI(p, "CreationNodeMask", 1);
    VT(p)->SetUI(p, "VisibilityNodeMask", 1);
    VT(p)->SetUI(p, "Width", (unsigned)rw);
    VT(p)->SetUI(p, "Height", (unsigned)rh);
    VT(p)->SetUI(p, "OutWidth", (unsigned)ow);
    VT(p)->SetUI(p, "OutHeight", (unsigned)oh);
    VT(p)->SetI(p, "PerfQualityValue", quality);
    /* display-referred colour (0..1, so not HDR), motion vectors at render size, depth not inverted */
    VT(p)->SetI(p, "DLSS.Feature.Create.Flags",
                NVSDK_NGX_DLSS_Feature_Flags_MVLowRes | NVSDK_NGX_DLSS_Feature_Flags_IsHDR | NVSDK_NGX_DLSS_Feature_Flags_AutoExposure);
    /* what NVIDIA's loader fills in when unset (Populate_Unspecified_Defaults): the default presets */
    static const char *presets[] = {"DLSS.Hint.Render.Preset.DLAA", "DLSS.Hint.Render.Preset.Quality",
                                    "DLSS.Hint.Render.Preset.Balanced", "DLSS.Hint.Render.Preset.Performance",
                                    "DLSS.Hint.Render.Preset.UltraPerformance", "DLSS.Hint.Render.Preset.UltraQuality"};
    for (int i = 0; i < 6; i++) VT(p)->SetUI(p, presets[i], 0);
    NVSDK_NGX_Result r = N.create(cl, NVSDK_NGX_Feature_SuperSampling, p, &N.dlss);
    if (r != NVSDK_NGX_Result_Success) {
        printf("ngx: DLSS create failed 0x%08X\n", (unsigned)r);
        N.dlss = NULL;
        return false;
    }
    printf("ngx: DLSS created %dx%d -> %dx%d (quality %d)\n", rw, rh, ow, oh, quality);
    return true;
}

bool ngx_dlss_evaluate(ID3D12GraphicsCommandList *cl, ID3D12Resource *color, ID3D12Resource *depth, ID3D12Resource *mv,
                       ID3D12Resource *output, float jx, float jy, int rw, int rh, bool reset) {
    if (!N.dlss) return false;
    NVSDK_NGX_Parameter *p = N.params;
    VT(p)->SetD3D12(p, "Color", color);
    VT(p)->SetD3D12(p, "Output", output);
    VT(p)->SetD3D12(p, "Depth", depth);
    VT(p)->SetD3D12(p, "MotionVectors", mv);
    VT(p)->SetF(p, "Jitter.Offset.X", jx);
    VT(p)->SetF(p, "Jitter.Offset.Y", jy);
    /* UW_MV_SCALE (a test): scales the motion vectors DLSS is given; 0 = as if nothing moved */
    static float mv_scale = -1;
    if (mv_scale < 0) {
        char v[16] = {0};
        mv_scale = GetEnvironmentVariableA("UW_MV_SCALE", v, sizeof v) ? (float)atof(v) : 1.0f;
    }
    VT(p)->SetF(p, "MV.Scale.X", mv_scale);
    VT(p)->SetF(p, "MV.Scale.Y", mv_scale);
    VT(p)->SetI(p, "Reset", reset ? 1 : 0);
    VT(p)->SetUI(p, "DLSS.Render.Subrect.Dimensions.Width", (unsigned)rw);
    VT(p)->SetUI(p, "DLSS.Render.Subrect.Dimensions.Height", (unsigned)rh);
    NVSDK_NGX_Result r = N.evaluate(cl, N.dlss, p, NULL);
    if (r != NVSDK_NGX_Result_Success) {
        printf("ngx: DLSS evaluate failed 0x%08X\n", (unsigned)r);
        return false;
    }
    return true;
}

void ngx_dlss_release(void) {
    if (N.dlss) N.release(N.dlss);
    N.dlss = NULL;
}

/* --- DLSS Ray Reconstruction ------------------------------------------------------------------ */
static struct {
    NVSDK_NGX_Parameter *params; /* its own: the guides it is given are not DLSS upscaling's */
    NVSDK_NGX_Handle *handle;
    bool failed;
} RR;

bool ngx_rr_ready(void) { return RR.handle != NULL; }

bool ngx_rr_create(ID3D12GraphicsCommandList *cl, int rw, int rh, int ow, int oh, int quality) {
    if (RR.handle) return true;
    if (!N.started || RR.failed) return false;
    if (!RR.params && N.alloc(&RR.params) != NVSDK_NGX_Result_Success) RR.params = NULL;
    if (!RR.params) return RR.failed = true, false;
    NVSDK_NGX_Parameter *p = RR.params;
    VT(p)->Reset(p);
    VT(p)->SetUI(p, "CreationNodeMask", 1);
    VT(p)->SetUI(p, "VisibilityNodeMask", 1);
    VT(p)->SetUI(p, "Width", (unsigned)rw);
    VT(p)->SetUI(p, "Height", (unsigned)rh);
    VT(p)->SetUI(p, "OutWidth", (unsigned)ow);
    VT(p)->SetUI(p, "OutHeight", (unsigned)oh);
    VT(p)->SetI(p, "PerfQualityValue", quality);
    VT(p)->SetI(p, "DLSS.Feature.Create.Flags",
                NVSDK_NGX_DLSS_Feature_Flags_MVLowRes | NVSDK_NGX_DLSS_Feature_Flags_IsHDR | NVSDK_NGX_DLSS_Feature_Flags_AutoExposure);
    VT(p)->SetI(p, "DLSS.Enable.Output.Subrects", 0);
    VT(p)->SetI(p, "DLSS.Denoise.Mode", 1);    /* DL unified */
    VT(p)->SetUI(p, "DLSS.Roughness.Mode", 1); /* packed in the normals' w */
    VT(p)->SetUI(p, "DLSS.Use.HW.Depth", 1);   /* the depth buffer as drawn */
    static const char *presets[] = {"RayReconstruction.Hint.Render.Preset.DLAA", "RayReconstruction.Hint.Render.Preset.Quality",
                                    "RayReconstruction.Hint.Render.Preset.Balanced", "RayReconstruction.Hint.Render.Preset.Performance",
                                    "RayReconstruction.Hint.Render.Preset.UltraPerformance", "RayReconstruction.Hint.Render.Preset.UltraQuality"};
    for (int i = 0; i < 6; i++) VT(p)->SetUI(p, presets[i], 0);
    NVSDK_NGX_Result r = N.create(cl, NVSDK_NGX_Feature_RayReconstruction, p, &RR.handle);
    if (r != NVSDK_NGX_Result_Success) {
        printf("ngx: Ray Reconstruction create failed 0x%08X (nvngx_dlssd.dll missing, or the driver is too old)\n", (unsigned)r);
        RR.handle = NULL, RR.failed = true;
        return false;
    }
    printf("ngx: Ray Reconstruction created %dx%d -> %dx%d (quality %d)\n", rw, rh, ow, oh, quality);
    return true;
}

bool ngx_rr_evaluate(ID3D12GraphicsCommandList *cl, ID3D12Resource *color, ID3D12Resource *depth, ID3D12Resource *mv,
                     ID3D12Resource *diffuse, ID3D12Resource *specular, ID3D12Resource *normal_rough, ID3D12Resource *output,
                     float jx, float jy, int rw, int rh, float *w2v, float *v2c, bool reset) {
    if (!RR.handle) return false;
    NVSDK_NGX_Parameter *p = RR.params;
    VT(p)->SetD3D12(p, "Color", color);
    VT(p)->SetD3D12(p, "Output", output);
    VT(p)->SetD3D12(p, "Depth", depth);
    VT(p)->SetD3D12(p, "MotionVectors", mv);
    VT(p)->SetD3D12(p, "DLSS.Input.DiffuseAlbedo", diffuse);
    VT(p)->SetD3D12(p, "DLSS.Input.SpecularAlbedo", specular);
    VT(p)->SetD3D12(p, "GBuffer.Normals", normal_rough);
    VT(p)->SetD3D12(p, "GBuffer.Roughness", normal_rough);
    VT(p)->SetVoid(p, "WorldToViewMatrix", w2v);
    VT(p)->SetVoid(p, "ViewToClipMatrix", v2c);
    VT(p)->SetF(p, "Jitter.Offset.X", jx);
    VT(p)->SetF(p, "Jitter.Offset.Y", jy);
    VT(p)->SetF(p, "MV.Scale.X", 1.0f);
    VT(p)->SetF(p, "MV.Scale.Y", 1.0f);
    VT(p)->SetI(p, "Reset", reset ? 1 : 0);
    VT(p)->SetF(p, "DLSS.Pre.Exposure", 1.0f);
    VT(p)->SetF(p, "DLSS.Exposure.Scale", 1.0f);
    VT(p)->SetUI(p, "DLSS.Render.Subrect.Dimensions.Width", (unsigned)rw);
    VT(p)->SetUI(p, "DLSS.Render.Subrect.Dimensions.Height", (unsigned)rh);
    NVSDK_NGX_Result r = N.evaluate(cl, RR.handle, p, NULL);
    if (r != NVSDK_NGX_Result_Success) {
        static int told;
        if (!told++) printf("ngx: Ray Reconstruction evaluate failed 0x%08X\n", (unsigned)r);
        return false;
    }
    return true;
}

void ngx_rr_release(void) {
    if (RR.handle) N.release(RR.handle);
    RR.handle = NULL;
    if (RR.params && N.destroy) N.destroy(RR.params);
    RR.params = NULL;
}

/* --- DLSS 5 neural rendering ------------------------------------------------------------------ */
typedef NVSDK_NGX_Result (*PFN_SnippetInit)(unsigned long long, const wchar_t *, ID3D12Device *, int, NVSDK_NGX_Parameter *);
typedef const char *(*PFN_LastError)(void);

/* the calls go through the engine's bridge, uw_nvngx.dll (nrbridge.c): the neural DLL accepts calls
 * only from a module whose path contains "nvngx.dll" */
typedef int (*PFN_BLoad)(const wchar_t *);
typedef int (*PFN_BInit)(unsigned long long, const wchar_t *, void *, int, void *);
typedef int (*PFN_BCreate)(void *, int, void *, void **);
typedef int (*PFN_BEvaluate)(void *, const void *, const void *);
typedef int (*PFN_BRelease)(void *);
static struct {
    HMODULE dll;
    PFN_BLoad load;
    PFN_BInit init;
    PFN_BCreate create;
    PFN_BEvaluate evaluate;
    PFN_BRelease release;
    PFN_LastError last_error;
    NVSDK_NGX_Parameter *params;
    NVSDK_NGX_Handle *handle[3]; /* one model per pass: each keeps its own history */
    int passes;
    NrSettings s;
    char error[200];
} NR;

const char *ngx_nr_error(void) { return NR.error; }
int ngx_nr_passes(void) { return NR.passes; }
void ngx_nr_set_params(float intensity, float structure, float tone) {
    NR.s.intensity = intensity, NR.s.local_structure = structure, NR.s.local_tone = tone;
}
void ngx_nr_set_style(int style) { NR.s.style = style; }
int ngx_nr_style(void) { return NR.s.style; }

static void nr_fail(const char *what, NVSDK_NGX_Result r) {
    const char *detail = NR.last_error ? NR.last_error() : NULL;
    snprintf(NR.error, sizeof NR.error, "%s (0x%08X)%s%s", what, (unsigned)r, detail && *detail ? ": " : "", detail ? detail : "");
    printf("ngx: DLSS 5 %s\n", NR.error);
}

bool ngx_nr_start(ID3D12Device *dev, ID3D12GraphicsCommandList *cl, const wchar_t *dll, const wchar_t *data_dir,
                  int width, int height, const NrSettings *s, int passes) {
    NR.s = *s;
    if (!N.started) return nr_fail("needs NGX started first", 0), false;
    wchar_t bridge[MAX_PATH];
    GetModuleFileNameW(NULL, bridge, MAX_PATH);
    wchar_t *slash = wcsrchr(bridge, L'\\');
    if (slash) wcscpy(slash + 1, L"uw_nvngx.dll");
    NR.dll = LoadLibraryW(bridge);
    if (!NR.dll) return nr_fail("cannot load the engine's bridge uw_nvngx.dll", (NVSDK_NGX_Result)GetLastError()), false;
    NR.load = (PFN_BLoad)(void *)GetProcAddress(NR.dll, "nrb_load");
    NR.init = (PFN_BInit)(void *)GetProcAddress(NR.dll, "nrb_init");
    NR.create = (PFN_BCreate)(void *)GetProcAddress(NR.dll, "nrb_create");
    NR.evaluate = (PFN_BEvaluate)(void *)GetProcAddress(NR.dll, "nrb_evaluate");
    NR.release = (PFN_BRelease)(void *)GetProcAddress(NR.dll, "nrb_release");
    NR.last_error = (PFN_LastError)(void *)GetProcAddress(NR.dll, "nrb_error");
    if (!NR.load || !NR.init || !NR.create || !NR.evaluate || !NR.release) return nr_fail("the bridge is incomplete", 0), false;
    if (!NR.load(dll)) return nr_fail("cannot load the neural DLL (or an export is missing)", 0), false;
    if (N.alloc(&NR.params) != NVSDK_NGX_Result_Success || !NR.params) return nr_fail("no parameter object", 0), false;
    /* as the forwarder: its application id, NGX API version 0x15, the parameter object as fifth argument */
    NVSDK_NGX_Result r = (NVSDK_NGX_Result)NR.init(0x24480451ull, data_dir, dev, 0x15, NR.params);
    if (r != NVSDK_NGX_Result_Success) return nr_fail("model initialisation failed", r), false;
    NVSDK_NGX_Parameter *p = NR.params;
    VT(p)->SetI(p, "DLSSNR.Enabled", 1);
    VT(p)->SetI(p, "DLSSNR.Width", width);
    VT(p)->SetI(p, "DLSSNR.Height", height);
    VT(p)->SetI(p, "CreationNodeMask", 1);
    VT(p)->SetI(p, "VisibilityNodeMask", 1);
    VT(p)->SetI(p, "DLSSNR.Hint.Render.Preset", s->preset);
    VT(p)->SetF(p, "DLSSNR.Intensity", s->intensity);
    VT(p)->SetI(p, "DLSSNR.Style", s->style);
    VT(p)->SetF(p, "DLSSNR.LocalStructureStrength", s->local_structure);
    VT(p)->SetF(p, "DLSSNR.LocalToneStrength", s->local_tone);
    VT(p)->SetF(p, "DLSSNR.SkinStructureStrength", s->skin_structure);
    VT(p)->SetVoid(p, "DLSSNR.ControlMask", NULL);
    VT(p)->SetI(p, "DLSSNR.UseAutoMask", s->auto_mask != 0);
    VT(p)->SetI(p, "DLSSNR.UICorrection", 0);
    NR.passes = 0;
    for (int k = 0; k < passes && k < 3; k++) {
        r = (NVSDK_NGX_Result)NR.create(cl, 18, p, (void **)&NR.handle[k]);
        if (r != NVSDK_NGX_Result_Success || !NR.handle[k]) {
            NR.handle[k] = NULL;
            if (k == 0) return nr_fail("model feature creation failed", r), false;
            break;
        }
        NR.passes = k + 1;
    }
    printf("ngx: DLSS 5 neural rendering created at %dx%d, %d pass%s\n", width, height, NR.passes, NR.passes > 1 ? "es" : "");
    return true;
}

/* the part of the picture DLSS 5 works on (output pixels; 0 wide: all of it): the original's 3D window when the
 * port shows its own screen round our view. Given the whole frame there, the model's tone darkened the window. */
static int nr_sub[4];
void ngx_nr_subrect(int x, int y, int w, int h) { nr_sub[0] = x, nr_sub[1] = y, nr_sub[2] = w, nr_sub[3] = h; }

bool ngx_nr_evaluate(ID3D12GraphicsCommandList *cl, ID3D12Resource *color, ID3D12Resource *depth, ID3D12Resource *mv,
                     ID3D12Resource *output, int w, int h, int rw, int rh, float mv_scale, bool reset, int pass) {
    if (pass < 0 || pass >= NR.passes || !NR.handle[pass]) return false;
    NVSDK_NGX_Parameter *p = NR.params;
    VT(p)->SetVoid(p, "DLSSNR.Color", color);
    VT(p)->SetVoid(p, "DLSSNR.Depth", depth);
    VT(p)->SetVoid(p, "DLSSNR.MVec", mv);
    VT(p)->SetVoid(p, "DLSSNR.Output", output);
    VT(p)->SetI(p, "DLSSNR.Enabled", 1);
    VT(p)->SetI(p, "DLSSNR.Width", w);
    VT(p)->SetI(p, "DLSSNR.Height", h);
    VT(p)->SetI(p, "DLSSNR.DepthInverted", 0);
    VT(p)->SetI(p, "DLSSNR.Reset", reset ? 1 : 0);
    static const char *rects[][4] = {
        {"DLSSNR.ColorSubrectBaseX", "DLSSNR.ColorSubrectBaseY", "DLSSNR.ColorSubrectWidth", "DLSSNR.ColorSubrectHeight"},
        {"DLSSNR.OutputSubrectBaseX", "DLSSNR.OutputSubrectBaseY", "DLSSNR.OutputSubrectWidth", "DLSSNR.OutputSubrectHeight"},
        {"DLSSNR.DepthSubrectBaseX", "DLSSNR.DepthSubrectBaseY", "DLSSNR.DepthSubrectWidth", "DLSSNR.DepthSubrectHeight"},
        {"DLSSNR.MVecSubrectBaseX", "DLSSNR.MVecSubrectBaseY", "DLSSNR.MVecSubrectWidth", "DLSSNR.MVecSubrectHeight"},
    };
    int sizes[4][4] = {{0, 0, w, h}, {0, 0, w, h}, {0, 0, rw, rh}, {0, 0, rw, rh}};
    if (nr_sub[2] > 0 && nr_sub[3] > 0 && (nr_sub[2] < w || nr_sub[3] < h))
        for (int i = 0; i < 4; i++) {   /* the window: in output pixels for colour and output, render pixels for the rest */
            int sw = i < 2 ? 1 : rw, sh = i < 2 ? 1 : rh, dw = i < 2 ? 1 : w, dh = i < 2 ? 1 : h;
            sizes[i][0] = nr_sub[0] * sw / dw, sizes[i][1] = nr_sub[1] * sh / dh;
            sizes[i][2] = nr_sub[2] * sw / dw, sizes[i][3] = nr_sub[3] * sh / dh;
        }
    for (int i = 0; i < 4; i++) {
        VT(p)->SetI(p, rects[i][0], sizes[i][0]);
        VT(p)->SetI(p, rects[i][1], sizes[i][1]);
        VT(p)->SetI(p, rects[i][2], sizes[i][2]);
        VT(p)->SetI(p, rects[i][3], sizes[i][3]);
    }
    VT(p)->SetF(p, "DLSSNR.MVecScaleX", mv_scale);
    VT(p)->SetF(p, "DLSSNR.MVecScaleY", mv_scale);
    VT(p)->SetF(p, "DLSSNR.Intensity", NR.s.intensity);
    VT(p)->SetI(p, "DLSSNR.Style", NR.s.style);
    VT(p)->SetF(p, "DLSSNR.LocalStructureStrength", NR.s.local_structure);
    VT(p)->SetF(p, "DLSSNR.LocalToneStrength", NR.s.local_tone);
    VT(p)->SetF(p, "DLSSNR.SkinStructureStrength", NR.s.skin_structure);
    VT(p)->SetVoid(p, "DLSSNR.ControlMask", NULL);
    VT(p)->SetI(p, "DLSSNR.UseAutoMask", NR.s.auto_mask != 0);
    NVSDK_NGX_Result r = (NVSDK_NGX_Result)NR.evaluate(cl, NR.handle[pass], p);
    if (r != NVSDK_NGX_Result_Success) {
        nr_fail("model evaluation failed", r);
        return false;
    }
    return true;
}

void ngx_nr_stop(void) {
    for (int k = 0; k < 3; k++) {
        if (NR.handle[k] && NR.release) NR.release(NR.handle[k]);
        NR.handle[k] = NULL;
    }
    NR.passes = 0;
    if (NR.params && N.destroy) N.destroy(NR.params);
    NR.params = NULL;
}

void ngx_stop(ID3D12Device *dev) {
    if (!N.started) return;
    ngx_nr_stop();
    ngx_rr_release();
    ngx_dlss_release();
    if (N.params) N.destroy(N.params);
    N.params = NULL;
    int remaining = -1;
    N.shutdown(dev, &remaining);
    N.started = false;
}
