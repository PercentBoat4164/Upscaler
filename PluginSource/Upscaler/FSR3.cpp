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
std::atomic<uint32_t> FSR3::users{};

Upscaler::Status (FSR3::*FSR3::fpCreate)(ffx::CreateContextDescUpscale&){&FSR3::safeFail};
Upscaler::Status (FSR3::*FSR3::fpEvaluate)(){&FSR3::safeFail};

Upscaler::SupportState FSR3::supported{Untested};

#    ifdef ENABLE_VULKAN
Upscaler::Status FSR3::VulkanCreate(ffx::CreateContextDescUpscale& createContextDescUpscale) {
    ffx::CreateBackendVKDesc createBackendVKDesc;
    createBackendVKDesc.vkDevice = Vulkan::getGraphicsInterface()->Instance().device;
    createBackendVKDesc.vkPhysicalDevice = Vulkan::getGraphicsInterface()->Instance().physicalDevice;
    createBackendVKDesc.vkDeviceProcAddr = Vulkan::getDeviceProcAddr();

    return setStatus(CreateContext(context, nullptr, createContextDescUpscale, createBackendVKDesc), "Failed to create the " + getName() + " context.");
}

Upscaler::Status FSR3::VulkanGetResource(FfxApiResource& resource, const Plugin::ImageID imageID) {
    VkAccessFlags      accessFlags{VK_ACCESS_SHADER_READ_BIT};
    FfxApiResorceUsage resourceUsage{FFX_API_RESOURCE_USAGE_READ_ONLY};
    if (imageID == Plugin::ImageID::Depth) resourceUsage = FFX_API_RESOURCE_USAGE_DEPTHTARGET;
    if (imageID == Plugin::ImageID::Output || imageID == Plugin::ImageID::Reactive) {
        accessFlags = VK_ACCESS_SHADER_WRITE_BIT;
        resourceUsage = FFX_API_RESOURCE_USAGE_UAV;
    }

    UnityVulkanImage image{};
    Vulkan::getGraphicsInterface()->AccessTexture(textures.at(imageID), UnityVulkanWholeImage, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, accessFlags, kUnityVulkanResourceAccess_PipelineBarrier, &image);
    RETURN_ON_FAILURE(setStatusIf(image.image == VK_NULL_HANDLE, RecoverableRuntimeError, "Unity provided a `VK_NULL_HANDLE` image."));

    const FfxApiResourceDescription description {
      .type=FFX_API_RESOURCE_TYPE_TEXTURE2D,
      .format=ffxApiGetSurfaceFormatVK(image.format),
      .width=image.extent.width,
      .height=image.extent.height,
      .depth=image.extent.depth,
      .mipCount=1U,
      .flags=FFX_API_RESOURCE_FLAGS_ALIASABLE,
      .usage=static_cast<uint32_t>(resourceUsage),
    };
    resource = {image.image, description, static_cast<uint32_t>(resourceUsage == FFX_API_RESOURCE_USAGE_UAV ? FFX_API_RESOURCE_STATE_UNORDERED_ACCESS : FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ)};
    return Success;
}

Upscaler::Status FSR3::VulkanEvaluate() {
    FfxApiResource color{}, depth{}, motion{}, output{}, reactiveMask{}, opaqueColor{};
    RETURN_ON_FAILURE(VulkanGetResource(color, Plugin::ImageID::Color));
    RETURN_ON_FAILURE(VulkanGetResource(depth, Plugin::ImageID::Depth));
    RETURN_ON_FAILURE(VulkanGetResource(motion, Plugin::ImageID::Motion));
    RETURN_ON_FAILURE(VulkanGetResource(output, Plugin::ImageID::Output));

    UnityVulkanRecordingState state{};
    Vulkan::getGraphicsInterface()->EnsureInsideRenderPass();
    RETURN_ON_FAILURE(Upscaler::setStatusIf(!Vulkan::getGraphicsInterface()->CommandRecordingState(&state, kUnityVulkanGraphicsQueueAccess_DontCare), FatalRuntimeError, "Unable to obtain a command recording state from Unity. This is fatal."));

    if (settings.autoReactive) {
        RETURN_ON_FAILURE(VulkanGetResource(reactiveMask, Plugin::ImageID::Reactive));
        RETURN_ON_FAILURE(VulkanGetResource(opaqueColor, Plugin::ImageID::Opaque));
        ffx::DispatchDescUpscaleGenerateReactiveMask dispatchDescUpscaleGenerateReactiveMask;
        dispatchDescUpscaleGenerateReactiveMask.commandList     = state.commandBuffer;
        dispatchDescUpscaleGenerateReactiveMask.colorOpaqueOnly = opaqueColor;
        dispatchDescUpscaleGenerateReactiveMask.colorPreUpscale = color;
        dispatchDescUpscaleGenerateReactiveMask.outReactive     = reactiveMask;
        dispatchDescUpscaleGenerateReactiveMask.renderSize      = {reactiveMask.description.width, reactiveMask.description.height};
        dispatchDescUpscaleGenerateReactiveMask.scale           = settings.reactiveScale;
        dispatchDescUpscaleGenerateReactiveMask.cutoffThreshold = settings.reactiveThreshold;
        dispatchDescUpscaleGenerateReactiveMask.binaryValue     = settings.reactiveValue;
        dispatchDescUpscaleGenerateReactiveMask.flags           =
            static_cast<unsigned>(FFX_UPSCALE_AUTOREACTIVEFLAGS_APPLY_TONEMAP) |
            static_cast<unsigned>(FFX_UPSCALE_AUTOREACTIVEFLAGS_APPLY_THRESHOLD) |
            static_cast<unsigned>(FFX_UPSCALE_AUTOREACTIVEFLAGS_USE_COMPONENTS_MAX);

        RETURN_ON_FAILURE(setStatus(Dispatch(context, dispatchDescUpscaleGenerateReactiveMask), "Failed to dispatch reactive mask generation commands."));
    }

    ffx::DispatchDescUpscale dispatchDescUpscale;
    dispatchDescUpscale.commandList                = state.commandBuffer;
    dispatchDescUpscale.color                      = color;
    dispatchDescUpscale.depth                      = depth;
    dispatchDescUpscale.motionVectors              = motion;
    dispatchDescUpscale.exposure                   = {};
    dispatchDescUpscale.reactive                   = reactiveMask;
    dispatchDescUpscale.transparencyAndComposition = {};
    dispatchDescUpscale.output                     = output;
    dispatchDescUpscale.jitterOffset               = {settings.jitter.x, settings.jitter.y};
    dispatchDescUpscale.motionVectorScale          = {-static_cast<float>(motion.description.width), -static_cast<float>(motion.description.height)};
    dispatchDescUpscale.renderSize                 = {color.description.width, color.description.height};
    dispatchDescUpscale.upscaleSize                = {output.description.width, output.description.height};
    dispatchDescUpscale.enableSharpening           = settings.sharpness > 0.0F;
    dispatchDescUpscale.sharpness                  = settings.sharpness;
    dispatchDescUpscale.frameTimeDelta             = settings.frameTime;
    dispatchDescUpscale.preExposure                = 1.0F;
    dispatchDescUpscale.reset                      = settings.resetHistory;
    dispatchDescUpscale.cameraNear                 = settings.camera.farPlane;
    dispatchDescUpscale.cameraFar                  = settings.camera.nearPlane;  // Switched because depth is inverted
    dispatchDescUpscale.cameraFovAngleVertical     = settings.camera.verticalFOV * (3.1415926535897932384626433F / 180.0F);
    dispatchDescUpscale.viewSpaceToMetersFactor    = 1.0F;

    return setStatus(Dispatch(context, dispatchDescUpscale), "Failed to dispatch " + getName() + " commands.");
}
#    endif

#    ifdef ENABLE_DX12
Upscaler::Status FSR3::DX12Create(ffx::CreateContextDescUpscale& createContextDescUpscale) {
    ffx::CreateBackendDX12Desc createBackendDX12Desc;
    createBackendDX12Desc.device = DX12::getGraphicsInterface()->GetDevice();

    return setStatus(CreateContext(context, nullptr, createContextDescUpscale, createBackendDX12Desc), "Failed to create the " + getName() + " context.");
}

Upscaler::Status FSR3::DX12GetResource(FfxApiResource& resource, const Plugin::ImageID imageID) {
    FfxApiResorceUsage resourceUsage{FFX_API_RESOURCE_USAGE_READ_ONLY};
    if (imageID == Plugin::ImageID::Depth) resourceUsage = FFX_API_RESOURCE_USAGE_DEPTHTARGET;
    if (imageID == Plugin::ImageID::Output || imageID == Plugin::ImageID::Reactive) resourceUsage = FFX_API_RESOURCE_USAGE_UAV;

    auto* image = static_cast<ID3D12Resource*>(textures.at(imageID));
    RETURN_ON_FAILURE(setStatusIf(image == nullptr, RecoverableRuntimeError, "Unity provided a `nullptr` image."));
    const D3D12_RESOURCE_DESC imageDescription = image->GetDesc();

    const FfxApiResourceDescription description {
      .type=FFX_API_RESOURCE_TYPE_TEXTURE2D,
      .format=ffxApiGetSurfaceFormatDX12(imageDescription.Format),
      .width=static_cast<uint32_t>(imageDescription.Width),
      .height=static_cast<uint32_t>(imageDescription.Height),
      .alignment=static_cast<uint32_t>(imageDescription.Alignment),
      .mipCount=1U,
      .flags=FFX_API_RESOURCE_FLAGS_ALIASABLE,
      .usage=static_cast<uint32_t>(resourceUsage),
    };
    resource = {image, description, static_cast<uint32_t>(resourceUsage == FFX_API_RESOURCE_USAGE_UAV ? FFX_API_RESOURCE_STATE_UNORDERED_ACCESS : FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ)};
    return Success;
}

Upscaler::Status FSR3::DX12Evaluate() {
    FfxApiResource color{}, depth{}, motion{}, output{}, reactiveMask{}, opaqueColor{};
    RETURN_ON_FAILURE(DX12GetResource(color, Plugin::ImageID::Color));
    RETURN_ON_FAILURE(DX12GetResource(depth, Plugin::ImageID::Depth));
    RETURN_ON_FAILURE(DX12GetResource(motion, Plugin::ImageID::Motion));
    RETURN_ON_FAILURE(DX12GetResource(output, Plugin::ImageID::Output));

    UnityGraphicsD3D12RecordingState state{};
    RETURN_ON_FAILURE(Upscaler::setStatusIf(!DX12::getGraphicsInterface()->CommandRecordingState(&state), FatalRuntimeError, "Unable to obtain a command recording state from Unity. This is fatal."));

    if (settings.autoReactive) {
        RETURN_ON_FAILURE(DX12GetResource(reactiveMask, Plugin::ImageID::Reactive));
        RETURN_ON_FAILURE(DX12GetResource(opaqueColor, Plugin::ImageID::Opaque));
        ffx::DispatchDescUpscaleGenerateReactiveMask dispatchDescUpscaleGenerateReactiveMask;
        dispatchDescUpscaleGenerateReactiveMask.commandList     = state.commandList;
        dispatchDescUpscaleGenerateReactiveMask.colorOpaqueOnly = opaqueColor;
        dispatchDescUpscaleGenerateReactiveMask.colorPreUpscale = color;
        dispatchDescUpscaleGenerateReactiveMask.outReactive     = reactiveMask;
        dispatchDescUpscaleGenerateReactiveMask.renderSize      = {reactiveMask.description.width, reactiveMask.description.height};
        dispatchDescUpscaleGenerateReactiveMask.scale           = settings.reactiveScale;
        dispatchDescUpscaleGenerateReactiveMask.cutoffThreshold = settings.reactiveThreshold;
        dispatchDescUpscaleGenerateReactiveMask.binaryValue     = settings.reactiveValue;
        dispatchDescUpscaleGenerateReactiveMask.flags           =
            static_cast<unsigned>(FFX_UPSCALE_AUTOREACTIVEFLAGS_APPLY_TONEMAP) |
            static_cast<unsigned>(FFX_UPSCALE_AUTOREACTIVEFLAGS_APPLY_THRESHOLD) |
            static_cast<unsigned>(FFX_UPSCALE_AUTOREACTIVEFLAGS_USE_COMPONENTS_MAX);

        RETURN_ON_FAILURE(setStatus(Dispatch(context, dispatchDescUpscaleGenerateReactiveMask), "Failed to dispatch reactive mask generation commands."));
    }

    ffx::DispatchDescUpscale dispatchDescUpscale;
    dispatchDescUpscale.commandList                = state.commandList;
    dispatchDescUpscale.color                      = color;
    dispatchDescUpscale.depth                      = depth;
    dispatchDescUpscale.motionVectors              = motion;
    dispatchDescUpscale.exposure                   = {};
    dispatchDescUpscale.reactive                   = reactiveMask;
    dispatchDescUpscale.transparencyAndComposition = {};
    dispatchDescUpscale.output                     = output;
    dispatchDescUpscale.jitterOffset               = {settings.jitter.x, settings.jitter.y};
    dispatchDescUpscale.motionVectorScale          = {-static_cast<float>(motion.description.width), -static_cast<float>(motion.description.height)};
    dispatchDescUpscale.renderSize                 = {color.description.width, color.description.height};
    dispatchDescUpscale.upscaleSize                = {output.description.width, output.description.height};
    dispatchDescUpscale.enableSharpening           = settings.sharpness > 0.0F;
    dispatchDescUpscale.sharpness                  = settings.sharpness;
    dispatchDescUpscale.frameTimeDelta             = settings.frameTime;
    dispatchDescUpscale.preExposure                = 1.0F;
    dispatchDescUpscale.reset                      = settings.resetHistory;
    dispatchDescUpscale.cameraNear                 = settings.camera.farPlane;
    dispatchDescUpscale.cameraFar                  = settings.camera.nearPlane;  // Switched because depth is inverted
    dispatchDescUpscale.cameraFovAngleVertical     = settings.camera.verticalFOV * (3.1415926535897932384626433F / 180.0F);
    dispatchDescUpscale.viewSpaceToMetersFactor    = 1.0F;

    return setStatus(Dispatch(context, dispatchDescUpscale), "Failed to dispatch " + getName() + " commands.");
}
#    endif

Upscaler::Status FSR3::setStatus(const ffx::ReturnCode t_error, const std::string &t_msg) {
    switch (t_error) {
        case ffx::ReturnCode::Ok: return Upscaler::setStatus(Success, t_msg + " | Ok");
        case ffx::ReturnCode::Error: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | Error");
        case ffx::ReturnCode::ErrorUnknownDesctype: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | ErrorUnknownDesctype");
        case ffx::ReturnCode::ErrorRuntimeError: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | ErrorRuntimeError");
        case ffx::ReturnCode::ErrorNoProvider: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | ErrorNoProvider");
        case ffx::ReturnCode::ErrorMemory: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | ErrorMemory");
        case ffx::ReturnCode::ErrorParameter: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | ErrorParameter");
        default: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | Unknown");
    }
}

void FSR3::log(const FfxApiMsgType type, const wchar_t *t_msg) {
    std::wstring message(t_msg);
    std::string  msg(message.length(), 0);
    std::ranges::transform(message, msg.begin(), [](const wchar_t c) { return static_cast<char>(c); });
    switch (type) {
        case FFX_API_MESSAGE_TYPE_ERROR: msg = "FSR3 Error ---> " + msg; break;
        case FFX_API_MESSAGE_TYPE_WARNING: msg = "FSR3 Warning -> " + msg; break;
        case FFX_API_MESSAGE_TYPE_COUNT: break;
    }
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
            fpCreate   = &FSR3::VulkanCreate;
            fpEvaluate = &FSR3::VulkanEvaluate;
            break;
        }
#    endif
#    ifdef ENABLE_DX12
        case GraphicsAPI::DX12: {
            fpCreate   = &FSR3::DX12Create;
            fpEvaluate = &FSR3::DX12Evaluate;
            break;
        }
#    endif
        default: {
            fpCreate   = &FSR3::invalidGraphicsAPIFail;
            fpEvaluate = &FSR3::invalidGraphicsAPIFail;
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
    if (context != nullptr) setStatus(ffx::DestroyContext(context), "Failed to destroy the " + getName() + " context.");
    context = nullptr;
    if (--users == 0 && library != nullptr) FreeLibrary(library);
    library = nullptr;
}

Upscaler::Status FSR3::useSettings(const Settings::Resolution resolution, const Settings::DLSSPreset /*unused*/, const enum Settings::Quality mode, const bool hdr) {
    RETURN_ON_FAILURE(getStatus());
    Settings optimalSettings         = settings;
    optimalSettings.outputResolution = resolution;
    optimalSettings.hdr              = hdr;
    optimalSettings.quality          = mode;

    ffx::QueryDescUpscaleGetRenderResolutionFromQualityMode queryDescUpscaleGetRenderResolutionFromQualityMode;
    queryDescUpscaleGetRenderResolutionFromQualityMode.displayWidth     = optimalSettings.outputResolution.width;
    queryDescUpscaleGetRenderResolutionFromQualityMode.displayHeight    = optimalSettings.outputResolution.height;
    queryDescUpscaleGetRenderResolutionFromQualityMode.qualityMode      = optimalSettings.getQuality<Upscaler::FSR3>();
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
    createContextDescUpscale.maxRenderSize = {optimalSettings.outputResolution.width, optimalSettings.outputResolution.height};
    createContextDescUpscale.maxUpscaleSize = {optimalSettings.outputResolution.width, optimalSettings.outputResolution.height};
#    ifndef NDEBUG
    createContextDescUpscale.fpMessage = reinterpret_cast<decltype(ffx::CreateContextDescUpscale::fpMessage)>(&FSR3::log);
#    endif

    if (context != nullptr) setStatus(ffx::DestroyContext(context), "Failed to destroy " + getName() + " context");
    context = nullptr;
    RETURN_ON_FAILURE((this->*fpCreate)(createContextDescUpscale));
    settings = optimalSettings;
    return Success;
}

Upscaler::Status FSR3::evaluate() {
    RETURN_ON_FAILURE((this->*fpEvaluate)());
    settings.resetHistory = false;
    return Success;
}
#endif