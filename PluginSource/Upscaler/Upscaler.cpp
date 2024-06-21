#include "Upscaler.hpp"

#include "DLSS.hpp"
#include "FSR2.hpp"
#include "GraphicsAPI/GraphicsAPI.hpp"
#include "NoUpscaler.hpp"
#include "XeSS.hpp"

#include <memory>
#include <utility>

void (*Upscaler::logCallback)(const char* msg){nullptr};

bool Upscaler::success(const Status t_status) {
    return t_status == Success;
}

bool Upscaler::failure(const Status t_status) {
    return !success(t_status);
}

bool Upscaler::recoverable(const Status t_status) {
    return (t_status & ERROR_RECOVERABLE) == ERROR_RECOVERABLE;
}

#ifdef ENABLE_VULKAN
std::vector<std::string> Upscaler::requestVulkanInstanceExtensions(const std::vector<std::string>& supportedExtensions) {
    std::vector<std::string> requestedExtensions = NoUpscaler::requestVulkanInstanceExtensions(supportedExtensions);
#    ifdef ENABLE_DLSS
    for (auto& extension : DLSS::requestVulkanInstanceExtensions(supportedExtensions))
        requestedExtensions.push_back(extension);
#    endif
    return requestedExtensions;
}

std::vector<std::string> Upscaler::requestVulkanDeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice, const std::vector<std::string>& supportedExtensions) {
    std::vector<std::string> requestedExtensions = NoUpscaler::requestVulkanDeviceExtensions(instance, physicalDevice, supportedExtensions);
#    ifdef ENABLE_DLSS
    for (auto& extension : DLSS::requestVulkanDeviceExtensions(instance, physicalDevice, supportedExtensions))
        requestedExtensions.push_back(extension);
#    endif
    return requestedExtensions;
}
#endif

bool Upscaler::isSupported(const Type type) {
    switch (type) {
        case NONE: return NoUpscaler::isSupported();
#ifdef ENABLE_DLSS
        case DLSS: return DLSS::isSupported();
#endif
#ifdef ENABLE_FSR2
        case FSR2: return FSR2::isSupported();
#endif
#ifdef ENABLE_XESS
        case XESS: return XeSS::isSupported();
#endif
        default: return false;
    }
    return false;
}

bool Upscaler::isSupported(const Type type, const enum Settings::Quality mode) {
    switch (type) {
        case NONE: return NoUpscaler::isSupported(mode);
#ifdef ENABLE_DLSS
        case DLSS: return DLSS::isSupported(mode);
#endif
#ifdef ENABLE_FSR2
        case FSR2: return FSR2::isSupported(mode);
#endif
#ifdef ENABLE_XESS
        case XESS: return XeSS::isSupported(mode);
#endif
        default: return false;
    }
    return false;
}

std::unique_ptr<Upscaler> Upscaler::fromType(const Type type) {
    switch (type) {
        case NONE: return std::make_unique<NoUpscaler>();
#ifdef ENABLE_DLSS
        case DLSS: return std::make_unique<class DLSS>(GraphicsAPI::getType());
#endif
#ifdef ENABLE_FSR2
        case FSR2: return std::make_unique<class FSR2>(GraphicsAPI::getType());
#endif
#ifdef ENABLE_XESS
        case XESS: return std::make_unique<XeSS>(GraphicsAPI::getType());
#endif
        default: return std::make_unique<NoUpscaler>();
    }
}

void Upscaler::setLogCallback(void (*pFunction)(const char*)) {
    logCallback = pFunction;
}

Upscaler::Status Upscaler::useImages(const std::array<void*, Plugin::IMAGE_ID_MAX_ENUM>& images) {
    textures = images;
    return Success;
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