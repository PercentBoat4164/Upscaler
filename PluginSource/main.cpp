// Project
#include "GraphicsAPI/NoGraphicsAPI.hpp"
#include "Upscaler/NoUpscaler.hpp"

#ifdef ENABLE_VULKAN
#    include "GraphicsAPI/Vulkan.hpp"
#endif
#ifdef ENABLE_DX12
#    include "GraphicsAPI/DX12.hpp"
#endif
#ifdef ENABLE_DX11
#    include "GraphicsAPI/DX11.hpp"
#endif
#ifdef ENABLE_DLSS
#    include "Upscaler/DLSS.hpp"
#endif

// Unity
#include <IUnityInterface.h>
#include <IUnityRenderingExtensions.h>

// Use 'handle SIGXCPU SIGPWR SIG35 SIG36 SIG37 nostop noprint' to prevent Unity's signals on Linux.

namespace Unity {
IUnityGraphics *graphicsInterface;
}  // namespace Unity

enum Event {
    UPSCALE,
};

void INTERNAL_Upscale() {
    Upscaler::get()->evaluate();
}

void UNITY_INTERFACE_API Upscaler_RenderingEventCallback(Event event) {
    switch (event) {
        case UPSCALE: INTERNAL_Upscale(); break;
    }
}

extern "C" UNITY_INTERFACE_EXPORT void *UNITY_INTERFACE_API Upscaler_GetRenderingEventCallback() {
    return reinterpret_cast<void *>(&Upscaler_RenderingEventCallback);
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_InitializePlugin() {
    GraphicsAPI::set(Unity::graphicsInterface->GetRenderer());
    GraphicsAPI::get()->prepareForOneTimeSubmits();
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::UpscalerStatus UNITY_INTERFACE_API Upscaler_Set(Upscaler::Type type) {
    Upscaler::get()->shutdown();
    Upscaler::set(type);
    return Upscaler::get()->initialize();
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::UpscalerStatus UNITY_INTERFACE_API Upscaler_GetError(Upscaler::Type type
) {
    return Upscaler::get(type)->getError();
}

extern "C" UNITY_INTERFACE_EXPORT const char *UNITY_INTERFACE_API Upscaler_GetErrorMessage(Upscaler::Type type) {
    return Upscaler::get(type)->getErrorMessage().c_str();
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::UpscalerStatus UNITY_INTERFACE_API Upscaler_GetCurrentError() {
    return Upscaler::get()->getError();
}

extern "C" UNITY_INTERFACE_EXPORT const char *UNITY_INTERFACE_API Upscaler_GetCurrentErrorMessage() {
    return Upscaler::get()->getErrorMessage().c_str();
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::UpscalerStatus UNITY_INTERFACE_API Upscaler_SetFramebufferSettings(
  unsigned int                t_width,
  unsigned int                t_height,
  Upscaler::Settings::Quality t_quality,
  bool                        t_HDR
) {
    Upscaler *upscaler = Upscaler::get();
    Upscaler::Settings settings = upscaler->getOptimalSettings({t_width, t_height}, t_quality, t_HDR);
    Upscaler::UpscalerStatus status = upscaler->getError();
    if (status == Upscaler::SUCCESS)
        Upscaler::settings = settings;
    return status;
}

extern "C" UNITY_INTERFACE_EXPORT uint64_t UNITY_INTERFACE_API Upscaler_GetRecommendedInputResolution() {
    return Upscaler::settings.recommendedInputResolution.asLong();
}

extern "C" UNITY_INTERFACE_EXPORT uint64_t UNITY_INTERFACE_API Upscaler_GetMinimumInputResolution() {
    return Upscaler::settings.dynamicMinimumInputResolution.asLong();
}

extern "C" UNITY_INTERFACE_EXPORT uint64_t UNITY_INTERFACE_API Upscaler_GetMaximumInputResolution() {
    return Upscaler::settings.dynamicMaximumInputResolution.asLong();
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::UpscalerStatus UNITY_INTERFACE_API Upscaler_SetSharpnessValue(float t_sharpness) {
    bool tooSmall = t_sharpness < 0.0;
    bool tooBig = t_sharpness > 1.0;
    return Upscaler::get()->setErrorIf(tooSmall || tooBig, Upscaler::SETTINGS_ERROR_INVALID_SHARPNESS_VALUE, std::string(tooBig ? "The selected sharpness value is too big." : "The selected sharpness value is too small.") + " The given sharpness value (" + std::to_string(t_sharpness) + ") must be greater than 0 but less than 1.");
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::UpscalerStatus UNITY_INTERFACE_API
                                  Upscaler_SetCurrentInputResolution(unsigned int t_width, unsigned int t_height) {
    bool safeToContinue{true};
    Upscaler *upscaler{Upscaler::get()};
    safeToContinue &= Upscaler::success(upscaler->setErrorIf(t_width > Upscaler::settings.dynamicMaximumInputResolution.width, Upscaler::SETTINGS_ERROR_INVALID_INPUT_RESOLUTION, "The given input resolution (" + std::to_string(t_width) + "x" + std::to_string(t_height) + ") is too wide. It must be thinner than the maximum supported input resolution (" + std::to_string(Upscaler::settings.dynamicMaximumInputResolution.width) + "x" + std::to_string(Upscaler::settings.dynamicMaximumInputResolution.height) +") for the given output resolution."));
    safeToContinue &= Upscaler::success(upscaler->setErrorIf(t_width < Upscaler::settings.dynamicMinimumInputResolution.width, Upscaler::SETTINGS_ERROR_INVALID_INPUT_RESOLUTION, "The given input resolution (" + std::to_string(t_width) + "x" + std::to_string(t_height) + ") is too thin. It must be wider than the minimum supported input resolution (" + std::to_string(Upscaler::settings.dynamicMinimumInputResolution.width) + "x" + std::to_string(Upscaler::settings.dynamicMinimumInputResolution.height) +") for the given output resolution."));
    safeToContinue &= Upscaler::success(upscaler->setErrorIf(t_height > Upscaler::settings.dynamicMaximumInputResolution.height, Upscaler::SETTINGS_ERROR_INVALID_INPUT_RESOLUTION, "The given input resolution (" + std::to_string(t_width) + "x" + std::to_string(t_height) + ") is too tall. It must be shorter than the maximum supported input resolution (" + std::to_string(Upscaler::settings.dynamicMaximumInputResolution.width) + "x" + std::to_string(Upscaler::settings.dynamicMaximumInputResolution.height) +") for the given output resolution."));
    safeToContinue &= Upscaler::success(upscaler->setErrorIf(t_height < Upscaler::settings.dynamicMinimumInputResolution.height, Upscaler::SETTINGS_ERROR_INVALID_INPUT_RESOLUTION, "The given input resolution (" + std::to_string(t_width) + "x" + std::to_string(t_height) + ") is too short. It must be taller than the minimum supported input resolution (" + std::to_string(Upscaler::settings.dynamicMinimumInputResolution.width) + "x" + std::to_string(Upscaler::settings.dynamicMinimumInputResolution.height) +") for the given output resolution."));
    if (safeToContinue)
        Upscaler::settings.currentInputResolution = {t_width, t_height};
    return upscaler->getError();
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_SetJitterInformation(float x, float y) {
    Upscaler::settings.jitter[0] = x;
    Upscaler::settings.jitter[1] = y;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_ResetHistory() {
    Upscaler::settings.resetHistory = true;
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::UpscalerStatus UNITY_INTERFACE_API Upscaler_Prepare(
  void                          *nativeDepthBuffer,
  UnityRenderingExtTextureFormat unityDepthFormat,
  void                          *nativeMotionVectors,
  UnityRenderingExtTextureFormat unityMotionVectorFormat,
  void                          *nativeInColor,
  UnityRenderingExtTextureFormat unityInColorFormat,
  void                          *nativeOutColor,
  UnityRenderingExtTextureFormat unityOutColorFormat
) {
    Upscaler::get()->setImageResources(
                       nativeDepthBuffer,
                       unityDepthFormat,
                       nativeMotionVectors,
                       unityMotionVectorFormat,
                       nativeInColor,
                       unityInColorFormat,
                       nativeOutColor,
                       unityOutColorFormat
                     );

    return Upscaler::get()->createFeature();
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_Shutdown() {
    Upscaler::get()->shutdown();
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_ShutdownPlugin() {
    // Finish all one time submits
    GraphicsAPI::get()->finishOneTimeSubmits();
    // Clean up
    for (Upscaler *upscaler : Upscaler::getAllUpscalers()) upscaler->shutdown();
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces *t_unityInterfaces) {
//    bool debuggerConnected;
//    while (!debuggerConnected);
    // Enabled plugin's interception of Vulkan initialization calls.
    for (GraphicsAPI *graphicsAPI : GraphicsAPI::getAllGraphicsAPIs())
        graphicsAPI->useUnityInterfaces(t_unityInterfaces);
    // Record graphics interface for future use.
    Unity::graphicsInterface = t_unityInterfaces->Get<IUnityGraphics>();
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginUnload() {
    Upscaler_ShutdownPlugin();
    // Remove vulkan initialization interception
    Vulkan::RemoveInterceptInitialization();
}
