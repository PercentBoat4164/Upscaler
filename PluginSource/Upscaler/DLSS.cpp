#include "DLSS.hpp"

#include <iomanip>

Upscaler::DLSS::Application Upscaler::DLSS::applicationInfo{};
bool Upscaler::DLSS::supported{true};

Upscaler::Upscaler *Upscaler::DLSS::get() {
    static DLSS *dlss{new DLSS};
    return dlss;
}

bool Upscaler::DLSS::isSupported() {
    return supported;
}

bool Upscaler::DLSS::setIsSupported(bool isSupported) {
    supported &= isSupported;
    return supported;
}

bool Upscaler::DLSS::staticSetIsSupported(bool isSupported) {
    supported &= isSupported;
    return supported;
}

ExtensionGroup Upscaler::DLSS::getRequiredVulkanInstanceExtensions() {
    uint32_t                            extensionCount{};
    ExtensionGroup extensions{};
    VkExtensionProperties   *extensionProperties{};
    if (!staticSetIsSupported(NVSDK_NGX_SUCCEED(NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(
      &applicationInfo.featureDiscoveryInfo,
      &extensionCount,
      &extensionProperties
    ))))
        return {};
    extensions.reserve(extensionCount);
    for (uint32_t i{}; i < extensionCount; ++i)
        extensions.emplace_back(extensionProperties[i].extensionName);
    return extensions;
}

ExtensionGroup
Upscaler::DLSS::getRequiredVulkanDeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice) {
    uint32_t                 extensionCount{};
    ExtensionGroup extensions{};
    VkExtensionProperties   *extensionProperties{};
    if (!staticSetIsSupported(NVSDK_NGX_SUCCEED(NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(
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

void Upscaler::DLSS::initialize() {
    if (!Plugin::DLSSSupported) return;

    UnityVulkanInstance vulkanInstance = GraphicsAPI::Vulkan::getVulkanInterface()->Instance();

    Plugin::prepareForOneTimeSubmits();

    // Initialize NGX SDK
    NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_Init(
      applicationInfo.id,
      applicationInfo.dataPath.c_str(),
      vulkanInstance.instance,
      vulkanInstance.physicalDevice,
      vulkanInstance.device,
      GraphicsAPI::Vulkan::getVkGetInstanceProcAddr(),
      nullptr,
      &applicationInfo.featureCommonInfo,
      NVSDK_NGX_Version_API
    );
    Logger::log("Initialize NGX SDK", result);
    staticSetIsSupported(NVSDK_NGX_SUCCEED(result));

    // Ensure that the device that Unity selected supports DLSS
    // Set and obtain parameters
    result = NVSDK_NGX_VULKAN_GetCapabilityParameters(&Plugin::parameters);
    Logger::log("Get NGX Vulkan capability parameters", result);
    if (!staticSetIsSupported(NVSDK_NGX_SUCCEED(result))) return;
    // Check for DLSS support
    // Is driver up-to-date
    int              needsUpdatedDriver{};
    int              requiredMajorDriverVersion{};
    int              requiredMinorDriverVersion{};
    NVSDK_NGX_Result updateDriverResult =
      Plugin::parameters->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsUpdatedDriver);
    Logger::log("Query DLSS graphics driver requirements", updateDriverResult);
    NVSDK_NGX_Result minMajorDriverVersionResult = Plugin::parameters->Get(
      NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor,
      &requiredMajorDriverVersion
    );
    Logger::log("Query DLSS minimum graphics driver major version", minMajorDriverVersionResult);
    NVSDK_NGX_Result minMinorDriverVersionResult = Plugin::parameters->Get(
      NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor,
      &requiredMinorDriverVersion
    );
    Logger::log("Query DLSS minimum graphics driver minor version", minMinorDriverVersionResult);
    if (!staticSetIsSupported(NVSDK_NGX_SUCCEED(updateDriverResult) && NVSDK_NGX_SUCCEED(minMajorDriverVersionResult) && NVSDK_NGX_SUCCEED(minMinorDriverVersionResult)))
        return;
    if (!staticSetIsSupported(needsUpdatedDriver == 0)) {
        Logger::log(
          "DLSS initialization failed. Minimum driver requirement not met. Update to at least: " +
          std::to_string(requiredMajorDriverVersion) + "." + std::to_string(requiredMinorDriverVersion)
        );
        return;
    }
    Logger::log(
      "Graphics driver version is greater than DLSS' required minimum version (" +
      std::to_string(requiredMajorDriverVersion) + "." + std::to_string(requiredMinorDriverVersion) + ")."
    );
    // Is DLSS available on this hardware and platform
    int DLSSSupported{};
    result = Plugin::parameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &DLSSSupported);
    Logger::log("Query DLSS feature availability", result);
    if (!staticSetIsSupported(NVSDK_NGX_SUCCEED(result))) return;
    if (!staticSetIsSupported(DLSSSupported != 0)) {
        NVSDK_NGX_Result FeatureInitResult = NVSDK_NGX_Result_Fail;
        NVSDK_NGX_Parameter_GetI(
          Plugin::parameters,
          NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult,
          reinterpret_cast<int *>(&FeatureInitResult)
        );
        std::stringstream stream;
        stream << "DLSSPlugin: DLSS is not available on this hardware or platform. FeatureInitResult = 0x"
               << std::setfill('0') << std::setw(sizeof(FeatureInitResult) * 2) << std::hex << FeatureInitResult
               << ", info: " << Logger::to_string(GetNGXResultAsString(FeatureInitResult));
        Logger::log(stream.str());
        return;
    }
    // Is DLSS denied for this application
    result = Plugin::parameters->Get(NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, &DLSSSupported);
    Logger::log("Query DLSS feature initialization", result);
    if (!staticSetIsSupported(NVSDK_NGX_SUCCEED(result))) return;
    // clean up
    if (!staticSetIsSupported(DLSSSupported != 0)) {
        Logger::log("DLSS is denied for this application.");
        return;
    }
}
