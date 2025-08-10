#ifdef ENABLE_DLSS
#    include "DLSS_Upscaler.hpp"
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

HMODULE DLSS_Upscaler::library{nullptr};
bool DLSS_Upscaler::loaded{false};

uint64_t DLSS_Upscaler::applicationID{0xDC98EECU};
uint32_t DLSS_Upscaler::users{0};

void* (*DLSS_Upscaler::fpGetDevice)(){&staticSafeFail<static_cast<void*>(nullptr)>};
Upscaler::Status (DLSS_Upscaler::*DLSS_Upscaler::fpSetResources)(const std::array<void*, 4>&){&DLSS_Upscaler::safeFail};
Upscaler::Status (*DLSS_Upscaler::fpGetCommandBuffer)(void*&){&staticSafeFail};

decltype(&slInit) DLSS_Upscaler::slInit{nullptr};
decltype(&slSetD3DDevice) DLSS_Upscaler::slSetD3DDevice{nullptr};
decltype(&slSetFeatureLoaded) DLSS_Upscaler::slSetFeatureLoaded{nullptr};
decltype(&slGetFeatureFunction) DLSS_Upscaler::slGetFeatureFunction{nullptr};
decltype(&slDLSSGetOptimalSettings) DLSS_Upscaler::slDLSSGetOptimalSettings{nullptr};
decltype(&slDLSSSetOptions) DLSS_Upscaler::slDLSSSetOptions{nullptr};
decltype(&slSetTagForFrame) DLSS_Upscaler::slSetTagForFrame{nullptr};
decltype(&slGetNewFrameToken) DLSS_Upscaler::slGetNewFrameToken{nullptr};
decltype(&slSetConstants) DLSS_Upscaler::slSetConstants{nullptr};
decltype(&slEvaluateFeature) DLSS_Upscaler::slEvaluateFeature{nullptr};
decltype(&slFreeResources) DLSS_Upscaler::slFreeResources{nullptr};
decltype(&slShutdown) DLSS_Upscaler::slShutdown{nullptr};

#    ifdef ENABLE_VULKAN
Upscaler::Status DLSS_Upscaler::VulkanSetResources(const std::array<void*, 4>& images) {
    for (Plugin::ImageID id{0}; id < images.size(); ++reinterpret_cast<uint8_t&>(id)) {
        UnityVulkanImage image {};
        Vulkan::getGraphicsInterface()->AccessTexture(images.at(id), UnityVulkanWholeImage, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, id == Plugin::Output ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT, kUnityVulkanResourceAccess_PipelineBarrier, &image);
        RETURN_STATUS_WITH_MESSAGE_IF(image.image == VK_NULL_HANDLE, RecoverableRuntimeError, "Unity provided a `VK_NULL_HANDLE` image.");
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

Upscaler::Status DLSS_Upscaler::VulkanGetCommandBuffer(void*& commandBuffer) {
    UnityVulkanRecordingState state {};
    Vulkan::getGraphicsInterface()->EnsureOutsideRenderPass();
    RETURN_STATUS_WITH_MESSAGE_IF(!Vulkan::getGraphicsInterface()->CommandRecordingState(&state, kUnityVulkanGraphicsQueueAccess_DontCare), FatalRuntimeError, "Unable to obtain a command recording state from Unity. This is fatal.");
    commandBuffer = state.commandBuffer;
    return Success;
}
#    endif

#    ifdef ENABLE_DX12
void* DLSS_Upscaler::DX12GetDevice() {
    return DX12::getGraphicsInterface()->GetDevice();
}

Upscaler::Status DLSS_Upscaler::DX12SetResources(const std::array<void*, 4>& images) {
    for (Plugin::ImageID id{0}; id < images.size(); ++reinterpret_cast<uint8_t&>(id)) {
        auto* const image = static_cast<ID3D12Resource*>(images.at(id));
        RETURN_STATUS_WITH_MESSAGE_IF(image == nullptr, RecoverableRuntimeError, "Unity provided a `nullptr` image.");
        const D3D12_RESOURCE_DESC desc     = image->GetDesc();
        sl::Resource&             resource = resources.at(id);
        resource                           = sl::Resource {sl::ResourceType::eTex2d, image, static_cast<uint32_t>(id == Plugin::Output ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_GENERIC_READ)};
        resource.width                     = desc.Width;
        resource.height                    = desc.Height;
        resource.nativeFormat              = desc.Format;
    }
    return Success;
}

Upscaler::Status DLSS_Upscaler::DX12GetCommandBuffer(void*& commandList) {
    UnityGraphicsD3D12RecordingState state {};
    RETURN_STATUS_WITH_MESSAGE_IF(!DX12::getGraphicsInterface()->CommandRecordingState(&state), FatalRuntimeError, "Unable to obtain a command recording state from Unity. This is fatal.");
    commandList = state.commandList;
    return Success;
}
#    endif

#    ifdef ENABLE_DX11
void* DLSS_Upscaler::DX11GetDevice() {
    return DX11::getGraphicsInterface()->GetDevice();
}

Upscaler::Status DLSS_Upscaler::DX11SetResources(const std::array<void*, 4>& images) {
    for (Plugin::ImageID id{0}; id < images.size(); ++reinterpret_cast<uint8_t&>(id)) {
        auto* const image = static_cast<ID3D11Texture2D*>(images.at(id));
        RETURN_STATUS_WITH_MESSAGE_IF(image == nullptr, RecoverableRuntimeError, "Unity provided a `nullptr` image.");
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

Upscaler::Status DLSS_Upscaler::DX11GetCommandBuffer(void*& deviceContext) {
    DX11::getGraphicsInterface()->GetDevice()->GetImmediateContext(reinterpret_cast<ID3D11DeviceContext**>(&deviceContext));
    return Success;
}
#    endif

Upscaler::Status DLSS_Upscaler::setStatus(const sl::Result t_error) {
    switch (t_error) {
        case sl::Result::eOk: return Success;
        case sl::Result::eErrorIO: return FatalRuntimeError;
        case sl::Result::eErrorDriverOutOfDate: return DriversOutOfDate;
        case sl::Result::eErrorOSOutOfDate:
        case sl::Result::eErrorOSDisabledHWS: return OperatingSystemNotSupported;
        case sl::Result::eErrorDeviceNotCreated: return FatalRuntimeError;
        case sl::Result::eErrorNoSupportedAdapterFound:
        case sl::Result::eErrorAdapterNotSupported: return DeviceNotSupported;
        case sl::Result::eErrorNoPlugins:
        case sl::Result::eErrorVulkanAPI:
        case sl::Result::eErrorDXGIAPI:
        case sl::Result::eErrorD3DAPI:
        case sl::Result::eErrorNRDAPI:
        case sl::Result::eErrorNVAPI:
        case sl::Result::eErrorReflexAPI:
        case sl::Result::eErrorNGXFailed:
        case sl::Result::eErrorJSONParsing:
        case sl::Result::eErrorMissingProxy:
        case sl::Result::eErrorMissingResourceState:
        case sl::Result::eErrorInvalidIntegration:
        case sl::Result::eErrorMissingInputParameter:
        case sl::Result::eErrorNotInitialized:
        case sl::Result::eErrorComputeFailed:
        case sl::Result::eErrorInitNotCalled:
        case sl::Result::eErrorExceptionHandler:
        case sl::Result::eErrorInvalidParameter:
        case sl::Result::eErrorMissingConstants:
        case sl::Result::eErrorDuplicatedConstants:
        case sl::Result::eErrorMissingOrInvalidAPI:
        case sl::Result::eErrorCommonConstantsMissing:
        case sl::Result::eErrorUnsupportedInterface:
        case sl::Result::eErrorFeatureMissing: return FatalRuntimeError;
        case sl::Result::eErrorFeatureNotSupported: return DeviceNotSupported;
        case sl::Result::eErrorFeatureMissingHooks:
        case sl::Result::eErrorFeatureFailedToLoad:
        case sl::Result::eErrorFeatureWrongPriority:
        case sl::Result::eErrorFeatureMissingDependency:
        case sl::Result::eErrorFeatureManagerInvalidState:
        case sl::Result::eErrorInvalidState: return FatalRuntimeError;
        case sl::Result::eWarnOutOfVRAM: return OutOfMemory;
        default: return FatalRuntimeError;
    }
}

void DLSS_Upscaler::log([[maybe_unused]] const sl::LogType type, const char* msg) {
    switch (type) {
        case sl::LogType::eInfo: Plugin::log(kUnityLogTypeLog, msg); break;
        case sl::LogType::eWarn: Plugin::log(kUnityLogTypeWarning, msg); break;
        case sl::LogType::eError: Plugin::log(kUnityLogTypeError, msg); break;
        case sl::LogType::eCount: break;
    }
}

sl::DLSSMode DLSS_Upscaler::getQuality(const enum Quality quality) const {
    switch (quality) {
        case Auto: {  // See page 7 of 'RTX UI Developer Guidelines.pdf'
            const uint32_t pixelCount{outputResolution.width * outputResolution.height};
            if (pixelCount <= 2560U * 1440U) return sl::DLSSMode::eMaxQuality;
            if (pixelCount <= 3840U * 2160U) return sl::DLSSMode::eMaxPerformance;
            return sl::DLSSMode::eUltraPerformance;
        }
        case AntiAliasing: return sl::DLSSMode::eDLAA;
        case Quality: return sl::DLSSMode::eMaxQuality;
        case Balanced: return sl::DLSSMode::eBalanced;
        case Performance: return sl::DLSSMode::eMaxPerformance;
        case UltraPerformance: return sl::DLSSMode::eUltraPerformance;
        default: return static_cast<sl::DLSSMode>(-1);
    }
}

bool DLSS_Upscaler::loadedCorrectly() {
    return loaded;
}

void DLSS_Upscaler::load(const GraphicsAPI::Type type, void* vkGetProcAddrFunc) {
    const std::wstring path = Plugin::path/"sl.interposer.dll";
    if (!sl::security::verifyEmbeddedSignature(path.c_str())) return (void)(loaded = false);
    library = LoadLibraryW(path.c_str());
    if (library == nullptr) return (void)(loaded = false);
    slInit               = reinterpret_cast<decltype(&::slInit)>(GetProcAddress(library, "slInit"));
    slSetD3DDevice       = reinterpret_cast<decltype(&::slSetD3DDevice)>(GetProcAddress(library, "slSetD3DDevice"));
    slSetFeatureLoaded   = reinterpret_cast<decltype(&::slSetFeatureLoaded)>(GetProcAddress(library, "slSetFeatureLoaded"));
    slGetFeatureFunction = reinterpret_cast<decltype(&::slGetFeatureFunction)>(GetProcAddress(library, "slGetFeatureFunction"));
    slSetTagForFrame     = reinterpret_cast<decltype(&::slSetTagForFrame)>(GetProcAddress(library, "slSetTagForFrame"));
    slGetNewFrameToken   = reinterpret_cast<decltype(&::slGetNewFrameToken)>(GetProcAddress(library, "slGetNewFrameToken"));
    slSetConstants       = reinterpret_cast<decltype(&::slSetConstants)>(GetProcAddress(library, "slSetConstants"));
    slEvaluateFeature    = reinterpret_cast<decltype(&::slEvaluateFeature)>(GetProcAddress(library, "slEvaluateFeature"));
    slFreeResources      = reinterpret_cast<decltype(&::slFreeResources)>(GetProcAddress(library, "slFreeResources"));
    slShutdown           = reinterpret_cast<decltype(&::slShutdown)>(GetProcAddress(library, "slShutdown"));
#ifdef ENABLE_VULKAN
    if (type == GraphicsAPI::VULKAN) *static_cast<PFN_vkGetInstanceProcAddr*>(vkGetProcAddrFunc) = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(library, "vkGetInstanceProcAddr"));
#endif
    if (slInit == nullptr || slSetD3DDevice == nullptr || slSetFeatureLoaded == nullptr || slGetFeatureFunction == nullptr || slSetTagForFrame == nullptr || slGetNewFrameToken == nullptr || slSetConstants == nullptr || slEvaluateFeature == nullptr || slFreeResources == nullptr || slShutdown == nullptr) return (void)(loaded = false);

    const std::array pathStrings { Plugin::path.wstring() };
    std::array paths { pathStrings[0].c_str() };
    constexpr std::array features { sl::kFeatureDLSS };
    sl::Preferences pref {};
    pref.logMessageCallback = &DLSS_Upscaler::log;
    pref.logLevel = sl::LogLevel::eVerbose;
    pref.pathsToPlugins = paths.data();
    pref.numPathsToPlugins = paths.size();
    pref.flags |= sl::PreferenceFlags::eUseManualHooking | sl::PreferenceFlags::eUseFrameBasedResourceTagging;
    pref.featuresToLoad = features.data();
    pref.numFeaturesToLoad = features.size();
    pref.applicationId = applicationID;
    pref.engine = sl::EngineType::eUnity;
    switch (type) {
        case GraphicsAPI::VULKAN: pref.renderAPI = sl::RenderAPI::eVulkan; break;
        case GraphicsAPI::DX12: pref.renderAPI = sl::RenderAPI::eD3D12; break;
        case GraphicsAPI::DX11: pref.renderAPI = sl::RenderAPI::eD3D11; break;
        default: loaded = false; return;
    }
    if (slInit(pref, sl::kSDKVersion) != sl::Result::eOk) return (void)(loaded = false);
    if ((type == GraphicsAPI::DX12 || type == GraphicsAPI::DX11) && slSetD3DDevice(fpGetDevice()) != sl::Result::eOk) return (void)(loaded = false);
    loaded = true;
}

void DLSS_Upscaler::shutdown() {
    if (slShutdown != nullptr) slShutdown();
}

void DLSS_Upscaler::unload() {
    if (library != nullptr) FreeLibrary(library);
    library              = nullptr;
    slInit               = nullptr;
    slSetD3DDevice       = nullptr;
    slSetFeatureLoaded   = nullptr;
    slGetFeatureFunction = nullptr;
    slSetTagForFrame     = nullptr;
    slGetNewFrameToken   = nullptr;
    slSetConstants       = nullptr;
    slEvaluateFeature    = nullptr;
    slFreeResources      = nullptr;
    slShutdown           = nullptr;
}

void DLSS_Upscaler::useGraphicsAPI(const GraphicsAPI::Type type) {
    switch (type) {
        case GraphicsAPI::VULKAN: {
            fpGetDevice        = nullptr;
            fpSetResources     = &DLSS_Upscaler::VulkanSetResources;
            fpGetCommandBuffer = &DLSS_Upscaler::VulkanGetCommandBuffer;
            break;
        }
        case GraphicsAPI::DX12: {
            fpGetDevice        = &DLSS_Upscaler::DX12GetDevice;
            fpSetResources     = &DLSS_Upscaler::DX12SetResources;
            fpGetCommandBuffer = &DLSS_Upscaler::DX12GetCommandBuffer;
            break;
        }
        case GraphicsAPI::DX11: {
            fpGetDevice        = &DLSS_Upscaler::DX11GetDevice;
            fpSetResources     = &DLSS_Upscaler::DX11SetResources;
            fpGetCommandBuffer = &DLSS_Upscaler::DX11GetCommandBuffer;
            break;
        }
        case GraphicsAPI::NONE: {
            fpGetDevice        = &staticSafeFail<static_cast<void*>(nullptr)>;
            fpSetResources     = &DLSS_Upscaler::safeFail<UnsupportedGraphicsApi>;
            fpGetCommandBuffer = &staticSafeFail<UnsupportedGraphicsApi>;
            break;
        }
    }
}

DLSS_Upscaler::DLSS_Upscaler() : handle(users++), viewToClip(), clipToView(), clipToPrevClip(), prevClipToClip(), position(), up(), right(), forward(), farPlane(0), nearPlane(0), verticalFOV(0) {
    if (users != 1U) return;
    RETURN_VOID_WITH_MESSAGE_IF(setStatus(slSetFeatureLoaded(sl::kFeatureDLSS, true)), "Failed to load the NVIDIA Deep Learning Super Sampling feature.");
    void* func{nullptr};
    RETURN_VOID_WITH_MESSAGE_IF(setStatus(slGetFeatureFunction(sl::kFeatureDLSS, "slDLSSGetOptimalSettings", func)), "Failed to get the 'slDLSSGetOptimalSettings' function.");
    slDLSSGetOptimalSettings = reinterpret_cast<decltype(&::slDLSSGetOptimalSettings)>(func);
    RETURN_VOID_WITH_MESSAGE_IF(setStatus(slGetFeatureFunction(sl::kFeatureDLSS, "slDLSSSetOptions", func)), "Failed to get the 'slDLSSSetOptions' function.");
    slDLSSSetOptions = reinterpret_cast<decltype(&::slDLSSSetOptions)>(func);
}

DLSS_Upscaler::~DLSS_Upscaler() {
#ifdef ENABLE_VULKAN
    if (GraphicsAPI::getType() == GraphicsAPI::VULKAN) for (const auto& resource : resources) Vulkan::destroyImageView(static_cast<VkImageView>(resource.view));
#endif
    slFreeResources(sl::kFeatureDLSS, handle);
    if (--users == 0) slSetFeatureLoaded(sl::kFeatureDLSS, false);
}

Upscaler::Status DLSS_Upscaler::useSettings(const Resolution resolution, const Preset preset, const enum Quality mode, const Flags flags) {
    outputResolution = resolution;
    sl::DLSSOptions options;
    options.mode                 = getQuality(mode);
    options.outputWidth          = outputResolution.width;
    options.outputHeight         = outputResolution.height;
    options.colorBuffersHDR      = (flags & EnableHDR) == EnableHDR ? sl::eTrue : sl::eFalse;
    options.indicatorInvertAxisY = sl::eTrue;
    options.useAutoExposure      = sl::eTrue;
    switch (preset) {
        case Stable:
            options.dlaaPreset             = sl::DLSSPreset::ePresetE;
            options.qualityPreset          = sl::DLSSPreset::ePresetE;
            options.balancedPreset         = sl::DLSSPreset::ePresetE;
            options.performancePreset      = sl::DLSSPreset::ePresetE;
            options.ultraPerformancePreset = sl::DLSSPreset::ePresetE;
            break;
        case FastPaced:
            options.dlaaPreset             = sl::DLSSPreset::ePresetC;
            options.qualityPreset          = sl::DLSSPreset::ePresetC;
            options.balancedPreset         = sl::DLSSPreset::ePresetC;
            options.performancePreset      = sl::DLSSPreset::ePresetC;
            options.ultraPerformancePreset = sl::DLSSPreset::ePresetC;
            break;
        case AnitGhosting:
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
    RETURN_WITH_MESSAGE_IF(setStatus(slDLSSGetOptimalSettings(options, slOptimalSettings)), "Failed to get NVIDIA Deep Learning Super Sampling optimal settings.");
    RETURN_WITH_MESSAGE_IF(setStatus(slDLSSSetOptions(handle, options)), "Failed to set NVIDIA Deep Learning Super Sampling options.");
    recommendedInputResolution    = Resolution{slOptimalSettings.optimalRenderWidth, slOptimalSettings.optimalRenderHeight};
    dynamicMinimumInputResolution = Resolution{slOptimalSettings.renderWidthMin, slOptimalSettings.renderHeightMin};
    dynamicMaximumInputResolution = Resolution{slOptimalSettings.renderWidthMax, slOptimalSettings.renderHeightMax};
    return Success;
}

Upscaler::Status DLSS_Upscaler::useImages(const std::array<void*, 4>& images) {
    return (this->*fpSetResources)(images);
}

Upscaler::Status DLSS_Upscaler::evaluate(const Resolution inputResolution) {
    void* commandBuffer {};
    RETURN_IF(fpGetCommandBuffer(commandBuffer));
    const sl::Extent colorExtent {0, 0, inputResolution.width, inputResolution.height};
    const sl::Extent depthExtent {0, 0, inputResolution.width, inputResolution.height};
    const sl::Extent motionExtent {0, 0, resources.at(Plugin::Motion).width, resources.at(Plugin::Motion).height};
    const sl::Extent outputExtent {0, 0, resources.at(Plugin::Output).width, resources.at(Plugin::Output).height};
    const std::array tags {
        sl::ResourceTag {&resources.at(Plugin::Color), sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eValidUntilEvaluate, &colorExtent},
        sl::ResourceTag {&resources.at(Plugin::Depth), sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilEvaluate, &depthExtent},
        sl::ResourceTag {&resources.at(Plugin::Motion), sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilEvaluate, &motionExtent},
        sl::ResourceTag {&resources.at(Plugin::Output), sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eValidUntilEvaluate, &outputExtent},
    };
    sl::FrameToken* frameToken{nullptr};
    RETURN_WITH_MESSAGE_IF(setStatus(slGetNewFrameToken(frameToken, nullptr)), "Failed to get new Streamline frame token.");
    RETURN_WITH_MESSAGE_IF(setStatus(slSetTagForFrame(*frameToken, handle, tags.data(), tags.size(), commandBuffer)), "Failed to set Streamline tags.");
    sl::Constants constants {};
    std::ranges::copy(viewToClip, reinterpret_cast<float*>(constants.cameraViewToClip.row));
    std::ranges::copy(clipToView, reinterpret_cast<float*>(constants.clipToCameraView.row));
    constants.clipToLensClip = sl::float4x4 {sl::float4 {1, 0, 0, 0}, sl::float4 {0, 1, 0, 0}, sl::float4 {0, 0, 1, 0}, sl::float4 {0, 0, 0, 1}};
    std::ranges::copy(clipToPrevClip, reinterpret_cast<float*>(constants.clipToPrevClip.row));
    std::ranges::copy(prevClipToClip, reinterpret_cast<float*>(constants.prevClipToClip.row));
    constants.cameraPinholeOffset = sl::float2 {0, 0};
    std::ranges::copy(position, reinterpret_cast<float*>(&constants.cameraPos));
    std::ranges::copy(up, reinterpret_cast<float*>(&constants.cameraUp));
    std::ranges::copy(right, reinterpret_cast<float*>(&constants.cameraRight));
    std::ranges::copy(forward, reinterpret_cast<float*>(&constants.cameraFwd));
    constants.jitterOffset         = sl::float2 {jitter.x, jitter.y};
    constants.mvecScale            = sl::float2 {-static_cast<float>(motionExtent.width) / static_cast<float>(colorExtent.width), -static_cast<float>(motionExtent.height) / static_cast<float>(colorExtent.height)};
    constants.cameraNear           = nearPlane;
    constants.cameraFar            = farPlane;
    constants.cameraFOV            = verticalFOV * (3.1415926535897932384626433F / 180.0F);
    constants.cameraAspectRatio    = static_cast<float>(outputResolution.width) / static_cast<float>(outputResolution.height);
    constants.reset                = resetHistory ? sl::eTrue : sl::eFalse;
    constants.depthInverted        = sl::eTrue;
    constants.cameraMotionIncluded = sl::eTrue;
    constants.motionVectors3D      = sl::eFalse;
    RETURN_WITH_MESSAGE_IF(setStatus(slSetConstants(constants, *frameToken, handle)), "Failed to set Streamline constants.");
    std::array<const sl::BaseStructure*, 1> evaluateInputs {&handle};
    RETURN_WITH_MESSAGE_IF(setStatus(slEvaluateFeature(sl::kFeatureDLSS, *frameToken, evaluateInputs.data(), evaluateInputs.size(), commandBuffer)), "Failed to evaluate DLSS");
    return Success;
}
#endif