#ifdef ENABLE_FRAME_GENERATION
#    include "FrameGenerator/FrameGenerator.hpp"
#    ifdef ENABLE_FSR
#        include "FrameGenerator/FSR_FrameGenerator.hpp"
#    endif
#endif
#ifdef ENABLE_VULKAN
    #include "GraphicsAPI/Vulkan.hpp"
#endif
#include "Plugin.hpp"
#include "Upscaler/DLSS_Upscaler.hpp"
#include "Upscaler/Upscaler.hpp"
#include "Upscaler/XeSS_UPscaler.hpp"

#include "IUnityRenderingExtensions.h"
#include "IUnityShaderCompilerAccess.h"

#include <vector>

// Use 'handle SIGXCPU SIGPWR SIG35 SIG36 SIG37 nostop noprint' to prevent Unity's signals with GDB on Linux.
// Use 'pro hand -p true -s false SIGXCPU SIGPWR' for LLDB on Linux.

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API LoadedCorrectlyPlugin() { return Plugin::loadedCorrectly; }

#pragma region Deep Learning Super Sampling
struct DeepLearningSuperSamplingUpscaleData
{
    DLSS_Upscaler* handle;
    std::array<float, 16> viewToClip;
    std::array<float, 16> clipToView;
    std::array<float, 16> clipToPrevClip;
    std::array<float, 16> prevClipToClip;
    std::array<float, 3> position;
    std::array<float, 3> up;
    std::array<float, 3> right;
    std::array<float, 3> forward;
    float farPlane;
    float nearPlane;
    float verticalFOV;
    Upscaler::Jitter jitter;
    Upscaler::Resolution inputResolution;
    bool resetHistory;
};

void UNITY_INTERFACE_API UpscaleCallbackDeepLearningSuperSampling(const int /*unused*/, void* d) {
    const auto&    data = *static_cast<DeepLearningSuperSamplingUpscaleData*>(d);
    DLSS_Upscaler& dlss = *data.handle;
    dlss.viewToClip     = data.viewToClip;
    dlss.clipToView     = data.clipToView;
    dlss.clipToPrevClip = data.clipToPrevClip;
    dlss.prevClipToClip = data.prevClipToClip;
    dlss.position       = data.position;
    dlss.up             = data.up;
    dlss.right          = data.right;
    dlss.forward        = data.forward;
    dlss.farPlane       = data.farPlane;
    dlss.nearPlane      = data.nearPlane;
    dlss.verticalFOV    = data.verticalFOV;
    dlss.resetHistory   = data.resetHistory;
    dlss.jitter         = data.jitter;
    dlss.evaluate(data.inputResolution);
}

extern "C" UNITY_INTERFACE_EXPORT UnityRenderingEventAndData UNITY_INTERFACE_API GetUpscaleCallbackDeepLearningSuperSampling() { return UpscaleCallbackDeepLearningSuperSampling; }
extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API LoadedCorrectlyDeepLearningSuperSampling() { return DLSS_Upscaler::loadedCorrectly(); }
extern "C" UNITY_INTERFACE_EXPORT DLSS_Upscaler* UNITY_INTERFACE_API CreateContextDeepLearningSuperSampling() { return new DLSS_Upscaler; }
extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API UpdateContextDeepLearningSuperSampling(DLSS_Upscaler* upscaler, const Upscaler::Resolution resolution, const Upscaler::Preset preset, const enum Upscaler::Quality mode, const Upscaler::Flags flags) { return upscaler->useSettings(resolution, preset, mode, flags); }
extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API SetImagesDeepLearningSuperSampling(DLSS_Upscaler* upscaler, void* color, void* depth, void* motion, void* output) { return upscaler->useImages({color, depth, motion, output}); }
#pragma endregion
#pragma region FidelityFX Super Resolution
struct FidelityFXSuperResolutionUpscaleData
{
    FSR_Upscaler* handle;
    float frameTime;
    float sharpness;
    float reactiveValue;
    float reactiveScale;
    float reactiveThreshold;
    float farPlane;
    float nearPlane;
    float verticalFOV;
    Upscaler::Jitter jitter;
    Upscaler::Resolution inputResolution;
    unsigned options;
};

void UNITY_INTERFACE_API UpscaleCallbackFidelityFXSuperResolution(const int /*unused*/, void* d) {
    const auto&   data    = *static_cast<FidelityFXSuperResolutionUpscaleData*>(d);
    FSR_Upscaler& fsr     = *data.handle;
    fsr.farPlane          = data.farPlane;
    fsr.nearPlane         = data.nearPlane;
    fsr.verticalFOV       = data.verticalFOV;
    fsr.frameTime         = data.frameTime;
    fsr.sharpness         = data.sharpness;
    fsr.reactiveValue     = data.reactiveValue;
    fsr.reactiveScale     = data.reactiveScale;
    fsr.reactiveThreshold = data.reactiveThreshold;
    fsr.debugView         = (data.options & 0x1U) != 0U;
    fsr.resetHistory      = (data.options & 0x2U) != 0U;
    fsr.jitter            = data.jitter;
    fsr.evaluate(data.inputResolution);
}

extern "C" UNITY_INTERFACE_EXPORT UnityRenderingEventAndData UNITY_INTERFACE_API GetUpscaleCallbackFidelityFXSuperResolution() { return UpscaleCallbackFidelityFXSuperResolution; }
extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API LoadedCorrectlyFidelityFXSuperResolution() { return FSR_Upscaler::loadedCorrectly(); }
extern "C" UNITY_INTERFACE_EXPORT FSR_Upscaler* UNITY_INTERFACE_API CreateContextFidelityFXSuperResolution() { return new FSR_Upscaler; }
extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API UpdateContextFidelityFXSuperResolution(FSR_Upscaler* upscaler, const Upscaler::Resolution resolution, const enum Upscaler::Quality mode, const Upscaler::Flags flags) { return upscaler->useSettings(resolution, mode, flags); }
extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API SetImagesFidelityFXSuperResolution(FSR_Upscaler* upscaler, void* color, void* depth, void* motion, void* output, void* reactive, void* opaque, const bool autoReactive) {
    upscaler->autoReactive = autoReactive;
    return upscaler->useImages({color, depth, motion, output, reactive, opaque});
}
#pragma endregion
#pragma region Xe Super Sampling
struct XeSuperSamplingUpscaleData
{
    XeSS_Upscaler* handle;
    Upscaler::Jitter jitter;
    Upscaler::Resolution inputResolution;
    bool resetHistory;
};

void UNITY_INTERFACE_API UpscaleCallbackXeSuperSampling(const int /*unused*/, void* d) {
    const auto&    data = *static_cast<XeSuperSamplingUpscaleData*>(d);
    XeSS_Upscaler& xess = *data.handle;
    xess.resetHistory   = data.resetHistory;
    xess.jitter         = data.jitter;
    xess.evaluate(data.inputResolution);
}

extern "C" UNITY_INTERFACE_EXPORT UnityRenderingEventAndData UNITY_INTERFACE_API GetUpscaleCallbackXeSuperSampling() { return UpscaleCallbackXeSuperSampling; }
extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API LoadedCorrectlyXeSuperSampling() { return XeSS_Upscaler::loadedCorrectly(); }
extern "C" UNITY_INTERFACE_EXPORT XeSS_Upscaler* UNITY_INTERFACE_API CreateContextXeSuperSampling() { return new XeSS_Upscaler; }
extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API UpdateContextXeSuperSampling(XeSS_Upscaler* upscaler, const Upscaler::Resolution resolution, const enum Upscaler::Quality mode, const Upscaler::Flags flags) { return upscaler->useSettings(resolution, mode, flags); }
extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API SetImagesXeSuperSampling(XeSS_Upscaler* upscaler, void* color, void* depth, void* motion, void* output) { return upscaler->useImages({color, depth, motion, output}); }
#pragma endregion

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Resolution UNITY_INTERFACE_API GetRecommendedResolution(const Upscaler* const upscaler) { return upscaler->recommendedInputResolution; }
extern "C" UNITY_INTERFACE_EXPORT Upscaler::Resolution UNITY_INTERFACE_API GetMinimumResolution(const Upscaler* const upscaler) { return upscaler->dynamicMinimumInputResolution; }
extern "C" UNITY_INTERFACE_EXPORT Upscaler::Resolution UNITY_INTERFACE_API GetMaximumResolution(const Upscaler* const upscaler) { return upscaler->dynamicMaximumInputResolution; }
extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API DestroyContext(const Upscaler* upscaler) { delete upscaler; }

#pragma region Frame Generation
#ifdef ENABLE_FRAME_GENERATION
#ifdef ENABLE_FSR
struct alignas(64) FrameGenerateDataFidelityFXSuperResolution {
    std::array<float, 4> rect;
    FfxApiFloatCoords2D renderSize;
    FfxApiFloatCoords2D jitter;
    float frameTime;
    float farPlane;
    float nearPlane;
    float verticalFOV;
    unsigned index;
    unsigned options;
    bool enable;
};

void UNITY_INTERFACE_API GenerateCallbackFidelityFXSuperResolution(const int /*unused*/, void* d) {
    const auto& data = *static_cast<FrameGenerateDataFidelityFXSuperResolution*>(d);
    FSR_FrameGenerator::evaluate(
        data.enable,
        FfxApiRect2D{static_cast<int32_t>(data.rect[0]), static_cast<int32_t>(data.rect[1]), static_cast<int32_t>(data.rect[2]), static_cast<int32_t>(data.rect[3])},
        data.renderSize,
        data.jitter,
        data.frameTime,
        data.farPlane,
        data.nearPlane,
        data.verticalFOV,
        data.index,
        data.options
    );
#endif
}

extern "C" UNITY_INTERFACE_EXPORT UnityRenderingEventAndData UNITY_INTERFACE_API GetGenerateCallbackFidelityFXSuperResolution() { return GenerateCallbackFidelityFXSuperResolution; }

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SetFrameGeneration(HWND hWnd) {
    if (hWnd == nullptr) Plugin::frameGenerationProvider = Plugin::None;
    else Plugin::frameGenerationProvider = Plugin::FSR;
#ifdef ENABLE_VULKAN
    Vulkan::setFrameGenerationHWND(hWnd);
#endif
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SetFrameGenerationImages(void* color0, void* color1, void* depth, void* motion) {
#ifdef ENABLE_FSR
    FSR_FrameGenerator::useImages(static_cast<VkImage>(color0), static_cast<VkImage>(color1), static_cast<VkImage>(depth), static_cast<VkImage>(motion));
#endif
}

extern "C" UNITY_INTERFACE_EXPORT UnityRenderingExtTextureFormat UNITY_INTERFACE_API GetBackBufferFormat() {
    return FrameGenerator::getBackBufferFormat();
}
#endif
#pragma endregion

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(const UnityGfxDeviceEventType eventType) {
    switch (eventType) {
        case kUnityGfxDeviceEventInitialize:
            Plugin::loadedCorrectly = true;
            GraphicsAPI::initialize(Plugin::Unity::graphicsInterface->GetRenderer());
            break;
        case kUnityGfxDeviceEventShutdown:
            GraphicsAPI::shutdown();
            break;
        default: break;
    }
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces) {
    GraphicsAPI::registerUnityInterfaces(unityInterfaces);
    Plugin::Unity::interfaces        = unityInterfaces;
    Plugin::Unity::logInterface      = unityInterfaces->Get<IUnityLog>();
    Plugin::Unity::graphicsInterface = unityInterfaces->Get<IUnityGraphics>();
    Plugin::Unity::graphicsInterface->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginUnload() {
    GraphicsAPI::unregisterUnityInterfaces();
    Plugin::Unity::graphicsInterface->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
    Plugin::Unity::interfaces        = nullptr;
    Plugin::Unity::graphicsInterface = nullptr;
}

extern "C" BOOL WINAPI DllMain(HINSTANCE dllInstance, const DWORD reason, LPVOID reserved) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    char path[MAX_PATH + 1] {};
    GetModuleFileName(dllInstance, path, std::extent_v<decltype(path)>);
    Plugin::path = std::filesystem::path(path).parent_path();
    return TRUE;
}