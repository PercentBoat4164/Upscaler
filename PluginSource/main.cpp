// Project
#include "Upscaler/NoUpscaler.hpp"

#ifdef ENABLE_VULKAN
#    include "GraphicsAPI/DX11.hpp"
#    include "GraphicsAPI/Vulkan.hpp"
#endif

// Unity
#include <IUnityInterface.h>
#include <IUnityRenderingExtensions.h>

// Use 'handle SIGXCPU SIGPWR SIG35 SIG36 SIG37 nostop noprint' to prevent Unity's signals with GCC on Linux.
// Use 'pro hand -p true -s false SIGXCPU SIGPWR' for LLDB on Linux.

namespace Unity {
IUnityGraphics *graphicsInterface;
}  // namespace Unity

enum Event {
    UPSCALE,
    PREPARE,
};

void INTERNAL_Upscale() {
    // Disable the callback so that errors are not thrown mid-render. Errors can instead be handled during the next
    // frame.
    void (*cb)(void *, Upscaler::Status, const char *) = Upscaler::setErrorCallback(nullptr, nullptr);
    Upscaler::get()->evaluate();
    Upscaler::setErrorCallback(nullptr, cb);
}

void INTERNAL_Prepare() {
    Upscaler::get()->create();
}

void UNITY_INTERFACE_API Upscaler_RenderingEventCallback(const Event event) {
    switch (event) {
        case UPSCALE: INTERNAL_Upscale(); break;
        case PREPARE: INTERNAL_Prepare(); break;
    }
}

extern "C" UNITY_INTERFACE_EXPORT void *UNITY_INTERFACE_API Upscaler_GetRenderingEventCallback() {
    return reinterpret_cast<void *>(&Upscaler_RenderingEventCallback);
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API
Upscaler_InitializePlugin(void *data, void (*t_errorCallback)(void *, Upscaler::Status, const char *), void (*t_logCallback)(const char*)=nullptr) {
    GraphicsAPI::set(Unity::graphicsInterface->GetRenderer());
#ifdef ENABLE_DX11
    if (GraphicsAPI::get()->getType() == GraphicsAPI::Type::DX11)
        GraphicsAPI::get<DX11>()->prepareForOneTimeSubmits();
#endif
    Upscaler::setErrorCallback(data, t_errorCallback);
    Upscaler::setLogCallback(t_logCallback);
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API
                                  Upscaler_SetUpscaler(const Upscaler::Type type) {
    Upscaler::get()->shutdown();
    Upscaler::set(type);
    return Upscaler::get()->initialize();
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API Upscaler_GetError(const Upscaler::Type type
) {
    return Upscaler::get(type)->getStatus();
}

extern "C" UNITY_INTERFACE_EXPORT const char *UNITY_INTERFACE_API
Upscaler_GetErrorMessage(const Upscaler::Type type) {
    return Upscaler::get(type)->getErrorMessage().c_str();
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API Upscaler_GetCurrentError() {
    return Upscaler::get()->getStatus();
}

extern "C" UNITY_INTERFACE_EXPORT const char *UNITY_INTERFACE_API Upscaler_GetCurrentErrorMessage() {
    return Upscaler::get()->getErrorMessage().c_str();
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API Upscaler_SetFramebufferSettings(
  const unsigned int                    t_width,
  const unsigned int                    t_height,
  const Upscaler::Settings::QualityMode t_quality,
  const bool                            t_HDR
) {
    Upscaler                *upscaler = Upscaler::get();
    const Upscaler::Settings settings = upscaler->getOptimalSettings({t_width, t_height}, t_quality, t_HDR);
    const Upscaler::Status   status   = upscaler->getStatus();
    if (Upscaler::success(status)) Upscaler::settings = settings;
    return status;
}

extern "C" UNITY_INTERFACE_EXPORT uint64_t UNITY_INTERFACE_API Upscaler_GetRecommendedInputResolution() {
    const auto recommendation = Upscaler::settings.recommendedInputResolution.asLong();
    Upscaler::get()->setStatusIf(
      recommendation == 0,
      Upscaler::Status::SETTINGS_ERROR,
      "Some setting is invalid. Please reset the framebuffer settings to something valid."
    );
    return recommendation;
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API
                                  Upscaler_SetSharpnessValue(const float t_sharpness) {
    const bool tooSmall = t_sharpness < 0.0;
    if (const bool tooBig = t_sharpness > 1.0; Upscaler::success(Upscaler::get()->setStatusIf(
          tooSmall || tooBig,
          Upscaler::SETTINGS_ERROR_INVALID_SHARPNESS_VALUE,
          std::string(
            tooBig ? "The selected sharpness value is too big." : "The selected sharpness value is too small."
          ) +
            " The given sharpness value (" + std::to_string(t_sharpness) +
            ") must be greater than 0 but less than 1."
        )))
        Upscaler::settings.sharpness = t_sharpness;
    return Upscaler::get()->getStatus();
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API
Upscaler_SetJitterInformation(const float x, const float y) {
    Upscaler::settings.jitter[0] = x;
    Upscaler::settings.jitter[1] = y;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_ResetHistory() {
    Upscaler::settings.resetHistory = true;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_ResetStatus() {
    Upscaler::get()->resetStatus();
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API
Upscaler_SetDepthBuffer(void *nativeHandle, const UnityRenderingExtTextureFormat unityFormat) {
    return Upscaler::get()->setDepthBuffer(nativeHandle, unityFormat);
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API
Upscaler_SetInputColor(void *nativeHandle, const UnityRenderingExtTextureFormat unityFormat) {
    return Upscaler::get()->setInputColor(nativeHandle, unityFormat);
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API
Upscaler_SetMotionVectors(void *nativeHandle, const UnityRenderingExtTextureFormat unityFormat) {
    return Upscaler::get()->setMotionVectors(nativeHandle, unityFormat);
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API
Upscaler_SetOutputColor(void *nativeHandle, const UnityRenderingExtTextureFormat unityFormat) {
    return Upscaler::get()->setOutputColor(nativeHandle, unityFormat);
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_Shutdown() {
    Upscaler::get()->shutdown();
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_ShutdownPlugin() {
#ifdef ENABLE_DX11
    // Finish all one time submits
    if (GraphicsAPI::get()->getType() == GraphicsAPI::Type::DX11) GraphicsAPI::get<DX11>()->finishOneTimeSubmits();
#endif
    // Clean up
    for (Upscaler *upscaler : Upscaler::getAllUpscalers()) upscaler->shutdown();
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces *unityInterfaces) {
    // Enabled plugin's interception of Vulkan initialization calls.
    for (GraphicsAPI *graphicsAPI : GraphicsAPI::getAllGraphicsAPIs())
        graphicsAPI->useUnityInterfaces(unityInterfaces);
    // Record graphics interface for future use.
    Unity::graphicsInterface = unityInterfaces->Get<IUnityGraphics>();
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginUnload() {
    Upscaler_ShutdownPlugin();
    // Remove vulkan initialization interception
    Vulkan::RemoveInterceptInitialization();
}
