/* ngxvk_test.c: src\ngx_vk.c against the driver, with no window: a Vulkan instance and device made with the
 * extensions NGX asks for, NGX started, DLSS Super Resolution and Ray Reconstruction created (on a command buffer,
 * submitted) and released. Prints each step; exit code 0 when DLSS is available and both were created.
 *   clang %CLANG_FLAGS% -std=c17 -I src -I dlss\include -I <vulkan sdk>\Include -o work\ngxvk\ngxvk_test.exe
 *         tests\ngxvk_test.c src\ngx_vk.c src\vk_load.c -ladvapi32
 *   work\ngxvk\ngxvk_test.exe DLSS_DIR LOG_DIR      (DLSS_DIR: the folder with nvngx_dlss.dll and nvngx_dlssd.dll) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ngx_vk.h"

static bool has(const VkExtensionProperties *e, uint32_t n, const char *name)
{
    for (uint32_t i = 0; i < n; i++)
        if (!strcmp(e[i].extensionName, name)) return true;
    return false;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 3) return printf("usage: ngxvk_test DLSS_DIR LOG_DIR\n"), 2;
    wchar_t dlss[512], logd[512];
    mbstowcs(dlss, argv[1], 511), mbstowcs(logd, argv[2], 511);
    if (!vkl_open()) return printf("no Vulkan (%s)\n", vkl_missing()), 2;
    if (!ngxvk_load()) return printf("no NGX\n"), 2;
    const char *const *ie, *const *de;
    unsigned ni, nd;
    ngxvk_extensions(&ie, &ni, &de, &nd);
    printf("NGX asks for %u instance and %u device extensions\n", ni, nd);
    for (unsigned i = 0; i < ni; i++) printf("  instance: %s\n", ie[i]);
    for (unsigned i = 0; i < nd; i++) printf("  device: %s\n", de[i]);

    uint32_t na = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &na, NULL);
    VkExtensionProperties *av = calloc(na + 1, sizeof *av);
    vkEnumerateInstanceExtensionProperties(NULL, &na, av);
    const char *iext[32];
    uint32_t nie = 0;
    for (unsigned i = 0; i < ni && nie < 32; i++)
        if (has(av, na, ie[i])) iext[nie++] = ie[i];
        else printf("  (instance extension %s is not here)\n", ie[i]);
    /* (vk_load.c's tables hold the renderer's surface and swap chain functions too: their extensions on, unused) */
    static const char *const more_i[] = {"VK_KHR_surface", "VK_KHR_win32_surface"};
    for (unsigned i = 0; i < 2 && nie < 32; i++)
        if (has(av, na, more_i[i])) iext[nie++] = more_i[i];
    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName = "ngxvk_test", .apiVersion = VK_API_VERSION_1_3};
    VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app, .enabledExtensionCount = nie,
                                .ppEnabledExtensionNames = iext};
    VkInstance inst;
    VkResult vr = vkCreateInstance(&ici, NULL, &inst);
    if (vr != VK_SUCCESS) return printf("instance failed %d\n", (int)vr), 2;
    if (!vkl_instance(inst)) return printf("instance without %s\n", vkl_missing()), 2;

    uint32_t np = 0;
    vkEnumeratePhysicalDevices(inst, &np, NULL);
    VkPhysicalDevice pd[16];
    if (np > 16) np = 16;
    vkEnumeratePhysicalDevices(inst, &np, pd);
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    for (uint32_t i = 0; i < np; i++) {
        VkPhysicalDeviceProperties p;
        vkGetPhysicalDeviceProperties(pd[i], &p);
        if (p.vendorID == 0x10DE && !phys) phys = pd[i], printf("GPU: %s\n", p.deviceName);
    }
    if (!phys) return printf("no NVIDIA GPU\n"), 2;
    uint32_t nq = 0, qf = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &nq, NULL);
    VkQueueFamilyProperties qp[32];
    if (nq > 32) nq = 32;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &nq, qp);
    for (uint32_t q = 0; q < nq; q++)
        if (qp[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) { qf = q; break; }
    uint32_t nda = 0;
    vkEnumerateDeviceExtensionProperties(phys, NULL, &nda, NULL);
    VkExtensionProperties *dav = calloc(nda + 1, sizeof *dav);
    vkEnumerateDeviceExtensionProperties(phys, NULL, &nda, dav);
    const char *dext[32];
    uint32_t nde = 0;
    for (unsigned i = 0; i < nd && nde < 32; i++)
        if (has(dav, nda, de[i])) dext[nde++] = de[i];
        else printf("  (device extension %s is not here)\n", de[i]);
    if (has(dav, nda, "VK_KHR_swapchain") && nde < 32) dext[nde++] = "VK_KHR_swapchain";
    float prio = 1;
    VkDeviceQueueCreateInfo qi = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = qf, .queueCount = 1, .pQueuePriorities = &prio};
    VkPhysicalDeviceVulkan12Features f12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .bufferDeviceAddress = VK_TRUE};
    VkPhysicalDeviceVulkan13Features f13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .pNext = &f12, .synchronization2 = VK_TRUE,
                                            .dynamicRendering = VK_TRUE};
    VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .pNext = &f13, .queueCreateInfoCount = 1, .pQueueCreateInfos = &qi,
                              .enabledExtensionCount = nde, .ppEnabledExtensionNames = dext};
    VkDevice dev;
    vr = vkCreateDevice(phys, &dci, NULL, &dev);
    if (vr != VK_SUCCESS) return printf("device failed %d\n", (int)vr), 2;
    if (!vkl_device(dev)) return printf("device without %s\n", vkl_missing()), 2;
    VkQueue queue;
    vkGetDeviceQueue(dev, qf, 0, &queue);
    VkCommandPoolCreateInfo pi = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .queueFamilyIndex = qf};
    VkCommandPool pool;
    vkCreateCommandPool(dev, &pi, NULL, &pool);
    VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = pool,
                                      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1};
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(dev, &ai, &cmd);

    const wchar_t *search[1] = {dlss};
    if (!ngxvk_start(inst, phys, dev, vkGetInstanceProcAddr, vkGetDeviceProcAddr, logd, search, 1)) return printf("NGX did not start\n"), 1;
    printf("DLSS available: %d\n", (int)ngxvk_dlss_available());
    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    vkBeginCommandBuffer(cmd, &bi);
    bool sr = ngxvk_dlss_create(dev, cmd, 1280, 720, 1920, 1080, NGXVK_QUALITY);
    bool rr = ngxvk_rr_create(dev, cmd, 1280, 720, 1920, 1080, NGXVK_QUALITY);
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd};
    vr = vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
    vkDeviceWaitIdle(dev);
    printf("submit %d; DLSS created: %d; Ray Reconstruction created: %d\n", (int)vr, (int)sr, (int)rr);
    ngxvk_stop(dev);
    vkDestroyCommandPool(dev, pool, NULL);
    vkDestroyDevice(dev, NULL);
    vkDestroyInstance(inst, NULL);
    printf("ngxvk_test: %s\n", sr && rr ? "PASS" : "FAIL");
    return sr && rr ? 0 : 1;
}
