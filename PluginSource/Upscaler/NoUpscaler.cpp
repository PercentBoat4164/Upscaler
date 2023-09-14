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

Upscaler::Settings NoUpscaler::getOptimalSettings(Upscaler::Settings::Resolution resolution) {
    return settings;
}

bool NoUpscaler::isSupportedAfter(bool b) {
    return true;
}

void NoUpscaler::setSupported(bool b) {
}

bool NoUpscaler::isAvailableAfter(bool b) {
    return true;
}

void NoUpscaler::setAvailable(bool b) {
}

bool NoUpscaler::isSupported() {
    return true;
}

bool NoUpscaler::isAvailable() {
    return true;
}

bool NoUpscaler::initialize() {
    return false;
}

bool NoUpscaler::createFeature() {
    return false;
}

bool NoUpscaler::setDepthBuffer(void *pVoid, UnityRenderingExtTextureFormat format) {
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
