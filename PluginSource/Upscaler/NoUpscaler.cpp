#include "NoUpscaler.hpp"

NoUpscaler *NoUpscaler::get() {
    NoUpscaler *noUpscaler{new NoUpscaler};
    noUpscaler->setStatus(NO_UPSCALER_SET, "'" + noUpscaler->getName() + "' is selected");
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

Upscaler::Status NoUpscaler::initialize() {
    return NO_UPSCALER_SET;
}

Upscaler::Status NoUpscaler::createFeature() {
    return NO_UPSCALER_SET;
}

Upscaler::Status NoUpscaler::setDepthBuffer(void *pVoid, UnityRenderingExtTextureFormat format) {
  return NO_UPSCALER_SET;
}

Upscaler::Status NoUpscaler::setInputColor(void *pVoid, UnityRenderingExtTextureFormat format) {
  return NO_UPSCALER_SET;
}

Upscaler::Status NoUpscaler::setMotionVectors(void *pVoid, UnityRenderingExtTextureFormat format) {
  return NO_UPSCALER_SET;
}

Upscaler::Status NoUpscaler::setOutputColor(void *pVoid, UnityRenderingExtTextureFormat format) {
  return NO_UPSCALER_SET;
}

Upscaler::Status NoUpscaler::evaluate() {
    return NO_UPSCALER_SET;
}

Upscaler::Status NoUpscaler::releaseFeature() {
    return NO_UPSCALER_SET;
}

Upscaler::Status NoUpscaler::shutdown() {
    Upscaler::shutdown();
    return NO_UPSCALER_SET;
}

void NoUpscaler::setFunctionPointers(GraphicsAPI::Type graphicsAPI) {
}
