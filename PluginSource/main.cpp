// Project
#include "GraphicsAPI/NoGraphicsAPI.hpp"
#include "Logger.hpp"
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

#ifndef NDEBUG
// Usage: Insert this where the debugger should connect. Execute. Connect the debugger. Set 'debuggerConnected' to
// true. Step.
// Use 'handle SIGXCPU SIGPWR SIG35 SIG36 SIG37 nostop noprint' to prevent Unity's signals.
#    define WAIT_FOR_DEBUGGER               \
        {                                   \
            bool debuggerConnected = false; \
            while (!debuggerConnected)      \
                ;                           \
        }
#else
#    define WAIT_FOR_DEBUGGER
#endif

namespace Unity {
IUnityGraphics *graphicsInterface;
}  // namespace Unity

enum Event {
    BEFORE_POSTPROCESSING,
};

void INTERNAL_BeforePostProcessing() {
    Upscaler::get()->evaluate();
}

void UNITY_INTERFACE_API Upscaler_RenderingEventCallback(Event event) {
    switch (event) {
        case BEFORE_POSTPROCESSING: INTERNAL_BeforePostProcessing(); break;
    }
}

extern "C" UNITY_INTERFACE_EXPORT void *UNITY_INTERFACE_API Upscaler_GetRenderingEventCallback() {
    return reinterpret_cast<void *>(&Upscaler_RenderingEventCallback);
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API
Upscaler_InitializePlugin(void (*t_debugFunction)(const char *)) {
    Logger::setLoggerCallback(t_debugFunction);
    Logger::flush();

    GraphicsAPI::set(Unity::graphicsInterface->GetRenderer());
    GraphicsAPI::get()->prepareForOneTimeSubmits();
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API Upscaler_Set(Upscaler::Type type) {
    Upscaler::get()->shutdown();
    Upscaler::set(type);
    return Upscaler::get()->initialize();
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API Upscaler_IsSupported(Upscaler::Type type) {
    return Upscaler::get(type)->isSupported();
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API Upscaler_IsCurrentlyAvailable() {
    return Upscaler::get()->isSupported();
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API Upscaler_IsAvailable(Upscaler::Type type) {
    return Upscaler::get(type)->isAvailable();
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API Upscaler_Initialize() {
    Upscaler::get()->initialize();
    return Upscaler::get()->isSupported();
}

extern "C" UNITY_INTERFACE_EXPORT uint64_t UNITY_INTERFACE_API
Upscaler_ResizeTargets(unsigned int t_width, unsigned int t_height) {
    Logger::log("Resizing up-scaling targets: " + std::to_string(t_width) + "x" + std::to_string(t_height));

    if (!Upscaler::get()->isSupported()) return 0;
    Upscaler::settings = Upscaler::get()->getOptimalSettings({t_width, t_height});

    return Upscaler::settings.inputResolution.asLong();
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API Upscaler_Prepare(
  void                          *nativeDepthBuffer,
  UnityRenderingExtTextureFormat unityDepthFormat,
  void                          *nativeMotionVectors,
  UnityRenderingExtTextureFormat unityMotionVectorFormat,
  void                          *nativeInColor,
  UnityRenderingExtTextureFormat unityInColorFormat,
  void                          *nativeOutColor,
  UnityRenderingExtTextureFormat unityOutColorFormat
) {
    bool available = Upscaler::get()->isAvailableAfter(Upscaler::get()->setImageResources(
      nativeDepthBuffer,
      unityDepthFormat,
      nativeMotionVectors,
      unityMotionVectorFormat,
      nativeInColor,
      unityInColorFormat,
      nativeOutColor,
      unityOutColorFormat
    ));

    Upscaler::get()->createFeature();
    return available;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API Upscaler_SetJitterInformation(float x, float y) {
    Upscaler::settings.jitter[0] = x;
    Upscaler::settings.jitter[1] = y;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces *t_unityInterfaces) {
    // Enabled plugin's interception of Vulkan initialization calls.
    for (GraphicsAPI *graphicsAPI : GraphicsAPI::getAllGraphicsAPIs())
        graphicsAPI->useUnityInterfaces(t_unityInterfaces);
    // Record graphics interface for future use.
    Unity::graphicsInterface = t_unityInterfaces->Get<IUnityGraphics>();
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginUnload() {
    // Finish all one time submits
    GraphicsAPI::get()->finishOneTimeSubmits();
    // Clean up
    for (Upscaler *upscaler : Upscaler::getAllUpscalers()) upscaler->shutdown();
    // Remove vulkan initialization interception
    Vulkan::RemoveInterceptInitialization();
}
