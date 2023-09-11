#include "DLSS.hpp"

#include "GraphicsAPI/Vulkan.hpp"

#include <iomanip>

bool (DLSS::*DLSS::graphicsAPIIndependentInitializeFunctionPointer)(){nullptr};
bool (DLSS::*DLSS::graphicsAPIIndependentGetParametersFunctionPointer)(){nullptr};
bool (DLSS::*DLSS::graphicsAPIIndependentCreateFeatureFunctionPointer)(NVSDK_NGX_DLSS_Create_Params){nullptr};
bool (DLSS::*DLSS::graphicsAPIIndependentEvaluateFunctionPointer)(){nullptr};
bool (DLSS::*DLSS::graphicsAPIIndependentReleaseFeatureFunctionPointer)(){nullptr};
bool (DLSS::*DLSS::graphicsAPIIndependentShutdownFunctionPointer)(){nullptr};

bool DLSS::VulkanInitialize() {
    UnityVulkanInstance vulkanInstance = Vulkan::getVulkanInterface()->Instance();

    NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_Init(
      applicationInfo.id,
      applicationInfo.dataPath.c_str(),
      vulkanInstance.instance,
      vulkanInstance.physicalDevice,
      vulkanInstance.device,
      Vulkan::getVkGetInstanceProcAddr(),
      nullptr,
      &applicationInfo.featureCommonInfo,
      NVSDK_NGX_Version_API
    );
    return isSupportedAfter(NVSDK_NGX_SUCCEED(result));
}

bool DLSS::VulkanGetParameters() {
    if (parameters != nullptr) return true;
    return NVSDK_NGX_SUCCEED(NVSDK_NGX_VULKAN_GetCapabilityParameters(&parameters));
}

bool DLSS::VulkanCreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams) {
    if (featureHandle != nullptr) return true;
    Vulkan::getVulkanInterface()->EnsureOutsideRenderPass();

    Vulkan::Device   device        = Vulkan::getDevice(Vulkan::getVulkanInterface()->Instance().device);
    VkCommandBuffer  commandBuffer = device.beginOneTimeSubmitRecording();
    NVSDK_NGX_Result result =
      NGX_VULKAN_CREATE_DLSS_EXT(commandBuffer, 1, 1, &featureHandle, parameters, &DLSSCreateParams);
    device.endOneTimeSubmitRecording();
    return NVSDK_NGX_SUCCEED(result);
}

bool DLSS::VulkanEvaluate() {
    UnityVulkanRecordingState state{};
    Vulkan::getVulkanInterface()->EnsureInsideRenderPass();
    Vulkan::getVulkanInterface()->CommandRecordingState(&state, kUnityVulkanGraphicsQueueAccess_DontCare);

    return NVSDK_NGX_SUCCEED(
      NGX_VULKAN_EVALUATE_DLSS_EXT(state.commandBuffer, featureHandle, parameters, &evalParameters)
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
            graphicsAPIIndependentInitializeFunctionPointer     = &DLSS::SafeFail;
            graphicsAPIIndependentGetParametersFunctionPointer  = &DLSS::SafeFail;
            graphicsAPIIndependentCreateFeatureFunctionPointer  = &DLSS::SafeFail;
            graphicsAPIIndependentEvaluateFunctionPointer       = &DLSS::SafeFail;
            graphicsAPIIndependentReleaseFeatureFunctionPointer = &DLSS::SafeFail;
            graphicsAPIIndependentShutdownFunctionPointer       = &DLSS::SafeFail;
            break;
        }
        case GraphicsAPI::VULKAN: {
            graphicsAPIIndependentInitializeFunctionPointer     = &DLSS::VulkanInitialize;
            graphicsAPIIndependentGetParametersFunctionPointer  = &DLSS::VulkanGetParameters;
            graphicsAPIIndependentCreateFeatureFunctionPointer  = &DLSS::VulkanCreateFeature;
            graphicsAPIIndependentEvaluateFunctionPointer       = &DLSS::VulkanEvaluate;
            graphicsAPIIndependentReleaseFeatureFunctionPointer = &DLSS::VulkanReleaseFeature;
            graphicsAPIIndependentShutdownFunctionPointer       = &DLSS::VulkanShutdown;
            break;
        }
    }
}

Upscaler *DLSS::get() {
    static DLSS *dlss{new DLSS};
    return dlss;
}

bool DLSS::isSupported() {
    return supported;
}

bool DLSS::isSupportedAfter(bool isSupported) {
    supported &= isSupported;
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

void DLSS::setDepthBuffer(VkImage depthBuffer, VkImageView depthBufferView) {
    // clang-format off
    depthBufferResource = {
      .Resource = {
        .ImageViewInfo = {
          .ImageView        = depthBufferView,
          .Image            = depthBuffer,
          .SubresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
          },
          .Format = VK_FORMAT_D24_UNORM_S8_UINT,
          .Width  = settings.renderResolution.width,
          .Height = settings.renderResolution.height,
        },
      },
      .Type      = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW,
      .ReadWrite = true,
    };

    evalParameters = {
      .pInDepth                  = &depthBufferResource,
      .pInMotionVectors          = nullptr,
      .InJitterOffsetX           = 0.F,
      .InJitterOffsetY           = 0.F,
      .InRenderSubrectDimensions = {
        .Width  = settings.renderResolution.width,
        .Height = settings.renderResolution.height,
      },
    };
    // clang-format on
}

bool DLSS::isAvailableAfter(bool isAvailable) {
    available &= isAvailable;
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

    // Initialize NGX SDK
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
    Settings optimalSettings   = settings;

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
