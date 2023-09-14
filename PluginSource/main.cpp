// Project
#include "GraphicsAPI/NoGraphicsAPI.hpp"
#include "GraphicsAPI/DX12.hpp"
#include "GraphicsAPI/Vulkan.hpp"
#include "Upscaler/DLSS.hpp"
#include "Upscaler/NoUpscaler.hpp"

// Unity
#include <IUnityGraphicsD3D12.h>
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
IUnityGraphics   *graphicsInterface;
}  // namespace Unity

extern "C" UNITY_INTERFACE_EXPORT void Upscaler_InitializePlugin(void (*t_debugFunction)(const char *)) {
    Logger::setLoggerCallback(t_debugFunction);
    Logger::flush();

    UnityGfxRenderer renderer = Unity::graphicsInterface->GetRenderer();
    switch (renderer) {
        case kUnityGfxRendererVulkan: GraphicsAPI::set<Vulkan>(); break;
        case kUnityGfxRendererD3D12: GraphicsAPI::set<DX12>(); break;
        default: GraphicsAPI::set<NoGraphicsAPI>(); break;
    }
    GraphicsAPI::get()->prepareForOneTimeSubmits();
}

extern "C" UNITY_INTERFACE_EXPORT bool Upscaler_Set(Upscaler::Type type) {
    Upscaler::get()->shutdown();
    Upscaler::set(type);
    return Upscaler::get()->initialize();
}

extern "C" UNITY_INTERFACE_EXPORT bool Upscaler_IsSupported(Upscaler::Type type) {
    return Upscaler::get(type)->isSupported();
}

extern "C" UNITY_INTERFACE_EXPORT bool Upscaler_IsCurrentlyAvailable() {
    return Upscaler::get()->isSupported();
}

extern "C" UNITY_INTERFACE_EXPORT bool Upscaler_IsAvailable(Upscaler::Type type) {
    return Upscaler::get(type)->isAvailable();
}

extern "C" UNITY_INTERFACE_EXPORT bool Upscaler_Initialize() {
    Upscaler::get()->initialize();
    return Upscaler::get()->isSupported();
}

extern "C" UNITY_INTERFACE_EXPORT uint64_t Upscaler_ResizeTargets(unsigned int t_width, unsigned int t_height) {
    Logger::log("Resizing up-scaling targets: " + std::to_string(t_width) + "x" + std::to_string(t_height));

    if (!Upscaler::get()->isSupported()) return 0;
    Upscaler::settings = Upscaler::get()->getOptimalSettings({t_width, t_height});

    return Upscaler::settings.renderResolution.asLong();
}

extern "C" UNITY_INTERFACE_EXPORT bool
Upscaler_Prepare(void *nativeBuffer, UnityRenderingExtTextureFormat unityFormat) {
    bool available = Upscaler::get()->isAvailableAfter(Upscaler::get()->setDepthBuffer(nativeBuffer, unityFormat));

    Upscaler::get()->createFeature();
    return available;
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
