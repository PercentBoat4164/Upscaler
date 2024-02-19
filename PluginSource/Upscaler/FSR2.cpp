#include "FSR2.hpp"
#ifdef ENABLE_FSR2
#    ifdef ENABLE_VULKAN
#        include "GraphicsAPI/Vulkan.hpp"
#    endif

#    include <algorithm>
#    include <ffx_fsr2_vk.h>

Upscaler::Status (FSR2::*FSR2::graphicsAPIIndependentInitializeFunctionPointer)(){&FSR2::safeFail};
Upscaler::Status (FSR2::*FSR2::graphicsAPIIndependentSetDepthBufferFunctionPointer)(
  void *,
  UnityRenderingExtTextureFormat
){&FSR2::safeFail};
Upscaler::Status (FSR2::*FSR2::graphicsAPIIndependentSetInputColorFunctionPointer)(
  void *,
  UnityRenderingExtTextureFormat
){&FSR2::safeFail};
Upscaler::Status (FSR2::*FSR2::graphicsAPIIndependentSetMotionVectorsFunctionPointer)(
  void *,
  UnityRenderingExtTextureFormat
){&FSR2::safeFail};
Upscaler::Status (FSR2::*FSR2::graphicsAPIIndependentSetOutputColorFunctionPointer)(
  void *,
  UnityRenderingExtTextureFormat
){&FSR2::safeFail};
Upscaler::Status (FSR2::*FSR2::graphicsAPIIndependentEvaluateFunctionPointer)(){&FSR2::safeFail};
Upscaler::Status (FSR2::*FSR2::graphicsAPIIndependentReleaseFunctionPointer)(){&FSR2::safeFail};
Upscaler::Status (FSR2::*FSR2::graphicsAPIIndependentShutdownFunctionPointer)(){&FSR2::safeFail};

#    ifdef ENABLE_VULKAN
Upscaler::Status FSR2::VulkanInitialize() {
    VkPhysicalDevice physicalDevice = GraphicsAPI::get<Vulkan>()->getUnityInterface()->Instance().physicalDevice;
    size_t           bufferSize     = ffxFsr2GetScratchMemorySizeVK(physicalDevice);
    return setStatus(ffxFsr2GetInterfaceVK(
      &interface,
      calloc(bufferSize, 1),
      bufferSize,
      physicalDevice,
      Vulkan::getVkGetDeviceProcAddr()
    ), "Upscaler is unable to get the FSR2 interface.");
}

Upscaler::Status FSR2::VulkanSetDepthBuffer(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) {
    if (failure(setStatusIf(
          nativeHandle == VK_NULL_HANDLE,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given output color image being `VK_NULL_HANDLE`."
        ))) {
        return getStatus();
    }

    if (nativeHandle == VK_NULL_HANDLE) return getStatus();
    VkImage        image  = *static_cast<VkImage *>(nativeHandle);
    const VkFormat format = Vulkan::getFormat(unityFormat);
    VkImageView    view   = GraphicsAPI::get<Vulkan>()->createImageView(image, format, VK_IMAGE_ASPECT_DEPTH_BIT);

    if (failure(setStatusIf(
          view == VK_NULL_HANDLE,
          SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING,
          "Refused to set image resources due to an attempt to create the output color image view resulting in a "
          "`VK_NULL_HANDLE` view handle."
        ))) {
        return getStatus();
    }

    depth = ffxGetTextureResourceVK(
      &context,
      image,
      view,
      settings.recommendedInputResolution.width,
      settings.recommendedInputResolution.height,
      format,
      L"Depth Buffer",
      FFX_RESOURCE_STATE_COMPUTE_READ
    );
    return getStatus();
}

Upscaler::Status FSR2::VulkanSetInputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) {
    if (failure(setStatusIf(
          nativeHandle == VK_NULL_HANDLE,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given input color image being `VK_NULL_HANDLE`."
        ))) {
        return getStatus();
    }

    if (nativeHandle == VK_NULL_HANDLE) return getStatus();
    VkImage        image  = *static_cast<VkImage *>(nativeHandle);
    const VkFormat format = Vulkan::getFormat(unityFormat);
    VkImageView    view   = GraphicsAPI::get<Vulkan>()->createImageView(image, format, VK_IMAGE_ASPECT_COLOR_BIT);

    if (failure(setStatusIf(
          view == VK_NULL_HANDLE,
          SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING,
          "Refused to set image resources due to an attempt to create the input color image view resulting in a "
          "`VK_NULL_HANDLE` view handle."
        ))) {
        return getStatus();
    }

    inColor = ffxGetTextureResourceVK(
      &context,
      image,
      view,
      settings.recommendedInputResolution.width,
      settings.recommendedInputResolution.height,
      format,
      L"Input Color",
      FFX_RESOURCE_STATE_COMPUTE_READ
    );
    return getStatus();
}

Upscaler::Status FSR2::VulkanSetMotionVectors(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) {
    if (failure(setStatusIf(
          nativeHandle == VK_NULL_HANDLE,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given motion vector image being `VK_NULL_HANDLE`."
        ))) {
        return getStatus();
    }

    if (nativeHandle == VK_NULL_HANDLE) return getStatus();
    VkImage        image  = *static_cast<VkImage *>(nativeHandle);
    const VkFormat format = Vulkan::getFormat(unityFormat);
    VkImageView    view   = GraphicsAPI::get<Vulkan>()->createImageView(image, format, VK_IMAGE_ASPECT_COLOR_BIT);

    if (failure(setStatusIf(
          view == VK_NULL_HANDLE,
          SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING,
          "Refused to set image resources due to an attempt to create the motion vector image view resulting in a "
          "`VK_NULL_HANDLE` view handle."
        ))) {
        return getStatus();
    }

    motionVectors = ffxGetTextureResourceVK(
      &context,
      image,
      view,
      settings.recommendedInputResolution.width,
      settings.recommendedInputResolution.height,
      format,
      L"Motion Vectors",
      FFX_RESOURCE_STATE_COMPUTE_READ
    );
    return getStatus();
}

Upscaler::Status FSR2::VulkanSetOutputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) {
    if (failure(setStatusIf(
          nativeHandle == VK_NULL_HANDLE,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given output color image being `VK_NULL_HANDLE`."
        ))) {
        return getStatus();
    }

    if (nativeHandle == VK_NULL_HANDLE) return getStatus();
    VkImage        image  = *static_cast<VkImage *>(nativeHandle);
    const VkFormat format = Vulkan::getFormat(unityFormat);
    VkImageView    view   = GraphicsAPI::get<Vulkan>()->createImageView(image, format, VK_IMAGE_ASPECT_COLOR_BIT);

    if (failure(setStatusIf(
          view == VK_NULL_HANDLE,
          SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING,
          "Refused to set image resources due to an attempt to create the output color image view resulting in a "
          "`VK_NULL_HANDLE` view handle."
        ))) {
        return getStatus();
    }

    outColor = ffxGetTextureResourceVK(
      &context,
      image,
      view,
      settings.outputResolution.width,
      settings.outputResolution.height,
      format,
      L"Output Color",
      FFX_RESOURCE_STATE_COMPUTE_READ
    );
    return getStatus();
}

Upscaler::Status FSR2::VulkanEvaluate() {
    UnityVulkanRecordingState state{};
    GraphicsAPI::get<Vulkan>()->getUnityInterface()->EnsureInsideRenderPass();
    GraphicsAPI::get<Vulkan>()->getUnityInterface()->CommandRecordingState(
      &state,
      kUnityVulkanGraphicsQueueAccess_DontCare
    );

    FfxFsr2DispatchDescription dispatchDescription{
      .commandList   = ffxGetCommandListVK(state.commandBuffer),
      .color         = inColor,
      .depth         = depth,
      .motionVectors = motionVectors,
      .exposure = ffxGetTextureResourceVK(&context, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, VK_FORMAT_UNDEFINED, nullptr, FFX_RESOURCE_STATE_COMPUTE_READ),
      .reactive = ffxGetTextureResourceVK(&context, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, VK_FORMAT_UNDEFINED, nullptr, FFX_RESOURCE_STATE_COMPUTE_READ),
      .transparencyAndComposition = ffxGetTextureResourceVK(&context, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, VK_FORMAT_UNDEFINED, nullptr, FFX_RESOURCE_STATE_COMPUTE_READ),
      .output        = outColor,
      .jitterOffset  = {settings.jitter[0],                                             settings.jitter[1]                        },
      .motionVectorScale =
        {-static_cast<float>(settings.recommendedInputResolution.width),
                        -static_cast<float>(settings.recommendedInputResolution.height)                                           },
      .renderSize       = {settings.recommendedInputResolution.width,                      settings.recommendedInputResolution.height},
      .enableSharpening = settings.sharpness > 0,
      .sharpness        = settings.sharpness,
      .frameTimeDelta = settings.frameTime,
      .reset            = settings.resetHistory,
      .cameraNear       = settings.camera.farPlane,
      .cameraFar = settings.camera.nearPlane,  // Switched because depth is inverted
      .cameraFovAngleVertical = settings.camera.verticalFOV,
      .viewSpaceToMetersFactor = 1.F,
    };

    return setStatus(ffxFsr2ContextDispatch(&context, &dispatchDescription), "Failed to dispatch FSR2.");
}

Upscaler::Status FSR2::VulkanRelease() {
    return Upscaler::SETTINGS_ERROR_UPSCALER_NOT_AVAILABLE;
}

Upscaler::Status FSR2::VulkanShutdown() {
    return Upscaler::SETTINGS_ERROR_UPSCALER_NOT_AVAILABLE;
}
#    endif

void FSR2::setFunctionPointers(const GraphicsAPI::Type graphicsAPI) {
    switch (graphicsAPI) {
        case GraphicsAPI::NONE: {
            graphicsAPIIndependentInitializeFunctionPointer       = &FSR2::safeFail;
            graphicsAPIIndependentSetDepthBufferFunctionPointer   = &FSR2::safeFail;
            graphicsAPIIndependentSetInputColorFunctionPointer    = &FSR2::safeFail;
            graphicsAPIIndependentSetMotionVectorsFunctionPointer = &FSR2::safeFail;
            graphicsAPIIndependentSetOutputColorFunctionPointer   = &FSR2::safeFail;
            graphicsAPIIndependentEvaluateFunctionPointer         = &FSR2::safeFail;
            graphicsAPIIndependentReleaseFunctionPointer          = &FSR2::safeFail;
            graphicsAPIIndependentShutdownFunctionPointer         = &FSR2::safeFail;
            break;
        }
#    ifdef ENABLE_VULKAN
        case GraphicsAPI::VULKAN: {
            graphicsAPIIndependentInitializeFunctionPointer       = &FSR2::VulkanInitialize;
            graphicsAPIIndependentSetDepthBufferFunctionPointer   = &FSR2::VulkanSetDepthBuffer;
            graphicsAPIIndependentSetInputColorFunctionPointer    = &FSR2::VulkanSetInputColor;
            graphicsAPIIndependentSetMotionVectorsFunctionPointer = &FSR2::VulkanSetMotionVectors;
            graphicsAPIIndependentSetOutputColorFunctionPointer   = &FSR2::VulkanSetOutputColor;
            graphicsAPIIndependentEvaluateFunctionPointer         = &FSR2::VulkanEvaluate;
            graphicsAPIIndependentReleaseFunctionPointer          = &FSR2::VulkanRelease;
            graphicsAPIIndependentShutdownFunctionPointer         = &FSR2::VulkanShutdown;
            break;
        }
#    endif
#    ifdef ENABLE_DX12
        case GraphicsAPI::DX12: {
            graphicsAPIIndependentInitializeFunctionPointer       = &FSR2::DX12Initialize;
            graphicsAPIIndependentCreateFunctionPointer           = &FSR2::DX12CreateFeature;
            graphicsAPIIndependentSetDepthBufferFunctionPointer   = &FSR2::DX12SetDepthBuffer;
            graphicsAPIIndependentSetInputColorFunctionPointer    = &FSR2::DX12SetInputColor;
            graphicsAPIIndependentSetMotionVectorsFunctionPointer = &FSR2::DX12SetMotionVectors;
            graphicsAPIIndependentSetOutputColorFunctionPointer   = &FSR2::DX12SetOutputColor;
            graphicsAPIIndependentEvaluateFunctionPointer         = &FSR2::DX12Evaluate;
            graphicsAPIIndependentReleaseFunctionPointer          = &FSR2::DX12ReleaseFeature;
            graphicsAPIIndependentShutdownFunctionPointer         = &FSR2::DX12Shutdown;
            break;
        }
#    endif
#    ifdef ENABLE_DX11
        case GraphicsAPI::DX11: {
            graphicsAPIIndependentInitializeFunctionPointer       = &FSR2::DX11Initialize;
            graphicsAPIIndependentCreateFunctionPointer           = &FSR2::DX11CreateFeature;
            graphicsAPIIndependentSetDepthBufferFunctionPointer   = &FSR2::DX11SetDepthBuffer;
            graphicsAPIIndependentSetInputColorFunctionPointer    = &FSR2::DX11SetInputColor;
            graphicsAPIIndependentSetMotionVectorsFunctionPointer = &FSR2::DX11SetMotionVectors;
            graphicsAPIIndependentSetOutputColorFunctionPointer   = &FSR2::DX11SetOutputColor;
            graphicsAPIIndependentEvaluateFunctionPointer         = &FSR2::DX11Evaluate;
            graphicsAPIIndependentReleaseFunctionPointer          = &FSR2::DX11ReleaseFeature;
            graphicsAPIIndependentShutdownFunctionPointer         = &FSR2::DX11Shutdown;
            break;
        }
#    endif
        default: {
            graphicsAPIIndependentInitializeFunctionPointer       = &FSR2::safeFail;
            graphicsAPIIndependentSetDepthBufferFunctionPointer   = &FSR2::safeFail;
            graphicsAPIIndependentSetInputColorFunctionPointer    = &FSR2::safeFail;
            graphicsAPIIndependentSetMotionVectorsFunctionPointer = &FSR2::safeFail;
            graphicsAPIIndependentSetOutputColorFunctionPointer   = &FSR2::safeFail;
            graphicsAPIIndependentEvaluateFunctionPointer         = &FSR2::safeFail;
            graphicsAPIIndependentReleaseFunctionPointer          = &FSR2::safeFail;
            graphicsAPIIndependentShutdownFunctionPointer         = &FSR2::safeFail;
            break;
        }
    }
}

void FSR2::log(FfxFsr2MsgType type, const wchar_t *message) {
    std::wstring msg(message);
    std::string fullMessage(msg.length(), 0);
    std::ranges::transform(fullMessage, msg.begin(), [](const wchar_t c) { return static_cast<char>(c); });
    switch (type) {
        case FFX_FSR2_MESSAGE_TYPE_ERROR: fullMessage =   "FSR2 Error ---> " + fullMessage; break;
        case FFX_FSR2_MESSAGE_TYPE_WARNING: fullMessage = "FSR2 Warning -> " + fullMessage; break;
        case FFX_FSR2_MESSAGE_TYPE_COUNT: break;
    }
    if (Upscaler::log != nullptr) Upscaler::log(fullMessage.c_str());
}

Upscaler::Status FSR2::setStatus(FfxErrorCode t_error, const std::string &t_msg) {
    switch (t_error) {
        case FFX_OK: return Upscaler::setStatus(SUCCESS, t_msg + " | FFX_OK");
        case FFX_ERROR_INVALID_POINTER:
            return Upscaler::setStatus(
              SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
              t_msg + " | FFX_ERROR_INVALID_POINTER"
            );
        case FFX_ERROR_INVALID_ALIGNMENT:
            return Upscaler::setStatus(
              SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR,
              t_msg + " | FFX_ERROR_INVALID_ALIGNMENT"
            );
        case FFX_ERROR_INVALID_SIZE:
            return Upscaler::setStatus(
              SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR,
              t_msg + " | FFX_ERROR_INVALID_SIZE"
            );
        case FFX_EOF: return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING, t_msg + " | FFX_EOF");
        case FFX_ERROR_INVALID_PATH:
            return Upscaler::setStatus(
              SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR,
              t_msg + " | FFX_ERROR_INVALID_PATH"
            );
        case FFX_ERROR_EOF:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_EOF");
        case FFX_ERROR_MALFORMED_DATA:
            return Upscaler::setStatus(
              SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR,
              t_msg + " | FFX_ERROR_MALFORMED_DATA"
            );
        case FFX_ERROR_OUT_OF_MEMORY:
            return Upscaler::setStatus(SOFTWARE_ERROR_OUT_OF_GPU_MEMORY, t_msg + " | FFX_ERROR_OUT_OF_MEMORY");
        case FFX_ERROR_INCOMPLETE_INTERFACE:
            return Upscaler::setStatus(
              SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR,
              t_msg + " | FFX_ERROR_INCOMPLETE_INTERFACE"
            );
        case FFX_ERROR_INVALID_ENUM:
            return Upscaler::setStatus(
              SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR,
              t_msg + " | FFX_ERROR_INVALID_ENUM"
            );
        case FFX_ERROR_INVALID_ARGUMENT:
            return Upscaler::setStatus(
              SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR,
              t_msg + " | FFX_ERROR_INVALID_ARGUMENT"
            );
        case FFX_ERROR_OUT_OF_RANGE:
            return Upscaler::setStatus(
              SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR,
              t_msg + " | FFX_ERROR_OUT_OF_RANGE"
            );
        case FFX_ERROR_NULL_DEVICE:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_NULL_DEVICE");
        case FFX_ERROR_BACKEND_API_ERROR:
            return Upscaler::setStatus(
              SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR,
              t_msg + " | FFX_ERROR_BACKEND_API_ERROR"
            );
        case FFX_ERROR_INSUFFICIENT_MEMORY:
            return Upscaler::setStatus(
              SOFTWARE_ERROR_OUT_OF_GPU_MEMORY,
              t_msg + " | FFX_ERROR_INSUFFICIENT_MEMORY"
            );
        default: return Upscaler::setStatus(GENERIC_ERROR, t_msg + " | Unknown");
    }
}

FSR2 *FSR2::get() {
    static FSR2 *fsr2{new FSR2};
    return fsr2;
}

Upscaler::Type FSR2::getType() {
    return Upscaler::FSR2;
}

std::string FSR2::getName() {
    return "AMD FidelityFX FSR";
}

#    ifdef ENABLE_VULKAN
std::vector<std::string> FSR2::getRequiredVulkanInstanceExtensions() {
    return {};
}

std::vector<std::string>
FSR2::getRequiredVulkanDeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice) {
    return {};
}
#    endif

Upscaler::Settings
FSR2::getOptimalSettings(const Settings::Resolution resolution, const Settings::QualityMode mode, const bool hdr) {
  Settings optimalSettings = settings;
  optimalSettings.outputResolution = resolution;
  optimalSettings.HDR = hdr;
  optimalSettings.quality = mode;

  setStatus(ffxFsr2GetRenderResolutionFromQualityMode(
    &optimalSettings.recommendedInputResolution.width,
    &optimalSettings.recommendedInputResolution.height,
    optimalSettings.outputResolution.width,
    optimalSettings.outputResolution.height,
    optimalSettings.getQuality<Upscaler::FSR2>()
  ),
    "Some invalid setting was set. Ensure that the sharpness is between 0F and 1F, and that the QualityMode setting is a valid enum value."
  );

  return optimalSettings;
}

Upscaler::Status FSR2::initialize() {
    return (this->*graphicsAPIIndependentInitializeFunctionPointer)();
}

Upscaler::Status FSR2::create() {
    description.maxRenderSize = {
      settings.outputResolution.width,
      settings.outputResolution.height
    };
    description.displaySize = {settings.outputResolution.width, settings.outputResolution.height};
    description.flags       = static_cast<unsigned>(FFX_FSR2_ENABLE_AUTO_EXPOSURE) |
      static_cast<unsigned>(FFX_FSR2_ENABLE_DEPTH_INVERTED) |
      static_cast<unsigned>(FFX_FSR2_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION) |
      (settings.HDR ? FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE : 0U);
#    ifndef NDEBUG
     description.flags |= FFX_FSR2_ENABLE_DEBUG_CHECKING;
     description.fpMessage = &FSR2::log;
#    else
     description.flags &= ~static_cast<unsigned>(FFX_FSR2_ENABLE_DEBUG_CHECKING);
     description.fpMessage = nullptr;
#    endif
     description.callbacks = interface;
     description.device = ffxGetDeviceVK(GraphicsAPI::get<Vulkan>()->getUnityInterface()->Instance().device);
     return setStatus(ffxFsr2ContextCreate(&context, &description), "Failed to create the FSR2 context.");
}

Upscaler::Status FSR2::setDepthBuffer(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) {
    return (this->*graphicsAPIIndependentSetDepthBufferFunctionPointer)(nativeHandle, unityFormat);
}

Upscaler::Status FSR2::setInputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) {
    return (this->*graphicsAPIIndependentSetInputColorFunctionPointer)(nativeHandle, unityFormat);
}

Upscaler::Status FSR2::setMotionVectors(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) {
    return (this->*graphicsAPIIndependentSetMotionVectorsFunctionPointer)(nativeHandle, unityFormat);
}

Upscaler::Status FSR2::setOutputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) {
    return (this->*graphicsAPIIndependentSetOutputColorFunctionPointer)(nativeHandle, unityFormat);
}

Upscaler::Status FSR2::evaluate() {
    return (this->*graphicsAPIIndependentEvaluateFunctionPointer)();
}

Upscaler::Status FSR2::release() {
    return (this->*graphicsAPIIndependentReleaseFunctionPointer)();
}
#endif