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
Upscaler::SupportState FSR_Upscaler::supported{Untested};

Upscaler::Status (FSR_Upscaler::*FSR_Upscaler::fpCreate)(ffxCreateContextDescUpscale&){&FSR_Upscaler::safeFail};
Upscaler::Status (FSR_Upscaler::*FSR_Upscaler::fpSetResources)(const std::array<void*, Plugin::NumImages>&){&FSR_Upscaler::safeFail};
Upscaler::Status (FSR_Upscaler::*FSR_Upscaler::fpGetCommandBuffer)(void*&){&FSR_Upscaler::safeFail};

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
    const Status status = setStatus(ffxCreateContext(&context, &createContextDescUpscale.header, nullptr), "Failed to create the AMD FidelityFX Super Resolution context.");
    createContextDescUpscale.header.pNext = createBackendVKDesc.header.pNext;
    return status;
}

Upscaler::Status FSR_Upscaler::VulkanSetResources(const std::array<void*, Plugin::NumImages>& images) {
    for (Plugin::ImageID id{0}; id < (settings.autoReactive ? Plugin::NumImages : Plugin::NumBaseImages); ++reinterpret_cast<uint8_t&>(id)) {
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
        RETURN_ON_FAILURE(setStatusIf(resource == VK_NULL_HANDLE, RecoverableRuntimeError, "Unity provided a `VK_NULL_HANDLE` image."));
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
    Vulkan::getGraphicsInterface()->EnsureInsideRenderPass();
    RETURN_ON_FAILURE(Upscaler::setStatusIf(!Vulkan::getGraphicsInterface()->CommandRecordingState(&state, kUnityVulkanGraphicsQueueAccess_DontCare), FatalRuntimeError, "Unable to obtain a command recording state from Unity. This is fatal."));
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
    const Status status = setStatus(ffxCreateContext(&context, &createContextDescUpscale.header, nullptr), "Failed to create the AMD FidelityFX Super Resolution context.");
    createContextDescUpscale.header.pNext = createBackendDX12Desc.header.pNext;
    return status;
}

Upscaler::Status FSR_Upscaler::DX12SetResources(const std::array<void*, Plugin::NumImages>& images) {
    for (Plugin::ImageID id{0}; id < (settings.autoReactive ? Plugin::NumImages : Plugin::NumBaseImages); ++reinterpret_cast<uint8_t&>(id)) {
        FfxApiResorceUsage resourceUsage{FFX_API_RESOURCE_USAGE_READ_ONLY};
        if (id == Plugin::Output || id == Plugin::Reactive)
            resourceUsage = FFX_API_RESOURCE_USAGE_UAV;
        auto& [resource, description, state] = resources.at(id);
        resource = images.at(id);
        RETURN_ON_FAILURE(setStatusIf(resource == nullptr, RecoverableRuntimeError, "Unity provided a `nullptr` image."));
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
    RETURN_ON_FAILURE(Upscaler::setStatusIf(!DX12::getGraphicsInterface()->CommandRecordingState(&state), FatalRuntimeError, "Unable to obtain a command recording state from Unity. This is fatal."));
    commandList = state.commandList;
    return Success;
}
#    endif

void FSR_Upscaler::load(const GraphicsAPI::Type type, void* /*unused*/) {
    std::filesystem::path path = Plugin::path;
    switch (type) {
#ifndef NDEBUG
        case GraphicsAPI::VULKAN: path /= "amd_fidelityfx_vkdrel.dll"; break;
        case GraphicsAPI::DX12: path /= "amd_fidelityfx_dx12drel.dll"; break;
# else
        case GraphicsAPI::VULKAN: path /= "amd_fidelityfx_vk.dll"; break;
        case GraphicsAPI::DX12: path /= "amd_fidelityfx_dx12.dll"; break;
#endif
        default: return (void)(supported = Unsupported);
    }
    library = LoadLibrary(path.string().c_str());
    if (library == nullptr) return (void)(supported = Unsupported);
    ffxCreateContext  = reinterpret_cast<PfnFfxCreateContext>(GetProcAddress(library, "ffxCreateContext"));
    ffxDestroyContext = reinterpret_cast<PfnFfxDestroyContext>(GetProcAddress(library, "ffxDestroyContext"));
    ffxConfigure      = reinterpret_cast<PfnFfxConfigure>(GetProcAddress(library, "ffxConfigure"));
    ffxQuery          = reinterpret_cast<PfnFfxQuery>(GetProcAddress(library, "ffxQuery"));
    ffxDispatch       = reinterpret_cast<PfnFfxDispatch>(GetProcAddress(library, "ffxDispatch"));
    if (ffxCreateContext == nullptr || ffxDestroyContext == nullptr || ffxConfigure == nullptr || ffxQuery == nullptr || ffxDispatch == nullptr) supported = Unsupported;
}

void FSR_Upscaler::shutdown() {}

void FSR_Upscaler::unload() {
    ffxCreateContext = nullptr;
    ffxDestroyContext = nullptr;
    ffxConfigure = nullptr;
    ffxQuery = nullptr;
    ffxDispatch = nullptr;
    if (library != nullptr) FreeLibrary(library);
    library = nullptr;
}

Upscaler::Status FSR_Upscaler::setStatus(const ffxReturnCode_t t_error, const std::string &t_msg) {
    switch (t_error) {
        case FFX_API_RETURN_OK: return Upscaler::setStatus(Success, t_msg + " | Ok");
        case FFX_API_RETURN_ERROR: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | Error");
        case FFX_API_RETURN_ERROR_UNKNOWN_DESCTYPE: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | ErrorUnknownDesctype");
        case FFX_API_RETURN_ERROR_RUNTIME_ERROR: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | ErrorRuntimeError");
        case FFX_API_RETURN_NO_PROVIDER: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | ErrorNoProvider");
        case FFX_API_RETURN_ERROR_MEMORY: return Upscaler::setStatus(OutOfMemory, t_msg + " | ErrorMemory");
        case FFX_API_RETURN_ERROR_PARAMETER: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | ErrorParameter");
        default: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | Unknown");
    }
}

void FSR_Upscaler::log(const FfxApiMsgType type, const wchar_t *t_msg) {
    std::wstring message(t_msg);
    std::string  msg(message.length(), 0);
    std::ranges::transform(message, msg.begin(), [](const wchar_t c) { return static_cast<char>(c); });
    UnityLogType unityType = kUnityLogTypeLog;
    switch (type) {
        case FFX_API_MESSAGE_TYPE_ERROR: unityType = kUnityLogTypeError; break;
        case FFX_API_MESSAGE_TYPE_WARNING: unityType = kUnityLogTypeWarning; break;
        default: break;
    }
    Plugin::log(msg, unityType);
}

bool FSR_Upscaler::isSupported() {
    if (supported != Untested) return supported == Supported;
    return (supported = success(FSR_Upscaler().useSettings({32, 32}, Settings::DLSSPreset::Default, Settings::Quality::Auto, false)) ? Supported : Unsupported) == Supported;
}

bool FSR_Upscaler::isSupported(const enum Settings::Quality mode) {
    return mode == Settings::AntiAliasing || mode == Settings::Auto || mode == Settings::Quality || mode == Settings::Balanced || mode == Settings::Performance || mode == Settings::UltraPerformance;
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
            fpCreate           = &FSR_Upscaler::invalidGraphicsAPIFail;
            fpSetResources     = &FSR_Upscaler::invalidGraphicsAPIFail;
            fpGetCommandBuffer = &FSR_Upscaler::invalidGraphicsAPIFail;
            break;
        }
    }
}

FSR_Upscaler::~FSR_Upscaler() {
    if (context != nullptr) setStatus(ffxDestroyContext(&context, nullptr), "Failed to destroy the AMD FidelityFX Super Resolution context.");
    context = nullptr;
}

Upscaler::Status FSR_Upscaler::useSettings(const Settings::Resolution resolution, const Settings::DLSSPreset /*unused*/, const enum Settings::Quality mode, const bool hdr) {
    RETURN_ON_FAILURE(getStatus());
    Settings optimalSettings         = settings;
    optimalSettings.outputResolution = resolution;
    optimalSettings.hdr              = hdr;

    ffxQueryDescUpscaleGetRenderResolutionFromQualityMode queryDescUpscaleGetRenderResolutionFromQualityMode {
        .header = {
            .type  = FFX_API_QUERY_DESC_TYPE_UPSCALE_GETRENDERRESOLUTIONFROMQUALITYMODE,
            .pNext = nullptr
        },
        .displayWidth     = optimalSettings.outputResolution.width,
        .displayHeight    = optimalSettings.outputResolution.height,
        .qualityMode      = static_cast<uint32_t>(optimalSettings.getQuality<FSR>(mode)),
        .pOutRenderWidth  = &optimalSettings.recommendedInputResolution.width,
        .pOutRenderHeight = &optimalSettings.recommendedInputResolution.height
    };
    RETURN_ON_FAILURE(setStatus(ffxQuery(nullptr, &queryDescUpscaleGetRenderResolutionFromQualityMode.header), "Failed to query render resolution from quality mode. Ensure that the QualityMode setting is a valid enum value."));
    optimalSettings.dynamicMaximumInputResolution = resolution;

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
            static_cast<uint32_t>(FFX_UPSCALE_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS) |
            (optimalSettings.hdr ? FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE : 0U),
        .maxRenderSize  = FfxApiDimensions2D {optimalSettings.outputResolution.width, optimalSettings.outputResolution.height},
        .maxUpscaleSize = FfxApiDimensions2D {optimalSettings.outputResolution.width, optimalSettings.outputResolution.height},
        .fpMessage      = reinterpret_cast<decltype(ffxCreateContextDescUpscale::fpMessage)>(&FSR_Upscaler::log)
    };

    if (context != nullptr) setStatus(ffxDestroyContext(&context, nullptr), "Failed to destroy AMD FidelityFX Super Resolution context");
    context = nullptr;
    RETURN_ON_FAILURE((this->*fpCreate)(createContextDescUpscale));
    settings = optimalSettings;
    return Success;
}

Upscaler::Status FSR_Upscaler::useImages(const std::array<void*, Plugin::NumImages>& images) {
    return (this->*fpSetResources)(images);
}

Upscaler::Status FSR_Upscaler::evaluate() {
    void* commandBuffer {};
    RETURN_ON_FAILURE((this->*fpGetCommandBuffer)(commandBuffer));

    if (settings.autoReactive) {
        const ffxDispatchDescUpscaleGenerateReactiveMask dispatchDescUpscaleGenerateReactiveMask{
          .header = {
              .type  = FFX_API_DISPATCH_DESC_TYPE_UPSCALE_GENERATEREACTIVEMASK,
              .pNext = nullptr
          },
          .commandList     = commandBuffer,
          .colorOpaqueOnly = resources.at(Plugin::Opaque),
          .colorPreUpscale = resources.at(Plugin::Color),
          .outReactive     = resources.at(Plugin::Reactive),
          .renderSize      = FfxApiDimensions2D{resources.at(Plugin::Reactive).description.width, resources.at(Plugin::Reactive).description.height},
          .scale           = settings.reactiveScale,
          .cutoffThreshold = settings.reactiveThreshold,
          .binaryValue     = settings.reactiveValue,
          .flags =
            static_cast<unsigned>(FFX_UPSCALE_AUTOREACTIVEFLAGS_APPLY_TONEMAP) |
            static_cast<unsigned>(FFX_UPSCALE_AUTOREACTIVEFLAGS_APPLY_THRESHOLD) |
            static_cast<unsigned>(FFX_UPSCALE_AUTOREACTIVEFLAGS_USE_COMPONENTS_MAX)
        };
        RETURN_ON_FAILURE(setStatus(ffxDispatch(&context, &dispatchDescUpscaleGenerateReactiveMask.header), "Failed to dispatch AMD FidelityFX Super Resolution reactive mask generation commands."));
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
        .reactive                   = settings.autoReactive ? resources.at(Plugin::Reactive) : FfxApiResource {},
        .transparencyAndComposition = FfxApiResource {},
        .output                     = resources.at(Plugin::Output),
        .jitterOffset               = FfxApiFloatCoords2D {settings.jitter.x, settings.jitter.y},
        .motionVectorScale          = FfxApiFloatCoords2D {-static_cast<float>(resources.at(Plugin::Motion).description.width), -static_cast<float>(resources.at(Plugin::Motion).description.height)},
        .renderSize                 = FfxApiDimensions2D {resources.at(Plugin::Color).description.width, resources.at(Plugin::Color).description.height},
        .upscaleSize                = FfxApiDimensions2D {resources.at(Plugin::Output).description.width, resources.at(Plugin::Output).description.height},
        .enableSharpening           = settings.sharpness > 0.0F,
        .sharpness                  = settings.sharpness,
        .frameTimeDelta             = settings.frameTime,
        .preExposure                = 1.0F,
        .reset                      = settings.resetHistory,
        .cameraNear                 = settings.farPlane,
        .cameraFar                  = settings.nearPlane,  // Switched because depth is inverted
        .cameraFovAngleVertical     = settings.verticalFOV * (3.1415926535897932384626433F / 180.0F),
        .viewSpaceToMetersFactor    = 1.0F,
        .flags                      = settings.debugView ? FFX_UPSCALE_FLAG_DRAW_DEBUG_VIEW : 0U
    };
    return setStatus(ffxDispatch(&context, &dispatchDescUpscale.header), "Failed to dispatch AMD FidelityFX Super Resolution upscale commands.");
}
#endif