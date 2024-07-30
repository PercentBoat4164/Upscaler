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

Upscaler::Status (DLSS::*DLSS::fpGetResources)(std::array<sl::Resource, 4>&, void*&);
Upscaler::Status (DLSS::*DLSS::fpSetDevice)();

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

void DLSS::log(sl::LogType type, const char* msg) {
    if (logCallback != nullptr) logCallback(msg);
}

#    ifdef ENABLE_VULKAN
Upscaler::Status DLSS::VulkanGetResources(std::array<sl::Resource, 4>& resources, void*& commandBuffer) {
    for (Plugin::ImageID id{Plugin::Color}; id <= Plugin::Output; ++reinterpret_cast<uint8_t&>(id)) {
        UnityVulkanImage image{};
        Vulkan::getGraphicsInterface()->AccessTexture(textures.at(id), UnityVulkanWholeImage, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, id == Plugin::Output ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT, kUnityVulkanResourceAccess_PipelineBarrier, &image);
        RETURN_ON_FAILURE(setStatusIf(image.image == VK_NULL_HANDLE, RecoverableRuntimeError, "Unity provided a `VK_NULL_HANDLE` image."));

        auto& resource        = resources.at(id);
        resource              = sl::Resource(sl::ResourceType::eTex2d, image.image, image.memory.memory, Vulkan::createImageView(image.image, image.format, image.aspect));
        resource.state        = image.layout;
        resource.usage        = image.usage;
        resource.width        = image.extent.width;
        resource.height       = image.extent.height;
        resource.nativeFormat = image.format;
    }
    UnityVulkanRecordingState state{};
    Vulkan::getGraphicsInterface()->EnsureInsideRenderPass();
    RETURN_ON_FAILURE(Upscaler::setStatusIf(!Vulkan::getGraphicsInterface()->CommandRecordingState(&state, kUnityVulkanGraphicsQueueAccess_DontCare), FatalRuntimeError, "Unable to obtain a command recording state from Unity. This is fatal."));
    commandBuffer = state.commandBuffer;
    return Success;
}
#    endif

#    ifdef ENABLE_DX12
Upscaler::Status DLSS::DX12GetResources(std::array<sl::Resource, 4>& resources, void*& commandList) {
    for (Plugin::ImageID id{Plugin::Color}; id <= Plugin::Output; ++reinterpret_cast<uint8_t&>(id)) {
        auto* const image = static_cast<ID3D12Resource*>(textures.at(id));
        RETURN_ON_FAILURE(setStatusIf(image == nullptr, RecoverableRuntimeError, "Unity provided a `nullptr` image."));
        const D3D12_RESOURCE_DESC desc = image->GetDesc();
        sl::Resource& resource = resources.at(id);
        resource              = sl::Resource(sl::ResourceType::eTex2d, image, id == Plugin::Output ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_RENDER_TARGET);
        resource.width        = desc.Width;
        resource.height       = desc.Height;
        resource.nativeFormat = desc.Format;
    }
    UnityGraphicsD3D12RecordingState state{};
    RETURN_ON_FAILURE(Upscaler::setStatusIf(!DX12::getGraphicsInterface()->CommandRecordingState(&state), FatalRuntimeError, "Unable to obtain a command recording state from Unity. This is fatal."));
    commandList = state.commandList;
    return Success;
}

Upscaler::Status DLSS::DX12SetDevice() {
    return setStatus(slSetD3DDevice(DX12::getGraphicsInterface()->GetDevice()), "Failed to set Streamline D3D12 device.");
}
#    endif

#    ifdef ENABLE_DX11
Upscaler::Status DLSS::DX11GetResources(std::array<sl::Resource, 4>& resources, void*& deviceContext) {
    for (Plugin::ImageID id{Plugin::Color}; id <= Plugin::Output; ++reinterpret_cast<uint8_t&>(id)) {
        auto* const image = static_cast<ID3D12Resource*>(textures.at(id));
        RETURN_ON_FAILURE(setStatusIf(image == nullptr, RecoverableRuntimeError, "Unity provided a `nullptr` image."));
        const D3D12_RESOURCE_DESC desc = image->GetDesc();
        sl::Resource& resource = resources.at(id);
        resource              = sl::Resource(sl::ResourceType::eTex2d, image);
        resource.width        = desc.Width;
        resource.height       = desc.Height;
        resource.nativeFormat = desc.Format;
    }
    DX11::getGraphicsInterface()->GetDevice()->GetImmediateContext(reinterpret_cast<ID3D11DeviceContext**>(&deviceContext));
    return Success;
}

Upscaler::Status DLSS::DX11SetDevice() {
    return setStatus(slSetD3DDevice(DX11::getGraphicsInterface()->GetDevice()), "Failed to set Streamline D3D11 device.");
}
#    endif

void DLSS::load(void*& vkGetProcAddrFunc, const GraphicsAPI::Type type) {
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
    if (type == GraphicsAPI::VULKAN) vkGetProcAddrFunc = GetProcAddress(library, "vkGetInstanceProcAddr");

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
    if (SL_FAILED(result, slInit(pref, sl::kSDKVersion))) supported = Unsupported;
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
            fpGetResources = &DLSS::VulkanGetResources;
            fpSetDevice    = &DLSS::Succeed;
            break;
        }
        case GraphicsAPI::DX12: {
            fpGetResources = &DLSS::DX12GetResources;
            fpSetDevice    = &DLSS::DX12SetDevice;
            break;
        }
        case GraphicsAPI::DX11: {
            fpGetResources = &DLSS::DX11GetResources;
            fpSetDevice    = &DLSS::DX11SetDevice;
            break;
        }
        case GraphicsAPI::NONE: {
            fpGetResources = &DLSS::invalidGraphicsAPIFail;
            fpSetDevice    = &DLSS::invalidGraphicsAPIFail;
            break;
        }
    }
}

DLSS::DLSS() {
    if (++users != 1U) return;
    RETURN_VOID_ON_FAILURE((this->*fpSetDevice)());
    RETURN_VOID_ON_FAILURE(setStatus(slSetFeatureLoaded(sl::kFeatureDLSS, true), "Failed to load the NVIDIA Deep Learning Super Sampling feature."));
    void* func{nullptr};
    RETURN_VOID_ON_FAILURE(setStatus(slGetFeatureFunction(sl::kFeatureDLSS, "slDLSSGetOptimalSettings", func), "Failed to get the 'slDLSSGetOptimalSettings' function."));
    slDLSSGetOptimalSettings = reinterpret_cast<decltype(&::slDLSSGetOptimalSettings)>(func);
    RETURN_VOID_ON_FAILURE(setStatus(slGetFeatureFunction(sl::kFeatureDLSS, "slDLSSSetOptions", func), "Failed to get the 'slDLSSSetOptions' function."));
    slDLSSSetOptions = reinterpret_cast<decltype(&::slDLSSSetOptions)>(func);
}

DLSS::~DLSS() {
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
    RETURN_ON_FAILURE(setStatus(slDLSSSetOptions(handle, options), "Failed to set NVIDIA Deep Learning Super Sampling options."));
    settings                               = optimalSettings;
    settings.recommendedInputResolution    = {slOptimalSettings.optimalRenderWidth, slOptimalSettings.optimalRenderHeight};
    settings.dynamicMinimumInputResolution = {slOptimalSettings.renderWidthMin, slOptimalSettings.renderHeightMin};
    settings.dynamicMaximumInputResolution = {slOptimalSettings.renderWidthMax, slOptimalSettings.renderHeightMax};
    return Success;
}

Upscaler::Status DLSS::evaluate() {
    std::array<sl::Resource, 4> resources;
    void* commandBuffer = nullptr;
    RETURN_ON_FAILURE((this->*fpGetResources)(resources, commandBuffer));
    const sl::Extent colorExtent{0, 0, resources[0].width, resources[0].height};
    const sl::Extent depthExtent{0, 0, resources[1].width, resources[1].height};
    const sl::Extent motionExtent{0, 0, resources[2].width, resources[2].height};
    const sl::Extent outputExtent{0, 0, resources[3].width, resources[3].height};
    const std::array tags {
        sl::ResourceTag{resources.data(), sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eValidUntilEvaluate, &colorExtent},
        sl::ResourceTag{&resources[1], sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilEvaluate, &depthExtent},
        sl::ResourceTag{&resources[2], sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilEvaluate, &motionExtent},
        sl::ResourceTag{&resources[3], sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eValidUntilEvaluate, &outputExtent},
    };
    RETURN_ON_FAILURE(setStatus(slSetTag(handle, tags.data(), tags.size(), commandBuffer), "Failed to set Streamline tags."));
    sl::FrameToken* frameToken{nullptr};
    RETURN_ON_FAILURE(setStatus(slGetNewFrameToken(frameToken, nullptr), "Failed to get new Streamline frame token."));
    sl::Constants constants{};
    constants.cameraFar            = settings.camera.farPlane;
    constants.cameraNear           = settings.camera.nearPlane;
    constants.cameraFOV            = settings.camera.verticalFOV * (3.1415926535897932384626433F / 180.0F);
    constants.mvecScale            = sl::float2{-1.0F, -1.0F};
    constants.jitterOffset         = sl::float2{settings.jitter.x, settings.jitter.y};
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