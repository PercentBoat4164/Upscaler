#include "Upscaler.hpp"

#include "DLSS.hpp"
#include "NoUpscaler.hpp"

#include <algorithm>
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

uint64_t Upscaler::Settings::Resolution::asUint64_t() const {
    return static_cast<uint64_t>(width) << 32U | height;
}

Upscaler* Upscaler::set(const Type upscaler) const {
    switch (upscaler) {
        case NONE: return new NoUpscaler();
#ifdef ENABLE_DLSS
        case DLSS: return new class DLSS(GraphicsAPI::getType());
#endif
    }
    return new NoUpscaler();
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

Upscaler::Status Upscaler::getStatus() const {
    return status;
}

Upscaler::Status Upscaler::setStatus(const Status t_error, const std::string& t_msg) {
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
    if (recoverable(status)) {
        status = SUCCESS;
        detailedErrorMessage.clear();
    }
    return status == SUCCESS;
}

std::string& Upscaler::getErrorMessage() {
    return detailedErrorMessage;
}

void Upscaler::setLogCallback(void (*pFunction)(const char*)) {
    log = pFunction;
}

void Upscaler::setErrorCallback(const void* data, void (*t_errorCallback)(const void*, Status, const char*)) {
    errorCallback = t_errorCallback;
    if (data != nullptr) userData = const_cast<void*>(data);
}