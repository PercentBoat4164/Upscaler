#include "Upscaler.hpp"

#include "DLSS.hpp"
#include "NoUpscaler.hpp"

#include <utility>
void(*Upscaler::errorCallback)(Upscaler::Status, const char *){nullptr};
Upscaler          *Upscaler::upscalerInUse{get<NoUpscaler>()};
Upscaler::Settings Upscaler::settings{};

bool Upscaler::success(Upscaler::Status t_status) {
    return t_status <= Status::NO_UPSCALER_SET;
}

bool Upscaler::failure(Upscaler::Status t_status) {
    return t_status > Status::NO_UPSCALER_SET;
}

bool Upscaler::recoverable(Upscaler::Status t_status) {
    return (t_status & ERROR_RECOVERABLE) == 1;
}

bool Upscaler::nonrecoverable(Upscaler::Status t_status) {
    return (t_status & ERROR_RECOVERABLE) == 0;
}

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

auto Upscaler::setErrorCallback(void (*t_errorCallback)(Upscaler::Status, const char *)) -> void(*)(Upscaler::Status, const char *) {
    void(*oldCallback)(Upscaler::Status, const char *) = errorCallback;
    errorCallback = t_errorCallback;
    return oldCallback;
}

Upscaler::Status Upscaler::getError() {
    return error;
}

Upscaler::Status Upscaler::setError(Upscaler::Status t_error, std::string t_msg) {
    if (success(error)) error = t_error;
    if (detailedErrorMessage.empty()) detailedErrorMessage = std::move(t_msg);
    if (failure(error) && errorCallback != nullptr) {
        errorCallback(error, detailedErrorMessage.c_str());
    }
    return error;
}

Upscaler::Status Upscaler::setErrorIf(bool t_shouldApplyError, Upscaler::Status t_error, std::string t_msg) {
    if (success(error) && t_shouldApplyError) error = t_error;
    if (detailedErrorMessage.empty() && t_shouldApplyError) detailedErrorMessage = std::move(t_msg);
    if (t_shouldApplyError && errorCallback != nullptr) {
        errorCallback(error, detailedErrorMessage.c_str());
    }
    return error;
}

bool Upscaler::resetError() {
    if ((error & ERROR_RECOVERABLE) != 0U) {
        error = SUCCESS;
        detailedErrorMessage.clear();
    }
    return error == SUCCESS;
}

std::string &Upscaler::getErrorMessage() {
    return detailedErrorMessage;
}

Upscaler::Status Upscaler::shutdown() {
    if (error != HARDWARE_ERROR_DEVICE_EXTENSIONS_NOT_SUPPORTED && error != SOFTWARE_ERROR_INSTANCE_EXTENSIONS_NOT_SUPPORTED) {
        error                = SUCCESS;
        detailedErrorMessage = "";
    }
    initialized = false;
    return SUCCESS;
}