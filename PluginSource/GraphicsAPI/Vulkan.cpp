#ifdef ENABLE_VULKAN
#    include "Vulkan.hpp"

#    include <Upscaler/Upscaler.hpp>
#    ifdef ENABLE_DLSS
#        include <Upscaler/DLSS_Upscaler.hpp>
#    endif
#    ifdef ENABLE_FSR
#        include <FrameGenerator/FSR_FrameGenerator.hpp>
#    endif

#    include <IUnityGraphicsVulkan.h>

#    define VQS_IMPLEMENTATION
#    include <vk_queue_selector.h>

#    include <iostream>

PFN_vkGetInstanceProcAddr    Vulkan::m_vkGetInstanceProcAddr{VK_NULL_HANDLE};
PFN_vkCreateInstance         Vulkan::m_vkCreateInstance{VK_NULL_HANDLE};
PFN_vkCreateDevice           Vulkan::m_vkCreateDevice{VK_NULL_HANDLE};
PFN_vkGetDeviceProcAddr      Vulkan::m_vkGetDeviceProcAddr{VK_NULL_HANDLE};
PFN_vkCreateSwapchainKHR     Vulkan::m_vkCreateSwapchainKHR{VK_NULL_HANDLE};
PFN_vkDestroySwapchainKHR    Vulkan::m_vkDestroySwapchainKHR{VK_NULL_HANDLE};
PFN_vkGetSwapchainImagesKHR  Vulkan::m_vkGetSwapchainImagesKHR{VK_NULL_HANDLE};
PFN_vkAcquireNextImageKHR    Vulkan::m_vkAcquireNextImageKHR{VK_NULL_HANDLE};
PFN_vkQueuePresentKHR        Vulkan::m_vkQueuePresentKHR{VK_NULL_HANDLE};
PFN_vkSetHdrMetadataEXT      Vulkan::m_vkSetHdrMetadataEXT{VK_NULL_HANDLE};
PFN_vkCreateWin32SurfaceKHR  Vulkan::m_vkCreateWin32SurfaceKHR{VK_NULL_HANDLE};
PFN_vkDestroySurfaceKHR      Vulkan::m_vkDestroySurfaceKHR{VK_NULL_HANDLE};
#ifdef ENABLE_FSR
PFN_vkCreateSwapchainFFXAPI  Vulkan::m_fxCreateSwapchainKHR{VK_NULL_HANDLE};
PFN_vkDestroySwapchainFFXAPI Vulkan::m_fxDestroySwapchainKHR{VK_NULL_HANDLE};
PFN_vkGetSwapchainImagesKHR  Vulkan::m_fxGetSwapchainImagesKHR{VK_NULL_HANDLE};
PFN_vkAcquireNextImageKHR    Vulkan::m_fxAcquireNextImageKHR{VK_NULL_HANDLE};
PFN_vkQueuePresentKHR        Vulkan::m_fxQueuePresentKHR{VK_NULL_HANDLE};
PFN_vkSetHdrMetadataEXT      Vulkan::m_fxSetHdrMetadataEXT{VK_NULL_HANDLE};
#endif
#ifdef ENABLE_DLSS
PFN_vkGetInstanceProcAddr   Vulkan::m_slGetInstanceProcAddr{VK_NULL_HANDLE};
PFN_vkCreateInstance        Vulkan::m_slCreateInstance{VK_NULL_HANDLE};
PFN_vkCreateDevice          Vulkan::m_slCreateDevice{VK_NULL_HANDLE};
PFN_vkGetDeviceProcAddr     Vulkan::m_slGetDeviceProcAddr{VK_NULL_HANDLE};
PFN_vkCreateSwapchainKHR    Vulkan::m_slCreateSwapchainKHR{VK_NULL_HANDLE};
PFN_vkDestroySwapchainKHR   Vulkan::m_slDestroySwapchainKHR{VK_NULL_HANDLE};
PFN_vkGetSwapchainImagesKHR Vulkan::m_slGetSwapchainImagesKHR{VK_NULL_HANDLE};
PFN_vkAcquireNextImageKHR   Vulkan::m_slAcquireNextImageKHR{VK_NULL_HANDLE};
PFN_vkQueuePresentKHR       Vulkan::m_slQueuePresentKHR{VK_NULL_HANDLE};
#endif
PFN_vkGetPhysicalDeviceQueueFamilyProperties Vulkan::m_vkGetPhysicalDeviceQueueFamilyProperties{VK_NULL_HANDLE};
PFN_vkGetPhysicalDeviceSurfaceSupportKHR     Vulkan::m_vkGetPhysicalDeviceSurfaceSupportKHR{VK_NULL_HANDLE};
PFN_vkGetDeviceQueue                         Vulkan::m_vkGetDeviceQueue{VK_NULL_HANDLE};
PFN_vkDestroyImage                           Vulkan::m_vkDestroyImage{VK_NULL_HANDLE};
PFN_vkCreateImageView                        Vulkan::m_vkCreateImageView{VK_NULL_HANDLE};
PFN_vkDestroyImageView                       Vulkan::m_vkDestroyImageView{VK_NULL_HANDLE};

VkInstance Vulkan::instance{VK_NULL_HANDLE};
IUnityGraphicsVulkanV2* Vulkan::graphicsInterface{nullptr};
HWND                    Vulkan::HWNDToIntercept{nullptr};
VkSurfaceKHR            Vulkan::surfaceToIntercept{VK_NULL_HANDLE};
VkSwapchainKHR          Vulkan::swapchainToIntercept{VK_NULL_HANDLE};

PFN_vkVoidFunction Vulkan::hook_vkGetInstanceProcAddr(VkInstance instance, const char* name) {
    if (strcmp(name, "vkGetInstanceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkGetInstanceProcAddr);
    if (strcmp(name, "vkGetDeviceProcAddr") == 0) {
        m_vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(m_vkGetInstanceProcAddr(instance, name));
#    ifdef ENABLE_DLSS
        if (m_slGetInstanceProcAddr != VK_NULL_HANDLE) m_slGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(m_slGetInstanceProcAddr(instance, name));
#    endif
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkGetDeviceProcAddr);
    }
    if (strcmp(name, "vkCreateInstance") == 0) {
        m_vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(m_vkGetInstanceProcAddr(instance, name));
#    ifdef ENABLE_DLSS
        if (m_slGetInstanceProcAddr != VK_NULL_HANDLE) m_slCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(m_slGetInstanceProcAddr(instance, name));
#    endif
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkCreateInstance);
    }
    if (strcmp(name, "vkCreateDevice") == 0) {
        m_vkCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(m_vkGetInstanceProcAddr(instance, name));
#    ifdef ENABLE_DLSS
        if (m_slGetInstanceProcAddr != VK_NULL_HANDLE) m_slCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(m_slGetInstanceProcAddr(instance, name));
#    endif
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkCreateDevice);
    }
    if (strcmp(name, "vkCreateWin32SurfaceKHR") == 0) {
        m_vkCreateWin32SurfaceKHR = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(m_vkGetInstanceProcAddr(instance, name));
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkCreateWin32SurfaceKHR);
    }
    if (strcmp(name, "vkDestroySurfaceKHR") == 0) { return reinterpret_cast<PFN_vkVoidFunction>(m_vkDestroySurfaceKHR = reinterpret_cast<PFN_vkDestroySurfaceKHR>(m_vkGetInstanceProcAddr(instance, name))); }
    if (strcmp(name, "vkGetPhysicalDeviceQueueFamilyProperties") == 0) return reinterpret_cast<PFN_vkVoidFunction>(m_vkGetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(m_vkGetInstanceProcAddr(instance, name)));
    if (strcmp(name, "vkGetPhysicalDeviceSurfaceSupportKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(m_vkGetPhysicalDeviceSurfaceSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(m_vkGetInstanceProcAddr(instance, name)));
    if (strcmp(name, "vkGetDeviceQueue") == 0) return reinterpret_cast<PFN_vkVoidFunction>(m_vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(m_vkGetInstanceProcAddr(instance, name)));
    if (strcmp(name, "vkCreateImageView") == 0) return reinterpret_cast<PFN_vkVoidFunction>(m_vkCreateImageView = reinterpret_cast<PFN_vkCreateImageView>(m_vkGetInstanceProcAddr(instance, name)));
    if (strcmp(name, "vkDestroyImageView") == 0) return reinterpret_cast<PFN_vkVoidFunction>(m_vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(m_vkGetInstanceProcAddr(instance, name)));
    if (strcmp(name, "vkCreateSwapchainKHR") == 0) {
        m_vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(m_vkGetInstanceProcAddr(instance, name));
#    ifdef ENABLE_DLSS
        if (m_slGetInstanceProcAddr != VK_NULL_HANDLE) m_slCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(m_slGetInstanceProcAddr(instance, name));
#    endif
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkCreateSwapchainKHR);
    }
    if (strcmp(name, "vkDestroySwapchainKHR") == 0) {
        m_vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(m_vkGetInstanceProcAddr(instance, name));
#    ifdef ENABLE_DLSS
        if (m_slGetInstanceProcAddr != VK_NULL_HANDLE) m_slDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(m_slGetInstanceProcAddr(instance, name));
#    endif
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkDestroySwapchainKHR);
    }
    if (strcmp(name, "vkGetSwapchainImagesKHR") == 0) {
        m_vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(m_vkGetInstanceProcAddr(instance, name));
#    ifdef ENABLE_DLSS
        if (m_slGetInstanceProcAddr != VK_NULL_HANDLE) m_slGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(m_slGetInstanceProcAddr(instance, name));
#    endif
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkGetSwapchainImagesKHR);
    }
    if (strcmp(name, "vkAcquireNextImageKHR") == 0) {
        m_vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(m_vkGetInstanceProcAddr(instance, name));
#    ifdef ENABLE_DLSS
        if (m_slGetInstanceProcAddr != VK_NULL_HANDLE) m_slAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(m_slGetInstanceProcAddr(instance, name));
#    endif
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkAcquireNextImageKHR);
    }
    if (strcmp(name, "vkQueuePresentKHR") == 0) {
        m_vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(m_vkGetInstanceProcAddr(instance, name));
#    ifdef ENABLE_DLSS
        if (m_slGetInstanceProcAddr != VK_NULL_HANDLE) m_slQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(m_slGetInstanceProcAddr(instance, name));
#    endif
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkQueuePresentKHR);
    }
    if (strcmp(name, "vkSetHdrMetadataEXT") == 0) {
        m_vkSetHdrMetadataEXT = reinterpret_cast<PFN_vkSetHdrMetadataEXT>(m_vkGetInstanceProcAddr(instance, name));
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkSetHdrMetadataEXT);
    }
#    ifdef ENABLE_DLSS
    if (m_slGetInstanceProcAddr != VK_NULL_HANDLE) return m_slGetInstanceProcAddr(instance, name);
#    endif
    return m_vkGetInstanceProcAddr(instance, name);
}

PFN_vkVoidFunction Vulkan::hook_vkGetDeviceProcAddr(VkDevice device, const char* name) {
    if (strcmp(name, "vkGetDeviceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkGetDeviceProcAddr);
    if (strcmp(name, "vkGetPhysicalDeviceQueueFamilyProperties") == 0) return reinterpret_cast<PFN_vkVoidFunction>(m_vkGetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(m_vkGetDeviceProcAddr(device, name)));
    if (strcmp(name, "vkGetDeviceQueue") == 0) return reinterpret_cast<PFN_vkVoidFunction>(m_vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(m_vkGetDeviceProcAddr(device, name)));
    if (strcmp(name, "vkCreateImageView") == 0) return reinterpret_cast<PFN_vkVoidFunction>(m_vkCreateImageView = reinterpret_cast<PFN_vkCreateImageView>(m_vkGetDeviceProcAddr(device, name)));
    if (strcmp(name, "vkDestroyImageView") == 0) return reinterpret_cast<PFN_vkVoidFunction>(m_vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(m_vkGetDeviceProcAddr(device, name)));
    if (strcmp(name, "vkCreateWin32SurfaceKHR") == 0) {
        m_vkCreateWin32SurfaceKHR = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(m_vkGetDeviceProcAddr(device, name));
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkCreateWin32SurfaceKHR);
    }
    if (strcmp(name, "vkCreateSwapchainKHR") == 0) {
        m_vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(m_vkGetDeviceProcAddr(device, name));
#    ifdef ENABLE_DLSS
       if (m_slGetInstanceProcAddr != VK_NULL_HANDLE)  m_slCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(m_slGetDeviceProcAddr(device, name));
#    endif
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkCreateSwapchainKHR);
    }
    if (strcmp(name, "vkDestroySwapchainKHR") == 0) {
        m_vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(m_vkGetDeviceProcAddr(device, name));
#    ifdef ENABLE_DLSS
        if (m_slGetInstanceProcAddr != VK_NULL_HANDLE) m_slDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(m_slGetDeviceProcAddr(device, name));
#    endif
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkDestroySwapchainKHR);
    }
    if (strcmp(name, "vkGetSwapchainImagesKHR") == 0) {
        m_vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(m_vkGetDeviceProcAddr(device, name));
#    ifdef ENABLE_DLSS
        if (m_slGetInstanceProcAddr != VK_NULL_HANDLE) m_slGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(m_slGetDeviceProcAddr(device, name));
#    endif
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkGetSwapchainImagesKHR);
    }
    if (strcmp(name, "vkAcquireNextImageKHR") == 0) {
        m_vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(m_vkGetDeviceProcAddr(device, name));
#    ifdef ENABLE_DLSS
        if (m_slGetInstanceProcAddr != VK_NULL_HANDLE) m_slAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(m_slGetDeviceProcAddr(device, name));
#    endif
        return reinterpret_cast<PFN_vkVoidFunction>(&hook_vkAcquireNextImageKHR);
    }
    if (strcmp(name, "vkQueuePresentKHR") == 0) {
        m_vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(m_vkGetDeviceProcAddr(device, name));
#    ifdef ENABLE_DLSS
        if (m_slGetInstanceProcAddr != VK_NULL_HANDLE) m_slQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(m_slGetDeviceProcAddr(device, name));
#    endif
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

VkSurfaceKHR Vulkan::createDummySurface(void*& hwnd) {
#    ifdef WIN32
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    hwnd = CreateWindow("STATIC", "DummyWindow", 0, 0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);
    VkSurfaceKHR                      surface{VK_NULL_HANDLE};
    const VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .hinstance = hInstance,
        .hwnd = static_cast<HWND>(hwnd)
    };
    m_vkCreateWin32SurfaceKHR(instance, &win32SurfaceCreateInfo, nullptr, &surface);
    return surface;
#    endif
}

void Vulkan::destroyDummySurface(void* hwnd, VkSurfaceKHR dummySurface) {
    m_vkDestroySurfaceKHR(instance, dummySurface, nullptr);
#    ifdef WIN32
    DestroyWindow(static_cast<HWND>(hwnd));
#    endif
}

VkResult Vulkan::hook_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) {
    VkResult result{VK_SUCCESS};
#ifdef ENABLE_DLSS
    if (m_slCreateInstance != VK_NULL_HANDLE) result = m_slCreateInstance(pCreateInfo, pAllocator, pInstance);
    else
#endif
        result = m_vkCreateInstance(pCreateInfo, pAllocator, pInstance);
    instance = *pInstance;
    return result;
}

VkResult Vulkan::hook_vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) {
    static VkDeviceCreateInfo createInfo = *pCreateInfo;
    void* hwnd = nullptr;
    const std::array asyncRequirements {
      VqsQueueRequirements{VK_QUEUE_TRANSFER_BIT, 0.9F, VK_NULL_HANDLE},
      VqsQueueRequirements{0,                     1.0F, createDummySurface(hwnd)},
      VqsQueueRequirements{VK_QUEUE_COMPUTE_BIT,  1.0F, VK_NULL_HANDLE}
    };
    const std::array noAsyncRequirements {
        asyncRequirements[0],
        asyncRequirements[1]
    };

    VqsVulkanFunctions vkFuncs {
        .vkGetPhysicalDeviceQueueFamilyProperties = [](VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties) {
            m_vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
            if (pQueueFamilyProperties == nullptr) return;
            for (uint32_t i{}; i < createInfo.queueCreateInfoCount; ++i) {
                const VkDeviceQueueCreateInfo& queueCreateInfo = createInfo.pQueueCreateInfos[i];
                pQueueFamilyProperties[queueCreateInfo.queueFamilyIndex].queueCount -= queueCreateInfo.queueCount;
            }
        },
        .vkGetPhysicalDeviceSurfaceSupportKHR = m_vkGetPhysicalDeviceSurfaceSupportKHR
    };

    VqsQueryCreateInfo queryCreateInfo {
        .physicalDevice = physicalDevice,
        .queueRequirementCount = std::size(asyncRequirements),
        .pQueueRequirements = asyncRequirements.data(),
        .pVulkanFunctions = &vkFuncs
    };

    VqsQuery query{VK_NULL_HANDLE};
    vqsCreateQuery(&queryCreateInfo, &query);
    std::vector<VqsQueueSelection> selections;
    bool overrideQueues{};
    if (vqsPerformQuery(query) != VK_SUCCESS) {
        vqsDestroyQuery(query);
        queryCreateInfo.queueRequirementCount = std::size(noAsyncRequirements);
        queryCreateInfo.pQueueRequirements = noAsyncRequirements.data();
        vqsCreateQuery(&queryCreateInfo, &query);
        if (vqsPerformQuery(query) == VK_SUCCESS) {
            selections.resize(std::size(noAsyncRequirements));
            vqsGetQueueSelections(query, selections.data());
            for (uint32_t i{}; i < createInfo.queueCreateInfoCount; ++i) for (auto& selection : selections) if (createInfo.pQueueCreateInfos[i].queueFamilyIndex == selection.queueFamilyIndex) selection.queueIndex += createInfo.pQueueCreateInfos[i].queueCount;
#ifdef ENABLE_FSR
            FSR_FrameGenerator::useQueues(selections);
#endif
            overrideQueues = true;
        }
    } else {
        selections.resize(std::size(asyncRequirements));
        vqsGetQueueSelections(query, selections.data());
        for (uint32_t i{}; i < createInfo.queueCreateInfoCount; ++i) for (auto& selection : selections) if (createInfo.pQueueCreateInfos[i].queueFamilyIndex == selection.queueFamilyIndex) selection.queueIndex += createInfo.pQueueCreateInfos[i].queueCount;
#ifdef ENABLE_FSR
        FSR_FrameGenerator::useQueues(selections);
#endif
        overrideQueues = true;
    }
    destroyDummySurface(hwnd, asyncRequirements[1].requiredPresentQueueSurface);
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::vector<float> priorities;
    std::vector<float> allPriorities;
    if (overrideQueues) {
        uint32_t queuePriorityCount{}, queueCreateInfoCount{};
        vqsEnumerateDeviceQueueCreateInfos(query, &queueCreateInfoCount, nullptr, &queuePriorityCount, nullptr);
        queueCreateInfos.resize(queueCreateInfoCount);
        priorities.resize(queuePriorityCount);
        vqsEnumerateDeviceQueueCreateInfos(query, &queueCreateInfoCount, queueCreateInfos.data(), &queuePriorityCount, priorities.data());
        uint32_t queueCount{};
        for (uint32_t i{}; i < createInfo.queueCreateInfoCount; ++i)
            for (const VkDeviceQueueCreateInfo& queueCreateInfo : queueCreateInfos)
                queueCount += queueCreateInfo.queueCount + createInfo.pQueueCreateInfos[i].queueCount;
        allPriorities.resize(queueCount);
        queueCount = 0;
        for (uint32_t i{}; i < createInfo.queueCreateInfoCount; ++i) {
            const VkDeviceQueueCreateInfo& originalQueueCreateInfo = createInfo.pQueueCreateInfos[i];
            for (VkDeviceQueueCreateInfo& queueCreateInfo : queueCreateInfos) {
                queueCreateInfo.pQueuePriorities = allPriorities.data() + queueCount;
                std::copy_n(originalQueueCreateInfo.pQueuePriorities, originalQueueCreateInfo.queueCount, allPriorities.data() + queueCount);
                queueCount += originalQueueCreateInfo.queueCount;
                std::copy_n(queueCreateInfo.pQueuePriorities, queueCreateInfo.queueCount, allPriorities.data() + queueCount);
                queueCount += queueCreateInfo.queueCount;
                if (queueCreateInfo.queueFamilyIndex == originalQueueCreateInfo.queueFamilyIndex)
                    queueCreateInfo.queueCount += originalQueueCreateInfo.queueCount;
            }
        }
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = queueCreateInfos.size();
    }
    vqsDestroyQuery(query);

#ifdef ENABLE_DLSS
    if (m_slCreateDevice != VK_NULL_HANDLE) return m_slCreateDevice(physicalDevice, &createInfo, pAllocator, pDevice);
#endif
    return m_vkCreateDevice(physicalDevice, &createInfo, pAllocator, pDevice);
}

VkResult Vulkan::hook_vkCreateWin32SurfaceKHR(VkInstance instance, const VkWin32SurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface) {
    const VkResult result = m_vkCreateWin32SurfaceKHR(instance, pCreateInfo, pAllocator, pSurface);
    FrameGenerator::addMapping(pCreateInfo->hwnd, *pSurface);
    if (HWNDToIntercept == pCreateInfo->hwnd) surfaceToIntercept = *pSurface;
    return result;
}

void Vulkan::hook_vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator) {
    m_vkDestroySurfaceKHR(instance, surface, pAllocator);
    FrameGenerator::removeMapping(surface);
}

VkResult Vulkan::hook_vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) {
    VkResult result = VK_RESULT_MAX_ENUM;
#ifdef ENABLE_FSR
    if (FrameGenerator::ownsSwapchain(*pSwapchain)) result = m_fxCreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain, FSR_FrameGenerator::getContext());
#endif
    if (result == VK_RESULT_MAX_ENUM) {
        result = m_vkCreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
        if (Plugin::frameGenerationProvider != Plugin::None && surfaceToIntercept == pCreateInfo->surface) {
            switch (Plugin::frameGenerationProvider) {
#ifdef ENABLE_FSR
                case Plugin::FSR: FSR_FrameGenerator::createSwapchain(pSwapchain, pCreateInfo, pAllocator, &m_fxCreateSwapchainKHR, &m_fxDestroySwapchainKHR, &m_fxGetSwapchainImagesKHR, &m_fxAcquireNextImageKHR, &m_fxQueuePresentKHR, &m_fxSetHdrMetadataEXT, nullptr); break;
#endif
                case Plugin::None:
                default: break;
            }
        }
    }
    FrameGenerator::addMapping(pCreateInfo->surface, *pSwapchain);
    if (surfaceToIntercept == pCreateInfo->surface) swapchainToIntercept = *pSwapchain;
    return result;
}

void Vulkan::hook_vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator) {
    FrameGenerator::removeMapping(swapchain);
#ifdef ENABLE_FSR
    if (FrameGenerator::ownsSwapchain(swapchain)) return FSR_FrameGenerator::destroySwapchain();
#endif
    m_vkDestroySwapchainKHR(device, swapchain, pAllocator);
}

VkResult Vulkan::hook_vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages) {
#ifdef ENABLE_FSR
    if (FrameGenerator::ownsSwapchain(swapchain)) return m_fxGetSwapchainImagesKHR(device, swapchain, pSwapchainImageCount, pSwapchainImages);
#endif
    return m_vkGetSwapchainImagesKHR(device, swapchain, pSwapchainImageCount, pSwapchainImages);
}

VkResult Vulkan::hook_vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, const uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex) {
#ifdef ENABLE_FSR
    const bool isFsrSwapchain = FrameGenerator::ownsSwapchain(swapchain);
    if (isFsrSwapchain ^ Plugin::frameGenerationProvider == Plugin::FSR && swapchainToIntercept == swapchain) return VK_ERROR_OUT_OF_DATE_KHR;
    if (isFsrSwapchain) return m_fxAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
#endif
    return m_vkAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
}

VkResult Vulkan::hook_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
#ifdef ENABLE_FSR
    if (pPresentInfo->swapchainCount > 0) {
        const bool isFsrSwapchain = FrameGenerator::ownsSwapchain(pPresentInfo->pSwapchains[0]);
        const bool isFsrProvider = Plugin::frameGenerationProvider == Plugin::FSR;
        if ((isFsrSwapchain && !isFsrProvider) || (!isFsrSwapchain && isFsrProvider && swapchainToIntercept == pPresentInfo->pSwapchains[0])) return VK_ERROR_OUT_OF_DATE_KHR;
        if (pPresentInfo->swapchainCount == 1 && isFsrSwapchain) return m_fxQueuePresentKHR(queue, pPresentInfo);
    }
#endif
    return m_vkQueuePresentKHR(queue, pPresentInfo);
}

void Vulkan::hook_vkSetHdrMetadataEXT(VkDevice device, const uint32_t swapchainCount, const VkSwapchainKHR* pSwapchains, const VkHdrMetadataEXT* pMetadata) {
    const PFN_vkSetHdrMetadataEXT* setHdrMetadataEXT = &m_vkSetHdrMetadataEXT;
#ifdef ENABLE_FSR
    for (uint32_t i{}; i < swapchainCount; ++i) {
        if (FrameGenerator::ownsSwapchain(pSwapchains[i])) {
            setHdrMetadataEXT = &m_fxSetHdrMetadataEXT;
            break;
        }
    }
#endif
    return (*setHdrMetadataEXT)(device, swapchainCount, pSwapchains, pMetadata);
}

PFN_vkGetInstanceProcAddr Vulkan::interceptInitialization(PFN_vkGetInstanceProcAddr t_getInstanceProcAddr, void* /*unused*/) {
    m_vkGetInstanceProcAddr = t_getInstanceProcAddr;
    Upscaler::load(VULKAN, &m_slGetInstanceProcAddr);
    FrameGenerator::load(VULKAN);
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

#ifdef ENABLE_FRAME_GENERATION
void Vulkan::setFrameGenerationHWND(HWND hWnd) {
    HWNDToIntercept      = hWnd;
    surfaceToIntercept   = FrameGenerator::getSurface(hWnd);
    swapchainToIntercept = FrameGenerator::getSwapchain(surfaceToIntercept);
}

VkQueue Vulkan::getQueue(const uint32_t family, const uint32_t index) {
    VkQueue queue{VK_NULL_HANDLE};
    m_vkGetDeviceQueue(graphicsInterface->Instance().device, family, index, &queue);
    return queue;
}
#endif

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
    return m_vkGetDeviceProcAddr;
}
#endif