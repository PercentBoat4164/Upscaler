#include "Upscaler.hpp"

#include "DLSS.hpp"
#include "NoUpscaler.hpp"

#include <utility>
void(*Upscaler::errorCallback)(void *, Status, const char *){nullptr};
void *Upscaler::userData{nullptr};
Upscaler          *Upscaler::upscalerInUse{get<NoUpscaler>()};
Upscaler::Settings Upscaler::settings{};

bool Upscaler::success(const Status t_status) {
    return t_status <= NO_UPSCALER_SET;
}

bool Upscaler::failure(const Status t_status) {
    return t_status > NO_UPSCALER_SET;
}

bool Upscaler::recoverable(const Status t_status) {
    return (t_status & ERROR_RECOVERABLE) == 1;
}

bool Upscaler::nonrecoverable(const Status t_status) {
    return (t_status & ERROR_RECOVERABLE) == 0;
}

uint64_t Upscaler::Settings::Resolution::asLong() const {
    return static_cast<uint64_t>(width) << 32U | height;
}

Upscaler *Upscaler::get(const Type upscaler) {
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
      get<NoUpscaler>(),
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

void Upscaler::set(const Type upscaler) {
    set(get(upscaler));
}

void Upscaler::set(Upscaler *upscaler) {
    upscalerInUse = upscaler;
}

void Upscaler::setGraphicsAPI(const GraphicsAPI::Type graphicsAPI) {
    for (Upscaler *upscaler : getAllUpscalers()) upscaler->setFunctionPointers(graphicsAPI);
}

void (*Upscaler::setErrorCallback(void *data, void (*t_errorCallback)(void *, Status, const char *)))(void *, Status, const char *) {
    void(*oldCallback)(void *, Status, const char *) = errorCallback;
    errorCallback = t_errorCallback;
    if (data != nullptr)
        userData = data;
    return oldCallback;
}

Upscaler::Status Upscaler::getStatus() const {
    return status;
}

Upscaler::Status Upscaler::setStatus(const Status t_error, std::string t_msg) {
    if (success(status)) status = t_error;
    if (detailedErrorMessage.empty()) detailedErrorMessage = std::move(t_msg);
    if (failure(status) && errorCallback != nullptr) errorCallback(userData, status, detailedErrorMessage.c_str());
    return status;
}

Upscaler::Status Upscaler::setStatusIf(const bool t_shouldApplyError, const Status t_error, std::string t_msg) {
    if (success(status) && t_shouldApplyError) status = t_error;
    if (detailedErrorMessage.empty() && t_shouldApplyError) detailedErrorMessage = std::move(t_msg);
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
