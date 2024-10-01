#include <FrameGenerator/FSR_FrameGenerator.hpp>
#ifdef ENABLE_VULKAN
#    include "Vulkan.hpp"

#    include <Upscaler/Upscaler.hpp>
#    ifdef ENABLE_DLSS
#        include <Upscaler/DLSS_Upscaler.hpp>
#    endif

#    include <IUnityGraphicsVulkan.h>

PFN_vkGetInstanceProcAddr    Vulkan::m_vkGetInstanceProcAddr{VK_NULL_HANDLE};
PFN_vkGetInstanceProcAddr    Vulkan::m_slGetInstanceProcAddr{VK_NULL_HANDLE};
PFN_vkCreateDevice           Vulkan::m_vkCreateDevice{VK_NULL_HANDLE};
PFN_vkCreateDevice           Vulkan::m_slCreateDevice{VK_NULL_HANDLE};
PFN_vkGetDeviceProcAddr      Vulkan::m_vkGetDeviceProcAddr{VK_NULL_HANDLE};
PFN_vkGetDeviceProcAddr      Vulkan::m_slGetDeviceProcAddr{VK_NULL_HANDLE};
PFN_vkCreateSwapchainKHR     Vulkan::m_vkCreateSwapchainKHR{VK_NULL_HANDLE};
PFN_vkCreateSwapchainKHR     Vulkan::m_slCreateSwapchainKHR{VK_NULL_HANDLE};
PFN_vkCreateSwapchainFFXAPI  Vulkan::m_fxCreateSwapchainKHR{VK_NULL_HANDLE};
PFN_vkDestroySwapchainKHR    Vulkan::m_vkDestroySwapchainKHR{VK_NULL_HANDLE};
PFN_vkDestroySwapchainKHR    Vulkan::m_slDestroySwapchainKHR{VK_NULL_HANDLE};
PFN_vkDestroySwapchainFFXAPI Vulkan::m_fxDestroySwapchainKHR{VK_NULL_HANDLE};
PFN_vkGetSwapchainImagesKHR  Vulkan::m_vkGetSwapchainImagesKHR{VK_NULL_HANDLE};
PFN_vkGetSwapchainImagesKHR  Vulkan::m_slGetSwapchainImagesKHR{VK_NULL_HANDLE};
PFN_vkGetSwapchainImagesKHR  Vulkan::m_fxGetSwapchainImagesKHR{VK_NULL_HANDLE};
PFN_vkAcquireNextImageKHR    Vulkan::m_vkAcquireNextImageKHR{VK_NULL_HANDLE};
PFN_vkAcquireNextImageKHR    Vulkan::m_slAcquireNextImageKHR{VK_NULL_HANDLE};
PFN_vkAcquireNextImageKHR    Vulkan::m_fxAcquireNextImageKHR{VK_NULL_HANDLE};
PFN_vkQueuePresentKHR        Vulkan::m_vkQueuePresentKHR{VK_NULL_HANDLE};
PFN_vkQueuePresentKHR        Vulkan::m_slQueuePresentKHR{VK_NULL_HANDLE};
PFN_vkQueuePresentKHR        Vulkan::m_fxQueuePresentKHR{VK_NULL_HANDLE};
PFN_vkSetHdrMetadataEXT      Vulkan::m_vkSetHdrMetadataEXT{VK_NULL_HANDLE};
PFN_vkSetHdrMetadataEXT      Vulkan::m_fxSetHdrMetadataEXT{VK_NULL_HANDLE};
// PFN_vkCreateWin32SurfaceKHR  Vulkan::m_vkCreateWin32SurfaceKHR{VK_NULL_HANDLE};
// PFN_vkDestroySurfaceKHR      Vulkan::m_vkDestroySurfaceKHR{VK_NULL_HANDLE};

PFN_vkGetPhysicalDeviceQueueFamilyProperties Vulkan::m_vkGetPhysicalDeviceQueueFamilyProperties{VK_NULL_HANDLE};
PFN_vkGetDeviceQueue                         Vulkan::m_vkGetDeviceQueue{VK_NULL_HANDLE};
PFN_vkDestroyImage                           Vulkan::m_vkDestroyImage{VK_NULL_HANDLE};
PFN_vkCreateImageView                        Vulkan::m_vkCreateImageView{VK_NULL_HANDLE};
PFN_vkDestroyImageView                       Vulkan::m_vkDestroyImageView{VK_NULL_HANDLE};
PFN_vkQueueSubmit                            Vulkan::m_vkQueueSubmit{VK_NULL_HANDLE};
// PFN_vkQueueSubmit2                           Vulkan::m_vkQueueSubmit2{VK_NULL_HANDLE};
// PFN_vkQueueWaitIdle                          Vulkan::m_vkQueueWaitIdle{VK_NULL_HANDLE};
// PFN_vkQueueBindSparse                        Vulkan::m_vkQueueBindSparse{VK_NULL_HANDLE};
// PFN_vkQueuePresentKHR                        Vulkan::m_vkQueuePresentKHR{VK_NULL_HANDLE};

IUnityGraphicsVulkanV2* Vulkan::graphicsInterface{nullptr};
uint64_t                Vulkan::SizeOfSwapchainToRecreate{};

PFN_vkVoidFunction Vulkan::hook_vkGetInstanceProcAddr(VkInstance instance, const char* name) {
    if (strcmp(name, "vkGetInstanceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkGetInstanceProcAddr);
    if (strcmp(name, "vkGetDeviceProcAddr") == 0) {
        m_vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(m_vkGetInstanceProcAddr(instance, name));
#    ifdef ENABLE_DLSS
        if (m_slGetInstanceProcAddr != VK_NULL_HANDLE) m_slGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(m_slGetInstanceProcAddr(instance, name));
#    endif
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkGetDeviceProcAddr);
    }
    if (strcmp(name, "vkCreateDevice") == 0) {
        m_vkCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(m_vkGetInstanceProcAddr(instance, name));
        m_slCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(m_slGetInstanceProcAddr(instance, name));
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkCreateDevice);
    }
    // if (strcmp(name, "vkCreateWin32SurfaceKHR") == 0) {
    //     m_vkCreateWin32SurfaceKHR = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(m_vkGetInstanceProcAddr(instance, name));
    //     return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkCreateWin32SurfaceKHR);
    // }
    // if (strcmp(name, "vkDestroySurfaceKHR") == 0) {
    //     m_vkDestroySurfaceKHR = reinterpret_cast<PFN_vkDestroySurfaceKHR>(m_vkGetInstanceProcAddr(instance, name));
    //     return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkDestroySurfaceKHR);
    // }
    if (strcmp(name, "vkGetPhysicalDeviceQueueFamilyProperties") == 0) return reinterpret_cast<PFN_vkVoidFunction>(m_vkGetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(m_vkGetInstanceProcAddr(instance, name)));
    if (strcmp(name, "vkGetDeviceQueue") == 0) return reinterpret_cast<PFN_vkVoidFunction>(m_vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(m_vkGetInstanceProcAddr(instance, name)));
    if (strcmp(name, "vkCreateImageView") == 0) return reinterpret_cast<PFN_vkVoidFunction>(m_vkCreateImageView = reinterpret_cast<PFN_vkCreateImageView>(m_vkGetInstanceProcAddr(instance, name)));
    if (strcmp(name, "vkDestroyImageView") == 0) return reinterpret_cast<PFN_vkVoidFunction>(m_vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(m_vkGetInstanceProcAddr(instance, name)));
    if (strcmp(name, "vkQueueSubmit") == 0) return reinterpret_cast<PFN_vkVoidFunction>(m_vkQueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(m_vkGetInstanceProcAddr(instance, name)));
    if (strcmp(name, "vkCreateSwapchainKHR") == 0) {
        m_vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(m_vkGetInstanceProcAddr(instance, name));
        m_slCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(m_slGetInstanceProcAddr(instance, name));
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkCreateSwapchainKHR);
    }
    if (strcmp(name, "vkDestroySwapchainKHR") == 0) {
        m_vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(m_vkGetInstanceProcAddr(instance, name));
        m_slDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(m_slGetInstanceProcAddr(instance, name));
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkDestroySwapchainKHR);
    }
    if (strcmp(name, "vkGetSwapchainImagesKHR") == 0) {
        m_vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(m_vkGetInstanceProcAddr(instance, name));
        m_slGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(m_slGetInstanceProcAddr(instance, name));
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkGetSwapchainImagesKHR);
    }
    if (strcmp(name, "vkAcquireNextImageKHR") == 0) {
        m_vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(m_vkGetInstanceProcAddr(instance, name));
        m_slAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(m_slGetInstanceProcAddr(instance, name));
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkAcquireNextImageKHR);
    }
    if (strcmp(name, "vkQueuePresentKHR") == 0) {
        m_vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(m_vkGetInstanceProcAddr(instance, name));
        m_slQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(m_slGetInstanceProcAddr(instance, name));
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkQueuePresentKHR);
    }
    if (strcmp(name, "vkSetHdrMetadataEXT") == 0) {
        m_vkSetHdrMetadataEXT = reinterpret_cast<PFN_vkSetHdrMetadataEXT>(m_vkGetInstanceProcAddr(instance, name));
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkSetHdrMetadataEXT);
    }
#    ifdef ENABLE_DLSS
    if (m_slGetInstanceProcAddr != VK_NULL_HANDLE)
        return m_slGetInstanceProcAddr(instance, name);
#    endif
    return m_vkGetInstanceProcAddr(instance, name);
}

PFN_vkVoidFunction Vulkan::hook_vkGetDeviceProcAddr(VkDevice device, const char* name) {
    if (strcmp(name, "vkGetDeviceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkGetDeviceProcAddr);
    if (strcmp(name, "vkGetPhysicalDeviceQueueFamilyProperties") == 0) return reinterpret_cast<PFN_vkVoidFunction>(m_vkGetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(m_vkGetDeviceProcAddr(device, name)));
    if (strcmp(name, "vkGetDeviceQueue") == 0) return reinterpret_cast<PFN_vkVoidFunction>(m_vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(m_vkGetDeviceProcAddr(device, name)));
    if (strcmp(name, "vkCreateImageView") == 0) return reinterpret_cast<PFN_vkVoidFunction>(m_vkCreateImageView = reinterpret_cast<PFN_vkCreateImageView>(m_vkGetDeviceProcAddr(device, name)));
    if (strcmp(name, "vkDestroyImageView") == 0) return reinterpret_cast<PFN_vkVoidFunction>(m_vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(m_vkGetDeviceProcAddr(device, name)));
    if (strcmp(name, "vkQueueSubmit") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(m_vkQueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(m_vkGetDeviceProcAddr(device, name)));
    if (strcmp(name, "vkCreateSwapchainKHR") == 0) {
        m_vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(m_vkGetDeviceProcAddr(device, name));
        m_slCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(m_slGetDeviceProcAddr(device, name));
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkCreateSwapchainKHR);
    }
    if (strcmp(name, "vkDestroySwapchainKHR") == 0) {
        m_vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(m_vkGetDeviceProcAddr(device, name));
        m_slDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(m_slGetDeviceProcAddr(device, name));
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkDestroySwapchainKHR);
    }
    if (strcmp(name, "vkGetSwapchainImagesKHR") == 0) {
        m_vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(m_vkGetDeviceProcAddr(device, name));
        m_slGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(m_slGetDeviceProcAddr(device, name));
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkGetSwapchainImagesKHR);
    }
    if (strcmp(name, "vkAcquireNextImageKHR") == 0) {
        m_vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(m_vkGetDeviceProcAddr(device, name));
        m_slAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(m_slGetDeviceProcAddr(device, name));
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkAcquireNextImageKHR);
    }
    if (strcmp(name, "vkQueuePresentKHR") == 0) {
        m_vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(m_vkGetDeviceProcAddr(device, name));
        m_slQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(m_slGetDeviceProcAddr(device, name));
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkQueuePresentKHR);
    }
    if (strcmp(name, "vkSetHdrMetadataEXT") == 0) {
        m_vkSetHdrMetadataEXT = reinterpret_cast<PFN_vkSetHdrMetadataEXT>(m_vkGetDeviceProcAddr(device, name));
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkSetHdrMetadataEXT);
    }
#    ifdef ENABLE_DLSS
    if (m_slGetDeviceProcAddr != VK_NULL_HANDLE) return m_slGetDeviceProcAddr(device, name);
#    endif
    return m_vkGetDeviceProcAddr(device, name);
}

VkResult Vulkan::hook_vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) {
    VkDeviceCreateInfo createInfo = *pCreateInfo;

    const std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueFamilyIndex = 0,
        .queueCount = 4,
        .pQueuePriorities = nullptr
    }};

    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = queueCreateInfos.size();

    if (m_slCreateDevice != VK_NULL_HANDLE)
        return m_slCreateDevice(physicalDevice, &createInfo, pAllocator, pDevice);
    return m_vkCreateDevice(physicalDevice, &createInfo, pAllocator, pDevice);
}

VkResult Vulkan::hook_vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) {
    uint64_t swapchainSize = static_cast<uint64_t>(pCreateInfo->imageExtent.width) << 32U | pCreateInfo->imageExtent.height;
    const VkResult result = m_vkCreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
    if (FSR_FrameGenerator::ownsSwapchain(*pSwapchain))
        return m_fxCreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain, FSR_FrameGenerator::getContext());
    if (Plugin::frameGenerationProvider != Plugin::None && SizeOfSwapchainToRecreate == swapchainSize)
        switch (Plugin::frameGenerationProvider) {
#    ifdef ENABLE_DLSS
            case Plugin::DLSS:
                if (m_slCreateSwapchainKHR != VK_NULL_HANDLE) return m_slCreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
#    endif
#    ifdef ENABLE_FSR
            case Plugin::FSR: {
                FSR_FrameGenerator::createSwapchain(pSwapchain, pCreateInfo, pAllocator, &m_fxCreateSwapchainKHR, &m_fxDestroySwapchainKHR, &m_fxGetSwapchainImagesKHR, &m_fxAcquireNextImageKHR, &m_fxQueuePresentKHR, &m_fxSetHdrMetadataEXT, nullptr);
                if (*pSwapchain == VK_NULL_HANDLE) return m_vkCreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
                return VK_SUCCESS;
            }
#    endif
            case Plugin::None:
                default: return m_vkCreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
        }
    FrameGenerator::addMapping(swapchainSize, *pSwapchain);
    if (FrameGenerator::getSwapchain(SizeOfSwapchainToRecreate) == *pSwapchain) SizeOfSwapchainToRecreate = 0;
    return result;
}

void Vulkan::hook_vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator) {
    FrameGenerator::removeMapping(swapchain);
    if (FSR_FrameGenerator::ownsSwapchain(swapchain))
        return FSR_FrameGenerator::destroySwapchain();
    return m_vkDestroySwapchainKHR(device, swapchain, pAllocator);
}

VkResult Vulkan::hook_vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages) {
    if (FSR_FrameGenerator::ownsSwapchain(swapchain))
        return m_fxGetSwapchainImagesKHR(device, swapchain, pSwapchainImageCount, pSwapchainImages);
    return m_vkGetSwapchainImagesKHR(device, swapchain, pSwapchainImageCount, pSwapchainImages);
}

VkResult Vulkan::hook_vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, const uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex) {
    VkResult   result{VK_SUCCESS};
    const bool isFsrSwapchain = FSR_FrameGenerator::ownsSwapchain(swapchain);
    if (isFsrSwapchain) result = m_fxAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
    else result = m_vkAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
    if ((isFsrSwapchain && Plugin::frameGenerationProvider != Plugin::FSR) || FrameGenerator::getSwapchain(SizeOfSwapchainToRecreate) == swapchain)
        return VK_ERROR_OUT_OF_DATE_KHR;
    return result;
}

VkResult Vulkan::hook_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    VkResult result{VK_SUCCESS};
    bool isFsrSwapchain = false;
    if (pPresentInfo->swapchainCount == 0) return result;
    if (pPresentInfo->swapchainCount == 1 && (isFsrSwapchain = FSR_FrameGenerator::ownsSwapchain(pPresentInfo->pSwapchains[0]))) result = m_fxQueuePresentKHR(queue, pPresentInfo);
    else result = m_vkQueuePresentKHR(queue, pPresentInfo);
    if ((isFsrSwapchain && Plugin::frameGenerationProvider != Plugin::FSR) || FrameGenerator::getSwapchain(SizeOfSwapchainToRecreate) == pPresentInfo->pSwapchains[0])
        return VK_ERROR_OUT_OF_DATE_KHR;
    return result;
}

void Vulkan::hook_vkSetHdrMetadataEXT(VkDevice device, const uint32_t swapchainCount, const VkSwapchainKHR* pSwapchains, const VkHdrMetadataEXT* pMetadata) {
    const PFN_vkSetHdrMetadataEXT* setHdrMetadataEXT = &m_vkSetHdrMetadataEXT;
    for (uint32_t i{}; i < swapchainCount; ++i) {
        if (FSR_FrameGenerator::ownsSwapchain(pSwapchains[i])) {
            setHdrMetadataEXT = &m_fxSetHdrMetadataEXT;
            break;
        }
    }
    return (*setHdrMetadataEXT)(device, swapchainCount, pSwapchains, pMetadata);
}

// VkResult Vulkan::hook_vkDeviceWaitIdle() {
// }

// VkResult Vulkan::hook_vkCreateWin32SurfaceKHR(VkInstance instance, const VkWin32SurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) {
//     return m_vkCreateWin32SurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);
// }

// void Vulkan::hook_vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator) {
//     return m_vkDestroySurfaceKHR(instance, surface, pAllocator);
// }

PFN_vkGetInstanceProcAddr Vulkan::interceptInitialization(PFN_vkGetInstanceProcAddr t_getInstanceProcAddr, void* /*unused*/) {
    m_vkGetInstanceProcAddr = t_getInstanceProcAddr;
#    ifdef ENABLE_DLSS
    DLSS_Upscaler::load(VULKAN, &const_cast<const void*&>(reinterpret_cast<void*&>(m_slGetInstanceProcAddr)));
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

void Vulkan::requestSwapchainRecreationBySize(uint64_t size) {
    SizeOfSwapchainToRecreate = size;
}

std::vector<VkQueue> Vulkan::getQueues() {
    std::vector<VkQueue> queues(4);
    for (uint32_t i{}; i < queues.size(); ++i) m_vkGetDeviceQueue(graphicsInterface->Instance().device, 0, i, &queues[i]);
    return queues;
}

VkResult Vulkan::createSwapchain(const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) {
    return m_vkCreateSwapchainKHR(graphicsInterface->Instance().device, pCreateInfo, pAllocator, pSwapchain);
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

VkResult Vulkan::submit(const uint32_t submitCount, const VkSubmitInfo* submitInfo, VkFence fence) {
    return m_vkQueueSubmit(graphicsInterface->Instance().graphicsQueue, submitCount, submitInfo, fence);
}

PFN_vkGetDeviceProcAddr Vulkan::getDeviceProcAddr() {
    return m_vkGetDeviceProcAddr;
}
#endif