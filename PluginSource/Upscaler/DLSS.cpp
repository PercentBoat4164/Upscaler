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

#        include <nvsdk_ngx_helpers_vk.h>

#        include <IUnityGraphicsVulkan.h>
#    endif

#    include "nvsdk_ngx_helpers.h"

#    include <algorithm>
#    include <atomic>
#    include <cstring>

DLSS::Application DLSS::applicationInfo;

Upscaler::Status (DLSS::*DLSS::fpInitialize)(){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::fpCreate)(NVSDK_NGX_DLSS_Create_Params*){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::fpEvaluate)(){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::fpGetParameters)(){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::fpRelease)(){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::fpShutdown)(){&DLSS::safeFail};

Upscaler::SupportState DLSS::supported{Untested};
std::atomic<uint32_t> DLSS::users{};

#    ifdef ENABLE_VULKAN
Upscaler::Status DLSS::VulkanGetParameters() {
    return setStatus(NVSDK_NGX_VULKAN_GetCapabilityParameters(&parameters), "Failed to get the " + getName() + " compatibility parameters.");
}

Upscaler::Status DLSS::VulkanInitialize() {
    const UnityVulkanInstance vulkanInstance = Vulkan::getGraphicsInterface()->Instance();
    RETURN_ON_FAILURE(setStatus(NVSDK_NGX_VULKAN_Init(applicationInfo.ngxIdentifier.v.ApplicationId, applicationInfo.featureCommonInfo.PathListInfo.Path[0], vulkanInstance.instance, vulkanInstance.physicalDevice, vulkanInstance.device), "Failed to initialize the NGX instance."));
    return VulkanGetParameters();
}

Upscaler::Status DLSS::VulkanCreate(NVSDK_NGX_DLSS_Create_Params* createParams) {
    VkCommandBuffer buffer = Vulkan::getOneTimeSubmitCommandBuffer();
    RETURN_ON_FAILURE(setStatusIf(buffer == VK_NULL_HANDLE, FatalRuntimeError, "Failed to create a one-time-submit command buffer."));
    RETURN_ON_FAILURE(setStatus(NGX_VULKAN_CREATE_DLSS_EXT1(Vulkan::getGraphicsInterface()->Instance().device, buffer, 1U, 1U, &handle, parameters, createParams), "Failed to create the " + getName() + " feature."));
    return setStatusIf(!Vulkan::submitOneTimeSubmitCommandBuffer(buffer), FatalRuntimeError, "Failed to submit one-time-submit command buffer.");
}

Upscaler::Status DLSS::VulkanGetResource(NVSDK_NGX_Resource_VK& resource, const Plugin::ImageID imageID) {
    VkAccessFlags accessFlags{VK_ACCESS_MEMORY_READ_BIT};
    VkImageLayout layout{VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL};
    if (imageID == Plugin::ImageID::Output) {
        accessFlags = VK_ACCESS_MEMORY_WRITE_BIT;
        layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    if (imageID == Plugin::ImageID::Depth) layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;

    UnityVulkanImage image{};
    Vulkan::getGraphicsInterface()->AccessTexture(textures.at(imageID), UnityVulkanWholeImage, layout, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, accessFlags, kUnityVulkanResourceAccess_PipelineBarrier, &image);
    RETURN_ON_FAILURE(setStatusIf(image.image == VK_NULL_HANDLE, RecoverableRuntimeError, "Unity provided a `VK_NULL_HANDLE` image."));

    VkImageView view = Vulkan::createImageView(image.image, image.format, image.aspect);
    RETURN_ON_FAILURE(setStatusIf(view == VK_NULL_HANDLE, FatalRuntimeError, "Failed to create a valid `VkImageView`."));
    Vulkan::destroyImageView(resource.Resource.ImageViewInfo.ImageView);
    resource = {
      .Resource = {
        .ImageViewInfo = {
          .ImageView        = view,
          .Image            = image.image,
          .SubresourceRange = {
            .aspectMask = image.aspect,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
          },
          .Format = image.format,
          .Width  = image.extent.width,
          .Height = image.extent.height,
        }
      },
      .Type      = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW,
      .ReadWrite = imageID == Plugin::ImageID::Output
    };
    return Success;
}

Upscaler::Status DLSS::VulkanEvaluate() {
    NVSDK_NGX_Resource_VK color{}, depth{}, motion{}, output{};
    RETURN_ON_FAILURE(VulkanGetResource(color, Plugin::ImageID::Color));
    RETURN_ON_FAILURE(VulkanGetResource(depth, Plugin::ImageID::Depth));
    RETURN_ON_FAILURE(VulkanGetResource(motion, Plugin::ImageID::Motion));
    RETURN_ON_FAILURE(VulkanGetResource(output, Plugin::ImageID::Output));

    NVSDK_NGX_VK_DLSS_Eval_Params DLSSEvalParameters {
      .Feature = {
        .pInColor = &color,
        .pInOutput = &output,
      },
      .pInDepth                  = &depth,
      .pInMotionVectors          = &motion,
      .InJitterOffsetX           = settings.jitter.x,
      .InJitterOffsetY           = settings.jitter.y,
      .InRenderSubrectDimensions = {
        .Width  = color.Resource.ImageViewInfo.Width,
        .Height = color.Resource.ImageViewInfo.Height,
      },
      .InReset    = static_cast<int>(settings.resetHistory),
      .InMVScaleX = -static_cast<float>(motion.Resource.ImageViewInfo.Width),
      .InMVScaleY = -static_cast<float>(motion.Resource.ImageViewInfo.Height),
#       ifndef NDEBUG
      .InIndicatorInvertYAxis = 1,
#       endif
      .InFrameTimeDeltaInMsec = settings.frameTime
    };

    UnityVulkanRecordingState state{};
    Vulkan::getGraphicsInterface()->EnsureInsideRenderPass();
    RETURN_ON_FAILURE(Upscaler::setStatusIf(!Vulkan::getGraphicsInterface()->CommandRecordingState(&state, kUnityVulkanGraphicsQueueAccess_DontCare), FatalRuntimeError, "Unable to obtain a command recording state from Unity. This is fatal."));
    return setStatus(NGX_VULKAN_EVALUATE_DLSS_EXT(state.commandBuffer, handle, parameters, &DLSSEvalParameters), "Failed to evaluate the " + getName() + " feature.");
}

Upscaler::Status DLSS::VulkanRelease() {
    RETURN_ON_FAILURE(setStatus(NVSDK_NGX_VULKAN_ReleaseFeature(handle), "Failed to release the " + getName() + " feature."));
    handle = nullptr;
    return Success;
}

Upscaler::Status DLSS::VulkanShutdown() {
    if (handle != nullptr) VulkanRelease();
    if (parameters != nullptr) {
        setStatus(NVSDK_NGX_VULKAN_DestroyParameters(parameters), "Failed to destroy the " + getName() + " compatibility parameters.");
        parameters = nullptr;
    }
    if (--users == 0) RETURN_ON_FAILURE(setStatus(NVSDK_NGX_VULKAN_Shutdown1(Vulkan::getGraphicsInterface()->Instance().device), "Failed to destroy the NGX instance."));
    return Success;
}
#    endif

#    ifdef ENABLE_DX12
Upscaler::Status DLSS::DX12GetParameters() {
    return setStatus(NVSDK_NGX_D3D12_GetCapabilityParameters(&parameters), "Failed to get the " + getName() + " compatibility parameters");
}

Upscaler::Status DLSS::DX12Initialize() {
    RETURN_ON_FAILURE(setStatus(NVSDK_NGX_D3D12_Init(applicationInfo.ngxIdentifier.v.ApplicationId, applicationInfo.featureCommonInfo.PathListInfo.Path[0], DX12::getGraphicsInterface()->GetDevice()), "Failed to initialize the NGX instance."));
    return DX12GetParameters();
}

Upscaler::Status DLSS::DX12Create(NVSDK_NGX_DLSS_Create_Params* createParams) {
    ID3D12GraphicsCommandList* list = DX12::getOneTimeSubmitCommandList();
    RETURN_ON_FAILURE(Upscaler::setStatusIf(list == nullptr, FatalRuntimeError, "Failed to get one-time-submit command list."));
    RETURN_ON_FAILURE(setStatus(NGX_D3D12_CREATE_DLSS_EXT(list, 1U, 1U, &handle, parameters, createParams), "Failed to create the " + getName() + " feature."));
    return setStatusIf(!DX12::executeOneTimeSubmitCommandList(list), FatalRuntimeError, "Failed to execute one-time-submit command list");
}

Upscaler::Status DLSS::DX12Evaluate() {
    const D3D12_RESOURCE_DESC colorDescription  = static_cast<ID3D12Resource*>(textures[Plugin::ImageID::Color])->GetDesc();
    const D3D12_RESOURCE_DESC motionDescription = static_cast<ID3D12Resource*>(textures[Plugin::ImageID::Motion])->GetDesc();

    NVSDK_NGX_D3D12_DLSS_Eval_Params DLSSEvalParameters {
      .Feature = {
        .pInColor = static_cast<ID3D12Resource*>(textures[Plugin::ImageID::Color]),
        .pInOutput = static_cast<ID3D12Resource*>(textures[Plugin::ImageID::Output]),
      },
      .pInDepth                  = static_cast<ID3D12Resource*>(textures[Plugin::ImageID::Depth]),
      .pInMotionVectors          = static_cast<ID3D12Resource*>(textures[Plugin::ImageID::Motion]),
      .InJitterOffsetX           = settings.jitter.x,
      .InJitterOffsetY           = settings.jitter.y,
      .InRenderSubrectDimensions = {
        .Width  = static_cast<uint32_t>(colorDescription.Width),
        .Height = static_cast<uint32_t>(colorDescription.Height),
      },
      .InReset    = static_cast<int>(settings.resetHistory),
      .InMVScaleX = -static_cast<float>(motionDescription.Width),
      .InMVScaleY = -static_cast<float>(motionDescription.Height),
#ifndef NDEBUG
      .InIndicatorInvertYAxis = 1,
#endif
      .InFrameTimeDeltaInMsec=settings.frameTime,
    };

    UnityGraphicsD3D12RecordingState state{};
    RETURN_ON_FAILURE(Upscaler::setStatusIf(!DX12::getGraphicsInterface()->CommandRecordingState(&state), FatalRuntimeError, "Unable to obtain a command recording state from Unity. This is fatal."));
    return setStatus(NGX_D3D12_EVALUATE_DLSS_EXT(state.commandList, handle, parameters, &DLSSEvalParameters), "Failed to evaluate the " + getName() + " feature.");
}

Upscaler::Status DLSS::DX12Release() {
    RETURN_ON_FAILURE(setStatus(NVSDK_NGX_D3D12_ReleaseFeature(handle), "Failed to release the " + getName() + " feature."));
    handle = nullptr;
    return Success;
}

Upscaler::Status DLSS::DX12Shutdown() {
    if (handle != nullptr) DX12Release();
    if (parameters != nullptr) {
        setStatus(NVSDK_NGX_D3D12_DestroyParameters(parameters), "Failed to release the " + getName() + " compatibility parameters.");
        parameters = nullptr;
    }
    if (--users == 0) RETURN_ON_FAILURE(setStatus(NVSDK_NGX_D3D12_Shutdown1(DX12::getGraphicsInterface()->GetDevice()), "Failed to destroy the NGX instance."));
    return Success;
}
#    endif

#    ifdef ENABLE_DX11
Upscaler::Status DLSS::DX11GetParameters() {
    return setStatus(NVSDK_NGX_D3D11_GetCapabilityParameters(&parameters), "Failed to get the " + getName() + " compatibility parameters");
}

Upscaler::Status DLSS::DX11Initialize() {
    RETURN_ON_FAILURE(setStatus(NVSDK_NGX_D3D11_Init(applicationInfo.ngxIdentifier.v.ApplicationId, applicationInfo.featureCommonInfo.PathListInfo.Path[0], DX11::getGraphicsInterface()->GetDevice()), "Failed to initialize the NGX instance."));
    return DX11GetParameters();
}

Upscaler::Status DLSS::DX11Create(NVSDK_NGX_DLSS_Create_Params* createParams) {
    return setStatus(NGX_D3D11_CREATE_DLSS_EXT(DX11::getOneTimeSubmitContext(), &handle, parameters, createParams), "Failed to create the " + getName() + " feature.");
}

Upscaler::Status DLSS::DX11Evaluate() {
    ID3D11Texture2D* tex     = nullptr;
    RETURN_ON_FAILURE(Upscaler::setStatusIf(FAILED(static_cast<ID3D11Resource*>(textures[Plugin::ImageID::Color])->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&tex))) || tex == nullptr, FatalRuntimeError, "The data passed as color input was not a texture."));
    D3D11_TEXTURE2D_DESC colorDescription;
    tex->GetDesc(&colorDescription);
    RETURN_ON_FAILURE(Upscaler::setStatusIf(FAILED(static_cast<ID3D11Resource*>(textures[Plugin::ImageID::Motion])->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&tex))) || tex == nullptr, FatalRuntimeError, "The data passed as motion input was not a texture."));
    D3D11_TEXTURE2D_DESC motionDescription;
    tex->GetDesc(&motionDescription);

    NVSDK_NGX_D3D11_DLSS_Eval_Params DLSSEvalParams {
      .Feature = {
        .pInColor = static_cast<ID3D11Resource*>(textures[Plugin::ImageID::Color]),
        .pInOutput = static_cast<ID3D11Resource*>(textures[Plugin::ImageID::Output]),
      },
      .pInDepth                  = static_cast<ID3D11Resource*>(textures[Plugin::ImageID::Depth]),
      .pInMotionVectors          = static_cast<ID3D11Resource*>(textures[Plugin::ImageID::Motion]),
      .InJitterOffsetX           = settings.jitter.x,
      .InJitterOffsetY           = settings.jitter.y,
      .InRenderSubrectDimensions = {
        .Width  = colorDescription.Width,
        .Height = colorDescription.Height,
      },
      .InReset    = static_cast<int>(settings.resetHistory),
      .InMVScaleX = -static_cast<float>(motionDescription.Width),
      .InMVScaleY = -static_cast<float>(motionDescription.Height),
#ifndef NDEBUG
      .InIndicatorInvertYAxis = 1,
#endif
      .InFrameTimeDeltaInMsec = settings.frameTime,
    };

    return setStatus(NGX_D3D11_EVALUATE_DLSS_EXT(DX11::getOneTimeSubmitContext(), handle, parameters, &DLSSEvalParams), "Failed to evaluate the " + getName() + " feature.");
}

Upscaler::Status DLSS::DX11Release() {
    RETURN_ON_FAILURE(setStatus(NVSDK_NGX_D3D11_ReleaseFeature(handle), "Failed to release the " + getName() + " feature."));
    handle = nullptr;
    return Success;
}

Upscaler::Status DLSS::DX11Shutdown() {
    if (handle != nullptr) DX11Release();
    if (parameters != nullptr) {
        setStatus(NVSDK_NGX_D3D11_DestroyParameters(parameters), "Failed to release the " + getName() + " compatibility parameters.");
        parameters = nullptr;
    }
    if (--users == 0) RETURN_ON_FAILURE(setStatus(NVSDK_NGX_D3D11_Shutdown1(DX11::getGraphicsInterface()->GetDevice()), "Failed to destroy the NGX instance."));
    return Success;
}
#    endif

Upscaler::Status DLSS::setStatus(const NVSDK_NGX_Result t_error, std::string t_msg) {
    std::wstring message = GetNGXResultAsString(t_error);
    std::string  msg(message.length(), 0);
    std::ranges::transform(message, msg.begin(), [](const wchar_t c) { return static_cast<char>(c); });
    t_msg += " | " + msg;
    Status error = Success;
    switch (t_error) {
        case NVSDK_NGX_Result_Success: return getStatus();
        case NVSDK_NGX_Result_Fail: error = FatalRuntimeError; break;
        case NVSDK_NGX_Result_FAIL_FeatureNotSupported: error = DeviceNotSupported; break;
        case NVSDK_NGX_Result_FAIL_PlatformError: error = OperatingSystemNotSupported; break;
        case NVSDK_NGX_Result_FAIL_FeatureAlreadyExists: return getStatus();
        case NVSDK_NGX_Result_FAIL_FeatureNotFound: error = FatalRuntimeError; break;
        case NVSDK_NGX_Result_FAIL_InvalidParameter: error = RecoverableRuntimeError; break;
        case NVSDK_NGX_Result_FAIL_ScratchBufferTooSmall:
        case NVSDK_NGX_Result_FAIL_NotInitialized:
        case NVSDK_NGX_Result_FAIL_UnsupportedInputFormat:
        case NVSDK_NGX_Result_FAIL_RWFlagMissing:
        case NVSDK_NGX_Result_FAIL_MissingInput:
        case NVSDK_NGX_Result_FAIL_UnableToInitializeFeature: error = FatalRuntimeError; break;
        case NVSDK_NGX_Result_FAIL_OutOfDate: error = DriversOutOfDate; break;
        case NVSDK_NGX_Result_FAIL_OutOfGPUMemory: error = OutOfMemory; break;
        case NVSDK_NGX_Result_FAIL_UnsupportedFormat:
        case NVSDK_NGX_Result_FAIL_UnableToWriteToAppDataPath:
        case NVSDK_NGX_Result_FAIL_UnsupportedParameter: error = FatalRuntimeError; break;
        case NVSDK_NGX_Result_FAIL_Denied: error = FeatureDenied; break;
        case NVSDK_NGX_Result_FAIL_NotImplemented: error = FatalRuntimeError; break;
    }
    return Upscaler::setStatus(error, t_msg);
}

void DLSS::log(const char* message, const NVSDK_NGX_Logging_Level loggingLevel, NVSDK_NGX_Feature /*unused*/) {
    std::string msg;
    switch (loggingLevel) {
        case NVSDK_NGX_LOGGING_LEVEL_OFF: break;
        case NVSDK_NGX_LOGGING_LEVEL_ON: msg = "DLSS ---------> "; break;
        case NVSDK_NGX_LOGGING_LEVEL_VERBOSE: msg = "DLSS Verbose -> "; break;
        case NVSDK_NGX_LOGGING_LEVEL_NUM: break;
    }
    if (logCallback != nullptr) logCallback((msg + message).c_str());
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
        if (std::ranges::find_if(supportedExtensions, [&extensionName](const std::string& str) { return strcmp(str.c_str(), extensionName) == 0; }) != supportedExtensions.end()) {
            requestedExtensions.emplace_back(extensionName);
        } else {
            supported = Unsupported;
            return {};
        }
    }
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
        if (std::ranges::find_if(supportedExtensions, [&extensionName](const std::string& str) { return strcmp(str.c_str(), extensionName) == 0; }) != supportedExtensions.end()) {
            requestedExtensions.emplace_back(extensionName);
        } else {
            supported = Unsupported;
            return {};
        }
    }
    return requestedExtensions;
}
#    endif

bool DLSS::isSupported() {
    if (supported != Untested) return supported == Supported;
    DLSS dlss(GraphicsAPI::getType());
    return (supported = success(dlss.useSettings({32, 32}, Settings::DLSSPreset::Default, Settings::Quality::Auto, false)) ? Supported : Unsupported) == Supported;
}

bool DLSS::isSupported(const enum Settings::Quality mode) {
    return mode == Settings::Auto || mode == Settings::AntiAliasing || mode == Settings::Quality || mode == Settings::Balanced || mode == Settings::Performance || mode == Settings::UltraPerformance;
}

DLSS::DLSS(const GraphicsAPI::Type type) {
    switch (type) {
        case GraphicsAPI::NONE: {
            fpInitialize    = &DLSS::invalidGraphicsAPIFail;
            fpCreate        = &DLSS::invalidGraphicsAPIFail;
            fpEvaluate      = &DLSS::invalidGraphicsAPIFail;
            fpGetParameters = &DLSS::invalidGraphicsAPIFail;
            fpRelease       = &DLSS::invalidGraphicsAPIFail;
            fpShutdown      = &DLSS::invalidGraphicsAPIFail;
            break;
        }
#    ifdef ENABLE_VULKAN
        case GraphicsAPI::VULKAN: {
            fpInitialize    = &DLSS::VulkanInitialize;
            fpCreate        = &DLSS::VulkanCreate;
            fpEvaluate      = &DLSS::VulkanEvaluate;
            fpGetParameters = &DLSS::VulkanGetParameters;
            fpRelease       = &DLSS::VulkanRelease;
            fpShutdown      = &DLSS::VulkanShutdown;
            break;
        }
#    endif
#    ifdef ENABLE_DX12
        case GraphicsAPI::DX12: {
            fpInitialize    = &DLSS::DX12Initialize;
            fpCreate        = &DLSS::DX12Create;
            fpEvaluate      = &DLSS::DX12Evaluate;
            fpGetParameters = &DLSS::DX12GetParameters;
            fpRelease       = &DLSS::DX12Release;
            fpShutdown      = &DLSS::DX12Shutdown;
            break;
        }
#    endif
#    ifdef ENABLE_DX11
        case GraphicsAPI::DX11: {
            fpInitialize    = &DLSS::DX11Initialize;
            fpCreate        = &DLSS::DX11Create;
            fpEvaluate      = &DLSS::DX11Evaluate;
            fpGetParameters = &DLSS::DX11GetParameters;
            fpRelease       = &DLSS::DX11Release;
            fpShutdown      = &DLSS::DX11Shutdown;
            break;
        }
#    endif
        default: {
            fpInitialize    = &DLSS::invalidGraphicsAPIFail;
            fpCreate        = &DLSS::invalidGraphicsAPIFail;
            fpEvaluate      = &DLSS::invalidGraphicsAPIFail;
            fpGetParameters = &DLSS::invalidGraphicsAPIFail;
            fpRelease       = &DLSS::invalidGraphicsAPIFail;
            fpShutdown      = &DLSS::invalidGraphicsAPIFail;
            break;
        }
    }
    if (++users == 1) {
        RETURN_VOID_ON_FAILURE((this->*fpInitialize)());
        int needsUpdatedDriver{};
        RETURN_VOID_ON_FAILURE(setStatus(parameters->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsUpdatedDriver), "Failed to query the selected device's driver update needs. This may result from outdated an driver, unsupported GPUs, a failure to initialize NGX, or a failure to get the " + getName() + " compatibility parameters."));
        if (needsUpdatedDriver != 0) {
            int requiredMajorDriverVersion{};
            RETURN_VOID_ON_FAILURE(setStatus(parameters->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, &requiredMajorDriverVersion), "Failed to query the major version of the minimum " + getName() + " supported driver for the selected graphics device. This may result from outdated an driver, unsupported GPUs, a failure to initialize NGX, or a failure to get the " + getName() + " compatibility parameters."));
            int requiredMinorDriverVersion{};
            RETURN_VOID_ON_FAILURE(setStatus(parameters->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, &requiredMinorDriverVersion), "Failed to query the minor version of the minimum " + getName() + " supported driver for the selected graphics device. This may result from outdated an driver, unsupported GPUs, a failure to initialize NGX, or a failure to get the " + getName() + " compatibility parameters."));
            Upscaler::setStatus(DriversOutOfDate, "The selected device's drivers are out-of-date. They must be (" + std::to_string(requiredMajorDriverVersion) + "." + std::to_string(requiredMinorDriverVersion) + ") at a minimum.");
            return;
        }
        int DLSSSupported{};
        RETURN_VOID_ON_FAILURE(setStatus(parameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &DLSSSupported), "Failed to query status of " + getName() + " support for the selected graphics device. This may result from outdated an driver, unsupported GPUs, a failure to initialize NGX, or a failure to get the " + getName() + " compatibility parameters."));
        RETURN_VOID_ON_FAILURE(setStatusIf(DLSSSupported == 0, DeviceNotSupported, getName() + " is not supported on the selected graphics device. If you are certain that you have a DLSS compatible device, please ensure that it is being used by Unity for rendering."));
        RETURN_VOID_ON_FAILURE(setStatus(parameters->Get(NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, &DLSSSupported), "Failed to query the major version of the minimum " + getName() + " supported driver for the selected graphics device. This may result from outdated driver, unsupported GPUs, a failure to initialize NGX, or a failure to get the " + getName() + " compatibility parameters."));
        setStatusIf(DLSSSupported == 0, FeatureDenied, getName() + " has been denied for this application. Consult an NVIDIA representative for more information.");
    } else (this->*fpGetParameters)();
}

DLSS::~DLSS() {
    NVSDK_NGX_Parameter_SetI(parameters, NVSDK_NGX_Parameter_FreeMemOnReleaseFeature, 1);
    (this->*fpShutdown)();
}

Upscaler::Status DLSS::useSettings(const Settings::Resolution resolution, const Settings::DLSSPreset preset, const enum Settings::Quality mode, const bool hdr) {
    RETURN_ON_FAILURE(getStatus());
    Settings optimalSettings         = settings;
    optimalSettings.outputResolution = resolution;
    optimalSettings.hdr              = hdr;
    optimalSettings.quality          = mode;
    optimalSettings.preset           = preset;
    RETURN_ON_FAILURE(setStatus(NGX_DLSS_GET_OPTIMAL_SETTINGS(parameters, optimalSettings.outputResolution.width, optimalSettings.outputResolution.height, optimalSettings.getQuality<Upscaler::DLSS>(), &optimalSettings.recommendedInputResolution.width, &optimalSettings.recommendedInputResolution.height, &optimalSettings.dynamicMaximumInputResolution.width, &optimalSettings.dynamicMaximumInputResolution.height, &optimalSettings.dynamicMinimumInputResolution.width, &optimalSettings.dynamicMinimumInputResolution.height, &optimalSettings.sharpness), "Some invalid setting was set. Ensure that the current input resolution is within allowed bounds given the output resolution, sharpness is between 0.0 and 1.0, the Quality setting is a valid enum value, and that the output resolution is larger than 32 pixels on each axis."));
    switch (preset) {
        case Settings::Default:
            NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Quality, NVSDK_NGX_DLSS_Hint_Render_Preset_Default);
            NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Balanced, NVSDK_NGX_DLSS_Hint_Render_Preset_Default);
            NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Performance, NVSDK_NGX_DLSS_Hint_Render_Preset_Default);
            NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_UltraPerformance, NVSDK_NGX_DLSS_Hint_Render_Preset_Default);
            NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA, NVSDK_NGX_DLSS_Hint_Render_Preset_Default);
            break;
        case Settings::Stable:
            NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Quality, NVSDK_NGX_DLSS_Hint_Render_Preset_E);
            NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Balanced, NVSDK_NGX_DLSS_Hint_Render_Preset_E);
            NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Performance, NVSDK_NGX_DLSS_Hint_Render_Preset_E);
            NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_UltraPerformance, NVSDK_NGX_DLSS_Hint_Render_Preset_E);
            NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA, NVSDK_NGX_DLSS_Hint_Render_Preset_E);
            break;
        case Settings::FastPaced:
            NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Quality, NVSDK_NGX_DLSS_Hint_Render_Preset_C);
            NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Balanced, NVSDK_NGX_DLSS_Hint_Render_Preset_C);
            NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Performance, NVSDK_NGX_DLSS_Hint_Render_Preset_C);
            NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_UltraPerformance, NVSDK_NGX_DLSS_Hint_Render_Preset_C);
            NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA, NVSDK_NGX_DLSS_Hint_Render_Preset_C);
            break;
        case Settings::AnitGhosting:
            NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Quality, NVSDK_NGX_DLSS_Hint_Render_Preset_A);
            NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Balanced, NVSDK_NGX_DLSS_Hint_Render_Preset_A);
            NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Performance, NVSDK_NGX_DLSS_Hint_Render_Preset_A);
            NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_UltraPerformance, NVSDK_NGX_DLSS_Hint_Render_Preset_B);
            NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA, NVSDK_NGX_DLSS_Hint_Render_Preset_C);
            break;
        default: break;
    }
    NVSDK_NGX_DLSS_Create_Params createParams = {
      .Feature = {
                  .InWidth            = optimalSettings.recommendedInputResolution.width,
                  .InHeight           = optimalSettings.recommendedInputResolution.height,
                  .InTargetWidth      = optimalSettings.outputResolution.width,
                  .InTargetHeight     = optimalSettings.outputResolution.height,
                  .InPerfQualityValue = optimalSettings.getQuality<Upscaler::DLSS>(),
                  },
      .InFeatureCreateFlags = static_cast<NVSDK_NGX_DLSS_Feature_Flags>(
        static_cast<unsigned>(NVSDK_NGX_DLSS_Feature_Flags_DepthInverted) |
        static_cast<unsigned>(NVSDK_NGX_DLSS_Feature_Flags_AutoExposure) |
        static_cast<unsigned>(NVSDK_NGX_DLSS_Feature_Flags_MVLowRes) |
        (optimalSettings.hdr ? NVSDK_NGX_DLSS_Feature_Flags_IsHDR : 0U)
      ),
      .InEnableOutputSubrects = false,
    };
    if (handle != nullptr) RETURN_ON_FAILURE((this->*fpRelease)());
    RETURN_ON_FAILURE((this->*fpCreate)(&createParams));
    settings = optimalSettings;
    return Success;
}

Upscaler::Status DLSS::evaluate() {
    RETURN_ON_FAILURE((this->*fpEvaluate)());
    settings.resetHistory = false;
    return Success;
}
#endif