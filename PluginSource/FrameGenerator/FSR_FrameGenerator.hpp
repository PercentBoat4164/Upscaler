#pragma once
#if defined(ENABLE_FRAME_GENERATION) && defined(ENABLE_FSR)
#include "FrameGenerator.hpp"

#include <GraphicsAPI/Vulkan.hpp>

#include <FidelityFX/host/backends/vk/ffx_api_vk.hpp>
#include <FidelityFX/host/ffx_api.hpp>

#include <vulkan/vulkan.h>

#include <IUnityGraphicsVulkan.h>

#include <ranges>
#include <vector>

class FSR_FrameGenerator final : protected FrameGenerator {
    static ffx::Context swapchainContext;

public:
    static void CreateSwapchain(VkSwapchainKHR* pSwapchain, const VkSwapchainCreateInfoKHR* pCreateInfo, VkAllocationCallbacks* pAllocator, PFN_vkCreateSwapchainFFXAPI* pCreate, PFN_vkDestroySwapchainFFXAPI* pDestroy, PFN_vkGetSwapchainImagesKHR* pGet, PFN_vkAcquireNextImageKHR* pAcquire, PFN_vkQueuePresentKHR* pPresent, PFN_vkSetHdrMetadataEXT* pSet, PFN_getLastPresentCountFFXAPI* pCount) {
        if (swapchainContext != nullptr)
            ffx::DestroyContext(swapchainContext, nullptr);
        swapchainContext = nullptr;
        const auto queues = Vulkan::getQueues({VK_QUEUE_COMPUTE_BIT, VK_QUEUE_TRANSFER_BIT, 0});
        ffx::CreateContextDescFrameGenerationSwapChainVK createContextDescFrameGenerationSwapChainVk{};
        createContextDescFrameGenerationSwapChainVk.physicalDevice = Vulkan::getGraphicsInterface()->Instance().physicalDevice;
        createContextDescFrameGenerationSwapChainVk.device         = Vulkan::getGraphicsInterface()->Instance().device;
        createContextDescFrameGenerationSwapChainVk.swapchain      = pSwapchain;
        createContextDescFrameGenerationSwapChainVk.allocator      = pAllocator;
        createContextDescFrameGenerationSwapChainVk.createInfo     = *pCreateInfo;
        createContextDescFrameGenerationSwapChainVk.gameQueue      = VkQueueInfoFFXAPI{
          Vulkan::getGraphicsInterface()->Instance().graphicsQueue,
          Vulkan::getGraphicsInterface()->Instance().queueFamilyIndex,
          nullptr
        };
        createContextDescFrameGenerationSwapChainVk.asyncComputeQueue = VkQueueInfoFFXAPI{
          queues[0].first,
          queues[0].second,
          nullptr
        };
        createContextDescFrameGenerationSwapChainVk.presentQueue = VkQueueInfoFFXAPI{
          queues[1].first,
          queues[1].second,
          nullptr
        };
        createContextDescFrameGenerationSwapChainVk.imageAcquireQueue = VkQueueInfoFFXAPI{
          queues[2].first,
          queues[2].second,
          nullptr
        };
        if (CreateContext(swapchainContext, nullptr, createContextDescFrameGenerationSwapChainVk) != ffx::ReturnCode::Ok) return;
        swapchain.vulkan = *pSwapchain;

        ffx::QueryDescSwapchainReplacementFunctionsVK replacementFunctionsVk{};
        Query(swapchainContext, replacementFunctionsVk);
        if (pCreate != VK_NULL_HANDLE) *pCreate = replacementFunctionsVk.pOutCreateSwapchainFFXAPI;
        if (pDestroy != VK_NULL_HANDLE) *pDestroy = replacementFunctionsVk.pOutDestroySwapchainFFXAPI;
        if (pGet != VK_NULL_HANDLE) *pGet = replacementFunctionsVk.pOutGetSwapchainImagesKHR;
        if (pAcquire != VK_NULL_HANDLE) *pAcquire = replacementFunctionsVk.pOutAcquireNextImageKHR;
        if (pPresent != VK_NULL_HANDLE) *pPresent = replacementFunctionsVk.pOutQueuePresentKHR;
        if (pSet != VK_NULL_HANDLE) *pSet = replacementFunctionsVk.pOutSetHdrMetadataEXT;
        if (pCount != VK_NULL_HANDLE) *pCount = replacementFunctionsVk.pOutGetLastPresentCountFFXAPI;
    }

    static void DestroySwapchain() {
        ffx::DestroyContext(swapchainContext, nullptr);
        swapchainContext = nullptr;
        swapchain.vulkan = VK_NULL_HANDLE;
    }

    static bool ownsSwapchain(VkSwapchainKHR swapchain) {
      return FSR_FrameGenerator::swapchain.vulkan != VK_NULL_HANDLE && swapchain == FSR_FrameGenerator::swapchain.vulkan;
    }

    static ffx::Context* getContext() { return &swapchainContext; }
};

#endif