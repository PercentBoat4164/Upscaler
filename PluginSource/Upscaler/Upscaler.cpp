#include "Upscaler.hpp"

#include "DLSS.hpp"
#include "FSR2.hpp"
#include "NoUpscaler.hpp"

#include <utility>

void (*Upscaler::log)(const char* msg) = nullptr;

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

Upscaler::Settings Upscaler::settings{};

void      (*Upscaler::errorCallback)(void *, Status, const char *){nullptr};
void     *Upscaler::userData{nullptr};
Upscaler *Upscaler::upscalerInUse{get<NoUpscaler>()};

void Upscaler::set(const Type upscaler) {
    set(get(upscaler));
}

void Upscaler::set(Upscaler *upscaler) {
    upscalerInUse = upscaler;
}

void Upscaler::setGraphicsAPI(const GraphicsAPI::Type graphicsAPI) {
    for (Upscaler *upscaler : getAllUpscalers()) upscaler->setFunctionPointers(graphicsAPI);
}

std::vector<Upscaler *> Upscaler::getAllUpscalers() {
    return {
      get<NoUpscaler>(),
#ifdef ENABLE_DLSS
      get<::DLSS>(),
#endif
#ifdef ENABLE_FSR2
      get<::FSR2>(),
#endif
    };
}

std::vector<Upscaler *> Upscaler::getUpscalersWithoutErrors() {
    std::vector<Upscaler *> upscalers;
    for (Upscaler *upscaler : getAllUpscalers())
        if (success(upscaler->getStatus())) upscalers.push_back(upscaler);
    return upscalers;
}

Upscaler *Upscaler::get(const Type upscaler) {
    switch (upscaler) {
        case NONE: return get<NoUpscaler>();
#ifdef ENABLE_DLSS
        case DLSS: return get<::DLSS>();
#endif
#ifdef ENABLE_FSR2
        case FSR2: return get<::FSR2>();
#endif
    }
    return get<NoUpscaler>();
}

Upscaler *Upscaler::get() {
    return upscalerInUse;
}

Upscaler::Status Upscaler::shutdown() {
    if (status != HARDWARE_ERROR_DEVICE_EXTENSIONS_NOT_SUPPORTED && status != SOFTWARE_ERROR_INSTANCE_EXTENSIONS_NOT_SUPPORTED) {
        status               = SUCCESS;
        detailedErrorMessage = "";
    }
    return SUCCESS;
}

Upscaler::Status Upscaler::getStatus() const {
    return status;
}

Upscaler::Status Upscaler::setStatus(const Status t_error, const std::string &t_msg) {
    return setStatusIf(true, t_error, t_msg);
}

Upscaler::Status Upscaler::setStatusIf(const bool t_shouldApplyError, const Status t_error, std::string t_msg) {
    bool shouldApplyError = success(status) && failure(t_error) && t_shouldApplyError;
    if (shouldApplyError) {
        status               = t_error;
        detailedErrorMessage = std::move(t_msg);
    }
    if (shouldApplyError && failure(status) && errorCallback != nullptr)
        errorCallback(userData, status, detailedErrorMessage.c_str());
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

void Upscaler::setLogCallback(void (*pFunction)(const char *)) {
    log = pFunction;
}

void (*Upscaler::
        setErrorCallback(void *data, void (*t_errorCallback)(void *, Status, const char *)))(void *, Status, const char *) {
    void (*oldCallback)(void *, Status, const char *) = errorCallback;
    errorCallback                                     = t_errorCallback;
    if (data != nullptr) userData = data;
    return oldCallback;
}
