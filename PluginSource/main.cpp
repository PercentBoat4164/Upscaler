#include "Plugin.hpp"
#include "Upscaler/Upscaler.hpp"

#include <memory>
#include <vector>

// Use 'handle SIGXCPU SIGPWR SIG35 SIG36 SIG37 nostop noprint' to prevent Unity's signals with GDB on Linux.
// Use 'pro hand -p true -s false SIGXCPU SIGPWR' for LLDB on Linux.

static std::vector<std::unique_ptr<Upscaler>> upscalers = {};

struct alignas(128) UpscalingData {
    void* color;
    void* depth;
    void* motion;
    void* output;
    void* reactive;
    void* opaque;
    float frameTime;
    float sharpness;
    float reactiveValue;
    float reactiveScale;
    float reactiveThreshold;
    uint16_t camera;
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
    unsigned autoReactive_orthographic;
};

void UNITY_INTERFACE_API INTERNAL_UpscaleCallback(const int event, void* d) {
    if (d == nullptr || event != Plugin::Unity::eventIDBase) return;
    const auto& data              = *static_cast<UpscalingData*>(d);
    Upscaler&   upscaler          = *upscalers[data.camera];
    upscaler.settings.farPlane    = data.farPlane;
    upscaler.settings.nearPlane   = data.nearPlane;
    upscaler.settings.verticalFOV = data.verticalFOV;
    std::ranges::copy(data.viewToClip, upscaler.settings.viewToClip.begin());
    std::ranges::copy(data.clipToView, upscaler.settings.clipToView.begin());
    std::ranges::copy(data.clipToPrevClip, upscaler.settings.clipToPrevClip.begin());
    std::ranges::copy(data.prevClipToClip, upscaler.settings.prevClipToClip.begin());
    std::ranges::copy(data.position, upscaler.settings.position.begin());
    std::ranges::copy(data.up, upscaler.settings.up.begin());
    std::ranges::copy(data.right, upscaler.settings.right.begin());
    std::ranges::copy(data.forward, upscaler.settings.forward.begin());
    upscaler.settings.frameTime         = data.frameTime;
    upscaler.settings.sharpness         = data.sharpness;
    upscaler.settings.reactiveValue     = data.reactiveValue;
    upscaler.settings.reactiveScale     = data.reactiveScale;
    upscaler.settings.reactiveThreshold = data.reactiveThreshold;
    upscaler.settings.autoReactive      = (data.autoReactive_orthographic & 0b1U) != 0U;
    upscaler.settings.orthographic      = (data.autoReactive_orthographic & 0b10U) != 0U;
    upscaler.useImages({data.color, data.depth, data.motion, data.output, data.reactive, data.opaque});
    upscaler.evaluate();
    upscaler.settings.resetHistory = false;
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
    const auto     iter = std::ranges::find_if(upscalers, [](const std::unique_ptr<Upscaler>& upscaler) { return !upscaler; });
    const uint16_t id = std::distance(upscalers.begin(), iter);
    if (iter == upscalers.end()) upscalers.push_back(Upscaler::fromType(Upscaler::NONE));
    else *iter = Upscaler::fromType(Upscaler::NONE);
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

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Settings::Jitter UNITY_INTERFACE_API Upscaler_GetCameraJitter(const uint16_t camera, const float inputWidth) {
    return upscalers[camera]->settings.getNextJitter(inputWidth);
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_ResetCameraHistory(const uint16_t camera) {
    upscalers[camera]->settings.resetHistory = true;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_UnregisterCamera(const uint16_t camera) {
    if (upscalers.size() > camera) upscalers[camera].reset();
}

static void UNITY_INTERFACE_API INTERNAL_OnGraphicsDeviceEvent(const UnityGfxDeviceEventType eventType) {
    switch (eventType) {
        case kUnityGfxDeviceEventInitialize:
            GraphicsAPI::initialize(Plugin::Unity::graphicsInterface->GetRenderer());
            break;
        case kUnityGfxDeviceEventShutdown:
            upscalers.clear();
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
