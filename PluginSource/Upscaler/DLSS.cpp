#include "DLSS.hpp"

#ifdef ENABLE_DLSS

// Project
#    include "GraphicsAPI/DX11.hpp"
#    include "GraphicsAPI/DX12.hpp"
#    include "GraphicsAPI/Vulkan.hpp"

Upscaler::UpscalerStatus (DLSS::*DLSS::graphicsAPIIndependentInitializeFunctionPointer)(){&DLSS::safeFail};
Upscaler::UpscalerStatus (DLSS::*DLSS::graphicsAPIIndependentGetParametersFunctionPointer)(){&DLSS::safeFail};
Upscaler::UpscalerStatus (DLSS::*DLSS::graphicsAPIIndependentCreateFeatureFunctionPointer)(
  NVSDK_NGX_DLSS_Create_Params
){&DLSS::safeFail};
Upscaler::UpscalerStatus (DLSS::*DLSS::graphicsAPIIndependentSetImageResourcesFunctionPointer)(
  void *,
  UnityRenderingExtTextureFormat,
  void *,
  UnityRenderingExtTextureFormat,
  void *,
  UnityRenderingExtTextureFormat,
  void *,
  UnityRenderingExtTextureFormat
){&DLSS::safeFail};
Upscaler::UpscalerStatus (DLSS::*DLSS::graphicsAPIIndependentEvaluateFunctionPointer)(){&DLSS::safeFail};
Upscaler::UpscalerStatus (DLSS::*DLSS::graphicsAPIIndependentReleaseFeatureFunctionPointer)(){&DLSS::safeFail};
Upscaler::UpscalerStatus (DLSS::*DLSS::graphicsAPIIndependentShutdownFunctionPointer)(){&DLSS::safeFail};

#    ifdef ENABLE_VULKAN
Upscaler::UpscalerStatus DLSS::VulkanInitialize() {
    UnityVulkanInstance vulkanInstance = GraphicsAPI::get<Vulkan>()->getUnityInterface()->Instance();

    return setError(NVSDK_NGX_VULKAN_Init(
      applicationInfo.id,
      applicationInfo.dataPath.c_str(),
      vulkanInstance.instance,
      vulkanInstance.physicalDevice,
      vulkanInstance.device
    ), "Failed to initialize the NGX instance.");
}

Upscaler::UpscalerStatus DLSS::VulkanGetParameters() {
    if (parameters != nullptr) return getError();
    setError(NVSDK_NGX_VULKAN_GetCapabilityParameters(&parameters), "Failed to get the " + getName() + " compatibility parameters");
    return getError();
}

Upscaler::UpscalerStatus DLSS::VulkanCreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams) {
    if (parameters == nullptr) VulkanGetParameters();
    if (featureHandle != nullptr) VulkanReleaseFeature();
    GraphicsAPI::get<Vulkan>()->getUnityInterface()->EnsureOutsideRenderPass();

    VkCommandBuffer commandBuffer = GraphicsAPI::get<Vulkan>()->beginOneTimeSubmitRecording();
    if (failure(setError(NGX_VULKAN_CREATE_DLSS_EXT(commandBuffer, 1, 1, &featureHandle, parameters, &DLSSCreateParams), "Vulkan failed to create the " + getName() + " feature."))) {
        GraphicsAPI::get<Vulkan>()->cancelOneTimeSubmitRecording();
        return getError();
    }
    GraphicsAPI::get<Vulkan>()->endOneTimeSubmitRecording();
    return setErrorIf(
      featureHandle == nullptr,
      SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
      "Failed to create the " + getName() + " feature. The handle returned from `NGX_VULKAN_CREATE_DLSS_EXT()` was "
      "`nullptr`."
    );
}

void DLSS::VulkanDestroyImageViews() {
    if (depth.vulkan != nullptr) {
        GraphicsAPI::get<Vulkan>()->destroyImageView(depth.vulkan->Resource.ImageViewInfo.ImageView);
        std::free(depth.vulkan);
        depth.vulkan = nullptr;
    }
    if (motion.vulkan != nullptr) {
        GraphicsAPI::get<Vulkan>()->destroyImageView(motion.vulkan->Resource.ImageViewInfo.ImageView);
        std::free(motion.vulkan);
        motion.vulkan = nullptr;
    }
    if (inColor.vulkan != nullptr) {
        GraphicsAPI::get<Vulkan>()->destroyImageView(inColor.vulkan->Resource.ImageViewInfo.ImageView);
        std::free(inColor.vulkan);
        inColor.vulkan = nullptr;
    }
    if (outColor.vulkan != nullptr) {
        GraphicsAPI::get<Vulkan>()->destroyImageView(outColor.vulkan->Resource.ImageViewInfo.ImageView);
        std::free(outColor.vulkan);
        outColor.vulkan = nullptr;
    }
}

Upscaler::UpscalerStatus DLSS::VulkanSetImageResources(
  void                          *nativeDepthBuffer,
  UnityRenderingExtTextureFormat unityDepthFormat,
  void                          *nativeMotionVectors,
  UnityRenderingExtTextureFormat unityMotionVectorFormat,
  void                          *nativeInColor,
  UnityRenderingExtTextureFormat unityInColorFormat,
  void                          *nativeOutColor,
  UnityRenderingExtTextureFormat unityOutColorFormat
) {
    bool isSafeToContinue{true};
    isSafeToContinue &= success(setErrorIf(
      nativeDepthBuffer == VK_NULL_HANDLE,
      UpscalerStatus::SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
      "Refused to set image resources due to the given depth image being `VK_NULL_HANDLE`."
    ));
    isSafeToContinue &= success(setErrorIf(
      nativeMotionVectors == VK_NULL_HANDLE,
      UpscalerStatus::SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
      "Refused to set image resources due to the given motion vector image being `VK_NULL_HANDLE`."
    ));
    isSafeToContinue &= success(setErrorIf(
      nativeInColor == VK_NULL_HANDLE,
      UpscalerStatus::SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
      "Refused to set image resources due to the given input color image being `VK_NULL_HANDLE`."
    ));
    isSafeToContinue &= success(setErrorIf(
      nativeInColor == VK_NULL_HANDLE,
      UpscalerStatus::SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING,
      "Refused to set image resources due to the given output color image being `VK_NULL_HANDLE`."
    ));
    if (!isSafeToContinue) return getError();

    VulkanDestroyImageViews();

    VkImage depthBuffer   = *reinterpret_cast<VkImage *>(nativeDepthBuffer);
    VkImage motionVectors = *reinterpret_cast<VkImage *>(nativeMotionVectors);
    VkImage inColorImage  = *reinterpret_cast<VkImage *>(nativeInColor);
    VkImage outColorImage = *reinterpret_cast<VkImage *>(nativeOutColor);

    VkFormat depthFormat        = Vulkan::getFormat(unityDepthFormat);
    VkFormat motionVectorFormat = Vulkan::getFormat(unityMotionVectorFormat);
    VkFormat inColorFormat      = Vulkan::getFormat(unityInColorFormat);
    VkFormat outColorFormat     = Vulkan::getFormat(unityOutColorFormat);

    VkImageView depthView =
      GraphicsAPI::get<Vulkan>()->get2DImageView(depthBuffer, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
    VkImageView motionVectorView =
      GraphicsAPI::get<Vulkan>()->get2DImageView(motionVectors, motionVectorFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    VkImageView inColorView =
      GraphicsAPI::get<Vulkan>()->get2DImageView(inColorImage, inColorFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    VkImageView outColorView =
      GraphicsAPI::get<Vulkan>()->get2DImageView(outColorImage, outColorFormat, VK_IMAGE_ASPECT_COLOR_BIT);

    isSafeToContinue &= success(setErrorIf(
      depthView == VK_NULL_HANDLE,
      UpscalerStatus::SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING,
      "Refused to set image resources due to an attempt to create the depth image view resulting in a `VK_NULL_HANDLE` view handle."
    ));
    isSafeToContinue &= success(setErrorIf(
      motionVectorView == VK_NULL_HANDLE,
      UpscalerStatus::SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING,
      "Refused to set image resources due to an attempt to create the motion vector image view resulting in a `VK_NULL_HANDLE` view handle."
    ));
    isSafeToContinue &= success(setErrorIf(
      inColorView == VK_NULL_HANDLE,
      UpscalerStatus::SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING,
      "Refused to set image resources due to an attempt to create the input color image view resulting in a `VK_NULL_HANDLE` view handle."
    ));
    isSafeToContinue &= success(setErrorIf(
      outColorView == VK_NULL_HANDLE,
      UpscalerStatus::SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING,
      "Refused to set image resources due to an attempt to create the output color image view resulting in a `VK_NULL_HANDLE` view handle."
    ));
    if (!isSafeToContinue) return getError();

    Upscaler::Settings::Resolution maxRenderResolution = (settings.quality == Upscaler::Settings::DYNAMIC_AUTO ||
        settings.quality == Upscaler::Settings::DYNAMIC_MANUAL) ?
      settings.dynamicMaximumInputResolution :
      settings.currentInputResolution;

    // clang-format off
    depth.vulkan = new NVSDK_NGX_Resource_VK {
      .Resource = {
        .ImageViewInfo = {
          .ImageView        = depthView,
          .Image            = depthBuffer,
          .SubresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
          },
          .Format = depthFormat,
          .Width  = maxRenderResolution.width,
          .Height = maxRenderResolution.height,
        },
      },
      .Type      = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW,
      .ReadWrite = true,
    };

    motion.vulkan = new NVSDK_NGX_Resource_VK {
      .Resource = {
        .ImageViewInfo = {
          .ImageView        = motionVectorView,
          .Image            = motionVectors,
          .SubresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
          },
          .Format = motionVectorFormat,
          .Width  = settings.outputResolution.width,
          .Height = settings.outputResolution.height,
        },
      },
      .Type      = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW,
      .ReadWrite = true,
    };

    inColor.vulkan = new NVSDK_NGX_Resource_VK {
      .Resource = {
        .ImageViewInfo = {
          .ImageView        = inColorView,
          .Image            = inColorImage,
          .SubresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
          },
          .Format = inColorFormat,
          .Width  = maxRenderResolution.width,
          .Height = maxRenderResolution.height,
        },
      },
      .Type      = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW,
      .ReadWrite = true,
    };

    outColor.vulkan = new NVSDK_NGX_Resource_VK {
      .Resource = {
        .ImageViewInfo = {
          .ImageView        = outColorView,
          .Image            = outColorImage,
          .SubresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
          },
          .Format = outColorFormat,
          .Width  = settings.outputResolution.width,
          .Height = settings.outputResolution.height,
        },
      },
      .Type      = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW,
      .ReadWrite = true,
    };
    // clang-format on

    return getError();
}

Upscaler::UpscalerStatus DLSS::VulkanEvaluate() {
    UnityVulkanRecordingState state{};
    GraphicsAPI::get<Vulkan>()->getUnityInterface()->EnsureInsideRenderPass();
    GraphicsAPI::get<Vulkan>()->getUnityInterface()->CommandRecordingState(
      &state,
      kUnityVulkanGraphicsQueueAccess_DontCare
    );

    // clang-format off
    NVSDK_NGX_VK_DLSS_Eval_Params DLSSEvalParameters = {
      .Feature = {
        .pInColor = inColor.vulkan,
        .pInOutput = outColor.vulkan,
        .InSharpness = settings.sharpness,
      },
      .pInDepth                  = depth.vulkan,
      .pInMotionVectors          = motion.vulkan,
      .InJitterOffsetX           = settings.jitter[0],
      .InJitterOffsetY           = settings.jitter[1],
      .InRenderSubrectDimensions = {
        .Width  = settings.currentInputResolution.width,
        .Height = settings.currentInputResolution.height,
      },
      .InReset    = (int)settings.resetHistory,
      .InMVScaleX = -(float)settings.currentInputResolution.width,
      .InMVScaleY = -(float)settings.currentInputResolution.height,
    };
    // clang-format on

    settings.resetHistory = false;
    return setError(
      NGX_VULKAN_EVALUATE_DLSS_EXT(state.commandBuffer, featureHandle, parameters, &DLSSEvalParameters),
      "Failed to evaluate the " + getName() + " feature."
    );
}

Upscaler::UpscalerStatus DLSS::VulkanReleaseFeature() {
    if (featureHandle == nullptr) return getError();
    setError(NVSDK_NGX_VULKAN_ReleaseFeature(featureHandle), "Failed to release the " + getName() + " feature.");
    featureHandle = nullptr;
    return getError();
}

Upscaler::UpscalerStatus DLSS::VulkanDestroyParameters() {
    if (parameters == nullptr) return getError();
    setError(NVSDK_NGX_VULKAN_DestroyParameters(parameters), "Failed to release the " + getName() + " compatibility parameters.");
    parameters = nullptr;
    return getError();
}

Upscaler::UpscalerStatus DLSS::VulkanShutdown() {
    if (!initialized) return getError();
    VulkanDestroyParameters();
    VulkanReleaseFeature();
    VulkanDestroyImageViews();
    return setError(NVSDK_NGX_VULKAN_Shutdown1(nullptr), "Failed to shutdown the NGX instance.");
}
#    endif

#    ifdef ENABLE_DX12
Upscaler::UpscalerStatus DLSS::DX12Initialize() {
    return setError(NVSDK_NGX_D3D12_Init(
      applicationInfo.id,
      applicationInfo.dataPath.c_str(),
      GraphicsAPI::get<DX12>()->getUnityInterface()->GetDevice()
    ));
}

Upscaler::UpscalerStatus DLSS::DX12GetParameters() {
    if (parameters != nullptr) return getError();
    return setError(NVSDK_NGX_D3D12_GetCapabilityParameters(&parameters));
}

Upscaler::UpscalerStatus DLSS::DX12CreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams) {
    if (featureHandle != nullptr) return getError();

    ID3D12GraphicsCommandList *commandList = GraphicsAPI::get<DX12>()->beginOneTimeSubmitRecording();
    if (setError(NGX_D3D12_CREATE_DLSS_EXT(commandList, 1U, 1U, &featureHandle, parameters, &DLSSCreateParams)) != SUCCESS) {
        GraphicsAPI::get<DX12>()->cancelOneTimeSubmitRecording();
        return getError();
    }
    GraphicsAPI::get<DX12>()->endOneTimeSubmitRecording();
    return setErrorIf(featureHandle == nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING);
}

Upscaler::UpscalerStatus DLSS::DX12SetImageResources(
  void *nativeDepthBuffer,
  UnityRenderingExtTextureFormat /* unused */,
  void *nativeMotionVectors,
  UnityRenderingExtTextureFormat /* unused */,
  void *nativeInColor,
  UnityRenderingExtTextureFormat /* unused */,
  void *nativeOutColor,
  UnityRenderingExtTextureFormat /* unused */
) {
    depth.dx12    = reinterpret_cast<ID3D12Resource *>(nativeDepthBuffer);
    motion.dx12   = reinterpret_cast<ID3D12Resource *>(nativeMotionVectors);
    inColor.dx12  = reinterpret_cast<ID3D12Resource *>(nativeInColor);
    outColor.dx12 = reinterpret_cast<ID3D12Resource *>(nativeOutColor);
    return setErrorIf(
      depth.dx12 == nullptr | motion.dx12 == nullptr | inColor.dx12 == nullptr | outColor.dx12 == nullptr,
      SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING
    );
}

Upscaler::UpscalerStatus DLSS::DX12Evaluate() {
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
      .InMVScaleX = (float)settings.outputResolution.width,
      .InMVScaleY = (float)settings.outputResolution.height,
    };
    // clang-format on

    return setError(NGX_D3D12_EVALUATE_DLSS_EXT(state.commandList, featureHandle, parameters, &DLSSEvalParameters)
    );
}

Upscaler::UpscalerStatus DLSS::DX12ReleaseFeature() {
    if (featureHandle == nullptr) return getError();
    setError(NVSDK_NGX_D3D12_ReleaseFeature(featureHandle));
    featureHandle = nullptr;
    return getError();
}

Upscaler::UpscalerStatus DLSS::DX12DestroyParameters() {
    if (parameters == nullptr) return getError();
    setError(NVSDK_NGX_D3D12_DestroyParameters(parameters));
    parameters = nullptr;
    return getError();
}

Upscaler::UpscalerStatus DLSS::DX12Shutdown() {
    if (!initialized) return getError();
    DX12DestroyParameters();
    DX12ReleaseFeature();
    return setError(NVSDK_NGX_D3D12_Shutdown1(nullptr));
}
#    endif

#    ifdef ENABLE_DX11
Upscaler::UpscalerStatus DLSS::DX11Initialize() {
    return setError(NVSDK_NGX_D3D11_Init(
      applicationInfo.id,
      applicationInfo.dataPath.c_str(),
      GraphicsAPI::get<DX11>()->getUnityInterface()->GetDevice()
    ));
}

Upscaler::UpscalerStatus DLSS::DX11GetParameters() {
    if (parameters != nullptr) return getError();
    return setError(NVSDK_NGX_D3D11_GetCapabilityParameters(&parameters));
}

Upscaler::UpscalerStatus DLSS::DX11CreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams) {
    if (featureHandle != nullptr) return getError();
    ID3D11DeviceContext *context = GraphicsAPI::get<DX11>()->beginOneTimeSubmitRecording();
    NVSDK_NGX_Result result = NGX_D3D11_CREATE_DLSS_EXT(context, &featureHandle, parameters, &DLSSCreateParams);
    if (setError(result) != SUCCESS) {
        GraphicsAPI::get<DX11>()->cancelOneTimeSubmitRecording();
        return getError();
    }
    GraphicsAPI::get<DX11>()->endOneTimeSubmitRecording();
    return setErrorIf(featureHandle == nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING);
}

Upscaler::UpscalerStatus DLSS::DX11SetImageResources(
  void *nativeDepthBuffer,
  UnityRenderingExtTextureFormat /* unused */,
  void *nativeMotionVectors,
  UnityRenderingExtTextureFormat /* unused */,
  void *nativeInColor,
  UnityRenderingExtTextureFormat /* unused */,
  void *nativeOutColor,
  UnityRenderingExtTextureFormat /* unused */
) {
    depth.dx11    = reinterpret_cast<ID3D11Resource *>(nativeDepthBuffer);
    motion.dx11   = reinterpret_cast<ID3D11Resource *>(nativeMotionVectors);
    inColor.dx11  = reinterpret_cast<ID3D11Resource *>(nativeInColor);
    outColor.dx11 = reinterpret_cast<ID3D11Resource *>(nativeOutColor);
    return setErrorIf(
      depth.dx11 == nullptr || motion.dx11 == nullptr || inColor.dx11 == nullptr || outColor.dx11 == nullptr,
      SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING
    );
}

Upscaler::UpscalerStatus DLSS::DX11Evaluate() {
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
      .InMVScaleX = (float)settings.outputResolution.width,
      .InMVScaleY = (float)settings.outputResolution.height,
    };
    // clang-format on

    ID3D11DeviceContext *context = GraphicsAPI::get<DX11>()->beginOneTimeSubmitRecording();
    if (setError(NGX_D3D11_EVALUATE_DLSS_EXT(context, featureHandle, parameters, &DLSSEvalParams)) != SUCCESS) {
        GraphicsAPI::get<DX11>()->cancelOneTimeSubmitRecording();
        return getError();
    }
    GraphicsAPI::get<DX11>()->endOneTimeSubmitRecording();
    return getError();
}

Upscaler::UpscalerStatus DLSS::DX11ReleaseFeature() {
    if (featureHandle == nullptr) return getError();
    setError(NVSDK_NGX_D3D11_ReleaseFeature(featureHandle));
    featureHandle = nullptr;
    return getError();
}

Upscaler::UpscalerStatus DLSS::DX11DestroyParameters() {
    if (parameters == nullptr) return getError();
    setError(NVSDK_NGX_D3D11_DestroyParameters(parameters));
    parameters = nullptr;
    return getError();
}

Upscaler::UpscalerStatus DLSS::DX11Shutdown() {
    if (!initialized) return getError();
    DX11DestroyParameters();
    DX11ReleaseFeature();
    return setError(NVSDK_NGX_D3D11_Shutdown1(nullptr));
}
#    endif

void DLSS::setFunctionPointers(GraphicsAPI::Type graphicsAPI) {
    switch (graphicsAPI) {
        case GraphicsAPI::NONE: {
            graphicsAPIIndependentInitializeFunctionPointer        = &DLSS::safeFail;
            graphicsAPIIndependentGetParametersFunctionPointer     = &DLSS::safeFail;
            graphicsAPIIndependentCreateFeatureFunctionPointer     = &DLSS::safeFail;
            graphicsAPIIndependentSetImageResourcesFunctionPointer = &DLSS::safeFail;
            graphicsAPIIndependentEvaluateFunctionPointer          = &DLSS::safeFail;
            graphicsAPIIndependentReleaseFeatureFunctionPointer    = &DLSS::safeFail;
            graphicsAPIIndependentShutdownFunctionPointer          = &DLSS::safeFail;
            break;
        }
#    ifdef ENABLE_VULKAN
        case GraphicsAPI::VULKAN: {
            graphicsAPIIndependentInitializeFunctionPointer        = &DLSS::VulkanInitialize;
            graphicsAPIIndependentGetParametersFunctionPointer     = &DLSS::VulkanGetParameters;
            graphicsAPIIndependentCreateFeatureFunctionPointer     = &DLSS::VulkanCreateFeature;
            graphicsAPIIndependentSetImageResourcesFunctionPointer = &DLSS::VulkanSetImageResources;
            graphicsAPIIndependentEvaluateFunctionPointer          = &DLSS::VulkanEvaluate;
            graphicsAPIIndependentReleaseFeatureFunctionPointer    = &DLSS::VulkanReleaseFeature;
            graphicsAPIIndependentShutdownFunctionPointer          = &DLSS::VulkanShutdown;
            break;
        }
#    endif
#    ifdef ENABLE_DX12
        case GraphicsAPI::DX12: {
            graphicsAPIIndependentInitializeFunctionPointer        = &DLSS::DX12Initialize;
            graphicsAPIIndependentGetParametersFunctionPointer     = &DLSS::DX12GetParameters;
            graphicsAPIIndependentCreateFeatureFunctionPointer     = &DLSS::DX12CreateFeature;
            graphicsAPIIndependentSetImageResourcesFunctionPointer = &DLSS::DX12SetImageResources;
            graphicsAPIIndependentEvaluateFunctionPointer          = &DLSS::DX12Evaluate;
            graphicsAPIIndependentReleaseFeatureFunctionPointer    = &DLSS::DX12ReleaseFeature;
            graphicsAPIIndependentShutdownFunctionPointer          = &DLSS::DX12Shutdown;
            break;
        }
#    endif
#    ifdef ENABLE_DX11
        case GraphicsAPI::DX11: {
            graphicsAPIIndependentInitializeFunctionPointer        = &DLSS::DX11Initialize;
            graphicsAPIIndependentGetParametersFunctionPointer     = &DLSS::DX11GetParameters;
            graphicsAPIIndependentCreateFeatureFunctionPointer     = &DLSS::DX11CreateFeature;
            graphicsAPIIndependentSetImageResourcesFunctionPointer = &DLSS::DX11SetImageResources;
            graphicsAPIIndependentEvaluateFunctionPointer          = &DLSS::DX11Evaluate;
            graphicsAPIIndependentReleaseFeatureFunctionPointer    = &DLSS::DX11ReleaseFeature;
            graphicsAPIIndependentShutdownFunctionPointer          = &DLSS::DX11Shutdown;
            break;
        }
#    endif
        default: {
            graphicsAPIIndependentInitializeFunctionPointer        = &DLSS::safeFail;
            graphicsAPIIndependentGetParametersFunctionPointer     = &DLSS::safeFail;
            graphicsAPIIndependentCreateFeatureFunctionPointer     = &DLSS::safeFail;
            graphicsAPIIndependentSetImageResourcesFunctionPointer = &DLSS::safeFail;
            graphicsAPIIndependentEvaluateFunctionPointer          = &DLSS::safeFail;
            graphicsAPIIndependentReleaseFeatureFunctionPointer    = &DLSS::safeFail;
            graphicsAPIIndependentShutdownFunctionPointer          = &DLSS::safeFail;
            break;
        }
    }
}

DLSS *DLSS::get() {
    static DLSS *dlss{new DLSS};
    return dlss;
}

std::vector<std::string> DLSS::getRequiredVulkanInstanceExtensions() {
    uint32_t                 extensionCount{};
    std::vector<std::string> extensions{};
    VkExtensionProperties   *extensionProperties{};
    if (failure(setError(NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(&applicationInfo.featureDiscoveryInfo, &extensionCount, &extensionProperties), "The application information passed to " + getName() + " may not be configured properly.")))
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
    if (failure(setError(NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(instance, physicalDevice, &applicationInfo.featureDiscoveryInfo, &extensionCount, &extensionProperties), "The application information passed to " + getName() + " may not be configured properly.")))
        return {};
    extensions.reserve(extensionCount);
    for (uint32_t i{}; i < extensionCount; ++i) extensions.emplace_back(extensionProperties[i].extensionName);
    return extensions;
}

Upscaler::Type DLSS::getType() {
    return Upscaler::DLSS;
}

std::string DLSS::getName() {
    return "NVIDIA DLSS";
}

Upscaler::UpscalerStatus DLSS::initialize() {
    if (failure(setErrorIf(initialized, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "This is probably ignorable. An attempt was made to initialize an upscaler that was already initialized."))) return getError();
    if (!resetError()) return getError();

    // Upscaler_Initialize NGX SDK
    (this->*graphicsAPIIndependentInitializeFunctionPointer)();
    initialized = true;
    (this->*graphicsAPIIndependentGetParametersFunctionPointer)();
    // Check for DLSS support
    // Is driver up-to-date
    int needsUpdatedDriver{};
    if (failure(setError(parameters->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsUpdatedDriver), "Failed to query the selected device's driver update needs. This may result from outdated driver, unsupported GPUs, a failure to initialize NGX, or a failure to get the " + getName() + " compatibility parameters.")))
        return getError();
    int requiredMajorDriverVersion{};
    if (failure(setError(parameters->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, &requiredMajorDriverVersion), "Failed to query the major version of the minimum " + getName() + " supported driver for the selected graphics device. This may result from outdated driver, unsupported GPUs, a failure to initialize NGX, or a failure to get the " + getName() + " compatibility parameters.")))
        return getError();
    int requiredMinorDriverVersion{};
    if (failure(setError(parameters->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, &requiredMinorDriverVersion), "Failed to query the minor version of the minimum " + getName() + " supported driver for the selected graphics device. This may result from outdated driver, unsupported GPUs, a failure to initialize NGX, or a failure to get the " + getName() + " compatibility parameters.")))
        return getError();
    if (failure(setErrorIf(needsUpdatedDriver != 0, SOFTWARE_ERROR_DEVICE_DRIVERS_OUT_OF_DATE, "The selected device's drivers are out-of-date. They must be (" + std::to_string(requiredMajorDriverVersion) + "." + std::to_string(requiredMinorDriverVersion) + ") at a minimum.")))
        return getError();
    // Is DLSS available on this hardware and platform
    int DLSSSupported{};
    if (failure(setError(parameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &DLSSSupported), "Failed to query status of " + getName() + " support for the selected graphics device. This may result from outdated driver, unsupported GPUs, a failure to initialize NGX, or a failure to get the " + getName() + " compatibility parameters.")))
        return getError();
    if (failure(setErrorIf(DLSSSupported == 0, GENERIC_ERROR_DEVICE_OR_INSTANCE_EXTENSIONS_NOT_SUPPORTED, getName() + " is not supported on the selected graphics device. If you are certain that you have a DLSS compatible device, please ensure that it is being used by Unity for rendering.")))
        return getError();
    // Is DLSS denied for this application
    if (failure(setError(parameters->Get(NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, &DLSSSupported), "Failed to query the major version of the minimum " + getName() + " supported driver for the selected graphics device. This may result from outdated driver, unsupported GPUs, a failure to initialize NGX, or a failure to get the " + getName() + " compatibility parameters.")))
        return getError();
    // clean up
    return setErrorIf(DLSSSupported == 0, SOFTWARE_ERROR_FEATURE_DENIED, getName() + " has been denied for this application. Consult an NVIDIA representative for more information.");
}

Upscaler::UpscalerStatus DLSS::createFeature() {
    // clang-format off
    NVSDK_NGX_DLSS_Create_Params DLSSCreateParams{
      .Feature = {
        .InWidth            = settings.recommendedInputResolution.width,
        .InHeight           = settings.recommendedInputResolution.height,
        .InTargetWidth      = settings.outputResolution.width,
        .InTargetHeight     = settings.outputResolution.height,
        .InPerfQualityValue = settings.getQuality<Upscaler::DLSS>(),
      },
      .InFeatureCreateFlags = static_cast<int>(
        NVSDK_NGX_DLSS_Feature_Flags_MVJittered | NVSDK_NGX_DLSS_Feature_Flags_DepthInverted |
        (settings.sharpness > 0 ? NVSDK_NGX_DLSS_Feature_Flags_DoSharpening : 0U) |
        (settings.HDR ? NVSDK_NGX_DLSS_Feature_Flags_IsHDR : 0U) |
        (settings.autoExposure ? NVSDK_NGX_DLSS_Feature_Flags_AutoExposure : 0U)
      ),
      .InEnableOutputSubrects = false,
    };
    // clang-format off

    (this->*graphicsAPIIndependentReleaseFeatureFunctionPointer)();
    if (getError() == SUCCESS)
        return (this->*graphicsAPIIndependentCreateFeatureFunctionPointer)(DLSSCreateParams);
    return getError();
}

Upscaler::UpscalerStatus DLSS::evaluate() {
    return (this->*graphicsAPIIndependentEvaluateFunctionPointer)();
}

Upscaler::UpscalerStatus DLSS::releaseFeature() {
    return (this->*graphicsAPIIndependentReleaseFeatureFunctionPointer)();
}

Upscaler::UpscalerStatus DLSS::shutdown() {
    UpscalerStatus error = (this->*graphicsAPIIndependentShutdownFunctionPointer)();
    Upscaler::shutdown();
    return error;
}

Upscaler::Settings DLSS::getOptimalSettings(Settings::Resolution t_outputResolution, Settings::Quality t_quality, bool t_HDR) {
    if (parameters == nullptr) return settings;

    Settings optimalSettings = settings;
    optimalSettings.outputResolution = t_outputResolution;
    optimalSettings.HDR = t_HDR;
    optimalSettings.quality = t_quality;

    if (failure(setError(NGX_DLSS_GET_OPTIMAL_SETTINGS(
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
    ), "Some invalid setting was set. Ensure that the current input resolution is within allowed bounds given the output resolution, sharpness is between 0F and 1F, and that the Quality setting is less than Ultra Quality."))) {
        optimalSettings.recommendedInputResolution    = optimalSettings.outputResolution;
        optimalSettings.dynamicMaximumInputResolution = optimalSettings.outputResolution;
        optimalSettings.dynamicMinimumInputResolution = optimalSettings.outputResolution;
        optimalSettings.sharpness                     = 0.F;
    }

    return optimalSettings;
}

Upscaler::UpscalerStatus DLSS::setImageResources(
  void                          *nativeDepthBuffer,
  UnityRenderingExtTextureFormat unityDepthFormat,
  void                          *nativeMotionVectors,
  UnityRenderingExtTextureFormat unityMotionVectorFormat,
  void                          *nativeInColor,
  UnityRenderingExtTextureFormat unityInColorFormat,
  void                          *nativeOutColor,
  UnityRenderingExtTextureFormat unityOutColorFormat
) {
    return (this->*graphicsAPIIndependentSetImageResourcesFunctionPointer)(
      nativeDepthBuffer,
      unityDepthFormat,
      nativeMotionVectors,
      unityMotionVectorFormat,
      nativeInColor,
      unityInColorFormat,
      nativeOutColor,
      unityOutColorFormat
    );
}

Upscaler::UpscalerStatus DLSS::setError(NVSDK_NGX_Result t_error, std::string t_msg) {
    std::wstring message = GetNGXResultAsString(t_error);
    t_msg += " | " + std::string{message.begin(), message.end()};
    UpscalerStatus error = SUCCESS;
    switch (t_error) {
        case NVSDK_NGX_Result_Success: error = SUCCESS; break;
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
    return Upscaler::setError(error, t_msg);
}
#endif