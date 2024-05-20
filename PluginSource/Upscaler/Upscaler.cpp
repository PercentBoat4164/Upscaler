#include "Upscaler.hpp"

#include "DLSS.hpp"
#include "GraphicsAPI/GraphicsAPI.hpp"
#include "NoUpscaler.hpp"

#include <algorithm>
#include <memory>
#include <utility>

void (*Upscaler::logCallback)(const char* msg){nullptr};

bool Upscaler::success(const Status t_status) {
    return t_status <= NO_UPSCALER_SET;
}

bool Upscaler::failure(const Status t_status) {
    return t_status > NO_UPSCALER_SET;
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
        case DLSS: return DLSS::isSupported();
        case TYPE_MAX_ENUM: return false;
    }
    return false;
}

std::unique_ptr<Upscaler> Upscaler::fromType(const Type type) {
    std::unique_ptr<Upscaler> newUpscaler;
    switch (type) {
        case NONE: newUpscaler = std::make_unique<NoUpscaler>(); break;
#    ifdef ENABLE_DLSS
        case DLSS: newUpscaler = std::make_unique<class DLSS>(GraphicsAPI::getType()); break;
#    endif
        default: newUpscaler = std::make_unique<NoUpscaler>(); break;
    }
    return newUpscaler;
}

void Upscaler::setLogCallback(void (*pFunction)(const char*)) {
    logCallback = pFunction;
}

Upscaler::Status Upscaler::useImage(const Plugin::ImageID imageID, const UnityTextureID unityID) {
    RETURN_ON_FAILURE(setStatusIf(imageID >= Plugin::IMAGE_ID_MAX_ENUM, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Attempted to set image with ID greater than IMAGE_ID_MAX_ENUM."));
    textureIDs.at(imageID) = unityID;
    return SUCCESS;
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
        status = SUCCESS;
        statusMessage.clear();
    }
    return status == SUCCESS;
}

void Upscaler::forceStatus(const Status newStatus, std::string message) {
    status = newStatus;
    statusMessage = std::move(message);
}
