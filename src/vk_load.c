/* vk_load.c: the Vulkan loader opened at run time and the renderer's functions taken from it (vk_load.h). */
#include "vk_load.h"
#include <stddef.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
#define VKL_DEFINE(f) PFN_##f f;
VKL_GLOBAL(VKL_DEFINE)
VKL_INSTANCE(VKL_DEFINE)
VKL_DEBUG(VKL_DEFINE)
VKL_DEVICE(VKL_DEFINE)
VKL_RT(VKL_DEFINE)

static const char *g_missing = "";
const char *vkl_missing(void) { return g_missing; }

bool vkl_open(void)
{
    static void *lib;
    if (vkGetInstanceProcAddr) return true;
#ifdef _WIN32
    lib = (void *)LoadLibraryA("vulkan-1.dll");
    if (lib) vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)(void *)GetProcAddress((HMODULE)lib, "vkGetInstanceProcAddr");
#else
    lib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (lib) vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vkGetInstanceProcAddr");
#endif
    if (!vkGetInstanceProcAddr) return g_missing = "vkGetInstanceProcAddr (no Vulkan loader)", false;
#define VKL_GET_GLOBAL(f) if (!(f = (PFN_##f)vkGetInstanceProcAddr(NULL, #f))) return g_missing = #f, false;
    VKL_GLOBAL(VKL_GET_GLOBAL)
    return true;
}

bool vkl_instance(VkInstance inst)
{
#define VKL_GET_INSTANCE(f) if (!(f = (PFN_##f)vkGetInstanceProcAddr(inst, #f))) return g_missing = #f, false;
#define VKL_GET_OPTIONAL(f) f = (PFN_##f)vkGetInstanceProcAddr(inst, #f);
    VKL_INSTANCE(VKL_GET_INSTANCE)
    VKL_DEBUG(VKL_GET_OPTIONAL)
    return true;
}

bool vkl_device(VkDevice dev)
{
#define VKL_GET_DEVICE(f) if (!(f = (PFN_##f)vkGetDeviceProcAddr(dev, #f))) return g_missing = #f, false;
    VKL_DEVICE(VKL_GET_DEVICE)
#define VKL_GET_DEVICE_OPT(f) f = (PFN_##f)vkGetDeviceProcAddr(dev, #f);
    VKL_RT(VKL_GET_DEVICE_OPT)
    return true;
}
