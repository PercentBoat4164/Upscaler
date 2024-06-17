#ifdef ENABLE_VULKAN
#    include "Vulkan.hpp"

#    include <Upscaler/Upscaler.hpp>

#    include <IUnityGraphicsVulkan.h>

#    include <cstring>

VkInstance Vulkan::temporaryInstance{VK_NULL_HANDLE};

PFN_vkGetInstanceProcAddr                  Vulkan::m_vkGetInstanceProcAddr{VK_NULL_HANDLE};
PFN_vkGetDeviceProcAddr                    Vulkan::m_vkGetDeviceProcAddr{VK_NULL_HANDLE};
PFN_vkCreateInstance                       Vulkan::m_vkCreateInstance{VK_NULL_HANDLE};
PFN_vkEnumerateInstanceExtensionProperties Vulkan::m_vkEnumerateInstanceExtensionProperties{VK_NULL_HANDLE};
PFN_vkCreateDevice                         Vulkan::m_vkCreateDevice{VK_NULL_HANDLE};
PFN_vkEnumerateDeviceExtensionProperties   Vulkan::m_vkEnumerateDeviceExtensionProperties{VK_NULL_HANDLE};

PFN_vkCreateImageView  Vulkan::m_vkCreateImageView{VK_NULL_HANDLE};
PFN_vkDestroyImageView Vulkan::m_vkDestroyImageView{VK_NULL_HANDLE};

IUnityGraphicsVulkanV2* Vulkan::graphicsInterface{nullptr};

void Vulkan::loadEarlyFunctionPointers() {
    m_vkCreateInstance                       = reinterpret_cast<PFN_vkCreateInstance>(m_vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance"));
    m_vkEnumerateInstanceExtensionProperties = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(m_vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties"));
}

void Vulkan::loadInstanceFunctionPointers(VkInstance instance) {
    m_vkEnumerateDeviceExtensionProperties = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(m_vkGetInstanceProcAddr(instance, "vkEnumerateDeviceExtensionProperties"));
    m_vkCreateDevice                       = reinterpret_cast<PFN_vkCreateDevice>(m_vkGetInstanceProcAddr(instance, "vkCreateDevice"));
    m_vkGetDeviceProcAddr                  = reinterpret_cast<PFN_vkGetDeviceProcAddr>(m_vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr"));
}

void Vulkan::loadDeviceFunctionPointers(VkDevice device) {
    m_vkCreateImageView  = reinterpret_cast<PFN_vkCreateImageView>(m_vkGetDeviceProcAddr(device, "vkCreateImageView"));
    m_vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(m_vkGetDeviceProcAddr(device, "vkDestroyImageView"));
}

std::vector<std::string> Vulkan::getSupportedInstanceExtensions() {
    uint32_t                           extensionCount{};
    std::vector<std::string>           extensions;
    std::vector<VkExtensionProperties> extensionProperties{};
    m_vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    extensionProperties.resize(extensionCount);
    m_vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProperties.data());
    extensions.reserve(extensionCount);
    for (auto [extensionName, specVersion] : extensionProperties) extensions.emplace_back(extensionName);
    return extensions;
}

VkResult Vulkan::Hook_vkCreateInstance(
  const VkInstanceCreateInfo*  pCreateInfo,
  const VkAllocationCallbacks* pAllocator,
  VkInstance*                  pInstance
) {
    VkInstanceCreateInfo createInfo = *pCreateInfo;

    // Find out which extensions are supported
    const std::vector<std::string>  supportedExtensions = getSupportedInstanceExtensions();
    const std::vector<std::string>& requestedExtensions = Upscaler::requestVulkanInstanceExtensions(supportedExtensions);

    // Vectorize the enabled extensions
    std::vector<const char*> enabledExtensions{};
    enabledExtensions.reserve(pCreateInfo->enabledExtensionCount + requestedExtensions.size());
    for (uint32_t i{}; i < pCreateInfo->enabledExtensionCount; ++i)
        enabledExtensions.emplace_back(pCreateInfo->ppEnabledExtensionNames[i]);
    for (const std::string& extension : requestedExtensions)
        if (std::ranges::find_if(enabledExtensions, [&extension](const char* str) { return strcmp(str, extension.c_str()) == 0; }) == enabledExtensions.end())
            enabledExtensions.push_back(extension.c_str());

    // Modify the createInfo.
    createInfo.enabledExtensionCount   = enabledExtensions.size();
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();

    // Create the Instance.
    VkResult result = m_vkCreateInstance(&createInfo, pAllocator, pInstance);
    if (result != VK_SUCCESS)
        result = m_vkCreateInstance(pCreateInfo, pAllocator, pInstance);
    temporaryInstance = *pInstance;
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
    for (auto [extensionName, specVersion] : extensionProperties) extensions.emplace_back(extensionName);
    return extensions;
}

VkResult Vulkan::Hook_vkCreateDevice(
  VkPhysicalDevice             physicalDevice,
  const VkDeviceCreateInfo*    pCreateInfo,
  const VkAllocationCallbacks* pAllocator,
  VkDevice*                    pDevice
) {
    loadInstanceFunctionPointers(temporaryInstance);

    VkDeviceCreateInfo createInfo = *pCreateInfo;

    // Find out which extensions are supported
    const std::vector<std::string>  supportedExtensions = getSupportedDeviceExtensions(physicalDevice);
    const std::vector<std::string>& requestedExtensions = Upscaler::requestVulkanDeviceExtensions(temporaryInstance, physicalDevice, supportedExtensions);
    temporaryInstance                                   = VK_NULL_HANDLE;

    // Vectorize the enabled extensions
    std::vector<const char*> enabledExtensions{};
    enabledExtensions.reserve(pCreateInfo->enabledExtensionCount + requestedExtensions.size());
    for (uint32_t i{}; i < pCreateInfo->enabledExtensionCount; ++i)
        enabledExtensions.emplace_back(pCreateInfo->ppEnabledExtensionNames[i]);
    for (const std::string& extension : requestedExtensions)
        if (std::ranges::find_if(enabledExtensions, [&extension](const char* str) { return strcmp(str, extension.c_str()) == 0; }) == enabledExtensions.end())
            enabledExtensions.push_back(extension.c_str());

    // Modify the createInfo.
    createInfo.enabledExtensionCount   = enabledExtensions.size();
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();

    // Create the Device
    VkResult result = m_vkCreateDevice(physicalDevice, &createInfo, pAllocator, pDevice);
    if (result != VK_SUCCESS)
        result = m_vkCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
    if (result == VK_SUCCESS)
        loadDeviceFunctionPointers(*pDevice);
    return result;
}

PFN_vkVoidFunction Vulkan::Hook_vkGetInstanceProcAddr(VkInstance t_instance, const char* pName) {
    if (pName == nullptr) return nullptr;
    if (strcmp(pName, "vkCreateInstance") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(&Hook_vkCreateInstance);
    if (strcmp(pName, "vkCreateDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Hook_vkCreateDevice);
    return m_vkGetInstanceProcAddr(t_instance, pName);
}

PFN_vkGetInstanceProcAddr
Vulkan::interceptInitialization(const PFN_vkGetInstanceProcAddr t_getInstanceProcAddr, void* /*unused*/) {
    m_vkGetInstanceProcAddr = t_getInstanceProcAddr;
    loadEarlyFunctionPointers();
    return Hook_vkGetInstanceProcAddr;
}

bool Vulkan::registerUnityInterfaces(IUnityInterfaces* t_unityInterfaces) {
    graphicsInterface = t_unityInterfaces->Get<IUnityGraphicsVulkanV2>();
    return graphicsInterface->AddInterceptInitialization(interceptInitialization, nullptr, 0);
}

IUnityGraphicsVulkanV2* Vulkan::getGraphicsInterface() {
    return graphicsInterface;
}

bool Vulkan::unregisterUnityInterfaces() {
    const bool result = graphicsInterface->RemoveInterceptInitialization(interceptInitialization);
    graphicsInterface = nullptr;
    return result;
}

VkImageView Vulkan::createImageView(VkImage image, const VkFormat format, const VkImageAspectFlags flags) {
    const VkImageViewCreateInfo createInfo {
      .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext    = nullptr,
      .flags    = 0x0U,
      .image    = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format   = format,
      .components = {
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
      },
      .subresourceRange = {
        .aspectMask     = flags,
        .baseMipLevel   = 0U,
        .levelCount     = 1U,
        .baseArrayLayer = 0U,
        .layerCount     = 1U,
      },
    };

    VkImageView view{VK_NULL_HANDLE};
    m_vkCreateImageView(graphicsInterface->Instance().device, &createInfo, nullptr, &view);
    return view;
}

void Vulkan::destroyImageView(VkImageView viewToDestroy) {
    if (viewToDestroy != VK_NULL_HANDLE) {
        m_vkDestroyImageView(graphicsInterface->Instance().device, viewToDestroy, nullptr);
        viewToDestroy = VK_NULL_HANDLE;
    }
}

PFN_vkGetDeviceProcAddr Vulkan::getDeviceProcAddr() {
    return m_vkGetDeviceProcAddr;
}
#endif