#include "NoUpscaler.hpp"
#ifdef ENABLE_VULKAN
std::vector<std::string> NoUpscaler::requestVulkanInstanceExtensions(const std::vector<std::string>& /*unused*/) {
    return {};
}

std::vector<std::string> NoUpscaler::requestVulkanDeviceExtensions(const std::vector<std::string>& /*unused*/) {
    return {};
}
#endif

bool NoUpscaler::isSupported() {
    return true;
}

NoUpscaler::NoUpscaler() {
    resetStatus();
}

Upscaler::Status NoUpscaler::getOptimalSettings(Settings::Resolution /*unused*/, Settings::Preset /*unused*/, enum Settings::Quality /*unused*/, const bool /*unused*/) {
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
