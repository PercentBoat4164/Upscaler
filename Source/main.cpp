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
  case NVSDK_NGX_Result_Success: log(t_actionDescription + ": Succeeded"); break;
  case NVSDK_NGX_Result_FAIL_FeatureNotSupported: log(t_actionDescription + ": Failed | Feature not supported."); break;
  case NVSDK_NGX_Result_FAIL_PlatformError: log(t_actionDescription + ": Failed | Platform error."); break;
  case NVSDK_NGX_Result_FAIL_FeatureAlreadyExists: log(t_actionDescription + ": Failed | Feature already exists"); break;
  case NVSDK_NGX_Result_FAIL_FeatureNotFound: log(t_actionDescription + ": Failed | Feature not found."); break;
  case NVSDK_NGX_Result_FAIL_InvalidParameter: log(t_actionDescription + ": Failed | Invalid parameter."); break;
  case NVSDK_NGX_Result_FAIL_ScratchBufferTooSmall: log(t_actionDescription + ": Failed | Scratch buffer too small."); break;
  case NVSDK_NGX_Result_FAIL_NotInitialized: log(t_actionDescription + ": Failed | Not initialized."); break;
  case NVSDK_NGX_Result_FAIL_UnsupportedInputFormat: log(t_actionDescription + ": Failed | Unsupported input format."); break;
  case NVSDK_NGX_Result_FAIL_RWFlagMissing: log(t_actionDescription + ": Failed | RW flag missing."); break;
  case NVSDK_NGX_Result_FAIL_MissingInput: log(t_actionDescription + ": Failed | Missing input."); break;
  case NVSDK_NGX_Result_FAIL_UnableToInitializeFeature: log(t_actionDescription + ": Failed | Unable to initialize feature."); break;
  case NVSDK_NGX_Result_FAIL_OutOfDate: log(t_actionDescription + ": Failed | Out of date."); break;
  case NVSDK_NGX_Result_FAIL_OutOfGPUMemory: log(t_actionDescription + ": Failed | Out of GPU memory."); break;
  case NVSDK_NGX_Result_FAIL_UnsupportedFormat: log(t_actionDescription + ": Failed | Unsupported format."); break;
  case NVSDK_NGX_Result_FAIL_UnableToWriteToAppDataPath: log(t_actionDescription + ": Failed | Unable to write to app data path."); break;
  case NVSDK_NGX_Result_FAIL_UnsupportedParameter: log(t_actionDescription + ": Failed | Unsupported parameter."); break;
  case NVSDK_NGX_Result_FAIL_Denied: log(t_actionDescription + ": Failed | Denied."); break;
  case NVSDK_NGX_Result_FAIL_NotImplemented: log(t_actionDescription + ": Failed | Not implemented."); break;

  default:
    log(t_actionDescription + ": Failed | Unknown Error");
  }
}
} // namespace Logger

namespace Unity {
IUnityGraphicsVulkan *vulkanGraphicsInterface;
} // namespace Unity

extern "C" UNITY_INTERFACE_EXPORT void InitializeNGX(const wchar_t *t_appDataPath) {
//  UnityVulkanInstance vulkan = Unity::vulkanGraphicsInterface->Instance();
  VkInstance instance = Unity::vulkanGraphicsInterface->Instance().instance;
  VkPhysicalDevice physicalDevice = Unity::vulkanGraphicsInterface->Instance().physicalDevice;
  VkDevice device = Unity::vulkanGraphicsInterface->Instance().device;
  NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_Init(
      NVSDK_NGX_ENGINE_TYPE_CUSTOM, t_appDataPath, instance, physicalDevice, device);
  Logger::log("Initialize NGX SDK", result);
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
  Unity::vulkanGraphicsInterface = t_unityInterfaces->Get<IUnityGraphicsVulkan>();
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload() {

}