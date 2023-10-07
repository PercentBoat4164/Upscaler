#include "NoUpscaler.hpp"

NoUpscaler *NoUpscaler::get() {
    NoUpscaler *noUpscaler{new NoUpscaler};
    noUpscaler->setError(NO_UPSCALER_SET, "'" + noUpscaler->getName() + "' is selected");
    return noUpscaler;
}

Upscaler::Type NoUpscaler::getType() {
    return NONE;
}

std::string NoUpscaler::getName() {
    return "Dummy Upscaler";
}

std::vector<std::string> NoUpscaler::getRequiredVulkanInstanceExtensions() {
    return {};
}

std::vector<std::string>
NoUpscaler::getRequiredVulkanDeviceExtensions(VkInstance instance, VkPhysicalDevice device) {
    return {};
}

Upscaler::Settings NoUpscaler::getOptimalSettings(
  Upscaler::Settings::Resolution /* unused */,
  Settings::Quality /* unused */,
  bool /* unused */
) {
    return settings;
}

Upscaler::UpscalerStatus NoUpscaler::initialize() {
    return NO_UPSCALER_SET;
}

Upscaler::UpscalerStatus NoUpscaler::createFeature() {
    return NO_UPSCALER_SET;
}

Upscaler::UpscalerStatus NoUpscaler::setImageResources(
  void                          *nativeDepthBuffer,
  UnityRenderingExtTextureFormat unityDepthFormat,
  void                          *nativeMotionVectors,
  UnityRenderingExtTextureFormat unityMotionVectorFormat,
  void                          *nativeInColor,
  UnityRenderingExtTextureFormat unityInColorFormat,
  void                          *nativeOutColor,
  UnityRenderingExtTextureFormat unityOutColorFormat
) {
    return NO_UPSCALER_SET;
}

Upscaler::UpscalerStatus NoUpscaler::evaluate() {
    return NO_UPSCALER_SET;
}

Upscaler::UpscalerStatus NoUpscaler::releaseFeature() {
    return NO_UPSCALER_SET;
}

Upscaler::UpscalerStatus NoUpscaler::shutdown() {
    Upscaler::shutdown();
    return NO_UPSCALER_SET;
}

void NoUpscaler::setFunctionPointers(GraphicsAPI::Type graphicsAPI) {
}
