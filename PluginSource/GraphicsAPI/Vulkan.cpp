#include "Vulkan.hpp"

#include "Logger.hpp"
#include "Upscaler/DLSS.hpp"

PFN_vkGetInstanceProcAddr                    Vulkan::m_vkGetInstanceProcAddr{};
PFN_vkGetDeviceProcAddr                      Vulkan::m_vkGetDeviceProcAddr{};
PFN_vkCreateInstance                         Vulkan::m_vkCreateInstance{};
PFN_vkEnumerateInstanceExtensionProperties   Vulkan::m_vkEnumerateInstanceExtensionProperties{};
PFN_vkCreateDevice                           Vulkan::m_vkCreateDevice{};
PFN_vkEnumerateDeviceExtensionProperties     Vulkan::m_vkEnumerateDeviceExtensionProperties{};
VkInstance                                   Vulkan::instance{};
std::unordered_map<VkDevice, Vulkan::Device> Vulkan::devices{};
IUnityGraphicsVulkanV2                      *Vulkan::vulkanInterface{};

Vulkan::Device::Device(VkDevice device, PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr) :
        device(device),
        // clang-format off
  m_vkCreateImageView(reinterpret_cast<PFN_vkCreateImageView>(vkGetDeviceProcAddr(device, "vkCreateImageView"))),
  m_vkCreateCommandPool(reinterpret_cast<PFN_vkCreateCommandPool>(vkGetDeviceProcAddr(device, "vkCreateCommandPool"))),
  m_vkAllocateCommandBuffers(reinterpret_cast<PFN_vkAllocateCommandBuffers>(vkGetDeviceProcAddr(device, "vkAllocateCommandBuffers"))),
  m_vkBeginCommandBuffer(reinterpret_cast<PFN_vkBeginCommandBuffer>(vkGetDeviceProcAddr(device, "vkBeginCommandBuffer"))),
  m_vkEndCommandBuffer(reinterpret_cast<PFN_vkEndCommandBuffer>(vkGetDeviceProcAddr(device, "vkEndCommandBuffer"))),
  m_vkQueueSubmit(reinterpret_cast<PFN_vkQueueSubmit>(vkGetDeviceProcAddr(device, "vkQueueSubmit"))),
  m_vkQueueWaitIdle(reinterpret_cast<PFN_vkQueueWaitIdle>(vkGetDeviceProcAddr(device, "vkQueueWaitIdle"))),
  m_vkResetCommandBuffer(reinterpret_cast<PFN_vkResetCommandBuffer>(vkGetDeviceProcAddr(device, "vkResetCommandBuffer"))),
  m_vkFreeCommandBuffers(reinterpret_cast<PFN_vkFreeCommandBuffers>(vkGetDeviceProcAddr(device, "vkFreeCommandBuffers"))),
  m_vkDestroyCommandPool(reinterpret_cast<PFN_vkDestroyCommandPool>(vkGetDeviceProcAddr(device, "vkDestroyCommandPool")))
// clang-format on
{
}

VkResult Vulkan::Device::vkCreateCommandPool(
  const VkCommandPoolCreateInfo *pCreateInfo,
  const VkAllocationCallbacks   *pAllocator,
  VkCommandPool                 *pCommandPool
) {
    return m_vkCreateCommandPool(device, pCreateInfo, pAllocator, pCommandPool);
}

VkResult Vulkan::Device::vkAllocateCommandBuffers(
  const VkCommandBufferAllocateInfo *pAllocateInfo,
  VkCommandBuffer                   *pCommandBuffers
) {
    return m_vkAllocateCommandBuffers(device, pAllocateInfo, pCommandBuffers);
}

VkResult
Vulkan::Device::vkBeginCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferBeginInfo *pBeginInfo) {
    return m_vkBeginCommandBuffer(commandBuffer, pBeginInfo);
}

VkResult Vulkan::Device::vkEndCommandBuffer(VkCommandBuffer commandBuffer) {
    return m_vkEndCommandBuffer(commandBuffer);
}

VkResult
Vulkan::Device::vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo *pSubmits, VkFence fence) {
    return m_vkQueueSubmit(queue, submitCount, pSubmits, fence);
}

VkResult Vulkan::Device::vkQueueWaitIdle(VkQueue queue) {
    return m_vkQueueWaitIdle(queue);
}

VkResult Vulkan::Device::vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) {
    return m_vkResetCommandBuffer(commandBuffer, flags);
}

void Vulkan::Device::vkFreeCommandBuffers(
  VkCommandPool    commandPool,
  uint32_t         commandBufferCount,
  VkCommandBuffer *pCommandBuffers
) {
    return m_vkFreeCommandBuffers(device, commandPool, commandBufferCount, pCommandBuffers);
}

void Vulkan::Device::vkDestroyCommandPool(VkCommandPool commandPool, const VkAllocationCallbacks *pAllocator) {
    return m_vkDestroyCommandPool(device, commandPool, pAllocator);
}

VkResult Vulkan::Device::vkCreateImageView(
  const VkImageViewCreateInfo *pCreateInfo,
  const VkAllocationCallbacks *pAllocator,
  VkImageView                 *pView
) {
    return m_vkCreateImageView(device, pCreateInfo, pAllocator, pView);
}

void Vulkan::Device::prepareForOneTimeSubmits() {
    if (_oneTimeSubmitRecording) return;
    UnityVulkanInstance     vulkanInstance = getVulkanInterface()->Instance();
    VkCommandPoolCreateInfo createInfo{
      .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext            = nullptr,
      .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
      .queueFamilyIndex = vulkanInstance.queueFamilyIndex,
    };

    vkCreateCommandPool(&createInfo, nullptr, &_oneTimeSubmitCommandPool);

    VkCommandBufferAllocateInfo allocateInfo{
      .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext              = nullptr,
      .commandPool        = _oneTimeSubmitCommandPool,
      .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 0x1,
    };

    vkAllocateCommandBuffers(&allocateInfo, &_oneTimeSubmitCommandBuffer);
    _oneTimeSubmitRecording = true;
}

VkCommandBuffer Vulkan::Device::beginOneTimeSubmitRecording() {
    VkCommandBufferBeginInfo beginInfo{
      .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext            = nullptr,
      .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = nullptr,
    };

    vkBeginCommandBuffer(_oneTimeSubmitCommandBuffer, &beginInfo);

    return _oneTimeSubmitCommandBuffer;
}

void Vulkan::Device::endOneTimeSubmitRecording() {
    if (!_oneTimeSubmitRecording) return;
    UnityVulkanInstance vulkanInstance = getVulkanInterface()->Instance();

    vkEndCommandBuffer(_oneTimeSubmitCommandBuffer);

    VkSubmitInfo submitInfo{
      .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext                = nullptr,
      .waitSemaphoreCount   = 0x0,
      .pWaitSemaphores      = nullptr,
      .pWaitDstStageMask    = nullptr,
      .commandBufferCount   = 0x1,
      .pCommandBuffers      = &_oneTimeSubmitCommandBuffer,
      .signalSemaphoreCount = 0x0,
      .pSignalSemaphores    = nullptr,
    };

    vkQueueSubmit(vulkanInstance.graphicsQueue, 1, &submitInfo, nullptr);
    vkQueueWaitIdle(vulkanInstance.graphicsQueue);
    vkResetCommandBuffer(_oneTimeSubmitCommandBuffer, 0x0);
    _oneTimeSubmitRecording = false;
}

void Vulkan::Device::cancelOneTimeSubmitRecording() {
    if (!_oneTimeSubmitRecording) return;
    vkResetCommandBuffer(_oneTimeSubmitCommandBuffer, 0x0);
    _oneTimeSubmitRecording = false;
}

void Vulkan::Device::finishOneTimeSubmits() {
    vkFreeCommandBuffers(_oneTimeSubmitCommandPool, 1, &_oneTimeSubmitCommandBuffer);
    vkDestroyCommandPool(_oneTimeSubmitCommandPool, nullptr);
}

bool Vulkan::loadEarlyFunctionPointers() {
    // clang-format off
    m_vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(m_vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance"));
    m_vkEnumerateInstanceExtensionProperties = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(m_vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties"));
    return (m_vkCreateInstance != nullptr) && (m_vkEnumerateInstanceExtensionProperties != nullptr);
    // clang-format on
}

bool Vulkan::loadLateFunctionPointers() {
    // clang-format off
    m_vkEnumerateDeviceExtensionProperties = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(m_vkGetInstanceProcAddr(instance, "vkEnumerateDeviceExtensionProperties"));
    m_vkCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(m_vkGetInstanceProcAddr(instance, "vkCreateDevice"));
    m_vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(m_vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr"));
    return (m_vkEnumerateInstanceExtensionProperties != nullptr) && (m_vkCreateDevice != nullptr) && (m_vkGetDeviceProcAddr != nullptr);
    // clang-format on
}

std::vector<std::string> Vulkan::getSupportedInstanceExtensions() {
    uint32_t                           extensionCount{};
    std::vector<std::string>           extensions;
    std::vector<VkExtensionProperties> extensionProperties{};
    m_vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    extensionProperties.resize(extensionCount);
    m_vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProperties.data());
    extensions.reserve(extensionCount);
    for (VkExtensionProperties extension : extensionProperties) extensions.emplace_back(extension.extensionName);
    return extensions;
}

VkResult Vulkan::Hook_vkCreateInstance(
  const VkInstanceCreateInfo  *pCreateInfo,
  const VkAllocationCallbacks *pAllocator,
  VkInstance                  *pInstance
) {
    VkInstanceCreateInfo createInfo = *pCreateInfo;
    std::stringstream    message;

    // Find out which extensions are supported
    std::vector<std::string> supportedExtensions = getSupportedInstanceExtensions();

    // Vectorize the enabled extensions
    std::vector<const char *> enabledExtensions{};
    enabledExtensions.reserve(pCreateInfo->enabledExtensionCount);
    for (uint32_t i{}; i < pCreateInfo->enabledExtensionCount; ++i)
        enabledExtensions.emplace_back(pCreateInfo->ppEnabledExtensionNames[i]);

    // Enable requested extensions in groups
    std::vector<std::string> newExtensions;
    for (Upscaler *upscaler : Upscaler::getAllUpscalers()) {
        // Mark supported features as such
        std::vector<std::string> requestedExtensions = upscaler->getRequiredVulkanInstanceExtensions();
        uint32_t                 supportedRequestedExtensionCount{};
        for (const std::string &requestedExtension : requestedExtensions) {
            for (const std::string &supportedExtension : supportedExtensions) {
                if (requestedExtension == supportedExtension) {
                    ++supportedRequestedExtensionCount;
                    break;
                }
            }
        }
        // If all extensions that were requested in this extension group are supported, then enable them.
        if (upscaler->isSupportedAfter(supportedRequestedExtensionCount == requestedExtensions.size())) {
            for (const std::string &extension : requestedExtensions) {
                bool enableExtension{true};
                for (const char *enabledExtension : enabledExtensions) {
                    if (extension == enabledExtension) {
                        enableExtension = false;
                        break;
                    }
                }
                if (enableExtension) {
                    newExtensions.push_back(extension);
                    enabledExtensions.push_back(newExtensions[newExtensions.size() - 1].c_str());
                    message << extension << ", ";
                }
            }
        }
    }

    // Modify the createInfo.
    createInfo.enabledExtensionCount   = enabledExtensions.size();
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();

    // Create the Instance.
    VkResult result = m_vkCreateInstance(&createInfo, pAllocator, pInstance);
    if (result == VK_SUCCESS) {
        instance = *pInstance;
        for (Upscaler *upscaler : Upscaler::getAllUpscalers())
            if (upscaler->isSupported())
                Logger::log("Successfully created a(n) " + upscaler->getName() + " compatible Vulkan instance.");
            else
                Logger::log(
                  "Failed to createFeature a(n) " + upscaler->getName() + " compatible Vulkan instance."
                );
        std::string msg = message.str();
        if (!msg.empty()) Logger::log("Added instance extensions: " + msg.substr(msg.length() - 2) + ".");
    } else {
        for (Upscaler *upscaler : Upscaler::getAllUpscalers())
            if (!upscaler->getRequiredVulkanInstanceExtensions().empty()) upscaler->isSupportedAfter(false);
        result = m_vkCreateInstance(pCreateInfo, pAllocator, pInstance);
        if (result == VK_SUCCESS) instance = *pInstance;
        else Logger::log("Failed to createFeature a Vulkan instance!");
    }
    return result;
}

std::vector<std::string> Vulkan::getSupportedDeviceExtensions(VkPhysicalDevice physicalDevice) {
    uint32_t                           extensionCount{};
    std::vector<std::string>           extensions;
    std::vector<VkExtensionProperties> extensionProperties{};
    m_vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    extensionProperties.resize(extensionCount);
    m_vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensionProperties.data());
    extensions.reserve(extensionCount);
    for (VkExtensionProperties extension : extensionProperties) extensions.emplace_back(extension.extensionName);
    return extensions;
}

VkResult Vulkan::Hook_vkCreateDevice(
  VkPhysicalDevice          physicalDevice,
  const VkDeviceCreateInfo *pCreateInfo,
  VkAllocationCallbacks    *pAllocator,
  VkDevice                 *pDevice
) {
    loadLateFunctionPointers();

    VkDeviceCreateInfo createInfo = *pCreateInfo;
    std::stringstream  message;

    // Find out which extensions are supported
    std::vector<std::string> supportedExtensions = getSupportedDeviceExtensions(physicalDevice);

    // Vectorize the enabled extensions
    std::vector<const char *> enabledExtensions{};
    enabledExtensions.reserve(pCreateInfo->enabledExtensionCount);
    for (uint32_t i{}; i < pCreateInfo->enabledExtensionCount; ++i)
        enabledExtensions.emplace_back(pCreateInfo->ppEnabledExtensionNames[i]);

    // Enable requested extensions in groups
    std::vector<std::string> newExtensions;
    for (Upscaler *upscaler : Upscaler::getAllUpscalers()) {
        // Mark supported features as such
        std::vector<std::string> requestedExtensions =
          upscaler->getRequiredVulkanDeviceExtensions(instance, physicalDevice);
        uint32_t supportedRequestedExtensionCount{};
        for (const std::string &requestedExtension : requestedExtensions) {
            for (const std::string &supportedExtension : supportedExtensions) {
                if (requestedExtension == supportedExtension) {
                    ++supportedRequestedExtensionCount;
                    break;
                }
            }
        }
        // If all extensions that were requested in this extension group are supported, then enable them.
        if (upscaler->isSupportedAfter(supportedRequestedExtensionCount == requestedExtensions.size())) {
            for (const std::string &extension : requestedExtensions) {
                bool enableExtension{true};
                for (const char *enabledExtension : enabledExtensions) {
                    if (extension == enabledExtension) {
                        enableExtension = false;
                        break;
                    }
                }
                if (enableExtension) {
                    newExtensions.push_back(extension);
                    enabledExtensions.push_back(newExtensions[newExtensions.size() - 1].c_str());
                    message << extension << ", ";
                }
            }
        }
    }

    // Modify the createInfo.
    createInfo.enabledExtensionCount   = enabledExtensions.size();
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();

    // Create the Device
    VkResult result = m_vkCreateDevice(physicalDevice, &createInfo, pAllocator, pDevice);
    if (result == VK_SUCCESS) {
        devices.insert({*pDevice, Device(*pDevice, m_vkGetDeviceProcAddr)});
        for (Upscaler *upscaler : Upscaler::getAllUpscalers())
            if (upscaler->isSupported())
                Logger::log("Successfully created a(n) " + upscaler->getName() + " compatible Vulkan device.");
            else Logger::log("Failed to createFeature a(n) " + upscaler->getName() + " compatible Vulkan device.");
        std::string msg = message.str();
        if (!msg.empty()) Logger::log("Added device extensions: " + msg.substr(0, msg.length() - 2) + ".");
    } else {
        for (Upscaler *upscaler : Upscaler::getAllUpscalers())
            if (!upscaler->getRequiredVulkanDeviceExtensions(instance, physicalDevice).empty())
                upscaler->isSupportedAfter(false);
        result = m_vkCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
        if (result == VK_SUCCESS) devices.insert({*pDevice, Device(*pDevice, m_vkGetDeviceProcAddr)});
        else Logger::log("Failed to createFeature a Vulkan instance!");
    }
    return result;
}

Vulkan::Device Vulkan::getDevice(VkDevice device) {
    return devices.at(device);
}

void Vulkan::setVkGetInstanceProcAddr(PFN_vkGetInstanceProcAddr t_vkGetInstanceProcAddr) {
    m_vkGetInstanceProcAddr = t_vkGetInstanceProcAddr;
}

PFN_vkGetInstanceProcAddr Vulkan::getVkGetInstanceProcAddr() {
    return m_vkGetInstanceProcAddr;
}

bool Vulkan::interceptInitialization(IUnityGraphicsVulkanV2 *t_vulkanInterface) {
    vulkanInterface = t_vulkanInterface;
    return vulkanInterface->AddInterceptInitialization(interceptInitialization, nullptr, 0);
}

bool Vulkan::RemoveInterceptInitialization() {
    bool result     = vulkanInterface->RemoveInterceptInitialization(interceptInitialization);
    vulkanInterface = nullptr;
    return result;
}

IUnityGraphicsVulkanV2 *Vulkan::getVulkanInterface() {
    return vulkanInterface;
}

GraphicsAPI::Type Vulkan::getType() {
    return GraphicsAPI::VULKAN;
}

PFN_vkVoidFunction Vulkan::Hook_vkGetInstanceProcAddr(VkInstance t_instance, const char *pName) {
    if (pName == nullptr) return nullptr;
    if (strcmp(pName, "vkCreateInstance") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(&Hook_vkCreateInstance);
    if (strcmp(pName, "vkCreateDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Hook_vkCreateDevice);
    return m_vkGetInstanceProcAddr(t_instance, pName);
}

PFN_vkGetInstanceProcAddr
Vulkan::interceptInitialization(PFN_vkGetInstanceProcAddr t_getInstanceProcAddr, void * /*unused*/) {
    setVkGetInstanceProcAddr(t_getInstanceProcAddr);
    loadEarlyFunctionPointers();
    return Hook_vkGetInstanceProcAddr;
}