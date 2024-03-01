#include "Upscaler.hpp"

#include "DLSS.hpp"
#include "NoUpscaler.hpp"

#include <algorithm>
#include <utility>
#include <memory>

void (*Upscaler::logCallback)(const char* msg){nullptr};

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

#ifdef ENABLE_VULKAN
std::vector<std::string> Upscaler::requestVulkanInstanceExtensions(const std::vector<std::string>& supportedExtensions) {
    std::vector<std::string> requestedExtensions = NoUpscaler::requestVulkanInstanceExtensions(supportedExtensions);
    for (auto& extension : DLSS::requestVulkanInstanceExtensions(supportedExtensions))
        requestedExtensions.push_back(extension);
    return requestedExtensions;
}

std::vector<std::string> Upscaler::requestVulkanDeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice, const std::vector<std::string>& supportedExtensions) {
    std::vector<std::string> requestedExtensions = NoUpscaler::requestVulkanDeviceExtensions(supportedExtensions);
    for (auto& extension : DLSS::requestVulkanDeviceExtensions(instance, physicalDevice, supportedExtensions))
        requestedExtensions.push_back(extension);
    return requestedExtensions;
}
#endif

Upscaler::Status Upscaler::useImage(Plugin::ImageID imageID, UnityTextureID unityID) {
    RETURN_ON_FAILURE(setStatusIf(imageID >= Plugin::IMAGE_ID_MAX_ENUM, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, "Attempted to set image with ID greater than IMAGE_ID_MAX_ENUM."));
    textureIDs[imageID] = unityID;
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
    bool shouldApplyError = success(status) && failure(t_error) && t_shouldApplyError;
    if (shouldApplyError) {
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

std::unique_ptr<Upscaler> Upscaler::copyFromType(const Type type) {
    std::unique_ptr<Upscaler> newUpscaler = fromType(type);
    newUpscaler->userData                 = userData;
    return newUpscaler;
}

std::unique_ptr<Upscaler> Upscaler::fromType(const Type type) {
    std::unique_ptr<Upscaler> newUpscaler;
    switch (type) {
        case NONE: newUpscaler = std::make_unique<NoUpscaler>(); break;
#ifdef ENABLE_DLSS
        case DLSS: newUpscaler = std::make_unique<class DLSS>(GraphicsAPI::getType()); break;
#endif
        default: newUpscaler = std::make_unique<NoUpscaler>(); break;
    }
    return newUpscaler;
}

void Upscaler::setLogCallback(void (*pFunction)(const char*)) {
    logCallback = pFunction;
}
