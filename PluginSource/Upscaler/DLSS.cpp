#include "DLSS.hpp"

#include "GraphicsAPI/Vulkan.hpp"

#include <iomanip>

bool (DLSS::*DLSS::graphicsAPIIndependentInitializeFunctionPointer)(){&DLSS::safeFail};
bool (DLSS::*DLSS::graphicsAPIIndependentGetParametersFunctionPointer)(){&DLSS::safeFail};
bool (DLSS::*DLSS::graphicsAPIIndependentCreateFeatureFunctionPointer)(NVSDK_NGX_DLSS_Create_Params
){&DLSS::safeFail};
bool (DLSS::*DLSS::graphicsAPIIndependentSetImageResourcesFunctionPointer)(
  void *,
  UnityRenderingExtTextureFormat,
  void *,
  UnityRenderingExtTextureFormat
){&DLSS::safeFail};
bool (DLSS::*DLSS::graphicsAPIIndependentEvaluateFunctionPointer)(){&DLSS::safeFail};
bool (DLSS::*DLSS::graphicsAPIIndependentReleaseFeatureFunctionPointer)(){&DLSS::safeFail};
bool (DLSS::*DLSS::graphicsAPIIndependentShutdownFunctionPointer)(){&DLSS::safeFail};

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
    GraphicsAPI::get<Vulkan>()->endOneTimeSubmitRecording();
    return NVSDK_NGX_SUCCEED(result);
}

bool DLSS::VulkanSetImageResources(
  void                          *depthBuffer,
  UnityRenderingExtTextureFormat unityDepthFormat,
  void                          *motionVectorBuffer,
  UnityRenderingExtTextureFormat unityMotionVectorFormat
) {
    GraphicsAPI::get<Vulkan>()->destroyImageView(vulkanDepthBufferResource.Resource.ImageViewInfo.ImageView);
    GraphicsAPI::get<Vulkan>()->destroyImageView(vulkanMotionVectorResource.Resource.ImageViewInfo.ImageView);
    VkImage     depthImage{*reinterpret_cast<VkImage *>(depthBuffer)};
    VkImage     motionVectorImage{*reinterpret_cast<VkImage *>(motionVectorBuffer)};
    VkFormat    depthFormat        = Vulkan::getFormat(unityDepthFormat);
    VkFormat    motionVectorFormat = Vulkan::getFormat(unityMotionVectorFormat);
    VkImageView depthView{GraphicsAPI::get<Vulkan>()->get2DImageView(depthImage, depthFormat)};
    VkImageView motionVectorView{
      GraphicsAPI::get<Vulkan>()->get2DImageView(motionVectorImage, motionVectorFormat)};

    // clang-format off
    vulkanDepthBufferResource = {
      .Resource = {
        .ImageViewInfo = {
          .ImageView        = depthView,
          .Image            = depthImage,
          .SubresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
          },
          .Format = depthFormat,
          .Width  = settings.renderResolution.width,
          .Height = settings.renderResolution.height,
        },
      },
      .Type      = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW,
      .ReadWrite = true,
    };

    vulkanMotionVectorResource = {
      .Resource = {
        .ImageViewInfo = {
          .ImageView        = motionVectorView,
          .Image            = motionVectorImage,
          .SubresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
          },
          .Format = motionVectorFormat,
          .Width  = settings.renderResolution.width,
          .Height = settings.renderResolution.height,
        },
      },
      .Type      = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW,
      .ReadWrite = true,
    };
    // clang-format on
    return depthView != VK_NULL_HANDLE && motionVectorView != VK_NULL_HANDLE;
}

bool DLSS::VulkanEvaluate() {
    UnityVulkanRecordingState state{};
    GraphicsAPI::get<Vulkan>()->getUnityInterface()->EnsureInsideRenderPass();
    GraphicsAPI::get<Vulkan>()->getUnityInterface()->CommandRecordingState(
      &state,
      kUnityVulkanGraphicsQueueAccess_DontCare
    );

    // clang-format off
    NVSDK_NGX_VK_DLSS_Eval_Params vulkanEvalParameters = {
      .pInDepth                  = &vulkanDepthBufferResource,
      .pInMotionVectors          = &vulkanMotionVectorResource,
      .InJitterOffsetX           = thisFrameJitterValues[0],
      .InJitterOffsetY           = thisFrameJitterValues[1],
      .InRenderSubrectDimensions = {
        .Width  = settings.renderResolution.width,
        .Height = settings.renderResolution.height,
      },
    };
    // clang-format on

    return NVSDK_NGX_SUCCEED(
      NGX_VULKAN_EVALUATE_DLSS_EXT(state.commandBuffer, featureHandle, parameters, &vulkanEvalParameters)
    );
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
                  .InWidth            = settings.renderResolution.width,
                  .InHeight           = settings.renderResolution.height,
                  .InTargetWidth      = settings.presentResolution.width,
                  .InTargetHeight     = settings.presentResolution.height,
                  .InPerfQualityValue = settings.getQuality<Upscaler::DLSS>(),
                  },
      .InFeatureCreateFlags = static_cast<int>(
        (settings.lowResolutionMotionVectors ? NVSDK_NGX_DLSS_Feature_Flags_MVLowRes : 0U) |
        (settings.jitteredMotionVectors ? NVSDK_NGX_DLSS_Feature_Flags_MVJittered : 0U) |
        (settings.HDR ? NVSDK_NGX_DLSS_Feature_Flags_IsHDR : 0U) |
        (settings.invertedDepth ? NVSDK_NGX_DLSS_Feature_Flags_DepthInverted : 0U) |
        (settings.sharpness > 0 ? NVSDK_NGX_DLSS_Feature_Flags_DoSharpening : 0U) |
        (settings.autoExposure ? NVSDK_NGX_DLSS_Feature_Flags_AutoExposure : 0U)
      ),
      .InEnableOutputSubrects = false,
    };

    NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA, 1);
    NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Quality, 1);
    NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Balanced, 1);
    NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Performance, 1);
    NVSDK_NGX_Parameter_SetUI(parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_UltraPerformance, 1);

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

Upscaler::Settings DLSS::getOptimalSettings(Upscaler::Settings::Resolution t_presentResolution) {
    settings.presentResolution = t_presentResolution;

    if (parameters == nullptr) return settings;

    Settings optimalSettings = settings;

    NVSDK_NGX_Result result = NGX_DLSS_GET_OPTIMAL_SETTINGS(
      parameters,
      optimalSettings.presentResolution.width,
      optimalSettings.presentResolution.height,
      optimalSettings.getQuality<Upscaler::DLSS>(),
      &optimalSettings.renderResolution.width,
      &optimalSettings.renderResolution.height,
      &optimalSettings.dynamicMaximumRenderResolution.width,
      &optimalSettings.dynamicMaximumRenderResolution.height,
      &optimalSettings.dynamicMinimumRenderResolution.width,
      &optimalSettings.dynamicMinimumRenderResolution.height,
      &optimalSettings.sharpness
    );
    if (!isAvailableAfter(NVSDK_NGX_SUCCEED(result))) {
        optimalSettings.renderResolution               = optimalSettings.presentResolution;
        optimalSettings.dynamicMaximumRenderResolution = optimalSettings.presentResolution;
        optimalSettings.dynamicMinimumRenderResolution = optimalSettings.presentResolution;
        optimalSettings.sharpness                      = 0.F;
    }

    return optimalSettings;
}

bool DLSS::setImageResources(
  void                          *nativeDepthBuffer,
  UnityRenderingExtTextureFormat unityDepthFormat,
  void                          *nativeMotionVectors,
  UnityRenderingExtTextureFormat unityMotionVectorFormat
) {
    return (this->*graphicsAPIIndependentSetImageResourcesFunctionPointer)(nativeDepthBuffer, unityDepthFormat, nativeMotionVectors, unityMotionVectorFormat);
}
