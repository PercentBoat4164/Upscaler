#include "Upscaler.hpp"

#include "DLSS.hpp"
#include "NoUpscaler.hpp"

#include <utility>
void(*Upscaler::errorCallback)(void *, Upscaler::Status, const char *){nullptr};
void *Upscaler::userData{nullptr};
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
        if (upscaler->getStatus() == SUCCESS) upscalers.push_back(upscaler);
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

void Upscaler::setErrorCallback(void *data, void (*t_errorCallback)(void *, Upscaler::Status, const char *)) {
    errorCallback = t_errorCallback;
    userData = data;
}

std::tuple<void *, void(*)(void *, Upscaler::Status, const char *)> Upscaler::getErrorCallbackData() {
    return {userData, errorCallback};
}

bool Upscaler::isInitialized() const {
    return initialized;
}

Upscaler::Status Upscaler::getStatus() {
    return status;
}

Upscaler::Status Upscaler::setStatus(Upscaler::Status t_error, std::string t_msg) {
    if (success(status)) status = t_error;
    if (detailedErrorMessage.empty() && failure(status)) detailedErrorMessage = std::move(t_msg);
    if (failure(status) && errorCallback != nullptr) errorCallback(userData, status, detailedErrorMessage.c_str());
    return status;
}

Upscaler::Status Upscaler::setStatusIf(bool t_shouldApplyError, Upscaler::Status t_error, std::string t_msg) {
    if (success(status) && t_shouldApplyError) status = t_error;
    if (detailedErrorMessage.empty() && failure(status)) detailedErrorMessage = std::move(t_msg);
    if (t_shouldApplyError && failure(status) && errorCallback != nullptr) errorCallback(userData, status, detailedErrorMessage.c_str());
    return status;
}

bool Upscaler::resetStatus() {
    if ((status & ERROR_RECOVERABLE) != 0U) {
        status = SUCCESS;
        detailedErrorMessage.clear();
    }
    return status == SUCCESS;
}

std::string &Upscaler::getErrorMessage() {
    return detailedErrorMessage;
}

Upscaler::Status Upscaler::shutdown() {
    if (status != HARDWARE_ERROR_DEVICE_EXTENSIONS_NOT_SUPPORTED && status != SOFTWARE_ERROR_INSTANCE_EXTENSIONS_NOT_SUPPORTED) {
        status               = SUCCESS;
        detailedErrorMessage = "";
    }
    initialized = false;
    return SUCCESS;
}
