#include "IUnityGraphics.h"
#include "IUnityGraphicsVulkan.h"
#include "IUnityInterface.h"

// DLSS requires the vulkan imports from Unity
#include "GraphicsAPI/Vulkan.hpp"
#include "Logger.hpp"
#include "Plugin.hpp"
#include "Upscaler/DLSS.hpp"

#include "nvsdk_ngx.h"
#include "nvsdk_ngx_defs.h"
#include "nvsdk_ngx_helpers.h"
#include "nvsdk_ngx_helpers_vk.h"
#include "nvsdk_ngx_params.h"
#include "nvsdk_ngx_vk.h"

#include <cstring>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>

#ifndef NDEBUG
// Usage: Insert this where the debugger should connect. Execute. Connect the debugger. Set 'debuggerConnected' to
// true. Step.
// Use 'handle SIGXCPU SIGPWR SIG35 SIG36 SIG37 nostop noprint' to prevent Unity's signals.
#    define WAIT_FOR_DEBUGGER               \
        {                                   \
            bool debuggerConnected = false; \
            while (!debuggerConnected); \
        }
#else
#    define WAIT_FOR_DEBUGGER
#endif

namespace Unity {
IUnityInterfaces *interfaces;
IUnityGraphics   *graphicsInterface;
}  // namespace Unity

extern "C" UNITY_INTERFACE_EXPORT uint64_t OnFramebufferResize(unsigned int t_width, unsigned int t_height) {
    Logger::log("Resizing DLSS targets: " + std::to_string(t_width) + "x" + std::to_string(t_height));

    if (!Upscaler::get()->isSupported()) return 0;

    // Release any previously existing feature
    if (Plugin::DLSS != nullptr) {
        NVSDK_NGX_VULKAN_ReleaseFeature(Plugin::DLSS);
        Plugin::DLSS = nullptr;
    }

    Plugin::Settings::setPresentResolution({t_width, t_height});
    Plugin::Settings::useOptimalSettings();
    NVSDK_NGX_DLSS_Create_Params DLSSCreateParams = Plugin::Settings::getDLSSCreateParams();

    NVSDK_NGX_Parameter_SetUI(Plugin::parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA, 1);
    NVSDK_NGX_Parameter_SetUI(Plugin::parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Quality, 1);
    NVSDK_NGX_Parameter_SetUI(Plugin::parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Balanced, 1);
    NVSDK_NGX_Parameter_SetUI(Plugin::parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Performance, 1);
    NVSDK_NGX_Parameter_SetUI(Plugin::parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_UltraPerformance, 1);

    GraphicsAPI::Vulkan::getVulkanInterface()->EnsureOutsideRenderPass();

    VkCommandBuffer  commandBuffer = Plugin::beginOneTimeSubmitRecording();
    NVSDK_NGX_Result result =
      NGX_VULKAN_CREATE_DLSS_EXT(commandBuffer, 1, 1, &Plugin::DLSS, Plugin::parameters, &DLSSCreateParams);
    Logger::log("Create DLSS feature", result);
    if (NVSDK_NGX_FAILED(result)) {
        Plugin::cancelOneTimeSubmitRecording();
        return 0;
    }
    Plugin::endOneTimeSubmitRecording();

    return (uint64_t)Plugin::Settings::renderResolution.width << 32U | Plugin::Settings::renderResolution.height;
}

extern "C" UNITY_INTERFACE_EXPORT void EvaluateDLSS() {
    UnityVulkanRecordingState state{};
    GraphicsAPI::Vulkan::getVulkanInterface()->EnsureInsideRenderPass();
    GraphicsAPI::Vulkan::getVulkanInterface()->CommandRecordingState(
      &state,
      kUnityVulkanGraphicsQueueAccess_DontCare
    );

    NVSDK_NGX_Result result =
      NGX_VULKAN_EVALUATE_DLSS_EXT(state.commandBuffer, Plugin::DLSS, Plugin::parameters, &Plugin::evalParameters);
    Logger::log("Evaluated DLSS feature", result);
}

extern "C" UNITY_INTERFACE_EXPORT bool initializeDLSS() {
    Upscaler::get()->initialize();
    return Upscaler::get()->setIsSupported(true);
}

extern "C" UNITY_INTERFACE_EXPORT void SetDebugCallback(void (*t_debugFunction)(const char *)) {
    Logger::setLoggerCallback(t_debugFunction);
    Logger::flush();
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType) {
    switch (eventType) {
        case kUnityGfxDeviceEventInitialize: {
            UnityGfxRenderer renderer = Unity::graphicsInterface->GetRenderer();
            if (renderer == kUnityGfxRendererNull) return;  // Run before API selected
            if (renderer == kUnityGfxRendererVulkan) Upscaler::get()->initialize();
            else Upscaler::disableAllUpscalers();
            break;
        }
        case kUnityGfxDeviceEventShutdown: {
            Unity::graphicsInterface->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
        }
        default: break;
    }
}

extern "C" UNITY_INTERFACE_EXPORT bool IsDLSSSupported() {
    return Upscaler::get<DLSS>()->isSupported();
}

extern "C" UNITY_INTERFACE_EXPORT void setDepthBuffer(VkImage *buffer, VkFormat format) {
    format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    VkImageViewCreateInfo createInfo {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0x0,
      .image = *buffer,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .components = {
         VK_COMPONENT_SWIZZLE_IDENTITY,
         VK_COMPONENT_SWIZZLE_IDENTITY,
         VK_COMPONENT_SWIZZLE_IDENTITY,
         VK_COMPONENT_SWIZZLE_IDENTITY,
      },
      .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
    };

    VkImageView view{};
    VkResult result = GraphicsAPI::Vulkan::get(GraphicsAPI::Vulkan::getVulkanInterface()->Instance().device).vkCreateImageView(&createInfo, nullptr, &view);
    if (result == VK_SUCCESS)
        Upscaler::get()->setDepthBuffer(*buffer, view);
    else
        Upscaler::get()->setIsAvailable(false);
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces *t_unityInterfaces) {
//    WAIT_FOR_DEBUGGER
    Unity::interfaces = t_unityInterfaces;
    if (!GraphicsAPI::Vulkan::interceptInitialization(t_unityInterfaces->Get<IUnityGraphicsVulkanV2>())) {
        Logger::log("DLSS Plugin failed to intercept initialization.");
        Upscaler::disableAllUpscalers();
        return;
    }
    Unity::graphicsInterface = t_unityInterfaces->Get<IUnityGraphics>();
    Unity::graphicsInterface->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
    Upscaler::set(Upscaler::DLSS);

}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginUnload() {
    // Finish all one time submits
    Plugin::finishOneTimeSubmits();
    // Clean up
    if (Plugin::parameters != nullptr) NVSDK_NGX_VULKAN_DestroyParameters(Plugin::parameters);
    // Release features
    if (Plugin::DLSS != nullptr) NVSDK_NGX_VULKAN_ReleaseFeature(Plugin::DLSS);
    Plugin::DLSS = nullptr;
    // Shutdown NGX
    NVSDK_NGX_VULKAN_Shutdown1(nullptr);
    // Remove vulkan initialization interception
    GraphicsAPI::Vulkan::RemoveInterceptInitialization();
    // Remove the device event callback
    Unity::graphicsInterface->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
    // Perform shutdown graphics event
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventShutdown);
}
