#ifdef ENABLE_DLSS
#    include "DLSS.hpp"
#    ifdef ENABLE_DX11
#        include "GraphicsAPI/DX11.hpp"

#        include <d3d11.h>

#        include <IUnityGraphicsD3D11.h>
#    endif
#    ifdef ENABLE_DX12
#        include "GraphicsAPI/DX12.hpp"

#        include <d3d12compatibility.h>

#        include <IUnityGraphicsD3D12.h>
#    endif
#    ifdef ENABLE_VULKAN
#        include "GraphicsAPI/Vulkan.hpp"

#        include <IUnityGraphicsVulkan.h>

#        include <IUnityRenderingExtensions.h>
#    endif

#    include <algorithm>
#    include <cstring>

#    ifdef ENABLE_VULKAN
void DLSS::RAII_NGXVulkanResource::ChangeResource(VkImageView view, VkImage image, VkImageAspectFlags aspect, VkFormat format, Settings::Resolution resolution) {
    Destroy();
    resource = {
      .Resource = {
        .ImageViewInfo = {
          .ImageView        = view,
          .Image            = image,
          .SubresourceRange = {
            .aspectMask     = aspect,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
          },
          .Format = format,
          .Width  = resolution.width,
          .Height = resolution.height,
        }
      },
      .Type      = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW,
      .ReadWrite = true
    };
}

NVSDK_NGX_Resource_VK& DLSS::RAII_NGXVulkanResource::GetResource() {
    return resource;
}

DLSS::RAII_NGXVulkanResource::~RAII_NGXVulkanResource() {
    Destroy();
}

void DLSS::RAII_NGXVulkanResource::Destroy() {
    Vulkan::destroyImageView(resource.Resource.ImageViewInfo.ImageView);
    resource = {};
}
#    endif

DLSS::Application DLSS::applicationInfo;

Upscaler::Status (DLSS::*DLSS::fpInitialize)(){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::fpCreate)(){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::fpEvaluate)(){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::fpRelease)(){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::fpShutdown)(){&DLSS::safeFail};

Upscaler::SupportState DLSS::supported{UNTESTED};
#    ifdef ENABLE_VULKAN
Upscaler::SupportState DLSS::instanceExtensionsSupported{UNTESTED};
Upscaler::SupportState DLSS::deviceExtensionsSupported{UNTESTED};
#    endif

uint32_t DLSS::users{};

#    ifdef ENABLE_VULKAN
Upscaler::Status DLSS::VulkanInitialize() {
    const UnityVulkanInstance vulkanInstance = Vulkan::getGraphicsInterface()->Instance();

    RETURN_ON_FAILURE(setStatus(NVSDK_NGX_VULKAN_Init(applicationInfo.ngxIdentifier.v.ApplicationId, applicationInfo.featureCommonInfo.PathListInfo.Path[0], vulkanInstance.instance, vulkanInstance.physicalDevice, vulkanInstance.device), "Failed to initialize the NGX instance."));
    RETURN_ON_FAILURE(setStatusIf(parameters != nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Parameters already exist!"));
    RETURN_ON_FAILURE(setStatus(NVSDK_NGX_VULKAN_GetCapabilityParameters(&parameters), "Failed to get the " + getName() + " compatibility parameters."));
    RETURN_ON_FAILURE(setStatusIf(parameters == nullptr, SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, "Parameters are invalid after attempting to build them."));
    return SUCCESS;
}

Upscaler::Status DLSS::VulkanCreate() {
    UnityVulkanRecordingState state{};
    Vulkan::getGraphicsInterface()->EnsureInsideRenderPass();
    Vulkan::getGraphicsInterface()->CommandRecordingState(&state, kUnityVulkanGraphicsQueueAccess_DontCare);

    RETURN_ON_FAILURE(setStatus(NGX_VULKAN_CREATE_DLSS_EXT1(Vulkan::getGraphicsInterface()->Instance().device, state.commandBuffer, 1U, 1U, &featureHandle, parameters, &DLSSCreateParams), "Failed to create the " + getName() + " feature."));
    return SUCCESS;
}

Upscaler::Status DLSS::VulkanUpdateResource(RAII_NGXVulkanResource* resource, Plugin::ImageID imageID) {
    RETURN_ON_FAILURE(Upscaler::setStatusIf(imageID >= Plugin::ImageID::IMAGE_ID_MAX_ENUM, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Attempted to get a NGX resource from a nonexistent image."));

    VkAccessFlags flags{VK_ACCESS_MEMORY_READ_BIT};
    if (imageID == Plugin::ImageID::OutputColor) flags = VK_ACCESS_MEMORY_WRITE_BIT;
    UnityVulkanImage image{};
    Vulkan::getGraphicsInterface()->AccessTextureByID(textureIDs[imageID], UnityVulkanWholeImage, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, flags, kUnityVulkanResourceAccess_PipelineBarrier, &image);

    VkImageView view  = Vulkan::createImageView(image.image, image.format, image.aspect);
    RETURN_ON_FAILURE(setStatusIf(view == VK_NULL_HANDLE, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Failed to create a valid `VkImageView`."));
    resource->ChangeResource(view, image.image, image.aspect, image.format, {image.extent.width, image.extent.height});
    return SUCCESS;
}

Upscaler::Status DLSS::VulkanEvaluate() {
    RETURN_ON_FAILURE(VulkanUpdateResource(color, Plugin::ImageID::SourceColor));
    RETURN_ON_FAILURE(VulkanUpdateResource(depth, Plugin::ImageID::SourceDepth));
    RETURN_ON_FAILURE(VulkanUpdateResource(motion, Plugin::ImageID::Motion));
    RETURN_ON_FAILURE(VulkanUpdateResource(output, Plugin::ImageID::OutputColor));

    // clang-format off
    NVSDK_NGX_VK_DLSS_Eval_Params DLSSEvalParameters = {
      .Feature = {
        .pInColor = &color->GetResource(),
        .pInOutput = &output->GetResource(),
        .InSharpness = settings.sharpness,
      },
      .pInDepth                  = &depth->GetResource(),
      .pInMotionVectors          = &motion->GetResource(),
      .InJitterOffsetX           = settings.jitter.x,
      .InJitterOffsetY           = settings.jitter.y,
      .InRenderSubrectDimensions = {
        .Width  = settings.renderingResolution.width,
        .Height = settings.renderingResolution.height,
      },
      .InReset    = static_cast<int>(settings.resetHistory),
      .InMVScaleX = -static_cast<float>(settings.renderingResolution.width),
      .InMVScaleY = -static_cast<float>(settings.renderingResolution.height),
#       ifndef NDEBUG
      .InIndicatorInvertYAxis = 1,
#       endif
      .InFrameTimeDeltaInMsec = settings.frameTime
    };
    // clang-format on

    settings.resetHistory = false;

    UnityVulkanRecordingState state{};
    Vulkan::getGraphicsInterface()->EnsureInsideRenderPass();
    Vulkan::getGraphicsInterface()->CommandRecordingState(&state, kUnityVulkanGraphicsQueueAccess_DontCare);

    RETURN_ON_FAILURE(setStatus(NGX_VULKAN_EVALUATE_DLSS_EXT(state.commandBuffer, featureHandle, parameters, &DLSSEvalParameters), "Failed to evaluate the " + getName() + " feature."));
    return SUCCESS;
}

Upscaler::Status DLSS::VulkanRelease() {
    RETURN_ON_FAILURE(setStatusIf(featureHandle == nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Feature has already been released."));
    RETURN_ON_FAILURE(setStatus(NVSDK_NGX_VULKAN_ReleaseFeature(featureHandle), "Failed to release the " + getName() + " feature."));
    featureHandle = nullptr;
    return SUCCESS;
}

Upscaler::Status DLSS::VulkanShutdown() {
    if (featureHandle != nullptr)
        RETURN_ON_FAILURE(VulkanRelease());
    if (parameters != nullptr) {
        RETURN_ON_FAILURE(setStatus(NVSDK_NGX_VULKAN_DestroyParameters(parameters), "Failed to release the " + getName() + " compatibility parameters."));
        parameters = nullptr;
    }

    color->Destroy();
    delete color;
    depth->Destroy();
    delete depth;
    motion->Destroy();
    delete motion;
    output->Destroy();
    delete output;

    if (--users == 0)
        RETURN_ON_FAILURE(setStatus(NVSDK_NGX_VULKAN_Shutdown1(Vulkan::getGraphicsInterface()->Instance().device), "Failed to shutdown the NGX instance."));

    return SUCCESS;
}
#    endif

#    ifdef ENABLE_DX12
Upscaler::Status DLSS::DX12Initialize() {
    RETURN_ON_FAILURE(setStatus(NVSDK_NGX_D3D12_Init(applicationInfo.ngxIdentifier.v.ApplicationId, applicationInfo.featureCommonInfo.PathListInfo.Path[0], DX12::getGraphicsInterface()->GetDevice()), "Failed to initialize the NGX instance."));
    RETURN_ON_FAILURE(setStatusIf(parameters != nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Parameters already exist!"));
    RETURN_ON_FAILURE(setStatus(NVSDK_NGX_D3D12_GetCapabilityParameters(&parameters), "Failed to get the " + getName() + " compatibility parameters"));
    RETURN_ON_FAILURE(setStatusIf(parameters == nullptr, SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, "Parameters are invalid after attempting to build them."));
    return SUCCESS;
}

Upscaler::Status DLSS::DX12Create() {
    UnityGraphicsD3D12RecordingState state{};
    DX12::getGraphicsInterface()->CommandRecordingState(&state);

    RETURN_ON_FAILURE(setStatus(NGX_D3D12_CREATE_DLSS_EXT(state.commandList, 1U, 1U, &featureHandle, parameters, &DLSSCreateParams), "Failed to create the " + getName() + " feature."));
    return SUCCESS;
}

Upscaler::Status DLSS::DX12Evaluate() {
    // clang-format off
    NVSDK_NGX_D3D12_DLSS_Eval_Params DLSSEvalParameters {
      .Feature = {
        .pInColor = DX12::getGraphicsInterface()->TextureFromNativeTexture(textureIDs[Plugin::ImageID::SourceColor]),
        .pInOutput = DX12::getGraphicsInterface()->TextureFromNativeTexture(textureIDs[Plugin::ImageID::OutputColor]),
        .InSharpness = settings.sharpness,
      },
      .pInDepth                  = DX12::getGraphicsInterface()->TextureFromNativeTexture(textureIDs[Plugin::ImageID::SourceDepth]),
      .pInMotionVectors          = DX12::getGraphicsInterface()->TextureFromNativeTexture(textureIDs[Plugin::ImageID::Motion]),
      .InJitterOffsetX           = settings.jitter.x,
      .InJitterOffsetY           = settings.jitter.y,
      .InRenderSubrectDimensions = {
        .Width  = settings.renderingResolution.width,
        .Height = settings.renderingResolution.height,
      },
      .InReset    = static_cast<int>(settings.resetHistory),
      .InMVScaleX = -static_cast<float>(settings.renderingResolution.width),
      .InMVScaleY = -static_cast<float>(settings.renderingResolution.height),
#ifndef NDEBUG
      .InIndicatorInvertYAxis = 1,
#endif
      .InFrameTimeDeltaInMsec=settings.frameTime,
    };
    // clang-format on

    UnityGraphicsD3D12RecordingState state{};
    DX12::getGraphicsInterface()->CommandRecordingState(&state);

    RETURN_ON_FAILURE(setStatus(NGX_D3D12_EVALUATE_DLSS_EXT(state.commandList, featureHandle, parameters, &DLSSEvalParameters), "Failed to evaluate the " + getName() + " feature."));
    return SUCCESS;
}

Upscaler::Status DLSS::DX12Release() {
    RETURN_ON_FAILURE(setStatusIf(featureHandle == nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Feature has already been released."));
    RETURN_ON_FAILURE(setStatus(NVSDK_NGX_D3D12_ReleaseFeature(featureHandle), "Failed to release the " + getName() + " feature."));
    featureHandle = nullptr;
    return SUCCESS;
}

Upscaler::Status DLSS::DX12Shutdown() {
    if (featureHandle != nullptr)
        RETURN_ON_FAILURE(DX12Release());
    if (parameters != nullptr) {
        RETURN_ON_FAILURE(setStatus(NVSDK_NGX_D3D12_DestroyParameters(parameters), "Failed to release the " + getName() + " compatibility parameters."));
        parameters = nullptr;
    }

    if (--users == 0)
        RETURN_ON_FAILURE(setStatus(NVSDK_NGX_D3D12_Shutdown1(DX12::getGraphicsInterface()->GetDevice()), "Failed to shutdown the NGX instance."));
    return SUCCESS;
}
#    endif

#    ifdef ENABLE_DX11
Upscaler::Status DLSS::DX11Initialize() {
    RETURN_ON_FAILURE(setStatus(NVSDK_NGX_D3D11_Init(applicationInfo.ngxIdentifier.v.ApplicationId, applicationInfo.featureCommonInfo.PathListInfo.Path[0], DX11::getGraphicsInterface()->GetDevice()), "Failed to initialize the NGX instance."));
    RETURN_ON_FAILURE(setStatusIf(parameters != nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Parameters already exist!"));
    RETURN_ON_FAILURE(setStatus(NVSDK_NGX_D3D11_GetCapabilityParameters(&parameters), "Failed to get the " + getName() + " compatibility parameters"));
    RETURN_ON_FAILURE(setStatusIf(parameters == nullptr, SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, "Parameters are invalid after attempting to build them."));
    return SUCCESS;
}

Upscaler::Status DLSS::DX11Create() {
    ID3D11DeviceContext* context = DX11::getOneTimeSubmitContext();

    RETURN_ON_FAILURE(setStatus(NGX_D3D11_CREATE_DLSS_EXT(context, &featureHandle, parameters, &DLSSCreateParams), "Failed to create the " + getName() + " feature."));
    return SUCCESS;
}

Upscaler::Status DLSS::DX11Evaluate() {
    // clang-format off
    NVSDK_NGX_D3D11_DLSS_Eval_Params DLSSEvalParams {
      .Feature = {
        .pInColor = DX11::getGraphicsInterface()->TextureFromNativeTexture(textureIDs[Plugin::ImageID::SourceColor]),
        .pInOutput = DX11::getGraphicsInterface()->TextureFromNativeTexture(textureIDs[Plugin::ImageID::OutputColor]),
        .InSharpness = settings.sharpness,
      },
      .pInDepth                  = DX11::getGraphicsInterface()->TextureFromNativeTexture(textureIDs[Plugin::ImageID::SourceDepth]),
      .pInMotionVectors          = DX11::getGraphicsInterface()->TextureFromNativeTexture(textureIDs[Plugin::ImageID::Motion]),
      .InJitterOffsetX           = settings.jitter.x,
      .InJitterOffsetY           = settings.jitter.y,
      .InRenderSubrectDimensions = {
        .Width  = settings.renderingResolution.width,
        .Height = settings.renderingResolution.height,
      },
      .InReset    = static_cast<int>(settings.resetHistory),
      .InMVScaleX = -static_cast<float>(settings.renderingResolution.width),
      .InMVScaleY = -static_cast<float>(settings.renderingResolution.height),
#ifndef NDEBUG
      .InIndicatorInvertYAxis = 1,
#endif
      .InFrameTimeDeltaInMsec = settings.frameTime,
    };
    // clang-format on

    ID3D11DeviceContext* context = DX11::getOneTimeSubmitContext();

    RETURN_ON_FAILURE(setStatus(NGX_D3D11_EVALUATE_DLSS_EXT(context, featureHandle, parameters, &DLSSEvalParams), "Failed to evaluate the " + getName() + " feature."));
    return SUCCESS;
}

Upscaler::Status DLSS::DX11Release() {
    RETURN_ON_FAILURE(setStatusIf(featureHandle == nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Feature has already been released."));
    RETURN_ON_FAILURE(setStatus(NVSDK_NGX_D3D11_ReleaseFeature(featureHandle), "Failed to release the " + getName() + " feature."));
    featureHandle = nullptr;
    return SUCCESS;
}

Upscaler::Status DLSS::DX11Shutdown() {
    if (featureHandle != nullptr)
        RETURN_ON_FAILURE(DX11Release());
    if (parameters != nullptr) {
        RETURN_ON_FAILURE(setStatus(NVSDK_NGX_D3D11_DestroyParameters(parameters), "Failed to release the " + getName() + " compatibility parameters."));
        parameters = nullptr;
    }

    if (--users == 0)
        RETURN_ON_FAILURE(setStatus(NVSDK_NGX_D3D11_Shutdown1(DX11::getGraphicsInterface()->GetDevice()), "Failed to shutdown the NGX instance."));
    return SUCCESS;
}
#    endif

Upscaler::Status DLSS::setStatus(const NVSDK_NGX_Result t_error, std::string t_msg) {
    std::wstring message = GetNGXResultAsString(t_error);
    std::string  msg(message.length(), 0);
    std::ranges::transform(message, msg.begin(), [](const wchar_t c) { return static_cast<char>(c); });
    t_msg += " | " + msg;
    Status error = SUCCESS;
    switch (t_error) {
        case NVSDK_NGX_Result_Success: return getStatus();
        case NVSDK_NGX_Result_Fail: error = UNKNOWN_ERROR; break;
        case NVSDK_NGX_Result_FAIL_FeatureNotSupported: error = HARDWARE_ERROR_DEVICE_NOT_SUPPORTED; break;
        case NVSDK_NGX_Result_FAIL_PlatformError: error = SOFTWARE_ERROR_OPERATING_SYSTEM_NOT_SUPPORTED; break;
        case NVSDK_NGX_Result_FAIL_FeatureAlreadyExists: error = SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING; break;
        case NVSDK_NGX_Result_FAIL_FeatureNotFound: error = SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR; break;
        case NVSDK_NGX_Result_FAIL_InvalidParameter: error = SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING; break;
        case NVSDK_NGX_Result_FAIL_ScratchBufferTooSmall: error = SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING; break;
        case NVSDK_NGX_Result_FAIL_NotInitialized: error = SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING; break;
        case NVSDK_NGX_Result_FAIL_UnsupportedInputFormat:
        case NVSDK_NGX_Result_FAIL_RWFlagMissing: error = SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING; break;
        case NVSDK_NGX_Result_FAIL_MissingInput: error = SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING; break;
        case NVSDK_NGX_Result_FAIL_UnableToInitializeFeature: error = SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING; break;
        case NVSDK_NGX_Result_FAIL_OutOfDate: error = SOFTWARE_ERROR_DEVICE_DRIVERS_OUT_OF_DATE; break;
        case NVSDK_NGX_Result_FAIL_OutOfGPUMemory: error = SOFTWARE_ERROR_OUT_OF_GPU_MEMORY; break;
        case NVSDK_NGX_Result_FAIL_UnsupportedFormat: error = SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING; break;
        case NVSDK_NGX_Result_FAIL_UnableToWriteToAppDataPath: error = SOFTWARE_ERROR_INVALID_WRITE_PERMISSIONS; break;
        case NVSDK_NGX_Result_FAIL_UnsupportedParameter: error = SETTINGS_ERROR; break;
        case NVSDK_NGX_Result_FAIL_Denied: error = SOFTWARE_ERROR_FEATURE_DENIED; break;
        case NVSDK_NGX_Result_FAIL_NotImplemented: error = SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR; break;
    }
    return Upscaler::setStatus(error, t_msg);
}

void DLSS::log(const char* message, NVSDK_NGX_Logging_Level loggingLevel, NVSDK_NGX_Feature sourceComponent) {
    std::string msg;
    switch (loggingLevel) {
        case NVSDK_NGX_LOGGING_LEVEL_OFF: break;
        case NVSDK_NGX_LOGGING_LEVEL_ON: msg = "DLSS ---------> "; break;
        case NVSDK_NGX_LOGGING_LEVEL_VERBOSE: msg = "DLSS Verbose -> "; break;
        case NVSDK_NGX_LOGGING_LEVEL_NUM: break;
    }
    if (Upscaler::logCallback != nullptr) Upscaler::logCallback((msg + message).c_str());
}

DLSS::DLSS(const GraphicsAPI::Type graphicsAPI) {
    switch (graphicsAPI) {
        case GraphicsAPI::NONE: {
            fpInitialize = &DLSS::safeFail;
            fpCreate     = &DLSS::safeFail;
            fpEvaluate   = &DLSS::safeFail;
            fpRelease    = &DLSS::safeFail;
            fpShutdown   = &DLSS::safeFail;
            break;
        }
#    ifdef ENABLE_VULKAN
        case GraphicsAPI::VULKAN: {
            color  = new RAII_NGXVulkanResource;
            output = new RAII_NGXVulkanResource;
            depth  = new RAII_NGXVulkanResource;
            motion = new RAII_NGXVulkanResource;

            fpInitialize = &DLSS::VulkanInitialize;
            fpCreate     = &DLSS::VulkanCreate;
            fpEvaluate   = &DLSS::VulkanEvaluate;
            fpRelease    = &DLSS::VulkanRelease;
            fpShutdown   = &DLSS::VulkanShutdown;
            break;
        }
#    endif
#    ifdef ENABLE_DX12
        case GraphicsAPI::DX12: {
            fpInitialize = &DLSS::DX12Initialize;
            fpCreate     = &DLSS::DX12Create;
            fpEvaluate   = &DLSS::DX12Evaluate;
            fpRelease    = &DLSS::DX12Release;
            fpShutdown   = &DLSS::DX12Shutdown;
            break;
        }
#    endif
#    ifdef ENABLE_DX11
        case GraphicsAPI::DX11: {
            fpInitialize = &DLSS::DX11Initialize;
            fpCreate     = &DLSS::DX11Create;
            fpEvaluate   = &DLSS::DX11Evaluate;
            fpRelease    = &DLSS::DX11Release;
            fpShutdown   = &DLSS::DX11Shutdown;
            break;
        }
#    endif
        default: {
            fpInitialize = &DLSS::safeFail;
            fpCreate     = &DLSS::safeFail;
            fpEvaluate   = &DLSS::safeFail;
            fpRelease    = &DLSS::safeFail;
            fpShutdown   = &DLSS::safeFail;
            break;
        }
    }
    initialize();
}

DLSS::~DLSS() {
    shutdown();
}

#    ifdef ENABLE_VULKAN
std::vector<std::string> DLSS::requestVulkanInstanceExtensions(const std::vector<std::string>& supportedExtensions) {
    uint32_t                 extensionCount{};
    std::vector<std::string> requestedExtensions{};
    VkExtensionProperties*   extensionProperties{};
    NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(&applicationInfo.featureDiscoveryInfo, &extensionCount, &extensionProperties);
    requestedExtensions.reserve(extensionCount);
    for (uint32_t i{}; i < extensionCount; ++i) {
        const char* extensionName = extensionProperties[i].extensionName;
        if (std::find_if(supportedExtensions.begin(), supportedExtensions.end(), [&extensionName](const std::string& str) { return strcmp(str.c_str(), extensionName) == 0; }) != supportedExtensions.end()) {
            requestedExtensions.emplace_back(extensionName);
        } else {
            instanceExtensionsSupported = UNSUPPORTED;
            return {};
        }
    }
    instanceExtensionsSupported = SUPPORTED;
    return requestedExtensions;
}

std::vector<std::string> DLSS::requestVulkanDeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice, const std::vector<std::string>& supportedExtensions) {
    uint32_t                 extensionCount{};
    std::vector<std::string> requestedExtensions{};
    VkExtensionProperties*   extensionProperties{};
    NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(instance, physicalDevice, &applicationInfo.featureDiscoveryInfo, &extensionCount, &extensionProperties);
    requestedExtensions.reserve(extensionCount);
    for (uint32_t i{}; i < extensionCount; ++i) {
        const char* extensionName = extensionProperties[i].extensionName;
        if (std::find_if(supportedExtensions.begin(), supportedExtensions.end(), [&extensionName](const std::string& str) { return strcmp(str.c_str(), extensionName) == 0; }) != supportedExtensions.end()) {
            requestedExtensions.emplace_back(extensionName);
        } else {
            deviceExtensionsSupported = UNSUPPORTED;
            return {};
        }
    }
    deviceExtensionsSupported = SUPPORTED;
    return requestedExtensions;
}
#    endif

bool DLSS::isSupported() {
    if (supported != UNTESTED)
        return supported == SUPPORTED;
#   ifdef ENABLE_VULKAN
    if (instanceExtensionsSupported == UNSUPPORTED || deviceExtensionsSupported == UNSUPPORTED) {
        supported = UNSUPPORTED;
        return false;
    }
#   endif
    return (supported = success(getStatus()) ? SUPPORTED : UNSUPPORTED) == SUPPORTED;
}

Upscaler::Status
DLSS::getOptimalSettings(const Settings::Resolution resolution, const Settings::QualityMode mode, const bool hdr) {
    RETURN_ON_FAILURE(setStatusIf(parameters == nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Parameters do not exist!"));
    RETURN_ON_FAILURE(setStatusIf(resolution.height < 32, SETTINGS_ERROR_INVALID_OUTPUT_RESOLUTION, "The output resolution must be more than 32 pixels in height."));
    RETURN_ON_FAILURE(setStatusIf(resolution.width < 32, SETTINGS_ERROR_INVALID_OUTPUT_RESOLUTION, "The output resolution must be more than 32 pixels in width."));
    RETURN_ON_FAILURE(setStatusIf(mode >= Upscaler::Settings::QUALITY_MODE_MAX_ENUM, SETTINGS_ERROR_QUALITY_MODE_NOT_AVAILABLE, "The selected quality mode is unavailable or invalid."));

    Settings optimalSettings         = settings;
    optimalSettings.outputResolution = resolution;
    optimalSettings.HDR              = hdr;
    optimalSettings.quality          = mode;

    RETURN_ON_FAILURE(setStatus(NGX_DLSS_GET_OPTIMAL_SETTINGS(parameters, optimalSettings.outputResolution.width, optimalSettings.outputResolution.height, optimalSettings.getQuality<Upscaler::DLSS>(), &optimalSettings.renderingResolution.width, &optimalSettings.renderingResolution.height, &optimalSettings.dynamicMaximumInputResolution.width, &optimalSettings.dynamicMaximumInputResolution.height, &optimalSettings.dynamicMinimumInputResolution.width, &optimalSettings.dynamicMinimumInputResolution.height, &optimalSettings.sharpness), "Some invalid setting was set. Ensure that the current input resolution is within allowed bounds given the output resolution, sharpness is between 0.0 and 1.0, and that the Quality setting is a valid enum value."));

    RETURN_ON_FAILURE(setStatusIf(optimalSettings.renderingResolution.width == 0, Upscaler::Status::SETTINGS_ERROR_INVALID_INPUT_RESOLUTION, "The input resolution's width cannot be zero."));
    RETURN_ON_FAILURE(setStatusIf(optimalSettings.renderingResolution.height == 0, Upscaler::Status::SETTINGS_ERROR_INVALID_INPUT_RESOLUTION, "The input resolution's height cannot be zero."));
    settings = optimalSettings;
    settings.jitterGenerator.generate(settings.renderingResolution, settings.outputResolution);
    return SUCCESS;
}

Upscaler::Status DLSS::initialize() {
    RETURN_ON_FAILURE(setStatusIf(initialized, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, getName() + " is already initialized!"));
    if (!resetStatus()) return getStatus();

    // Upscaler_Initialize NGX SDK
    RETURN_ON_FAILURE((this->*fpInitialize)());
    // Check for DLSS support
    // Is driver up-to-date
    int needsUpdatedDriver{};
    RETURN_ON_FAILURE(setStatus(parameters->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsUpdatedDriver), "Failed to query the selected device's driver update needs. This may result from outdated an driver, unsupported GPUs, a failure to initialize NGX, or a failure to get the " + getName() + " compatibility parameters."));
    if (needsUpdatedDriver != 0) {
        int requiredMajorDriverVersion{};
        RETURN_ON_FAILURE(setStatus(parameters->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, &requiredMajorDriverVersion), "Failed to query the major version of the minimum " + getName() + " supported driver for the selected graphics device. This may result from outdated an driver, unsupported GPUs, a failure to initialize NGX, or a failure to get the " + getName() + " compatibility parameters."));
        int requiredMinorDriverVersion{};
        RETURN_ON_FAILURE(setStatus(parameters->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, &requiredMinorDriverVersion), "Failed to query the minor version of the minimum " + getName() + " supported driver for the selected graphics device. This may result from outdated an driver, unsupported GPUs, a failure to initialize NGX, or a failure to get the " + getName() + " compatibility parameters."));
        return Upscaler::setStatus(SOFTWARE_ERROR_DEVICE_DRIVERS_OUT_OF_DATE, "The selected device's drivers are out-of-date. They must be (" + std::to_string(requiredMajorDriverVersion) + "." + std::to_string(requiredMinorDriverVersion) + ") at a minimum.");
    }
    // Is DLSS available on this hardware and platform
    int DLSSSupported{};
    RETURN_ON_FAILURE(setStatus(parameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &DLSSSupported), "Failed to query status of " + getName() + " support for the selected graphics device. This may result from outdated an driver, unsupported GPUs, a failure to initialize NGX, or a failure to get the " + getName() + " compatibility parameters."));
    RETURN_ON_FAILURE(setStatusIf(DLSSSupported == 0, GENERIC_ERROR_DEVICE_OR_INSTANCE_EXTENSIONS_NOT_SUPPORTED, getName() + " is not supported on the selected graphics device. If you are certain that you have a DLSS compatible device, please ensure that it is being used by Unity for rendering."));
    // Is DLSS denied for this application
    RETURN_ON_FAILURE(setStatus(parameters->Get(NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, &DLSSSupported), "Failed to query the major version of the minimum " + getName() + " supported driver for the selected graphics device. This may result from outdated driver, unsupported GPUs, a failure to initialize NGX, or a failure to get the " + getName() + " compatibility parameters."));
    RETURN_ON_FAILURE(setStatusIf(DLSSSupported == 0, SOFTWARE_ERROR_FEATURE_DENIED, getName() + " has been denied for this application. Consult an NVIDIA representative for more information."));

    initialized = true;
    users += 1;
    return SUCCESS;
}

Upscaler::Status DLSS::create() {
    RETURN_ON_FAILURE(setStatusIf(parameters == nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Parameters do not exist!"));
    if (featureHandle != nullptr)
        RETURN_ON_FAILURE((this->*fpRelease)());
    // clang-format off
    DLSSCreateParams = {
      .Feature = {
        .InWidth            = settings.renderingResolution.width,
        .InHeight           = settings.renderingResolution.height,
        .InTargetWidth      = settings.outputResolution.width,
        .InTargetHeight     = settings.outputResolution.height,
        .InPerfQualityValue = settings.getQuality<Upscaler::DLSS>(),
      },
      .InFeatureCreateFlags = static_cast<int>(
        static_cast<unsigned>(NVSDK_NGX_DLSS_Feature_Flags_MVLowRes) |
        static_cast<unsigned>(NVSDK_NGX_DLSS_Feature_Flags_MVJittered) |
        static_cast<unsigned>(NVSDK_NGX_DLSS_Feature_Flags_DepthInverted) |
        static_cast<unsigned>(NVSDK_NGX_DLSS_Feature_Flags_AutoExposure) |
        static_cast<unsigned>(NVSDK_NGX_DLSS_Feature_Flags_DoSharpening) |
        (settings.HDR ? NVSDK_NGX_DLSS_Feature_Flags_IsHDR : 0U)
      ),
      .InEnableOutputSubrects = false,
    };
    // clang-format on

    RETURN_ON_FAILURE((this->*fpCreate)());
    RETURN_ON_FAILURE(setStatusIf(featureHandle == nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Failed to create the " + getName() + " feature. The handle returned from `NGX_*_CREATE_DLSS_EXT1()` was `nullptr`."));
    return SUCCESS;
}

Upscaler::Status DLSS::evaluate() {
    RETURN_ON_FAILURE(setStatusIf(parameters == nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Parameters do not exist!"));
    RETURN_ON_FAILURE(setStatusIf(featureHandle == nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Feature does not exist!"));

    RETURN_ON_FAILURE((this->*fpEvaluate)());
    return SUCCESS;
}

Upscaler::Status DLSS::shutdown() {
    if (parameters != nullptr)
        NVSDK_NGX_Parameter_SetI(parameters, NVSDK_NGX_Parameter_FreeMemOnReleaseFeature, 1);
    RETURN_ON_FAILURE((this->*fpShutdown)());
    initialized = false;
    return SUCCESS;
}
#endif