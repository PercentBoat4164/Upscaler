#include "Upscaler.hpp"

#include "DLSS.hpp"
#include "FSR3.hpp"
#include "GraphicsAPI/GraphicsAPI.hpp"
#include "NoUpscaler.hpp"
#include "XeSS.hpp"

#include <memory>
#include <utility>

float UpscalerBase::Settings::JitterState::advance(const uint32_t maxIterations) {
    if (++iterations >= maxIterations) {
        n          = 0U;
        d          = 1U;
        iterations = -1U;
    }
    const uint32_t x = d - n;
    if (x == 1U) {
        n = 1U;
        d *= base;
    } else {
        uint32_t y = d / base;
        while (x <= y) y /= base;
        n = (base + 1U) * y - x;
    }
    return static_cast<float>(n) / static_cast<float>(d) - 0.5F;
}

UpscalerBase::Settings::Jitter& UpscalerBase::Settings::getNextJitter(const float inputWidth) {
    const float scalingFactor = static_cast<float>(outputResolution.width) / inputWidth;
    const auto  jitterSamples = static_cast<uint32_t>(std::ceil(static_cast<float>(SamplesPerPixel) * scalingFactor * scalingFactor));
    jitter                    = Jitter {x.advance(jitterSamples), y.advance(jitterSamples)};
    return jitter;
}

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

void Upscaler::setSupported() {
#ifdef ENABLE_DLSS
    DLSS::setSupported();
#endif
#ifdef ENABLE_FSR3
    FSR3::setSupported();
#endif
#ifdef ENABLE_XESS
    XeSS::setSupported();
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