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
    backBufferFormat = vkFormatToUnityFormat(pCreateInfo->imageFormat);

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
    backBufferFormat = kUnityRenderingExtFormatNone;
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
    static uint32_t frameNumber;

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
          if (callbackContext.reset) params->reset = callbackContext.reset;
          return FSR_Upscaler::ffxDispatch(callbackContext.context, &params->header);
                 },
      .frameGenerationCallbackUserContext = nullptr,
      .frameGenerationEnabled             = enable,
      .allowAsyncWorkloads                = (options & 0x20U) != 0U && asyncComputeSupported,
      .HUDLessColor                       = hudlessColorResource.at(index),
      .flags                              = ((options & 0x1U) != 0U ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_VIEW : 0U) | ((options & 0x2U) != 0U ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_TEAR_LINES : 0U) | ((options & 0x4U) != 0U ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_RESET_INDICATORS : 0U) | ((options & 0x8U) != 0U ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_PACING_LINES : 0U),
      .onlyPresentGenerated               = (options & 0x10U) != 0U,
      .generationRect                     = generationRect,
      .frameID                            = frameNumber,
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
    ++frameNumber;
}

ffxContext* FSR_FrameGenerator::getContext() {
    return &swapchainContext;
}

UnityRenderingExtTextureFormat FSR_FrameGenerator::vkFormatToUnityFormat(const VkFormat format) {
    switch (format) {
        case VK_FORMAT_R8_UNORM: return kUnityRenderingExtFormatR8_UNorm;
        case VK_FORMAT_R8_SNORM: return kUnityRenderingExtFormatR8_SNorm;
        case VK_FORMAT_R8_UINT: return kUnityRenderingExtFormatR8_UInt;
        case VK_FORMAT_R8_SINT: return kUnityRenderingExtFormatR8_SInt;
        case VK_FORMAT_R8_SRGB: return kUnityRenderingExtFormatR8_SRGB;
        case VK_FORMAT_R8G8_UNORM: return kUnityRenderingExtFormatR8G8_UNorm;
        case VK_FORMAT_R8G8_SNORM: return kUnityRenderingExtFormatR8G8_SNorm;
        case VK_FORMAT_R8G8_UINT: return kUnityRenderingExtFormatR8G8_UInt;
        case VK_FORMAT_R8G8_SINT: return kUnityRenderingExtFormatR8G8_SInt;
        case VK_FORMAT_R8G8_SRGB: return kUnityRenderingExtFormatR8G8_SRGB;
        case VK_FORMAT_R8G8B8_UNORM: return kUnityRenderingExtFormatR8G8B8_UNorm;
        case VK_FORMAT_R8G8B8_SNORM: return kUnityRenderingExtFormatR8G8B8_SNorm;
        case VK_FORMAT_R8G8B8_UINT: return kUnityRenderingExtFormatR8G8B8_UInt;
        case VK_FORMAT_R8G8B8_SINT: return kUnityRenderingExtFormatR8G8B8_SInt;
        case VK_FORMAT_R8G8B8_SRGB: return kUnityRenderingExtFormatR8G8B8_SRGB;
        case VK_FORMAT_B8G8R8_UNORM: return kUnityRenderingExtFormatB8G8R8_UNorm;
        case VK_FORMAT_B8G8R8_SNORM: return kUnityRenderingExtFormatB8G8R8_SNorm;
        case VK_FORMAT_B8G8R8_UINT: return kUnityRenderingExtFormatB8G8R8_UInt;
        case VK_FORMAT_B8G8R8_SINT: return kUnityRenderingExtFormatB8G8R8_SInt;
        case VK_FORMAT_B8G8R8_SRGB: return kUnityRenderingExtFormatB8G8R8_SRGB;
        case VK_FORMAT_R8G8B8A8_UNORM: return kUnityRenderingExtFormatR8G8B8A8_UNorm;
        case VK_FORMAT_R8G8B8A8_SNORM: return kUnityRenderingExtFormatR8G8B8A8_SNorm;
        case VK_FORMAT_R8G8B8A8_UINT: return kUnityRenderingExtFormatR8G8B8A8_UInt;
        case VK_FORMAT_R8G8B8A8_SINT: return kUnityRenderingExtFormatR8G8B8A8_SInt;
        case VK_FORMAT_R8G8B8A8_SRGB: return kUnityRenderingExtFormatR8G8B8A8_SRGB;
        case VK_FORMAT_B8G8R8A8_UNORM: return kUnityRenderingExtFormatB8G8R8A8_UNorm;
        case VK_FORMAT_B8G8R8A8_SNORM: return kUnityRenderingExtFormatB8G8R8A8_SNorm;
        case VK_FORMAT_B8G8R8A8_UINT: return kUnityRenderingExtFormatB8G8R8A8_UInt;
        case VK_FORMAT_B8G8R8A8_SINT: return kUnityRenderingExtFormatB8G8R8A8_SInt;
        case VK_FORMAT_B8G8R8A8_SRGB: return kUnityRenderingExtFormatB8G8R8A8_SRGB;
        case VK_FORMAT_R16_UNORM: return kUnityRenderingExtFormatR16_UNorm;
        case VK_FORMAT_R16_SNORM: return kUnityRenderingExtFormatR16_SNorm;
        case VK_FORMAT_R16_UINT: return kUnityRenderingExtFormatR16_UInt;
        case VK_FORMAT_R16_SINT: return kUnityRenderingExtFormatR16_SInt;
        case VK_FORMAT_R16_SFLOAT: return kUnityRenderingExtFormatR16_SFloat;
        case VK_FORMAT_R16G16_UNORM: return kUnityRenderingExtFormatR16G16_UNorm;
        case VK_FORMAT_R16G16_SNORM: return kUnityRenderingExtFormatR16G16_SNorm;
        case VK_FORMAT_R16G16_UINT: return kUnityRenderingExtFormatR16G16_UInt;
        case VK_FORMAT_R16G16_SINT: return kUnityRenderingExtFormatR16G16_SInt;
        case VK_FORMAT_R16G16_SFLOAT: return kUnityRenderingExtFormatR16G16_SFloat;
        case VK_FORMAT_R16G16B16_UNORM: return kUnityRenderingExtFormatR16G16B16_UNorm;
        case VK_FORMAT_R16G16B16_SNORM: return kUnityRenderingExtFormatR16G16B16_SNorm;
        case VK_FORMAT_R16G16B16_UINT: return kUnityRenderingExtFormatR16G16B16_UInt;
        case VK_FORMAT_R16G16B16_SINT: return kUnityRenderingExtFormatR16G16B16_SInt;
        case VK_FORMAT_R16G16B16_SFLOAT: return kUnityRenderingExtFormatR16G16B16_SFloat;
        case VK_FORMAT_R16G16B16A16_UNORM: return kUnityRenderingExtFormatR16G16B16A16_UNorm;
        case VK_FORMAT_R16G16B16A16_SNORM: return kUnityRenderingExtFormatR16G16B16A16_SNorm;
        case VK_FORMAT_R16G16B16A16_UINT: return kUnityRenderingExtFormatR16G16B16A16_UInt;
        case VK_FORMAT_R16G16B16A16_SINT: return kUnityRenderingExtFormatR16G16B16A16_SInt;
        case VK_FORMAT_R16G16B16A16_SFLOAT: return kUnityRenderingExtFormatR16G16B16A16_SFloat;
        case VK_FORMAT_R32_UINT: return kUnityRenderingExtFormatR32_UInt;
        case VK_FORMAT_R32_SINT: return kUnityRenderingExtFormatR32_SInt;
        case VK_FORMAT_R32_SFLOAT: return kUnityRenderingExtFormatR32_SFloat;
        case VK_FORMAT_R32G32_UINT: return kUnityRenderingExtFormatR32G32_UInt;
        case VK_FORMAT_R32G32_SINT: return kUnityRenderingExtFormatR32G32_SInt;
        case VK_FORMAT_R32G32_SFLOAT: return kUnityRenderingExtFormatR32G32_SFloat;
        case VK_FORMAT_R32G32B32_UINT: return kUnityRenderingExtFormatR32G32B32_UInt;
        case VK_FORMAT_R32G32B32_SINT: return kUnityRenderingExtFormatR32G32B32_SInt;
        case VK_FORMAT_R32G32B32_SFLOAT: return kUnityRenderingExtFormatR32G32B32_SFloat;
        case VK_FORMAT_R32G32B32A32_UINT: return kUnityRenderingExtFormatR32G32B32A32_UInt;
        case VK_FORMAT_R32G32B32A32_SINT: return kUnityRenderingExtFormatR32G32B32A32_SInt;
        case VK_FORMAT_R32G32B32A32_SFLOAT: return kUnityRenderingExtFormatR32G32B32A32_SFloat;
        case VK_FORMAT_R4G4B4A4_UNORM_PACK16: return kUnityRenderingExtFormatR4G4B4A4_UNormPack16;
        case VK_FORMAT_B4G4R4A4_UNORM_PACK16: return kUnityRenderingExtFormatB4G4R4A4_UNormPack16;
        case VK_FORMAT_R5G6B5_UNORM_PACK16: return kUnityRenderingExtFormatR5G6B5_UNormPack16;
        case VK_FORMAT_B5G6R5_UNORM_PACK16: return kUnityRenderingExtFormatB5G6R5_UNormPack16;
        case VK_FORMAT_R5G5B5A1_UNORM_PACK16: return kUnityRenderingExtFormatR5G5B5A1_UNormPack16;
        case VK_FORMAT_B5G5R5A1_UNORM_PACK16: return kUnityRenderingExtFormatB5G5R5A1_UNormPack16;
        case VK_FORMAT_A1R5G5B5_UNORM_PACK16: return kUnityRenderingExtFormatA1R5G5B5_UNormPack16;
        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: return kUnityRenderingExtFormatE5B9G9R9_UFloatPack32;
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return kUnityRenderingExtFormatB10G11R11_UFloatPack32;
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return kUnityRenderingExtFormatA2R10G10B10_UNormPack32;
        case VK_FORMAT_A2R10G10B10_UINT_PACK32: return kUnityRenderingExtFormatA2R10G10B10_UIntPack32;
        case VK_FORMAT_A2R10G10B10_SINT_PACK32: return kUnityRenderingExtFormatA2R10G10B10_SIntPack32;
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return kUnityRenderingExtFormatA2B10G10R10_UNormPack32;
        case VK_FORMAT_A2B10G10R10_UINT_PACK32: return kUnityRenderingExtFormatA2B10G10R10_UIntPack32;
        case VK_FORMAT_A2B10G10R10_SINT_PACK32: return kUnityRenderingExtFormatA2B10G10R10_SIntPack32;
        default: return kUnityRenderingExtFormatNone;
    }
}

VkFormat FSR_FrameGenerator::unityFormatToVkFormat(const UnityRenderingExtTextureFormat format) {
    switch (format) {
        case kUnityRenderingExtFormatR8_UNorm: return VK_FORMAT_R8_UNORM;
        case kUnityRenderingExtFormatR8_SNorm: return VK_FORMAT_R8_SNORM;
        case kUnityRenderingExtFormatR8_UInt: return VK_FORMAT_R8_UINT;
        case kUnityRenderingExtFormatR8_SInt: return VK_FORMAT_R8_SINT;
        case kUnityRenderingExtFormatR8_SRGB: return VK_FORMAT_R8_SRGB;
        case kUnityRenderingExtFormatR8G8_UNorm: return VK_FORMAT_R8G8_UNORM;
        case kUnityRenderingExtFormatR8G8_SNorm: return VK_FORMAT_R8G8_SNORM;
        case kUnityRenderingExtFormatR8G8_UInt: return VK_FORMAT_R8G8_UINT;
        case kUnityRenderingExtFormatR8G8_SInt: return VK_FORMAT_R8G8_SINT;
        case kUnityRenderingExtFormatR8G8_SRGB: return VK_FORMAT_R8G8_SRGB;
        case kUnityRenderingExtFormatR8G8B8_UNorm: return VK_FORMAT_R8G8B8_UNORM;
        case kUnityRenderingExtFormatR8G8B8_SNorm: return VK_FORMAT_R8G8B8_SNORM;
        case kUnityRenderingExtFormatR8G8B8_UInt: return VK_FORMAT_R8G8B8_UINT;
        case kUnityRenderingExtFormatR8G8B8_SInt: return VK_FORMAT_R8G8B8_SINT;
        case kUnityRenderingExtFormatR8G8B8_SRGB: return VK_FORMAT_R8G8B8_SRGB;
        case kUnityRenderingExtFormatB8G8R8_UNorm: return VK_FORMAT_B8G8R8_UNORM;
        case kUnityRenderingExtFormatB8G8R8_SNorm: return VK_FORMAT_B8G8R8_SNORM;
        case kUnityRenderingExtFormatB8G8R8_UInt: return VK_FORMAT_B8G8R8_UINT;
        case kUnityRenderingExtFormatB8G8R8_SInt: return VK_FORMAT_B8G8R8_SINT;
        case kUnityRenderingExtFormatB8G8R8_SRGB: return VK_FORMAT_B8G8R8_SRGB;
        case kUnityRenderingExtFormatR8G8B8A8_UNorm: return VK_FORMAT_R8G8B8A8_UNORM;
        case kUnityRenderingExtFormatR8G8B8A8_SNorm: return VK_FORMAT_R8G8B8A8_SNORM;
        case kUnityRenderingExtFormatR8G8B8A8_UInt: return VK_FORMAT_R8G8B8A8_UINT;
        case kUnityRenderingExtFormatR8G8B8A8_SInt: return VK_FORMAT_R8G8B8A8_SINT;
        case kUnityRenderingExtFormatR8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
        case kUnityRenderingExtFormatB8G8R8A8_UNorm: return VK_FORMAT_B8G8R8A8_UNORM;
        case kUnityRenderingExtFormatB8G8R8A8_SNorm: return VK_FORMAT_B8G8R8A8_SNORM;
        case kUnityRenderingExtFormatB8G8R8A8_UInt: return VK_FORMAT_B8G8R8A8_UINT;
        case kUnityRenderingExtFormatB8G8R8A8_SInt: return VK_FORMAT_B8G8R8A8_SINT;
        case kUnityRenderingExtFormatB8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
        case kUnityRenderingExtFormatR16_UNorm: return VK_FORMAT_R16_UNORM;
        case kUnityRenderingExtFormatR16_SNorm: return VK_FORMAT_R16_SNORM;
        case kUnityRenderingExtFormatR16_UInt: return VK_FORMAT_R16_UINT;
        case kUnityRenderingExtFormatR16_SInt: return VK_FORMAT_R16_SINT;
        case kUnityRenderingExtFormatR16_SFloat: return VK_FORMAT_R16_SFLOAT;
        case kUnityRenderingExtFormatR16G16_UNorm: return VK_FORMAT_R16G16_UNORM;
        case kUnityRenderingExtFormatR16G16_SNorm: return VK_FORMAT_R16G16_SNORM;
        case kUnityRenderingExtFormatR16G16_UInt: return VK_FORMAT_R16G16_UINT;
        case kUnityRenderingExtFormatR16G16_SInt: return VK_FORMAT_R16G16_SINT;
        case kUnityRenderingExtFormatR16G16_SFloat: return VK_FORMAT_R16G16_SFLOAT;
        case kUnityRenderingExtFormatR16G16B16_UNorm: return VK_FORMAT_R16G16B16_UNORM;
        case kUnityRenderingExtFormatR16G16B16_SNorm: return VK_FORMAT_R16G16B16_SNORM;
        case kUnityRenderingExtFormatR16G16B16_UInt: return VK_FORMAT_R16G16B16_UINT;
        case kUnityRenderingExtFormatR16G16B16_SInt: return VK_FORMAT_R16G16B16_SINT;
        case kUnityRenderingExtFormatR16G16B16_SFloat: return VK_FORMAT_R16G16B16_SFLOAT;
        case kUnityRenderingExtFormatR16G16B16A16_UNorm: return VK_FORMAT_R16G16B16A16_UNORM;
        case kUnityRenderingExtFormatR16G16B16A16_SNorm: return VK_FORMAT_R16G16B16A16_SNORM;
        case kUnityRenderingExtFormatR16G16B16A16_UInt: return VK_FORMAT_R16G16B16A16_UINT;
        case kUnityRenderingExtFormatR16G16B16A16_SInt: return VK_FORMAT_R16G16B16A16_SINT;
        case kUnityRenderingExtFormatR16G16B16A16_SFloat: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case kUnityRenderingExtFormatR32_UInt: return VK_FORMAT_R32_UINT;
        case kUnityRenderingExtFormatR32_SInt: return VK_FORMAT_R32_SINT;
        case kUnityRenderingExtFormatR32_SFloat: return VK_FORMAT_R32_SFLOAT;
        case kUnityRenderingExtFormatR32G32_UInt: return VK_FORMAT_R32G32_UINT;
        case kUnityRenderingExtFormatR32G32_SInt: return VK_FORMAT_R32G32_SINT;
        case kUnityRenderingExtFormatR32G32_SFloat: return VK_FORMAT_R32G32_SFLOAT;
        case kUnityRenderingExtFormatR32G32B32_UInt: return VK_FORMAT_R32G32B32_UINT;
        case kUnityRenderingExtFormatR32G32B32_SInt: return VK_FORMAT_R32G32B32_SINT;
        case kUnityRenderingExtFormatR32G32B32_SFloat: return VK_FORMAT_R32G32B32_SFLOAT;
        case kUnityRenderingExtFormatR32G32B32A32_UInt: return VK_FORMAT_R32G32B32A32_UINT;
        case kUnityRenderingExtFormatR32G32B32A32_SInt: return VK_FORMAT_R32G32B32A32_SINT;
        case kUnityRenderingExtFormatR32G32B32A32_SFloat: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case kUnityRenderingExtFormatR4G4B4A4_UNormPack16: return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
        case kUnityRenderingExtFormatB4G4R4A4_UNormPack16: return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
        case kUnityRenderingExtFormatR5G6B5_UNormPack16: return VK_FORMAT_R5G6B5_UNORM_PACK16;
        case kUnityRenderingExtFormatB5G6R5_UNormPack16: return VK_FORMAT_B5G6R5_UNORM_PACK16;
        case kUnityRenderingExtFormatR5G5B5A1_UNormPack16: return VK_FORMAT_R5G5B5A1_UNORM_PACK16;
        case kUnityRenderingExtFormatB5G5R5A1_UNormPack16: return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
        case kUnityRenderingExtFormatA1R5G5B5_UNormPack16: return VK_FORMAT_A1R5G5B5_UNORM_PACK16;
        case kUnityRenderingExtFormatE5B9G9R9_UFloatPack32: return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
        case kUnityRenderingExtFormatB10G11R11_UFloatPack32: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        case kUnityRenderingExtFormatA2R10G10B10_UNormPack32: return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
        case kUnityRenderingExtFormatA2R10G10B10_UIntPack32: return VK_FORMAT_A2R10G10B10_UINT_PACK32;
        case kUnityRenderingExtFormatA2R10G10B10_SIntPack32: return VK_FORMAT_A2R10G10B10_SINT_PACK32;
        case kUnityRenderingExtFormatA2B10G10R10_UNormPack32: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case kUnityRenderingExtFormatA2B10G10R10_UIntPack32: return VK_FORMAT_A2B10G10R10_UINT_PACK32;
        case kUnityRenderingExtFormatA2B10G10R10_SIntPack32: return VK_FORMAT_A2B10G10R10_SINT_PACK32;
        default: return VK_FORMAT_UNDEFINED;
    }
}
#endif