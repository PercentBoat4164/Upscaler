#pragma once
#if defined(ENABLE_FRAME_GENERATION) && defined(ENABLE_FSR)
#include "FrameGenerator.hpp"

#ifdef ENABLE_VULKAN
#    include <vk/ffx_api_vk.h>

#    include <vulkan/vulkan.h>
#endif

#include <ffx_api.h>

#include <array>

#ifdef ENABLE_VULKAN
struct VqsQueueSelection;
#endif

class FSR_FrameGenerator final : protected FrameGenerator {
    static ffxContext swapchainContext;
    static ffxContext context;
    static std::array<FfxApiResource, 2> hudlessColorResource;
    static FfxApiResource depthResource;
    static FfxApiResource motionResource;

    static struct alignas(8) QueueData {
        uint32_t family{}, index{};
    } asyncCompute, present, imageAcquire;
    static bool asyncComputeSupported;
    static struct alignas(16) CallbackContext {
        ffxContext* context;
        bool reset;
    } callbackContext;

public:
#ifdef ENABLE_VULKAN
    static void useQueues(std::vector<VqsQueueSelection> selection);
#endif

    static void createSwapchain(VkSwapchainKHR* pSwapchain, const VkSwapchainCreateInfoKHR* pCreateInfo, VkAllocationCallbacks* pAllocator, PFN_vkCreateSwapchainFFXAPI* pCreate, PFN_vkDestroySwapchainFFXAPI* pDestroy, PFN_vkGetSwapchainImagesKHR* pGet, PFN_vkAcquireNextImageKHR* pAcquire, PFN_vkQueuePresentKHR* pPresent, PFN_vkSetHdrMetadataEXT* pSet, PFN_getLastPresentCountFFXAPI* pCount);

    static void destroySwapchain();

    static void useImages(VkImage color0, VkImage color1, VkImage depth, VkImage motion);

    static void evaluate(bool enable, FfxApiRect2D generationRect, const float cameraPosition[], const float cameraUp[], const float cameraRight[], const float cameraForward[], FfxApiFloatCoords2D renderSize, FfxApiFloatCoords2D jitter, float frameTime, float farPlane, float nearPlane, float verticalFOV, unsigned index, unsigned options);

    static ffxContext* getContext();

    static UnityRenderingExtTextureFormat vkFormatToUnityFormat(VkFormat format);
    static VkFormat unityFormatToVkFormat(UnityRenderingExtTextureFormat format);
};
#endif