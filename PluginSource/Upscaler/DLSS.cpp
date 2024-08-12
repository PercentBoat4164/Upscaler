#ifdef ENABLE_DLSS
#    include "DLSS.hpp"
#    ifdef ENABLE_DX11
#        include "GraphicsAPI/DX11.hpp"

#        include <d3d11.h>

#        include <IUnityGraphicsD3D11.h>
#    endif
#    ifdef ENABLE_DX12
#        include "GraphicsAPI/DX12.hpp"

#        include <d3d12compatibility.h>

#        include <IUnityGraphicsD3D12.h>
#    endif
#    ifdef ENABLE_VULKAN
#        include "GraphicsAPI/Vulkan.hpp"

#        include <IUnityGraphicsVulkan.h>
#    endif

#    include <sl_helpers_vk.h>
#    include <sl_security.h>

#    include <filesystem>

HMODULE DLSS::library{nullptr};
uint32_t  DLSS::users{};
Upscaler::SupportState DLSS::supported{Untested};

uint64_t DLSS::applicationID{0xDC98EECU};

void* (*DLSS::fpGetDevice)(){&DLSS::nullFunc<void*>};
Upscaler::Status (DLSS::*DLSS::fpSetResources)(const std::array<void*, Plugin::NumImages>&){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::fpGetCommandBuffer)(void*&){&DLSS::safeFail};

decltype(&slInit) DLSS::slInit{nullptr};
decltype(&slSetD3DDevice) DLSS::slSetD3DDevice{nullptr};
decltype(&slSetFeatureLoaded) DLSS::slSetFeatureLoaded{nullptr};
decltype(&slGetFeatureFunction) DLSS::slGetFeatureFunction{nullptr};
decltype(&slDLSSGetOptimalSettings) DLSS::slDLSSGetOptimalSettings{nullptr};
decltype(&slDLSSSetOptions) DLSS::slDLSSSetOptions{nullptr};
decltype(&slSetTag) DLSS::slSetTag{nullptr};
decltype(&slGetNewFrameToken) DLSS::slGetNewFrameToken{nullptr};
decltype(&slSetConstants) DLSS::slSetConstants{nullptr};
decltype(&slEvaluateFeature) DLSS::slEvaluateFeature{nullptr};
decltype(&slFreeResources) DLSS::slFreeResources{nullptr};
decltype(&slShutdown) DLSS::slShutdown{nullptr};

Upscaler::Status DLSS::setStatus(const sl::Result t_error, const std::string& t_msg) {
    switch (t_error) {
        case sl::Result::eOk: return Upscaler::setStatus(Success, t_msg + " | eOk");
        case sl::Result::eErrorIO: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorIO");
        case sl::Result::eErrorDriverOutOfDate: return Upscaler::setStatus(DriversOutOfDate, t_msg + " | eErrorDriverOutOfDate");
        case sl::Result::eErrorOSOutOfDate: return Upscaler::setStatus(OperatingSystemNotSupported, t_msg + " | eErrorOSOutOfDate");
        case sl::Result::eErrorOSDisabledHWS: return Upscaler::setStatus(OperatingSystemNotSupported, t_msg + " | eErrorOSDisabledHWS");
        case sl::Result::eErrorDeviceNotCreated: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorDeviceNotCreated");
        case sl::Result::eErrorNoSupportedAdapterFound: return Upscaler::setStatus(DeviceNotSupported, t_msg + " | eErrorNoSupportedAdapterFound");
        case sl::Result::eErrorAdapterNotSupported: return Upscaler::setStatus(DeviceNotSupported, t_msg + " | eErrorAdapterNotSupported");
        case sl::Result::eErrorNoPlugins: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorNoPlugins");
        case sl::Result::eErrorVulkanAPI: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorVulkanAPI");
        case sl::Result::eErrorDXGIAPI: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorDXGIAPI");
        case sl::Result::eErrorD3DAPI: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorD3DAPI");
        case sl::Result::eErrorNRDAPI: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorNRDAPI");
        case sl::Result::eErrorNVAPI: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorNVAPI");
        case sl::Result::eErrorReflexAPI: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorReflexAPI");
        case sl::Result::eErrorNGXFailed: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorNGXFailed");
        case sl::Result::eErrorJSONParsing: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorJSONParsing");
        case sl::Result::eErrorMissingProxy: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorMissingProxy");
        case sl::Result::eErrorMissingResourceState: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorMissingResourceState");
        case sl::Result::eErrorInvalidIntegration: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorInvalidIntegration");
        case sl::Result::eErrorMissingInputParameter: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorMissingInputParameter");
        case sl::Result::eErrorNotInitialized: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorNotInitialized");
        case sl::Result::eErrorComputeFailed: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorComputeFailed");
        case sl::Result::eErrorInitNotCalled: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorInitNotCalled");
        case sl::Result::eErrorExceptionHandler: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorExceptionHandler");
        case sl::Result::eErrorInvalidParameter: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorInvalidParameter");
        case sl::Result::eErrorMissingConstants: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorMissingConstants");
        case sl::Result::eErrorDuplicatedConstants: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorDuplicatedConstants");
        case sl::Result::eErrorMissingOrInvalidAPI: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorMissingOrInvalidAPI");
        case sl::Result::eErrorCommonConstantsMissing: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorCommonConstantsMissing");
        case sl::Result::eErrorUnsupportedInterface: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorUnsupportedInterface");
        case sl::Result::eErrorFeatureMissing: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorFeatureMissing");
        case sl::Result::eErrorFeatureNotSupported: return Upscaler::setStatus(DeviceNotSupported, t_msg + " | eErrorFeatureNotSupported");
        case sl::Result::eErrorFeatureMissingHooks: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorFeatureMissingHooks");
        case sl::Result::eErrorFeatureFailedToLoad: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorFeatureFailedToLoad");
        case sl::Result::eErrorFeatureWrongPriority: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorFeatureWrongPriority");
        case sl::Result::eErrorFeatureMissingDependency: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorFeatureMissingDependency");
        case sl::Result::eErrorFeatureManagerInvalidState: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorFeatureManagerInvalidState");
        case sl::Result::eErrorInvalidState: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | eErrorInvalidState");
        case sl::Result::eWarnOutOfVRAM: return Upscaler::setStatus(OutOfMemory, t_msg + " | eWarnOutOfVRAM");
        default: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | Unknown");
    }
}

void DLSS::log(sl::LogType /*unused*/, const char* msg) {
    if (logCallback != nullptr) logCallback(msg);
}

#    ifdef ENABLE_VULKAN
Upscaler::Status DLSS::VulkanSetResources(const std::array<void*, Plugin::NumImages>& images) {
    for (Plugin::ImageID id{0}; id < Plugin::NumBaseImages; ++reinterpret_cast<uint8_t&>(id)) {
        UnityVulkanImage image {};
        Vulkan::getGraphicsInterface()->AccessTexture(images.at(id), UnityVulkanWholeImage, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, id == Plugin::Output ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT, kUnityVulkanResourceAccess_PipelineBarrier, &image);
        RETURN_ON_FAILURE(setStatusIf(image.image == VK_NULL_HANDLE, RecoverableRuntimeError, "Unity provided a `VK_NULL_HANDLE` image."));
        auto& resource = resources.at(id);
        Vulkan::destroyImageView(static_cast<VkImageView>(resource.view));
        resource              = sl::Resource {sl::ResourceType::eTex2d, image.image, image.memory.memory, Vulkan::createImageView(image.image, image.format, image.aspect)};
        resource.state        = image.layout;
        resource.usage        = image.usage;
        resource.width        = image.extent.width;
        resource.height       = image.extent.height;
        resource.nativeFormat = image.format;
    }
    return Success;
}

Upscaler::Status DLSS::VulkanGetCommandBuffer(void*& commandBuffer) {
    UnityVulkanRecordingState state {};
    Vulkan::getGraphicsInterface()->EnsureInsideRenderPass();
    RETURN_ON_FAILURE(Upscaler::setStatusIf(!Vulkan::getGraphicsInterface()->CommandRecordingState(&state, kUnityVulkanGraphicsQueueAccess_DontCare), FatalRuntimeError, "Unable to obtain a command recording state from Unity. This is fatal."));
    commandBuffer = state.commandBuffer;
    return Success;
}
#    endif

#    ifdef ENABLE_DX12
void* DLSS::DX12GetDevice() {
    return DX12::getGraphicsInterface()->GetDevice();
}

Upscaler::Status DLSS::DX12SetResources(const std::array<void*, Plugin::NumImages>& images) {
    for (Plugin::ImageID id{0}; id < Plugin::NumBaseImages; ++reinterpret_cast<uint8_t&>(id)) {
        auto* const image = static_cast<ID3D12Resource*>(images.at(id));
        RETURN_ON_FAILURE(setStatusIf(image == nullptr, RecoverableRuntimeError, "Unity provided a `nullptr` image."));
        const D3D12_RESOURCE_DESC desc     = image->GetDesc();
        sl::Resource&             resource = resources.at(id);
        resource                           = sl::Resource {sl::ResourceType::eTex2d, image, static_cast<uint32_t>(id == Plugin::Output ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_GENERIC_READ)};
        resource.width                     = desc.Width;
        resource.height                    = desc.Height;
        resource.nativeFormat              = desc.Format;
    }
    return Success;
}

Upscaler::Status DLSS::DX12GetCommandBuffer(void*& commandList) {
    UnityGraphicsD3D12RecordingState state {};
    RETURN_ON_FAILURE(Upscaler::setStatusIf(!DX12::getGraphicsInterface()->CommandRecordingState(&state), FatalRuntimeError, "Unable to obtain a command recording state from Unity. This is fatal."));
    commandList = state.commandList;
    return Success;
}
#    endif

#    ifdef ENABLE_DX11
void* DLSS::DX11GetDevice() {
    return DX11::getGraphicsInterface()->GetDevice();
}

Upscaler::Status DLSS::DX11SetResources(const std::array<void*, Plugin::NumImages>& images) {
    for (Plugin::ImageID id{0}; id < Plugin::NumBaseImages; ++reinterpret_cast<uint8_t&>(id)) {
        auto* const image = static_cast<ID3D11Texture2D*>(images.at(id));
        RETURN_ON_FAILURE(setStatusIf(image == nullptr, RecoverableRuntimeError, "Unity provided a `nullptr` image."));
        D3D11_TEXTURE2D_DESC desc;
        image->GetDesc(&desc);
        sl::Resource& resource = resources.at(id);
        resource               = sl::Resource {sl::ResourceType::eTex2d, image};
        resource.width         = desc.Width;
        resource.height        = desc.Height;
        resource.nativeFormat  = desc.Format;
    }
    return Success;
}

Upscaler::Status DLSS::DX11GetCommandBuffer(void*& deviceContext) {
    DX11::getGraphicsInterface()->GetDevice()->GetImmediateContext(reinterpret_cast<ID3D11DeviceContext**>(&deviceContext));
    return Success;
}
#    endif

void DLSS::load(const GraphicsAPI::Type type, const void** const vkGetProcAddrFunc) {
    std::wstring path;
    if (std::filesystem::exists("sl.interposer.dll")) path = L"sl.interposer.dll";
    else if (std::filesystem::exists("Assets\\Plugins\\sl.interposer.dll")) path = L"Assets\\Plugins\\sl.interposer.dll";
    if (!sl::security::verifyEmbeddedSignature(path.c_str())) {
        supported = Unsupported;
        return;
    }
    library = LoadLibraryW(path.c_str());
    if (library == nullptr) {
        supported = Unsupported;
        return;
    }
    slInit               = reinterpret_cast<decltype(&::slInit)>(GetProcAddress(library, "slInit"));
    slSetD3DDevice       = reinterpret_cast<decltype(&::slSetD3DDevice)>(GetProcAddress(library, "slSetD3DDevice"));
    slSetFeatureLoaded   = reinterpret_cast<decltype(&::slSetFeatureLoaded)>(GetProcAddress(library, "slSetFeatureLoaded"));
    slGetFeatureFunction = reinterpret_cast<decltype(&::slGetFeatureFunction)>(GetProcAddress(library, "slGetFeatureFunction"));
    slSetTag             = reinterpret_cast<decltype(&::slSetTag)>(GetProcAddress(library, "slSetTag"));
    slGetNewFrameToken   = reinterpret_cast<decltype(&::slGetNewFrameToken)>(GetProcAddress(library, "slGetNewFrameToken"));
    slSetConstants       = reinterpret_cast<decltype(&::slSetConstants)>(GetProcAddress(library, "slSetConstants"));
    slEvaluateFeature    = reinterpret_cast<decltype(&::slEvaluateFeature)>(GetProcAddress(library, "slEvaluateFeature"));
    slFreeResources      = reinterpret_cast<decltype(&::slFreeResources)>(GetProcAddress(library, "slFreeResources"));
    slShutdown           = reinterpret_cast<decltype(&::slShutdown)>(GetProcAddress(library, "slShutdown"));
    if (type == GraphicsAPI::VULKAN) *vkGetProcAddrFunc = reinterpret_cast<const void* const>(GetProcAddress(library, "vkGetInstanceProcAddr"));

    const std::array pathStrings {
        std::filesystem::current_path().wstring(),
        (std::filesystem::current_path()/"Assets"/"Plugins").wstring()
    };
    std::array paths {
        pathStrings[0].c_str(),
        pathStrings[1].c_str()
    };
    constexpr std::array features {
        sl::kFeatureDLSS
    };
    sl::Preferences pref {};
#    ifndef NDEBUG
    pref.logMessageCallback = &DLSS::log;
    pref.logLevel = sl::LogLevel::eDefault;
#    endif
    pref.pathsToPlugins = paths.data();
    pref.numPathsToPlugins = paths.size();
    pref.flags |= sl::PreferenceFlags::eUseManualHooking;
    pref.featuresToLoad = features.data();
    pref.numFeaturesToLoad = features.size();
    pref.applicationId = applicationID;
    switch (type) {
        case GraphicsAPI::VULKAN: pref.renderAPI = sl::RenderAPI::eVulkan; break;
        case GraphicsAPI::DX12: pref.renderAPI = sl::RenderAPI::eD3D12; break;
        case GraphicsAPI::DX11: pref.renderAPI = sl::RenderAPI::eD3D11; break;
        default: supported = Unsupported; return;
    }
    if (SL_FAILED(result, slInit(pref, sl::kSDKVersion)) || ((type == GraphicsAPI::DX12 || type == GraphicsAPI::DX11) && slSetD3DDevice(fpGetDevice()) != sl::Result::eOk))
        supported = Unsupported;
}

void DLSS::unload() {
    if (slShutdown != nullptr) slShutdown();
    slInit               = nullptr;
    slSetD3DDevice       = nullptr;
    slSetFeatureLoaded   = nullptr;
    slGetFeatureFunction = nullptr;
    slSetTag             = nullptr;
    slGetNewFrameToken   = nullptr;
    slSetConstants       = nullptr;
    slEvaluateFeature    = nullptr;
    slFreeResources      = nullptr;
    slShutdown           = nullptr;
    if (library != nullptr) FreeLibrary(library);
    library = nullptr;
}

bool DLSS::isSupported() {
    if (supported != Untested) return supported == Supported;
    return (supported = success(DLSS().useSettings({32, 32}, Settings::DLSSPreset::Default, Settings::Quality::Auto, false)) ? Supported : Unsupported) == Supported;
}

bool DLSS::isSupported(const enum Settings::Quality mode) {
    return mode == Settings::Auto || mode == Settings::AntiAliasing || mode == Settings::Quality || mode == Settings::Balanced || mode == Settings::Performance || mode == Settings::UltraPerformance;
}

void DLSS::useGraphicsAPI(const GraphicsAPI::Type type) {
    switch (type) {
        case GraphicsAPI::VULKAN: {
            fpGetDevice        = &DLSS::nullFunc<void*>;
            fpSetResources     = &DLSS::VulkanSetResources;
            fpGetCommandBuffer = &DLSS::VulkanGetCommandBuffer;
            break;
        }
        case GraphicsAPI::DX12: {
            fpGetDevice        = &DLSS::DX12GetDevice;
            fpSetResources     = &DLSS::DX12SetResources;
            fpGetCommandBuffer = &DLSS::DX12GetCommandBuffer;
            break;
        }
        case GraphicsAPI::DX11: {
            fpGetDevice        = &DLSS::DX11GetDevice;
            fpSetResources     = &DLSS::DX11SetResources;
            fpGetCommandBuffer = &DLSS::DX11GetCommandBuffer;
            break;
        }
        case GraphicsAPI::NONE: {
            fpGetDevice        = &DLSS::nullFunc<void*>;
            fpSetResources     = &DLSS::invalidGraphicsAPIFail;
            fpGetCommandBuffer = &DLSS::invalidGraphicsAPIFail;
            break;
        }
    }
}

DLSS::DLSS() : handle(users++) {
    if (users != 1U) return;
    RETURN_VOID_ON_FAILURE(setStatus(slSetFeatureLoaded(sl::kFeatureDLSS, true), "Failed to load the NVIDIA Deep Learning Super Sampling feature."));
    void* func{nullptr};
    RETURN_VOID_ON_FAILURE(setStatus(slGetFeatureFunction(sl::kFeatureDLSS, "slDLSSGetOptimalSettings", func), "Failed to get the 'slDLSSGetOptimalSettings' function."));
    slDLSSGetOptimalSettings = reinterpret_cast<decltype(&::slDLSSGetOptimalSettings)>(func);
    RETURN_VOID_ON_FAILURE(setStatus(slGetFeatureFunction(sl::kFeatureDLSS, "slDLSSSetOptions", func), "Failed to get the 'slDLSSSetOptions' function."));
    slDLSSSetOptions = reinterpret_cast<decltype(&::slDLSSSetOptions)>(func);
}

DLSS::~DLSS() {
#ifdef ENABLE_VULKAN
    if (GraphicsAPI::getType() == GraphicsAPI::VULKAN) for (const auto& resource : resources) Vulkan::destroyImageView(static_cast<VkImageView>(resource.view));
#endif
    slFreeResources(sl::kFeatureDLSS, handle);
    if (--users == 0) slSetFeatureLoaded(sl::kFeatureDLSS, false);
}

Upscaler::Status DLSS::useSettings(const Settings::Resolution resolution, const Settings::DLSSPreset preset, const enum Settings::Quality mode, const bool hdr) {
    RETURN_ON_FAILURE(getStatus());
    Settings optimalSettings         = settings;
    optimalSettings.outputResolution = resolution;
    optimalSettings.hdr              = hdr;
    sl::DLSSOptions options;
    options.mode                 = optimalSettings.getQuality<Upscaler::DLSS>(mode);
    options.outputWidth          = resolution.width;
    options.outputHeight         = resolution.height;
    options.colorBuffersHDR      = hdr ? sl::eTrue : sl::eFalse;
    options.indicatorInvertAxisY = sl::eTrue;
    options.useAutoExposure      = sl::eTrue;
    switch (preset) {
        case Settings::Stable:
            options.dlaaPreset             = sl::DLSSPreset::ePresetE;
            options.qualityPreset          = sl::DLSSPreset::ePresetE;
            options.balancedPreset         = sl::DLSSPreset::ePresetE;
            options.performancePreset      = sl::DLSSPreset::ePresetE;
            options.ultraPerformancePreset = sl::DLSSPreset::ePresetE;
            break;
        case Settings::FastPaced:
            options.dlaaPreset             = sl::DLSSPreset::ePresetC;
            options.qualityPreset          = sl::DLSSPreset::ePresetC;
            options.balancedPreset         = sl::DLSSPreset::ePresetC;
            options.performancePreset      = sl::DLSSPreset::ePresetC;
            options.ultraPerformancePreset = sl::DLSSPreset::ePresetC;
            break;
        case Settings::AnitGhosting:
            options.dlaaPreset             = sl::DLSSPreset::ePresetA;
            options.qualityPreset          = sl::DLSSPreset::ePresetA;
            options.balancedPreset         = sl::DLSSPreset::ePresetA;
            options.performancePreset      = sl::DLSSPreset::ePresetB;
            options.ultraPerformancePreset = sl::DLSSPreset::ePresetC;
            break;
        default: break;
    }
    options.useAutoExposure = sl::Boolean::eTrue;
    sl::DLSSOptimalSettings slOptimalSettings;
    RETURN_ON_FAILURE(setStatus(slDLSSGetOptimalSettings(options, slOptimalSettings), "Failed to get NVIDIA Deep Learning Super Sampling optimal settings."));
    // options.sharpness = settings.sharpness;
    RETURN_ON_FAILURE(setStatus(slDLSSSetOptions(handle, options), "Failed to set NVIDIA Deep Learning Super Sampling options."));
    settings                               = optimalSettings;
    settings.recommendedInputResolution    = Settings::Resolution{slOptimalSettings.optimalRenderWidth, slOptimalSettings.optimalRenderHeight};
    settings.dynamicMinimumInputResolution = Settings::Resolution{slOptimalSettings.renderWidthMin, slOptimalSettings.renderHeightMin};
    settings.dynamicMaximumInputResolution = Settings::Resolution{slOptimalSettings.renderWidthMax, slOptimalSettings.renderHeightMax};
    return Success;
}

Upscaler::Status DLSS::useImages(const std::array<void*, Plugin::NumImages>& images) {
    return (this->*fpSetResources)(images);
}

Upscaler::Status DLSS::evaluate() {
    void* commandBuffer {};
    RETURN_ON_FAILURE((this->*fpGetCommandBuffer)(commandBuffer));
    const sl::Extent colorExtent {0, 0, resources[Plugin::Color].width, resources[Plugin::Color].height};
    const sl::Extent depthExtent {0, 0, resources[Plugin::Depth].width, resources[Plugin::Depth].height};
    const sl::Extent motionExtent {0, 0, resources[Plugin::Motion].width, resources[Plugin::Motion].height};
    const sl::Extent outputExtent {0, 0, resources[Plugin::Output].width, resources[Plugin::Output].height};
    const std::array tags {
        sl::ResourceTag {&resources[Plugin::Color], sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eValidUntilEvaluate, &colorExtent},
        sl::ResourceTag {&resources[Plugin::Depth], sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilEvaluate, &depthExtent},
        sl::ResourceTag {&resources[Plugin::Motion], sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilEvaluate, &motionExtent},
        sl::ResourceTag {&resources[Plugin::Output], sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eValidUntilEvaluate, &outputExtent},
    };
    RETURN_ON_FAILURE(setStatus(slSetTag(handle, tags.data(), tags.size(), commandBuffer), "Failed to set Streamline tags."));
    sl::FrameToken* frameToken{nullptr};
    RETURN_ON_FAILURE(setStatus(slGetNewFrameToken(frameToken, nullptr), "Failed to get new Streamline frame token."));
    sl::Constants constants {};
    std::ranges::copy(settings.viewToClip, reinterpret_cast<float*>(constants.cameraViewToClip.row));
    std::ranges::copy(settings.clipToView, reinterpret_cast<float*>(constants.clipToCameraView.row));
    constants.clipToLensClip = sl::float4x4 {sl::float4 {1, 0, 0, 0}, sl::float4 {0, 1, 0, 0}, sl::float4 {0, 0, 1, 0}, sl::float4 {0, 0, 0, 1}};
    std::ranges::copy(settings.clipToPrevClip, reinterpret_cast<float*>(constants.clipToPrevClip.row));
    std::ranges::copy(settings.prevClipToClip, reinterpret_cast<float*>(constants.prevClipToClip.row));
    constants.cameraPinholeOffset = sl::float2 {0, 0};
    std::ranges::copy(settings.position, reinterpret_cast<float*>(&constants.cameraPos));
    std::ranges::copy(settings.up, reinterpret_cast<float*>(&constants.cameraUp));
    std::ranges::copy(settings.right, reinterpret_cast<float*>(&constants.cameraRight));
    std::ranges::copy(settings.forward, reinterpret_cast<float*>(&constants.cameraFwd));
    constants.jitterOffset         = sl::float2 {settings.jitter.x, settings.jitter.y};
    constants.mvecScale            = sl::float2 {-static_cast<float>(motionExtent.width) / static_cast<float>(colorExtent.width), -static_cast<float>(motionExtent.height) / static_cast<float>(colorExtent.height)};
    constants.cameraNear           = settings.nearPlane;
    constants.cameraFar            = settings.farPlane;
    constants.cameraFOV            = settings.verticalFOV * (3.1415926535897932384626433F / 180.0F);
    constants.cameraAspectRatio    = static_cast<float>(settings.outputResolution.width) / static_cast<float>(settings.outputResolution.height);
    constants.reset                = settings.resetHistory ? sl::eTrue : sl::eFalse;
    constants.depthInverted        = sl::eTrue;
    constants.cameraMotionIncluded = sl::eTrue;
    constants.motionVectors3D      = sl::eFalse;
    RETURN_ON_FAILURE(setStatus(slSetConstants(constants, *frameToken, handle), "Failed to set Streamline constants."));
    std::array<const sl::BaseStructure*, 1> evaluateInputs {&handle};
    return setStatus(slEvaluateFeature(sl::kFeatureDLSS, *frameToken, evaluateInputs.data(), evaluateInputs.size(), commandBuffer), "Failed to evaluate DLSS");
}
#endif