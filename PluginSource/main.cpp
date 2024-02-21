// Project
#include "Upscaler/NoUpscaler.hpp"
#include "Upscaler/Upscaler.hpp"

#ifdef ENABLE_DX11
#    include "GraphicsAPI/DX11.hpp"
#endif

#ifdef ENABLE_VULKAN
#    include "GraphicsAPI/Vulkan.hpp"
#endif

// Unity
#include <IUnityInterface.h>
#include <IUnityRenderingExtensions.h>

#include <memory>
#include <unordered_map>

// Use 'handle SIGXCPU SIGPWR SIG35 SIG36 SIG37 nostop noprint' to prevent Unity's signals with GCC on Linux.
// Use 'pro hand -p true -s false SIGXCPU SIGPWR' for LLDB on Linux.

namespace Unity {
IUnityGraphics* graphicsInterface;
}  // namespace Unity

enum Event {
    UPSCALE,
    PREPARE,
};

static std::unordered_map<const void*, std::unique_ptr<Upscaler>> cameraToUpscaler = {};

void INTERNAL_Upscale(const void* camera) {
    cameraToUpscaler.at(camera)->evaluate();
}

void INTERNAL_Prepare(const void* camera) {
    cameraToUpscaler.at(camera)->create();
}

void UNITY_INTERFACE_API INTERNAL_RenderingEventCallback(const Event event, const void* camera) {
    switch (event) {
        case UPSCALE: INTERNAL_Upscale(camera); break;
        case PREPARE: INTERNAL_Prepare(camera); break;
    }
}

extern "C" UNITY_INTERFACE_EXPORT void* UNITY_INTERFACE_API Upscaler_GetRenderingEventCallback() {
    return reinterpret_cast<void*>(&INTERNAL_RenderingEventCallback);
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API Upscaler_IsUpscalerSupported(Upscaler::Type type) {
    std::unique_ptr<Upscaler> s = std::make_unique<NoUpscaler>();
    s.reset(s->set(type));
    return s->isSupported();
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_RegisterCamera(const void* camera) {
    std::unique_ptr<Upscaler>& upscaler = cameraToUpscaler[camera] = std::make_unique<NoUpscaler>();
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_RegisterCameraErrorCallback(const void* camera, const void* data, void (*errorCallback)(const void*, const Upscaler::Status, const char*)) {
    cameraToUpscaler.at(camera)->setErrorCallback(data, errorCallback);
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_RegisterLogCallback(void(logCallback)(const char*)) {
    Upscaler::setLogCallback(logCallback);
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API Upscaler_SetCameraUpscaler(const void* camera, const Upscaler::Type type) {
    std::unique_ptr<Upscaler>& upscaler = cameraToUpscaler.at(camera);
    upscaler->shutdown();
    upscaler.reset(upscaler->set(type));
    return upscaler->initialize();
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API Upscaler_GetCameraUpscalerStatus(const void* camera) {
    return cameraToUpscaler.at(camera)->getStatus();
}

extern "C" UNITY_INTERFACE_EXPORT const char* UNITY_INTERFACE_API Upscaler_GetCameraUpscalerStatusMessage(const void* camera) {
    return cameraToUpscaler.at(camera)->getErrorMessage().c_str();
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API Upscaler_ResetCameraUpscalerStatus(const void* camera) {
    return cameraToUpscaler.at(camera)->resetStatus();
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API Upscaler_SetCameraFramebufferSettings(
  const void*                           camera,
  const unsigned int                    t_width,
  const unsigned int                    t_height,
  const Upscaler::Settings::QualityMode t_quality,
  const bool                            t_HDR
) {
    std::unique_ptr<Upscaler>& upscaler = cameraToUpscaler.at(camera);
    const Upscaler::Settings   settings = upscaler->getOptimalSettings({t_width, t_height}, t_quality, t_HDR);
    const Upscaler::Status     status   = upscaler->getStatus();
    if (Upscaler::success(status)) upscaler->settings = settings;
    return status;
}

extern "C" UNITY_INTERFACE_EXPORT uint64_t UNITY_INTERFACE_API Upscaler_GetRecommendedCameraResolution(const void* camera) {
    std::unique_ptr<Upscaler>& upscaler       = cameraToUpscaler.at(camera);
    const auto                 recommendation = upscaler->settings.inputResolution.asUint64_t();
    upscaler->setStatusIf(recommendation == 0, Upscaler::Status::SETTINGS_ERROR, "Some setting is invalid. Please reset the framebuffer settings to something valid.");
    return recommendation;
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API Upscaler_SetCameraSharpnessValue(const void* camera, const float t_sharpness) {
    const bool                 tooSmall = t_sharpness < 0.0;
    std::unique_ptr<Upscaler>& upscaler = cameraToUpscaler.at(camera);
    if (const bool tooBig = t_sharpness > 1.0; Upscaler::success(upscaler->setStatusIf(
          tooSmall || tooBig,
          Upscaler::SETTINGS_ERROR_INVALID_SHARPNESS_VALUE,
          std::string(tooBig ? "The selected sharpness value is too big." : "The selected sharpness value is too small.") + " The given sharpness value (" + std::to_string(t_sharpness) + ") must be greater than 0 but less than 1."
        ))) upscaler->settings.sharpness = t_sharpness;
    return upscaler->getStatus();
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_SetCameraJitterInformation(const void* camera, const float x, const float y) {
    std::unique_ptr<Upscaler>& upscaler = cameraToUpscaler.at(camera);
    upscaler->settings.jitter[0]        = x;
    upscaler->settings.jitter[1]        = y;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_SetCameraFrameInformation(const void* camera, float frameTime, Upscaler::Settings::Camera cameraInfo) {
    std::unique_ptr<Upscaler>& upscaler = cameraToUpscaler.at(camera);
    upscaler->settings.frameTime        = frameTime;
    upscaler->settings.camera           = cameraInfo;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_ResetCameraHistory(const void* camera) {
    cameraToUpscaler.at(camera)->settings.resetHistory = true;
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API Upscaler_SetCameraDepth(const void* camera, void* nativeHandle, const UnityRenderingExtTextureFormat unityFormat) {
    return cameraToUpscaler.at(camera)->setDepth(nativeHandle, unityFormat);
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API Upscaler_SetCameraInputColor(const void* camera, void* nativeHandle, const UnityRenderingExtTextureFormat unityFormat) {
    return cameraToUpscaler.at(camera)->setInputColor(nativeHandle, unityFormat);
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API Upscaler_SetCameraMotionVectors(const void* camera, void* nativeHandle, const UnityRenderingExtTextureFormat unityFormat) {
    return cameraToUpscaler.at(camera)->setMotionVectors(nativeHandle, unityFormat);
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API Upscaler_SetCameraOutputColor(const void* camera, void* nativeHandle, const UnityRenderingExtTextureFormat unityFormat) {
    return cameraToUpscaler.at(camera)->setOutputColor(nativeHandle, unityFormat);
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_UnregisterCamera(const void* camera) {
    cameraToUpscaler.at(camera)->shutdown();
    cameraToUpscaler.erase(camera);
}

static void UNITY_INTERFACE_API INTERNAL_OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType) {
    switch (eventType) {
        case kUnityGfxDeviceEventInitialize:
            GraphicsAPI::set(Unity::graphicsInterface->GetRenderer());
#ifdef ENABLE_DX11
            if (GraphicsAPI::get()->getType() == GraphicsAPI::Type::DX11)
                GraphicsAPI::get<DX11>()->prepareForOneTimeSubmits();
#endif
            break;
        case kUnityGfxDeviceEventShutdown:
            // Shut down all upscalers
            for (auto& u : cameraToUpscaler)
                u.second->shutdown();
#ifdef ENABLE_DX11
            if (GraphicsAPI::get()->getType() == GraphicsAPI::Type::DX11) GraphicsAPI::get<DX11>()->finishOneTimeSubmits();
#endif
            break;
        default: break;
    };
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces) {
    // Enabled plugin's interception of Vulkan initialization calls.
    Vulkan::registerUnityInterface(unityInterfaces);
    // Record graphics interface for future use.
    Unity::graphicsInterface = unityInterfaces->Get<IUnityGraphics>();
    Unity::graphicsInterface->RegisterDeviceEventCallback(INTERNAL_OnGraphicsDeviceEvent);
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginUnload() {
    // Remove vulkan initialization interception
    Vulkan::unregisterUnityInterface();
    Unity::graphicsInterface->UnregisterDeviceEventCallback(INTERNAL_OnGraphicsDeviceEvent);
}
