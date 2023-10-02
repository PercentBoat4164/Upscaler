#include "NoUpscaler.hpp"

NoUpscaler *NoUpscaler::get() {
    NoUpscaler *noUpscaler{new NoUpscaler};
    noUpscaler->setError(ERROR_DUMMY_UPSCALER);
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

Upscaler::ErrorReason NoUpscaler::initialize() {
    return ERROR_DUMMY_UPSCALER;
}

Upscaler::ErrorReason NoUpscaler::createFeature() {
    return ERROR_DUMMY_UPSCALER;
}

Upscaler::ErrorReason NoUpscaler::setImageResources(
  void                          *nativeDepthBuffer,
  UnityRenderingExtTextureFormat unityDepthFormat,
  void                          *nativeMotionVectors,
  UnityRenderingExtTextureFormat unityMotionVectorFormat,
  void                          *nativeInColor,
  UnityRenderingExtTextureFormat unityInColorFormat,
  void                          *nativeOutColor,
  UnityRenderingExtTextureFormat unityOutColorFormat
) {
    return ERROR_DUMMY_UPSCALER;
}

Upscaler::ErrorReason NoUpscaler::evaluate() {
    return ERROR_DUMMY_UPSCALER;
}

Upscaler::ErrorReason NoUpscaler::releaseFeature() {
    return ERROR_DUMMY_UPSCALER;
}

Upscaler::ErrorReason NoUpscaler::shutdown() {
    Upscaler::shutdown();
    return ERROR_DUMMY_UPSCALER;
}

void NoUpscaler::setFunctionPointers(GraphicsAPI::Type graphicsAPI) {
}
