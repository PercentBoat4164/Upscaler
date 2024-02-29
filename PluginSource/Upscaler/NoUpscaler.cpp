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

Upscaler::Status NoUpscaler::getOptimalSettings(Settings::Resolution /*unused*/, Settings::QualityMode /*unused*/, bool /*unused*/) {
    return NO_UPSCALER_SET;
}

Upscaler::Status NoUpscaler::initialize() {
    return NO_UPSCALER_SET;
}

Upscaler::Status NoUpscaler::create() {
    return NO_UPSCALER_SET;
}

Upscaler::Status NoUpscaler::evaluate() {
    return NO_UPSCALER_SET;
}

Upscaler::Status NoUpscaler::shutdown() {
    return NO_UPSCALER_SET;
}
