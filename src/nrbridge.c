/* uw_nvngx.dll: the engine's bridge to the user's DLSS 5 neural DLL (nvngx_dlssnr.dll).
 *
 * That DLL's NVSDK_NGX_D3D12_Init_Ext checks who called it: GetModuleHandleExW(FROM_ADDRESS) on its
 * return address, then GetModuleFileNameW, and it refuses (0xBAD00002) unless the calling module's
 * path contains "nvngx.dll" (normally NVIDIA's NGX core; OptiScaler's forwarder is named
 * nvngx.dll_dlssnr.dll). So the engine makes those calls from this small DLL, whose file name
 * contains that text. The calls themselves are the ones the forwarder makes (decoded with REA).
 * Built with -fno-optimize-sibling-calls: every call here must return here, not jump, so that the
 * neural DLL sees this module as its caller. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef int (*PFN_Init)(unsigned long long, const wchar_t *, void *, int, void *);
typedef int (*PFN_Create)(void *, int, void *, void **);
typedef int (*PFN_Evaluate)(void *, const void *, const void *, void *);
typedef int (*PFN_Release)(void *);
typedef const char *(*PFN_Err)(void);

static HMODULE g_dll;
static PFN_Init g_init;
static PFN_Create g_create;
static PFN_Evaluate g_evaluate;
static PFN_Release g_release;
static PFN_Err g_err;

__declspec(dllexport) int nrb_load(const wchar_t *path) {
    g_dll = LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!g_dll) return 0;
    g_init = (PFN_Init)(void *)GetProcAddress(g_dll, "NVSDK_NGX_D3D12_Init_Ext");
    g_create = (PFN_Create)(void *)GetProcAddress(g_dll, "NVSDK_NGX_D3D12_CreateFeature");
    g_evaluate = (PFN_Evaluate)(void *)GetProcAddress(g_dll, "NVSDK_NGX_D3D12_EvaluateFeature");
    g_release = (PFN_Release)(void *)GetProcAddress(g_dll, "NVSDK_NGX_D3D12_ReleaseFeature");
    g_err = (PFN_Err)(void *)GetProcAddress(g_dll, "NVFP4_GetLastError");
    return g_init && g_create && g_evaluate && g_release;
}

__declspec(dllexport) int nrb_init(unsigned long long app, const wchar_t *data, void *device, int version, void *params) {
    volatile int r = g_init(app, data, device, version, params);
    return r;
}

__declspec(dllexport) int nrb_create(void *cl, int feature, void *params, void **handle) {
    volatile int r = g_create(cl, feature, params, handle);
    return r;
}

__declspec(dllexport) int nrb_evaluate(void *cl, const void *handle, const void *params) {
    volatile int r = g_evaluate(cl, handle, params, NULL);
    return r;
}

__declspec(dllexport) int nrb_release(void *handle) {
    volatile int r = g_release(handle);
    return r;
}

__declspec(dllexport) const char *nrb_error(void) { return g_err ? g_err() : ""; }
