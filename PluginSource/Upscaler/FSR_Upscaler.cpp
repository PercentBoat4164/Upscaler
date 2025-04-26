#ifdef ENABLE_FSR
#    include "FSR_Upscaler.hpp"
#    ifdef ENABLE_VULKAN
#        include "GraphicsAPI/Vulkan.hpp"

#        include <IUnityGraphicsVulkan.h>

#        include <vk/ffx_api_vk.h>
#    endif
#    ifdef ENABLE_DX12
#        include "GraphicsAPI/DX12.hpp"

#        include <d3d12compatibility.h>

#        include <IUnityGraphicsD3D12.h>

#        include <dx12/ffx_api_dx12.h>
#    endif

#    include <ffx_upscale.h>

#    include <algorithm>

HMODULE FSR_Upscaler::library{nullptr};
bool FSR_Upscaler::loaded{false};

Upscaler::Status (FSR_Upscaler::*FSR_Upscaler::fpCreate)(ffxCreateContextDescUpscale&){&safeFail};
Upscaler::Status (FSR_Upscaler::*FSR_Upscaler::fpSetResources)(const std::array<void*, 6>&){&safeFail};
Upscaler::Status (*FSR_Upscaler::fpGetCommandBuffer)(void*&){&staticSafeFail};

PfnFfxCreateContext FSR_Upscaler::ffxCreateContext;
PfnFfxDestroyContext FSR_Upscaler::ffxDestroyContext;
PfnFfxConfigure FSR_Upscaler::ffxConfigure;
PfnFfxQuery FSR_Upscaler::ffxQuery;
PfnFfxDispatch FSR_Upscaler::ffxDispatch;

#    ifdef ENABLE_VULKAN
Upscaler::Status FSR_Upscaler::VulkanCreate(ffxCreateContextDescUpscale& createContextDescUpscale) {
    ffxCreateBackendVKDesc createBackendVKDesc {
        .header ={
            .type  = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_VK,
            .pNext = createContextDescUpscale.header.pNext
        },
        .vkDevice         = Vulkan::getGraphicsInterface()->Instance().device,
        .vkPhysicalDevice = Vulkan::getGraphicsInterface()->Instance().physicalDevice,
        .vkDeviceProcAddr = Vulkan::getDeviceProcAddr()
    };
    createContextDescUpscale.header.pNext = &createBackendVKDesc.header;
    const Status status = setStatus(ffxCreateContext(&context, &createContextDescUpscale.header, nullptr));
    createContextDescUpscale.header.pNext = createBackendVKDesc.header.pNext;
    return status;
}

Upscaler::Status FSR_Upscaler::VulkanSetResources(const std::array<void*, 6>& images) {
    for (Plugin::ImageID id{0}; id < (autoReactive ? images.size() : 4); ++reinterpret_cast<uint8_t&>(id)) {
        VkAccessFlags      accessFlags{VK_ACCESS_SHADER_READ_BIT};
        FfxApiResorceUsage resourceUsage{FFX_API_RESOURCE_USAGE_READ_ONLY};
        if (id == Plugin::Output || id == Plugin::Reactive) {
            accessFlags   = VK_ACCESS_SHADER_WRITE_BIT;
            resourceUsage = FFX_API_RESOURCE_USAGE_UAV;
        }
        UnityVulkanImage image {};
        Vulkan::getGraphicsInterface()->AccessTexture(images.at(id), UnityVulkanWholeImage, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, accessFlags, kUnityVulkanResourceAccess_PipelineBarrier, &image);
        auto& [resource, description, state] = resources.at(id);
        resource = image.image;
        RETURN_STATUS_WITH_MESSAGE_IF(resource == VK_NULL_HANDLE, RecoverableRuntimeError, "Unity provided a `VK_NULL_HANDLE` image.");
        description = {
            .type     = FFX_API_RESOURCE_TYPE_TEXTURE2D,
            .format   = ffxApiGetSurfaceFormatVK(image.format),
            .width    = image.extent.width,
            .height   = image.extent.height,
            .depth    = image.extent.depth,
            .mipCount = 1U,
            .flags    = FFX_API_RESOURCE_FLAGS_ALIASABLE,
            .usage    = static_cast<uint32_t>(resourceUsage),
        };
        state = static_cast<uint32_t>(resourceUsage == FFX_API_RESOURCE_USAGE_UAV ? FFX_API_RESOURCE_STATE_UNORDERED_ACCESS : FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    }
    return Success;
}

Upscaler::Status FSR_Upscaler::VulkanGetCommandBuffer(void*& commandBuffer) {
    UnityVulkanRecordingState state {};
    Vulkan::getGraphicsInterface()->EnsureOutsideRenderPass();
    RETURN_STATUS_WITH_MESSAGE_IF(!Vulkan::getGraphicsInterface()->CommandRecordingState(&state, kUnityVulkanGraphicsQueueAccess_DontCare), FatalRuntimeError, "Unable to obtain a command recording state from Unity. This is fatal.");
    commandBuffer = state.commandBuffer;
    return Success;
}
#    endif

#    ifdef ENABLE_DX12
Upscaler::Status FSR_Upscaler::DX12Create(ffxCreateContextDescUpscale& createContextDescUpscale) {
    ffxCreateBackendDX12Desc createBackendDX12Desc {
        .header = {
            .type  = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12,
            .pNext = createContextDescUpscale.header.pNext
        },
        .device = DX12::getGraphicsInterface()->GetDevice()
    };
    createContextDescUpscale.header.pNext = &createBackendDX12Desc.header;
    const Status status = setStatus(ffxCreateContext(&context, &createContextDescUpscale.header, nullptr));
    createContextDescUpscale.header.pNext = createBackendDX12Desc.header.pNext;
    return status;
}

Upscaler::Status FSR_Upscaler::DX12SetResources(const std::array<void*, 6>& images) {
    for (Plugin::ImageID id{0}; id < (autoReactive ? images.size() : 4); ++reinterpret_cast<uint8_t&>(id)) {
        FfxApiResorceUsage resourceUsage{FFX_API_RESOURCE_USAGE_READ_ONLY};
        if (id == Plugin::Output || id == Plugin::Reactive) resourceUsage = FFX_API_RESOURCE_USAGE_UAV;
        auto& [resource, description, state] = resources.at(id);
        resource = images.at(id);
        RETURN_STATUS_WITH_MESSAGE_IF(resource == nullptr, RecoverableRuntimeError, "Unity provided a `nullptr` image.");
        const D3D12_RESOURCE_DESC imageDescription = static_cast<ID3D12Resource*>(resource)->GetDesc();
        description = {
            .type      = FFX_API_RESOURCE_TYPE_TEXTURE2D,
            .format    = ffxApiGetSurfaceFormatDX12(imageDescription.Format),
            .width     = static_cast<uint32_t>(imageDescription.Width),
            .height    = static_cast<uint32_t>(imageDescription.Height),
            .alignment = static_cast<uint32_t>(imageDescription.Alignment),
            .mipCount  = 1U,
            .flags     = FFX_API_RESOURCE_FLAGS_NONE,
            .usage     = static_cast<uint32_t>(resourceUsage),
        };
        state = static_cast<uint32_t>(resourceUsage == FFX_API_RESOURCE_USAGE_UAV ? FFX_API_RESOURCE_STATE_UNORDERED_ACCESS : FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    }
    return Success;
}

Upscaler::Status FSR_Upscaler::DX12GetCommandBuffer(void*& commandList) {
    UnityGraphicsD3D12RecordingState state {};
    RETURN_STATUS_WITH_MESSAGE_IF(!DX12::getGraphicsInterface()->CommandRecordingState(&state), FatalRuntimeError, "Unable to obtain a command recording state from Unity. This is fatal.");
    commandList = state.commandList;
    return Success;
}
#    endif

bool FSR_Upscaler::loadedCorrectly() {
    return loaded;
}

void FSR_Upscaler::load(const GraphicsAPI::Type type, void* /*unused*/) {
    std::filesystem::path path = Plugin::path;
    switch (type) {
#ifndef NDEBUG
        case GraphicsAPI::VULKAN: path /= "amd_fidelityfx_vkd.dll"; break;
        case GraphicsAPI::DX12: path /= "amd_fidelityfx_dx12d.dll"; break;
# else
        case GraphicsAPI::VULKAN: path /= "amd_fidelityfx_vk.dll"; break;
        case GraphicsAPI::DX12: path /= "amd_fidelityfx_dx12.dll"; break;
#endif
        default: return (void)(loaded = false);
    }
    library = LoadLibrary(path.string().c_str());
    if (library == nullptr) return (void)(loaded = false);
    ffxCreateContext  = reinterpret_cast<PfnFfxCreateContext>(GetProcAddress(library, "ffxCreateContext"));
    ffxDestroyContext = reinterpret_cast<PfnFfxDestroyContext>(GetProcAddress(library, "ffxDestroyContext"));
    ffxConfigure      = reinterpret_cast<PfnFfxConfigure>(GetProcAddress(library, "ffxConfigure"));
    ffxQuery          = reinterpret_cast<PfnFfxQuery>(GetProcAddress(library, "ffxQuery"));
    ffxDispatch       = reinterpret_cast<PfnFfxDispatch>(GetProcAddress(library, "ffxDispatch"));
    loaded = ffxCreateContext != nullptr && ffxDestroyContext != nullptr && ffxConfigure != nullptr && ffxQuery != nullptr && ffxDispatch != nullptr;
}

void FSR_Upscaler::unload() {
    ffxCreateContext = nullptr;
    ffxDestroyContext = nullptr;
    ffxConfigure = nullptr;
    ffxQuery = nullptr;
    ffxDispatch = nullptr;
    if (library != nullptr) FreeLibrary(library);
    library = nullptr;
}

Upscaler::Status FSR_Upscaler::setStatus(const ffxReturnCode_t t_error) {
    switch (t_error) {
        case FFX_API_RETURN_OK: return Success;
        case FFX_API_RETURN_ERROR: return FatalRuntimeError;
        case FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE: return FatalRuntimeError;
        case FFX_API_RETURN_ERROR_RUNTIME_ERROR: return FatalRuntimeError;
        case FFX_API_RETURN_NO_PROVIDER: return FatalRuntimeError;
        case FFX_API_RETURN_ERROR_MEMORY: return OutOfMemory;
        case FFX_API_RETURN_ERROR_PARAMETER: return FatalRuntimeError;
        default: return FatalRuntimeError;
    }
}

void FSR_Upscaler::log(const FfxApiMsgType /*unused*/, const wchar_t* t_msg) {
    std::wstring message(t_msg);
    std::string  msg(message.length(), 0);
    std::ranges::transform(message, msg.begin(), [](const wchar_t c) { return static_cast<char>(c); });
    Plugin::log(msg);
}

FfxApiUpscaleQualityMode FSR_Upscaler::getQuality(const enum Quality quality) const {
    switch (quality) {
        case Auto: {
            const uint32_t pixelCount{outputResolution.width * outputResolution.height};
            if (pixelCount <= 2560U * 1440U) return FFX_UPSCALE_QUALITY_MODE_QUALITY;
            if (pixelCount <= 3840U * 2160U) return FFX_UPSCALE_QUALITY_MODE_PERFORMANCE;
            return FFX_UPSCALE_QUALITY_MODE_ULTRA_PERFORMANCE;
        }
        case AntiAliasing: return FFX_UPSCALE_QUALITY_MODE_NATIVEAA;
        case Quality: return FFX_UPSCALE_QUALITY_MODE_QUALITY;
        case Balanced: return FFX_UPSCALE_QUALITY_MODE_BALANCED;
        case Performance: return FFX_UPSCALE_QUALITY_MODE_PERFORMANCE;
        case UltraPerformance: return FFX_UPSCALE_QUALITY_MODE_ULTRA_PERFORMANCE;
        default: return static_cast<FfxApiUpscaleQualityMode>(-1);
    }
}

void FSR_Upscaler::useGraphicsAPI(const GraphicsAPI::Type type) {
    switch (type) {
#    ifdef ENABLE_VULKAN
        case GraphicsAPI::VULKAN: {
            fpCreate           = &FSR_Upscaler::VulkanCreate;
            fpSetResources     = &FSR_Upscaler::VulkanSetResources;
            fpGetCommandBuffer = &FSR_Upscaler::VulkanGetCommandBuffer;
            break;
        }
#    endif
#    ifdef ENABLE_DX12
        case GraphicsAPI::DX12: {
            fpCreate           = &FSR_Upscaler::DX12Create;
            fpSetResources     = &FSR_Upscaler::DX12SetResources;
            fpGetCommandBuffer = &FSR_Upscaler::DX12GetCommandBuffer;
            break;
        }
#    endif
        default: {
            fpCreate           = &safeFail<UnsupportedGraphicsApi>;
            fpSetResources     = &safeFail<UnsupportedGraphicsApi>;
            fpGetCommandBuffer = &staticSafeFail<UnsupportedGraphicsApi>;
            break;
        }
    }
}

FSR_Upscaler::~FSR_Upscaler() {
    if (context != nullptr) setStatus(ffxDestroyContext(&context, nullptr));
    context = nullptr;
}

Upscaler::Status FSR_Upscaler::useSettings(const Resolution resolution, const enum Quality mode, const bool hdr) {
    outputResolution = resolution;
    ffxQueryDescUpscaleGetRenderResolutionFromQualityMode queryDescUpscaleGetRenderResolutionFromQualityMode {
        .header = {
            .type  = FFX_API_QUERY_DESC_TYPE_UPSCALE_GETRENDERRESOLUTIONFROMQUALITYMODE,
            .pNext = nullptr
        },
        .displayWidth     = outputResolution.width,
        .displayHeight    = outputResolution.height,
        .qualityMode      = static_cast<uint32_t>(getQuality(mode)),
        .pOutRenderWidth  = &recommendedInputResolution.width,
        .pOutRenderHeight = &recommendedInputResolution.height
    };
    RETURN_WITH_MESSAGE_IF(setStatus(ffxQuery(nullptr, &queryDescUpscaleGetRenderResolutionFromQualityMode.header)), "Failed to query render resolution from quality mode. Ensure that the QualityMode setting is a valid enum value.");

    dynamicMinimumInputResolution = {1, 1};
    dynamicMaximumInputResolution = outputResolution;

    ffxCreateContextDescUpscale createContextDescUpscale {
        .header = {
            .type  = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE,
            .pNext = nullptr
        },
        .flags =
#ifndef NDEBUG
            static_cast<uint32_t>(FFX_UPSCALE_ENABLE_DEBUG_CHECKING) |
#endif
            static_cast<uint32_t>(FFX_UPSCALE_ENABLE_AUTO_EXPOSURE) |
            static_cast<uint32_t>(FFX_UPSCALE_ENABLE_DEPTH_INVERTED) |
            static_cast<uint32_t>(FFX_UPSCALE_ENABLE_DYNAMIC_RESOLUTION) |
            (hdr ? FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE : 0U),
        .maxRenderSize  = FfxApiDimensions2D {outputResolution.width, outputResolution.height},
        .maxUpscaleSize = FfxApiDimensions2D {outputResolution.width, outputResolution.height},
        .fpMessage      = reinterpret_cast<decltype(ffxCreateContextDescUpscale::fpMessage)>(&FSR_Upscaler::log)
    };

    if (context != nullptr) RETURN_WITH_MESSAGE_IF(setStatus(ffxDestroyContext(&context, nullptr)), "Failed to destroy AMD FidelityFX Super Resolution context");
    context = nullptr;
    RETURN_IF((this->*fpCreate)(createContextDescUpscale));
    return Success;
}

Upscaler::Status FSR_Upscaler::useImages(const std::array<void*, 6>& images) {
    return (this->*fpSetResources)(images);
}

Upscaler::Status FSR_Upscaler::evaluate(const Resolution inputResolution) {
    void* commandBuffer {};
    RETURN_IF(fpGetCommandBuffer(commandBuffer));

    if (autoReactive) {
        const ffxDispatchDescUpscaleGenerateReactiveMask dispatchDescUpscaleGenerateReactiveMask{
          .header = {
              .type  = FFX_API_DISPATCH_DESC_TYPE_UPSCALE_GENERATEREACTIVEMASK,
              .pNext = nullptr
          },
          .commandList     = commandBuffer,
          .colorOpaqueOnly = resources.at(Plugin::Opaque),
          .colorPreUpscale = resources.at(Plugin::Color),
          .outReactive     = resources.at(Plugin::Reactive),
          .renderSize      = FfxApiDimensions2D{inputResolution.width, inputResolution.height},
          .scale           = reactiveScale,
          .cutoffThreshold = reactiveThreshold,
          .binaryValue     = reactiveValue,
          .flags =
            static_cast<unsigned>(FFX_UPSCALE_AUTOREACTIVEFLAGS_APPLY_TONEMAP) |
            static_cast<unsigned>(FFX_UPSCALE_AUTOREACTIVEFLAGS_APPLY_THRESHOLD) |
            static_cast<unsigned>(FFX_UPSCALE_AUTOREACTIVEFLAGS_USE_COMPONENTS_MAX)
        };
        RETURN_WITH_MESSAGE_IF(setStatus(ffxDispatch(&context, &dispatchDescUpscaleGenerateReactiveMask.header)), "Failed to dispatch AMD FidelityFX Super Resolution reactive mask generation commands.");
    }
    const ffxDispatchDescUpscale dispatchDescUpscale {
        .header = {
            .type  = FFX_API_DISPATCH_DESC_TYPE_UPSCALE,
            .pNext = nullptr
        },
        .commandList                = commandBuffer,
        .color                      = resources.at(Plugin::Color),
        .depth                      = resources.at(Plugin::Depth),
        .motionVectors              = resources.at(Plugin::Motion),
        .exposure                   = FfxApiResource {},
        .reactive                   = autoReactive ? resources.at(Plugin::Reactive) : FfxApiResource {},
        .transparencyAndComposition = FfxApiResource {},
        .output                     = resources.at(Plugin::Output),
        .jitterOffset               = FfxApiFloatCoords2D {jitter.x, jitter.y},
        .motionVectorScale          = FfxApiFloatCoords2D {-static_cast<float>(resources.at(Plugin::Motion).description.width), -static_cast<float>(resources.at(Plugin::Motion).description.height)},
        .renderSize                 = FfxApiDimensions2D {inputResolution.width, inputResolution.height},
        .upscaleSize                = FfxApiDimensions2D {resources.at(Plugin::Output).description.width, resources.at(Plugin::Output).description.height},
        .enableSharpening           = sharpness > 0.0F,
        .sharpness                  = sharpness,
        .frameTimeDelta             = frameTime,
        .preExposure                = 1.0F,
        .reset                      = resetHistory,
        .cameraNear                 = farPlane,
        .cameraFar                  = nearPlane,  // Switched because depth is inverted
        .cameraFovAngleVertical     = verticalFOV * (3.1415926535897932384626433F / 180.0F),
        .viewSpaceToMetersFactor    = 1.0F,
        .flags                      = debugView ? FFX_UPSCALE_FLAG_DRAW_DEBUG_VIEW : 0U
    };
    RETURN_WITH_MESSAGE_IF(setStatus(ffxDispatch(&context, &dispatchDescUpscale.header)), "Failed to dispatch AMD FidelityFX Super Resolution upscaling commands.");
    return Success;
}
#endif