#include "FSR2.hpp"
#ifdef ENABLE_FSR2
#    ifdef ENABLE_VULKAN
#        include "GraphicsAPI/Vulkan.hpp"
#    endif

#    include <algorithm>
#    include <ffx_fsr2_vk.h>

Upscaler::Status (FSR2::*FSR2::fpInitialize)(){&FSR2::safeFail};
Upscaler::Status (FSR2::*FSR2::fpSetDepth)(void *, UnityRenderingExtTextureFormat){&FSR2::safeFail};
Upscaler::Status (FSR2::*FSR2::fpSetInputColor)(void *, UnityRenderingExtTextureFormat){&FSR2::safeFail};
Upscaler::Status (FSR2::*FSR2::fpSetMotionVectors)(void *, UnityRenderingExtTextureFormat){&FSR2::safeFail};
Upscaler::Status (FSR2::*FSR2::fpSetOutputColor)(void *, UnityRenderingExtTextureFormat){&FSR2::safeFail};
Upscaler::Status (FSR2::*FSR2::fpEvaluate)(){&FSR2::safeFail};
Upscaler::Status (FSR2::*FSR2::fpRelease)(){&FSR2::safeFail};
Upscaler::Status (FSR2::*FSR2::fpShutdown)(){&FSR2::safeFail};

#    ifdef ENABLE_VULKAN
Upscaler::Status FSR2::VulkanInitialize() {
    VkPhysicalDevice physicalDevice = GraphicsAPI::get<Vulkan>()->getUnityInterface()->Instance().physicalDevice;
    size_t           bufferSize     = ffxFsr2GetScratchMemorySizeVK(physicalDevice);
    RETURN_ON_FAILURE(setStatusIf(bufferSize == 0, SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, getName() + " does not work in this environment."));
    void *buffer = calloc(bufferSize, 1);
    RETURN_ON_FAILURE(setStatusIf(buffer == nullptr, SOFTWARE_ERROR_OUT_OF_SYSTEM_MEMORY, getName() + " requires at least " + std::to_string(bufferSize) + " of contiguous system memory."));
    RETURN_ON_FAILURE(setStatus(ffxFsr2GetInterfaceVK(&interface, buffer, bufferSize, physicalDevice, Vulkan::getVkGetDeviceProcAddr()), "Upscaler is unable to get the FSR2 interface."));
    return SUCCESS;
}

Upscaler::Status FSR2::VulkanSetDepth(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) {
    RETURN_ON_FAILURE(setStatusIf(nativeHandle == VK_NULL_HANDLE, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Refused to set image resources due to the given output color image being `VK_NULL_HANDLE`."));

    VkImage        image  = *static_cast<VkImage *>(nativeHandle);
    const VkFormat format = Vulkan::getFormat(unityFormat);
    VkImageView    view   = GraphicsAPI::get<Vulkan>()->createImageView(image, format, VK_IMAGE_ASPECT_DEPTH_BIT);

    RETURN_ON_FAILURE(setStatusIf(view == VK_NULL_HANDLE, SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING, "Refused to set image resources due to an attempt to create the output color image view resulting in a `VK_NULL_HANDLE` view handle."));

    depth = ffxGetTextureResourceVK(&context, image, view, settings.inputResolution.width, settings.inputResolution.height, format, L"Depth Buffer", FFX_RESOURCE_STATE_COMPUTE_READ);
    return SUCCESS;
}

Upscaler::Status FSR2::VulkanSetInputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) {
    RETURN_ON_FAILURE(setStatusIf(nativeHandle == VK_NULL_HANDLE, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Refused to set image resources due to the given input color image being `VK_NULL_HANDLE`."));

    VkImage        image  = *static_cast<VkImage *>(nativeHandle);
    const VkFormat format = Vulkan::getFormat(unityFormat);
    VkImageView    view   = GraphicsAPI::get<Vulkan>()->createImageView(image, format, VK_IMAGE_ASPECT_COLOR_BIT);

    RETURN_ON_FAILURE(setStatusIf(view == VK_NULL_HANDLE, SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING, "Refused to set image resources due to an attempt to create the input color image view resulting in a `VK_NULL_HANDLE` view handle."));

    inColor = ffxGetTextureResourceVK(&context, image, view, settings.inputResolution.width, settings.inputResolution.height, format, L"Input Color", FFX_RESOURCE_STATE_COMPUTE_READ);
    return SUCCESS;
}

Upscaler::Status FSR2::VulkanSetMotionVectors(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) {
    RETURN_ON_FAILURE(setStatusIf(nativeHandle == VK_NULL_HANDLE, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Refused to set image resources due to the given motion vector image being `VK_NULL_HANDLE`."));

    VkImage        image  = *static_cast<VkImage *>(nativeHandle);
    const VkFormat format = Vulkan::getFormat(unityFormat);
    VkImageView    view   = GraphicsAPI::get<Vulkan>()->createImageView(image, format, VK_IMAGE_ASPECT_COLOR_BIT);

    RETURN_ON_FAILURE(setStatusIf(view == VK_NULL_HANDLE, SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING, "Refused to set image resources due to an attempt to create the motion vector image view resulting in a `VK_NULL_HANDLE` view handle."));

    motionVectors = ffxGetTextureResourceVK(&context, image, view, settings.inputResolution.width, settings.inputResolution.height, format, L"Motion Vectors", FFX_RESOURCE_STATE_COMPUTE_READ);
    return SUCCESS;
}

Upscaler::Status FSR2::VulkanSetOutputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) {
    RETURN_ON_FAILURE(setStatusIf(nativeHandle == VK_NULL_HANDLE, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Refused to set image resources due to the given output color image being `VK_NULL_HANDLE`."));

    VkImage        image  = *static_cast<VkImage *>(nativeHandle);
    const VkFormat format = Vulkan::getFormat(unityFormat);
    VkImageView    view   = GraphicsAPI::get<Vulkan>()->createImageView(image, format, VK_IMAGE_ASPECT_COLOR_BIT);

    RETURN_ON_FAILURE(setStatusIf(view == VK_NULL_HANDLE, SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING, "Refused to set image resources due to an attempt to create the output color image view resulting in a `VK_NULL_HANDLE` view handle."));

    outColor = ffxGetTextureResourceVK(&context, image, view, settings.outputResolution.width, settings.outputResolution.height, format, L"Output Color", FFX_RESOURCE_STATE_COMPUTE_READ);
    return SUCCESS;
}

Upscaler::Status FSR2::VulkanEvaluate() {
    UnityVulkanRecordingState state{};
    GraphicsAPI::get<Vulkan>()->getUnityInterface()->EnsureInsideRenderPass();
    GraphicsAPI::get<Vulkan>()->getUnityInterface()->CommandRecordingState(&state, kUnityVulkanGraphicsQueueAccess_DontCare);

    FfxFsr2DispatchDescription dispatchDescription{
      .commandList                = ffxGetCommandListVK(state.commandBuffer),
      .color                      = inColor,
      .depth                      = depth,
      .motionVectors              = motionVectors,
      .exposure                   = ffxGetTextureResourceVK(&context, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, VK_FORMAT_UNDEFINED, nullptr, FFX_RESOURCE_STATE_COMPUTE_READ),
      .reactive                   = ffxGetTextureResourceVK(&context, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, VK_FORMAT_UNDEFINED, nullptr, FFX_RESOURCE_STATE_COMPUTE_READ),
      .transparencyAndComposition = ffxGetTextureResourceVK(&context, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, VK_FORMAT_UNDEFINED, nullptr, FFX_RESOURCE_STATE_COMPUTE_READ),
      .output                     = outColor,
      .jitterOffset               = {
                                     settings.jitter[0],
                                     settings.jitter[1]
      },
      .motionVectorScale       = {-static_cast<float>(settings.inputResolution.width), -static_cast<float>(settings.inputResolution.height)},
      .renderSize              = {settings.inputResolution.width,                      settings.inputResolution.height                     },
      .enableSharpening        = settings.sharpness > 0,
      .sharpness               = settings.sharpness,
      .frameTimeDelta          = settings.frameTime,
      .preExposure             = 1.F,
      .reset                   = settings.resetHistory,
      .cameraNear              = settings.camera.farPlane,
      .cameraFar               = settings.camera.nearPlane, // Switched because depth is inverted
      .cameraFovAngleVertical  = settings.camera.verticalFOV * (FFX_PI / 180),
      .viewSpaceToMetersFactor = 1.F,
    };

    RETURN_ON_FAILURE(setStatus(ffxFsr2ContextDispatch(&context, &dispatchDescription), "Failed to dispatch FSR2."));
    return SUCCESS;
}

Upscaler::Status FSR2::VulkanRelease() {
    RETURN_ON_FAILURE(setStatus(ffxFsr2ContextDestroy(&context), "Failed to destroy the " + getName() + " context."));
    return SUCCESS;
}

Upscaler::Status FSR2::VulkanShutdown() {
    RETURN_ON_FAILURE(VulkanRelease());
    return SUCCESS;
}
#    endif

void FSR2::setFunctionPointers(const GraphicsAPI::Type graphicsAPI) {
    switch (graphicsAPI) {
        case GraphicsAPI::NONE: {
            fpInitialize       = &FSR2::safeFail;
            fpSetDepth         = &FSR2::safeFail;
            fpSetInputColor    = &FSR2::safeFail;
            fpSetMotionVectors = &FSR2::safeFail;
            fpSetOutputColor   = &FSR2::safeFail;
            fpEvaluate         = &FSR2::safeFail;
            fpRelease          = &FSR2::safeFail;
            fpShutdown         = &FSR2::safeFail;
            break;
        }
#    ifdef ENABLE_VULKAN
        case GraphicsAPI::VULKAN: {
            fpInitialize       = &FSR2::VulkanInitialize;
            fpSetDepth         = &FSR2::VulkanSetDepth;
            fpSetInputColor    = &FSR2::VulkanSetInputColor;
            fpSetMotionVectors = &FSR2::VulkanSetMotionVectors;
            fpSetOutputColor   = &FSR2::VulkanSetOutputColor;
            fpEvaluate         = &FSR2::VulkanEvaluate;
            fpRelease          = &FSR2::VulkanRelease;
            fpShutdown         = &FSR2::VulkanShutdown;
            break;
        }
#    endif
#    ifdef ENABLE_DX12
        case GraphicsAPI::DX12: {
            fpInitialize       = &FSR2::DX12Initialize;
            fpCreate           = &FSR2::DX12CreateFeature;
            fpSetDepth         = &FSR2::DX12SetDepthBuffer;
            fpSetInputColor    = &FSR2::DX12SetInputColor;
            fpSetMotionVectors = &FSR2::DX12SetMotionVectors;
            fpSetOutputColor   = &FSR2::DX12SetOutputColor;
            fpEvaluate         = &FSR2::DX12Evaluate;
            fpRelease          = &FSR2::DX12ReleaseFeature;
            fpShutdown         = &FSR2::DX12Shutdown;
            break;
        }
#    endif
#    ifdef ENABLE_DX11
        case GraphicsAPI::DX11: {
            fpInitialize       = &FSR2::DX11Initialize;
            fpCreate           = &FSR2::DX11CreateFeature;
            fpSetDepth         = &FSR2::DX11SetDepthBuffer;
            fpSetInputColor    = &FSR2::DX11SetInputColor;
            fpSetMotionVectors = &FSR2::DX11SetMotionVectors;
            fpSetOutputColor   = &FSR2::DX11SetOutputColor;
            fpEvaluate         = &FSR2::DX11Evaluate;
            fpRelease          = &FSR2::DX11ReleaseFeature;
            fpShutdown         = &FSR2::DX11Shutdown;
            break;
        }
#    endif
        default: {
            fpInitialize       = &FSR2::safeFail;
            fpSetDepth         = &FSR2::safeFail;
            fpSetInputColor    = &FSR2::safeFail;
            fpSetMotionVectors = &FSR2::safeFail;
            fpSetOutputColor   = &FSR2::safeFail;
            fpEvaluate         = &FSR2::safeFail;
            fpRelease          = &FSR2::safeFail;
            fpShutdown         = &FSR2::safeFail;
            break;
        }
    }
}

void FSR2::log(FfxFsr2MsgType type, const wchar_t *t_msg) {
    std::wstring message(t_msg);
    std::string  msg(message.length(), 0);
    std::ranges::transform(message, msg.begin(), [](const wchar_t c) { return static_cast<char>(c); });
    switch (type) {
        case FFX_FSR2_MESSAGE_TYPE_ERROR: msg = "FSR2 Error ---> " + msg; break;
        case FFX_FSR2_MESSAGE_TYPE_WARNING: msg = "FSR2 Warning -> " + msg; break;
        case FFX_FSR2_MESSAGE_TYPE_COUNT: break;
    }
    if (Upscaler::log != nullptr) Upscaler::log(msg.c_str());
}

Upscaler::Status FSR2::setStatus(FfxErrorCode t_error, const std::string &t_msg) {
    switch (t_error) {
        case FFX_OK:
            return Upscaler::setStatus(SUCCESS, t_msg + " | FFX_OK");
        case FFX_ERROR_INVALID_POINTER:
            return Upscaler::setStatus(SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, t_msg + " | FFX_ERROR_INVALID_POINTER");
        case FFX_ERROR_INVALID_ALIGNMENT:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_INVALID_ALIGNMENT");
        case FFX_ERROR_INVALID_SIZE:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_INVALID_SIZE");
        case FFX_EOF:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING, t_msg + " | FFX_EOF");
        case FFX_ERROR_INVALID_PATH:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_INVALID_PATH");
        case FFX_ERROR_EOF:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_EOF");
        case FFX_ERROR_MALFORMED_DATA:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_MALFORMED_DATA");
        case FFX_ERROR_OUT_OF_MEMORY:
            return Upscaler::setStatus(SOFTWARE_ERROR_OUT_OF_GPU_MEMORY, t_msg + " | FFX_ERROR_OUT_OF_MEMORY");
        case FFX_ERROR_INCOMPLETE_INTERFACE:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_INCOMPLETE_INTERFACE");
        case FFX_ERROR_INVALID_ENUM:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_INVALID_ENUM");
        case FFX_ERROR_INVALID_ARGUMENT:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_INVALID_ARGUMENT");
        case FFX_ERROR_OUT_OF_RANGE:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_OUT_OF_RANGE");
        case FFX_ERROR_NULL_DEVICE:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_NULL_DEVICE");
        case FFX_ERROR_BACKEND_API_ERROR:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_BACKEND_API_ERROR");
        case FFX_ERROR_INSUFFICIENT_MEMORY:
            return Upscaler::setStatus(SOFTWARE_ERROR_OUT_OF_GPU_MEMORY, t_msg + " | FFX_ERROR_INSUFFICIENT_MEMORY");
        default:
            return Upscaler::setStatus(GENERIC_ERROR, t_msg + " | Unknown");
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
    Settings optimalSettings         = settings;
    optimalSettings.outputResolution = resolution;
    optimalSettings.HDR              = hdr;
    optimalSettings.quality          = mode;

    setStatus(
      ffxFsr2GetRenderResolutionFromQualityMode(
        &optimalSettings.inputResolution.width,
        &optimalSettings.inputResolution.height,
        optimalSettings.outputResolution.width,
        optimalSettings.outputResolution.height,
        optimalSettings.getQuality<Upscaler::FSR2>()
      ),
      "Some invalid setting was set. Ensure that the sharpness is between 0F and 1F, and that the QualityMode "
      "setting is a valid enum value."
    );

    return optimalSettings;
}

Upscaler::Status FSR2::initialize() {
    return (this->*fpInitialize)();
}

Upscaler::Status FSR2::create() {
    FfxFsr2ContextDescription description{
      .flags =
#    ifndef NDEBUG
        static_cast<unsigned>(FFX_FSR2_ENABLE_DEBUG_CHECKING) |
#    endif
        static_cast<unsigned>(FFX_FSR2_ENABLE_AUTO_EXPOSURE) |
        static_cast<unsigned>(FFX_FSR2_ENABLE_DEPTH_INVERTED) |
        static_cast<unsigned>(FFX_FSR2_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION) |
        (settings.HDR ? FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE : 0U),
      .maxRenderSize = {settings.outputResolution.width, settings.outputResolution.height},
      .displaySize   = {settings.outputResolution.width, settings.outputResolution.height},
      .callbacks     = interface,
      .device        = ffxGetDeviceVK(GraphicsAPI::get<Vulkan>()->getUnityInterface()->Instance().device),
#    ifndef NDEBUG
      .fpMessage = &FSR2::log
#    endif
    };
    RETURN_ON_FAILURE(setStatus(ffxFsr2ContextCreate(&context, &description), "Failed to create the FSR2 context."));
    return SUCCESS;
}

Upscaler::Status FSR2::setDepth(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) {
    return (this->*fpSetDepth)(nativeHandle, unityFormat);
}

Upscaler::Status FSR2::setInputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) {
    return (this->*fpSetInputColor)(nativeHandle, unityFormat);
}

Upscaler::Status FSR2::setMotionVectors(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) {
    return (this->*fpSetMotionVectors)(nativeHandle, unityFormat);
}

Upscaler::Status FSR2::setOutputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) {
    return (this->*fpSetOutputColor)(nativeHandle, unityFormat);
}

Upscaler::Status FSR2::evaluate() {
    return (this->*fpEvaluate)();
}

Upscaler::Status FSR2::release() {
    return (this->*fpRelease)();
}

Upscaler::Status FSR2::shutdown() {
    if (initialized)
        RETURN_ON_FAILURE((this->*fpShutdown)());
    initialized = false;
    return SUCCESS;
}
#endif