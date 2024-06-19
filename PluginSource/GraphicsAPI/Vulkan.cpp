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

PFN_vkCreateImageView        Vulkan::m_vkCreateImageView{VK_NULL_HANDLE};
PFN_vkDestroyImageView       Vulkan::m_vkDestroyImageView{VK_NULL_HANDLE};
PFN_vkCreateCommandPool      Vulkan::m_vkCreateCommandPool{VK_NULL_HANDLE};
PFN_vkAllocateCommandBuffers Vulkan::m_vkAllocateCommandBuffers{VK_NULL_HANDLE};
PFN_vkBeginCommandBuffer     Vulkan::m_vkBeginCommandBuffer{VK_NULL_HANDLE};
PFN_vkEndCommandBuffer       Vulkan::m_vkEndCommandBuffer{VK_NULL_HANDLE};
PFN_vkQueueSubmit            Vulkan::m_vkQueueSubmit{VK_NULL_HANDLE};
PFN_vkCreateFence            Vulkan::m_vkCreateFence{VK_NULL_HANDLE};
PFN_vkWaitForFences          Vulkan::m_vkWaitForFences{VK_NULL_HANDLE};
PFN_vkFreeCommandBuffers     Vulkan::m_vkFreeCommandBuffers{VK_NULL_HANDLE};
PFN_vkDestroyCommandPool     Vulkan::m_vkDestroyCommandPool{VK_NULL_HANDLE};

IUnityGraphicsVulkanV2* Vulkan::graphicsInterface{nullptr};

VkCommandPool Vulkan::commandPool{VK_NULL_HANDLE};

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
    m_vkCreateCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(m_vkGetDeviceProcAddr(device, "vkCreateCommandPool"));
    m_vkAllocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(m_vkGetDeviceProcAddr(device, "vkAllocateCommandBuffers"));
    m_vkBeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(m_vkGetDeviceProcAddr(device, "vkBeginCommandBuffer"));
    m_vkEndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(m_vkGetDeviceProcAddr(device, "vkEndCommandBuffer"));
    m_vkQueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(m_vkGetDeviceProcAddr(device, "vkQueueSubmit"));
    m_vkCreateFence = reinterpret_cast<PFN_vkCreateFence>(m_vkGetDeviceProcAddr(device, "vkCreateFence"));
    m_vkWaitForFences = reinterpret_cast<PFN_vkWaitForFences>(m_vkGetDeviceProcAddr(device, "vkWaitForFences"));
    m_vkFreeCommandBuffers = reinterpret_cast<PFN_vkFreeCommandBuffers>(m_vkGetDeviceProcAddr(device, "vkFreeCommandBuffers"));
    m_vkDestroyCommandPool = reinterpret_cast<PFN_vkDestroyCommandPool>(m_vkGetDeviceProcAddr(device, "vkDestroyCommandPool"));
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

bool Vulkan::initializeOneTimeSubmits() {
    const VkCommandPoolCreateInfo commandPoolCreateInfo{
      .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext            = nullptr,
      .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
      .queueFamilyIndex = graphicsInterface->Instance().queueFamilyIndex
    };
    return m_vkCreateCommandPool(graphicsInterface->Instance().device, &commandPoolCreateInfo, nullptr, &commandPool) == VK_SUCCESS;
}

VkCommandBuffer Vulkan::getOneTimeSubmitCommandBuffer() {
    const VkCommandBufferAllocateInfo commandBufferAllocateInfo {
      .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext              = nullptr,
      .commandPool        = commandPool,
      .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1
    };
    VkCommandBuffer buffer{VK_NULL_HANDLE};
    VkResult result = m_vkAllocateCommandBuffers(graphicsInterface->Instance().device, &commandBufferAllocateInfo, &buffer);
    if (result != VK_SUCCESS) return VK_NULL_HANDLE;
    constexpr VkCommandBufferBeginInfo commandBufferBeginInfo {
      .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext            = nullptr,
      .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = nullptr
    };
    return m_vkBeginCommandBuffer(buffer, &commandBufferBeginInfo) == VK_SUCCESS ? buffer : VK_NULL_HANDLE;
}

struct alignas(16) QueueAccessData {
    VkCommandBuffer commandBuffer;
    bool success;
};

void Vulkan::_submitOneTimeCommandBuffer(int /*unused*/, void* data) {
    auto*          qData = static_cast<QueueAccessData*>(data);
    const VkDevice device = graphicsInterface->Instance().device;
    qData->success = m_vkEndCommandBuffer(qData->commandBuffer) == VK_SUCCESS;
    if (!qData->success) return;
    constexpr VkFenceCreateInfo fenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0
      };
    VkFence fence{VK_NULL_HANDLE};
    qData->success = m_vkCreateFence(device, &fenceCreateInfo, nullptr, &fence) == VK_SUCCESS;
    if (!qData->success) return;
    const VkSubmitInfo submitInfo{
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext              = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers    = &qData->commandBuffer
      };
    qData->success = m_vkQueueSubmit(graphicsInterface->Instance().graphicsQueue, 1, &submitInfo, fence) == VK_SUCCESS;
    if (!qData->success) return;
    qData->success = m_vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS;
    if (!qData->success) return;
    m_vkFreeCommandBuffers(device, commandPool, 1, &qData->commandBuffer);
}

bool Vulkan::submitOneTimeSubmitCommandBuffer(VkCommandBuffer commandBuffer) {
    QueueAccessData data {commandBuffer, false};
    graphicsInterface->AccessQueue(&_submitOneTimeCommandBuffer, 0, &data, true);
    return data.success;
}

void Vulkan::shutdownOneTimeSubmits() {
    m_vkDestroyCommandPool(graphicsInterface->Instance().device, commandPool, nullptr);
    commandPool = VK_NULL_HANDLE;
}

PFN_vkGetDeviceProcAddr Vulkan::getDeviceProcAddr() {
    return m_vkGetDeviceProcAddr;
}
#endif