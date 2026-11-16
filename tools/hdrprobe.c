/* hdrprobe.c: what HDR this Linux machine's Vulkan driver offers, by every route the renderer could take
 * (the user, 2026-11-16: HDR on Linux Mint "in our own way", and the Wayland way too). It draws nothing; it asks the
 * driver which picture formats and colour spaces it would give a swap chain, and prints them:
 *
 *   1. a window on X11 (Linux Mint's Cinnamon; under a Wayland desktop this is XWayland)
 *   2. a window on Wayland (a Wayland desktop only: KDE Plasma, GNOME)
 *   3. the monitor itself, without the desktop (VK_KHR_display): as the driver lists it, and with --take the monitor
 *      taken from X for a moment (VK_EXT_acquire_xlib_display), which is how a full-screen HDR mode of our own would
 *      have to work on X11. The screen may go black while it is taken; it is given back at once, and the program
 *      ends itself after 15 seconds whatever happens.
 *
 * A colour space past plain sRGB (HDR10 ST.2084, scRGB "extended sRGB linear", BT.2020) on a route means HDR can be
 * had that way; the last lines say which routes have one. Built by tools\build_hdrprobe.py (cross-built on Windows
 * against the Debian sysroot, as libuwview.so); run on Linux by hdrprobe.sh beside it. */
#define _DEFAULT_SOURCE
#include <dlfcn.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <SDL3/SDL.h>

/* X11's types as the Vulkan headers want them (the sysroot has no X11 headers) */
typedef struct _XDisplay Display;
typedef unsigned long Window;
typedef unsigned long VisualID;
typedef unsigned long RROutput;
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_xlib.h>
#include <vulkan/vulkan_wayland.h>
#include <vulkan/vulkan_xlib_xrandr.h>

/* Vulkan is opened at run time (libvulkan.so.1, as src\vk_load.c does: nothing of it is linked) and only the
   instance's functions are needed: nothing is drawn, so no device is made. FN(name) fetches one where it is used; a
   function the driver lacks comes back NULL and is checked there. */
static PFN_vkGetInstanceProcAddr gipa;
static VkInstance inst;
#define FN(name) PFN_##name name = (PFN_##name)gipa(inst, #name)

/* each route's answer in a line, printed together at the end (what the user reads first) */
static char verdict[3][160] = {"not tried", "not tried", "not tried"};
enum { R_X11, R_WAYLAND, R_DISPLAY };

static const char *fmt_name(VkFormat f)
{
    static char b[32];
    switch ((int)f) {
    case 4: return "R5G6B5_UNORM";
    case 37: return "R8G8B8A8_UNORM";
    case 43: return "R8G8B8A8_SRGB";
    case 44: return "B8G8R8A8_UNORM";
    case 50: return "B8G8R8A8_SRGB";
    case 58: return "A2R10G10B10_UNORM (10 bit)";
    case 64: return "A2B10G10R10_UNORM (10 bit)";
    case 91: return "R16G16B16A16_UNORM (16 bit)";
    case 97: return "R16G16B16A16_SFLOAT (FP16)";
    case 122: return "B10G11R11_UFLOAT";
    }
    snprintf(b, sizeof b, "format %d", (int)f);
    return b;
}

/* the colour space's name; *hdr: it is past plain sRGB's range (HDR can be shown through it) */
static const char *cs_name(VkColorSpaceKHR c, bool *hdr)
{
    static char b[32];
    *hdr = false;
    switch ((int)c) {
    case 0: return "SRGB_NONLINEAR (plain SDR)";
    case 1000104001: return "DISPLAY_P3_NONLINEAR";
    case 1000104002: *hdr = true; return "EXTENDED_SRGB_LINEAR (scRGB)";
    case 1000104003: return "DISPLAY_P3_LINEAR";
    case 1000104004: return "DCI_P3_NONLINEAR";
    case 1000104005: return "BT709_LINEAR";
    case 1000104006: return "BT709_NONLINEAR";
    case 1000104007: *hdr = true; return "BT2020_LINEAR";
    case 1000104008: *hdr = true; return "HDR10_ST2084 (HDR10)";
    case 1000104009: *hdr = true; return "DOLBYVISION";
    case 1000104010: *hdr = true; return "HDR10_HLG";
    case 1000104011: return "ADOBERGB_LINEAR";
    case 1000104012: return "ADOBERGB_NONLINEAR";
    case 1000104013: return "PASS_THROUGH";
    case 1000104014: *hdr = true; return "EXTENDED_SRGB_NONLINEAR";
    case 1000213000: return "DISPLAY_NATIVE_AMD";
    }
    snprintf(b, sizeof b, "colour space %d", (int)c);
    return b;
}

/* the formats one GPU offers a surface; returns how many are HDR (-1: the query failed), the first HDR one's names
   into first (when there is one) */
static int list_formats(VkPhysicalDevice pd, VkSurfaceKHR s, char *first, size_t nfirst)
{
    FN(vkGetPhysicalDeviceSurfaceFormatsKHR);
    FN(vkGetPhysicalDeviceSurfaceSupportKHR);
    VkBool32 sup = VK_FALSE;
    if (vkGetPhysicalDeviceSurfaceSupportKHR) vkGetPhysicalDeviceSurfaceSupportKHR(pd, 0, s, &sup);
    uint32_t n = 0;
    VkResult r = vkGetPhysicalDeviceSurfaceFormatsKHR(pd, s, &n, NULL);
    if (r < 0) return printf("      the formats could not be asked for (error %d)%s\n", (int)r, sup ? "" : "; this GPU cannot draw to it"), -1;
    VkSurfaceFormatKHR fm[256];
    if (n > 256) n = 256;
    vkGetPhysicalDeviceSurfaceFormatsKHR(pd, s, &n, fm);
    int hdr = 0;
    for (uint32_t i = 0; i < n; i++) {
        bool h;
        const char *cn = cs_name(fm[i].colorSpace, &h);
        printf("      %-30s %s%s\n", fmt_name(fm[i].format), cn, h ? "   <-- HDR" : "");
        if (h && !hdr++ && first) snprintf(first, nfirst, "%s, %s", fmt_name(fm[i].format), cn);
    }
    if (!n) printf("      (no formats)\n");
    return hdr;
}

/* the graphics cards Vulkan sees and their names (main): every route asks each of them */
static VkPhysicalDevice gpus[8];
static uint32_t ngpu;
static char gpu_name[8][256];

/* a window by one of SDL's video drivers, its Vulkan surface, the formats every GPU offers it */
static void window_route(int route, const char *driver, bool have_ext, void **xdpy_out)
{
    printf("\n== %d. A window on %s ==\n", route + 1, route == R_X11 ? "X11" : "Wayland");
    if (!have_ext) {
        printf("   The Vulkan driver has no %s windows at all.\n", route == R_X11 ? "X11" : "Wayland");
        snprintf(verdict[route], sizeof verdict[route], "no (the driver has no such windows)");
        return;
    }
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, driver);
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        printf("   Not available in this session (%s).\n", SDL_GetError());
        snprintf(verdict[route], sizeof verdict[route], "not available in this session");
        return;
    }
    SDL_Window *w = SDL_CreateWindow("hdrprobe", 320, 200, 0);
    if (!w) {
        printf("   No window (%s).\n", SDL_GetError());
        snprintf(verdict[route], sizeof verdict[route], "no window could be made");
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }
    for (int i = 0; i < 10; i++) SDL_PumpEvents(), SDL_Delay(20);
    SDL_PropertiesID pr = SDL_GetWindowProperties(w);
    VkSurfaceKHR s = VK_NULL_HANDLE;
    VkResult r = VK_ERROR_INITIALIZATION_FAILED;
    if (route == R_WAYLAND) {
        FN(vkCreateWaylandSurfaceKHR);
        VkWaylandSurfaceCreateInfoKHR si = {VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
                                            .display = SDL_GetPointerProperty(pr, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL),
                                            .surface = SDL_GetPointerProperty(pr, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL)};
        if (vkCreateWaylandSurfaceKHR && si.display && si.surface) r = vkCreateWaylandSurfaceKHR(inst, &si, NULL, &s);
    } else {
        FN(vkCreateXlibSurfaceKHR);
        VkXlibSurfaceCreateInfoKHR si = {VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
                                         .dpy = SDL_GetPointerProperty(pr, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL),
                                         .window = (Window)SDL_GetNumberProperty(pr, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0)};
        if (vkCreateXlibSurfaceKHR && si.dpy && si.window) r = vkCreateXlibSurfaceKHR(inst, &si, NULL, &s);
        if (xdpy_out) *xdpy_out = si.dpy;
    }
    if (r != VK_SUCCESS) {
        printf("   The window's Vulkan surface could not be made (error %d).\n", (int)r);
        snprintf(verdict[route], sizeof verdict[route], "the surface could not be made");
    } else {
        int best = 0;
        char first[96] = "";
        for (uint32_t g = 0; g < ngpu; g++) {
            printf("   %s offers:\n", gpu_name[g]);
            int h = list_formats(gpus[g], s, best ? NULL : first, sizeof first);
            if (h > best) best = h;
        }
        if (best) snprintf(verdict[route], sizeof verdict[route], "YES (%d HDR formats; e.g. %s)", best, first);
        else snprintf(verdict[route], sizeof verdict[route], "no (SDR formats only)");
        FN(vkDestroySurfaceKHR);
        vkDestroySurfaceKHR(inst, s, NULL);
    }
    /* (the X11 window and SDL's video stay up when the monitor test needs the X display: closed by main) */
    if (!(xdpy_out && *xdpy_out)) {
        SDL_DestroyWindow(w);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
}

/* the monitors as the driver lists them without a desktop (VK_KHR_display); take: each taken from X for the asking */
static void display_route(bool have_display, bool have_acquire, void *xdpy, bool take)
{
    printf("\n== 3. The monitor itself, without the desktop (VK_KHR_display) ==\n");
    if (!have_display) {
        printf("   The Vulkan driver has no VK_KHR_display.\n");
        snprintf(verdict[R_DISPLAY], sizeof verdict[R_DISPLAY], "no (the driver has no VK_KHR_display)");
        return;
    }
    FN(vkGetPhysicalDeviceDisplayPropertiesKHR);
    FN(vkGetPhysicalDeviceDisplayPlanePropertiesKHR);
    FN(vkGetDisplayPlaneSupportedDisplaysKHR);
    FN(vkGetDisplayModePropertiesKHR);
    FN(vkGetDisplayPlaneCapabilitiesKHR);
    FN(vkCreateDisplayPlaneSurfaceKHR);
    FN(vkDestroySurfaceKHR);
    FN(vkAcquireXlibDisplayEXT);
    FN(vkReleaseDisplayEXT);
    if (!vkGetPhysicalDeviceDisplayPropertiesKHR || !vkCreateDisplayPlaneSurfaceKHR) return;
    int best = -1, seen = 0;
    char first[96] = "";
    bool took = false;
    for (uint32_t g = 0; g < ngpu; g++) {
        uint32_t nd = 0;
        VkDisplayPropertiesKHR dp[8];
        vkGetPhysicalDeviceDisplayPropertiesKHR(gpus[g], &nd, NULL);
        if (nd > 8) nd = 8;
        vkGetPhysicalDeviceDisplayPropertiesKHR(gpus[g], &nd, dp);
        printf("   %s lists %u monitor(s)%s\n", gpu_name[g], nd, nd ? ":" : ".");
        uint32_t np = 0;
        VkDisplayPlanePropertiesKHR pp[32];
        vkGetPhysicalDeviceDisplayPlanePropertiesKHR(gpus[g], &np, NULL);
        if (np > 32) np = 32;
        vkGetPhysicalDeviceDisplayPlanePropertiesKHR(gpus[g], &np, pp);
        for (uint32_t d = 0; d < nd; d++) {
            seen++;
            uint32_t nm = 0;
            VkDisplayModePropertiesKHR mp[64];
            vkGetDisplayModePropertiesKHR(gpus[g], dp[d].display, &nm, NULL);
            if (nm > 64) nm = 64;
            vkGetDisplayModePropertiesKHR(gpus[g], dp[d].display, &nm, mp);
            printf("   - %s, %ux%u, %u mode(s)", dp[d].displayName ? dp[d].displayName : "(no name)", dp[d].physicalResolution.width,
                   dp[d].physicalResolution.height, nm);
            if (nm) printf(", first %ux%u at %.2f Hz", mp[0].parameters.visibleRegion.width, mp[0].parameters.visibleRegion.height,
                           (double)mp[0].parameters.refreshRate / 1000.0);
            printf("\n");
            if (!nm) continue;
            /* a plane that can show on this monitor */
            int plane = -1;
            for (uint32_t p = 0; p < np && plane < 0; p++) {
                uint32_t c = 0;
                VkDisplayKHR list[8];
                vkGetDisplayPlaneSupportedDisplaysKHR(gpus[g], p, &c, NULL);
                if (c > 8) c = 8;
                vkGetDisplayPlaneSupportedDisplaysKHR(gpus[g], p, &c, list);
                for (uint32_t k = 0; k < c; k++)
                    if (list[k] == dp[d].display) plane = (int)p;
            }
            if (plane < 0) { printf("     no plane of the GPU shows on it\n"); continue; }
            for (int pass = 0; pass < (take ? 2 : 1); pass++) {
                if (pass == 1) {
                    if (!have_acquire || !vkAcquireXlibDisplayEXT || !xdpy) {
                        printf("     taking it from X: not possible here (%s)\n", !xdpy ? "no X11 session" : "the driver has no VK_EXT_acquire_xlib_display");
                        break;
                    }
                    fflush(stdout);
                    VkResult ar = vkAcquireXlibDisplayEXT(gpus[g], (Display *)xdpy, dp[d].display);
                    if (ar != VK_SUCCESS) { printf("     taking it from X: refused (error %d)\n", (int)ar); break; }
                    took = true;
                    printf("     taken from X; as its owner the driver offers:\n");
                } else
                    printf("     as listed (the desktop still owns it), the driver offers:\n");
                VkDisplayPlaneCapabilitiesKHR caps;
                memset(&caps, 0, sizeof caps);
                if (vkGetDisplayPlaneCapabilitiesKHR) vkGetDisplayPlaneCapabilitiesKHR(gpus[g], mp[0].displayMode, (uint32_t)plane, &caps);
                VkDisplayPlaneAlphaFlagBitsKHR alpha = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
                for (int b = 0; b < 4; b++)
                    if (caps.supportedAlpha & (1u << b)) { alpha = (VkDisplayPlaneAlphaFlagBitsKHR)(1u << b); break; }
                VkDisplaySurfaceCreateInfoKHR ci = {VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR, .displayMode = mp[0].displayMode,
                                                    .planeIndex = (uint32_t)plane, .planeStackIndex = pp[plane].currentStackIndex,
                                                    .transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR, .globalAlpha = 1.0f, .alphaMode = alpha,
                                                    .imageExtent = mp[0].parameters.visibleRegion};
                VkSurfaceKHR s = VK_NULL_HANDLE;
                VkResult r = vkCreateDisplayPlaneSurfaceKHR(inst, &ci, NULL, &s);
                if (r != VK_SUCCESS) printf("      no surface on it (error %d)\n", (int)r);
                else {
                    int h = list_formats(gpus[g], s, best > 0 ? NULL : first, sizeof first);
                    if (h > best) best = h;
                    vkDestroySurfaceKHR(inst, s, NULL);
                }
                if (pass == 1 && vkReleaseDisplayEXT) {
                    vkReleaseDisplayEXT(gpus[g], dp[d].display);
                    printf("     given back to X\n");
                }
                fflush(stdout);
            }
        }
    }
    if (!seen) snprintf(verdict[R_DISPLAY], sizeof verdict[R_DISPLAY], "no (the driver lists no monitor this way)");
    else if (best > 0) snprintf(verdict[R_DISPLAY], sizeof verdict[R_DISPLAY], "YES (%d HDR formats; e.g. %s)", best, first);
    else if (best == 0) snprintf(verdict[R_DISPLAY], sizeof verdict[R_DISPLAY], "no (SDR formats only%s)", take && took ? ", also when taken from X" : take ? "; it could not be taken from X" : "; not yet tried with the monitor taken from X: --take");
    else snprintf(verdict[R_DISPLAY], sizeof verdict[R_DISPLAY], "no answer (no surface could be made on a monitor%s)", take ? "" : "; try --take");
}

/* name is among the n extensions e */
static bool has(const VkExtensionProperties *e, uint32_t n, const char *name)
{
    for (uint32_t i = 0; i < n; i++)
        if (!strcmp(e[i].extensionName, name)) return true;
    return false;
}

static const char *env_or(const char *name, const char *none)
{
    const char *e = getenv(name);
    return e && *e ? e : none;
}

/* the 15 seconds' end (main's alarm): should the driver hang with the monitor taken from X, the program still ends,
   and the driver gives the monitor back when its owner is gone. Only write and _exit: a signal handler's own. */
static void on_alarm(int sig)
{
    static const char m[] = "\nhdrprobe: 15 seconds are up: ending (the monitor goes back to the desktop).\n";
    (void)sig;
    if (write(1, m, sizeof m - 1)) {}
    _exit(3);
}

int main(int argc, char **argv)
{
    bool take = false;
    for (int i = 1; i < argc; i++) take = take || !strcmp(argv[i], "--take");
    setvbuf(stdout, NULL, _IOLBF, 0);
    signal(SIGALRM, on_alarm);
    alarm(15);
    printf("hdrprobe: what HDR this machine's Vulkan driver offers, by each route%s\n", take ? " (--take: the monitor taken from X for a moment)" : "");
    printf("session: %s, desktop %s, DISPLAY %s, WAYLAND_DISPLAY %s\n", env_or("XDG_SESSION_TYPE", "?"), env_or("XDG_CURRENT_DESKTOP", "?"),
           env_or("DISPLAY", "(none)"), env_or("WAYLAND_DISPLAY", "(none)"));

    /* the Vulkan loader, and an instance with every extension of the list below that the driver has: both kinds of
       window, the colour spaces past sRGB (without VK_EXT_swapchain_colorspace none is ever listed), the monitors */
    void *lib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (lib) gipa = (PFN_vkGetInstanceProcAddr)dlsym(lib, "vkGetInstanceProcAddr");
    if (!gipa) return printf("No Vulkan on this machine (libvulkan.so.1 not found): install the graphics driver.\n"), 1;
    PFN_vkEnumerateInstanceExtensionProperties eie = (PFN_vkEnumerateInstanceExtensionProperties)gipa(NULL, "vkEnumerateInstanceExtensionProperties");
    PFN_vkCreateInstance mk = (PFN_vkCreateInstance)gipa(NULL, "vkCreateInstance");
    if (!eie || !mk) return printf("The Vulkan loader is broken.\n"), 1;
    uint32_t na = 0;
    eie(NULL, &na, NULL);
    VkExtensionProperties *av = calloc(na ? na : 1, sizeof *av);
    if (!av) return 1;
    eie(NULL, &na, av);
    static const char *const WANT[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XLIB_SURFACE_EXTENSION_NAME, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
                                       VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME, VK_KHR_DISPLAY_EXTENSION_NAME,
                                       VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME, VK_EXT_ACQUIRE_XLIB_DISPLAY_EXTENSION_NAME,
                                       "VK_EXT_acquire_drm_display", "VK_KHR_get_surface_capabilities2", "VK_KHR_get_display_properties2"};
    const char *ext[16];
    uint32_t ne = 0;
    bool got[16];
    printf("\nThe Vulkan driver's parts that matter here:\n");
    for (size_t i = 0; i < sizeof WANT / sizeof *WANT; i++) {
        got[i] = has(av, na, WANT[i]);
        printf("   %-34s %s\n", WANT[i], got[i] ? "yes" : "NO");
        if (got[i]) ext[ne++] = WANT[i];
    }
    free(av);
    if (!got[0]) return printf("No VK_KHR_surface: this driver cannot draw to a screen at all.\n"), 1;
    if (!got[3]) printf("   (without VK_EXT_swapchain_colorspace no HDR colour space can be offered by any route)\n");
    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "hdrprobe", .apiVersion = VK_API_VERSION_1_1};
    VkInstanceCreateInfo ci = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app, .enabledExtensionCount = ne, .ppEnabledExtensionNames = ext};
    VkResult r = mk(&ci, NULL, &inst);
    if (r != VK_SUCCESS) return printf("Vulkan would not start (error %d).\n", (int)r), 1;

    /* the graphics cards: the name, the driver's version as its maker writes it (NVIDIA: 10 bits . 8 . 8), and
       whether it has swap chains and VK_EXT_hdr_metadata (the monitor told the picture's brightness: HDR10 needs it) */
    FN(vkEnumeratePhysicalDevices);
    FN(vkGetPhysicalDeviceProperties);
    FN(vkEnumerateDeviceExtensionProperties);
    ngpu = 8;
    vkEnumeratePhysicalDevices(inst, &ngpu, gpus);
    printf("\nThe graphics cards Vulkan sees:\n");
    for (uint32_t g = 0; g < ngpu; g++) {
        VkPhysicalDeviceProperties p;
        vkGetPhysicalDeviceProperties(gpus[g], &p);
        snprintf(gpu_name[g], sizeof gpu_name[g], "%s", p.deviceName);
        char drv[48];
        uint32_t v = p.driverVersion;
        if (p.vendorID == 0x10DE) snprintf(drv, sizeof drv, "NVIDIA driver %u.%u.%u", (v >> 22) & 0x3FF, (v >> 14) & 0xFF, (v >> 6) & 0xFF);
        else snprintf(drv, sizeof drv, "driver %u.%u.%u", VK_API_VERSION_MAJOR(v), VK_API_VERSION_MINOR(v), VK_API_VERSION_PATCH(v));
        uint32_t nd = 0;
        vkEnumerateDeviceExtensionProperties(gpus[g], NULL, &nd, NULL);
        VkExtensionProperties *de = calloc(nd ? nd : 1, sizeof *de);
        bool meta = false, sc = false;
        if (de) {
            vkEnumerateDeviceExtensionProperties(gpus[g], NULL, &nd, de);
            meta = has(de, nd, "VK_EXT_hdr_metadata"), sc = has(de, nd, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            free(de);
        }
        printf("   %s (%s, Vulkan %u.%u.%u): swap chains %s, HDR metadata (VK_EXT_hdr_metadata) %s\n", p.deviceName, drv,
               VK_API_VERSION_MAJOR(p.apiVersion), VK_API_VERSION_MINOR(p.apiVersion), VK_API_VERSION_PATCH(p.apiVersion), sc ? "yes" : "NO",
               meta ? "yes" : "NO");
    }
    if (!ngpu) return printf("   none.\n"), 1;

    /* the three routes. Wayland first and X11 second, though numbered the other way: the X11 window and its display
       stay open for the monitor's route, which takes the monitor from that X display (--take) */
    void *xdpy = NULL;
    window_route(R_WAYLAND, "wayland", got[2], NULL);
    window_route(R_X11, "x11", got[1], &xdpy);
    display_route(got[4], got[6], xdpy, take);
    if (xdpy) SDL_QuitSubSystem(SDL_INIT_VIDEO);

    printf("\n== HDR by each route ==\n");
    printf("   1. a window on X11:          %s\n", verdict[R_X11]);
    printf("   2. a window on Wayland:      %s\n", verdict[R_WAYLAND]);
    printf("   3. the monitor, no desktop:  %s\n", verdict[R_DISPLAY]);
    printf("(The monitor's own HDR must be switched on, and on a Wayland desktop HDR switched on in its display settings,\n"
           " for a route to offer it.)\n");
    return 0;
}
