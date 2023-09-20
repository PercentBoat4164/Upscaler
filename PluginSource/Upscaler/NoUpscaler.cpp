#include "NoUpscaler.hpp"

NoUpscaler *NoUpscaler::get() {
    NoUpscaler *noUpscaler{new NoUpscaler};
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

Upscaler::Settings NoUpscaler::getOptimalSettings(Upscaler::Settings::Resolution /* unused */, bool /* unused */) {
    return settings;
}

bool NoUpscaler::initialize() {
    return false;
}

bool NoUpscaler::createFeature() {
    return false;
}

bool NoUpscaler::setImageResources(
  void                          *nativeDepthBuffer,
  UnityRenderingExtTextureFormat unityDepthFormat,
  void                          *nativeMotionVectors,
  UnityRenderingExtTextureFormat unityMotionVectorFormat,
  void                          *nativeInColor,
  UnityRenderingExtTextureFormat unityInColorFormat,
  void                          *nativeOutColor,
  UnityRenderingExtTextureFormat unityOutColorFormat
) {
    return false;
}

bool NoUpscaler::evaluate() {
    return false;
}

bool NoUpscaler::releaseFeature() {
    return false;
}

bool NoUpscaler::shutdown() {
    return false;
}

void NoUpscaler::setFunctionPointers(GraphicsAPI::Type graphicsAPI) {
}
