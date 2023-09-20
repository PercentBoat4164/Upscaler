#include "DLSS.hpp"

#ifdef ENABLE_DLSS

// Project
#    include "GraphicsAPI/DX11.hpp"
#    include "GraphicsAPI/DX12.hpp"
#    include "GraphicsAPI/Vulkan.hpp"

// System
#    include <iomanip>

bool (DLSS::*DLSS::graphicsAPIIndependentInitializeFunctionPointer)(){&DLSS::safeFail};
bool (DLSS::*DLSS::graphicsAPIIndependentGetParametersFunctionPointer)(){&DLSS::safeFail};
bool (DLSS::*DLSS::graphicsAPIIndependentCreateFeatureFunctionPointer)(NVSDK_NGX_DLSS_Create_Params
){&DLSS::safeFail};
bool (DLSS::*DLSS::graphicsAPIIndependentSetImageResourcesFunctionPointer)(
  void *,
  UnityRenderingExtTextureFormat,
  void *,
  UnityRenderingExtTextureFormat,
  void *,
  UnityRenderingExtTextureFormat,
  void *,
  UnityRenderingExtTextureFormat
){&DLSS::safeFail};
bool (DLSS::*DLSS::graphicsAPIIndependentEvaluateFunctionPointer)(){&DLSS::safeFail};
bool (DLSS::*DLSS::graphicsAPIIndependentReleaseFeatureFunctionPointer)(){&DLSS::safeFail};
bool (DLSS::*DLSS::graphicsAPIIndependentShutdownFunctionPointer)(){&DLSS::safeFail};

#    ifdef ENABLE_VULKAN
bool DLSS::VulkanInitialize() {
    UnityVulkanInstance vulkanInstance = GraphicsAPI::get<Vulkan>()->getUnityInterface()->Instance();

    NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_Init(
      applicationInfo.id,
      applicationInfo.dataPath.c_str(),
      vulkanInstance.instance,
      vulkanInstance.physicalDevice,
      vulkanInstance.device
    );
    return isSupportedAfter(NVSDK_NGX_SUCCEED(result));
}

bool DLSS::VulkanGetParameters() {
    if (parameters != nullptr) return true;
    return NVSDK_NGX_SUCCEED(NVSDK_NGX_VULKAN_GetCapabilityParameters(&parameters));
}

bool DLSS::VulkanCreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams) {
    if (featureHandle != nullptr) return true;
    GraphicsAPI::get<Vulkan>()->getUnityInterface()->EnsureOutsideRenderPass();

    VkCommandBuffer  commandBuffer = GraphicsAPI::get<Vulkan>()->beginOneTimeSubmitRecording();
    NVSDK_NGX_Result result =
      NGX_VULKAN_CREATE_DLSS_EXT(commandBuffer, 1, 1, &featureHandle, parameters, &DLSSCreateParams);
    if (NVSDK_NGX_SUCCEED(result)) {
        GraphicsAPI::get<Vulkan>()->endOneTimeSubmitRecording();
        return featureHandle != nullptr;
    }
    GraphicsAPI::get<Vulkan>()->cancelOneTimeSubmitRecording();
    return false;
}

bool DLSS::VulkanSetImageResources(
  void                          *nativeDepthBuffer,
  UnityRenderingExtTextureFormat unityDepthFormat,
  void                          *nativeMotionVectors,
  UnityRenderingExtTextureFormat unityMotionVectorFormat,
  void                          *nativeInColor,
  UnityRenderingExtTextureFormat unityInColorFormat,
  void                          *nativeOutColor,
  UnityRenderingExtTextureFormat unityOutColorFormat
) {
    if (depth.vulkan != nullptr) {
        GraphicsAPI::get<Vulkan>()->destroyImageView(depth.vulkan->Resource.ImageViewInfo.ImageView);
        std::free(depth.vulkan);
    }
    if (motion.vulkan != nullptr) {
        GraphicsAPI::get<Vulkan>()->destroyImageView(motion.vulkan->Resource.ImageViewInfo.ImageView);
        std::free(motion.vulkan);
    }
    if (inColor.vulkan != nullptr) {
        GraphicsAPI::get<Vulkan>()->destroyImageView(inColor.vulkan->Resource.ImageViewInfo.ImageView);
        std::free(inColor.vulkan);
    }
    if (outColor.vulkan != nullptr) {
        GraphicsAPI::get<Vulkan>()->destroyImageView(outColor.vulkan->Resource.ImageViewInfo.ImageView);
        std::free(outColor.vulkan);
    }

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
          .Width  = settings.inputResolution.width,
          .Height = settings.inputResolution.height,
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
          .Width  = settings.inputResolution.width,
          .Height = settings.inputResolution.height,
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
          .Width  = settings.inputResolution.width,
          .Height = settings.inputResolution.height,
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
    return depthView != VK_NULL_HANDLE && motionVectorView != VK_NULL_HANDLE && inColorView != VK_NULL_HANDLE &&
      outColorView != VK_NULL_HANDLE;
}

bool DLSS::VulkanEvaluate() {
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
        .Width  = settings.inputResolution.width,
        .Height = settings.inputResolution.height,
      },
      .InMVScaleX = (float)settings.inputResolution.width,
      .InMVScaleY = (float)settings.inputResolution.height,
    };
    // clang-format on

    NVSDK_NGX_Result result =
      NGX_VULKAN_EVALUATE_DLSS_EXT(state.commandBuffer, featureHandle, parameters, &DLSSEvalParameters);

    return NVSDK_NGX_SUCCEED(result);
}

bool DLSS::VulkanReleaseFeature() {
    if (featureHandle == nullptr) return true;
    NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_ReleaseFeature(featureHandle);
    if (NVSDK_NGX_FAILED(result)) return false;
    featureHandle = nullptr;
    return true;
}

bool DLSS::VulkanDestroyParameters() {
    if (parameters == nullptr) return true;
    NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_DestroyParameters(parameters);
    if (NVSDK_NGX_FAILED(result)) return false;
    parameters = nullptr;
    return true;
}

bool DLSS::VulkanShutdown() {
    VulkanDestroyParameters();
    VulkanReleaseFeature();
    return NVSDK_NGX_SUCCEED(NVSDK_NGX_VULKAN_Shutdown1(nullptr));
}
#    endif

#    ifdef ENABLE_DX12
bool DLSS::DX12Initialize() {
    NVSDK_NGX_Result result = NVSDK_NGX_D3D12_Init(
      applicationInfo.id,
      applicationInfo.dataPath.c_str(),
      GraphicsAPI::get<DX12>()->getUnityInterface()->GetDevice()
    );
    return isSupportedAfter(NVSDK_NGX_SUCCEED(result));
}

bool DLSS::DX12GetParameters() {
    if (parameters != nullptr) return true;
    return NVSDK_NGX_SUCCEED(NVSDK_NGX_D3D12_GetCapabilityParameters(&parameters));
}

bool DLSS::DX12CreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams) {
    if (featureHandle != nullptr) return true;

    ID3D12GraphicsCommandList *commandList = GraphicsAPI::get<DX12>()->beginOneTimeSubmitRecording();
    NVSDK_NGX_Result           result =
      NGX_D3D12_CREATE_DLSS_EXT(commandList, 1U, 1U, &featureHandle, parameters, &DLSSCreateParams);
    if (NVSDK_NGX_SUCCEED(result)) {
        GraphicsAPI::get<DX12>()->endOneTimeSubmitRecording();
        return featureHandle != nullptr;
    }
    GraphicsAPI::get<DX12>()->cancelOneTimeSubmitRecording();
    return false;
}

bool DLSS::DX12SetDepthBuffer(
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
    return true;
}

bool DLSS::DX12Evaluate() {
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
      .InMVScaleX = (float)settings.inputResolution.width,
      .InMVScaleY = (float)settings.inputResolution.height,
    };
    // clang-format on

    return NVSDK_NGX_SUCCEED(
      NGX_D3D12_EVALUATE_DLSS_EXT(state.commandList, featureHandle, parameters, &DLSSEvalParameters)
    );
}

bool DLSS::DX12ReleaseFeature() {
    if (featureHandle == nullptr) return true;
    NVSDK_NGX_Result result = NVSDK_NGX_D3D12_ReleaseFeature(featureHandle);
    if (NVSDK_NGX_FAILED(result)) return false;
    featureHandle = nullptr;
    return true;
}

bool DLSS::DX12DestroyParameters() {
    if (parameters == nullptr) return true;
    NVSDK_NGX_Result result = NVSDK_NGX_D3D12_DestroyParameters(parameters);
    if (NVSDK_NGX_FAILED(result)) return false;
    parameters = nullptr;
    return true;
}

bool DLSS::DX12Shutdown() {
    DX12DestroyParameters();
    DX12ReleaseFeature();
    return NVSDK_NGX_SUCCEED(NVSDK_NGX_D3D12_Shutdown1(nullptr));
}
#    endif

#    ifdef ENABLE_DX11
bool DLSS::DX11Initialize() {
    NVSDK_NGX_Result result = NVSDK_NGX_D3D11_Init(
      applicationInfo.id,
      applicationInfo.dataPath.c_str(),
      GraphicsAPI::get<DX11>()->getUnityInterface()->GetDevice()
    );
    return isSupportedAfter(NVSDK_NGX_SUCCEED(result));
}

bool DLSS::DX11GetParameters() {
    if (parameters != nullptr) return true;
    return NVSDK_NGX_SUCCEED(NVSDK_NGX_D3D11_GetCapabilityParameters(&parameters));
}

bool DLSS::DX11CreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams) {
    if (featureHandle != nullptr) return true;
    ID3D11DeviceContext *context = GraphicsAPI::get<DX11>()->beginOneTimeSubmitRecording();
    NVSDK_NGX_Result result = NGX_D3D11_CREATE_DLSS_EXT(context, &featureHandle, parameters, &DLSSCreateParams);
    if (NVSDK_NGX_SUCCEED(result)) {
        GraphicsAPI::get<DX11>()->endOneTimeSubmitRecording();
        return featureHandle != nullptr;
    }
    GraphicsAPI::get<DX11>()->cancelOneTimeSubmitRecording();
    return false;
}

bool DLSS::DX11SetDepthBuffer(
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
    return true;
}

bool DLSS::DX11Evaluate() {
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
      .InMVScaleX = (float)settings.inputResolution.width,
      .InMVScaleY = (float)settings.inputResolution.height,
    };
    // clang-format on

    ID3D11DeviceContext *context = GraphicsAPI::get<DX11>()->beginOneTimeSubmitRecording();
    NVSDK_NGX_Result     result = NGX_D3D11_EVALUATE_DLSS_EXT(context, featureHandle, parameters, &DLSSEvalParams);
    if (NVSDK_NGX_SUCCEED(result)) {
        GraphicsAPI::get<DX11>()->endOneTimeSubmitRecording();
        return true;
    }
    GraphicsAPI::get<DX11>()->cancelOneTimeSubmitRecording();
    return false;
}

bool DLSS::DX11ReleaseFeature() {
    if (featureHandle == nullptr) return true;
    NVSDK_NGX_Result result = NVSDK_NGX_D3D11_ReleaseFeature(featureHandle);
    if (NVSDK_NGX_FAILED(result)) return false;
    featureHandle = nullptr;
    return true;
}

bool DLSS::DX11DestroyParameters() {
    if (parameters == nullptr) return true;
    NVSDK_NGX_Result result = NVSDK_NGX_D3D11_DestroyParameters(parameters);
    if (NVSDK_NGX_FAILED(result)) return false;
    parameters = nullptr;
    return true;
}

bool DLSS::DX11Shutdown() {
    DX11DestroyParameters();
    DX11ReleaseFeature();
    return NVSDK_NGX_SUCCEED(NVSDK_NGX_D3D11_Shutdown1(nullptr));
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
            graphicsAPIIndependentSetImageResourcesFunctionPointer = &DLSS::DX12SetDepthBuffer;
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
            graphicsAPIIndependentSetImageResourcesFunctionPointer = &DLSS::DX11SetDepthBuffer;
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

bool DLSS::isSupported() {
    return supported;
}

bool DLSS::isSupportedAfter(bool isSupported) {
    supported &= isSupported;
    available &= isSupported;
    active &= isSupported;
    return supported;
}

void DLSS::setSupported(bool isSupported) {
    supported = isSupported;
}

std::vector<std::string> DLSS::getRequiredVulkanInstanceExtensions() {
    uint32_t                 extensionCount{};
    std::vector<std::string> extensions{};
    VkExtensionProperties   *extensionProperties{};
    if (!isSupportedAfter(NVSDK_NGX_SUCCEED(NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(
          &applicationInfo.featureDiscoveryInfo,
          &extensionCount,
          &extensionProperties
        ))))
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
    if (!isSupportedAfter(NVSDK_NGX_SUCCEED(NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(
          instance,
          physicalDevice,
          &applicationInfo.featureDiscoveryInfo,
          &extensionCount,
          &extensionProperties
        ))))
        return {};
    extensions.reserve(extensionCount);
    for (uint32_t i{}; i < extensionCount; ++i) extensions.emplace_back(extensionProperties[i].extensionName);
    return extensions;
}

bool DLSS::isAvailableAfter(bool isAvailable) {
    available &= isAvailable;
    active &= isAvailable;
    return available;
}

bool DLSS::isAvailable() {
    return available;
}

void DLSS::setAvailable(bool isAvailable) {
    available = isAvailable;
}

Upscaler::Type DLSS::getType() {
    return Upscaler::DLSS;
}

std::string DLSS::getName() {
    return "NVIDIA DLSS";
}

bool DLSS::initialize() {
    if (!isSupported()) return false;

    // Upscaler_Initialize NGX SDK
    (this->*graphicsAPIIndependentInitializeFunctionPointer)();
    (this->*graphicsAPIIndependentGetParametersFunctionPointer)();
    // Check for DLSS support
    // Is driver up-to-date
    int              needsUpdatedDriver{};
    int              requiredMajorDriverVersion{};
    int              requiredMinorDriverVersion{};
    NVSDK_NGX_Result updateDriverResult =
      parameters->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsUpdatedDriver);
    Logger::log("Query DLSS graphics driver requirements", updateDriverResult);
    NVSDK_NGX_Result minMajorDriverVersionResult =
      parameters->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, &requiredMajorDriverVersion);
    Logger::log("Query DLSS minimum graphics driver major version", minMajorDriverVersionResult);
    NVSDK_NGX_Result minMinorDriverVersionResult =
      parameters->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, &requiredMinorDriverVersion);
    Logger::log("Query DLSS minimum graphics driver minor version", minMinorDriverVersionResult);
    if (!isSupportedAfter(
          NVSDK_NGX_SUCCEED(updateDriverResult) && NVSDK_NGX_SUCCEED(minMajorDriverVersionResult) &&
          NVSDK_NGX_SUCCEED(minMinorDriverVersionResult)
        ))
        return false;
    if (!isSupportedAfter(needsUpdatedDriver == 0)) {
        Logger::log(
          "DLSS initialization failed. Minimum driver requirement not met. Update to at least: " +
          std::to_string(requiredMajorDriverVersion) + "." + std::to_string(requiredMinorDriverVersion)
        );
        return false;
    }
    Logger::log(
      "Graphics driver version is greater than DLSS' required minimum version (" +
      std::to_string(requiredMajorDriverVersion) + "." + std::to_string(requiredMinorDriverVersion) + ")."
    );
    // Is DLSS available on this hardware and platform
    int              DLSSSupported{};
    NVSDK_NGX_Result result = parameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &DLSSSupported);
    Logger::log("Query DLSS feature availability", result);
    if (!isSupportedAfter(NVSDK_NGX_SUCCEED(result))) return false;
    if (!isSupportedAfter(DLSSSupported != 0)) {
        NVSDK_NGX_Result FeatureInitResult = NVSDK_NGX_Result_Fail;
        NVSDK_NGX_Parameter_GetI(
          parameters,
          NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult,
          reinterpret_cast<int *>(&FeatureInitResult)
        );
        std::stringstream stream;
        stream << "DLSSPlugin: DLSS is not available on this hardware or platform. FeatureInitResult = 0x"
               << std::setfill('0') << std::setw(sizeof(FeatureInitResult) * 2) << std::hex << FeatureInitResult
               << ", info: " << Logger::to_string(GetNGXResultAsString(FeatureInitResult));
        Logger::log(stream.str());
        return false;
    }
    // Is DLSS denied for this application
    result = parameters->Get(NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, &DLSSSupported);
    Logger::log("Query DLSS feature initialization", result);
    if (!isSupportedAfter(NVSDK_NGX_SUCCEED(result))) return false;
    // clean up
    if (!isSupportedAfter(DLSSSupported != 0)) {
        Logger::log("DLSS is denied for this application.");
        return false;
    }
    return isSupported();
}

bool DLSS::createFeature() {
    NVSDK_NGX_DLSS_Create_Params DLSSCreateParams{
      .Feature =
        {
                  .InWidth            = settings.inputResolution.width,
                  .InHeight           = settings.inputResolution.height,
                  .InTargetWidth      = settings.outputResolution.width,
                  .InTargetHeight     = settings.outputResolution.height,
                  .InPerfQualityValue = settings.getQuality<Upscaler::DLSS>(),
                  },
      .InFeatureCreateFlags = static_cast<int>(
        NVSDK_NGX_DLSS_Feature_Flags_MVLowRes | NVSDK_NGX_DLSS_Feature_Flags_MVJittered |
        (settings.sharpness > 0 ? NVSDK_NGX_DLSS_Feature_Flags_DoSharpening : 0U) |
        (settings.HDR ? NVSDK_NGX_DLSS_Feature_Flags_IsHDR : 0U) |
        (settings.autoExposure ? NVSDK_NGX_DLSS_Feature_Flags_AutoExposure : 0U)
      ),
      .InEnableOutputSubrects = false,
    };

    (this->*graphicsAPIIndependentReleaseFeatureFunctionPointer)();
    return (this->*graphicsAPIIndependentCreateFeatureFunctionPointer)(DLSSCreateParams);
}

bool DLSS::evaluate() {
    return (this->*graphicsAPIIndependentEvaluateFunctionPointer)();
}

bool DLSS::releaseFeature() {
    return (this->*graphicsAPIIndependentReleaseFeatureFunctionPointer)();
}

bool DLSS::shutdown() {
    return (this->*graphicsAPIIndependentShutdownFunctionPointer)();
}

Upscaler::Settings DLSS::getOptimalSettings(Upscaler::Settings::Resolution t_presentResolution, bool t_HDR) {
    settings.outputResolution = t_presentResolution;

    if (parameters == nullptr) return settings;

    Settings optimalSettings = settings;
    optimalSettings.HDR = t_HDR;

    NVSDK_NGX_PerfQuality_Value DLSSQuality = optimalSettings.getQuality<Upscaler::DLSS>();
    NVSDK_NGX_Result            result      = NGX_DLSS_GET_OPTIMAL_SETTINGS(
      parameters,
      optimalSettings.outputResolution.width,
      optimalSettings.outputResolution.height,
      DLSSQuality,
      &optimalSettings.inputResolution.width,
      &optimalSettings.inputResolution.height,
      &optimalSettings.dynamicMaximumInputResolution.width,
      &optimalSettings.dynamicMaximumInputResolution.height,
      &optimalSettings.dynamicMinimumInputResolution.width,
      &optimalSettings.dynamicMinimumInputResolution.height,
      &optimalSettings.sharpness
    );
    if (!isAvailableAfter(NVSDK_NGX_SUCCEED(result))) {
        optimalSettings.inputResolution               = optimalSettings.outputResolution;
        optimalSettings.dynamicMaximumInputResolution = optimalSettings.outputResolution;
        optimalSettings.dynamicMinimumInputResolution = optimalSettings.outputResolution;
        optimalSettings.sharpness                     = 0.F;
    }

    return optimalSettings;
}

bool DLSS::setImageResources(
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
#endif