#include "Unity/IUnityGraphics.h"
#include "Unity/IUnityGraphicsVulkan.h"
#include "Unity/IUnityInterface.h"

#include "DLSS/nvsdk_ngx_vk.h"

#include <iostream>

namespace Logger {
void (*Info)(const char *) = nullptr;

void log(std::string t_message) { Info(t_message.c_str()); }

void log(std::string t_actionDescription, NVSDK_NGX_Result t_result) {
  switch (t_result) {
  case NVSDK_NGX_Result_Success:
    log(t_actionDescription + ": Succeeded");
    break;
  case NVSDK_NGX_Result_FAIL_FeatureNotSupported:
    log(t_actionDescription + ": Failed | Feature not supported.");
    break;
  case NVSDK_NGX_Result_FAIL_PlatformError:
    log(t_actionDescription + ": Failed | Platform error.");
    break;
  case NVSDK_NGX_Result_FAIL_FeatureAlreadyExists:
    log(t_actionDescription + ": Failed | Feature already exists");
    break;
  case NVSDK_NGX_Result_FAIL_FeatureNotFound:
    log(t_actionDescription + ": Failed | Feature not found.");
    break;
  case NVSDK_NGX_Result_FAIL_InvalidParameter:
    log(t_actionDescription + ": Failed | Invalid parameter.");
    break;
  case NVSDK_NGX_Result_FAIL_ScratchBufferTooSmall:
    log(t_actionDescription + ": Failed | Scratch buffer too small.");
    break;
  case NVSDK_NGX_Result_FAIL_NotInitialized:
    log(t_actionDescription + ": Failed | Not initialized.");
    break;
  case NVSDK_NGX_Result_FAIL_UnsupportedInputFormat:
    log(t_actionDescription + ": Failed | Unsupported input format.");
    break;
  case NVSDK_NGX_Result_FAIL_RWFlagMissing:
    log(t_actionDescription + ": Failed | RW flag missing.");
    break;
  case NVSDK_NGX_Result_FAIL_MissingInput:
    log(t_actionDescription + ": Failed | Missing input.");
    break;
  case NVSDK_NGX_Result_FAIL_UnableToInitializeFeature:
    log(t_actionDescription + ": Failed | Unable to initialize feature.");
    break;
  case NVSDK_NGX_Result_FAIL_OutOfDate:
    log(t_actionDescription + ": Failed | Out of date.");
    break;
  case NVSDK_NGX_Result_FAIL_OutOfGPUMemory:
    log(t_actionDescription + ": Failed | Out of GPU memory.");
    break;
  case NVSDK_NGX_Result_FAIL_UnsupportedFormat:
    log(t_actionDescription + ": Failed | Unsupported format.");
    break;
  case NVSDK_NGX_Result_FAIL_UnableToWriteToAppDataPath:
    log(t_actionDescription + ": Failed | Unable to write to app data path.");
    break;
  case NVSDK_NGX_Result_FAIL_UnsupportedParameter:
    log(t_actionDescription + ": Failed | Unsupported parameter.");
    break;
  case NVSDK_NGX_Result_FAIL_Denied:
    log(t_actionDescription + ": Failed | Denied.");
    break;
  case NVSDK_NGX_Result_FAIL_NotImplemented:
    log(t_actionDescription + ": Failed | Not implemented.");
    break;

  default:
    log(t_actionDescription + ": Failed | Unknown Error");
  }
}

void log(const char *t_message, NVSDK_NGX_Logging_Level t_loggingLevel,
         NVSDK_NGX_Feature t_sourceComponent) {
  log(t_message);
}
} // namespace Logger

namespace ApplicationInfo {
uint64_t id;
std::wstring dataPath;
NVSDK_NGX_Application_Identifier ngxIdentifier;
} // namespace ApplicationInfo

namespace Unity {
IUnityGraphicsVulkan *vulkanGraphicsInterface;
} // namespace Unity

extern "C" UNITY_INTERFACE_EXPORT void
IdentifyApplication(const wchar_t *t_appDataPath, uint64_t t_id) {
  ApplicationInfo::id = t_id;  // Must be any non-zero number.
  ApplicationInfo::dataPath = t_appDataPath;
  Logger::log(std::to_string(t_id));
  ApplicationInfo::ngxIdentifier = {
      .IdentifierType = NVSDK_NGX_Application_Identifier_Type_Application_Id,
      .v = {
          .ApplicationId = ApplicationInfo::id,
      }
  };
}

extern "C" UNITY_INTERFACE_EXPORT bool InitializeDLSS() {
  UnityVulkanInstance vulkan = Unity::vulkanGraphicsInterface->Instance();

  // Initialize NGX SDK
  NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_Init(
      ApplicationInfo::id, ApplicationInfo::dataPath.c_str(),
      vulkan.instance, vulkan.physicalDevice, vulkan.device);
  Logger::log("Initialize NGX SDK", result);

  // Set up the Super Sampling feature
  NVSDK_NGX_FeatureCommonInfo featureCommonInfo{
      .PathListInfo = {
          .Path = nullptr,
          .Length = 0,
      },
      .InternalData = nullptr,
      .LoggingInfo = {
          .LoggingCallback = Logger::log,
          .MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_VERBOSE,
          .DisableOtherLoggingSinks = false,
      }
  };
  NVSDK_NGX_FeatureDiscoveryInfo featureDiscoveryInfo{
      .SDKVersion = NVSDK_NGX_Version_API,
      .FeatureID = NVSDK_NGX_Feature_SuperSampling,
      .Identifier = ApplicationInfo::ngxIdentifier,
      .ApplicationDataPath = ApplicationInfo::dataPath.c_str(),
      .FeatureInfo = &featureCommonInfo,
  };
  NVSDK_NGX_FeatureRequirement featureRequirement;
  result = NVSDK_NGX_VULKAN_GetFeatureRequirements(
      vulkan.instance, vulkan.physicalDevice, &featureDiscoveryInfo,
      &featureRequirement);
  Logger::log("Get DLSS feature requirements", result);

  if (!NVSDK_NGX_SUCCEED(result) || featureRequirement.FeatureSupported != NVSDK_NGX_FeatureSupportResult_Supported) return false;

  // Get required vulkan extensions @todo Make this happen before Unity has fully initialized its graphics engine.
  uint32_t deviceExtensionCount;
  VkExtensionProperties *deviceExtensions;
  NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(vulkan.instance, vulkan.physicalDevice, &featureDiscoveryInfo, &deviceExtensionCount, &deviceExtensions);

  uint32_t instanceExtensionCount;
  VkExtensionProperties *instanceExtensions;
  NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(&featureDiscoveryInfo, &instanceExtensionCount, &instanceExtensions);

  // DLSS is available.
  return true;
}

extern "C" UNITY_INTERFACE_EXPORT void
SetDebugCallback(void (*t_debugFunction)(const char *)) {
  Logger::Info = t_debugFunction;
  if (Unity::vulkanGraphicsInterface != nullptr)
    Logger::log("DLSS Plugin loaded successfully.");
  else
    Logger::log("DLSS Plugin failed to load.");
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
UnityPluginLoad(IUnityInterfaces *t_unityInterfaces) {
  Unity::vulkanGraphicsInterface =
      t_unityInterfaces->Get<IUnityGraphicsVulkan>();
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload() {
  NVSDK_NGX_VULKAN_Shutdown1(Unity::vulkanGraphicsInterface->Instance().device);
}