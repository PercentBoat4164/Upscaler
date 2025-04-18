#include "FrameGenerator/FrameGenerator.hpp"
#ifdef ENABLE_FSR
#include "FrameGenerator/FSR_FrameGenerator.hpp"
#endif
#include "GraphicsAPI/Vulkan.hpp"
#include "Plugin.hpp"
#include "Upscaler/Upscaler.hpp"

#include "IUnityRenderingExtensions.h"
#include "IUnityShaderCompilerAccess.h"

#include <memory>
#include <vector>

// Use 'handle SIGXCPU SIGPWR SIG35 SIG36 SIG37 nostop noprint' to prevent Unity's signals with GDB on Linux.
// Use 'pro hand -p true -s false SIGXCPU SIGPWR' for LLDB on Linux.

// static std::vector<std::unique_ptr<Upscaler>> upscalers = {};
#ifdef ENABLE_FRAME_GENERATION
static std::unique_ptr<FrameGenerator> frameGenerator{};
#endif

struct alignas(128) UpscaleData {
    float frameTime;
    float sharpness;
    float reactiveValue;
    float reactiveScale;
    float reactiveThreshold;
    Upscaler* upscaler;
    float viewToClip[16];
    float clipToView[16];
    float clipToPrevClip[16];
    float prevClipToClip[16];
    float farPlane;
    float nearPlane;
    float verticalFOV;
    float position[3];
    float up[3];
    float right[3];
    float forward[3];
    float jitter[2];
    int inputResolution[2];
    unsigned options;
};

struct FidelityFXSuperResolutionUpscaleData
{
    float frameTime;
    float sharpness;
    float reactiveValue;
    float reactiveScale;
    float reactiveThreshold;
    FSR_Upscaler* handle;
    float farPlane;
    float nearPlane;
    float verticalFOV;
    float jitter[2];
    int inputResolution[2];
    unsigned options;
};

void UNITY_INTERFACE_API UpscaleCallback(const int event, void* d) {
    if (d == nullptr || event - Plugin::Unity::eventIDBase != Plugin::Upscale) return;
    const auto& data              = *static_cast<FidelityFXSuperResolutionUpscaleData*>(d);
    FSR_Upscaler& upscaler        = *data.handle;
    upscaler.settings.farPlane    = data.farPlane;
    upscaler.settings.nearPlane   = data.nearPlane;
    upscaler.settings.verticalFOV = data.verticalFOV;
    upscaler.settings.frameTime         = data.frameTime;
    upscaler.settings.sharpness         = data.sharpness;
    upscaler.settings.reactiveValue     = data.reactiveValue;
    upscaler.settings.reactiveScale     = data.reactiveScale;
    upscaler.settings.reactiveThreshold = data.reactiveThreshold;
    upscaler.settings.orthographic      = (data.options & 0x1U) != 0U;
    upscaler.settings.debugView         = (data.options & 0x2U) != 0U;
    upscaler.settings.resetHistory      = (data.options & 0x4U) != 0U;
    std::construct_at(&upscaler.settings.jitter, data.jitter[0], data.jitter[1]);
    upscaler.evaluate({static_cast<uint32_t>(data.inputResolution[0]), static_cast<uint32_t>(data.inputResolution[1])});
}


struct alignas(64) FrameGenerateData {
    bool enable;
    float rect[4];
    float renderSize[2];
    float jitter[2];
    float frameTime;
    float farPlane;
    float nearPlane;
    float verticalFOV;
    unsigned index;
    unsigned options;
};

// void UNITY_INTERFACE_API UpscaleCallback(const int event, void* d) {
//     if (d == nullptr) return;
//     switch (event - Plugin::Unity::eventIDBase) {
//         case Plugin::Upscale: {
//             const auto& data              = *static_cast<UpscaleData*>(d);
//             Upscaler&   upscaler          = *data.upscaler;
//             upscaler.settings.farPlane    = data.farPlane;
//             upscaler.settings.nearPlane   = data.nearPlane;
//             upscaler.settings.verticalFOV = data.verticalFOV;
//             std::ranges::copy(data.viewToClip,     upscaler.settings.viewToClip.begin());
//             std::ranges::copy(data.clipToView,     upscaler.settings.clipToView.begin());
//             std::ranges::copy(data.clipToPrevClip, upscaler.settings.clipToPrevClip.begin());
//             std::ranges::copy(data.prevClipToClip, upscaler.settings.prevClipToClip.begin());
//             std::ranges::copy(data.position,       upscaler.settings.position.begin());
//             std::ranges::copy(data.up,             upscaler.settings.up.begin());
//             std::ranges::copy(data.right,          upscaler.settings.right.begin());
//             std::ranges::copy(data.forward,        upscaler.settings.forward.begin());
//             upscaler.settings.frameTime         = data.frameTime;
//             upscaler.settings.sharpness         = data.sharpness;
//             upscaler.settings.reactiveValue     = data.reactiveValue;
//             upscaler.settings.reactiveScale     = data.reactiveScale;
//             upscaler.settings.reactiveThreshold = data.reactiveThreshold;
//             upscaler.settings.orthographic      = (data.options & 0x1U) != 0U;
//             upscaler.settings.debugView         = (data.options & 0x2U) != 0U;
//             upscaler.settings.resetHistory      = (data.options & 0x4U) != 0U;
//             std::construct_at(&upscaler.settings.jitter, data.jitter[0], data.jitter[1]);
//             upscaler.evaluate({static_cast<uint32_t>(data.inputResolution[0]), static_cast<uint32_t>(data.inputResolution[1])});
//             break;
//         }
//         case Plugin::FrameGenerate: {
//             const auto& data = *static_cast<FrameGenerateData*>(d);
// #ifdef ENABLE_FSR
//             FSR_FrameGenerator::evaluate(
//                 data.enable,
//                 FfxApiRect2D{static_cast<int32_t>(data.rect[0]), static_cast<int32_t>(data.rect[1]), static_cast<int32_t>(data.rect[2]), static_cast<int32_t>(data.rect[3])},
//                 FfxApiFloatCoords2D{data.renderSize[0], data.renderSize[1]},
//                 FfxApiFloatCoords2D{data.jitter[0], data.jitter[1]},
//                 data.frameTime,
//                 data.farPlane,
//                 data.nearPlane,
//                 data.verticalFOV,
//                 data.index,
//                 data.options
//             );
// #endif
//             break;
//         }
//         default: break;
//     }
// }

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API LoadedCorrectly() {
    return Plugin::loadedCorrectly;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SetLogLevel(const UnityLogType type) {
    Plugin::logLevel = type;
    Plugin::log("", type);
}

#ifdef ENABLE_FRAME_GENERATION
extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SetFrameGeneration(HWND hWnd) {
    if (hWnd == nullptr) Plugin::frameGenerationProvider = Plugin::None;
    else Plugin::frameGenerationProvider = Plugin::FSR;
#ifdef ENABLE_VULKAN
    Vulkan::setFrameGenerationHWND(hWnd);
#endif
}
#endif

extern "C" UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetEventIDBase() {
    return Plugin::Unity::eventIDBase;
}

extern "C" UNITY_INTERFACE_EXPORT UnityRenderingEventAndData UNITY_INTERFACE_API GetRenderingEventCallback() {
    return UpscaleCallback;
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API IsUpscalerSupported(const Upscaler::Type type) {
    return Upscaler::isSupported(type);
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API IsQualitySupported(const Upscaler::Type type, const enum Upscaler::Settings::Quality mode) {
    return Upscaler::isSupported(type, mode);
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler* UNITY_INTERFACE_API RegisterCamera(const Upscaler::Type type) {
    return Upscaler::fromType(type);
}

extern "C" UNITY_INTERFACE_EXPORT FSR_Upscaler* UNITY_INTERFACE_API CreateContextFidelityFXSuperResolution() { return new FSR_Upscaler; }
extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API UpdateContextFidelityFXSuperResolution(FSR_Upscaler* upscaler, const Upscaler::Settings::Resolution resolution, const enum Upscaler::Settings::Quality mode, const bool hdr) { return upscaler->useSettings(resolution, Upscaler::Settings::Default, mode, hdr); }
extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API SetImagesFidelityFXSuperResolution(FSR_Upscaler* upscaler, void* color, void* depth, void* motion, void* output, void* reactive, void* opaque, const bool autoReactive) {
    upscaler->settings.autoReactive = autoReactive;
    return upscaler->useImages({color, depth, motion, output, reactive, opaque});
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API GetCameraUpscalerStatus(const Upscaler* upscaler) {
    return upscaler->getStatus();
}

extern "C" UNITY_INTERFACE_EXPORT const char* UNITY_INTERFACE_API GetCameraUpscalerStatusMessage(Upscaler* const upscaler) {
    return upscaler->getErrorMessage().c_str();
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API SetCameraUpscalerStatus(Upscaler* const upscaler, Upscaler::Status status, const char* message) {
    return upscaler->setStatus(status, message);
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API SetCameraPerFeatureSettings(
  Upscaler* const                       upscaler,
  const Upscaler::Settings::Resolution   resolution,
  const Upscaler::Settings::DLSSPreset   preset,
  const enum Upscaler::Settings::Quality quality,
  const bool                             hdr
) {
    upscaler->resetStatus();
    return upscaler->useSettings(resolution, preset, quality, hdr);
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Settings::Resolution UNITY_INTERFACE_API GetRecommendedCameraResolution(const Upscaler* const upscaler) {
    return upscaler->settings.recommendedInputResolution;
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Settings::Resolution UNITY_INTERFACE_API GetMaximumCameraResolution(const Upscaler* const upscaler) {
    return upscaler->settings.dynamicMaximumInputResolution;
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Settings::Resolution UNITY_INTERFACE_API GetMinimumCameraResolution(const Upscaler* const upscaler) {
    return upscaler->settings.dynamicMinimumInputResolution;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SetUpscalingImages(Upscaler* const upscaler, void* color, void* depth, void* motion, void* output, void* reactive, void* opaque, const bool autoReactive) {
    upscaler->settings.autoReactive = autoReactive;
    upscaler->useImages({color, depth, motion, output, reactive, opaque});
}

#ifdef ENABLE_FRAME_GENERATION
extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SetFrameGenerationImages(void* color0, void* color1, void* depth, void* motion) {
#ifdef ENABLE_FSR
    FSR_FrameGenerator::useImages(static_cast<VkImage>(color0), static_cast<VkImage>(color1), static_cast<VkImage>(depth), static_cast<VkImage>(motion));
#endif
}

extern "C" UNITY_INTERFACE_EXPORT UnityRenderingExtTextureFormat UNITY_INTERFACE_API GetBackBufferFormat() {
    return FrameGenerator::getBackBufferFormat();
}
#endif

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnregisterCamera(const Upscaler* upscaler) {
    delete upscaler;
}

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
    Plugin::Unity::eventIDBase = Plugin::Unity::graphicsInterface->ReserveEventIDRange(1);
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginUnload() {
    GraphicsAPI::unregisterUnityInterfaces();
    Plugin::Unity::graphicsInterface->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
    Plugin::Unity::interfaces        = nullptr;
    Plugin::Unity::graphicsInterface = nullptr;
}

extern "C" BOOL WINAPI DllMain(const HINSTANCE dllInstance, const DWORD reason, LPVOID reserved) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    char path[MAX_PATH + 1] {};
    GetModuleFileName(dllInstance, path, std::extent_v<decltype(path)>);
    Plugin::path = std::filesystem::path(path).parent_path();
    return TRUE;
}