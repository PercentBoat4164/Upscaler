#include "Unity/IUnityGraphics.h"
#include "Unity/IUnityGraphicsVulkan.h"
#include "Unity/IUnityInterface.h"

namespace Logger {
void (*Info)(const char *) = nullptr;
}
namespace Vulkan {
VkResult (*createInstance)(const VkInstanceCreateInfo *pCreateInfo,
                           const VkAllocationCallbacks *pAllocator,
                           VkInstance *) = nullptr;
}

extern "C" UNITY_INTERFACE_EXPORT void
SetDebugCallback(void (*t_debugFunction)(const char *)) {
  Logger::Info = t_debugFunction;
  if (Vulkan::createInstance) Logger::Info("DLSS Plugin online.");
  else Logger::Info("DLSS Plugin offline.");
}

extern "C" UNITY_INTERFACE_EXPORT PFN_vkGetInstanceProcAddr
initializeNGX(PFN_vkGetInstanceProcAddr t_instance, void *user_data) {
  Vulkan::createInstance = (typeof(Vulkan::createInstance))t_instance(nullptr, "vkCreateInstance");
  return nullptr;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
UnityPluginLoad(IUnityInterfaces *t_unityInterfaces) {
  auto *graphics = t_unityInterfaces->Get<IUnityGraphicsVulkan>();
  graphics->InterceptInitialization(initializeNGX, nullptr);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload() {

}