#ifdef ENABLE_FSR3
#    include "FSR3.hpp"
#    ifdef ENABLE_VULKAN
#        include "GraphicsAPI/Vulkan.hpp"

#        include <IUnityGraphicsVulkan.h>

#        include <ffx_api/vk/ffx_api_vk.hpp>
#    endif
#    ifdef ENABLE_DX12
#        include "GraphicsAPI/DX12.hpp"

#        include <d3d12compatibility.h>

#        include <IUnityGraphicsD3D12.h>

#        include <ffx_api/dx12/ffx_api_dx12.hpp>
#    endif

#    include <ffx_api/ffx_upscale.hpp>

#    include <algorithm>

HMODULE FSR3::library{nullptr};
uint32_t FSR3::users{};
Upscaler::SupportState FSR3::supported{Untested};

Upscaler::Status (FSR3::*FSR3::fpCreate)(ffx::CreateContextDescUpscale&){&FSR3::safeFail};
Upscaler::Status (FSR3::*FSR3::fpGetResources)(std::array<FfxApiResource, 6>&, void*&){&FSR3::safeFail};

#    ifdef ENABLE_VULKAN
Upscaler::Status FSR3::VulkanCreate(ffx::CreateContextDescUpscale& createContextDescUpscale) {
    ffx::CreateBackendVKDesc createBackendVKDesc;
    createBackendVKDesc.vkDevice = Vulkan::getGraphicsInterface()->Instance().device;
    createBackendVKDesc.vkPhysicalDevice = Vulkan::getGraphicsInterface()->Instance().physicalDevice;
    createBackendVKDesc.vkDeviceProcAddr = Vulkan::getDeviceProcAddr();
    return setStatus(CreateContext(context, nullptr, createContextDescUpscale, createBackendVKDesc), "Failed to create the AMD FidelityFx Super Resolution context.");
}

Upscaler::Status FSR3::VulkanGetResources(std::array<FfxApiResource, 6>& resources, void*& commandBuffer) {
    for (Plugin::ImageID id{Plugin::ImageID::Color}; id <= (settings.autoReactive ? Plugin::ImageID::Opaque : Plugin::ImageID::Output); ++reinterpret_cast<uint8_t&>(id)) {
        VkAccessFlags      accessFlags{VK_ACCESS_SHADER_READ_BIT};
        FfxApiResorceUsage resourceUsage{id == Plugin::ImageID::Depth ? FFX_API_RESOURCE_USAGE_DEPTHTARGET : FFX_API_RESOURCE_USAGE_READ_ONLY};
        if (id == Plugin::ImageID::Output || id == Plugin::ImageID::Reactive) {
            accessFlags   = VK_ACCESS_SHADER_WRITE_BIT;
            resourceUsage = FFX_API_RESOURCE_USAGE_UAV;
        }
        UnityVulkanImage image {};
        Vulkan::getGraphicsInterface()->AccessTexture(textures.at(id), UnityVulkanWholeImage, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, accessFlags, kUnityVulkanResourceAccess_PipelineBarrier, &image);
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
    UnityVulkanRecordingState state {};
    Vulkan::getGraphicsInterface()->EnsureInsideRenderPass();
    RETURN_ON_FAILURE(Upscaler::setStatusIf(!Vulkan::getGraphicsInterface()->CommandRecordingState(&state, kUnityVulkanGraphicsQueueAccess_DontCare), FatalRuntimeError, "Unable to obtain a command recording state from Unity. This is fatal."));
    commandBuffer = state.commandBuffer;
    return Success;
}
#    endif

#    ifdef ENABLE_DX12
Upscaler::Status FSR3::DX12Create(ffx::CreateContextDescUpscale& createContextDescUpscale) {
    ffx::CreateBackendDX12Desc createBackendDX12Desc;
    createBackendDX12Desc.device = DX12::getGraphicsInterface()->GetDevice();
    return setStatus(CreateContext(context, nullptr, createContextDescUpscale, createBackendDX12Desc), "Failed to create the AMD FidelityFx Super Resolution context.");
}

Upscaler::Status FSR3::DX12GetResources(std::array<FfxApiResource, 6>& resources, void*& commandList) {
    for (Plugin::ImageID id{Plugin::Color}; id <= (settings.autoReactive ? Plugin::Opaque : Plugin::Output); ++reinterpret_cast<uint8_t&>(id)) {
        const FfxApiResorceUsage resourceUsage{id == Plugin::Depth ? FFX_API_RESOURCE_USAGE_DEPTHTARGET : id == Plugin::Output || id == Plugin::Reactive ? FFX_API_RESOURCE_USAGE_UAV : FFX_API_RESOURCE_USAGE_READ_ONLY};
        auto& [resource, description, state] = resources.at(id);
        resource = textures.at(id);
        RETURN_ON_FAILURE(setStatusIf(resource == nullptr, RecoverableRuntimeError, "Unity provided a `nullptr` image."));
        const D3D12_RESOURCE_DESC imageDescription = static_cast<ID3D12Resource*>(resource)->GetDesc();
        description = {
            .type=FFX_API_RESOURCE_TYPE_TEXTURE2D,
            .format=ffxApiGetSurfaceFormatDX12(imageDescription.Format),
            .width=static_cast<uint32_t>(imageDescription.Width),
            .height=static_cast<uint32_t>(imageDescription.Height),
            .alignment=static_cast<uint32_t>(imageDescription.Alignment),
            .mipCount=1U,
            .flags=FFX_API_RESOURCE_FLAGS_ALIASABLE,
            .usage=static_cast<uint32_t>(resourceUsage),
          };
        state = static_cast<uint32_t>(resourceUsage == FFX_API_RESOURCE_USAGE_UAV ? FFX_API_RESOURCE_STATE_UNORDERED_ACCESS : FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    }
    UnityGraphicsD3D12RecordingState state {};
    RETURN_ON_FAILURE(Upscaler::setStatusIf(!DX12::getGraphicsInterface()->CommandRecordingState(&state), FatalRuntimeError, "Unable to obtain a command recording state from Unity. This is fatal."));
    commandList = state.commandList;
    return Success;
}
#    endif

Upscaler::Status FSR3::setStatus(const ffx::ReturnCode t_error, const std::string &t_msg) {
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

void FSR3::log(const FfxApiMsgType type, const wchar_t *t_msg) {
    std::wstring message(t_msg);
    std::string  msg(message.length(), 0);
    std::ranges::transform(message, msg.begin(), [](const wchar_t c) { return static_cast<char>(c); });
    if (logCallback != nullptr) logCallback(msg.c_str());
}

bool FSR3::isSupported() {
    if (supported != Untested) return supported == Supported;
    return (supported = success(FSR3().useSettings({32, 32}, Settings::DLSSPreset::Default, Settings::Quality::Auto, false)) ? Supported : Unsupported) == Supported;
}

bool FSR3::isSupported(const enum Settings::Quality mode) {
    return mode == Settings::AntiAliasing || mode == Settings::Auto || mode == Settings::Quality || mode == Settings::Balanced || mode == Settings::Performance || mode == Settings::UltraPerformance;
}

void FSR3::useGraphicsAPI(const GraphicsAPI::Type type) {
    switch (type) {
#    ifdef ENABLE_VULKAN
        case GraphicsAPI::VULKAN: {
            fpCreate       = &FSR3::VulkanCreate;
            fpGetResources = &FSR3::VulkanGetResources;
            break;
        }
#    endif
#    ifdef ENABLE_DX12
        case GraphicsAPI::DX12: {
            fpCreate       = &FSR3::DX12Create;
            fpGetResources = &FSR3::DX12GetResources;
            break;
        }
#    endif
        default: {
            fpCreate       = &FSR3::invalidGraphicsAPIFail;
            fpGetResources = &FSR3::invalidGraphicsAPIFail;
            break;
        }
    }
}

FSR3::FSR3() {
    if (++users != 1U) return;
    std::string path;
    if (GraphicsAPI::getType() == GraphicsAPI::DX12) path = R"(amd_fidelityfx_dx12.dll)";
    else if (GraphicsAPI::getType() == GraphicsAPI::VULKAN) path = R"(amd_fidelityfx_vk.dll)";
    else RETURN_VOID_ON_FAILURE(Upscaler::setStatus(UnsupportedGraphicsApi, getName() + " only supports Vulkan and DX12."));
    library = LoadLibrary(path.c_str());
    if (library == nullptr) library = LoadLibrary((R"(Assets\Plugins\)" + path).c_str());
    RETURN_VOID_ON_FAILURE(setStatusIf(library == nullptr, LibraryNotLoaded, "Failed to load '" + path + "'."));
    ffxCreateContext  = reinterpret_cast<PfnFfxCreateContext>(GetProcAddress(library, "ffxCreateContext"));
    ffxDestroyContext = reinterpret_cast<PfnFfxDestroyContext>(GetProcAddress(library, "ffxDestroyContext"));
    ffxConfigure      = reinterpret_cast<PfnFfxConfigure>(GetProcAddress(library, "ffxConfigure"));
    ffxQuery          = reinterpret_cast<PfnFfxQuery>(GetProcAddress(library, "ffxQuery"));
    ffxDispatch       = reinterpret_cast<PfnFfxDispatch>(GetProcAddress(library, "ffxDispatch"));
    setStatusIf(ffxCreateContext == nullptr || ffxDestroyContext == nullptr || ffxConfigure == nullptr || ffxQuery == nullptr || ffxDispatch == nullptr, LibraryNotLoaded, "'" + path + "' had missing symbols.");
}

FSR3::~FSR3() {
    if (context != nullptr) setStatus(ffx::DestroyContext(context), "Failed to destroy the AMD FidelityFx Super Resolution context.");
    context = nullptr;
    if (--users == 0 && library != nullptr) FreeLibrary(library);
    library = nullptr;
}

Upscaler::Status FSR3::useSettings(const Settings::Resolution resolution, const Settings::DLSSPreset /*unused*/, const enum Settings::Quality mode, const bool hdr) {
    RETURN_ON_FAILURE(getStatus());
    Settings optimalSettings         = settings;
    optimalSettings.outputResolution = resolution;
    optimalSettings.hdr              = hdr;

    ffx::QueryDescUpscaleGetRenderResolutionFromQualityMode queryDescUpscaleGetRenderResolutionFromQualityMode;
    queryDescUpscaleGetRenderResolutionFromQualityMode.displayWidth     = optimalSettings.outputResolution.width;
    queryDescUpscaleGetRenderResolutionFromQualityMode.displayHeight    = optimalSettings.outputResolution.height;
    queryDescUpscaleGetRenderResolutionFromQualityMode.qualityMode      = optimalSettings.getQuality<Upscaler::FSR3>(mode);
    queryDescUpscaleGetRenderResolutionFromQualityMode.pOutRenderWidth  = &optimalSettings.recommendedInputResolution.width;
    queryDescUpscaleGetRenderResolutionFromQualityMode.pOutRenderHeight = &optimalSettings.recommendedInputResolution.height;
    RETURN_ON_FAILURE(setStatus(ffx::Query(queryDescUpscaleGetRenderResolutionFromQualityMode), "Failed to query render resolution from quality mode. Ensure that the QualityMode setting is a valid enum value."));
    optimalSettings.dynamicMaximumInputResolution = resolution;

    ffx::CreateContextDescUpscale createContextDescUpscale;
    createContextDescUpscale.flags =
#    ifndef NDEBUG
        static_cast<unsigned>(FFX_UPSCALE_ENABLE_DEBUG_CHECKING) |
#    endif
        static_cast<unsigned>(FFX_UPSCALE_ENABLE_AUTO_EXPOSURE) |
        static_cast<unsigned>(FFX_UPSCALE_ENABLE_DEPTH_INVERTED) |
        static_cast<unsigned>(FFX_UPSCALE_ENABLE_DYNAMIC_RESOLUTION) |
        (optimalSettings.hdr ? FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE : 0U);
    createContextDescUpscale.maxRenderSize  = FfxApiDimensions2D {optimalSettings.outputResolution.width, optimalSettings.outputResolution.height};
    createContextDescUpscale.maxUpscaleSize = FfxApiDimensions2D {optimalSettings.outputResolution.width, optimalSettings.outputResolution.height};
#    ifndef NDEBUG
    createContextDescUpscale.fpMessage = reinterpret_cast<decltype(ffx::CreateContextDescUpscale::fpMessage)>(&FSR3::log);
#    endif

    if (context != nullptr) setStatus(ffx::DestroyContext(context), "Failed to destroy AMD FidelityFx Super Resolution context");
    context = nullptr;
    RETURN_ON_FAILURE((this->*fpCreate)(createContextDescUpscale));
    settings = optimalSettings;
    return Success;
}

Upscaler::Status FSR3::evaluate() {
    std::array<FfxApiResource, 6> resources {};
    void* commandBuffer {};
    RETURN_ON_FAILURE((this->*fpGetResources)(resources, commandBuffer));

    if (settings.autoReactive) {
        ffx::DispatchDescUpscaleGenerateReactiveMask dispatchDescUpscaleGenerateReactiveMask;
        dispatchDescUpscaleGenerateReactiveMask.commandList     = commandBuffer;
        dispatchDescUpscaleGenerateReactiveMask.colorOpaqueOnly = resources.at(Plugin::ImageID::Opaque);
        dispatchDescUpscaleGenerateReactiveMask.colorPreUpscale = resources.at(Plugin::ImageID::Color);
        dispatchDescUpscaleGenerateReactiveMask.outReactive     = resources.at(Plugin::ImageID::Reactive);
        dispatchDescUpscaleGenerateReactiveMask.renderSize      = FfxApiDimensions2D {resources.at(Plugin::ImageID::Reactive).description.width, resources.at(Plugin::ImageID::Reactive).description.height};
        dispatchDescUpscaleGenerateReactiveMask.scale           = settings.reactiveScale;
        dispatchDescUpscaleGenerateReactiveMask.cutoffThreshold = settings.reactiveThreshold;
        dispatchDescUpscaleGenerateReactiveMask.binaryValue     = settings.reactiveValue;
        dispatchDescUpscaleGenerateReactiveMask.flags           =
            static_cast<unsigned>(FFX_UPSCALE_AUTOREACTIVEFLAGS_APPLY_TONEMAP) |
            static_cast<unsigned>(FFX_UPSCALE_AUTOREACTIVEFLAGS_APPLY_THRESHOLD) |
            static_cast<unsigned>(FFX_UPSCALE_AUTOREACTIVEFLAGS_USE_COMPONENTS_MAX);
        RETURN_ON_FAILURE(setStatus(Dispatch(context, dispatchDescUpscaleGenerateReactiveMask), "Failed to dispatch AMD FidelityFx Super Resolution reactive mask generation and upscale commands."));
    }
    ffx::DispatchDescUpscale dispatchDescUpscale;
    dispatchDescUpscale.commandList                = commandBuffer;
    dispatchDescUpscale.color                      = resources.at(Plugin::ImageID::Color);
    dispatchDescUpscale.depth                      = resources.at(Plugin::ImageID::Depth);
    dispatchDescUpscale.motionVectors              = resources.at(Plugin::ImageID::Motion);
    dispatchDescUpscale.exposure                   = FfxApiResource {};
    dispatchDescUpscale.reactive                   = resources.at(Plugin::ImageID::Reactive);
    dispatchDescUpscale.transparencyAndComposition = FfxApiResource {};
    dispatchDescUpscale.output                     = resources.at(Plugin::ImageID::Output);
    dispatchDescUpscale.jitterOffset               = FfxApiFloatCoords2D {settings.jitter.x, settings.jitter.y};
    dispatchDescUpscale.motionVectorScale          = FfxApiFloatCoords2D {-static_cast<float>(resources.at(Plugin::ImageID::Motion).description.width), -static_cast<float>(resources.at(Plugin::ImageID::Motion).description.height)};
    dispatchDescUpscale.renderSize                 = FfxApiDimensions2D {resources.at(Plugin::ImageID::Color).description.width, resources.at(Plugin::ImageID::Color).description.height};
    dispatchDescUpscale.upscaleSize                = FfxApiDimensions2D {resources.at(Plugin::ImageID::Output).description.width, resources.at(Plugin::ImageID::Output).description.height};
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