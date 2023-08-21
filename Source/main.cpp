#include "Unity/IUnityGraphics.h"
#include "Unity/IUnityGraphicsVulkan.h"
#include "Unity/IUnityInterface.h"

#include "DLSS/nvsdk_ngx_vk.h"

#include <iostream>

namespace Logger {
void (*Info)(const char *) = nullptr;

void log(std::string t_message) {
  Info(t_message.c_str());
}

void log(std::string t_actionDescription, NVSDK_NGX_Result t_result) {
  switch (t_result) {
      case NVSDK_NGX_Result_Success:
        log(t_actionDescription + ": Success"); break;
      default:
        log(t_actionDescription + ": Fail");
    }
}
}

namespace Vulkan {
VkResult (*createInstance)(const VkInstanceCreateInfo *pCreateInfo,
                           const VkAllocationCallbacks *pAllocator,
                           VkInstance *) = nullptr;
}

namespace Unity {
bool interceptedGraphicsInitialization;
IUnityGraphicsVulkan* vulkanGraphicsInterface;
}

extern "C" UNITY_INTERFACE_EXPORT void InitializeNGX() {
  NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_Init(NVSDK_NGX_ENGINE_TYPE_CUSTOM, nullptr, nullptr, nullptr, nullptr);
  Logger::log("Initialize NGX SDK", result);
}

extern "C" UNITY_INTERFACE_EXPORT void
SetDebugCallback(void (*t_debugFunction)(const char *)) {
  Logger::Info = t_debugFunction;
  if (Unity::interceptedGraphicsInitialization) Logger::log("DLSS Plugin loaded successfully.");
  else Logger::log("DLSS Plugin failed to load.");
}

extern "C" UNITY_INTERFACE_EXPORT PFN_vkGetInstanceProcAddr
interceptInitialization(PFN_vkGetInstanceProcAddr t_instance, void *user_data) {
  Vulkan::createInstance = (typeof(Vulkan::createInstance))t_instance(nullptr, "vkCreateInstance");
  return t_instance;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
UnityPluginLoad(IUnityInterfaces *t_unityInterfaces) {
  Unity::vulkanGraphicsInterface = t_unityInterfaces->Get<IUnityGraphicsVulkan>();
  Unity::interceptedGraphicsInitialization = Unity::vulkanGraphicsInterface->InterceptInitialization(interceptInitialization, nullptr);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload() {

}