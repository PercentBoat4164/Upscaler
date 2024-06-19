#include "Plugin.hpp"
#include "Upscaler/Upscaler.hpp"

#include <IUnityRenderingExtensions.h>

#ifdef ENABLE_DX11
#    include "GraphicsAPI/DX11.hpp"
#endif

#ifdef ENABLE_VULKAN
#    include "GraphicsAPI/Vulkan.hpp"
#endif

#include <memory>
#include <mutex>
#include <unordered_map>

// Use 'handle SIGXCPU SIGPWR SIG35 SIG36 SIG37 nostop noprint' to prevent Unity's signals with GDB on Linux.
// Use 'pro hand -p true -s false SIGXCPU SIGPWR' for LLDB on Linux.

static std::mutex pluginLock;
static std::vector<std::unique_ptr<Upscaler>> upscalers = {};
static std::vector<std::unique_ptr<std::mutex>> locks = {};

struct UpscalingData {
    void* color;
    void* depth;
    void* motion;
    void* output;
    void* reactive;
    void* opaque;
    float frameTime;
    float sharpness;
    float tcThreshold;
    float tcScale;
    float reactiveScale;
    float reactiveMax;
    Upscaler::Settings::Camera cameraInfo;
    uint16_t camera;
    bool autoReactive;
};

void UNITY_INTERFACE_API INTERNAL_UpscaleCallback(const int event, void* d) {
    if (d == nullptr) return;
    const UpscalingData& data = *static_cast<UpscalingData*>(d);
    std::lock_guard lock{*locks[data.camera]};
    Upscaler& upscaler = *upscalers[data.camera];
    upscaler.settings.camera = data.cameraInfo;
    upscaler.settings.frameTime = data.frameTime;
    upscaler.settings.sharpness = data.sharpness;
    upscaler.settings.tcThreshold = data.tcThreshold;
    upscaler.settings.tcScale = data.tcScale;
    upscaler.settings.reactiveScale = data.reactiveScale;
    upscaler.settings.reactiveMax = data.reactiveMax;
    upscaler.settings.autoReactive = data.autoReactive;
    upscaler.useImages({data.color, data.depth, data.motion, data.output, data.reactive, data.opaque});
    upscaler.evaluate();
}

extern "C" UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API Upscaler_GetEventIDBase() {
    return Plugin::Unity::eventIDBase;
}

extern "C" UNITY_INTERFACE_EXPORT UnityRenderingEventAndData UNITY_INTERFACE_API Upscaler_GetRenderingEventCallback() {
    return INTERNAL_UpscaleCallback;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_RegisterGlobalLogCallback(void(logCallback)(const char*)) {
    Upscaler::setLogCallback(logCallback);
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API Upscaler_IsUpscalerSupported(const Upscaler::Type type) {
    return Upscaler::isSupported(type);
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API Upscaler_IsQualitySupported(const Upscaler::Type type, const enum Upscaler::Settings::Quality mode) {
    return Upscaler::isSupported(type, mode);
}

extern "C" UNITY_INTERFACE_EXPORT uint16_t UNITY_INTERFACE_API Upscaler_RegisterCamera() {
    std::lock_guard lock{pluginLock};
    const auto     iter = std::ranges::find_if(upscalers, [](const std::unique_ptr<Upscaler>& upscaler) { return !upscaler; });
    const uint16_t id = std::distance(upscalers.begin(), iter);
    if (iter == upscalers.end()) {
        upscalers.push_back(Upscaler::fromType(Upscaler::NONE));
        locks.emplace_back(std::make_unique<std::mutex>());
    }
    else {
        *iter = Upscaler::fromType(Upscaler::NONE);
        locks[id] = std::make_unique<std::mutex>();
    }
    return id;
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API Upscaler_GetCameraUpscalerStatus(const uint16_t camera) {
    return upscalers[camera]->getStatus();
}

extern "C" UNITY_INTERFACE_EXPORT const char* UNITY_INTERFACE_API Upscaler_GetCameraUpscalerStatusMessage(const uint16_t camera) {
    return upscalers[camera]->getErrorMessage().c_str();
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API Upscaler_SetCameraUpscalerStatus(const uint16_t camera, Upscaler::Status status, const char* message) {
    return upscalers[camera]->setStatus(status, message);
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API Upscaler_SetCameraPerFeatureSettings(
  const uint16_t                         camera,
  const Upscaler::Settings::Resolution   resolution,
  const Upscaler::Type                   type,
  const Upscaler::Settings::DLSSPreset   preset,
  const enum Upscaler::Settings::Quality quality,
  const bool                             hdr
) {
    std::lock_guard lock{*locks[camera]};
    std::unique_ptr<Upscaler>& upscaler = upscalers[camera];
    if (upscaler->getType() != type) upscaler = std::move(Upscaler::fromType(type));
    else upscaler->resetStatus();
    return upscaler->useSettings(resolution, preset, quality, hdr);
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Settings::Resolution UNITY_INTERFACE_API Upscaler_GetRecommendedCameraResolution(const uint16_t camera) {
    return upscalers[camera]->settings.recommendedInputResolution;
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Settings::Resolution UNITY_INTERFACE_API Upscaler_GetMaximumCameraResolution(const uint16_t camera) {
    return upscalers[camera]->settings.dynamicMaximumInputResolution;
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Settings::Resolution UNITY_INTERFACE_API Upscaler_GetMinimumCameraResolution(const uint16_t camera) {
    return upscalers[camera]->settings.dynamicMinimumInputResolution;
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Settings::Jitter UNITY_INTERFACE_API Upscaler_GetCameraJitter(const uint16_t camera) {
    return upscalers[camera]->settings.getNextJitter();
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_ResetCameraHistory(const uint16_t camera) {
    upscalers[camera]->settings.resetHistory = true;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_UnregisterCamera(const uint16_t camera) {
    std::lock_guard lock{pluginLock};
    if (upscalers.size() > camera) upscalers[camera].reset();
    if (locks.size() > camera) locks[camera].reset();
}

static void UNITY_INTERFACE_API INTERNAL_OnGraphicsDeviceEvent(const UnityGfxDeviceEventType eventType) {
    switch (eventType) {
        case kUnityGfxDeviceEventInitialize:
            GraphicsAPI::initialize(Plugin::Unity::graphicsInterface->GetRenderer());
            break;
        case kUnityGfxDeviceEventShutdown:
            upscalers.clear();
            locks.clear();
            GraphicsAPI::shutdown();
            break;
        default: break;
    }
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces) {
    GraphicsAPI::registerUnityInterfaces(unityInterfaces);
    Plugin::Unity::graphicsInterface = unityInterfaces->Get<IUnityGraphics>();
    Plugin::Unity::graphicsInterface->RegisterDeviceEventCallback(INTERNAL_OnGraphicsDeviceEvent);
    Plugin::Unity::eventIDBase = Plugin::Unity::graphicsInterface->ReserveEventIDRange(1);
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginUnload() {
    GraphicsAPI::unregisterUnityInterfaces();
    Plugin::Unity::graphicsInterface->UnregisterDeviceEventCallback(INTERNAL_OnGraphicsDeviceEvent);
}
