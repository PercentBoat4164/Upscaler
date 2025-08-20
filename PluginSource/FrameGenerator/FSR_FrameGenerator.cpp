#include "ffx_framegeneration.h"
#include "Upscaler/FSR_Upscaler.hpp"
#if defined(ENABLE_FRAME_GENERATION) & defined(ENABLE_FSR)
#include "FSR_FrameGenerator.hpp"

#ifdef ENABLE_VULKAN
#    include "GraphicsAPI/Vulkan.hpp"

#    include <vk_queue_selector.h>

#    include <IUnityGraphicsVulkan.h>
#endif

#include <vector>

ffxContext FSR_FrameGenerator::swapchainContext {nullptr};
ffxContext FSR_FrameGenerator::context {nullptr};
std::array<FfxApiResource, 2> FSR_FrameGenerator::hudlessColorResource {};
FfxApiResource FSR_FrameGenerator::depthResource {};
FfxApiResource FSR_FrameGenerator::motionResource {};

FSR_FrameGenerator::QueueData FSR_FrameGenerator::asyncCompute{}, FSR_FrameGenerator::present{}, FSR_FrameGenerator::imageAcquire{};
bool FSR_FrameGenerator::asyncComputeSupported{false};
FSR_FrameGenerator::CallbackContext FSR_FrameGenerator::callbackContext{&FSR_FrameGenerator::context, false};

void FSR_FrameGenerator::useQueues(std::vector<VqsQueueSelection> selection) {
    if (selection.size() >= 2) {
        std::construct_at(&imageAcquire, selection[0].queueFamilyIndex, selection[0].queueIndex);
        std::construct_at(&present, selection[1].queueFamilyIndex, selection[1].queueIndex);
        if (selection.size() == 3) {
            std::construct_at(&asyncCompute, selection[2].queueFamilyIndex, selection[2].queueIndex);
            asyncComputeSupported = true;
        }
    }
}

void FSR_FrameGenerator::createSwapchain(VkSwapchainKHR* pSwapchain, const VkSwapchainCreateInfoKHR* pCreateInfo, VkAllocationCallbacks* pAllocator, PFN_vkCreateSwapchainFFXAPI* pCreate, PFN_vkDestroySwapchainFFXAPI* pDestroy, PFN_vkGetSwapchainImagesKHR* pGet, PFN_vkAcquireNextImageKHR* pAcquire, PFN_vkQueuePresentKHR* pPresent, PFN_vkSetHdrMetadataEXT* pSet, PFN_getLastPresentCountFFXAPI* pCount) {
    destroySwapchain();
    ffxCreateContextDescFrameGenerationSwapChainVK createContextDescFrameGenerationSwapChainVk{
      .header = {
        .type  = FFX_API_CREATE_CONTEXT_DESC_TYPE_FGSWAPCHAIN_VK,
        .pNext = nullptr
      },
      .physicalDevice    = Vulkan::getGraphicsInterface()->Instance().physicalDevice,
      .device            = Vulkan::getGraphicsInterface()->Instance().device,
      .swapchain         = pSwapchain,
      .allocator         = pAllocator,
      .createInfo        = *pCreateInfo,
      .gameQueue         = {Vulkan::getGraphicsInterface()->Instance().graphicsQueue, Vulkan::getGraphicsInterface()->Instance().queueFamilyIndex, nullptr},
      .asyncComputeQueue = asyncComputeSupported ? VkQueueInfoFFXAPI{Vulkan::getQueue(asyncCompute.family, asyncCompute.index), asyncCompute.family, nullptr} : VkQueueInfoFFXAPI{},
      .presentQueue      = {Vulkan::getQueue(present.family, present.index), present.family, nullptr},
      .imageAcquireQueue = {Vulkan::getQueue(imageAcquire.family, imageAcquire.index), imageAcquire.family, nullptr},
    };
    if (FSR_Upscaler::ffxCreateContext(&swapchainContext, &createContextDescFrameGenerationSwapChainVk.header, nullptr) != FFX_API_RETURN_OK)
        return Plugin::log(kUnityLogTypeError, "Failed to create swapchain context.");

    ffxCreateBackendVKDesc createBackendVkDesc{
      .header = {
        .type  = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_VK,
        .pNext = nullptr
      },
      .vkDevice         = Vulkan::getGraphicsInterface()->Instance().device,
      .vkPhysicalDevice = Vulkan::getGraphicsInterface()->Instance().physicalDevice,
      .vkDeviceProcAddr = Vulkan::getDeviceProcAddr(),
    };
    ffxCreateContextDescFrameGeneration createContextDescFrameGeneration{
      .header = {
        .type  = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION,
        .pNext = &createBackendVkDesc.header
      },
      .flags            = (asyncComputeSupported ? FFX_FRAMEGENERATION_ENABLE_ASYNC_WORKLOAD_SUPPORT : 0U) | static_cast<unsigned>(FFX_FRAMEGENERATION_ENABLE_DEPTH_INVERTED) | static_cast<unsigned>(FFX_FRAMEGENERATION_ENABLE_HIGH_DYNAMIC_RANGE) | static_cast<unsigned>(FFX_FRAMEGENERATION_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS),
      .displaySize      = {pCreateInfo->imageExtent.width, pCreateInfo->imageExtent.height},
      .maxRenderSize    = {pCreateInfo->imageExtent.width, pCreateInfo->imageExtent.height},
      .backBufferFormat = ffxApiGetSurfaceFormatVK(pCreateInfo->imageFormat),
    };
    if (FSR_Upscaler::ffxCreateContext(&context, &createContextDescFrameGeneration.header, nullptr) != FFX_API_RETURN_OK || context == nullptr)
        return Plugin::log(kUnityLogTypeError, "Failed to create frame generation context.");

    swapchain.vulkan = *pSwapchain;

    ffxQueryDescSwapchainReplacementFunctionsVK replacementFunctionsVk{
      .header = {
        .type  = FFX_API_QUERY_DESC_TYPE_FGSWAPCHAIN_FUNCTIONS_VK,
        .pNext = nullptr
      }
    };
    if (FSR_Upscaler::ffxQuery(&swapchainContext, &replacementFunctionsVk.header) != FFX_API_RETURN_OK)
        return Plugin::log(kUnityLogTypeError, "Failed to query swapchain functions.");
    if (pCreate != VK_NULL_HANDLE) *pCreate = replacementFunctionsVk.pOutCreateSwapchainFFXAPI;
    if (pDestroy != VK_NULL_HANDLE) *pDestroy = replacementFunctionsVk.pOutDestroySwapchainFFXAPI;
    if (pGet != VK_NULL_HANDLE) *pGet = replacementFunctionsVk.pOutGetSwapchainImagesKHR;
    if (pAcquire != VK_NULL_HANDLE) *pAcquire = replacementFunctionsVk.pOutAcquireNextImageKHR;
    if (pPresent != VK_NULL_HANDLE) *pPresent = replacementFunctionsVk.pOutQueuePresentKHR;
    if (pSet != VK_NULL_HANDLE) *pSet = replacementFunctionsVk.pOutSetHdrMetadataEXT;
    if (pCount != VK_NULL_HANDLE) *pCount = replacementFunctionsVk.pOutGetLastPresentCountFFXAPI;
}

void FSR_FrameGenerator::destroySwapchain() {
    if (context != nullptr) {
        const ffxConfigureDescFrameGeneration configureDescFrameGeneration{
          .header = {
            .type  = FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION,
            .pNext = nullptr
          },
          .swapChain                          = swapchain.vulkan,
          .presentCallback                    = nullptr,
          .presentCallbackUserContext         = nullptr,
          .frameGenerationCallback            = nullptr,
          .frameGenerationCallbackUserContext = nullptr,
          .frameGenerationEnabled             = false,
          .allowAsyncWorkloads                = false,
          .HUDLessColor                       = {},
          .flags                              = 0,
          .onlyPresentGenerated               = false,
          .generationRect                     = {0, 0, 0, 0},
          .frameID                            = 0,
        };
        if (FSR_Upscaler::ffxConfigure(&context, &configureDescFrameGeneration.header) != FFX_API_RETURN_OK)
            Plugin::log(kUnityLogTypeError, "Failed to configure frame generation.");
    }
    if (swapchainContext != nullptr) FSR_Upscaler::ffxDestroyContext(&swapchainContext, nullptr);
    swapchainContext = nullptr;
    if (context != nullptr) FSR_Upscaler::ffxDestroyContext(&context, nullptr);
    context          = nullptr;
    swapchain.vulkan = VK_NULL_HANDLE;
}

void FSR_FrameGenerator::useImages(VkImage color0, VkImage color1, VkImage depth, VkImage motion) {
    UnityVulkanImage image{};
    Vulkan::getGraphicsInterface()->AccessTexture(color0, UnityVulkanWholeImage, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, kUnityVulkanResourceAccess_PipelineBarrier, &image);
    hudlessColorResource.at(0).resource    = image.image;
    hudlessColorResource.at(0).description = {
      .type     = FFX_API_RESOURCE_TYPE_TEXTURE2D,
      .format   = ffxApiGetSurfaceFormatVK(image.format),
      .width    = image.extent.width,
      .height   = image.extent.height,
      .depth    = image.extent.depth,
      .mipCount = 1U,
      .flags    = FFX_API_RESOURCE_FLAGS_ALIASABLE,
      .usage    = static_cast<uint32_t>(FFX_API_RESOURCE_USAGE_READ_ONLY),
    };
    hudlessColorResource.at(0).state = static_cast<uint32_t>(FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    Vulkan::getGraphicsInterface()->AccessTexture(color1, UnityVulkanWholeImage, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, kUnityVulkanResourceAccess_PipelineBarrier, &image);
    hudlessColorResource.at(1).resource    = image.image;
    hudlessColorResource.at(1).description = {
      .type     = FFX_API_RESOURCE_TYPE_TEXTURE2D,
      .format   = ffxApiGetSurfaceFormatVK(image.format),
      .width    = image.extent.width,
      .height   = image.extent.height,
      .depth    = image.extent.depth,
      .mipCount = 1U,
      .flags    = FFX_API_RESOURCE_FLAGS_ALIASABLE,
      .usage    = static_cast<uint32_t>(FFX_API_RESOURCE_USAGE_READ_ONLY),
    };
    hudlessColorResource.at(1).state = static_cast<uint32_t>(FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    image                            = {};
    Vulkan::getGraphicsInterface()->AccessTexture(depth, UnityVulkanWholeImage, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, kUnityVulkanResourceAccess_PipelineBarrier, &image);
    depthResource.resource    = image.image;
    depthResource.description = {
      .type     = FFX_API_RESOURCE_TYPE_TEXTURE2D,
      .format   = ffxApiGetSurfaceFormatVK(image.format),
      .width    = image.extent.width,
      .height   = image.extent.height,
      .depth    = image.extent.depth,
      .mipCount = 1U,
      .flags    = FFX_API_RESOURCE_FLAGS_ALIASABLE,
      .usage    = static_cast<uint32_t>(FFX_API_RESOURCE_USAGE_READ_ONLY),
    };
    depthResource.state = static_cast<uint32_t>(FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    image               = {};
    Vulkan::getGraphicsInterface()->AccessTexture(motion, UnityVulkanWholeImage, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, kUnityVulkanResourceAccess_PipelineBarrier, &image);
    motionResource.resource    = image.image;
    motionResource.description = {
      .type     = FFX_API_RESOURCE_TYPE_TEXTURE2D,
      .format   = ffxApiGetSurfaceFormatVK(image.format),
      .width    = image.extent.width,
      .height   = image.extent.height,
      .depth    = image.extent.depth,
      .mipCount = 1U,
      .flags    = FFX_API_RESOURCE_FLAGS_ALIASABLE,
      .usage    = static_cast<uint32_t>(FFX_API_RESOURCE_USAGE_READ_ONLY),
    };
    motionResource.state = static_cast<uint32_t>(FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
}

void FSR_FrameGenerator::evaluate(bool enable, FfxApiRect2D generationRect, const float cameraPosition[3], const float cameraUp[3], const float cameraRight[3], const float cameraForward[3], FfxApiFloatCoords2D renderSize, FfxApiFloatCoords2D jitter, float frameTime, float farPlane, float nearPlane, float verticalFOV, unsigned index, unsigned options) {
    if (context == nullptr) return;
    callbackContext.reset = (options & 0x40U) != 0U;

    ffxConfigureDescFrameGeneration configureDescFrameGeneration{
      .header = {
        .type  = FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION,
        .pNext = nullptr
      },
      .swapChain                  = swapchain.vulkan,
      .presentCallback            = nullptr,
      .presentCallbackUserContext = nullptr,
      .frameGenerationCallback    = [](ffxDispatchDescFrameGeneration* params, void*) -> ffxReturnCode_t {
          static uint32_t frameNumber;
          params->reset |= callbackContext.reset;
          params->frameID = ++frameNumber;
          return FSR_Upscaler::ffxDispatch(callbackContext.context, &params->header);
      },
      .frameGenerationCallbackUserContext = nullptr,
      .frameGenerationEnabled             = enable,
      .allowAsyncWorkloads                = (options & 0x20U) != 0U && asyncComputeSupported,
      .HUDLessColor                       = hudlessColorResource.at(index),
      .flags                              = ((options & 0x1U) != 0U ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_VIEW : 0U) |
                                            ((options & 0x2U) != 0U ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_TEAR_LINES : 0U) |
                                            ((options & 0x4U) != 0U ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_RESET_INDICATORS : 0U) |
                                            ((options & 0x8U) != 0U ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_PACING_LINES : 0U),
      .onlyPresentGenerated               = (options & 0x10U) != 0U,
      .generationRect                     = generationRect,
      .frameID                            = 0,  // This can be set to zero because it is later assigned above.
    };
    if (FSR_Upscaler::ffxConfigure(&context, &configureDescFrameGeneration.header) != FFX_API_RETURN_OK)
        Plugin::log(kUnityLogTypeError, "Failed to configure frame generation.");

    if (configureDescFrameGeneration.frameGenerationEnabled) {
        UnityVulkanRecordingState state{};
        Vulkan::getGraphicsInterface()->CommandRecordingState(&state, kUnityVulkanGraphicsQueueAccess_DontCare);

        ffxDispatchDescFrameGenerationPrepareCameraInfo dispatchDescFrameGenerationPrepareCameraInfo{
          .header = {
            .type  = FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE_CAMERAINFO,
            .pNext = nullptr
          },
          .cameraPosition = {cameraPosition[0], cameraPosition[1], cameraPosition[2]},
          .cameraUp       = {cameraUp[0], cameraUp[1], cameraUp[2]},
          .cameraRight    = {cameraRight[0], cameraRight[1], cameraRight[2]},
          .cameraForward  = {cameraForward[0], cameraForward[1], cameraForward[2]}
        };
        ffxDispatchDescFrameGenerationPrepare dispatchDescFrameGenerationPrepare{
          .header = {
            .type  = FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE,
            .pNext = &dispatchDescFrameGenerationPrepareCameraInfo.header
          },
          .frameID                 = configureDescFrameGeneration.frameID,
          .flags                   = configureDescFrameGeneration.flags,
          .commandList             = state.commandBuffer,
          .renderSize              = {static_cast<uint32_t>(renderSize.x), static_cast<uint32_t>(renderSize.y)},
          .jitterOffset            = jitter,
          .motionVectorScale       = {-static_cast<float>(generationRect.width), static_cast<float>(generationRect.height)},
          .frameTimeDelta          = frameTime,
          .unused_reset            = false,
          .cameraNear              = farPlane,
          .cameraFar               = nearPlane, // Switched because depth is inverted
          .cameraFovAngleVertical  = verticalFOV,
          .viewSpaceToMetersFactor = 1.0F,
          .depth                   = depthResource,
          .motionVectors           = motionResource,
        };
        if (FSR_Upscaler::ffxDispatch(&context, &dispatchDescFrameGenerationPrepare.header) != FFX_API_RETURN_OK)
            Plugin::log(kUnityLogTypeError, "Failed to dispatch frame generation prepare command.");
    }
}

ffxContext* FSR_FrameGenerator::getContext() {
    return &swapchainContext;
}
#endif