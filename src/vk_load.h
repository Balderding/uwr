/* vk_load.h: the Vulkan loader opened at run time (vulkan-1.dll on Windows, libvulkan.so.1 elsewhere) and the functions
 * the renderer uses, as pointers (VK_NO_PROTOTYPES): nothing is linked against Vulkan, so the program starts on a
 * machine without it and the renderer says so (docs\superpowers\plans\2026-11-15-vulkan-renderer.md, T1). */
#pragma once
#include <stdbool.h>
#define VK_NO_PROTOTYPES
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>

#define VKL_GLOBAL(X) X(vkCreateInstance) X(vkEnumerateInstanceExtensionProperties) X(vkEnumerateInstanceLayerProperties) \
    X(vkEnumerateInstanceVersion)
#ifdef _WIN32
#define VKL_INSTANCE_OS(X) X(vkCreateWin32SurfaceKHR)
#else
#define VKL_INSTANCE_OS(X)
#endif
#define VKL_INSTANCE(X) X(vkDestroyInstance) X(vkEnumeratePhysicalDevices) X(vkGetPhysicalDeviceProperties) \
    X(vkGetPhysicalDeviceQueueFamilyProperties) X(vkGetPhysicalDeviceMemoryProperties) X(vkCreateDevice) \
    X(vkGetDeviceProcAddr) X(vkEnumerateDeviceExtensionProperties) X(vkGetPhysicalDeviceFeatures2) X(vkDestroySurfaceKHR) \
    X(vkGetPhysicalDeviceSurfaceSupportKHR) X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) \
    X(vkGetPhysicalDeviceSurfaceFormatsKHR) X(vkGetPhysicalDeviceSurfacePresentModesKHR) VKL_INSTANCE_OS(X)
#define VKL_DEBUG(X) X(vkCreateDebugUtilsMessengerEXT) X(vkDestroyDebugUtilsMessengerEXT)
#define VKL_DEVICE(X) X(vkDestroyDevice) X(vkGetDeviceQueue) X(vkDeviceWaitIdle) X(vkQueueSubmit) X(vkQueueWaitIdle) \
    X(vkCreateCommandPool) X(vkDestroyCommandPool) X(vkAllocateCommandBuffers) X(vkResetCommandPool) \
    X(vkBeginCommandBuffer) X(vkEndCommandBuffer) X(vkCreateFence) X(vkDestroyFence) X(vkWaitForFences) \
    X(vkResetFences) X(vkCreateSemaphore) X(vkDestroySemaphore) X(vkCreateSwapchainKHR) X(vkDestroySwapchainKHR) \
    X(vkGetSwapchainImagesKHR) X(vkAcquireNextImageKHR) X(vkQueuePresentKHR) X(vkCreateImage) X(vkDestroyImage) \
    X(vkCreateImageView) X(vkDestroyImageView) X(vkGetImageMemoryRequirements) X(vkAllocateMemory) X(vkFreeMemory) \
    X(vkBindImageMemory) X(vkCreateBuffer) X(vkDestroyBuffer) X(vkGetBufferMemoryRequirements) X(vkBindBufferMemory) \
    X(vkMapMemory) X(vkUnmapMemory) X(vkCreateShaderModule) X(vkDestroyShaderModule) X(vkCreateDescriptorSetLayout) \
    X(vkDestroyDescriptorSetLayout) X(vkCreatePipelineLayout) X(vkDestroyPipelineLayout) X(vkCreateGraphicsPipelines) \
    X(vkDestroyPipeline) X(vkCreateDescriptorPool) X(vkDestroyDescriptorPool) X(vkAllocateDescriptorSets) \
    X(vkUpdateDescriptorSets) X(vkCmdPipelineBarrier2) X(vkCmdBeginRendering) X(vkCmdEndRendering) X(vkCmdBindPipeline) \
    X(vkCmdBindDescriptorSets) X(vkCmdSetViewport) X(vkCmdSetScissor) X(vkCmdDraw) X(vkCmdCopyBufferToImage) \
    X(vkCmdCopyImageToBuffer) X(vkCmdClearColorImage) X(vkCmdBlitImage) \
    X(vkCmdBindVertexBuffers) X(vkCmdBindIndexBuffer) X(vkCmdDrawIndexed) X(vkCmdCopyBuffer) \
    X(vkCreateComputePipelines) X(vkCmdDispatch) X(vkGetBufferDeviceAddress) X(vkCmdFillBuffer) X(vkCreateSampler) X(vkDestroySampler)
/* ray tracing (M4): taken when the device has VK_KHR_acceleration_structure; NULL otherwise */
#define VKL_RT(X) X(vkGetAccelerationStructureBuildSizesKHR) X(vkCreateAccelerationStructureKHR) \
    X(vkDestroyAccelerationStructureKHR) X(vkCmdBuildAccelerationStructuresKHR) X(vkGetAccelerationStructureDeviceAddressKHR)

#define VKL_DECLARE(f) extern PFN_##f f;
extern PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
VKL_GLOBAL(VKL_DECLARE)
VKL_INSTANCE(VKL_DECLARE)
VKL_DEBUG(VKL_DECLARE)
VKL_DEVICE(VKL_DECLARE)
VKL_RT(VKL_DECLARE)

bool vkl_open(void);                    /* the loader and its global functions: false when there is no Vulkan */
bool vkl_instance(VkInstance inst);     /* the instance's (the debug ones may stay NULL) */
bool vkl_device(VkDevice dev);          /* the device's */
const char *vkl_missing(void);          /* the first function not found, for the log */
