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

extern "C" UNITY_INTERFACE_EXPORT void OnFramebufferResize(unsigned int t_width, unsigned int t_height) {
    Logger::log("Resizing DLSS targets: " + std::to_string(t_width) + "x" + std::to_string(t_height));

    if (!Plugin::DLSSSupported) return;

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
        return;
    }
    Plugin::endOneTimeSubmitRecording();
}

extern "C" UNITY_INTERFACE_EXPORT void PrepareDLSS(VkImage t_depthBuffer) {
    VkImageView imageView{nullptr};

    // clang-format off
    VkImageViewCreateInfo createInfo{
      .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext    = nullptr,
      .flags    = 0,
      .image    = t_depthBuffer,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format   = VK_FORMAT_D24_UNORM_S8_UINT,
      .components = {
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
      },
      .subresourceRange = {
        .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
      },
    };
    // clang-format on

    VkResult result = GraphicsAPI::Vulkan::get(GraphicsAPI::Vulkan::getVulkanInterface()->Instance().device)
                        .vkCreateImageView(&createInfo, nullptr, &imageView);
    if (result == VK_SUCCESS) Logger::log("Created depth resource for DLSS.");


    // clang-format off
    Plugin::depthBufferResource = {
      .Resource = {
        .ImageViewInfo = {
          .ImageView        = imageView,
          .Image            = t_depthBuffer,
          .SubresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
          },
          .Format = VK_FORMAT_D24_UNORM_S8_UINT,
          .Width  = Plugin::Settings::renderResolution.width,
          .Height = Plugin::Settings::renderResolution.height,
        },
      },
      .Type      = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW,
      .ReadWrite = true,
    };

    Plugin::evalParameters = {
      .pInDepth                  = &Plugin::depthBufferResource,
      .pInMotionVectors          = nullptr,
      .InJitterOffsetX           = 0.F,
      .InJitterOffsetY           = 0.F,
      .InRenderSubrectDimensions = {
        .Width  = Plugin::Settings::renderResolution.width,
        .Height = Plugin::Settings::renderResolution.height,
      },
    };
    // clang-format on
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
    Upscaler::DLSS::initialize();
    return Upscaler::DLSS::staticSetIsSupported(true);
}

extern "C" UNITY_INTERFACE_EXPORT void SetDebugCallback(void (*t_debugFunction)(const char *)) {
    Logger::setLoggerCallback(t_debugFunction);
    Logger::flush();
}

extern "C" UNITY_INTERFACE_EXPORT void OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType) {
    switch (eventType) {
        case kUnityGfxDeviceEventInitialize: {
            UnityGfxRenderer renderer = Unity::graphicsInterface->GetRenderer();
            if (renderer == kUnityGfxRendererNull) break;
            if (renderer == kUnityGfxRendererVulkan) Plugin::DLSSSupported = initializeDLSS();
            break;
        }
        case kUnityGfxDeviceEventShutdown: {
            Unity::graphicsInterface->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
        }
        default: break;
    }
}

extern "C" UNITY_INTERFACE_EXPORT bool IsDLSSSupported() {
    return Plugin::DLSSSupported;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces *t_unityInterfaces) {
    Unity::interfaces = t_unityInterfaces;
    if (!GraphicsAPI::Vulkan::interceptInitialization(t_unityInterfaces->Get<IUnityGraphicsVulkanV2>())) {
        Logger::log("DLSS Plugin failed to intercept initialization.");
        Plugin::DLSSSupported = false;
        return;
    }
    Unity::graphicsInterface = t_unityInterfaces->Get<IUnityGraphics>();
    Unity::graphicsInterface->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
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
