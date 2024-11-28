#include "Upscaler.hpp"

#include "DLSS.hpp"
#include "FSR3.hpp"
#include "GraphicsAPI/GraphicsAPI.hpp"
#include "NoUpscaler.hpp"
#include "XeSS.hpp"

#include <memory>
#include <utility>

bool Upscaler::success(const Status t_status) {
    return t_status == Success;
}

bool Upscaler::failure(const Status t_status) {
    return !success(t_status);
}

bool Upscaler::recoverable(const Status t_status) {
    return (t_status & ERROR_RECOVERABLE) == ERROR_RECOVERABLE;
}

bool Upscaler::isSupported(const Type type) {
    switch (type) {
        case NONE: return NoUpscaler::isSupported();
#ifdef ENABLE_DLSS
        case DLSS: return DLSS::isSupported();
#endif
#ifdef ENABLE_FSR3
        case FSR3: return FSR3::isSupported();
#endif
#ifdef ENABLE_XESS
        case XESS: return XeSS::isSupported();
#endif
        default: return false;
    }
}

bool Upscaler::isSupported(const Type type, const enum Settings::Quality mode) {
    switch (type) {
        case NONE: return NoUpscaler::isSupported(mode);
#ifdef ENABLE_DLSS
        case DLSS: return DLSS::isSupported(mode);
#endif
#ifdef ENABLE_FSR3
        case FSR3: return FSR3::isSupported(mode);
#endif
#ifdef ENABLE_XESS
        case XESS: return XeSS::isSupported(mode);
#endif
        default: return false;
    }
}

std::unique_ptr<Upscaler> Upscaler::fromType(const Type type) {
    switch (type) {
        case NONE: return std::make_unique<NoUpscaler>();
#ifdef ENABLE_DLSS
        case DLSS: return std::make_unique<class DLSS>();
#endif
#ifdef ENABLE_FSR3
        case FSR3: return std::make_unique<class FSR3>();
#endif
#ifdef ENABLE_XESS
        case XESS: return std::make_unique<XeSS>();
#endif
        default: return std::make_unique<NoUpscaler>();
    }
}

void Upscaler::useGraphicsAPI(const GraphicsAPI::Type type) {
#ifdef ENABLE_DLSS
    DLSS::useGraphicsAPI(type);
#endif
#ifdef ENABLE_FSR3
    FSR3::useGraphicsAPI(type);
#endif
#ifdef ENABLE_XESS
    XeSS::useGraphicsAPI(type);
#endif
}

Upscaler::Status Upscaler::getStatus() const {
    return status;
}

std::string& Upscaler::getErrorMessage() {
    return statusMessage;
}

Upscaler::Status Upscaler::setStatus(const Status t_error, const std::string& t_msg) {
    return setStatusIf(true, t_error, t_msg);
}

Upscaler::Status Upscaler::setStatusIf(const bool t_shouldApplyError, const Status t_error, std::string t_msg) {
    if (success(status) && failure(t_error) && t_shouldApplyError) {
        status        = t_error;
        statusMessage = std::move(t_msg);
    }
    return status;
}

bool Upscaler::resetStatus() {
    if (recoverable(status)) {
        status = Success;
        statusMessage.clear();
    }
    return status == Success;
}

void Upscaler::forceStatus(const Status newStatus, std::string message) {
    status = newStatus;
    statusMessage = std::move(message);
}