#include "Upscaler.hpp"

#include "DLSS_Upscaler.hpp"
#include "FSR_Upscaler.hpp"
#include "None_Upscaler.hpp"
#include "XeSS_UPscaler.hpp"

#include "GraphicsAPI/GraphicsAPI.hpp"

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

void Upscaler::load(const GraphicsAPI::Type type, void* vkGetProcAddrFunc) {
#    ifdef ENABLE_DLSS
    DLSS_Upscaler::load(type, vkGetProcAddrFunc);
#    endif
#    ifdef ENABLE_FSR
    FSR_Upscaler::load(type, nullptr);
#    endif
#    ifdef ENABLE_XESS
    XeSS_Upscaler::load(type, nullptr);
#    endif
}

void Upscaler::shutdown() {
#    ifdef ENABLE_DLSS
    DLSS_Upscaler::shutdown();
#    endif
}

void Upscaler::unload() {
#    ifdef ENABLE_DLSS
    DLSS_Upscaler::unload();
#    endif
#    ifdef ENABLE_FSR
    FSR_Upscaler::unload();
#    endif
#    ifdef ENABLE_XESS
    XeSS_Upscaler::unload();
#    endif
}

bool Upscaler::isSupported(const Type type) {
    switch (type) {
        case NONE: return None_Upscaler::isSupported();
#ifdef ENABLE_DLSS
        case DLSS: return DLSS_Upscaler::isSupported();
#endif
#ifdef ENABLE_FSR
        case FSR: return FSR_Upscaler::isSupported();
#endif
#ifdef ENABLE_XESS
        case XESS: return XeSS_Upscaler::isSupported();
#endif
        default: return false;
    }
}

bool Upscaler::isSupported(const Type type, const enum Settings::Quality mode) {
    switch (type) {
        case NONE: return None_Upscaler::isSupported(mode);
#ifdef ENABLE_DLSS
        case DLSS: return DLSS_Upscaler::isSupported(mode);
#endif
#ifdef ENABLE_FSR
        case FSR: return FSR_Upscaler::isSupported(mode);
#endif
#ifdef ENABLE_XESS
        case XESS: return XeSS_Upscaler::isSupported(mode);
#endif
        default: return false;
    }
}

Upscaler* Upscaler::fromType(const Type type) {
    switch (type) {
        case NONE: return nullptr;
#ifdef ENABLE_DLSS
        case DLSS: return new DLSS_Upscaler();
#endif
#ifdef ENABLE_FSR
        case FSR: return new FSR_Upscaler();
#endif
#ifdef ENABLE_XESS
        case XESS: return new XeSS_Upscaler();
#endif
        default: return nullptr;
    }
}

void Upscaler::useGraphicsAPI(const GraphicsAPI::Type type) {
#ifdef ENABLE_DLSS
    DLSS_Upscaler::useGraphicsAPI(type);
#endif
#ifdef ENABLE_FSR
    FSR_Upscaler::useGraphicsAPI(type);
#endif
#ifdef ENABLE_XESS
    XeSS_Upscaler::useGraphicsAPI(type);
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