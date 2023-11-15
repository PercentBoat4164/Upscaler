#include "DLSS.hpp"

// Project
#include "GraphicsAPI/DX11.hpp"
#include "GraphicsAPI/DX12.hpp"
#include "GraphicsAPI/Vulkan.hpp"

// System
#include <algorithm>

void DLSS::RAII_NGXVulkanResource::ChangeResource(const NVSDK_NGX_ImageViewInfo_VK &info) {
    Destroy();
    resource =
      NVSDK_NGX_Resource_VK{.Resource = info, .Type = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW, .ReadWrite = true};
}

NVSDK_NGX_Resource_VK DLSS::RAII_NGXVulkanResource::GetResource() const {
    return resource;
}

DLSS::RAII_NGXVulkanResource::~RAII_NGXVulkanResource() {
    Destroy();
}

void DLSS::RAII_NGXVulkanResource::Destroy() const {
    GraphicsAPI::get<Vulkan>()->destroyImageView(resource.Resource.ImageViewInfo.ImageView);
}

Upscaler::Status (DLSS::*DLSS::graphicsAPIIndependentInitializeFunctionPointer)(){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::graphicsAPIIndependentGetParametersFunctionPointer)(){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::graphicsAPIIndependentCreateFeatureFunctionPointer)(NVSDK_NGX_DLSS_Create_Params
){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::graphicsAPIIndependentSetDepthBufferFunctionPointer)(
  void *,
  UnityRenderingExtTextureFormat
){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::graphicsAPIIndependentSetInputColorFunctionPointer)(
  void *,
  UnityRenderingExtTextureFormat
){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::graphicsAPIIndependentSetMotionVectorsFunctionPointer)(
  void *,
  UnityRenderingExtTextureFormat
){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::graphicsAPIIndependentSetOutputColorFunctionPointer)(
  void *,
  UnityRenderingExtTextureFormat
){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::graphicsAPIIndependentEvaluateFunctionPointer)(){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::graphicsAPIIndependentReleaseFeatureFunctionPointer)(){&DLSS::safeFail};
Upscaler::Status (DLSS::*DLSS::graphicsAPIIndependentShutdownFunctionPointer)(){&DLSS::safeFail};

#ifdef ENABLE_VULKAN
Upscaler::Status DLSS::VulkanInitialize() {
    const UnityVulkanInstance vulkanInstance = GraphicsAPI::get<Vulkan>()->getUnityInterface()->Instance();

    return setStatus(
      NVSDK_NGX_VULKAN_Init(
        applicationInfo.id,
        applicationInfo.dataPath.c_str(),
        vulkanInstance.instance,
        vulkanInstance.physicalDevice,
        vulkanInstance.device
      ),
      "Failed to initialize the NGX instance."
    );
}

Upscaler::Status DLSS::VulkanGetParameters() {
    if (parameters != nullptr) return getStatus();
    Status error = setStatus(
      NVSDK_NGX_VULKAN_GetCapabilityParameters(&parameters),
      "Failed to get the " + getName() + " compatibility parameters."
    );
    if (failure(error)) return error;
    if (parameters == nullptr)
        error = GENERIC_ERROR;  // In the tested cases, this is actually an invalid graphics API.
    return error;
}

Upscaler::Status DLSS::VulkanCreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams) {
    if (parameters == nullptr) VulkanGetParameters();
    if (featureHandle != nullptr) VulkanReleaseFeature();
    GraphicsAPI::get<Vulkan>()->getUnityInterface()->EnsureOutsideRenderPass();

    if (VkCommandBuffer commandBuffer = GraphicsAPI::get<Vulkan>()->beginOneTimeSubmitRecording();
        failure(setStatus(
          NGX_VULKAN_CREATE_DLSS_EXT(commandBuffer, 1, 1, &featureHandle, parameters, &DLSSCreateParams),
          "Failed to create the " + getName() + " feature."
        ))) {
        GraphicsAPI::get<Vulkan>()->cancelOneTimeSubmitRecording();
        return getStatus();
    }
    GraphicsAPI::get<Vulkan>()->endOneTimeSubmitRecording();
    return setStatusIf(
      featureHandle == nullptr,
      SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
      "Failed to create the " + getName() +
        " feature. The handle returned from `NGX_VULKAN_CREATE_DLSS_EXT()` was "
        "`nullptr`."
    );
}

Upscaler::Status DLSS::VulkanSetDepthBuffer(void *nativeHandle, const UnityRenderingExtTextureFormat unityFormat) {
    if (failure(setStatusIf(
          nativeHandle == VK_NULL_HANDLE,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given depth image being `VK_NULL_HANDLE`."
        )))
        return getStatus();

    if (nativeHandle == VK_NULL_HANDLE) return getStatus();
    VkImage        image  = *static_cast<VkImage *>(nativeHandle);
    const VkFormat format = Vulkan::getFormat(unityFormat);
    VkImageView    view   = GraphicsAPI::get<Vulkan>()->createImageView(image, format, VK_IMAGE_ASPECT_DEPTH_BIT);

    if (failure(setStatusIf(
          view == VK_NULL_HANDLE,
          SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING,
          "Refused to set image resources due to an attempt to create the depth image view resulting in a "
          "`VK_NULL_HANDLE` view handle."
        )))
        return getStatus();

    const auto [width, height] =
      settings.quality == Settings::DYNAMIC_AUTO || settings.quality == Settings::DYNAMIC_MANUAL ?
      settings.dynamicMaximumInputResolution :
      settings.currentInputResolution;

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
      .Width  = width,
      .Height = height,
    });
    // clang-format on

    return getStatus();
}

Upscaler::Status DLSS::VulkanSetInputColor(void *nativeHandle, const UnityRenderingExtTextureFormat unityFormat) {
    if (failure(setStatusIf(
          nativeHandle == VK_NULL_HANDLE,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given input color image being `VK_NULL_HANDLE`."
        )))
        return getStatus();

    if (nativeHandle == VK_NULL_HANDLE) return getStatus();
    VkImage        image  = *static_cast<VkImage *>(nativeHandle);
    const VkFormat format = Vulkan::getFormat(unityFormat);
    VkImageView    view   = GraphicsAPI::get<Vulkan>()->createImageView(image, format, VK_IMAGE_ASPECT_COLOR_BIT);

    if (failure(setStatusIf(
          view == VK_NULL_HANDLE,
          SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING,
          "Refused to set image resources due to an attempt to create the input color image view resulting in a "
          "`VK_NULL_HANDLE` view handle."
        )))
        return getStatus();

    const auto [width, height] =
      settings.quality == Settings::DYNAMIC_AUTO || settings.quality == Settings::DYNAMIC_MANUAL ?
      settings.dynamicMaximumInputResolution :
      settings.currentInputResolution;

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
      .Width  = width,
      .Height = height,
    });
    // clang-format on

    return getStatus();
}

Upscaler::Status
DLSS::VulkanSetMotionVectors(void *nativeHandle, const UnityRenderingExtTextureFormat unityFormat) {
    if (failure(setStatusIf(
          nativeHandle == VK_NULL_HANDLE,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given motion vector image being `VK_NULL_HANDLE`."
        )))
        return getStatus();

    if (nativeHandle == VK_NULL_HANDLE) return getStatus();
    VkImage        image  = *static_cast<VkImage *>(nativeHandle);
    const VkFormat format = Vulkan::getFormat(unityFormat);
    VkImageView    view   = GraphicsAPI::get<Vulkan>()->createImageView(image, format, VK_IMAGE_ASPECT_COLOR_BIT);

    if (failure(setStatusIf(
          view == VK_NULL_HANDLE,
          SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING,
          "Refused to set image resources due to an attempt to create the motion vector image view resulting in a "
          "`VK_NULL_HANDLE` view handle."
        )))
        return getStatus();

    const auto [width, height] =
      settings.quality == Settings::DYNAMIC_AUTO || settings.quality == Settings::DYNAMIC_MANUAL ?
      settings.dynamicMaximumInputResolution :
      settings.currentInputResolution;

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
      .Width  = width,
      .Height = height,
    });
    // clang-format on

    return getStatus();
}

Upscaler::Status DLSS::VulkanSetOutputColor(void *nativeHandle, const UnityRenderingExtTextureFormat unityFormat) {
    if (failure(setStatusIf(
          nativeHandle == VK_NULL_HANDLE,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given output color image being `VK_NULL_HANDLE`."
        )))
        return getStatus();

    if (nativeHandle == VK_NULL_HANDLE) return getStatus();
    VkImage        image  = *static_cast<VkImage *>(nativeHandle);
    const VkFormat format = Vulkan::getFormat(unityFormat);
    VkImageView    view   = GraphicsAPI::get<Vulkan>()->createImageView(image, format, VK_IMAGE_ASPECT_COLOR_BIT);

    if (failure(setStatusIf(
          view == VK_NULL_HANDLE,
          SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING,
          "Refused to set image resources due to an attempt to create the output color image view resulting in a "
          "`VK_NULL_HANDLE` view handle."
        )))
        return getStatus();

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

    return getStatus();
}

Upscaler::Status DLSS::VulkanEvaluate() {
    UnityVulkanRecordingState state{};
    GraphicsAPI::get<Vulkan>()->getUnityInterface()->EnsureInsideRenderPass();
    GraphicsAPI::get<Vulkan>()->getUnityInterface()->CommandRecordingState(
      &state,
      kUnityVulkanGraphicsQueueAccess_DontCare
    );

    NVSDK_NGX_Resource_VK inColorResource  = inColor.vulkan->GetResource();
    NVSDK_NGX_Resource_VK outColorResource = outColor.vulkan->GetResource();
    NVSDK_NGX_Resource_VK depthResource    = depth.vulkan->GetResource();
    NVSDK_NGX_Resource_VK motionResource   = motion.vulkan->GetResource();

    // clang-format off
    NVSDK_NGX_VK_DLSS_Eval_Params DLSSEvalParameters = {
      .Feature = {
        .pInColor = &inColorResource,
        .pInOutput = &outColorResource,
        .InSharpness = settings.sharpness,
      },
      .pInDepth                  = &depthResource,
      .pInMotionVectors          = &motionResource,
      .InJitterOffsetX           = settings.jitter[0],
      .InJitterOffsetY           = settings.jitter[1],
      .InRenderSubrectDimensions = {
        .Width  = settings.currentInputResolution.width,
        .Height = settings.currentInputResolution.height,
      },
      .InReset    = static_cast<int>(settings.resetHistory),
      .InMVScaleX = -static_cast<float>(settings.currentInputResolution.width),
      .InMVScaleY = -static_cast<float>(settings.currentInputResolution.height),
#ifndef NDEBUG
      .InIndicatorInvertYAxis = 1,
#endif
    };
    // clang-format on

    settings.resetHistory = false;
    return setStatus(
      NGX_VULKAN_EVALUATE_DLSS_EXT(state.commandBuffer, featureHandle, parameters, &DLSSEvalParameters),
      "Failed to evaluate the " + getName() + " feature."
    );
}

Upscaler::Status DLSS::VulkanReleaseFeature() {
    if (featureHandle == nullptr) return getStatus();
    setStatus(NVSDK_NGX_VULKAN_ReleaseFeature(featureHandle), "Failed to release the " + getName() + " feature.");
    featureHandle = nullptr;
    return getStatus();
}

Upscaler::Status DLSS::VulkanDestroyParameters() {
    if (parameters == nullptr) return getStatus();
    setStatus(
      NVSDK_NGX_VULKAN_DestroyParameters(parameters),
      "Failed to release the " + getName() + " compatibility parameters."
    );
    parameters = nullptr;
    return getStatus();
}

Upscaler::Status DLSS::VulkanShutdown() {
    if (!initialized) return getStatus();
    VulkanDestroyParameters();
    VulkanReleaseFeature();
    inColor.vulkan->Destroy();
    outColor.vulkan->Destroy();
    depth.vulkan->Destroy();
    motion.vulkan->Destroy();
    return setStatus(
      NVSDK_NGX_VULKAN_Shutdown1(GraphicsAPI::get<Vulkan>()->getUnityInterface()->Instance().device),
      "Failed to shutdown the NGX instance."
    );
}
#endif

#ifdef ENABLE_DX12
Upscaler::Status DLSS::DX12Initialize() {
    return setStatus(
      NVSDK_NGX_D3D12_Init(
        applicationInfo.id,
        applicationInfo.dataPath.c_str(),
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

Upscaler::Status DLSS::DX12CreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams) {
    if (featureHandle != nullptr) return getStatus();

    if (ID3D12GraphicsCommandList *commandList = GraphicsAPI::get<DX12>()->beginOneTimeSubmitRecording();
        failure(setStatus(
          NGX_D3D12_CREATE_DLSS_EXT(commandList, 1U, 1U, &featureHandle, parameters, &DLSSCreateParams),
          "Failed to create the " + getName() + " feature."
        ))) {
        GraphicsAPI::get<DX12>()->cancelOneTimeSubmitRecording();
        return getStatus();
    }
    GraphicsAPI::get<DX12>()->endOneTimeSubmitRecording();
    return setStatusIf(
      featureHandle == nullptr,
      SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
      "Failed to create the " + getName() +
        " feature. The handle returned from `NGX_DX12_CREATE_DLSS_EXT()` was "
        "`nullptr`."
    );
}

Upscaler::Status DLSS::DX12SetDepthBuffer(void *nativeHandle, UnityRenderingExtTextureFormat /* unused */) {
    if (failure(setStatusIf(
          nativeHandle == nullptr,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given depth image being `VK_NULL_HANDLE`."
        )))
        return getStatus();
    depth.dx12 = static_cast<ID3D12Resource *>(nativeHandle);
    return getStatus();
}

Upscaler::Status DLSS::DX12SetInputColor(void *nativeHandle, UnityRenderingExtTextureFormat /* unused */) {
    if (failure(setStatusIf(
          nativeHandle == nullptr,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given input color image being `VK_NULL_HANDLE`."
        )))
        return getStatus();
    inColor.dx12 = static_cast<ID3D12Resource *>(nativeHandle);
    return getStatus();
}

Upscaler::Status DLSS::DX12SetMotionVectors(void *nativeHandle, UnityRenderingExtTextureFormat /* unused */) {
    if (failure(setStatusIf(
          nativeHandle == nullptr,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given motion vector image being `VK_NULL_HANDLE`."
        )))
        return getStatus();
    motion.dx12 = static_cast<ID3D12Resource *>(nativeHandle);
    return getStatus();
}

Upscaler::Status DLSS::DX12SetOutputColor(void *nativeHandle, UnityRenderingExtTextureFormat /* unused */) {
    if (failure(setStatusIf(
          nativeHandle == nullptr,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given output color image being `VK_NULL_HANDLE`."
        )))
        return getStatus();
    outColor.dx12 = static_cast<ID3D12Resource *>(nativeHandle);
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
        .Width  = settings.currentInputResolution.width,
        .Height = settings.currentInputResolution.height,
      },
      .InReset    = static_cast<int>(settings.resetHistory),
      .InMVScaleX = -static_cast<float>(settings.currentInputResolution.width),
      .InMVScaleY = -static_cast<float>(settings.currentInputResolution.height),
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
#endif

#ifdef ENABLE_DX11
Upscaler::Status DLSS::DX11Initialize() {
    return setStatus(
      NVSDK_NGX_D3D11_Init(
        applicationInfo.id,
        applicationInfo.dataPath.c_str(),
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

Upscaler::Status DLSS::DX11CreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams) {
    if (featureHandle != nullptr) return getStatus();
    if (ID3D11DeviceContext *context = GraphicsAPI::get<DX11>()->beginOneTimeSubmitRecording(); failure(setStatus(
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

Upscaler::Status DLSS::DX11SetDepthBuffer(void *nativeHandle, UnityRenderingExtTextureFormat /* unused */) {
    if (failure(setStatusIf(
          nativeHandle == nullptr,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given depth image being `VK_NULL_HANDLE`."
        )))
        return getStatus();
    depth.dx11 = static_cast<ID3D11Resource *>(nativeHandle);
    return getStatus();
}

Upscaler::Status DLSS::DX11SetInputColor(void *nativeHandle, UnityRenderingExtTextureFormat /* unused */) {
    if (failure(setStatusIf(
          nativeHandle == nullptr,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given input color image being `VK_NULL_HANDLE`."
        )))
        return getStatus();
    inColor.dx11 = static_cast<ID3D11Resource *>(nativeHandle);
    return getStatus();
}

Upscaler::Status DLSS::DX11SetMotionVectors(void *nativeHandle, UnityRenderingExtTextureFormat /* unused */) {
    if (failure(setStatusIf(
          nativeHandle == nullptr,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given motion vector image being `VK_NULL_HANDLE`."
        )))
        return getStatus();
    motion.dx11 = static_cast<ID3D11Resource *>(nativeHandle);
    return getStatus();
}

Upscaler::Status DLSS::DX11SetOutputColor(void *nativeHandle, UnityRenderingExtTextureFormat /* unused */) {
    if (failure(setStatusIf(
          nativeHandle == nullptr,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "Refused to set image resources due to the given output color image being `VK_NULL_HANDLE`."
        )))
        return getStatus();
    outColor.dx11 = static_cast<ID3D11Resource *>(nativeHandle);
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
        .Width  = settings.currentInputResolution.width,
        .Height = settings.currentInputResolution.height,
      },
      .InReset    = static_cast<int>(settings.resetHistory),
      .InMVScaleX = -static_cast<float>(settings.currentInputResolution.width),
      .InMVScaleY = -static_cast<float>(settings.currentInputResolution.height),
#ifndef NDEBUG
      .InIndicatorInvertYAxis = 1,
#endif
    };
    // clang-format on

    if (ID3D11DeviceContext *context = GraphicsAPI::get<DX11>()->beginOneTimeSubmitRecording(); failure(setStatus(
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
#endif

void DLSS::setFunctionPointers(const GraphicsAPI::Type graphicsAPI) {
    switch (graphicsAPI) {
        case GraphicsAPI::NONE: {
            graphicsAPIIndependentInitializeFunctionPointer       = &DLSS::safeFail;
            graphicsAPIIndependentGetParametersFunctionPointer    = &DLSS::safeFail;
            graphicsAPIIndependentCreateFeatureFunctionPointer    = &DLSS::safeFail;
            graphicsAPIIndependentSetDepthBufferFunctionPointer   = &DLSS::safeFail;
            graphicsAPIIndependentSetInputColorFunctionPointer    = &DLSS::safeFail;
            graphicsAPIIndependentSetMotionVectorsFunctionPointer = &DLSS::safeFail;
            graphicsAPIIndependentSetOutputColorFunctionPointer   = &DLSS::safeFail;
            graphicsAPIIndependentEvaluateFunctionPointer         = &DLSS::safeFail;
            graphicsAPIIndependentReleaseFeatureFunctionPointer   = &DLSS::safeFail;
            graphicsAPIIndependentShutdownFunctionPointer         = &DLSS::safeFail;
            break;
        }
#ifdef ENABLE_VULKAN
        case GraphicsAPI::VULKAN: {
            inColor.vulkan  = new RAII_NGXVulkanResource;
            outColor.vulkan = new RAII_NGXVulkanResource;
            depth.vulkan    = new RAII_NGXVulkanResource;
            motion.vulkan   = new RAII_NGXVulkanResource;

            graphicsAPIIndependentInitializeFunctionPointer       = &DLSS::VulkanInitialize;
            graphicsAPIIndependentGetParametersFunctionPointer    = &DLSS::VulkanGetParameters;
            graphicsAPIIndependentCreateFeatureFunctionPointer    = &DLSS::VulkanCreateFeature;
            graphicsAPIIndependentSetDepthBufferFunctionPointer   = &DLSS::VulkanSetDepthBuffer;
            graphicsAPIIndependentSetInputColorFunctionPointer    = &DLSS::VulkanSetInputColor;
            graphicsAPIIndependentSetMotionVectorsFunctionPointer = &DLSS::VulkanSetMotionVectors;
            graphicsAPIIndependentSetOutputColorFunctionPointer   = &DLSS::VulkanSetOutputColor;
            graphicsAPIIndependentEvaluateFunctionPointer         = &DLSS::VulkanEvaluate;
            graphicsAPIIndependentReleaseFeatureFunctionPointer   = &DLSS::VulkanReleaseFeature;
            graphicsAPIIndependentShutdownFunctionPointer         = &DLSS::VulkanShutdown;
            break;
        }
#endif
#ifdef ENABLE_DX12
        case GraphicsAPI::DX12: {
            graphicsAPIIndependentInitializeFunctionPointer       = &DLSS::DX12Initialize;
            graphicsAPIIndependentGetParametersFunctionPointer    = &DLSS::DX12GetParameters;
            graphicsAPIIndependentCreateFeatureFunctionPointer    = &DLSS::DX12CreateFeature;
            graphicsAPIIndependentSetDepthBufferFunctionPointer   = &DLSS::DX12SetDepthBuffer;
            graphicsAPIIndependentSetInputColorFunctionPointer    = &DLSS::DX12SetInputColor;
            graphicsAPIIndependentSetMotionVectorsFunctionPointer = &DLSS::DX12SetMotionVectors;
            graphicsAPIIndependentSetOutputColorFunctionPointer   = &DLSS::DX12SetOutputColor;
            graphicsAPIIndependentEvaluateFunctionPointer         = &DLSS::DX12Evaluate;
            graphicsAPIIndependentReleaseFeatureFunctionPointer   = &DLSS::DX12ReleaseFeature;
            graphicsAPIIndependentShutdownFunctionPointer         = &DLSS::DX12Shutdown;
            break;
        }
#endif
#ifdef ENABLE_DX11
        case GraphicsAPI::DX11: {
            graphicsAPIIndependentInitializeFunctionPointer       = &DLSS::DX11Initialize;
            graphicsAPIIndependentGetParametersFunctionPointer    = &DLSS::DX11GetParameters;
            graphicsAPIIndependentCreateFeatureFunctionPointer    = &DLSS::DX11CreateFeature;
            graphicsAPIIndependentSetDepthBufferFunctionPointer   = &DLSS::DX11SetDepthBuffer;
            graphicsAPIIndependentSetInputColorFunctionPointer    = &DLSS::DX11SetInputColor;
            graphicsAPIIndependentSetMotionVectorsFunctionPointer = &DLSS::DX11SetMotionVectors;
            graphicsAPIIndependentSetOutputColorFunctionPointer   = &DLSS::DX11SetOutputColor;
            graphicsAPIIndependentEvaluateFunctionPointer         = &DLSS::DX11Evaluate;
            graphicsAPIIndependentReleaseFeatureFunctionPointer   = &DLSS::DX11ReleaseFeature;
            graphicsAPIIndependentShutdownFunctionPointer         = &DLSS::DX11Shutdown;
            break;
        }
#endif
        default: {
            graphicsAPIIndependentInitializeFunctionPointer       = &DLSS::safeFail;
            graphicsAPIIndependentGetParametersFunctionPointer    = &DLSS::safeFail;
            graphicsAPIIndependentCreateFeatureFunctionPointer    = &DLSS::safeFail;
            graphicsAPIIndependentSetDepthBufferFunctionPointer   = &DLSS::safeFail;
            graphicsAPIIndependentSetInputColorFunctionPointer    = &DLSS::safeFail;
            graphicsAPIIndependentSetMotionVectorsFunctionPointer = &DLSS::safeFail;
            graphicsAPIIndependentSetOutputColorFunctionPointer   = &DLSS::safeFail;
            graphicsAPIIndependentEvaluateFunctionPointer         = &DLSS::safeFail;
            graphicsAPIIndependentReleaseFeatureFunctionPointer   = &DLSS::safeFail;
            graphicsAPIIndependentShutdownFunctionPointer         = &DLSS::safeFail;
            break;
        }
    }
}

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
        case NVSDK_NGX_Result_FAIL_UnableToInitializeFeature:
            error = SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING;
            break;
        case NVSDK_NGX_Result_FAIL_OutOfDate: error = SOFTWARE_ERROR_DEVICE_DRIVERS_OUT_OF_DATE; break;
        case NVSDK_NGX_Result_FAIL_OutOfGPUMemory: error = SOFTWARE_ERROR_OUT_OF_GPU_MEMORY; break;
        case NVSDK_NGX_Result_FAIL_UnsupportedFormat: error = SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING; break;
        case NVSDK_NGX_Result_FAIL_UnableToWriteToAppDataPath:
            error = SOFTWARE_ERROR_INVALID_WRITE_PERMISSIONS;
            break;
        case NVSDK_NGX_Result_FAIL_UnsupportedParameter: error = SETTINGS_ERROR; break;
        case NVSDK_NGX_Result_FAIL_Denied: error = SOFTWARE_ERROR_FEATURE_DENIED; break;
        case NVSDK_NGX_Result_FAIL_NotImplemented: error = SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR; break;
    }
    return Upscaler::setStatus(error, t_msg);
}

DLSS *DLSS::get() {
    static DLSS *dlss{new DLSS};
    return dlss;
}

Upscaler::Type DLSS::getType() {
    return Upscaler::DLSS;
}

std::string DLSS::getName() {
    return "NVIDIA DLSS";
}

std::vector<std::string> DLSS::getRequiredVulkanInstanceExtensions() {
    uint32_t                 extensionCount{};
    std::vector<std::string> extensions{};
    VkExtensionProperties   *extensionProperties{};
    if (failure(setStatus(
          NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(
            &applicationInfo.featureDiscoveryInfo,
            &extensionCount,
            &extensionProperties
          ),
          "The application information passed to " + getName() + " may not be configured properly."
        )))
        return {};
    extensions.reserve(extensionCount);
    for (uint32_t i{}; i < extensionCount; ++i) extensions.emplace_back(extensionProperties[i].extensionName);
    return extensions;
}

std::vector<std::string>
DLSS::getRequiredVulkanDeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice) {
    uint32_t                 extensionCount{};
    std::vector<std::string> extensions{};
    VkExtensionProperties   *extensionProperties{};
    if (failure(setStatus(
          NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(
            instance,
            physicalDevice,
            &applicationInfo.featureDiscoveryInfo,
            &extensionCount,
            &extensionProperties
          ),
          "The application information passed to " + getName() + " may not be configured properly."
        )))
        return {};
    extensions.reserve(extensionCount);
    for (uint32_t i{}; i < extensionCount; ++i) extensions.emplace_back(extensionProperties[i].extensionName);
    return extensions;
}

Upscaler::Settings DLSS::getOptimalSettings(
  const Settings::Resolution t_outputResolution,
  const Settings::Quality    t_quality,
  const bool                 t_HDR
) {
    if (parameters == nullptr) return settings;

    Settings optimalSettings         = settings;
    optimalSettings.outputResolution = t_outputResolution;
    optimalSettings.HDR              = t_HDR;
    optimalSettings.quality          = t_quality;

    failure(setStatus(
      NGX_DLSS_GET_OPTIMAL_SETTINGS(
        parameters,
        optimalSettings.outputResolution.width,
        optimalSettings.outputResolution.height,
        optimalSettings.getQuality<Upscaler::DLSS>(),
        &optimalSettings.recommendedInputResolution.width,
        &optimalSettings.recommendedInputResolution.height,
        &optimalSettings.dynamicMaximumInputResolution.width,
        &optimalSettings.dynamicMaximumInputResolution.height,
        &optimalSettings.dynamicMinimumInputResolution.width,
        &optimalSettings.dynamicMinimumInputResolution.height,
        &optimalSettings.sharpness
      ),
      "Some invalid setting was set. Ensure that the current input resolution is within allowed bounds given the"
      "output resolution, sharpness is between 0F and 1F, and that the Quality setting is less than Ultra Quality."
    ));

    return optimalSettings;
}

Upscaler::Status DLSS::initialize() {
    if (failure(setStatusIf(
          initialized,
          SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
          "This is probably ignorable. An attempt was made to initialize an upscaler that was already initialized."
        )))
        return getStatus();
    if (!resetStatus()) return getStatus();

    // Upscaler_Initialize NGX SDK
    Upscaler::setStatus((this->*graphicsAPIIndependentInitializeFunctionPointer)(), "Failed to initialized NGX.");
    if (failure(getStatus())) return getStatus();
    initialized = true;
    Upscaler::setStatus(
      (this->*graphicsAPIIndependentGetParametersFunctionPointer)(),
      "Failed to get NGX "
      "Parameters. This may be caused by a host of errors. If the Status is Unknown, then likely "
      "an unsupported graphics API is in use."
    );
    if (failure(getStatus())) return getStatus();
    // Check for DLSS support
    // Is driver up-to-date
    int needsUpdatedDriver{};
    if (failure(setStatus(
          parameters->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsUpdatedDriver),
          "Failed to query the selected device's driver update needs. This may result from outdated an driver, "
          "unsupported GPUs, a failure to initialize NGX, or a failure to get the " +
            getName() + " compatibility parameters."
        )))
        return getStatus();
    int requiredMajorDriverVersion{};
    if (failure(setStatus(
          parameters->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, &requiredMajorDriverVersion),
          "Failed to query the major version of the minimum " + getName() +
            " supported driver for the selected graphics device. This may result from outdated an driver, "
            "unsupported GPUs, a failure to initialize NGX, or a failure to get the " +
            getName() + " compatibility parameters."
        )))
        return getStatus();
    int requiredMinorDriverVersion{};
    if (failure(setStatus(
          parameters->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, &requiredMinorDriverVersion),
          "Failed to query the minor version of the minimum " + getName() +
            " supported driver for the selected graphics device. This may result from outdated an driver, "
            "unsupported GPUs, a failure to initialize NGX, or a failure to get the " +
            getName() + " compatibility parameters."
        )))
        return getStatus();
    if (failure(setStatusIf(
          needsUpdatedDriver != 0,
          SOFTWARE_ERROR_DEVICE_DRIVERS_OUT_OF_DATE,
          "The selected device's drivers are out-of-date. They must be (" +
            std::to_string(requiredMajorDriverVersion) + "." + std::to_string(requiredMinorDriverVersion) +
            ") at a minimum."
        )))
        return getStatus();
    // Is DLSS available on this hardware and platform
    int DLSSSupported{};
    if (failure(setStatus(
          parameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &DLSSSupported),
          "Failed to query status of " + getName() +
            " support for the selected graphics device. This may result from outdated an driver, unsupported "
            "GPUs, "
            "a failure to initialize NGX, or a failure to get the " +
            getName() + " compatibility parameters."
        )))
        return getStatus();
    if (failure(setStatusIf(
          DLSSSupported == 0,
          GENERIC_ERROR_DEVICE_OR_INSTANCE_EXTENSIONS_NOT_SUPPORTED,
          getName() +
            " is not supported on the selected graphics device. If you are certain that you have a DLSS "
            "compatible device, please ensure that it is being used by Unity for rendering."
        )))
        return getStatus();
    // Is DLSS denied for this application
    if (failure(setStatus(
          parameters->Get(NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, &DLSSSupported),
          "Failed to query the major version of the minimum " + getName() +
            " supported driver for the selected graphics device. This may result from outdated driver, "
            "unsupported GPUs, a failure to initialize NGX, or a failure to get the " +
            getName() + " compatibility parameters."
        )))
        return getStatus();
    // clean up
    return setStatusIf(
      DLSSSupported == 0,
      SOFTWARE_ERROR_FEATURE_DENIED,
      getName() + " has been denied for this application. Consult an NVIDIA representative for more information."
    );
}

Upscaler::Status DLSS::createFeature() {
    // clang-format off
    const NVSDK_NGX_DLSS_Create_Params DLSSCreateParams{
      .Feature = {
        .InWidth            = settings.recommendedInputResolution.width,
        .InHeight           = settings.recommendedInputResolution.height,
        .InTargetWidth      = settings.outputResolution.width,
        .InTargetHeight     = settings.outputResolution.height,
        .InPerfQualityValue = settings.getQuality<Upscaler::DLSS>(),
      },
      .InFeatureCreateFlags = static_cast<int>(
        static_cast<unsigned>(NVSDK_NGX_DLSS_Feature_Flags_MVLowRes) |
        static_cast<unsigned>(NVSDK_NGX_DLSS_Feature_Flags_MVJittered) |
        static_cast<unsigned>(NVSDK_NGX_DLSS_Feature_Flags_DepthInverted) |
        (settings.sharpness > 0.0F ? NVSDK_NGX_DLSS_Feature_Flags_DoSharpening : 0U) |
        (settings.HDR ? NVSDK_NGX_DLSS_Feature_Flags_IsHDR : 0U) |
        (settings.autoExposure ? NVSDK_NGX_DLSS_Feature_Flags_AutoExposure : 0U)
      ),
      .InEnableOutputSubrects = false,
    };
    // clang-format off

    (this->*graphicsAPIIndependentReleaseFeatureFunctionPointer)();
    if (getStatus() == SUCCESS)
        return (this->*graphicsAPIIndependentCreateFeatureFunctionPointer)(DLSSCreateParams);
    return getStatus();
}

Upscaler::Status DLSS::setDepthBuffer(
  void                          *nativeHandle,
  const UnityRenderingExtTextureFormat unityFormat
) {
    return (this->*graphicsAPIIndependentSetDepthBufferFunctionPointer)(nativeHandle, unityFormat);
}

Upscaler::Status DLSS::setInputColor(
  void                          *nativeHandle,
  const UnityRenderingExtTextureFormat unityFormat
) {
    return (this->*graphicsAPIIndependentSetInputColorFunctionPointer)(nativeHandle, unityFormat);
}

Upscaler::Status DLSS::setMotionVectors(
  void                          *nativeHandle,
  const UnityRenderingExtTextureFormat unityFormat
) {
    return (this->*graphicsAPIIndependentSetMotionVectorsFunctionPointer)(nativeHandle, unityFormat);
}

Upscaler::Status DLSS::setOutputColor(
  void                          *nativeHandle,
  const UnityRenderingExtTextureFormat unityFormat
) {
    return (this->*graphicsAPIIndependentSetOutputColorFunctionPointer)(nativeHandle, unityFormat);
}

Upscaler::Status DLSS::evaluate() {
    return (this->*graphicsAPIIndependentEvaluateFunctionPointer)();
}

Upscaler::Status DLSS::releaseFeature() {
    return (this->*graphicsAPIIndependentReleaseFeatureFunctionPointer)();
}

Upscaler::Status DLSS::shutdown() {
    const Status error = (this->*graphicsAPIIndependentShutdownFunctionPointer)();
    Upscaler::shutdown();
    return error;
}