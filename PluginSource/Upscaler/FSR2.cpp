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

uint32_t FSR2::users{};
FfxInterface* FSR2::ffxInterface{nullptr};

Upscaler::Status (FSR2::*FSR2::fpInitialize)(){&FSR2::safeFail};
Upscaler::Status (FSR2::*FSR2::fpEvaluate)(){&FSR2::safeFail};

Upscaler::SupportState FSR2::supported{Untested};

#    ifdef ENABLE_VULKAN
Upscaler::Status FSR2::VulkanInitialize() {
    delete ffxInterface;
    const UnityVulkanInstance instance = Vulkan::getGraphicsInterface()->Instance();
    VkDeviceContext deviceContext{
          .vkDevice=instance.device,
          .vkPhysicalDevice=instance.physicalDevice,
          .vkDeviceProcAddr=Vulkan::getDeviceProcAddr()
    };
    device                  = ffxGetDeviceVK(&deviceContext);
    const size_t bufferSize = ffxGetScratchMemorySizeVK(instance.physicalDevice, 1);
    RETURN_ON_FAILURE(setStatusIf(bufferSize == 0, FatalRuntimeError, getName() + " does not work in this environment (OS, graphics API, device, or drivers)."));
    void *buffer = calloc(bufferSize, 1);
    RETURN_ON_FAILURE(setStatusIf(buffer == nullptr, OutOfMemory, getName() + " requires at least " + std::to_string(bufferSize) + " of contiguous system memory."));
    ffxInterface = new FfxInterface;
    RETURN_ON_FAILURE(setStatus(ffxGetInterfaceVK(ffxInterface, device, buffer, bufferSize, 1), "Upscaler is unable to get the FSR2 interface."));
    return Success;
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

Upscaler::Status FSR2::VulkanGetResource(FfxResource& resource, const Plugin::ImageID imageID) {
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
    RETURN_ON_FAILURE(setStatusIf(image.image == VK_NULL_HANDLE, RecoverableRuntimeError, "Unity provided a `VK_NULL_HANDLE` image."));

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
    resource = ffxGetResourceVK(image.image, description, std::wstring(L"").data(), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
    return Success;
}

Upscaler::Status FSR2::VulkanEvaluate() {
    FfxResource color, depth, motion, output, reactiveMask, opaqueColor;
    RETURN_ON_FAILURE(VulkanGetResource(color, Plugin::ImageID::SourceColor));
    RETURN_ON_FAILURE(VulkanGetResource(depth, Plugin::ImageID::Depth));
    RETURN_ON_FAILURE(VulkanGetResource(motion, Plugin::ImageID::Motion));
    RETURN_ON_FAILURE(VulkanGetResource(output, Plugin::ImageID::OutputColor));
    if (settings.autoReactive) {
        RETURN_ON_FAILURE(VulkanGetResource(reactiveMask, Plugin::ImageID::ReactiveMask));
        RETURN_ON_FAILURE(VulkanGetResource(opaqueColor, Plugin::ImageID::OpaqueColor));
    }

    UnityVulkanRecordingState state{};
    Vulkan::getGraphicsInterface()->EnsureInsideRenderPass();
    RETURN_ON_FAILURE(Upscaler::setStatusIf(!Vulkan::getGraphicsInterface()->CommandRecordingState(&state, kUnityVulkanGraphicsQueueAccess_DontCare), FatalRuntimeError, "Unable to obtain a command recording state from Unity. This is fatal."));

    FfxCommandList commandList = ffxGetCommandListVK(state.commandBuffer);

    // clang-format off
    // const FfxFsr2UpscalerGenerateReactiveDescription generateReactiveDescription {
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

    const FfxFsr2DispatchDescription dispatchDescription {
      .commandList                = commandList,
      .color                      = color,
      .depth                      = depth,
      .motionVectors              = motion,
      .exposure                   = ffxGetResourceVK(VK_NULL_HANDLE, FfxResourceDescription{}, std::wstring(L"Exposure").data(), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ),
      .reactive                   = settings.autoReactive ? reactiveMask : ffxGetResourceVK(VK_NULL_HANDLE, FfxResourceDescription{}, std::wstring(L"Reactive Mask").data(), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ),
      .transparencyAndComposition = ffxGetResourceVK(VK_NULL_HANDLE, FfxResourceDescription{}, std::wstring(L"T/C Mask").data(), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ),
      .output                     = output,
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
      .enableAutoReactive      = settings.autoReactive,
      .colorOpaqueOnly         = settings.autoReactive ? opaqueColor : ffxGetResourceVK(VK_NULL_HANDLE, FfxResourceDescription{}, std::wstring(L"Opaque Color").data(), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ),
      .autoTcThreshold         = settings.tcThreshold,
      .autoTcScale             = settings.tcScale,
      .autoReactiveScale       = settings.reactiveScale,
      .autoReactiveMax         = settings.reactiveMax,
    };
    // clang-format on

    // RETURN_ON_FAILURE(setStatus(ffxFsr2UpscalerContextGenerateReactiveMask(context, &generateReactiveDescription), "Failed to generate reactive mask."));
    RETURN_ON_FAILURE(setStatus(ffxFsr2ContextDispatch(context, &dispatchDescription), "Failed to dispatch " + getName() + "."));
    return Success;
}
#    endif

#    ifdef ENABLE_DX12
Upscaler::Status FSR2::DX12Initialize() {
    delete ffxInterface;
    device                      = ffxGetDeviceDX12(DX12::getGraphicsInterface()->GetDevice());
    const size_t bufferSize     = ffxGetScratchMemorySizeDX12(1);
    RETURN_ON_FAILURE(setStatusIf(bufferSize == 0, FatalRuntimeError, getName() + " does not work in this environment."));
    void *buffer = calloc(bufferSize, 1);
    RETURN_ON_FAILURE(setStatusIf(buffer == nullptr, OutOfMemory, getName() + " requires at least " + std::to_string(bufferSize) + " of contiguous system memory."));
    ffxInterface = new FfxInterface;
    RETURN_ON_FAILURE(setStatus(ffxGetInterfaceDX12(ffxInterface, device, buffer, bufferSize, 1), "Upscaler is unable to get the " + getName() + " interface."));
    return Success;
}

Upscaler::Status FSR2::DX12GetResource(FfxResource& resource, const Plugin::ImageID imageID) {
    FfxResourceUsage resourceUsage{FFX_RESOURCE_USAGE_READ_ONLY};
    if (imageID == Plugin::ImageID::Depth) {
        resourceUsage = FFX_RESOURCE_USAGE_DEPTHTARGET;
    }
    ID3D12Resource*           image            = DX12::getGraphicsInterface()->TextureFromNativeTexture(textureIDs.at(imageID));
    RETURN_ON_FAILURE(setStatusIf(image == nullptr, RecoverableRuntimeError, "Unity provided a `nullptr` image."));
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
    return Success;
}

Upscaler::Status FSR2::DX12Evaluate() {
    FfxResource color, depth, motion, output, reactiveMask, opaqueColor;
    RETURN_ON_FAILURE(DX12GetResource(color, Plugin::ImageID::SourceColor));
    RETURN_ON_FAILURE(DX12GetResource(depth, Plugin::ImageID::Depth));
    RETURN_ON_FAILURE(DX12GetResource(motion, Plugin::ImageID::Motion));
    RETURN_ON_FAILURE(DX12GetResource(output, Plugin::ImageID::OutputColor));
    if (settings.autoReactive) {
        RETURN_ON_FAILURE(DX12GetResource(reactiveMask, Plugin::ImageID::ReactiveMask));
        RETURN_ON_FAILURE(DX12GetResource(opaqueColor, Plugin::ImageID::OpaqueColor));
    }

    UnityGraphicsD3D12RecordingState state{};
    RETURN_ON_FAILURE(Upscaler::setStatusIf(!DX12::getGraphicsInterface()->CommandRecordingState(&state), FatalRuntimeError, "Unable to obtain a command recording state from Unity. This is fatal."));

    FfxCommandList commandList = ffxGetCommandListDX12(state.commandList);

    // clang-format off
    // const FfxFsr2UpscalerGenerateReactiveDescription generateReactiveDescription {
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

    const FfxFsr2DispatchDescription dispatchDescription{
      .commandList                = commandList,
      .color                      = color,
      .depth                      = depth,
      .motionVectors              = motion,
      .exposure                   = ffxGetResourceDX12(nullptr, FfxResourceDescription{}, std::wstring(L"Exposure").data(), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ),
      .reactive                   = settings.autoReactive ? reactiveMask : ffxGetResourceDX12(VK_NULL_HANDLE, FfxResourceDescription{}, std::wstring(L"Reactive Mask").data(), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ),
      .transparencyAndComposition = ffxGetResourceDX12(nullptr, FfxResourceDescription{}, std::wstring(L"T/C Mask").data(), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ),
      .output                     = output,
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
      .enableAutoReactive      = settings.autoReactive,
      .colorOpaqueOnly         = settings.autoReactive ? opaqueColor : ffxGetResourceDX12(VK_NULL_HANDLE, FfxResourceDescription{}, std::wstring(L"Opaque Color").data(), FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ),
      .autoTcThreshold         = settings.tcThreshold,
      .autoTcScale             = settings.tcScale,
      .autoReactiveScale       = settings.reactiveScale,
      .autoReactiveMax         = settings.reactiveMax,
    };
    // clang-format on

    // RETURN_ON_FAILURE(setStatus(ffxFsr2UpscalerContextGenerateReactiveMask(context, &generateReactiveDescription), "Failed to generate reactive mask."));
    RETURN_ON_FAILURE(setStatus(ffxFsr2ContextDispatch(context, &dispatchDescription), "Failed to dispatch " + getName() + "."));
    return Success;
}
#    endif

Upscaler::Status FSR2::setStatus(const FfxErrorCode t_error, const std::string &t_msg) {
    switch (t_error) {
        case static_cast<int>(FFX_OK): return Upscaler::setStatus(Success, t_msg + " | FFX_OK");
        case static_cast<int>(FFX_ERROR_INVALID_POINTER): return Upscaler::setStatus(FatalRuntimeError, t_msg + " | FFX_ERROR_INVALID_POINTER");
        case static_cast<int>(FFX_ERROR_INVALID_ALIGNMENT): return Upscaler::setStatus(FatalRuntimeError, t_msg + " | FFX_ERROR_INVALID_ALIGNMENT");
        case static_cast<int>(FFX_ERROR_INVALID_SIZE): return Upscaler::setStatus(FatalRuntimeError, t_msg + " | FFX_ERROR_INVALID_SIZE");
        case static_cast<int>(FFX_EOF): return Upscaler::setStatus(FatalRuntimeError, t_msg + " | FFX_EOF");
        case static_cast<int>(FFX_ERROR_INVALID_PATH): return Upscaler::setStatus(FatalRuntimeError, t_msg + " | FFX_ERROR_INVALID_PATH");
        case static_cast<int>(FFX_ERROR_EOF): return Upscaler::setStatus(FatalRuntimeError, t_msg + " | FFX_ERROR_EOF");
        case static_cast<int>(FFX_ERROR_MALFORMED_DATA): return Upscaler::setStatus(FatalRuntimeError, t_msg + " | FFX_ERROR_MALFORMED_DATA");
        case static_cast<int>(FFX_ERROR_OUT_OF_MEMORY): return Upscaler::setStatus(OutOfMemory, t_msg + " | FFX_ERROR_OUT_OF_MEMORY");
        case static_cast<int>(FFX_ERROR_INCOMPLETE_INTERFACE): return Upscaler::setStatus(RecoverableRuntimeError, t_msg + " | FFX_ERROR_INCOMPLETE_INTERFACE");
        case static_cast<int>(FFX_ERROR_INVALID_ENUM): return Upscaler::setStatus(RecoverableRuntimeError, t_msg + " | FFX_ERROR_INVALID_ENUM");
        case static_cast<int>(FFX_ERROR_INVALID_ARGUMENT): return Upscaler::setStatus(RecoverableRuntimeError, t_msg + " | FFX_ERROR_INVALID_ARGUMENT");
        case static_cast<int>(FFX_ERROR_OUT_OF_RANGE): return Upscaler::setStatus(FatalRuntimeError, t_msg + " | FFX_ERROR_OUT_OF_RANGE");
        case static_cast<int>(FFX_ERROR_NULL_DEVICE): return Upscaler::setStatus(FatalRuntimeError, t_msg + " | FFX_ERROR_NULL_DEVICE");
        case static_cast<int>(FFX_ERROR_BACKEND_API_ERROR): return Upscaler::setStatus(FatalRuntimeError, t_msg + " | FFX_ERROR_BACKEND_API_ERROR");
        case static_cast<int>(FFX_ERROR_INSUFFICIENT_MEMORY): return Upscaler::setStatus(OutOfMemory, t_msg + " | FFX_ERROR_INSUFFICIENT_MEMORY");
        default: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | Unknown");
    }
}

void FSR2::log(const FfxMsgType type, const wchar_t *t_msg) {
    std::wstring message(t_msg);
    std::string  msg(message.length(), 0);
    std::ranges::transform(message, msg.begin(), [](const wchar_t c) { return static_cast<char>(c); });
    switch (type) {
        case FFX_MESSAGE_TYPE_ERROR: msg = "FSR2 Error ---> " + msg; break;
        case FFX_MESSAGE_TYPE_WARNING: msg = "FSR2 Warning -> " + msg; break;
        case FFX_MESSAGE_TYPE_COUNT: break;
    }
    if (logCallback != nullptr) logCallback(msg.c_str());
}

bool FSR2::isSupported() {
    if (supported != Untested)
        return supported == Supported;
    const FSR2 fsr2(GraphicsAPI::getType());
    return (supported = success(fsr2.getStatus()) ? Supported : Unsupported) == Supported;
}

bool FSR2::isSupported(const enum Settings::Quality mode) {
    return mode == Settings::Auto || mode == Settings::Quality || mode == Settings::Balanced || mode == Settings::Performance || mode == Settings::UltraPerformance;
}

FSR2::FSR2(const GraphicsAPI::Type type) {
    switch (type) {
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

Upscaler::Status FSR2::getOptimalSettings(const Settings::Resolution resolution, Settings::DLSSPreset /*unused*/, const enum Settings::Quality mode, const bool hdr) {
    Settings optimalSettings         = settings;
    optimalSettings.outputResolution = resolution;
    optimalSettings.hdr              = hdr;
    optimalSettings.quality          = mode;

    RETURN_ON_FAILURE(setStatus(ffxFsr2GetRenderResolutionFromQualityMode(&optimalSettings.recommendedInputResolution.width, &optimalSettings.recommendedInputResolution.height, optimalSettings.outputResolution.width, optimalSettings.outputResolution.height, optimalSettings.getQuality<Upscaler::FSR2>()), "Some invalid setting was set. Ensure that the sharpness is between 0F and 1F, and that the QualityMode setting is a valid enum value."));
    optimalSettings.dynamicMaximumInputResolution = resolution;

    settings = optimalSettings;
    return Success;
}

Upscaler::Status FSR2::initialize() {
    if (ffxInterface == nullptr) {
        RETURN_ON_FAILURE((this->*fpInitialize)());
        ++users;
    }
    return Success;
}

Upscaler::Status FSR2::create() {
    if (context != nullptr) {
        ++users;  // Corrected by `shutdown()`. Also prevents the interface from being destroyed.
        RETURN_ON_FAILURE(shutdown());
    }
    // clang-format off
    const FfxFsr2ContextDescription description{
      .flags =
#    ifndef NDEBUG
        static_cast<unsigned>(FFX_FSR2_ENABLE_DEBUG_CHECKING) |
#    endif
        static_cast<unsigned>(FFX_FSR2_ENABLE_AUTO_EXPOSURE) |
        static_cast<unsigned>(FFX_FSR2_ENABLE_DEPTH_INVERTED) |
        static_cast<unsigned>(FFX_FSR2_ENABLE_DYNAMIC_RESOLUTION) |
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
    return Success;
}

Upscaler::Status FSR2::evaluate() {
    RETURN_ON_FAILURE((this->*fpEvaluate)());
    settings.resetHistory = false;
    return Success;
}

Upscaler::Status FSR2::shutdown() {
    if (context != nullptr) {
        RETURN_ON_FAILURE(setStatus(ffxFsr2ContextDestroy(context), "Failed to destroy the " + getName() + " context."));
        delete context;
        context = nullptr;
    }
    if (ffxInterface != nullptr && --users == 0) {
        operator delete(ffxInterface->scratchBuffer, ffxInterface->scratchBufferSize);
        delete ffxInterface;
        ffxInterface = nullptr;
    }
    return Success;
}
#endif