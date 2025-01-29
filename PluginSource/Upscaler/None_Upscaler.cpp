#include "None_Upscaler.hpp"

bool None_Upscaler::isSupported() {
    return true;
}

bool None_Upscaler::isSupported(enum Settings::Quality /*unused*/) {
    return false;
}

None_Upscaler::None_Upscaler() {
    (void)resetStatus();
}

Upscaler::Status None_Upscaler::useSettings(const Settings::Resolution resolution, const Settings::DLSSPreset /*unused*/, const enum Settings::Quality /*unused*/, const bool /*unused*/) {
    settings.recommendedInputResolution    = resolution;
    settings.outputResolution              = resolution;
    settings.dynamicMaximumInputResolution = resolution;
    settings.dynamicMinimumInputResolution = resolution;
    return getStatus();
}

Upscaler::Status None_Upscaler::useImages(const std::array<void*, Plugin::NumImages>& /*unused*/) {
    return Success;
}

Upscaler::Status None_Upscaler::evaluate(Settings::Resolution inputResolution) {
    return getStatus();
}

bool None_Upscaler::resetStatus() {
    forceStatus(Success, "No upscaler is currently active.");
    return true;
}
