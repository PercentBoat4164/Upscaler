// Project
#include "GraphicsAPI/Vulkan.hpp"

#include <Upscaler/DLSS.hpp>

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
IUnityInterfaces *interfaces;
IUnityGraphics   *graphicsInterface;
}  // namespace Unity

extern "C" UNITY_INTERFACE_EXPORT uint64_t OnFramebufferResize(unsigned int t_width, unsigned int t_height) {
    Logger::log("Resizing up-scaling targets: " + std::to_string(t_width) + "x" + std::to_string(t_height));

    if (!Upscaler::get()->isSupported()) return 0;
    Upscaler::settings = Upscaler::get()->getOptimalSettings({t_width, t_height});
    Upscaler::get()->createFeature();

    return Upscaler::settings.renderResolution.asLong();
}

extern "C" UNITY_INTERFACE_EXPORT void InitializePlugin() {
    UnityGfxRenderer renderer = Unity::graphicsInterface->GetRenderer();
    switch (renderer) {
        case kUnityGfxRendererVulkan:
            GraphicsAPI::set<Vulkan>();
            break;
        default: GraphicsAPI::set(GraphicsAPI::NONE); break;
    }

    if (GraphicsAPI::get() == nullptr)
        return;
    GraphicsAPI::get()->prepareForOneTimeSubmits();
}

extern "C" UNITY_INTERFACE_EXPORT void EvaluateDLSS() {
    Upscaler::get()->evaluate();
}

extern "C" UNITY_INTERFACE_EXPORT bool initializeDLSS() {
    Upscaler::get()->initialize();
    return Upscaler::get()->isSupported();
}

extern "C" UNITY_INTERFACE_EXPORT void SetDebugCallback(void (*t_debugFunction)(const char *)) {
    Logger::setLoggerCallback(t_debugFunction);
    Logger::flush();
}

//extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType
//) {
//    switch (eventType) {
//        case kUnityGfxDeviceEventInitialize: {
//            UnityGfxRenderer renderer = Unity::graphicsInterface->GetRenderer();
//            if (renderer == kUnityGfxRendererNull) return;  // Is being run before an API has been selected
//            else {
//                Upscaler::disableAllUpscalers();
//            }
//            break;
//        }
//        case kUnityGfxDeviceEventShutdown: {
//            Unity::graphicsInterface->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
//        }
//        default: break;
//    }
//}

extern "C" UNITY_INTERFACE_EXPORT bool IsDLSSSupported() {
    return Upscaler::get<DLSS>()->isSupported();
}

extern "C" UNITY_INTERFACE_EXPORT void
setDepthBuffer(void *nativeBuffer, UnityRenderingExtTextureFormat unityFormat) {
    Upscaler::get()->isAvailableAfter(Upscaler::get()->setDepthBuffer(nativeBuffer, unityFormat));
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces *t_unityInterfaces) {
//    WAIT_FOR_DEBUGGER
    Unity::interfaces = t_unityInterfaces;
    if (!Vulkan::interceptInitialization(t_unityInterfaces->Get<IUnityGraphicsVulkanV2>())) {
        Logger::log("DLSS Plugin failed to intercept initialization.");
        Upscaler::disableAllUpscalers();
        return;
    }
    Unity::graphicsInterface = t_unityInterfaces->Get<IUnityGraphics>();
//    Unity::graphicsInterface->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
//    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
    Upscaler::set(Upscaler::DLSS);
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginUnload() {
    // Finish all one time submits
    GraphicsAPI::get()->finishOneTimeSubmits();
    // Clean up
    for (Upscaler *upscaler : Upscaler::getAllUpscalers()) upscaler->shutdown();
    // Remove vulkan initialization interception
    Vulkan::RemoveInterceptInitialization();
    // Remove the device event callback
//    Unity::graphicsInterface->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
    // Perform shutdown graphics event
//    OnGraphicsDeviceEvent(kUnityGfxDeviceEventShutdown);
}
