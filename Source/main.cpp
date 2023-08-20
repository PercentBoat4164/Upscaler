#include "Unity/IUnityGraphics.h"
#include "Unity/IUnityGraphicsVulkan.h"
#include "Unity/IUnityInterface.h"
#include <iostream>
//#include "DLSS/nvsdk_ngx.h"
//#include "DLSS/nvsdk_ngx_defs.h"
//#include "DLSS/nvsdk_ngx_helpers.h"
//#include "DLSS/nvsdk_ngx_helpers_vk.h"
//#include "DLSS/nvsdk_ngx_params.h"
#include "DLSS/nvsdk_ngx_vk.h"

namespace Logger {
void (*Info)(const char *) = nullptr;
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

extern "C" UNITY_INTERFACE_EXPORT void
SetDebugCallback(void (*t_debugFunction)(const char *)) {
  Logger::Info = t_debugFunction;
  if (Unity::interceptedGraphicsInitialization) Logger::Info("DLSS Plugin loaded successfully.");
  else Logger::Info("DLSS Plugin failed to load.");
  NVSDK_NGX_VULKAN_Init(NVSDK_NGX_ENGINE_TYPE_CUSTOM, nullptr, nullptr, nullptr, nullptr);
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