#ifdef ENABLE_FSR
#    include "FSR_Upscaler.hpp"
#    ifdef ENABLE_VULKAN
#        include "GraphicsAPI/Vulkan.hpp"

#        include <IUnityGraphicsVulkan.h>

#        include <vk/ffx_api_vk.hpp>
#    endif
#    ifdef ENABLE_DX12
#        include "GraphicsAPI/DX12.hpp"

#        include <d3d12compatibility.h>

#        include <IUnityGraphicsD3D12.h>

#        include <dx12/ffx_api_dx12.hpp>
#    endif

#    include <ffx_upscale.hpp>

#    include <algorithm>

Upscaler::SupportState FSR_Upscaler::supported{Untested};

Upscaler::Status (FSR_Upscaler::*FSR_Upscaler::fpCreate)(ffx::CreateContextDescUpscale&){&FSR_Upscaler::safeFail};
Upscaler::Status (FSR_Upscaler::*FSR_Upscaler::fpSetResources)(const std::array<void*, Plugin::NumImages>&){&FSR_Upscaler::safeFail};
Upscaler::Status (FSR_Upscaler::*FSR_Upscaler::fpGetCommandBuffer)(void*&){&FSR_Upscaler::safeFail};

#    ifdef ENABLE_VULKAN
Upscaler::Status FSR_Upscaler::VulkanCreate(ffx::CreateContextDescUpscale& createContextDescUpscale) {
    ffx::CreateBackendVKDesc createBackendVKDesc;
    createBackendVKDesc.vkDevice = Vulkan::getGraphicsInterface()->Instance().device;
    createBackendVKDesc.vkPhysicalDevice = Vulkan::getGraphicsInterface()->Instance().physicalDevice;
    createBackendVKDesc.vkDeviceProcAddr = Vulkan::getDeviceProcAddr();
    return setStatus(CreateContext(context, nullptr, createContextDescUpscale, createBackendVKDesc), "Failed to create the AMD FidelityFx Super Resolution context.");
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
Upscaler::Status FSR_Upscaler::DX12Create(ffx::CreateContextDescUpscale& createContextDescUpscale) {
    ffx::CreateBackendDX12Desc createBackendDX12Desc;
    createBackendDX12Desc.device = DX12::getGraphicsInterface()->GetDevice();
    return setStatus(CreateContext(context, nullptr, createContextDescUpscale, createBackendDX12Desc), "Failed to create the AMD FidelityFx Super Resolution context.");
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
            .type=FFX_API_RESOURCE_TYPE_TEXTURE2D,
            .format=ffxApiGetSurfaceFormatDX12(imageDescription.Format),
            .width=static_cast<uint32_t>(imageDescription.Width),
            .height=static_cast<uint32_t>(imageDescription.Height),
            .alignment=static_cast<uint32_t>(imageDescription.Alignment),
            .mipCount=1U,
            .flags=FFX_API_RESOURCE_FLAGS_NONE,
            .usage=static_cast<uint32_t>(resourceUsage),
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

Upscaler::Status FSR_Upscaler::setStatus(const ffx::ReturnCode t_error, const std::string &t_msg) {
    switch (t_error) {
        case ffx::ReturnCode::Ok: return Upscaler::setStatus(Success, t_msg + " | Ok");
        case ffx::ReturnCode::Error: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | Error");
        case ffx::ReturnCode::ErrorUnknownDesctype: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | ErrorUnknownDesctype");
        case ffx::ReturnCode::ErrorRuntimeError: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | ErrorRuntimeError");
        case ffx::ReturnCode::ErrorNoProvider: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | ErrorNoProvider");
        case ffx::ReturnCode::ErrorMemory: return Upscaler::setStatus(OutOfMemory, t_msg + " | ErrorMemory");
        case ffx::ReturnCode::ErrorParameter: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | ErrorParameter");
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
    if (context != nullptr) setStatus(ffx::DestroyContext(context), "Failed to destroy the AMD FidelityFx Super Resolution context.");
    context = nullptr;
}

Upscaler::Status FSR_Upscaler::useSettings(const Settings::Resolution resolution, const Settings::DLSSPreset /*unused*/, const enum Settings::Quality mode, const bool hdr) {
    RETURN_ON_FAILURE(getStatus());
    Settings optimalSettings         = settings;
    optimalSettings.outputResolution = resolution;
    optimalSettings.hdr              = hdr;

    ffx::QueryDescUpscaleGetRenderResolutionFromQualityMode queryDescUpscaleGetRenderResolutionFromQualityMode;
    queryDescUpscaleGetRenderResolutionFromQualityMode.displayWidth     = optimalSettings.outputResolution.width;
    queryDescUpscaleGetRenderResolutionFromQualityMode.displayHeight    = optimalSettings.outputResolution.height;
    queryDescUpscaleGetRenderResolutionFromQualityMode.qualityMode      = optimalSettings.getQuality<Upscaler::FSR>(mode);
    queryDescUpscaleGetRenderResolutionFromQualityMode.pOutRenderWidth  = &optimalSettings.recommendedInputResolution.width;
    queryDescUpscaleGetRenderResolutionFromQualityMode.pOutRenderHeight = &optimalSettings.recommendedInputResolution.height;
    RETURN_ON_FAILURE(setStatus(ffx::Query(queryDescUpscaleGetRenderResolutionFromQualityMode), "Failed to query render resolution from quality mode. Ensure that the QualityMode setting is a valid enum value."));
    optimalSettings.dynamicMaximumInputResolution = resolution;

    ffx::CreateContextDescUpscale createContextDescUpscale;
    createContextDescUpscale.flags =
      static_cast<uint32_t>(FFX_UPSCALE_ENABLE_DEBUG_CHECKING) |
      static_cast<uint32_t>(FFX_UPSCALE_ENABLE_AUTO_EXPOSURE) |
      static_cast<uint32_t>(FFX_UPSCALE_ENABLE_DEPTH_INVERTED) |
      static_cast<uint32_t>(FFX_UPSCALE_ENABLE_DYNAMIC_RESOLUTION) |
      static_cast<uint32_t>(FFX_UPSCALE_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS) |
      (optimalSettings.hdr ? FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE : 0U);
    createContextDescUpscale.maxRenderSize  = FfxApiDimensions2D {optimalSettings.outputResolution.width, optimalSettings.outputResolution.height};
    createContextDescUpscale.maxUpscaleSize = FfxApiDimensions2D {optimalSettings.outputResolution.width, optimalSettings.outputResolution.height};
    createContextDescUpscale.fpMessage = reinterpret_cast<decltype(ffx::CreateContextDescUpscale::fpMessage)>(&FSR_Upscaler::log);

    if (context != nullptr) setStatus(ffx::DestroyContext(context), "Failed to destroy AMD FidelityFx Super Resolution context");
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
        ffx::DispatchDescUpscaleGenerateReactiveMask dispatchDescUpscaleGenerateReactiveMask;
        dispatchDescUpscaleGenerateReactiveMask.commandList     = commandBuffer;
        dispatchDescUpscaleGenerateReactiveMask.colorOpaqueOnly = resources.at(Plugin::Opaque);
        dispatchDescUpscaleGenerateReactiveMask.colorPreUpscale = resources.at(Plugin::Color);
        dispatchDescUpscaleGenerateReactiveMask.outReactive     = resources.at(Plugin::Reactive);
        dispatchDescUpscaleGenerateReactiveMask.renderSize      = FfxApiDimensions2D {resources.at(Plugin::Reactive).description.width, resources.at(Plugin::Reactive).description.height};
        dispatchDescUpscaleGenerateReactiveMask.scale           = settings.reactiveScale;
        dispatchDescUpscaleGenerateReactiveMask.cutoffThreshold = settings.reactiveThreshold;
        dispatchDescUpscaleGenerateReactiveMask.binaryValue     = settings.reactiveValue;
        dispatchDescUpscaleGenerateReactiveMask.flags           =
            static_cast<unsigned>(FFX_UPSCALE_AUTOREACTIVEFLAGS_APPLY_TONEMAP) |
            static_cast<unsigned>(FFX_UPSCALE_AUTOREACTIVEFLAGS_APPLY_THRESHOLD) |
            static_cast<unsigned>(FFX_UPSCALE_AUTOREACTIVEFLAGS_USE_COMPONENTS_MAX);
        RETURN_ON_FAILURE(setStatus(Dispatch(context, dispatchDescUpscaleGenerateReactiveMask), "Failed to dispatch AMD FidelityFx Super Resolution reactive mask generation commands."));
    }
    ffx::DispatchDescUpscale dispatchDescUpscale;
    dispatchDescUpscale.flags                      = settings.debugView ? FFX_UPSCALE_FLAG_DRAW_DEBUG_VIEW : 0U;
    dispatchDescUpscale.commandList                = commandBuffer;
    dispatchDescUpscale.color                      = resources.at(Plugin::Color);
    dispatchDescUpscale.depth                      = resources.at(Plugin::Depth);
    dispatchDescUpscale.motionVectors              = resources.at(Plugin::Motion);
    dispatchDescUpscale.exposure                   = FfxApiResource {};
    dispatchDescUpscale.reactive                   = settings.autoReactive ? resources.at(Plugin::Reactive) : FfxApiResource {};
    dispatchDescUpscale.transparencyAndComposition = FfxApiResource {};
    dispatchDescUpscale.output                     = resources.at(Plugin::Output);
    dispatchDescUpscale.jitterOffset               = FfxApiFloatCoords2D {settings.jitter.x, settings.jitter.y};
    dispatchDescUpscale.motionVectorScale          = FfxApiFloatCoords2D {-static_cast<float>(resources.at(Plugin::Motion).description.width), -static_cast<float>(resources.at(Plugin::Motion).description.height)};
    dispatchDescUpscale.renderSize                 = FfxApiDimensions2D {resources.at(Plugin::Color).description.width, resources.at(Plugin::Color).description.height};
    dispatchDescUpscale.upscaleSize                = FfxApiDimensions2D {resources.at(Plugin::Output).description.width, resources.at(Plugin::Output).description.height};
    dispatchDescUpscale.enableSharpening           = settings.sharpness > 0.0F;
    dispatchDescUpscale.sharpness                  = settings.sharpness;
    dispatchDescUpscale.frameTimeDelta             = settings.frameTime;
    dispatchDescUpscale.preExposure                = 1.0F;
    dispatchDescUpscale.reset                      = settings.resetHistory;
    dispatchDescUpscale.cameraNear                 = settings.farPlane;
    dispatchDescUpscale.cameraFar                  = settings.nearPlane;  // Switched because depth is inverted
    dispatchDescUpscale.cameraFovAngleVertical     = settings.verticalFOV * (3.1415926535897932384626433F / 180.0F);
    dispatchDescUpscale.viewSpaceToMetersFactor    = 1.0F;
    return setStatus(Dispatch(context, dispatchDescUpscale), "Failed to dispatch AMD FidelityFx Super Resolution upscale commands.");
}
#endif