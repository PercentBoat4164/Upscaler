#ifdef ENABLE_VULKAN
#    include "Vulkan.hpp"

#    include <Upscaler/Upscaler.hpp>
#    ifdef ENABLE_DLSS
#        include <Upscaler/DLSS.hpp>
#    endif

#    include <IUnityGraphicsVulkan.h>

PFN_vkGetInstanceProcAddr Vulkan::m_vkGetInstanceProcAddr{VK_NULL_HANDLE};
PFN_vkGetInstanceProcAddr Vulkan::m_nvGetInstanceProcAddr{VK_NULL_HANDLE};
PFN_vkGetDeviceProcAddr   Vulkan::m_vkGetDeviceProcAddr{VK_NULL_HANDLE};
PFN_vkGetDeviceProcAddr   Vulkan::m_nvGetDeviceProcAddr{VK_NULL_HANDLE};
PFN_vkCreateSwapchainKHR  Vulkan::m_vkCreateSwapchainKHR{VK_NULL_HANDLE};
PFN_vkCreateSwapchainKHR  Vulkan::m_nvCreateSwapchainKHR{VK_NULL_HANDLE};

PFN_vkCreateImageView Vulkan::m_vkCreateImageView{VK_NULL_HANDLE};
PFN_vkDestroyImageView Vulkan::m_vkDestroyImageView{VK_NULL_HANDLE};

IUnityGraphicsVulkanV2* Vulkan::graphicsInterface{nullptr};

PFN_vkVoidFunction Vulkan::hook_vkGetInstanceProcAddr(VkInstance instance, const char* name) {
    if (strcmp(name, "vkGetDeviceProcAddr") == 0) {
        m_vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(m_vkGetInstanceProcAddr(instance, name));
#    ifdef ENABLE_DLSS
        if (m_nvGetInstanceProcAddr != VK_NULL_HANDLE) m_nvGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(m_nvGetInstanceProcAddr(instance, name));
#    endif
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkGetDeviceProcAddr);
    }
#    ifdef ENABLE_DLSS
    if (m_nvGetInstanceProcAddr != VK_NULL_HANDLE) return m_nvGetInstanceProcAddr(instance, name);
#    endif
    return m_vkGetInstanceProcAddr(instance, name);
}

PFN_vkVoidFunction Vulkan::hook_vkGetDeviceProcAddr(VkDevice device, const char* name) {
    if (strcmp(name, "vkCreateSwapchainKHR") == 0) {
        m_vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(m_vkGetDeviceProcAddr(device, name));
        m_nvCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(m_nvGetDeviceProcAddr(device, name));
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkCreateSwapchainKHR);
    }
#    ifdef ENABLE_DLSS
    if (m_nvGetDeviceProcAddr != VK_NULL_HANDLE) return m_nvGetDeviceProcAddr(device, name);
#    endif
    return m_nvGetDeviceProcAddr(device, name);
}

VkResult Vulkan::hook_vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) {
    switch (Plugin::frameGenerationProvider) {
#    ifdef ENABLE_DLSS
        case Plugin::DLSS: if (m_nvCreateSwapchainKHR != VK_NULL_HANDLE) return m_nvCreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
#    endif
#    ifdef ENABLE_FSR3
        case Plugin::FSR: {
            // Create new frame generation context (separate from upscaling context)
            // Get function pointers for new swapchain.
            // In hooks for swapchain functions call FSR functions if swapchain is an FSR swapchain (Plugin::frameGenerationProvider)
        }
#    endif
        case Plugin::None:
        default: return m_vkCreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
    }
}

PFN_vkGetInstanceProcAddr Vulkan::interceptInitialization(PFN_vkGetInstanceProcAddr t_getInstanceProcAddr, void* /*unused*/) {
    m_vkGetInstanceProcAddr = t_getInstanceProcAddr;
#    ifdef ENABLE_DLSS
    DLSS::load(VULKAN, &const_cast<const void*&>(reinterpret_cast<void*&>(m_nvGetInstanceProcAddr)));
#    endif
    return &hook_vkGetInstanceProcAddr;
}

bool Vulkan::registerUnityInterfaces(IUnityInterfaces* t_unityInterfaces) {
    graphicsInterface = t_unityInterfaces->Get<IUnityGraphicsVulkanV2>();
    return graphicsInterface->AddInterceptInitialization(interceptInitialization, nullptr, kUnityVulkanInitCallbackMaxPriority);
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
      .components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
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
    if (viewToDestroy != VK_NULL_HANDLE) m_vkDestroyImageView(graphicsInterface->Instance().device, viewToDestroy, nullptr);
    viewToDestroy = VK_NULL_HANDLE;
}

PFN_vkGetDeviceProcAddr Vulkan::getDeviceProcAddr() {
    return m_nvGetDeviceProcAddr;
}
#endif