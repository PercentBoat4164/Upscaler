#include "FSR2.hpp"
#ifdef ENABLE_FSR2
#    ifdef ENABLE_VULKAN
#        include "GraphicsAPI/Vulkan.hpp"

#        include <IUnityGraphicsVulkan.h>

#        include <ffx_vk.h>
#    endif
#    ifdef ENABLE_DX12
#        include "GraphicsAPI/DX12.hpp"

#        include <d3d12compatibility.h>

#        include <IUnityGraphicsD3D12.h>

#        include <ffx_dx12.h>
#    endif
#    include <algorithm>

FfxInterface* FSR2::ffxInterface{nullptr};

Upscaler::Status (FSR2::*FSR2::fpInitialize)(){&FSR2::safeFail};
Upscaler::Status (FSR2::*FSR2::fpEvaluate)(){&FSR2::safeFail};

Upscaler::SupportState FSR2::supported{UNTESTED};

#    ifdef ENABLE_VULKAN
Upscaler::Status FSR2::VulkanInitialize() {
    RETURN_ON_FAILURE(setStatusIf(ffxInterface != nullptr, SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, "The " + getName() + " interface has already been initialized."));
    UnityVulkanInstance instance = Vulkan::getGraphicsInterface()->Instance();
    VkDeviceContext deviceContext{
          .vkDevice=instance.device,
          .vkPhysicalDevice=instance.physicalDevice,
          .vkDeviceProcAddr=Vulkan::getDeviceProcAddr()
    };
    device = ffxGetDeviceVK(&deviceContext);
    size_t           bufferSize     = ffxGetScratchMemorySizeVK(instance.physicalDevice, 1);
    RETURN_ON_FAILURE(setStatusIf(bufferSize == 0, SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, getName() + " does not work in this environment."));
    void *buffer = calloc(bufferSize, 1);
    RETURN_ON_FAILURE(setStatusIf(buffer == nullptr, SOFTWARE_ERROR_OUT_OF_SYSTEM_MEMORY, getName() + " requires at least " + std::to_string(bufferSize) + " of contiguous system memory."));
    ffxInterface = new FfxInterface;
    RETURN_ON_FAILURE(setStatus(ffxGetInterfaceVK(ffxInterface, device, buffer, bufferSize, 1), "Upscaler is unable to get the FSR2 interface."));
    return SUCCESS;
}

FfxSurfaceFormat getFormat(VkFormat format) {
    switch(format) {
        case VK_FORMAT_R8_UNORM: return FFX_SURFACE_FORMAT_R8_UNORM;
        case VK_FORMAT_R8_UINT: return FFX_SURFACE_FORMAT_R8_UINT;
        case VK_FORMAT_R8G8_UNORM: return FFX_SURFACE_FORMAT_R8G8_UNORM;
        case VK_FORMAT_R8G8_UINT: return FFX_SURFACE_FORMAT_R8G8_UINT;
        case VK_FORMAT_R8G8B8A8_UNORM: return FFX_SURFACE_FORMAT_R8G8B8A8_UNORM;
        case VK_FORMAT_R8G8B8A8_SNORM: return FFX_SURFACE_FORMAT_R8G8B8A8_SNORM;
        case VK_FORMAT_R8G8B8A8_USCALED:
        case VK_FORMAT_R8G8B8A8_SSCALED:
        case VK_FORMAT_R8G8B8A8_UINT:
        case VK_FORMAT_R8G8B8A8_SINT: return FFX_SURFACE_FORMAT_R8G8B8A8_TYPELESS;
        case VK_FORMAT_R8G8B8A8_SRGB: return FFX_SURFACE_FORMAT_R8G8B8A8_SRGB;
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SNORM:
        case VK_FORMAT_B8G8R8A8_USCALED:
        case VK_FORMAT_B8G8R8A8_SSCALED:
        case VK_FORMAT_B8G8R8A8_UINT:
        case VK_FORMAT_B8G8R8A8_SINT:
        case VK_FORMAT_B8G8R8A8_SRGB: return FFX_SURFACE_FORMAT_R8G8B8A8_TYPELESS;
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return FFX_SURFACE_FORMAT_R11G11B10_FLOAT;
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_R16_UNORM: return FFX_SURFACE_FORMAT_R16_UNORM;
        case VK_FORMAT_R16_SNORM: return FFX_SURFACE_FORMAT_R16_SNORM;
        case VK_FORMAT_R16_UINT: return FFX_SURFACE_FORMAT_R16_UINT;
        case VK_FORMAT_R16_SFLOAT: return FFX_SURFACE_FORMAT_R16_FLOAT;
        case VK_FORMAT_R16G16_UINT: return FFX_SURFACE_FORMAT_R16G16_UINT;
        case VK_FORMAT_R16G16_SINT: return FFX_SURFACE_FORMAT_R16G16_SINT;
        case VK_FORMAT_R16G16_SFLOAT: return FFX_SURFACE_FORMAT_R16G16_FLOAT;
        case VK_FORMAT_R16G16B16A16_SFLOAT: return FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT;
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_R32_SFLOAT: return FFX_SURFACE_FORMAT_R32_FLOAT;
        case VK_FORMAT_R32_UINT: return FFX_SURFACE_FORMAT_R32_UINT;
        case VK_FORMAT_R32G32_SFLOAT: return FFX_SURFACE_FORMAT_R32G32_FLOAT;
        case VK_FORMAT_R32G32B32A32_UINT: return FFX_SURFACE_FORMAT_R32G32B32A32_UINT;
        case VK_FORMAT_R32G32B32A32_SINT: return FFX_SURFACE_FORMAT_R32G32B32A32_TYPELESS;
        case VK_FORMAT_R32G32B32A32_SFLOAT: return FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT;
        default: return FFX_SURFACE_FORMAT_UNKNOWN;
    }
}

Upscaler::Status FSR2::VulkanGetResource(FfxResource& resource, Plugin::ImageID imageID) {
    RETURN_ON_FAILURE(Upscaler::setStatusIf(imageID >= Plugin::ImageID::IMAGE_ID_MAX_ENUM, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Attempted to get a FFX resource from a nonexistent image."));

    VkAccessFlags accessFlags{VK_ACCESS_MEMORY_READ_BIT};
    FfxResourceUsage resourceUsage{FFX_RESOURCE_USAGE_UAV};
    VkImageLayout layout{VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};
    if (imageID == Plugin::ImageID::OutputColor) {
        accessFlags = VK_ACCESS_MEMORY_WRITE_BIT;
        layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    if (imageID == Plugin::ImageID::Depth) {
        layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
        resourceUsage = FFX_RESOURCE_USAGE_DEPTHTARGET;
    }
    if (imageID == Plugin::ImageID::Motion) {
        resourceUsage = FFX_RESOURCE_USAGE_READ_ONLY;
    }
    UnityVulkanImage image{};
    Vulkan::getGraphicsInterface()->AccessTextureByID(textureIDs[imageID], UnityVulkanWholeImage, layout, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, accessFlags, kUnityVulkanResourceAccess_PipelineBarrier, &image);

    FfxResourceDescription description {
      .type=FFX_RESOURCE_TYPE_TEXTURE2D,
      .format=getFormat(image.format),
      .width=image.extent.width,
      .height=image.extent.height,
      .depth=image.extent.depth,
      .mipCount=1,
      .flags=FFX_RESOURCE_FLAGS_NONE,
      .usage=resourceUsage,
    };
    RETURN_ON_FAILURE(Upscaler::setStatusIf(description.format == FFX_SURFACE_FORMAT_UNKNOWN, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "The given image format is not compatible with " + getName() + ". Please select a different image format."));

    resource = ffxGetResourceVK(image.image, description, std::wstring(L"").data(), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    return SUCCESS;
}

Upscaler::Status FSR2::VulkanEvaluate() {
    FfxResource color, depth, motion, output;
    RETURN_ON_FAILURE(VulkanGetResource(color, Plugin::ImageID::SourceColor));
    RETURN_ON_FAILURE(VulkanGetResource(depth, Plugin::ImageID::Depth));
    RETURN_ON_FAILURE(VulkanGetResource(motion, Plugin::ImageID::Motion));
    RETURN_ON_FAILURE(VulkanGetResource(output, Plugin::ImageID::OutputColor));

    UnityVulkanRecordingState state{};
    Vulkan::getGraphicsInterface()->EnsureInsideRenderPass();
    RETURN_ON_FAILURE(Upscaler::setStatusIf(!Vulkan::getGraphicsInterface()->CommandRecordingState(&state, kUnityVulkanGraphicsQueueAccess_DontCare), SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, "Unable to obtain a command recording state from Unity. This is fatal."));

    // clang-format off
    FfxFsr2DispatchDescription dispatchDescription{
      .commandList                = ffxGetCommandListVK(state.commandBuffer),
      .color                      = color,
      .depth                      = depth,
      .motionVectors              = motion,
      .exposure                   = ffxGetResourceVK(VK_NULL_HANDLE, FfxResourceDescription{}, std::wstring(L"Exposure").data(), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ),
      .reactive                   = ffxGetResourceVK(VK_NULL_HANDLE, FfxResourceDescription{}, std::wstring(L"Reactive Mask").data(), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ),
      .transparencyAndComposition = ffxGetResourceVK(VK_NULL_HANDLE, FfxResourceDescription{}, std::wstring(L"Transparency/Composition Mask").data(), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ),
      .output                     = output,
      .jitterOffset               = {
            settings.jitter.x,
            settings.jitter.y
      },
      .motionVectorScale = {
            -static_cast<float>(color.description.width),
            -static_cast<float>(color.description.height)
      },
      .renderSize = {
            color.description.width,
            color.description.height
      },
      .enableSharpening        = settings.sharpness > 0,
      .sharpness               = settings.sharpness,
      .frameTimeDelta          = settings.frameTime,
      .preExposure             = 1.F,
      .reset                   = settings.resetHistory,
      .cameraNear              = settings.camera.farPlane,
      .cameraFar               = settings.camera.nearPlane,  // Switched because depth is inverted
      .cameraFovAngleVertical  = settings.camera.verticalFOV * (FFX_PI / 180),
      .viewSpaceToMetersFactor = 1.F,
    };
    // clang-format on

    RETURN_ON_FAILURE(setStatus(ffxFsr2ContextDispatch(context, &dispatchDescription), "Failed to dispatch FSR2."));
    return SUCCESS;
}
#    endif

#    ifdef ENABLE_DX12
Upscaler::Status FSR2::DX12Initialize() {
    RETURN_ON_FAILURE(setStatusIf(ffxInterface != nullptr, SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, "The " + getName() + " interface has already been initialized."));
    device = ffxGetDeviceDX12(DX12::getGraphicsInterface()->GetDevice());
    size_t           bufferSize     = ffxGetScratchMemorySizeDX12(1);
    RETURN_ON_FAILURE(setStatusIf(bufferSize == 0, SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, getName() + " does not work in this environment."));
    void *buffer = calloc(bufferSize, 1);
    RETURN_ON_FAILURE(setStatusIf(buffer == nullptr, SOFTWARE_ERROR_OUT_OF_SYSTEM_MEMORY, getName() + " requires at least " + std::to_string(bufferSize) + " of contiguous system memory."));
    ffxInterface = new FfxInterface;
    RETURN_ON_FAILURE(setStatus(ffxGetInterfaceDX12(ffxInterface, device, buffer, bufferSize, 1), "Upscaler is unable to get the FSR2 interface."));
    return SUCCESS;
}

Upscaler::Status FSR2::DX12GetResource(FfxResource& resource, Plugin::ImageID imageID) {
    RETURN_ON_FAILURE(Upscaler::setStatusIf(imageID >= Plugin::ImageID::IMAGE_ID_MAX_ENUM, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Attempted to get a FFX resource from a nonexistent image."));

    FfxResourceUsage resourceUsage{FFX_RESOURCE_USAGE_READ_ONLY};
    if (imageID == Plugin::ImageID::Depth) {
        resourceUsage = FFX_RESOURCE_USAGE_DEPTHTARGET;
    }
    ID3D12Resource* image = DX12::getGraphicsInterface()->TextureFromNativeTexture(textureIDs[imageID]);
    D3D12_RESOURCE_DESC imageDescription = image->GetDesc();

    FfxResourceDescription description {
      .type=FFX_RESOURCE_TYPE_TEXTURE2D,
      .format= ffxGetSurfaceFormatDX12(imageDescription.Format),
      .width=static_cast<uint32_t>(imageDescription.Width),
      .height=static_cast<uint32_t>(imageDescription.Height),
      .alignment=static_cast<uint32_t>(imageDescription.Alignment),
      .mipCount=1,
      .flags=FFX_RESOURCE_FLAGS_NONE,
      .usage=resourceUsage,
    };
    resource = ffxGetResourceDX12(image, description, const_cast<wchar_t*>(std::wstring(L"").c_str()), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    return SUCCESS;
}

Upscaler::Status FSR2::DX12Evaluate() {
    FfxResource color, depth, motion, output;
    RETURN_ON_FAILURE(DX12GetResource(color, Plugin::ImageID::SourceColor));
    RETURN_ON_FAILURE(DX12GetResource(depth, Plugin::ImageID::Depth));
    RETURN_ON_FAILURE(DX12GetResource(motion, Plugin::ImageID::Motion));
    RETURN_ON_FAILURE(DX12GetResource(output, Plugin::ImageID::OutputColor));

    UnityGraphicsD3D12RecordingState state{};
    DX12::getGraphicsInterface()->CommandRecordingState(&state);

    // clang-format off
    FfxFsr2DispatchDescription dispatchDescription{
      .commandList                = ffxGetCommandListDX12(state.commandList),
      .color                      = color,
      .depth                      = depth,
      .motionVectors              = motion,
      .exposure                   = ffxGetResourceDX12(nullptr, FfxResourceDescription{}, const_cast<wchar_t*>(std::wstring(L"Exposure").c_str()), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ),
      .reactive                   = ffxGetResourceDX12(nullptr, FfxResourceDescription{}, const_cast<wchar_t*>(std::wstring(L"Reactive Mask").c_str()), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ),
      .transparencyAndComposition = ffxGetResourceDX12(nullptr, FfxResourceDescription{}, const_cast<wchar_t*>(std::wstring(L"Transparency/Composition Mask").c_str()), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ),
      .output                     = output,
      .jitterOffset               = {
           settings.jitter.x,
           settings.jitter.y
      },
      .motionVectorScale = {
          -static_cast<float>(color.description.width),
          -static_cast<float>(color.description.height)
      },
      .renderSize = {
          color.description.width,
          color.description.height
      },
      .enableSharpening        = settings.sharpness > 0,
      .sharpness               = settings.sharpness,
      .frameTimeDelta          = settings.frameTime,
      .preExposure             = 1.F,
      .reset                   = settings.resetHistory,
      .cameraNear              = settings.camera.farPlane,
      .cameraFar               = settings.camera.nearPlane,  // Switched because depth is inverted
      .cameraFovAngleVertical  = settings.camera.verticalFOV * (FFX_PI / 180),
      .viewSpaceToMetersFactor = 1.F,
    };
    // clang-format on

    RETURN_ON_FAILURE(setStatus(ffxFsr2ContextDispatch(context, &dispatchDescription), "Failed to dispatch FSR2."));
    return SUCCESS;
}
#    endif

Upscaler::Status FSR2::setStatus(FfxErrorCode t_error, const std::string &t_msg) {
    switch (t_error) {
        case FFX_OK:
            return Upscaler::setStatus(SUCCESS, t_msg + " | FFX_OK");
        case (int)FFX_ERROR_INVALID_POINTER:
            return Upscaler::setStatus(SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, t_msg + " | FFX_ERROR_INVALID_POINTER");
        case (int)FFX_ERROR_INVALID_ALIGNMENT:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_INVALID_ALIGNMENT");
        case (int)FFX_ERROR_INVALID_SIZE:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_INVALID_SIZE");
        case (int)FFX_EOF:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_EOF");
        case (int)FFX_ERROR_INVALID_PATH:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_INVALID_PATH");
        case (int)FFX_ERROR_EOF:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_EOF");
        case (int)FFX_ERROR_MALFORMED_DATA:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_MALFORMED_DATA");
        case (int)FFX_ERROR_OUT_OF_MEMORY:
            return Upscaler::setStatus(SOFTWARE_ERROR_OUT_OF_GPU_MEMORY, t_msg + " | FFX_ERROR_OUT_OF_MEMORY");
        case (int)FFX_ERROR_INCOMPLETE_INTERFACE:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_INCOMPLETE_INTERFACE");
        case (int)FFX_ERROR_INVALID_ENUM:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_INVALID_ENUM");
        case (int)FFX_ERROR_INVALID_ARGUMENT:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_INVALID_ARGUMENT");
        case (int)FFX_ERROR_OUT_OF_RANGE:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_OUT_OF_RANGE");
        case (int)FFX_ERROR_NULL_DEVICE:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_NULL_DEVICE");
        case (int)FFX_ERROR_BACKEND_API_ERROR:
            return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_BACKEND_API_ERROR");
        case (int)FFX_ERROR_INSUFFICIENT_MEMORY:
            return Upscaler::setStatus(SOFTWARE_ERROR_OUT_OF_GPU_MEMORY, t_msg + " | FFX_ERROR_INSUFFICIENT_MEMORY");
        default:
            return Upscaler::setStatus(GENERIC_ERROR, t_msg + " | Unknown");
    }
}

void FSR2::log(FfxMsgType type, const wchar_t *t_msg) {
    std::wstring message(t_msg);
    std::string  msg(message.length(), 0);
    std::ranges::transform(message, msg.begin(), [](const wchar_t c) { return static_cast<char>(c); });
    switch (type) {
        case FFX_MESSAGE_TYPE_ERROR: msg = "FSR2 Error ---> " + msg; break;
        case FFX_MESSAGE_TYPE_WARNING: msg = "FSR2 Warning -> " + msg; break;
        case FFX_MESSAGE_TYPE_COUNT: break;
    }
    if (Upscaler::logCallback != nullptr) Upscaler::logCallback(msg.c_str());
}

FSR2::FSR2(const GraphicsAPI::Type graphicsAPI) {
    switch (graphicsAPI) {
        case GraphicsAPI::NONE: {
            fpInitialize       = &FSR2::safeFail;
            fpEvaluate         = &FSR2::safeFail;
            break;
        }
#    ifdef ENABLE_VULKAN
        case GraphicsAPI::VULKAN: {
            fpInitialize       = &FSR2::VulkanInitialize;
            fpEvaluate         = &FSR2::VulkanEvaluate;
            break;
        }
#    endif
#    ifdef ENABLE_DX12
        case GraphicsAPI::DX12: {
            fpInitialize       = &FSR2::DX12Initialize;
            fpEvaluate         = &FSR2::DX12Evaluate;
            break;
        }
#    endif
#    ifdef ENABLE_DX11
        case GraphicsAPI::DX11: {
            fpInitialize       = &FSR2::invalidGraphicsAPIFail;
            fpEvaluate         = &FSR2::invalidGraphicsAPIFail;
            break;
        }
#    endif
        default: {
            fpInitialize       = &FSR2::safeFail;
            fpEvaluate         = &FSR2::safeFail;
            break;
        }
    }
    initialize();
}

FSR2::~FSR2() {
    shutdown();
}

#    ifdef ENABLE_VULKAN
std::vector<std::string> FSR2::requestVulkanInstanceExtensions(const std::vector<std::string>& /*unused*/) {
    return {};
}
std::vector<std::string> FSR2::requestVulkanDeviceExtensions(VkInstance /*unused*/, VkPhysicalDevice /*unused*/, const std::vector<std::string>& /*unused*/) {
    return {};
}
#    endif

bool FSR2::isSupported() {
    if (supported != UNTESTED)
        return supported == SUPPORTED;
    FSR2 fsr2(GraphicsAPI::getType());
    return (supported = success(fsr2.getStatus()) ? SUPPORTED : UNSUPPORTED) == SUPPORTED;
}

Upscaler::Status FSR2::getOptimalSettings(const Settings::Resolution resolution, Settings::Preset /*unused*/, const enum Settings::Quality mode, const bool hdr) {
    RETURN_ON_FAILURE(setStatusIf(mode >= Upscaler::Settings::QUALITY_MODE_MAX_ENUM, SETTINGS_ERROR_QUALITY_MODE_NOT_AVAILABLE, "The selected quality mode is unavailable or invalid."));

    Settings optimalSettings         = settings;
    optimalSettings.outputResolution = resolution;
    optimalSettings.hdr              = hdr;
    optimalSettings.quality          = mode;

    RETURN_ON_FAILURE(setStatus(ffxFsr2GetRenderResolutionFromQualityMode(&optimalSettings.renderingResolution.width, &optimalSettings.renderingResolution.height, optimalSettings.outputResolution.width, optimalSettings.outputResolution.height, optimalSettings.getQuality<Upscaler::FSR2>()), "Some invalid setting was set. Ensure that the sharpness is between 0F and 1F, and that the QualityMode setting is a valid enum value."));
    optimalSettings.dynamicMaximumInputResolution = resolution;

    RETURN_ON_FAILURE(setStatusIf(optimalSettings.renderingResolution.width == 0, Upscaler::Status::SETTINGS_ERROR_INVALID_INPUT_RESOLUTION, "The input resolution's width cannot be zero."));
    RETURN_ON_FAILURE(setStatusIf(optimalSettings.renderingResolution.height == 0, Upscaler::Status::SETTINGS_ERROR_INVALID_INPUT_RESOLUTION, "The input resolution's height cannot be zero."));

    settings = optimalSettings;
    settings.jitterGenerator.generate(settings.renderingResolution, settings.outputResolution);
    return SUCCESS;
}

Upscaler::Status FSR2::initialize() {
    if (ffxInterface == nullptr)
        return ((this->*fpInitialize)());
    return SUCCESS;
}

Upscaler::Status FSR2::create() {
    if (context != nullptr)
        RETURN_ON_FAILURE(shutdown());
    // clang-format off
    FfxFsr2ContextDescription description{
      .flags =
#    ifndef NDEBUG
        static_cast<unsigned>(FFX_FSR2_ENABLE_DEBUG_CHECKING) |
#    endif
        static_cast<unsigned>(FFX_FSR2_ENABLE_AUTO_EXPOSURE) |
        static_cast<unsigned>(FFX_FSR2_ENABLE_DEPTH_INVERTED) |
        static_cast<unsigned>(FFX_FSR2_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION) |
        static_cast<unsigned>(FFX_FSR2_ENABLE_DYNAMIC_RESOLUTION) |  /**@todo Make me a switch.*/
        (settings.hdr ? FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE : 0U),
      .maxRenderSize = {settings.outputResolution.width, settings.outputResolution.height},
      .displaySize   = {settings.outputResolution.width, settings.outputResolution.height},
      .backendInterface = *ffxInterface,
#    ifndef NDEBUG
      .fpMessage = &FSR2::log
#    endif
    };
    // clang-format on

    context = new FfxFsr2Context;
    RETURN_ON_FAILURE(setStatus(ffxFsr2ContextCreate(context, &description), "Failed to create the " + getName() + " context."));
    RETURN_ON_FAILURE(setStatusIf(context == nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Failed to create the " + getName() + " context."));
    return SUCCESS;
}

Upscaler::Status FSR2::evaluate() {
    return (this->*fpEvaluate)();
}

Upscaler::Status FSR2::shutdown() {
    if (context != nullptr)
        RETURN_ON_FAILURE(setStatus(ffxFsr2ContextDestroy(context), "Failed to destroy the " + getName() + " context."));
    return SUCCESS;
}
#endif