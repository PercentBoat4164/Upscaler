#include "NoUpscaler.hpp"

bool NoUpscaler::isSupported() {
    return true;
}

bool NoUpscaler::isSupported(enum Settings::Quality /*unused*/) {
    return false;
}

NoUpscaler::NoUpscaler() {
    (void)resetStatus();
}

Upscaler::Status NoUpscaler::useSettings(const Settings::Resolution resolution, const Settings::DLSSPreset /*unused*/, const enum Settings::Quality /*unused*/, const bool /*unused*/) {
    settings.recommendedInputResolution    = resolution;
    settings.outputResolution              = resolution;
    settings.dynamicMaximumInputResolution = resolution;
    settings.dynamicMinimumInputResolution = resolution;
    return getStatus();
}

Upscaler::Status NoUpscaler::useImages(const std::array<void*, Plugin::NumImages>& /*unused*/) {
    return Success;
}

Upscaler::Status NoUpscaler::evaluate() {
    return getStatus();
}

bool NoUpscaler::resetStatus() {
    forceStatus(Success, "No upscaler is currently active.");
    return true;
}
