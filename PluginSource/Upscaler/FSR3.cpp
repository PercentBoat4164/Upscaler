#include "FSR3.hpp"
#ifdef ENABLE_FSR3
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

uint32_t FSR3::users{};
std::array<FfxInterface*, 3> FSR3::ffxInterfaces{nullptr, nullptr, nullptr};

Upscaler::Status (FSR3::*FSR3::fpInitialize)(){&FSR3::safeFail};
Upscaler::Status (FSR3::*FSR3::fpEvaluate)(){&FSR3::safeFail};

Upscaler::SupportState FSR3::supported{UNTESTED};

#    ifdef ENABLE_VULKAN
Upscaler::Status FSR3::VulkanInitialize() {
    const UnityVulkanInstance instance = Vulkan::getGraphicsInterface()->Instance();
    VkDeviceContext deviceContext{
        .vkDevice=instance.device,
        .vkPhysicalDevice=instance.physicalDevice,
        .vkDeviceProcAddr=Vulkan::getDeviceProcAddr()
    };
    device = ffxGetDeviceVK(&deviceContext);
    const size_t bufferSize = ffxGetScratchMemorySizeVK(instance.physicalDevice, 1);
    RETURN_ON_FAILURE(setStatusIf(bufferSize == 0, SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, getName() + " does not work in this environment."));
    for (FfxInterface*& ffxInterface : ffxInterfaces) {
        void *buffer = calloc(bufferSize, 1);
        RETURN_ON_FAILURE(setStatusIf(buffer == nullptr, SOFTWARE_ERROR_OUT_OF_SYSTEM_MEMORY, getName() + " requires at least " + std::to_string(bufferSize) + " of contiguous system memory."));
        ffxInterface = new FfxInterface;
        RETURN_ON_FAILURE(setStatus(ffxGetInterfaceVK(ffxInterface, device, buffer, bufferSize, 1), "Upscaler is unable to get the FSR3 interface."));
    }
    return SUCCESS;
}

FfxSurfaceFormat getFormat(const VkFormat format) {
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

Upscaler::Status FSR3::VulkanGetResource(FfxResource& resource, const Plugin::ImageID imageID) {
    RETURN_ON_FAILURE(Upscaler::setStatusIf(imageID >= Plugin::ImageID::IMAGE_ID_MAX_ENUM, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Attempted to get a FFX resource from a nonexistent image."));

    VkAccessFlags accessFlags{VK_ACCESS_MEMORY_READ_BIT};
    FfxResourceUsage resourceUsage{FFX_RESOURCE_USAGE_READ_ONLY};
    VkImageLayout layout{VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};
    if (imageID == Plugin::ImageID::OutputColor) {
        accessFlags = VK_ACCESS_MEMORY_WRITE_BIT;
        layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        resourceUsage = FFX_RESOURCE_USAGE_UAV;
    }
    if (imageID == Plugin::ImageID::Depth)
        layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;

    UnityVulkanImage image{};
    Vulkan::getGraphicsInterface()->AccessTextureByID(textureIDs.at(imageID), UnityVulkanWholeImage, layout, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, accessFlags, kUnityVulkanResourceAccess_PipelineBarrier, &image);
    RETURN_ON_FAILURE(setStatusIf(image.image == VK_NULL_HANDLE, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Unity provided a `VK_NULL_HANDLE` image."));

    const FfxResourceDescription description {
      .type=FFX_RESOURCE_TYPE_TEXTURE2D,
      .format=getFormat(image.format),
      .width=image.extent.width,
      .height=image.extent.height,
      .depth=image.extent.depth,
      .mipCount=1U,
      .flags=FFX_RESOURCE_FLAGS_NONE,
      .usage=resourceUsage,
    };
    RETURN_ON_FAILURE(Upscaler::setStatusIf(description.format == FFX_SURFACE_FORMAT_UNKNOWN, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "The given image format is not compatible with " + getName() + ". Please select a different image format."));
    resource = ffxGetResourceVK(image.image, description, std::wstring(L"").data(), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    return SUCCESS;
}

Upscaler::Status FSR3::VulkanEvaluate() {
    FfxResource color, depth, motion, output, reactiveMask, opaqueColor;
    RETURN_ON_FAILURE(VulkanGetResource(color, Plugin::ImageID::SourceColor));
    RETURN_ON_FAILURE(VulkanGetResource(depth, Plugin::ImageID::Depth));
    RETURN_ON_FAILURE(VulkanGetResource(motion, Plugin::ImageID::Motion));
    RETURN_ON_FAILURE(VulkanGetResource(output, Plugin::ImageID::OutputColor));
    if (settings.autoReactive) {
        RETURN_ON_FAILURE(VulkanGetResource(reactiveMask, Plugin::ImageID::ReactiveMask));
        RETURN_ON_FAILURE(VulkanGetResource(opaqueColor, Plugin::ImageID::OpaqueColor));
    }

    RETURN_ON_FAILURE(setStatusIf(color.description.width < settings.dynamicMinimumInputResolution.width, SETTINGS_ERROR_INVALID_INPUT_RESOLUTION, "The input resolution's width is too small. " + std::to_string(color.description.width) + " < " + std::to_string(settings.dynamicMinimumInputResolution.width)));
    RETURN_ON_FAILURE(setStatusIf(color.description.height < settings.dynamicMinimumInputResolution.height, SETTINGS_ERROR_INVALID_INPUT_RESOLUTION, "The input resolution's height is too small. " + std::to_string(color.description.height) + " < " + std::to_string(settings.dynamicMinimumInputResolution.height)));
    RETURN_ON_FAILURE(setStatusIf(color.description.width > settings.dynamicMaximumInputResolution.width, SETTINGS_ERROR_INVALID_INPUT_RESOLUTION, "The input resolution's width is too big. " + std::to_string(color.description.width) + " > " + std::to_string(settings.dynamicMaximumInputResolution.width)));
    RETURN_ON_FAILURE(setStatusIf(color.description.height > settings.dynamicMaximumInputResolution.height, SETTINGS_ERROR_INVALID_INPUT_RESOLUTION, "The input resolution's height is too big. " + std::to_string(color.description.height) + " > " + std::to_string(settings.dynamicMaximumInputResolution.height)));

    UnityVulkanRecordingState state{};
    Vulkan::getGraphicsInterface()->EnsureInsideRenderPass();
    RETURN_ON_FAILURE(Upscaler::setStatusIf(!Vulkan::getGraphicsInterface()->CommandRecordingState(&state, kUnityVulkanGraphicsQueueAccess_DontCare), SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, "Unable to obtain a command recording state from Unity. This is fatal."));

    FfxCommandList commandList = ffxGetCommandListVK(state.commandBuffer);

    // clang-format off
    // const FfxFsr3UpscalerGenerateReactiveDescription generateReactiveDescription {
    //     .commandList = commandList,
    //     .colorOpaqueOnly = opaqueColor,
    //     .colorPreUpscale = color,
    //     .outReactive = reactiveMask,
    //     .renderSize = {
    //         .width = color.description.width,
    //         .height = color.description.height
    //     },
    //     .scale = settings.reactiveScale,
    //     .cutoffThreshold = settings.reactiveMax,
    //     .binaryValue = 1.0F,
    //     .flags = 0
    // };

    const FfxFsr3DispatchUpscaleDescription dispatchDescription{
      .commandList                = commandList,
      .color                      = color,
      .depth                      = depth,
      .motionVectors              = motion,
      .exposure                   = ffxGetResourceVK(VK_NULL_HANDLE, FfxResourceDescription{}, std::wstring(L"Exposure").data(), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ),
      .reactive                   = settings.autoReactive ? reactiveMask : ffxGetResourceVK(VK_NULL_HANDLE, FfxResourceDescription{}, std::wstring(L"Reactive Mask").data(), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ),
      .transparencyAndComposition = ffxGetResourceVK(VK_NULL_HANDLE, FfxResourceDescription{}, std::wstring(L"T/C Mask").data(), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ),
      .upscaleOutput              = output,
      .jitterOffset               = {
            settings.jitter.x,
            settings.jitter.y
      },
      .motionVectorScale = {
            -static_cast<float>(motion.description.width),
            -static_cast<float>(motion.description.height)
      },
      .renderSize = {
            color.description.width,
            color.description.height
      },
      .enableSharpening        = settings.sharpness > 0.0F,
      .sharpness               = settings.sharpness,
      .frameTimeDelta          = settings.frameTime,
      .preExposure             = 1.0F,
      .reset                   = settings.resetHistory,
      .cameraNear              = settings.camera.farPlane,
      .cameraFar               = settings.camera.nearPlane,  // Switched because depth is inverted
      .cameraFovAngleVertical  = settings.camera.verticalFOV * (FFX_PI / 180.0F),
      .viewSpaceToMetersFactor = 1.0F,
    };
    // clang-format on

    // RETURN_ON_FAILURE(setStatus(ffxFsr3UpscalerContextGenerateReactiveMask(context, &generateReactiveDescription), "Failed to generate reactive mask."));
    RETURN_ON_FAILURE(setStatus(ffxFsr3ContextDispatchUpscale(context, &dispatchDescription), "Failed to dispatch " + getName() + "."));
    return SUCCESS;
}
#    endif

#    ifdef ENABLE_DX12
Upscaler::Status FSR3::DX12Initialize() {
    device                      = ffxGetDeviceDX12(DX12::getGraphicsInterface()->GetDevice());
    const size_t bufferSize     = ffxGetScratchMemorySizeDX12(1);
    RETURN_ON_FAILURE(setStatusIf(bufferSize == 0, SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, getName() + " does not work in this environment."));
    for (FfxInterface*& ffxInterface : ffxInterfaces) {
        delete ffxInterface;
        void *buffer = calloc(bufferSize, 1);
        RETURN_ON_FAILURE(setStatusIf(buffer == nullptr, SOFTWARE_ERROR_OUT_OF_SYSTEM_MEMORY, getName() + " requires at least " + std::to_string(bufferSize) + " of contiguous system memory."));
        ffxInterface = new FfxInterface;
        RETURN_ON_FAILURE(setStatus(ffxGetInterfaceDX12(ffxInterface, device, buffer, bufferSize, 1), "Upscaler is unable to get the " + getName() + " interface."));
    }
    swapchain = ffxGetSwapchainDX12(reinterpret_cast<IDXGISwapChain4*>(DX12::getGraphicsInterface()->GetSwapChain()));
    RETURN_ON_FAILURE(setStatusIf(swapchain == nullptr, SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, "Unity is using an out-of-date swapchain representation."));
    return SUCCESS;
}

Upscaler::Status FSR3::DX12GetResource(FfxResource& resource, const Plugin::ImageID imageID) {
    RETURN_ON_FAILURE(Upscaler::setStatusIf(imageID >= Plugin::ImageID::IMAGE_ID_MAX_ENUM, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Attempted to get a FFX resource from a nonexistent image."));

    FfxResourceUsage resourceUsage{FFX_RESOURCE_USAGE_READ_ONLY};
    if (imageID == Plugin::ImageID::Depth) {
        resourceUsage = FFX_RESOURCE_USAGE_DEPTHTARGET;
    }
    ID3D12Resource*           image            = DX12::getGraphicsInterface()->TextureFromNativeTexture(textureIDs.at(imageID));
    RETURN_ON_FAILURE(setStatusIf(image == nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Unity provided a `nullptr` image."));
    const D3D12_RESOURCE_DESC imageDescription = image->GetDesc();

    const FfxResourceDescription description {
      .type=FFX_RESOURCE_TYPE_TEXTURE2D,
      .format= ffxGetSurfaceFormatDX12(imageDescription.Format),
      .width=static_cast<uint32_t>(imageDescription.Width),
      .height=static_cast<uint32_t>(imageDescription.Height),
      .alignment=static_cast<uint32_t>(imageDescription.Alignment),
      .mipCount=1U,
      .flags=FFX_RESOURCE_FLAGS_NONE,
      .usage=resourceUsage,
    };
    resource = ffxGetResourceDX12(image, description, std::wstring(L"").data(), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    return SUCCESS;
}

Upscaler::Status FSR3::DX12Evaluate() {
    FfxResource color, depth, motion, output, reactiveMask, opaqueColor;
    RETURN_ON_FAILURE(DX12GetResource(color, Plugin::ImageID::SourceColor));
    RETURN_ON_FAILURE(DX12GetResource(depth, Plugin::ImageID::Depth));
    RETURN_ON_FAILURE(DX12GetResource(motion, Plugin::ImageID::Motion));
    RETURN_ON_FAILURE(DX12GetResource(output, Plugin::ImageID::OutputColor));
    if (settings.autoReactive) {
        RETURN_ON_FAILURE(DX12GetResource(reactiveMask, Plugin::ImageID::ReactiveMask));
        RETURN_ON_FAILURE(DX12GetResource(opaqueColor, Plugin::ImageID::OpaqueColor));
    }

    RETURN_ON_FAILURE(setStatusIf(color.description.width < settings.dynamicMinimumInputResolution.width, SETTINGS_ERROR_INVALID_INPUT_RESOLUTION, "The input resolution's width is too small. " + std::to_string(color.description.width) + " < " + std::to_string(settings.dynamicMinimumInputResolution.width)));
    RETURN_ON_FAILURE(setStatusIf(color.description.height < settings.dynamicMinimumInputResolution.height, SETTINGS_ERROR_INVALID_INPUT_RESOLUTION, "The input resolution's height is too small. " + std::to_string(color.description.height) + " < " + std::to_string(settings.dynamicMinimumInputResolution.height)));
    RETURN_ON_FAILURE(setStatusIf(color.description.width > settings.dynamicMaximumInputResolution.width, SETTINGS_ERROR_INVALID_INPUT_RESOLUTION, "The input resolution's width is too big. " + std::to_string(color.description.width) + " > " + std::to_string(settings.dynamicMaximumInputResolution.width)));
    RETURN_ON_FAILURE(setStatusIf(color.description.height > settings.dynamicMaximumInputResolution.height, SETTINGS_ERROR_INVALID_INPUT_RESOLUTION, "The input resolution's height is too big. " + std::to_string(color.description.height) + " > " + std::to_string(settings.dynamicMaximumInputResolution.height)));

    UnityGraphicsD3D12RecordingState state{};
    RETURN_ON_FAILURE(Upscaler::setStatusIf(!DX12::getGraphicsInterface()->CommandRecordingState(&state), SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, "Unable to obtain a command recording state from Unity. This is fatal."));

    FfxCommandList commandList = ffxGetCommandListDX12(state.commandList);

    if (settings.autoReactive) {
        const FfxFsr3GenerateReactiveDescription generateReactiveDescription {
            .commandList = commandList,
            .colorOpaqueOnly = opaqueColor,
            .colorPreUpscale = color,
            .outReactive = reactiveMask,
            .renderSize = {
                .width = color.description.width,
                .height = color.description.height
            },
            .scale = settings.reactiveScale,
            .cutoffThreshold = settings.reactiveMax,
            .binaryValue = 1.0F,
            .flags = 0
        };
        RETURN_ON_FAILURE(setStatus(ffxFsr3ContextGenerateReactiveMask(context, &generateReactiveDescription), "Failed to generate reactive mask."));
    }

    const FfxFsr3DispatchUpscaleDescription dispatchDescription{
      .commandList                = commandList,
      .color                      = color,
      .depth                      = depth,
      .motionVectors              = motion,
      .exposure                   = ffxGetResourceDX12(nullptr, FfxResourceDescription{}, std::wstring(L"Exposure").data(), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ),
      .reactive                   = settings.autoReactive ? reactiveMask : ffxGetResourceDX12(VK_NULL_HANDLE, FfxResourceDescription{}, std::wstring(L"Reactive Mask").data(), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ),
      .transparencyAndComposition = ffxGetResourceDX12(nullptr, FfxResourceDescription{}, std::wstring(L"T/C Mask").data(), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ),
      .upscaleOutput              = output,
      .jitterOffset               = {
           settings.jitter.x,
           settings.jitter.y
      },
      .motionVectorScale = {
          -static_cast<float>(motion.description.width),
          -static_cast<float>(motion.description.height)
      },
      .renderSize = {
          color.description.width,
          color.description.height
      },
      .enableSharpening        = settings.sharpness > 0.0F,
      .sharpness               = settings.sharpness,
      .frameTimeDelta          = settings.frameTime,
      .preExposure             = 1.0F,
      .reset                   = settings.resetHistory,
      .cameraNear              = settings.camera.farPlane,
      .cameraFar               = settings.camera.nearPlane,  // Switched because depth is inverted
      .cameraFovAngleVertical  = settings.camera.verticalFOV * (FFX_PI / 180.0F),
      .viewSpaceToMetersFactor = 1.0F,
    };

    RETURN_ON_FAILURE(setStatus(ffxFsr3ContextDispatchUpscale(context, &dispatchDescription), "Failed to dispatch " + getName() + "."));
    return SUCCESS;
}
#    endif

Upscaler::Status FSR3::setStatus(const FfxErrorCode t_error, const std::string &t_msg) {
    switch (t_error) {
        case static_cast<int>(FFX_OK): return Upscaler::setStatus(SUCCESS, t_msg + " | FFX_OK");
        case static_cast<int>(FFX_ERROR_INVALID_POINTER): return Upscaler::setStatus(SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, t_msg + " | FFX_ERROR_INVALID_POINTER");
        case static_cast<int>(FFX_ERROR_INVALID_ALIGNMENT): return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_INVALID_ALIGNMENT");
        case static_cast<int>(FFX_ERROR_INVALID_SIZE): return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_INVALID_SIZE");
        case static_cast<int>(FFX_EOF): return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_EOF");
        case static_cast<int>(FFX_ERROR_INVALID_PATH): return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_INVALID_PATH");
        case static_cast<int>(FFX_ERROR_EOF): return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_EOF");
        case static_cast<int>(FFX_ERROR_MALFORMED_DATA): return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_MALFORMED_DATA");
        case static_cast<int>(FFX_ERROR_OUT_OF_MEMORY): return Upscaler::setStatus(SOFTWARE_ERROR_OUT_OF_GPU_MEMORY, t_msg + " | FFX_ERROR_OUT_OF_MEMORY");
        case static_cast<int>(FFX_ERROR_INCOMPLETE_INTERFACE): return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_INCOMPLETE_INTERFACE");
        case static_cast<int>(FFX_ERROR_INVALID_ENUM): return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_INVALID_ENUM");
        case static_cast<int>(FFX_ERROR_INVALID_ARGUMENT): return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_INVALID_ARGUMENT");
        case static_cast<int>(FFX_ERROR_OUT_OF_RANGE): return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_OUT_OF_RANGE");
        case static_cast<int>(FFX_ERROR_NULL_DEVICE): return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_NULL_DEVICE");
        case static_cast<int>(FFX_ERROR_BACKEND_API_ERROR): return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | FFX_ERROR_BACKEND_API_ERROR");
        case static_cast<int>(FFX_ERROR_INSUFFICIENT_MEMORY): return Upscaler::setStatus(SOFTWARE_ERROR_OUT_OF_GPU_MEMORY, t_msg + " | FFX_ERROR_INSUFFICIENT_MEMORY");
        default: return Upscaler::setStatus(UNKNOWN_ERROR, t_msg + " | Unknown");
    }
}

void FSR3::log(const FfxMsgType type, const wchar_t *t_msg) {
    std::wstring message(t_msg);
    std::string  msg(message.length(), 0);
    std::ranges::transform(message, msg.begin(), [](const wchar_t c) { return static_cast<char>(c); });
    switch (type) {
        case FFX_MESSAGE_TYPE_ERROR: msg = "FSR3 Error ---> " + msg; break;
        case FFX_MESSAGE_TYPE_WARNING: msg = "FSR3 Warning -> " + msg; break;
        case FFX_MESSAGE_TYPE_COUNT: break;
    }
    if (logCallback != nullptr) logCallback(msg.c_str());
}

bool FSR3::isSupported() {
    if (supported != UNTESTED)
        return supported == SUPPORTED;
    const FSR3 fsr3(GraphicsAPI::getType());
    return (supported = success(fsr3.getStatus()) ? SUPPORTED : UNSUPPORTED) == SUPPORTED;
}

bool FSR3::isSupported(const enum Settings::Quality mode) {
    return mode == Settings::Auto || mode == Settings::Quality || mode == Settings::Balanced || mode == Settings::Performance || mode == Settings::UltraPerformance;
}

FSR3::FSR3(const GraphicsAPI::Type type) {
    switch (type) {
        case GraphicsAPI::NONE: {
            fpInitialize       = &FSR3::safeFail;
            fpEvaluate         = &FSR3::safeFail;
            break;
        }
#    ifdef ENABLE_VULKAN
        case GraphicsAPI::VULKAN: {
            fpInitialize       = &FSR3::VulkanInitialize;
            fpEvaluate         = &FSR3::VulkanEvaluate;
            break;
        }
#    endif
#    ifdef ENABLE_DX12
        case GraphicsAPI::DX12: {
            fpInitialize       = &FSR3::DX12Initialize;
            fpEvaluate         = &FSR3::DX12Evaluate;
            break;
        }
#    endif
#    ifdef ENABLE_DX11
        case GraphicsAPI::DX11: {
            fpInitialize       = &FSR3::invalidGraphicsAPIFail;
            fpEvaluate         = &FSR3::invalidGraphicsAPIFail;
            break;
        }
#    endif
        default: {
            fpInitialize       = &FSR3::safeFail;
            fpEvaluate         = &FSR3::safeFail;
            break;
        }
    }
    initialize();
}

FSR3::~FSR3() {
    shutdown();
}

Upscaler::Status FSR3::getOptimalSettings(const Settings::Resolution resolution, Settings::Preset /*unused*/, const enum Settings::Quality mode, const bool hdr) {
    RETURN_ON_FAILURE(setStatusIf(mode >= Upscaler::Settings::QUALITY_MODE_MAX_ENUM, SETTINGS_ERROR_QUALITY_MODE_NOT_AVAILABLE, "The selected quality mode is unavailable or invalid."));

    Settings optimalSettings         = settings;
    optimalSettings.outputResolution = resolution;
    optimalSettings.hdr              = hdr;
    optimalSettings.quality          = mode;

    RETURN_ON_FAILURE(setStatus(ffxFsr3GetRenderResolutionFromQualityMode(&optimalSettings.renderingResolution.width, &optimalSettings.renderingResolution.height, optimalSettings.outputResolution.width, optimalSettings.outputResolution.height, optimalSettings.getQuality<Upscaler::FSR3>()), "Some invalid setting was set. Ensure that the sharpness is between 0F and 1F, and that the QualityMode setting is a valid enum value."));
    RETURN_ON_FAILURE(setStatusIf(optimalSettings.renderingResolution.width == 0, Upscaler::Status::SETTINGS_ERROR_INVALID_INPUT_RESOLUTION, "The input resolution's width cannot be zero."));
    RETURN_ON_FAILURE(setStatusIf(optimalSettings.renderingResolution.height == 0, Upscaler::Status::SETTINGS_ERROR_INVALID_INPUT_RESOLUTION, "The input resolution's height cannot be zero."));
    optimalSettings.dynamicMaximumInputResolution = resolution;

    settings = optimalSettings;
    return SUCCESS;
}

Upscaler::Status FSR3::initialize() {
    RETURN_ON_FAILURE((this->*fpInitialize)());
    ++users;
    return SUCCESS;
}

Upscaler::Status FSR3::create() {
    if (context != nullptr) return SUCCESS;
    // clang-format off
    FfxFsr3ContextDescription description {
      .flags =
#    ifndef NDEBUG
        static_cast<unsigned>(FFX_FSR3_ENABLE_DEBUG_CHECKING) |
#    endif
        static_cast<unsigned>(FFX_FSR3_ENABLE_AUTO_EXPOSURE) |
        static_cast<unsigned>(FFX_FSR3_ENABLE_DEPTH_INVERTED) |
        static_cast<unsigned>(FFX_FSR3_ENABLE_DYNAMIC_RESOLUTION) |
        (settings.hdr ? FFX_FSR3_ENABLE_HIGH_DYNAMIC_RANGE : 0U),
      .maxRenderSize = {settings.outputResolution.width, settings.outputResolution.height},
      .upscaleOutputSize = {settings.outputResolution.width, settings.outputResolution.height},
      .displaySize   = {settings.outputResolution.width, settings.outputResolution.height},
      .backendInterfaceSharedResources = *ffxInterfaces[0],
      .backendInterfaceUpscaling = *ffxInterfaces[1],
      .backendInterfaceFrameInterpolation = *ffxInterfaces[2],
#    ifndef NDEBUG
      .fpMessage = &FSR3::log,
#    endif
      .backBufferFormat = FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT,
    };
    // clang-format on

    context = new FfxFsr3Context;
    RETURN_ON_FAILURE(setStatus(ffxFsr3ContextCreate(context, &description), "Failed to create the " + getName() + " context."));
    RETURN_ON_FAILURE(setStatusIf(context == nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Failed to create the " + getName() + " context."));
    return SUCCESS;
}

Upscaler::Status FSR3::evaluate() {
    RETURN_ON_FAILURE((this->*fpEvaluate)());
    settings.resetHistory = false;
    return SUCCESS;
}

Upscaler::Status FSR3::shutdown() {
    RETURN_ON_FAILURE(setStatusIf(!swapchain, SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, "Swapchain not specified."));
    ffxWaitForPresents(swapchain);
    const FfxFrameGenerationConfig frameGenerationConfig {
        .swapChain = swapchain,
        .presentCallback = nullptr,
        .frameGenerationCallback = nullptr,
        .frameGenerationEnabled = false,
        .allowAsyncWorkloads = false,
        .HUDLessColor = FfxResource({}),
        .flags = 0,
        .onlyPresentInterpolated = false
    };
    ffxFsr3ConfigureFrameGeneration(context, &frameGenerationConfig);
    ffxRegisterFrameinterpolationUiResourceDX12(swapchain, FfxResource({}));
    if (context != nullptr) {
        setStatus(ffxFsr3ContextDestroy(context), "Failed to destroy the " + getName() + " context.");
        delete context;
        context = nullptr;
    }
    for (FfxInterface*& ffxInterface : ffxInterfaces) {
        if (ffxInterface != nullptr && users == 0) {
            free(ffxInterface->scratchBuffer);
            delete ffxInterface;
            ffxInterface = nullptr;
        }
    }
    return SUCCESS;
}
#endif