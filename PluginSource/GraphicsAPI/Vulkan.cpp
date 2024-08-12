#ifdef ENABLE_VULKAN
#    include "Vulkan.hpp"

#    include <Upscaler/Upscaler.hpp>
#    ifdef ENABLE_DLSS
#        include <Upscaler/DLSS.hpp>
#    endif

#    include <IUnityGraphicsVulkan.h>

PFN_vkGetInstanceProcAddr Vulkan::m_vkGetInstanceProcAddr{VK_NULL_HANDLE};
PFN_vkGetDeviceProcAddr Vulkan::m_vkGetDeviceProcAddr{VK_NULL_HANDLE};

PFN_vkCreateImageView Vulkan::m_vkCreateImageView{VK_NULL_HANDLE};
PFN_vkDestroyImageView Vulkan::m_vkDestroyImageView{VK_NULL_HANDLE};

IUnityGraphicsVulkanV2* Vulkan::graphicsInterface{nullptr};

PFN_vkGetInstanceProcAddr Vulkan::interceptInitialization(PFN_vkGetInstanceProcAddr t_getInstanceProcAddr, void* /*unused*/) {
#    ifdef ENABLE_DLSS
    auto& f = const_cast<const void*&>(reinterpret_cast<void*&>(m_vkGetInstanceProcAddr));
    DLSS::load(VULKAN, &f);
    if (m_vkGetInstanceProcAddr == nullptr)
#    endif
        m_vkGetInstanceProcAddr = t_getInstanceProcAddr;
    return m_vkGetInstanceProcAddr;
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
    if (m_vkGetDeviceProcAddr == VK_NULL_HANDLE) m_vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(m_vkGetInstanceProcAddr(getGraphicsInterface()->Instance().instance, "vkGetDeviceProcAddr"));
    if (m_vkCreateImageView == VK_NULL_HANDLE) m_vkCreateImageView = reinterpret_cast<PFN_vkCreateImageView>(m_vkGetDeviceProcAddr(getGraphicsInterface()->Instance().device, "vkCreateImageView"));
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
    if (m_vkGetDeviceProcAddr == VK_NULL_HANDLE) m_vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(m_vkGetInstanceProcAddr(getGraphicsInterface()->Instance().instance, "vkGetDeviceProcAddr"));
    if (m_vkDestroyImageView == VK_NULL_HANDLE) m_vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(m_vkGetDeviceProcAddr(getGraphicsInterface()->Instance().device, "vkDestroyImageView"));
    if (viewToDestroy != VK_NULL_HANDLE) m_vkDestroyImageView(graphicsInterface->Instance().device, viewToDestroy, nullptr);
    viewToDestroy = VK_NULL_HANDLE;
}

PFN_vkGetDeviceProcAddr Vulkan::getDeviceProcAddr() {
    return m_vkGetDeviceProcAddr;
}
#endif