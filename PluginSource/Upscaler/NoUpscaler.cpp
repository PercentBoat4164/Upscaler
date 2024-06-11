#include "NoUpscaler.hpp"
#ifdef ENABLE_VULKAN
std::vector<std::string> NoUpscaler::requestVulkanInstanceExtensions(const std::vector<std::string>& /*unused*/) {
    return {};
}

std::vector<std::string> NoUpscaler::requestVulkanDeviceExtensions(VkInstance /*unused*/, VkPhysicalDevice /*unused*/, const std::vector<std::string>& /*unused*/) {
    return {};
}
#endif

bool NoUpscaler::isSupported() {
    return true;
}

bool NoUpscaler::isSupported(enum Settings::Quality /*unused*/) {
    return false;
}

NoUpscaler::NoUpscaler() {
    (void)resetStatus();
}

Upscaler::Status NoUpscaler::getOptimalSettings(const Settings::Resolution resolution, Settings::Preset /*unused*/, enum Settings::Quality /*unused*/, const bool /*unused*/) {
    settings.renderingResolution = resolution;
    settings.outputResolution = resolution;
    settings.dynamicMaximumInputResolution = resolution;
    settings.dynamicMinimumInputResolution = resolution;
    return getStatus();
}

Upscaler::Status NoUpscaler::initialize() {
    return getStatus();
}

Upscaler::Status NoUpscaler::create() {
    return getStatus();
}

Upscaler::Status NoUpscaler::evaluate() {
    return getStatus();
}

Upscaler::Status NoUpscaler::shutdown() {
    return getStatus();
}

bool NoUpscaler::resetStatus() {
    forceStatus(NO_UPSCALER_SET, getName() + " is currently active.");
    return true;
}
