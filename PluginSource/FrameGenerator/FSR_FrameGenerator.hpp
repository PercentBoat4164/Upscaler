#pragma once
#include <vk_queue_selector.h>
#if defined(ENABLE_FRAME_GENERATION) && defined(ENABLE_FSR)
#include "FrameGenerator.hpp"

#include "GraphicsAPI/Vulkan.hpp"
#include "Plugin.hpp"

#include <ffx_api.hpp>
#include <ffx_framegeneration.hpp>
#include <vk/ffx_api_vk.hpp>

#include <vulkan/vulkan.h>

#include <IUnityGraphicsVulkan.h>

#include <array>

struct VqsQueueSelection;

class FSR_FrameGenerator final : protected FrameGenerator {
    static ffx::Context swapchainContext;
    static ffx::Context context;
    static std::array<FfxApiResource, 2> hudlessColorResource;
    static FfxApiResource depthResource;
    static FfxApiResource motionResource;

    static struct alignas(8) QueueData {
        uint32_t family{}, index{};
    } asyncCompute, present, imageAcquire;
    static bool supported;
    static bool asyncComputeSupported;

public:
    static void useQueues(std::vector<VqsQueueSelection> selection) {
        if (selection.size() >= 2) {
            std::construct_at(&imageAcquire, selection[0].queueFamilyIndex, selection[0].queueIndex);
            std::construct_at(&present, selection[1].queueFamilyIndex, selection[1].queueIndex);
            if (selection.size() == 3) {
                std::construct_at(&asyncCompute, selection[1].queueFamilyIndex, selection[1].queueIndex);
                // asyncComputeSupported = true;
            }
        }
    }

    static void createSwapchain(VkSwapchainKHR* pSwapchain, const VkSwapchainCreateInfoKHR* pCreateInfo, VkAllocationCallbacks* pAllocator, PFN_vkCreateSwapchainFFXAPI* pCreate, PFN_vkDestroySwapchainFFXAPI* pDestroy, PFN_vkGetSwapchainImagesKHR* pGet, PFN_vkAcquireNextImageKHR* pAcquire, PFN_vkQueuePresentKHR* pPresent, PFN_vkSetHdrMetadataEXT* pSet, PFN_getLastPresentCountFFXAPI* pCount) {
        destroySwapchain();
        ffx::CreateContextDescFrameGenerationSwapChainVK createContextDescFrameGenerationSwapChainVk{};
        createContextDescFrameGenerationSwapChainVk.physicalDevice = Vulkan::getGraphicsInterface()->Instance().physicalDevice;
        createContextDescFrameGenerationSwapChainVk.device         = Vulkan::getGraphicsInterface()->Instance().device;
        createContextDescFrameGenerationSwapChainVk.swapchain      = pSwapchain;
        createContextDescFrameGenerationSwapChainVk.allocator      = pAllocator;
        createContextDescFrameGenerationSwapChainVk.createInfo     = *pCreateInfo;
        std::construct_at(&createContextDescFrameGenerationSwapChainVk.gameQueue, Vulkan::getGraphicsInterface()->Instance().graphicsQueue, Vulkan::getGraphicsInterface()->Instance().queueFamilyIndex, nullptr);
        std::construct_at(&createContextDescFrameGenerationSwapChainVk.presentQueue, Vulkan::getQueue(present.family, present.index), present.family, nullptr);
        std::construct_at(&createContextDescFrameGenerationSwapChainVk.imageAcquireQueue, Vulkan::getQueue(imageAcquire.family, imageAcquire.index), imageAcquire.family, nullptr);
        if (asyncComputeSupported) std::construct_at(&createContextDescFrameGenerationSwapChainVk.asyncComputeQueue, Vulkan::getQueue(asyncCompute.family, asyncCompute.index), asyncCompute.family, nullptr);

        if (CreateContext(swapchainContext, nullptr, createContextDescFrameGenerationSwapChainVk) != ffx::ReturnCode::Ok)
            return Plugin::log("Failed to create swapchain context.", kUnityLogTypeError);

        ffx::CreateContextDescFrameGeneration createContextDescFrameGeneration{};
        createContextDescFrameGeneration.flags = (asyncComputeSupported ? FFX_FRAMEGENERATION_ENABLE_ASYNC_WORKLOAD_SUPPORT : 0U) |
                                                 static_cast<unsigned>(FFX_FRAMEGENERATION_ENABLE_DEPTH_INVERTED) |
                                                 static_cast<unsigned>(FFX_FRAMEGENERATION_ENABLE_HIGH_DYNAMIC_RANGE) |
                                                 static_cast<unsigned>(FFX_FRAMEGENERATION_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS);
        std::construct_at(&createContextDescFrameGeneration.displaySize, pCreateInfo->imageExtent.width, pCreateInfo->imageExtent.height);
        createContextDescFrameGeneration.backBufferFormat = ffxApiGetSurfaceFormatVK(pCreateInfo->imageFormat);
        std::construct_at(&createContextDescFrameGeneration.maxRenderSize, pCreateInfo->imageExtent.width, pCreateInfo->imageExtent.height);

        ffx::CreateBackendVKDesc createBackendVkDesc {};
        createBackendVkDesc.vkDevice         = Vulkan::getGraphicsInterface()->Instance().device;
        createBackendVkDesc.vkPhysicalDevice = Vulkan::getGraphicsInterface()->Instance().physicalDevice;
        createBackendVkDesc.vkDeviceProcAddr = Vulkan::getDeviceProcAddr();
        if (CreateContext(context, nullptr, createContextDescFrameGeneration, createBackendVkDesc) != ffx::ReturnCode::Ok || context == nullptr)
            return Plugin::log("Failed to create frame generation context.", kUnityLogTypeError);
        swapchain.vulkan = *pSwapchain;
        backBufferFormat = vkFormatToUnityFormat(pCreateInfo->imageFormat);

        ffx::QueryDescSwapchainReplacementFunctionsVK replacementFunctionsVk{};
        if (Query(swapchainContext, replacementFunctionsVk) != ffx::ReturnCode::Ok)
          return Plugin::log("Failed to query swapchain functions.", kUnityLogTypeError);
        if (pCreate != VK_NULL_HANDLE) *pCreate = replacementFunctionsVk.pOutCreateSwapchainFFXAPI;
        if (pDestroy != VK_NULL_HANDLE) *pDestroy = replacementFunctionsVk.pOutDestroySwapchainFFXAPI;
        if (pGet != VK_NULL_HANDLE) *pGet = replacementFunctionsVk.pOutGetSwapchainImagesKHR;
        if (pAcquire != VK_NULL_HANDLE) *pAcquire = replacementFunctionsVk.pOutAcquireNextImageKHR;
        if (pPresent != VK_NULL_HANDLE) *pPresent = replacementFunctionsVk.pOutQueuePresentKHR;
        if (pSet != VK_NULL_HANDLE) *pSet = replacementFunctionsVk.pOutSetHdrMetadataEXT;
        if (pCount != VK_NULL_HANDLE) *pCount = replacementFunctionsVk.pOutGetLastPresentCountFFXAPI;
    }

    static void destroySwapchain() {
        if (context != nullptr) {
            ffx::ConfigureDescFrameGeneration configureDescFrameGeneration {};
            configureDescFrameGeneration.swapChain                          = swapchain.vulkan;
            configureDescFrameGeneration.presentCallback                    = nullptr;
            configureDescFrameGeneration.presentCallbackUserContext         = nullptr;
            configureDescFrameGeneration.frameGenerationCallback            = nullptr;
            configureDescFrameGeneration.frameGenerationCallbackUserContext = nullptr;
            configureDescFrameGeneration.frameGenerationEnabled             = false;
            configureDescFrameGeneration.allowAsyncWorkloads                = false;
            std::construct_at(&configureDescFrameGeneration.HUDLessColor);
            configureDescFrameGeneration.flags                              = 0;
            configureDescFrameGeneration.onlyPresentGenerated               = false;
            std::construct_at(&configureDescFrameGeneration.generationRect, 0, 0, 0, 0);
            configureDescFrameGeneration.frameID                            = 0;
            if (Configure(context, configureDescFrameGeneration) != ffx::ReturnCode::Ok)
                Plugin::log("Failed to configure frame generation.", kUnityLogTypeError);
        }
        if (swapchainContext != nullptr) ffx::DestroyContext(swapchainContext, nullptr);
        swapchainContext = nullptr;
        if (context != nullptr) ffx::DestroyContext(context, nullptr);
        context = nullptr;
        swapchain.vulkan = VK_NULL_HANDLE;
        backBufferFormat = kUnityRenderingExtFormatNone;
    }

    static void useImages(VkImage color0, VkImage color1, VkImage depth, VkImage motion) {
        UnityVulkanImage image{};
        Vulkan::getGraphicsInterface()->AccessTexture(color0, UnityVulkanWholeImage, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, kUnityVulkanResourceAccess_PipelineBarrier, &image);
        hudlessColorResource.at(0).resource = image.image;
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
        hudlessColorResource.at(1).resource = image.image;
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
        image = {};
        Vulkan::getGraphicsInterface()->AccessTexture(depth, UnityVulkanWholeImage, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, kUnityVulkanResourceAccess_PipelineBarrier, &image);
        depthResource.resource = image.image;
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
        image = {};
        Vulkan::getGraphicsInterface()->AccessTexture(motion, UnityVulkanWholeImage, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, kUnityVulkanResourceAccess_PipelineBarrier, &image);
        motionResource.resource = image.image;
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

    static void evaluate(bool enable, FfxApiRect2D generationRect, FfxApiFloatCoords2D jitter, float frameTime, float farPlane, float nearPlane, float verticalFOV, unsigned index, unsigned options) {
        static uint32_t frameNumber;

        ffx::ConfigureDescFrameGeneration configureDescFrameGeneration {};
        configureDescFrameGeneration.swapChain                          = swapchain.vulkan;
        configureDescFrameGeneration.presentCallback                    = nullptr;
        configureDescFrameGeneration.presentCallbackUserContext         = nullptr;
        configureDescFrameGeneration.frameGenerationCallback            = [](ffxDispatchDescFrameGeneration* params, void* pUserCtx) -> ffxReturnCode_t { return ffxDispatch(static_cast<ffx::Context*>(pUserCtx), &params->header); };
        configureDescFrameGeneration.frameGenerationCallbackUserContext = &context;
        configureDescFrameGeneration.frameGenerationEnabled             = enable;
        configureDescFrameGeneration.allowAsyncWorkloads                = (options & 0x20U) != 0U && asyncComputeSupported;
        configureDescFrameGeneration.HUDLessColor                       = hudlessColorResource.at(index);
        configureDescFrameGeneration.flags                              = ((options & 0x1U) != 0U ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_VIEW : 0U) |
                                                                          ((options & 0x2U) != 0U ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_TEAR_LINES : 0U) |
                                                                          ((options & 0x4U) != 0U ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_RESET_INDICATORS : 0U) |
                                                                          ((options & 0x8U) != 0U ? FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_PACING_LINES : 0U) &
                                                                          ~static_cast<unsigned>(FFX_FRAMEGENERATION_FLAG_NO_SWAPCHAIN_CONTEXT_NOTIFY);
        configureDescFrameGeneration.onlyPresentGenerated               = (options & 0x10U) != 0U;
        configureDescFrameGeneration.generationRect                     = generationRect;
        configureDescFrameGeneration.frameID                            = frameNumber;
        if (Configure(context, configureDescFrameGeneration) != ffx::ReturnCode::Ok)
            Plugin::log("Failed to configure frame generation.", kUnityLogTypeError);

        if (configureDescFrameGeneration.frameGenerationEnabled) {
            UnityVulkanRecordingState state {};
            Vulkan::getGraphicsInterface()->CommandRecordingState(&state, kUnityVulkanGraphicsQueueAccess_DontCare);

            ffx::DispatchDescFrameGenerationPrepare dispatchDescFrameGenerationPrepare {};
            dispatchDescFrameGenerationPrepare.frameID                 = configureDescFrameGeneration.frameID;
            dispatchDescFrameGenerationPrepare.flags                   = configureDescFrameGeneration.flags;
            dispatchDescFrameGenerationPrepare.commandList             = state.commandBuffer;
            std::construct_at(&dispatchDescFrameGenerationPrepare.renderSize, generationRect.width, generationRect.height);
            dispatchDescFrameGenerationPrepare.jitterOffset            = jitter;
            std::construct_at(&dispatchDescFrameGenerationPrepare.motionVectorScale, -static_cast<float>(generationRect.width), -static_cast<float>(generationRect.height));
            dispatchDescFrameGenerationPrepare.frameTimeDelta          = frameTime;
            dispatchDescFrameGenerationPrepare.unused_reset            = false;
            dispatchDescFrameGenerationPrepare.cameraNear              = farPlane;
            dispatchDescFrameGenerationPrepare.cameraFar               = nearPlane;  // Switched because depth is inverted
            dispatchDescFrameGenerationPrepare.cameraFovAngleVertical  = verticalFOV;
            dispatchDescFrameGenerationPrepare.viewSpaceToMetersFactor = 1.0F;
            dispatchDescFrameGenerationPrepare.depth                   = depthResource;
            dispatchDescFrameGenerationPrepare.motionVectors           = motionResource;
            if (Dispatch(context, dispatchDescFrameGenerationPrepare) != ffx::ReturnCode::Ok)
                Plugin::log("Failed to dispatch frame generation prepare command.", kUnityLogTypeError);
        }
        ++frameNumber;
    }

    static bool ownsSwapchain(VkSwapchainKHR swapchain) {
        return FSR_FrameGenerator::swapchain.vulkan != VK_NULL_HANDLE && swapchain == FSR_FrameGenerator::swapchain.vulkan;
    }

    static ffx::Context* getContext() {
        return &swapchainContext;
    }

    static UnityRenderingExtTextureFormat vkFormatToUnityFormat(const VkFormat format) {
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

    static VkFormat unityFormatToVkFormat(const UnityRenderingExtTextureFormat format) {
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
};

#endif