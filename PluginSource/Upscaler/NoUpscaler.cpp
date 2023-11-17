#include "NoUpscaler.hpp"

void NoUpscaler::setFunctionPointers(GraphicsAPI::Type /* unused */) {
}

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
NoUpscaler::getRequiredVulkanDeviceExtensions(VkInstance /* unused */, VkPhysicalDevice /* unused */) {
    return {};
}

Upscaler::Settings NoUpscaler::getOptimalSettings(
  Settings::Resolution /* unused */,
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

Upscaler::Status NoUpscaler::setDepthBuffer(void */* unused */, UnityRenderingExtTextureFormat /* unused */) {
  return NO_UPSCALER_SET;
}

Upscaler::Status NoUpscaler::setInputColor(void */* unused */, UnityRenderingExtTextureFormat /* unused */) {
  return NO_UPSCALER_SET;
}

Upscaler::Status NoUpscaler::setMotionVectors(void */* unused */, UnityRenderingExtTextureFormat /* unused */) {
  return NO_UPSCALER_SET;
}

Upscaler::Status NoUpscaler::setOutputColor(void * /* unused */, UnityRenderingExtTextureFormat /* unused */) {
    return NO_UPSCALER_SET;
}

void NoUpscaler::updateImages() {
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
