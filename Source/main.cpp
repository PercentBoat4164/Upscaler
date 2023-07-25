#include "Unity/IUnityGraphics.h"
#include "Unity/IUnityInterface.h"

namespace Logger{
  void(*Info)(const char *);
}

extern "C" UNITY_INTERFACE_EXPORT void SetDebugCallback(void(*t_debugFunction)(const char *)) {
  Logger::Info = t_debugFunction;
}

extern "C" UNITY_INTERFACE_EXPORT void initializeNGX() {

}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces *t_unityInterfaces) {
  auto* graphics = t_unityInterfaces->Get<IUnityGraphics>();
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload() {
  
}