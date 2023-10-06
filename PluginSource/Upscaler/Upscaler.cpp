#include "Upscaler.hpp"

#include "DLSS.hpp"
#include "NoUpscaler.hpp"

Upscaler          *Upscaler::upscalerInUse{get<NoUpscaler>()};
Upscaler::Settings Upscaler::settings{};

uint64_t Upscaler::Settings::Resolution::asLong() const {
    return (uint64_t) width << 32U | height;
}

Upscaler *Upscaler::get(Type upscaler) {
    switch (upscaler) {
        case NONE: return get<NoUpscaler>();
#ifdef ENABLE_DLSS
        case DLSS: return get<::DLSS>();
#endif
    }
    return get<NoUpscaler>();
}

Upscaler *Upscaler::get() {
    return upscalerInUse;
}

std::vector<Upscaler *> Upscaler::getAllUpscalers() {
    return {
      get<::NoUpscaler>(),
#ifdef ENABLE_DLSS
      get<::DLSS>(),
#endif
    };
}

std::vector<Upscaler *> Upscaler::getUpscalersWithoutErrors() {
    std::vector<Upscaler *> upscalers;
    for (Upscaler *upscaler : getAllUpscalers())
        if (upscaler->getError() == SUCCESS) upscalers.push_back(upscaler);
    return upscalers;
}

void Upscaler::set(Type upscaler) {
    set(get(upscaler));
}

void Upscaler::set(Upscaler *upscaler) {
    upscalerInUse = upscaler;
}

void Upscaler::setGraphicsAPI(GraphicsAPI::Type graphicsAPI) {
    for (Upscaler *upscaler : getAllUpscalers()) upscaler->setFunctionPointers(graphicsAPI);
}

Upscaler::UpscalerStatus Upscaler::getError() {
    return error;
}

Upscaler::UpscalerStatus Upscaler::setError(Upscaler::UpscalerStatus t_error) {
    if (error == SUCCESS) error = t_error;
    return error;
}

Upscaler::UpscalerStatus Upscaler::setErrorIf(bool t_shouldApplyError, Upscaler::UpscalerStatus t_error) {
    if (error == SUCCESS && t_shouldApplyError) error = t_error;
    return error;
}

bool Upscaler::resetError() {
    if ((error & ERROR_RECOVERABLE) != 0U) error = SUCCESS;
    return error == SUCCESS;
}

bool Upscaler::setErrorMessage(std::string msg) {
    if (!detailedErrorMessage.empty()) return false;
    detailedErrorMessage = std::move(msg);
    return true;
}

std::string &Upscaler::getErrorMessage() {
    return detailedErrorMessage;
}

Upscaler::UpscalerStatus Upscaler::shutdown() {
    if (error != HARDWARE_ERROR_DEVICE_EXTENSIONS_NOT_SUPPORTED && error != SOFTWARE_ERROR_INSTANCE_EXTENSIONS_NOT_SUPPORTED) {
        error                = SUCCESS;
        detailedErrorMessage = "";
    }
    initialized = false;
    return SUCCESS;
}
