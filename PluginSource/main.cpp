#include "Plugin.hpp"
#include "Upscaler/Upscaler.hpp"

#include <IUnityRenderingExtensions.h>

#ifdef ENABLE_DX11
#    include "GraphicsAPI/DX11.hpp"
#endif

#ifdef ENABLE_VULKAN
#    include "GraphicsAPI/Vulkan.hpp"
#    include "GraphicsAPI/Vulkan.hpp"
#endif

#include <algorithm>
#include <memory>
#include <unordered_map>

// Use 'handle SIGXCPU SIGPWR SIG35 SIG36 SIG37 nostop noprint' to prevent Unity's signals with GCC on Linux.
// Use 'pro hand -p true -s false SIGXCPU SIGPWR' for LLDB on Linux.

static std::vector<std::unique_ptr<Upscaler>> upscalers = {};

void UNITY_INTERFACE_API INTERNAL_RenderingEventCallback(int eventID, void* data) {
    if (eventID == kUnityRenderingExtEventUpdateTextureBeginV2) {
        auto* params = static_cast<UnityRenderingExtTextureUpdateParamsV2*>(data);
        upscalers[params->userData & 0x0000FFFFU]->useImage((Plugin::ImageID)(params->userData >> 16U), params->textureID);
    } else if (eventID - Plugin::Unity::eventIDBase == Plugin::Event::Prepare) {
        std::unique_ptr<Upscaler>& upscaler = upscalers[reinterpret_cast<uint64_t>(data)];
        upscaler->resetStatus();
        upscaler->create();
    } else if (eventID - Plugin::Unity::eventIDBase == Plugin::Event::Upscale) {
        upscalers[reinterpret_cast<uint64_t>(data)]->evaluate();
    }
}

extern "C" UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API Upscaler_GetEventIDBase() {
    return Plugin::Unity::eventIDBase;
}

extern "C" UNITY_INTERFACE_EXPORT UnityRenderingEventAndData UNITY_INTERFACE_API Upscaler_GetRenderingEventCallback() {
    return INTERNAL_RenderingEventCallback;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_RegisterGlobalLogCallback(void(logCallback)(const char*)) {
    Upscaler::setLogCallback(logCallback);
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API Upscaler_IsUpscalerSupported(Upscaler::Type type) {
    return Upscaler::fromType(type)->isSupported();
}

extern "C" UNITY_INTERFACE_EXPORT uint16_t UNITY_INTERFACE_API Upscaler_RegisterCamera() {
    auto     iter = std::find_if(upscalers.begin(), upscalers.end(), [](std::unique_ptr<Upscaler>& upscaler) { return !upscaler; });
    uint16_t id   = iter - upscalers.begin();
    if (iter == upscalers.end()) upscalers.push_back(Upscaler::fromType(Upscaler::NONE));
    else *iter = Upscaler::fromType(Upscaler::NONE);
    return id;
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API Upscaler_GetCameraUpscalerStatus(uint16_t camera) {
    return upscalers[camera]->getStatus();
}

extern "C" UNITY_INTERFACE_EXPORT const char* UNITY_INTERFACE_API Upscaler_GetCameraUpscalerStatusMessage(uint16_t camera) {
    return upscalers[camera]->getErrorMessage().c_str();
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API Upscaler_ResetCameraUpscalerStatus(uint16_t camera) {
    return upscalers[camera]->resetStatus();
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API Upscaler_SetCameraPerFeatureSettings(
  uint16_t                               camera,
  const Upscaler::Settings::Resolution   resolution,
  const Upscaler::Type                   type,
  const Upscaler::Settings::Preset       preset,
  const enum Upscaler::Settings::Quality quality,
  const bool                             hdr
) {
    std::unique_ptr<Upscaler>& upscaler = upscalers[camera];
    if (upscaler->getType() != type) upscaler = upscaler->copyFromType(type);
    return upscaler->getOptimalSettings(resolution, preset, quality, hdr);
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Settings::Resolution UNITY_INTERFACE_API Upscaler_GetRecommendedCameraResolution(uint16_t camera) {
    return upscalers[camera]->settings.renderingResolution;
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Settings::Resolution UNITY_INTERFACE_API Upscaler_GetMaximumCameraResolution(uint16_t camera) {
    return upscalers[camera]->settings.dynamicMaximumInputResolution;
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Settings::Resolution UNITY_INTERFACE_API Upscaler_GetMinimumCameraResolution(uint16_t camera) {
    return upscalers[camera]->settings.dynamicMinimumInputResolution;
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API Upscaler_SetCameraPerFrameData(
  uint16_t                       camera,
  float                          frameTime,
  float                          sharpness,
  Upscaler::Settings::Camera     cameraInfo
) {
    std::unique_ptr<Upscaler>& upscaler = upscalers[camera];
    upscaler->setStatusIf(sharpness < 0.F, Upscaler::SETTINGS_ERROR_INVALID_SHARPNESS_VALUE, "The sharpness value of " + std::to_string(sharpness) + " is too small. Expected a value between 0 and 1 inclusive.");
    Upscaler::Status status = upscaler->setStatusIf(sharpness > 1.F, Upscaler::SETTINGS_ERROR_INVALID_SHARPNESS_VALUE, "The sharpness value of " + std::to_string(sharpness) + " is too big. Expected a value between 0 and 1 inclusive.");
    if (Upscaler::failure(status)) return status;

    upscaler->settings.frameTime           = frameTime;
    upscaler->settings.sharpness           = sharpness;
    upscaler->settings.camera              = cameraInfo;
    return Upscaler::SUCCESS;
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Settings::Jitter UNITY_INTERFACE_API Upscaler_GetCameraJitter(uint16_t camera, bool advance) {
    if (advance) return upscalers[camera]->settings.getNextJitter();
    return upscalers[camera]->settings.jitter;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_ResetCameraHistory(uint16_t camera) {
    upscalers[camera]->settings.resetHistory = true;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_UnregisterCamera(uint16_t camera) {
    if (upscalers.size() > camera) upscalers[camera].reset();
}

static void UNITY_INTERFACE_API INTERNAL_OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType) {
    switch (eventType) {
        case kUnityGfxDeviceEventInitialize:
            GraphicsAPI::set(Plugin::Unity::graphicsInterface->GetRenderer());
#ifdef ENABLE_DX11
            if (GraphicsAPI::getType() == GraphicsAPI::Type::DX11)
                DX11::createOneTimeSubmitContext();
#endif
            break;
        case kUnityGfxDeviceEventShutdown:
            upscalers.clear();
#ifdef ENABLE_DX11
            if (GraphicsAPI::getType() == GraphicsAPI::Type::DX11) DX11::destroyOneTimeSubmitContext();
#endif
            break;
        default: break;
    };
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces) {
    GraphicsAPI::registerUnityInterfaces(unityInterfaces);
    Plugin::Unity::graphicsInterface = unityInterfaces->Get<IUnityGraphics>();
    Plugin::Unity::graphicsInterface->RegisterDeviceEventCallback(INTERNAL_OnGraphicsDeviceEvent);
    Plugin::Unity::eventIDBase = Plugin::Unity::graphicsInterface->ReserveEventIDRange(2);
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginUnload() {
    GraphicsAPI::unregisterUnityInterfaces();
    Plugin::Unity::graphicsInterface->UnregisterDeviceEventCallback(INTERNAL_OnGraphicsDeviceEvent);
}
