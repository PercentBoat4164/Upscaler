#include "Upscaler/Upscaler.hpp"

#ifdef ENABLE_DX11
#    include "GraphicsAPI/DX11.hpp"
#endif

#ifdef ENABLE_VULKAN
#    include "GraphicsAPI/Vulkan.hpp"
#endif

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

struct UpscaleInfo {
    const void* const camera = nullptr;
    void* color;
    UnityRenderingExtTextureFormat colorFormat;
    void* depth;
    UnityRenderingExtTextureFormat depthFormat;
    void* motion;
    UnityRenderingExtTextureFormat motionFormat;
    void* output;
    UnityRenderingExtTextureFormat outputFormat;
};

static std::unordered_map<const void*, std::unique_ptr<Upscaler>> cameraToUpscaler = {};

void INTERNAL_Upscale(const UpscaleInfo* const upscaleInfo) {

    cameraToUpscaler.at(upscaleInfo->camera)->evaluate(
      upscaleInfo->color, upscaleInfo->colorFormat,
      upscaleInfo->depth, upscaleInfo->depthFormat,
      upscaleInfo->motion, upscaleInfo->motionFormat,
      upscaleInfo->output, upscaleInfo->outputFormat);
}

void INTERNAL_Prepare(const void* const camera) {
    std::unique_ptr<Upscaler>& upscaler = cameraToUpscaler.at(camera);
    upscaler->resetStatus();
    upscaler->create();
}

void UNITY_INTERFACE_API INTERNAL_RenderingEventCallback(const Event event, const void* const data) {
    switch (event) {
        case UPSCALE: INTERNAL_Upscale(static_cast<const UpscaleInfo* const>(data)); break;
        case PREPARE: INTERNAL_Prepare(data); break;
    }
}

extern "C" UNITY_INTERFACE_EXPORT void* UNITY_INTERFACE_API Upscaler_GetRenderingEventCallback() {
    return reinterpret_cast<void*>(&INTERNAL_RenderingEventCallback);
}

extern "C" UNITY_INTERFACE_EXPORT void Upscaler_RegisterGlobalLogCallback(void(logCallback)(const char*)) {
    Upscaler::setLogCallback(logCallback);
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API Upscaler_IsUpscalerSupported(Upscaler::Type type) {
    return Upscaler::FromType(type)->isSupported();
}

extern "C" UNITY_INTERFACE_EXPORT UpscaleInfo* UNITY_INTERFACE_API Upscaler_RegisterCamera(const void* const camera) {
     cameraToUpscaler[camera] = Upscaler::FromType(Upscaler::NONE);
    return new UpscaleInfo;
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API Upscaler_GetCameraUpscalerStatus(const void* const camera) {
    return cameraToUpscaler.at(camera)->getStatus();
}

extern "C" UNITY_INTERFACE_EXPORT const char* UNITY_INTERFACE_API Upscaler_GetCameraUpscalerStatusMessage(const void* const camera) {
    return cameraToUpscaler.at(camera)->getErrorMessage().c_str();
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API Upscaler_ResetCameraUpscalerStatus(const void* const camera) {
    return cameraToUpscaler.at(camera)->resetStatus();
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API Upscaler_SetCameraPerFeatureSettings(
  const void*                           camera,
  const Upscaler::Settings::Resolution  resolution,
  const Upscaler::Type                  type,
  const Upscaler::Settings::QualityMode quality,
  const bool                            hdr
) {
    std::unique_ptr<Upscaler>& upscaler = cameraToUpscaler.at(camera);
    if (upscaler->getType() != type)
        upscaler = upscaler->fromType(type);
    return upscaler->getOptimalSettings(resolution, quality, hdr);
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Settings::Resolution UNITY_INTERFACE_API Upscaler_GetRecommendedCameraResolution(const void* const camera) {
    return cameraToUpscaler.at(camera)->settings.inputResolution;
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API Upscaler_SetCameraPerFrameData(const void* const camera, float frameTime, float sharpness, Upscaler::Settings::Camera cameraInfo) {
    std::unique_ptr<Upscaler>& upscaler = cameraToUpscaler.at(camera);
    Upscaler::Status           status   = upscaler->setStatusIf(sharpness < 0.F, Upscaler::SETTINGS_ERROR_INVALID_SHARPNESS_VALUE, "The sharpness value of " + std::to_string(sharpness) + " is too small. Expected a value between 0 and 1 inclusive.");
    if (Upscaler::failure(status)) return status;
    status = upscaler->setStatusIf(sharpness > 1.F, Upscaler::SETTINGS_ERROR_INVALID_SHARPNESS_VALUE, "The sharpness value of " + std::to_string(sharpness) + " is too big. Expected a value between 0 and 1 inclusive.");
    if (Upscaler::failure(status)) return status;
    upscaler->settings.frameTime = frameTime;
    upscaler->settings.sharpness = sharpness;
    upscaler->settings.camera    = cameraInfo;
    return Upscaler::SUCCESS;
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Settings::Jitter UNITY_INTERFACE_API Upscaler_GetCameraJitter(const void* const camera) {
    return cameraToUpscaler.at(camera)->settings.getNextJitter();
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_ResetCameraHistory(const void* const camera) {
    cameraToUpscaler.at(camera)->settings.resetHistory = true;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_UnregisterCamera(const void* const camera, UpscaleInfo* upscaleInfoMemory) {
    cameraToUpscaler.erase(camera);
    delete upscaleInfoMemory;
}

static void UNITY_INTERFACE_API INTERNAL_OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType) {
    switch (eventType) {
        case kUnityGfxDeviceEventInitialize:
            GraphicsAPI::set(Unity::graphicsInterface->GetRenderer());
#ifdef ENABLE_DX11
            if (GraphicsAPI::getType() == GraphicsAPI::Type::DX11)
                DX11::createOneTimeSubmitContext();
#endif
            break;
        case kUnityGfxDeviceEventShutdown:
            // Shut down all upscalers
            for (auto& u : cameraToUpscaler)
                u.second->shutdown();
#ifdef ENABLE_DX11
            if (GraphicsAPI::getType() == GraphicsAPI::Type::DX11) DX11::destroyOneTimeSubmitContext();
#endif
            break;
        default: break;
    };
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces) {
    // Enabled plugin's interception of Vulkan initialization calls.
    GraphicsAPI::registerUnityInterfaces(unityInterfaces);
    // Record graphics interface for future use.
    Unity::graphicsInterface = unityInterfaces->Get<IUnityGraphics>();
    Unity::graphicsInterface->RegisterDeviceEventCallback(INTERNAL_OnGraphicsDeviceEvent);
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginUnload() {
    // Remove vulkan initialization interception
    GraphicsAPI::unregisterUnityInterfaces();
    Unity::graphicsInterface->UnregisterDeviceEventCallback(INTERNAL_OnGraphicsDeviceEvent);
}
