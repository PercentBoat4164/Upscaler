#include "DLSS.hpp"
#ifdef ENABLE_DLSS
// Project
#    ifdef ENABLE_DX11
#        include "GraphicsAPI/DX11.hpp"
#    endif
#    ifdef ENABLE_DX12
#        include "GraphicsAPI/DX12.hpp"
#    endif
#    ifdef ENABLE_VULKAN
#        include "GraphicsAPI/Vulkan.hpp"
#    endif

// System
#    include <algorithm>
#    include <cstring>
#    include <limits>

#    ifdef ENABLE_VULKAN
void DLSS::RAII_NGXVulkanResource::ChangeResource(const NVSDK_NGX_ImageViewInfo_VK& info) {
    Destroy();
    resource = NVSDK_NGX_Resource_VK{
      .Resource  = {info},
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
Upscaler::Status (DLSS::*DLSS::fpGetParameters)(){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::fpCreate)(){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::fpSetDepth)(
  void*,
  UnityRenderingExtTextureFormat
){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::fpSetInputColor)(
  void*,
  UnityRenderingExtTextureFormat
){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::fpSetMotionVectors)(
  void*,
  UnityRenderingExtTextureFormat
){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::fpSetOutputColor)(
  void*,
  UnityRenderingExtTextureFormat
){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::fpEvaluate)(){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::fpRelease)(){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::fpShutdown)(){&DLSS::safeFail};

Upscaler::SupportState DLSS::supported = UNTESTED;
#    ifdef ENABLE_VULKAN
Upscaler::SupportState DLSS::instanceExtensionsSupported = UNTESTED;
Upscaler::SupportState DLSS::deviceExtensionsSupported   = UNTESTED;
#    endif

#    ifdef ENABLE_VULKAN
Upscaler::Status DLSS::VulkanInitialize() {
    const UnityVulkanInstance vulkanInstance = Vulkan::getUnityInterface()->Instance();

    RETURN_ON_FAILURE(setStatus(NVSDK_NGX_VULKAN_Init(applicationInfo.ngxIdentifier.v.ApplicationId, applicationInfo.featureCommonInfo.PathListInfo.Path[0], vulkanInstance.instance, vulkanInstance.physicalDevice, vulkanInstance.device), "Failed to initialize the NGX instance."));
    return SUCCESS;
}

Upscaler::Status DLSS::VulkanGetParameters() {
    RETURN_ON_FAILURE(setStatusIf(parameters != nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Parameters already exist!"));
    RETURN_ON_FAILURE(setStatus(NVSDK_NGX_VULKAN_GetCapabilityParameters(&parameters), "Failed to get the " + getName() + " compatibility parameters."));
    RETURN_ON_FAILURE(setStatusIf(parameters == nullptr, SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, "Parameters are invalid after attempting to build them."));
    return SUCCESS;
}

Upscaler::Status DLSS::VulkanCreate() {
    RETURN_ON_FAILURE(setStatusIf(parameters == nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Parameters do not exist!"));
    RETURN_ON_FAILURE(setStatusIf(featureHandle != nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Feature already exists!"));

    UnityVulkanRecordingState state{};
    Vulkan::getUnityInterface()->EnsureInsideRenderPass();
    Vulkan::getUnityInterface()->CommandRecordingState(&state, kUnityVulkanGraphicsQueueAccess_DontCare);

    RETURN_ON_FAILURE(setStatus(NGX_VULKAN_CREATE_DLSS_EXT(state.commandBuffer, 1, 1, &featureHandle, parameters, &DLSSCreateParams), "Failed to create the " + getName() + " feature."));
    RETURN_ON_FAILURE(setStatusIf(featureHandle == nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Failed to create the " + getName() + " feature. The handle returned from `NGX_VULKAN_CREATE_DLSS_EXT()` was `nullptr`."));
    return SUCCESS;
}

Upscaler::Status DLSS::VulkanSetDepth(void* nativeHandle, UnityRenderingExtTextureFormat unityFormat) {
    RETURN_ON_FAILURE(setStatusIf(nativeHandle == VK_NULL_HANDLE, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Refused to set image resources due to the given depth image being `VK_NULL_HANDLE`."));

    VkImage        image  = *static_cast<VkImage*>(nativeHandle);
    const VkFormat format = Vulkan::getFormat(unityFormat);
    VkImageView    view   = Vulkan::createImageView(image, format, VK_IMAGE_ASPECT_DEPTH_BIT);

    RETURN_ON_FAILURE(setStatusIf(view == VK_NULL_HANDLE, SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING, "Refused to set image resources due to an attempt to create the depth image view resulting in a `VK_NULL_HANDLE` view handle."));

    // clang-format off
    depth.vulkan->ChangeResource({
      .ImageView        = view,
      .Image            = image,
      .SubresourceRange = {
        .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
      },
      .Format = format,
      .Width  = settings.inputResolution.width,
      .Height = settings.inputResolution.height,
    });
    // clang-format on

    return SUCCESS;
}

Upscaler::Status DLSS::VulkanSetInputColor(void* nativeHandle, const UnityRenderingExtTextureFormat unityFormat) {
    RETURN_ON_FAILURE(setStatusIf(nativeHandle == VK_NULL_HANDLE, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Refused to set image resources due to the given input color image being `VK_NULL_HANDLE`."));

    VkImage        image  = *static_cast<VkImage*>(nativeHandle);
    const VkFormat format = Vulkan::getFormat(unityFormat);
    VkImageView    view   = Vulkan::createImageView(image, format, VK_IMAGE_ASPECT_COLOR_BIT);

    RETURN_ON_FAILURE(setStatusIf(view == VK_NULL_HANDLE, SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING, "Refused to set image resources due to an attempt to create the input color image view resulting in a `VK_NULL_HANDLE` view handle."));

    // clang-format off
    inColor.vulkan->ChangeResource({
      .ImageView        = view,
      .Image            = image,
      .SubresourceRange = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
      },
      .Format = format,
      .Width  = settings.inputResolution.width,
      .Height = settings.inputResolution.height,
    });
    // clang-format on

    return SUCCESS;
}

Upscaler::Status
DLSS::VulkanSetMotionVectors(void* nativeHandle, const UnityRenderingExtTextureFormat unityFormat) {
    RETURN_ON_FAILURE(setStatusIf(nativeHandle == VK_NULL_HANDLE, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Refused to set image resources due to the given motion vector image being `VK_NULL_HANDLE`."));

    VkImage        image  = *static_cast<VkImage*>(nativeHandle);
    const VkFormat format = Vulkan::getFormat(unityFormat);
    VkImageView    view   = Vulkan::createImageView(image, format, VK_IMAGE_ASPECT_COLOR_BIT);

    RETURN_ON_FAILURE(setStatusIf(view == VK_NULL_HANDLE, SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING, "Refused to set image resources due to an attempt to create the motion vector image view resulting in a `VK_NULL_HANDLE` view handle."));

    // clang-format off
    motion.vulkan->ChangeResource({
      .ImageView        = view,
      .Image            = image,
      .SubresourceRange = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
      },
      .Format = format,
      .Width  = settings.inputResolution.width,
      .Height = settings.inputResolution.height,
    });
    // clang-format on

    return SUCCESS;
}

Upscaler::Status DLSS::VulkanSetOutputColor(void* nativeHandle, const UnityRenderingExtTextureFormat unityFormat) {
    RETURN_ON_FAILURE(setStatusIf(nativeHandle == VK_NULL_HANDLE, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Refused to set image resources due to the given output color image being `VK_NULL_HANDLE`."));

    VkImage        image  = *static_cast<VkImage*>(nativeHandle);
    const VkFormat format = Vulkan::getFormat(unityFormat);
    VkImageView    view   = Vulkan::createImageView(image, format, VK_IMAGE_ASPECT_COLOR_BIT);

    RETURN_ON_FAILURE(setStatusIf(view == VK_NULL_HANDLE, SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING, "Refused to set image resources due to an attempt to create the output color image view resulting in a `VK_NULL_HANDLE` view handle."));

    // clang-format off
    outColor.vulkan->ChangeResource({
      .ImageView        = view,
      .Image            = image,
      .SubresourceRange = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
      },
      .Format = format,
      .Width  = settings.outputResolution.width,
      .Height = settings.outputResolution.height,
    });
    // clang-format on

    return SUCCESS;
}

Upscaler::Status DLSS::VulkanEvaluate() {
    RETURN_ON_FAILURE(setStatusIf(parameters == nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Parameters do not exist!"));
    RETURN_ON_FAILURE(setStatusIf(featureHandle == nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Feature does not exist!"));

    UnityVulkanRecordingState state{};
    Vulkan::getUnityInterface()->EnsureInsideRenderPass();
    Vulkan::getUnityInterface()->CommandRecordingState(
      &state,
      kUnityVulkanGraphicsQueueAccess_DontCare
    );

    // clang-format off
    NVSDK_NGX_VK_DLSS_Eval_Params DLSSEvalParameters = {
      .Feature = {
        .pInColor = &inColor.vulkan->GetResource(),
        .pInOutput = &outColor.vulkan->GetResource(),
        .InSharpness = settings.sharpness,
      },
      .pInDepth                  = &depth.vulkan->GetResource(),
      .pInMotionVectors          = &motion.vulkan->GetResource(),
      .InJitterOffsetX           = settings.jitter[0],
      .InJitterOffsetY           = settings.jitter[1],
      .InRenderSubrectDimensions = {
        .Width  = settings.inputResolution.width,
        .Height = settings.inputResolution.height,
      },
      .InReset    = static_cast<int>(settings.resetHistory),
      .InMVScaleX = -static_cast<float>(settings.inputResolution.width),
      .InMVScaleY = -static_cast<float>(settings.inputResolution.height),
#       ifndef NDEBUG
      .InIndicatorInvertYAxis = 1,
#       endif
    };
    // clang-format on

    settings.resetHistory = false;

    RETURN_ON_FAILURE(setStatus(NGX_VULKAN_EVALUATE_DLSS_EXT(state.commandBuffer, featureHandle, parameters, &DLSSEvalParameters), "Failed to evaluate the " + getName() + " feature."));
    return SUCCESS;
}

Upscaler::Status DLSS::VulkanRelease() {
    RETURN_ON_FAILURE(setStatusIf(featureHandle == nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Feature has already been released."));
    RETURN_ON_FAILURE(setStatus(NVSDK_NGX_VULKAN_ReleaseFeature(featureHandle), "Failed to release the " + getName() + " feature."));
    featureHandle = nullptr;
    return SUCCESS;
}

Upscaler::Status DLSS::VulkanDestroyParameters() {
    RETURN_ON_FAILURE(setStatusIf(parameters == nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Parameters do not exist!"));
    RETURN_ON_FAILURE(setStatus(NVSDK_NGX_VULKAN_DestroyParameters(parameters), "Failed to release the " + getName() + " compatibility parameters."));
    parameters = nullptr;
    return SUCCESS;
}

Upscaler::Status DLSS::VulkanShutdown() {
    if (featureHandle != nullptr)
        RETURN_ON_FAILURE(VulkanRelease());
    if (parameters != nullptr)
        RETURN_ON_FAILURE(VulkanDestroyParameters());

    inColor.vulkan->Destroy();
    outColor.vulkan->Destroy();
    depth.vulkan->Destroy();
    motion.vulkan->Destroy();

    RETURN_ON_FAILURE(setStatus(NVSDK_NGX_VULKAN_Shutdown1(Vulkan::getUnityInterface()->Instance().device), "Failed to shutdown the NGX instance."));

    return SUCCESS;
}
#    endif

#    ifdef ENABLE_DX12
Upscaler::Status DLSS::DX12Initialize() {
    return setStatus(
      NVSDK_NGX_D3D12_Init(
        applicationInfo.ngxIdentifier.v.ApplicationId,
        applicationInfo.featureCommonInfo.PathListInfo.Path[0],
        GraphicsAPI::get<DX12>()->getUnityInterface()->GetDevice()
      ),
      "Failed to initialize the NGX instance."
    );
}

Upscaler::Status DLSS::DX12GetParameters() {
    if (parameters != nullptr) return getStatus();
    return setStatus(
      NVSDK_NGX_D3D12_GetCapabilityParameters(&parameters),
      "Failed to get the " + getName() + " compatibility parameters"
    );
}

Upscaler::Status DLSS::DX12CreateFeature() {
    if (parameters == nullptr) DX12GetParameters();
    if (featureHandle != nullptr) DX12ReleaseFeature();

    UnityGraphicsD3D12RecordingState state{};
    GraphicsAPI::get<DX12>()->getUnityInterface()->CommandRecordingState(&state);

    if (failure(setStatus(
          NGX_D3D12_CREATE_DLSS_EXT(state.commandList, 1U, 1U, &featureHandle, parameters, &DLSSCreateParams),
          "Failed to create the " + getName() + " feature."
        )))
        return getStatus();
    return setStatusIf(
      featureHandle == nullptr,
      SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
      "Failed to create the " + getName() +
        " feature. The handle returned from `NGX_DX12_CREATE_DLSS_EXT()` was "
        "`nullptr`."
    );
}

Upscaler::Status DLSS::DX12SetDepthBuffer(void* nativeHandle, UnityRenderingExtTextureFormat /*unused*/) {
    if (failure(setStatusIf(
          nativeHandle == nullptr,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given depth image being `VK_NULL_HANDLE`."
        )))
        return getStatus();
    depth.dx12 = static_cast<ID3D12Resource*>(nativeHandle);
    return getStatus();
}

Upscaler::Status DLSS::DX12SetInputColor(void* nativeHandle, UnityRenderingExtTextureFormat /*unused*/) {
    if (failure(setStatusIf(
          nativeHandle == nullptr,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given input color image being `VK_NULL_HANDLE`."
        )))
        return getStatus();
    inColor.dx12 = static_cast<ID3D12Resource*>(nativeHandle);
    return getStatus();
}

Upscaler::Status DLSS::DX12SetMotionVectors(void* nativeHandle, UnityRenderingExtTextureFormat /*unused*/) {
    if (failure(setStatusIf(
          nativeHandle == nullptr,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given motion vector image being `VK_NULL_HANDLE`."
        )))
        return getStatus();
    motion.dx12 = static_cast<ID3D12Resource*>(nativeHandle);
    return getStatus();
}

Upscaler::Status DLSS::DX12SetOutputColor(void* nativeHandle, UnityRenderingExtTextureFormat /*unused*/) {
    if (failure(setStatusIf(
          nativeHandle == nullptr,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given output color image being `VK_NULL_HANDLE`."
        )))
        return getStatus();
    outColor.dx12 = static_cast<ID3D12Resource*>(nativeHandle);
    return getStatus();
}

Upscaler::Status DLSS::DX12Evaluate() {
    UnityGraphicsD3D12RecordingState state{};
    GraphicsAPI::get<DX12>()->getUnityInterface()->CommandRecordingState(&state);

    // clang-format off
    NVSDK_NGX_D3D12_DLSS_Eval_Params DLSSEvalParameters = {
      .Feature = {
        .pInColor = inColor.dx12,
        .pInOutput = outColor.dx12,
        .InSharpness = settings.sharpness,
      },
      .pInDepth                  = depth.dx12,
      .pInMotionVectors          = motion.dx12,
      .InJitterOffsetX           = settings.jitter[0],
      .InJitterOffsetY           = settings.jitter[1],
      .InRenderSubrectDimensions = {
        .Width  = settings.inputResolution.width,
        .Height = settings.inputResolution.height,
      },
      .InReset    = static_cast<int>(settings.resetHistory),
      .InMVScaleX = -static_cast<float>(settings.inputResolution.width),
      .InMVScaleY = -static_cast<float>(settings.inputResolution.height),
#ifndef NDEBUG
      .InIndicatorInvertYAxis = 1,
#endif
    };
    // clang-format on

    return setStatus(
      NGX_D3D12_EVALUATE_DLSS_EXT(state.commandList, featureHandle, parameters, &DLSSEvalParameters),
      "Failed to evaluate the " + getName() + " feature."
    );
}

Upscaler::Status DLSS::DX12ReleaseFeature() {
    if (featureHandle == nullptr) return getStatus();
    setStatus(NVSDK_NGX_D3D12_ReleaseFeature(featureHandle), "Failed to release the " + getName() + " feature.");
    featureHandle = nullptr;
    return getStatus();
}

Upscaler::Status DLSS::DX12DestroyParameters() {
    if (parameters == nullptr) return getStatus();
    setStatus(
      NVSDK_NGX_D3D12_DestroyParameters(parameters),
      "Failed to release the " + getName() + " compatibility parameters."
    );
    parameters = nullptr;
    return getStatus();
}

Upscaler::Status DLSS::DX12Shutdown() {
    if (!initialized) return getStatus();
    DX12DestroyParameters();
    DX12ReleaseFeature();
    return setStatus(NVSDK_NGX_D3D12_Shutdown1(nullptr), "Failed to shutdown the NGX instance.");
}
#    endif

#    ifdef ENABLE_DX11
Upscaler::Status DLSS::DX11Initialize() {
    return setStatus(
      NVSDK_NGX_D3D11_Init(
        applicationInfo.ngxIdentifier.v.ApplicationId,
        applicationInfo.featureCommonInfo.PathListInfo.Path[0],
        GraphicsAPI::get<DX11>()->getUnityInterface()->GetDevice()
      ),
      "Failed to initialize the NGX instance."
    );
}

Upscaler::Status DLSS::DX11GetParameters() {
    if (parameters != nullptr) return getStatus();
    return setStatus(
      NVSDK_NGX_D3D11_GetCapabilityParameters(&parameters),
      "Failed to get the " + getName() + " compatibility parameters."
    );
}

Upscaler::Status DLSS::DX11CreateFeature() {
    if (featureHandle != nullptr) return getStatus();

    if (ID3D11DeviceContext* context = GraphicsAPI::get<DX11>()->beginOneTimeSubmitRecording(); failure(setStatus(
          NGX_D3D11_CREATE_DLSS_EXT(context, &featureHandle, parameters, &DLSSCreateParams),
          "Failed to create the " + getName() + " feature."
        ))) {
        GraphicsAPI::get<DX11>()->cancelOneTimeSubmitRecording();
        return getStatus();
    }
    GraphicsAPI::get<DX11>()->endOneTimeSubmitRecording();
    return setStatusIf(
      featureHandle == nullptr,
      SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
      "Failed to create the " + getName() +
        " feature. The handle returned from `NGX_DX11_CREATE_DLSS_EXT()` was "
        "`nullptr`."
    );
}

Upscaler::Status DLSS::DX11SetDepthBuffer(void* nativeHandle, UnityRenderingExtTextureFormat /*unused*/) {
    if (failure(setStatusIf(
          nativeHandle == nullptr,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given depth image being `VK_NULL_HANDLE`."
        )))
        return getStatus();
    depth.dx11 = static_cast<ID3D11Resource*>(nativeHandle);
    return getStatus();
}

Upscaler::Status DLSS::DX11SetInputColor(void* nativeHandle, UnityRenderingExtTextureFormat /*unused*/) {
    if (failure(setStatusIf(
          nativeHandle == nullptr,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given input color image being `VK_NULL_HANDLE`."
        )))
        return getStatus();
    inColor.dx11 = static_cast<ID3D11Resource*>(nativeHandle);
    return getStatus();
}

Upscaler::Status DLSS::DX11SetMotionVectors(void* nativeHandle, UnityRenderingExtTextureFormat /*unused*/) {
    if (failure(setStatusIf(
          nativeHandle == nullptr,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given motion vector image being `VK_NULL_HANDLE`."
        )))
        return getStatus();
    motion.dx11 = static_cast<ID3D11Resource*>(nativeHandle);
    return getStatus();
}

Upscaler::Status DLSS::DX11SetOutputColor(void* nativeHandle, UnityRenderingExtTextureFormat /*unused*/) {
    if (failure(setStatusIf(
          nativeHandle == nullptr,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given output color image being `VK_NULL_HANDLE`."
        )))
        return getStatus();
    outColor.dx11 = static_cast<ID3D11Resource*>(nativeHandle);
    return getStatus();
}

Upscaler::Status DLSS::DX11Evaluate() {
    // clang-format off
    NVSDK_NGX_D3D11_DLSS_Eval_Params DLSSEvalParams = {
      .Feature = {
        .pInColor = inColor.dx11,
        .pInOutput = outColor.dx11,
        .InSharpness = settings.sharpness,
      },
      .pInDepth                  = depth.dx11,
      .pInMotionVectors          = motion.dx11,
      .InJitterOffsetX           = settings.jitter[0],
      .InJitterOffsetY           = settings.jitter[1],
      .InRenderSubrectDimensions = {
        .Width  = settings.inputResolution.width,
        .Height = settings.inputResolution.height,
      },
      .InReset    = static_cast<int>(settings.resetHistory),
      .InMVScaleX = -static_cast<float>(settings.inputResolution.width),
      .InMVScaleY = -static_cast<float>(settings.inputResolution.height),
#ifndef NDEBUG
      .InIndicatorInvertYAxis = 1,
#endif
    };
    // clang-format on

    if (ID3D11DeviceContext* context = GraphicsAPI::get<DX11>()->beginOneTimeSubmitRecording(); failure(setStatus(
          NGX_D3D11_EVALUATE_DLSS_EXT(context, featureHandle, parameters, &DLSSEvalParams),
          "Failed to evaluate the " + getName() + " feature."
        ))) {
        GraphicsAPI::get<DX11>()->cancelOneTimeSubmitRecording();
        return getStatus();
    }
    GraphicsAPI::get<DX11>()->endOneTimeSubmitRecording();
    return getStatus();
}

Upscaler::Status DLSS::DX11ReleaseFeature() {
    if (featureHandle == nullptr) return getStatus();
    setStatus(NVSDK_NGX_D3D11_ReleaseFeature(featureHandle), "Failed to release the " + getName() + " feature.");
    featureHandle = nullptr;
    return getStatus();
}

Upscaler::Status DLSS::DX11DestroyParameters() {
    if (parameters == nullptr) return getStatus();
    setStatus(
      NVSDK_NGX_D3D11_DestroyParameters(parameters),
      "Failed to release the " + getName() + " compatibility parameters."
    );
    parameters = nullptr;
    return getStatus();
}

Upscaler::Status DLSS::DX11Shutdown() {
    if (!initialized) return getStatus();
    DX11DestroyParameters();
    DX11ReleaseFeature();
    return setStatus(NVSDK_NGX_D3D11_Shutdown1(nullptr), "Failed to shutdown the NGX instance.");
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
    if (Upscaler::log != nullptr) Upscaler::log((msg + message).c_str());
}

DLSS::DLSS(const GraphicsAPI::Type graphicsAPI) {
    switch (graphicsAPI) {
        case GraphicsAPI::NONE: {
            fpInitialize       = &DLSS::safeFail;
            fpGetParameters    = &DLSS::safeFail;
            fpCreate           = &DLSS::safeFail;
            fpSetDepth         = &DLSS::safeFail;
            fpSetInputColor    = &DLSS::safeFail;
            fpSetMotionVectors = &DLSS::safeFail;
            fpSetOutputColor   = &DLSS::safeFail;
            fpEvaluate         = &DLSS::safeFail;
            fpRelease          = &DLSS::safeFail;
            fpShutdown         = &DLSS::safeFail;
            break;
        }
#    ifdef ENABLE_VULKAN
        case GraphicsAPI::VULKAN: {
            inColor.vulkan  = new RAII_NGXVulkanResource;
            outColor.vulkan = new RAII_NGXVulkanResource;
            depth.vulkan    = new RAII_NGXVulkanResource;
            motion.vulkan   = new RAII_NGXVulkanResource;

            fpInitialize       = &DLSS::VulkanInitialize;
            fpGetParameters    = &DLSS::VulkanGetParameters;
            fpCreate           = &DLSS::VulkanCreate;
            fpSetDepth         = &DLSS::VulkanSetDepth;
            fpSetInputColor    = &DLSS::VulkanSetInputColor;
            fpSetMotionVectors = &DLSS::VulkanSetMotionVectors;
            fpSetOutputColor   = &DLSS::VulkanSetOutputColor;
            fpEvaluate         = &DLSS::VulkanEvaluate;
            fpRelease          = &DLSS::VulkanRelease;
            fpShutdown         = &DLSS::VulkanShutdown;
            break;
        }
#    endif
#    ifdef ENABLE_DX12
        case GraphicsAPI::DX12: {
            fpInitialize       = &DLSS::DX12Initialize;
            fpGetParameters    = &DLSS::DX12GetParameters;
            fpCreate           = &DLSS::DX12CreateFeature;
            fpSetDepth         = &DLSS::DX12SetDepthBuffer;
            fpSetInputColor    = &DLSS::DX12SetInputColor;
            fpSetMotionVectors = &DLSS::DX12SetMotionVectors;
            fpSetOutputColor   = &DLSS::DX12SetOutputColor;
            fpEvaluate         = &DLSS::DX12Evaluate;
            fpRelease          = &DLSS::DX12ReleaseFeature;
            fpShutdown         = &DLSS::DX12Shutdown;
            break;
        }
#    endif
#    ifdef ENABLE_DX11
        case GraphicsAPI::DX11: {
            fpInitialize       = &DLSS::DX11Initialize;
            fpGetParameters    = &DLSS::DX11GetParameters;
            fpCreate           = &DLSS::DX11CreateFeature;
            fpSetDepth         = &DLSS::DX11SetDepthBuffer;
            fpSetInputColor    = &DLSS::DX11SetInputColor;
            fpSetMotionVectors = &DLSS::DX11SetMotionVectors;
            fpSetOutputColor   = &DLSS::DX11SetOutputColor;
            fpEvaluate         = &DLSS::DX11Evaluate;
            fpRelease          = &DLSS::DX11ReleaseFeature;
            fpShutdown         = &DLSS::DX11Shutdown;
            break;
        }
#    endif
        default: {
            fpInitialize       = &DLSS::safeFail;
            fpGetParameters    = &DLSS::safeFail;
            fpCreate           = &DLSS::safeFail;
            fpSetDepth         = &DLSS::safeFail;
            fpSetInputColor    = &DLSS::safeFail;
            fpSetMotionVectors = &DLSS::safeFail;
            fpSetOutputColor   = &DLSS::safeFail;
            fpEvaluate         = &DLSS::safeFail;
            fpRelease          = &DLSS::safeFail;
            fpShutdown         = &DLSS::safeFail;
            break;
        }
    }
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
        if (std::find_if(supportedExtensions.begin(), supportedExtensions.end(), [&extensionName](const std::string& str) { return strcmp(str.c_str(), extensionName) == 0; }) != supportedExtensions.end())
            requestedExtensions.emplace_back(extensionName);
        else {
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
        if (std::find_if(supportedExtensions.begin(), supportedExtensions.end(), [&extensionName](const std::string& str) { return strcmp(str.c_str(), extensionName) == 0; }) != supportedExtensions.end())
            requestedExtensions.emplace_back(extensionName);
        else {
            deviceExtensionsSupported = UNSUPPORTED;
            return {};
        }
    }
    deviceExtensionsSupported = SUPPORTED;
    return requestedExtensions;
}
#    endif

Upscaler::Type DLSS::getType() {
    return Upscaler::DLSS;
}

std::string DLSS::getName() {
    return "NVIDIA DLSS";
}

bool DLSS::isSupported() {
    if (supported != UNTESTED)
        return supported == SUPPORTED;
    if (instanceExtensionsSupported != SUPPORTED || deviceExtensionsSupported != SUPPORTED)
        return (supported = UNSUPPORTED) == SUPPORTED;
    if (!initialized) {
        initialize();
        shutdown();
    }
    return (supported = success(getStatus()) ? SUPPORTED : UNSUPPORTED) == SUPPORTED;
}

Upscaler::Settings
DLSS::getOptimalSettings(const Settings::Resolution resolution, const Settings::QualityMode mode, const bool hdr) {
    if (parameters == nullptr) return settings;

    if (resolution.height < 32 || resolution.width < 32)
        Upscaler::setStatus(
          SETTINGS_ERROR_INVALID_OUTPUT_RESOLUTION,
          getName() + " does not support output resolutions less than 32x32."
        );

    Settings optimalSettings         = settings;
    optimalSettings.outputResolution = resolution;
    optimalSettings.HDR              = hdr;
    optimalSettings.quality          = mode;

    setStatus(
      NGX_DLSS_GET_OPTIMAL_SETTINGS(
        parameters,
        optimalSettings.outputResolution.width,
        optimalSettings.outputResolution.height,
        optimalSettings.getQuality<Upscaler::DLSS>(),
        &optimalSettings.inputResolution.width,
        &optimalSettings.inputResolution.height,
        &optimalSettings.dynamicMaximumInputResolution.width,
        &optimalSettings.dynamicMaximumInputResolution.height,
        &optimalSettings.dynamicMinimumInputResolution.width,
        &optimalSettings.dynamicMinimumInputResolution.height,
        &optimalSettings.sharpness
      ),
      "Some invalid setting was set. Ensure that the current input resolution is within allowed bounds given the"
      "output resolution, sharpness is between 0F and 1F, and that the QualityMode setting is a valid enum value."
    );

    return optimalSettings;
}

Upscaler::Status DLSS::initialize() {
    RETURN_ON_FAILURE(setStatusIf(initialized, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, getName() + " is already initialized!"));
    if (!resetStatus()) return getStatus();

    // Upscaler_Initialize NGX SDK
    RETURN_ON_FAILURE(Upscaler::setStatus((this->*fpInitialize)(), "Failed to initialize NGX."));
    RETURN_ON_FAILURE(Upscaler::setStatus((this->*fpGetParameters)(), "Failed to get NGX Parameters. This may be caused by a host of errors. If the Status is Unknown, then likely an unsupported graphics API is in use."));
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
    return SUCCESS;
}

Upscaler::Status DLSS::create() {
    // clang-format off
    DLSSCreateParams = {
      .Feature = {
        .InWidth            = settings.inputResolution.width,
        .InHeight           = settings.inputResolution.height,
        .InTargetWidth      = settings.outputResolution.width,
        .InTargetHeight     = settings.outputResolution.height,
        .InPerfQualityValue = settings.getQuality<Upscaler::DLSS>(),
      },
      .InFeatureCreateFlags = static_cast<int>(
        static_cast<unsigned>(NVSDK_NGX_DLSS_Feature_Flags_MVLowRes) |
        static_cast<unsigned>(NVSDK_NGX_DLSS_Feature_Flags_MVJittered) |
        static_cast<unsigned>(NVSDK_NGX_DLSS_Feature_Flags_DepthInverted) |
        static_cast<unsigned>(NVSDK_NGX_DLSS_Feature_Flags_AutoExposure) |
        (settings.sharpness > 0.0F ? NVSDK_NGX_DLSS_Feature_Flags_DoSharpening : 0U) |
        (settings.HDR ? NVSDK_NGX_DLSS_Feature_Flags_IsHDR : 0U)
      ),
      .InEnableOutputSubrects = false,
    };
    // clang-format on

    if (featureHandle != nullptr)
        RETURN_ON_FAILURE((this->*fpRelease)());
    RETURN_ON_FAILURE((this->*fpCreate)());
    return SUCCESS;
}

Upscaler::Status DLSS::setDepth(void* nativeHandle, UnityRenderingExtTextureFormat unityFormat) {
    return (this->*fpSetDepth)(nativeHandle, unityFormat);
}

Upscaler::Status DLSS::setInputColor(void* nativeHandle, const UnityRenderingExtTextureFormat unityFormat) {
    return (this->*fpSetInputColor)(nativeHandle, unityFormat);
}

Upscaler::Status DLSS::setMotionVectors(void* nativeHandle, const UnityRenderingExtTextureFormat unityFormat) {
    return (this->*fpSetMotionVectors)(nativeHandle, unityFormat);
}

Upscaler::Status DLSS::setOutputColor(void* nativeHandle, const UnityRenderingExtTextureFormat unityFormat) {
    return (this->*fpSetOutputColor)(nativeHandle, unityFormat);
}

Upscaler::Status DLSS::evaluate() {
    return (this->*fpEvaluate)();
}

Upscaler::Status DLSS::release() {
    return (this->*fpRelease)();
}

Upscaler::Status DLSS::shutdown() {
    if (initialized)
        RETURN_ON_FAILURE((this->*fpShutdown)());
    initialized = false;
    return SUCCESS;
}
#endif