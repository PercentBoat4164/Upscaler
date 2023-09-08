#include "Vulkan.hpp"

#include "Logger.hpp"
#include "Upscaler/DLSS.hpp"

GraphicsAPI::Vulkan::DeviceFunctions::DeviceFunctions(
  VkDevice                device,
  PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr
) :
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

VkResult GraphicsAPI::Vulkan::DeviceFunctions::vkCreateCommandPool(
  const VkCommandPoolCreateInfo *pCreateInfo,
  const VkAllocationCallbacks   *pAllocator,
  VkCommandPool                 *pCommandPool
) {
    return m_vkCreateCommandPool(device, pCreateInfo, pAllocator, pCommandPool);
}

VkResult GraphicsAPI::Vulkan::DeviceFunctions::vkAllocateCommandBuffers(
  const VkCommandBufferAllocateInfo *pAllocateInfo,
  VkCommandBuffer                   *pCommandBuffers
) {
    return m_vkAllocateCommandBuffers(device, pAllocateInfo, pCommandBuffers);
}

VkResult GraphicsAPI::Vulkan::DeviceFunctions::vkBeginCommandBuffer(
  VkCommandBuffer           commandBuffer,
  VkCommandBufferBeginInfo *pBeginInfo
) {
    return m_vkBeginCommandBuffer(commandBuffer, pBeginInfo);
}

VkResult GraphicsAPI::Vulkan::DeviceFunctions::vkEndCommandBuffer(VkCommandBuffer commandBuffer) {
    return m_vkEndCommandBuffer(commandBuffer);
}

VkResult GraphicsAPI::Vulkan::DeviceFunctions::vkQueueSubmit(
  VkQueue             queue,
  uint32_t            submitCount,
  const VkSubmitInfo *pSubmits,
  VkFence             fence
) {
    return m_vkQueueSubmit(queue, submitCount, pSubmits, fence);
}

VkResult GraphicsAPI::Vulkan::DeviceFunctions::vkQueueWaitIdle(VkQueue queue) {
    return m_vkQueueWaitIdle(queue);
}

VkResult GraphicsAPI::Vulkan::DeviceFunctions::vkResetCommandBuffer(
  VkCommandBuffer           commandBuffer,
  VkCommandBufferResetFlags flags
) {
    return m_vkResetCommandBuffer(commandBuffer, flags);
}

void GraphicsAPI::Vulkan::DeviceFunctions::vkFreeCommandBuffers(
  VkCommandPool    commandPool,
  uint32_t         commandBufferCount,
  VkCommandBuffer *pCommandBuffers
) {
    return m_vkFreeCommandBuffers(device, commandPool, commandBufferCount, pCommandBuffers);
}

void GraphicsAPI::Vulkan::DeviceFunctions::vkDestroyCommandPool(
  VkCommandPool                commandPool,
  const VkAllocationCallbacks *pAllocator
) {
    return m_vkDestroyCommandPool(device, commandPool, pAllocator);
}

VkResult GraphicsAPI::Vulkan::DeviceFunctions::vkCreateImageView(
  const VkImageViewCreateInfo *pCreateInfo,
  const VkAllocationCallbacks *pAllocator,
  VkImageView                 *pView
) {
    return m_vkCreateImageView(device, pCreateInfo, pAllocator, pView);
}

PFN_vkGetInstanceProcAddr                  GraphicsAPI::Vulkan::m_vkGetInstanceProcAddr{};
PFN_vkGetDeviceProcAddr                    GraphicsAPI::Vulkan::m_vkGetDeviceProcAddr{};
PFN_vkCreateInstance                       GraphicsAPI::Vulkan::m_vkCreateInstance{};
PFN_vkEnumerateInstanceExtensionProperties GraphicsAPI::Vulkan::m_vkEnumerateInstanceExtensionProperties{};
PFN_vkCreateDevice                         GraphicsAPI::Vulkan::m_vkCreateDevice{};
PFN_vkEnumerateDeviceExtensionProperties   GraphicsAPI::Vulkan::m_vkEnumerateDeviceExtensionProperties{};
VkInstance                                    GraphicsAPI::Vulkan::instance{};
std::unordered_map<VkDevice, GraphicsAPI::Vulkan::DeviceFunctions> GraphicsAPI::Vulkan::deviceFunctions{};
IUnityGraphicsVulkanV2                       *GraphicsAPI::Vulkan::vulkanInterface{};

bool GraphicsAPI::Vulkan::loadEarlyFunctionPointers() {
    // clang-format off
    m_vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(m_vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance"));
    m_vkEnumerateInstanceExtensionProperties = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(m_vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties"));
    return (m_vkCreateInstance != nullptr) && (m_vkEnumerateInstanceExtensionProperties != nullptr);
    // clang-format on
}

bool GraphicsAPI::Vulkan::loadLateFunctionPointers() {
    // clang-format off
    m_vkEnumerateDeviceExtensionProperties = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(m_vkGetInstanceProcAddr(instance, "vkEnumerateDeviceExtensionProperties"));
    m_vkCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(m_vkGetInstanceProcAddr(instance, "vkCreateDevice"));
    m_vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(m_vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr"));
    return (m_vkEnumerateInstanceExtensionProperties != nullptr) && (m_vkCreateDevice != nullptr) && (m_vkGetDeviceProcAddr != nullptr);
    // clang-format on
}

ExtensionGroup GraphicsAPI::Vulkan::getSupportedInstanceExtensions() {
    uint32_t                           extensionCount{};
    ExtensionGroup                     extensions;
    std::vector<VkExtensionProperties> extensionProperties{};
    m_vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    extensionProperties.resize(extensionCount);
    m_vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProperties.data());
    extensions.reserve(extensionCount);
    for (VkExtensionProperties extension : extensionProperties) extensions.emplace_back(extension.extensionName);
    return extensions;
}

VkResult GraphicsAPI::Vulkan::Hook_vkCreateInstance(
  const VkInstanceCreateInfo  *pCreateInfo,
  const VkAllocationCallbacks *pAllocator,
  VkInstance                  *pInstance
) {
    VkInstanceCreateInfo createInfo = *pCreateInfo;
    std::stringstream    message;

    // Find out which extensions are supported
    ExtensionGroup supportedExtensions = getSupportedInstanceExtensions();

    // Find out which extensions need to be added for each upscaler.
    std::vector<std::pair<Upscaler::Upscaler *, ExtensionGroup>> requestedExtensions{
      {Upscaler::Upscaler::get<Upscaler::DLSS>(), Upscaler::DLSS::getRequiredVulkanInstanceExtensions()},
    };

    // Vectorize the enabled extensions
    std::vector<const char *> enabledExtensions{};
    enabledExtensions.reserve(pCreateInfo->enabledExtensionCount);
    for (uint32_t i{}; i < pCreateInfo->enabledExtensionCount; ++i)
        enabledExtensions.emplace_back(pCreateInfo->ppEnabledExtensionNames[i]);

    // Mark supported features as such
    for (std::pair<Upscaler::Upscaler *, ExtensionGroup> &requestedExtensionGroup : requestedExtensions) {
        uint32_t supportedRequestedExtensionCount{};
        for (const std::string& requestedExtension : requestedExtensionGroup.second) {
            for (const std::string& supportedExtension : supportedExtensions) {
                if (requestedExtension == supportedExtension) {
                    ++supportedRequestedExtensionCount;
                    break;
                }
            }
        }
        // If all extensions that were requested in this extension group are supported, then enable them.
        if (requestedExtensionGroup.first->setIsSupported(
          supportedRequestedExtensionCount == requestedExtensionGroup.second.size()
        )) {
            for (const std::string &extension : requestedExtensionGroup.second) {
                bool enableExtension{true};
                for (const char *enabledExtension : enabledExtensions) {
                    if (extension == enabledExtension) {
                        enableExtension = false;
                        break;
                    }
                }
                if (enableExtension) {
                    enabledExtensions.push_back(extension.c_str());
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
        if (Upscaler::DLSS::isSupported()) {
            Logger::log("Successfully created A DLSS compatible Vulkan instance.");
            std::string msg = message.str();
            if (msg.empty()) Logger::log("All requested instance extensions were already enabled.");
            else Logger::log("Added instance extensions: " + msg.substr(msg.length() - 2) + ".");
        } else
            Logger::log("Failed to create A DLSS compatible Vulkan instance.");
    } else
        Logger::log("Failed to create A DLSS compatible Vulkan instance.");
    return result;
}

ExtensionGroup GraphicsAPI::Vulkan::getSupportedDeviceExtensions(VkPhysicalDevice physicalDevice) {
    uint32_t                           extensionCount{};
    ExtensionGroup                     extensions;
    std::vector<VkExtensionProperties> extensionProperties{};
    m_vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    extensionProperties.resize(extensionCount);
    m_vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensionProperties.data());
    extensions.reserve(extensionCount);
    for (VkExtensionProperties extension : extensionProperties) extensions.emplace_back(extension.extensionName);
    return extensions;
}

VkResult GraphicsAPI::Vulkan::Hook_vkCreateDevice(
  VkPhysicalDevice          physicalDevice,
  const VkDeviceCreateInfo *pCreateInfo,
  VkAllocationCallbacks    *pAllocator,
  VkDevice                 *pDevice
) {
    loadLateFunctionPointers();

    VkDeviceCreateInfo createInfo = *pCreateInfo;
    std::stringstream  message;

    // Find out which extensions are supported
    ExtensionGroup supportedExtensions = getSupportedDeviceExtensions(physicalDevice);

    // Find out which extensions need to be added for each upscaler.
    std::vector<std::pair<Upscaler::Upscaler *, ExtensionGroup>> requestedExtensions{
        {Upscaler::Upscaler::get<Upscaler::DLSS>(), Upscaler::DLSS::getRequiredVulkanDeviceExtensions(instance, physicalDevice)},
    };

    // Vectorize the enabled extensions
    std::vector<const char *> enabledExtensions{};
    enabledExtensions.reserve(pCreateInfo->enabledExtensionCount);
    for (uint32_t i{}; i < pCreateInfo->enabledExtensionCount; ++i)
        enabledExtensions.emplace_back(pCreateInfo->ppEnabledExtensionNames[i]);

    // Mark supported features as such
    for (std::pair<Upscaler::Upscaler *, ExtensionGroup> &requestedExtensionGroup : requestedExtensions) {
        uint32_t supportedRequestedExtensionCount{};
        for (const std::string& requestedExtension : requestedExtensionGroup.second) {
            for (const std::string& supportedExtension : supportedExtensions) {
                if (requestedExtension == supportedExtension) {
                    ++supportedRequestedExtensionCount;
                    break;
                }
            }
        }
        // If all extensions that were requested in this extension group are supported, then enable any that are not already enabled.
        if (requestedExtensionGroup.first->setIsSupported(
          supportedRequestedExtensionCount == requestedExtensionGroup.second.size()
        )) {
            for (const std::string &extension : requestedExtensionGroup.second) {
                bool enableExtension{true};
                for (const char *enabledExtension : enabledExtensions) {
                    if (extension == enabledExtension) {
                        enableExtension = false;
                        break;
                    }
                }
                if (enableExtension) {
                    enabledExtensions.push_back(extension.c_str());
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
        deviceFunctions.insert({*pDevice, DeviceFunctions(*pDevice, m_vkGetDeviceProcAddr)});
        if (Upscaler::DLSS::isSupported()) {
            Logger::log("Successfully created a DLSS compatible Vulkan device.");
            std::string msg = message.str();
            if (msg.empty()) Logger::log("All requested device extensions were already enabled.");
            else Logger::log("Added device requestedExtensions: " + msg.substr(0, msg.length() - 2) + ".");
        }
        else
            Logger::log("Failed to create a DLSS compatible Vulkan device.");
    } else
        Logger::log("Failed to create a DLSS compatible Vulkan device.");
    return result;
}

GraphicsAPI::Vulkan::DeviceFunctions GraphicsAPI::Vulkan::get(VkDevice device) {
    return deviceFunctions.at(device);
}

void GraphicsAPI::Vulkan::setVkGetInstanceProcAddr(PFN_vkGetInstanceProcAddr t_vkGetInstanceProcAddr) {
    m_vkGetInstanceProcAddr = t_vkGetInstanceProcAddr;
}

PFN_vkGetInstanceProcAddr GraphicsAPI::Vulkan::getVkGetInstanceProcAddr() {
    return m_vkGetInstanceProcAddr;
}

bool GraphicsAPI::Vulkan::interceptInitialization(IUnityGraphicsVulkanV2 *t_vulkanInterface) {
    vulkanInterface = t_vulkanInterface;
    return vulkanInterface->AddInterceptInitialization(interceptInitialization, nullptr, 0);
}

bool GraphicsAPI::Vulkan::RemoveInterceptInitialization() {
    bool result     = vulkanInterface->RemoveInterceptInitialization(interceptInitialization);
    vulkanInterface = nullptr;
    return result;
}

IUnityGraphicsVulkanV2 *GraphicsAPI::Vulkan::getVulkanInterface() {
    return vulkanInterface;
}
