#ifdef ENABLE_VULKAN
#    include "Vulkan.hpp"

#    include <Upscaler/Upscaler.hpp>
#    ifdef ENABLE_DLSS
#        include <Upscaler/DLSS_Upscaler.hpp>
#    endif
#    ifdef ENABLE_FSR
#        include <FrameGenerator/FSR_FrameGenerator.hpp>
#        include <Upscaler/FSR_Upscaler.hpp>
#    endif
#    ifdef ENABLE_XESS
#        include "Upscaler/XeSS_Upscaler.hpp"
#    endif

#    include <IUnityGraphicsVulkan.h>

#    define VQS_IMPLEMENTATION
#    include <vk_queue_selector.h>

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

VkSurfaceKHR Vulkan::createDummySurface(void*& hWnd) {
#    ifdef WIN32
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    hWnd = CreateWindow("STATIC", "DummyWindow", 0, 0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);
    VkSurfaceKHR                      surface{VK_NULL_HANDLE};
    const VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .hinstance = hInstance,
        .hwnd = static_cast<HWND>(hWnd)
    };
    m_vkCreateWin32SurfaceKHR(instance, &win32SurfaceCreateInfo, nullptr, &surface);
    return surface;
#    endif
}

void Vulkan::destroyDummySurface(void* hWnd, VkSurfaceKHR dummySurface) {
    m_vkDestroySurfaceKHR(instance, dummySurface, nullptr);
#    ifdef WIN32
    DestroyWindow(static_cast<HWND>(hWnd));
#    endif
}

VkResult Vulkan::hook_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) {
    VkResult result{VK_SUCCESS};
#    ifdef ENABLE_DLSS
    if (m_slCreateInstance != VK_NULL_HANDLE) result = m_slCreateInstance(pCreateInfo, pAllocator, pInstance);
    else
#    endif
        result = m_vkCreateInstance(pCreateInfo, pAllocator, pInstance);
    instance = *pInstance;
    return result;
}

VkResult Vulkan::hook_vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) {
    static VkDeviceCreateInfo createInfo = *pCreateInfo;
    void* hWnd = nullptr;
    const std::array asyncRequirements {
      VqsQueueRequirements{VK_QUEUE_TRANSFER_BIT, 0.9F, VK_NULL_HANDLE},
      VqsQueueRequirements{0,                     1.0F, createDummySurface(hWnd)},
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
    destroyDummySurface(hWnd, asyncRequirements[1].requiredPresentQueueSurface);
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
    FrameGenerator::addMapping(pCreateInfo->surface, *pSwapchain, toUnityFormat(pCreateInfo->imageFormat));
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
    if (isFsrSwapchain ^ (Plugin::frameGenerationProvider == Plugin::FSR) && swapchainToIntercept == swapchain) return VK_ERROR_OUT_OF_DATE_KHR;
    if (isFsrSwapchain) return m_fxAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
#endif
    return m_vkAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
}

VkResult Vulkan::hook_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
#ifdef ENABLE_FRAME_GENERATION
    const bool intercepting = swapchainToIntercept != nullptr;
    VkPresentInfoKHR presentInfo = *pPresentInfo;
    std::vector<VkSwapchainKHR> nativeSwapchains;
    std::unordered_map<std::size_t, uint32_t> mapping;
    uint32_t swapchainCount = presentInfo.swapchainCount;
    nativeSwapchains.reserve(swapchainCount);
    VkResult swapchainPresentResult = VK_SUCCESS;
    for (; swapchainCount > 0; --swapchainCount) {
        const uint32_t index = swapchainCount - 1;
        const bool isFsrSwapchain = FrameGenerator::ownsSwapchain(pPresentInfo->pSwapchains[index]);
        if (isFsrSwapchain || swapchainToIntercept == pPresentInfo->pSwapchains[index] && pPresentInfo->pResults != nullptr) mapping[-1] = index;
        if ((isFsrSwapchain && !intercepting) || (!isFsrSwapchain && swapchainToIntercept == pPresentInfo->pSwapchains[index])) swapchainPresentResult = VK_ERROR_OUT_OF_DATE_KHR;
        else if (isFsrSwapchain) swapchainPresentResult = m_fxQueuePresentKHR(queue, &presentInfo);
        else {
            nativeSwapchains.emplace_back(pPresentInfo->pSwapchains[index]);
            if (pPresentInfo->pResults != nullptr) mapping[nativeSwapchains.size()] = index;
        }
    }
    presentInfo.pSwapchains = nativeSwapchains.data();
    presentInfo.swapchainCount = nativeSwapchains.size();
    std::vector<VkResult> results(nativeSwapchains.size());
    presentInfo.pResults = results.data();
    if (presentInfo.swapchainCount == 0) return swapchainPresentResult;
    const VkResult result = m_vkQueuePresentKHR(queue, &presentInfo);
    if (pPresentInfo->pResults != nullptr)
        for (const auto & [src, dst] : mapping) {
            if (src == -1) pPresentInfo->pResults[dst] = swapchainPresentResult;
            else pPresentInfo->pResults[dst] = results[src];
        }
    if (result == VK_ERROR_DEVICE_LOST || swapchainPresentResult == VK_ERROR_DEVICE_LOST) return VK_ERROR_DEVICE_LOST;
    if (result == VK_ERROR_SURFACE_LOST_KHR || swapchainPresentResult == VK_ERROR_SURFACE_LOST_KHR) return VK_ERROR_SURFACE_LOST_KHR;
    if (result == VK_ERROR_OUT_OF_DATE_KHR || swapchainPresentResult == VK_ERROR_OUT_OF_DATE_KHR) return VK_ERROR_OUT_OF_DATE_KHR;
    if (result == VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT || swapchainPresentResult == VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT) return VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT;
    if (result == VK_SUBOPTIMAL_KHR || swapchainPresentResult == VK_SUBOPTIMAL_KHR) return VK_SUBOPTIMAL_KHR;
    return result;
#else
    return m_vkQueuePresentKHR(queue, pPresentInfo);
#endif
}

void Vulkan::hook_vkSetHdrMetadataEXT(VkDevice device, uint32_t swapchainCount, const VkSwapchainKHR* pSwapchains, const VkHdrMetadataEXT* pMetadata) {
#ifdef ENABLE_FRAME_GENERATION
    std::vector<VkSwapchainKHR> nativeSwapchains;
    nativeSwapchains.reserve(swapchainCount);
    for (; swapchainCount > 0; --swapchainCount)
        if (FrameGenerator::ownsSwapchain(pSwapchains[swapchainCount - 1])) m_fxSetHdrMetadataEXT(device, 1, &pSwapchains[swapchainCount - 1], pMetadata);
        else nativeSwapchains.emplace_back(pSwapchains[swapchainCount - 1]);
    pSwapchains = nativeSwapchains.data();
    swapchainCount = nativeSwapchains.size();
    if (swapchainCount == 0) return;
#endif
    return m_vkSetHdrMetadataEXT(device, swapchainCount, pSwapchains, pMetadata);
}

PFN_vkGetInstanceProcAddr Vulkan::interceptInitialization(PFN_vkGetInstanceProcAddr t_getInstanceProcAddr, void* /*unused*/) {
    m_vkGetInstanceProcAddr = t_getInstanceProcAddr;
    Upscaler::load(VULKAN, &m_slGetInstanceProcAddr);
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

#ifdef ENABLE_FRAME_GENERATION
#pragma region Format Conversions
UnityRenderingExtTextureFormat Vulkan::toUnityFormat(const VkFormat format) {
    switch (format) {
        case VK_FORMAT_R8_UNORM: return kUnityRenderingExtFormatR8_UNorm;
        case VK_FORMAT_R8_SNORM: return kUnityRenderingExtFormatR8_SNorm;
        case VK_FORMAT_R8_UINT: return kUnityRenderingExtFormatR8_UInt;
        case VK_FORMAT_R8_SINT: return kUnityRenderingExtFormatR8_SInt;
        case VK_FORMAT_R8_SRGB: return kUnityRenderingExtFormatR8_SRGB;
        case VK_FORMAT_R8G8_UNORM: return kUnityRenderingExtFormatR8G8_UNorm;
        case VK_FORMAT_R8G8_SNORM: return kUnityRenderingExtFormatR8G8_SNorm;
        case VK_FORMAT_R8G8_UINT: return kUnityRenderingExtFormatR8G8_UInt;
        case VK_FORMAT_R8G8_SINT: return kUnityRenderingExtFormatR8G8_SInt;
        case VK_FORMAT_R8G8_SRGB: return kUnityRenderingExtFormatR8G8_SRGB;
        case VK_FORMAT_R8G8B8_UNORM: return kUnityRenderingExtFormatR8G8B8_UNorm;
        case VK_FORMAT_R8G8B8_SNORM: return kUnityRenderingExtFormatR8G8B8_SNorm;
        case VK_FORMAT_R8G8B8_UINT: return kUnityRenderingExtFormatR8G8B8_UInt;
        case VK_FORMAT_R8G8B8_SINT: return kUnityRenderingExtFormatR8G8B8_SInt;
        case VK_FORMAT_R8G8B8_SRGB: return kUnityRenderingExtFormatR8G8B8_SRGB;
        case VK_FORMAT_B8G8R8_UNORM: return kUnityRenderingExtFormatB8G8R8_UNorm;
        case VK_FORMAT_B8G8R8_SNORM: return kUnityRenderingExtFormatB8G8R8_SNorm;
        case VK_FORMAT_B8G8R8_UINT: return kUnityRenderingExtFormatB8G8R8_UInt;
        case VK_FORMAT_B8G8R8_SINT: return kUnityRenderingExtFormatB8G8R8_SInt;
        case VK_FORMAT_B8G8R8_SRGB: return kUnityRenderingExtFormatB8G8R8_SRGB;
        case VK_FORMAT_R8G8B8A8_UNORM: return kUnityRenderingExtFormatR8G8B8A8_UNorm;
        case VK_FORMAT_R8G8B8A8_SNORM: return kUnityRenderingExtFormatR8G8B8A8_SNorm;
        case VK_FORMAT_R8G8B8A8_UINT: return kUnityRenderingExtFormatR8G8B8A8_UInt;
        case VK_FORMAT_R8G8B8A8_SINT: return kUnityRenderingExtFormatR8G8B8A8_SInt;
        case VK_FORMAT_R8G8B8A8_SRGB: return kUnityRenderingExtFormatR8G8B8A8_SRGB;
        case VK_FORMAT_B8G8R8A8_UNORM: return kUnityRenderingExtFormatB8G8R8A8_UNorm;
        case VK_FORMAT_B8G8R8A8_SNORM: return kUnityRenderingExtFormatB8G8R8A8_SNorm;
        case VK_FORMAT_B8G8R8A8_UINT: return kUnityRenderingExtFormatB8G8R8A8_UInt;
        case VK_FORMAT_B8G8R8A8_SINT: return kUnityRenderingExtFormatB8G8R8A8_SInt;
        case VK_FORMAT_B8G8R8A8_SRGB: return kUnityRenderingExtFormatB8G8R8A8_SRGB;
        case VK_FORMAT_R16_UNORM: return kUnityRenderingExtFormatR16_UNorm;
        case VK_FORMAT_R16_SNORM: return kUnityRenderingExtFormatR16_SNorm;
        case VK_FORMAT_R16_UINT: return kUnityRenderingExtFormatR16_UInt;
        case VK_FORMAT_R16_SINT: return kUnityRenderingExtFormatR16_SInt;
        case VK_FORMAT_R16_SFLOAT: return kUnityRenderingExtFormatR16_SFloat;
        case VK_FORMAT_R16G16_UNORM: return kUnityRenderingExtFormatR16G16_UNorm;
        case VK_FORMAT_R16G16_SNORM: return kUnityRenderingExtFormatR16G16_SNorm;
        case VK_FORMAT_R16G16_UINT: return kUnityRenderingExtFormatR16G16_UInt;
        case VK_FORMAT_R16G16_SINT: return kUnityRenderingExtFormatR16G16_SInt;
        case VK_FORMAT_R16G16_SFLOAT: return kUnityRenderingExtFormatR16G16_SFloat;
        case VK_FORMAT_R16G16B16_UNORM: return kUnityRenderingExtFormatR16G16B16_UNorm;
        case VK_FORMAT_R16G16B16_SNORM: return kUnityRenderingExtFormatR16G16B16_SNorm;
        case VK_FORMAT_R16G16B16_UINT: return kUnityRenderingExtFormatR16G16B16_UInt;
        case VK_FORMAT_R16G16B16_SINT: return kUnityRenderingExtFormatR16G16B16_SInt;
        case VK_FORMAT_R16G16B16_SFLOAT: return kUnityRenderingExtFormatR16G16B16_SFloat;
        case VK_FORMAT_R16G16B16A16_UNORM: return kUnityRenderingExtFormatR16G16B16A16_UNorm;
        case VK_FORMAT_R16G16B16A16_SNORM: return kUnityRenderingExtFormatR16G16B16A16_SNorm;
        case VK_FORMAT_R16G16B16A16_UINT: return kUnityRenderingExtFormatR16G16B16A16_UInt;
        case VK_FORMAT_R16G16B16A16_SINT: return kUnityRenderingExtFormatR16G16B16A16_SInt;
        case VK_FORMAT_R16G16B16A16_SFLOAT: return kUnityRenderingExtFormatR16G16B16A16_SFloat;
        case VK_FORMAT_R32_UINT: return kUnityRenderingExtFormatR32_UInt;
        case VK_FORMAT_R32_SINT: return kUnityRenderingExtFormatR32_SInt;
        case VK_FORMAT_R32_SFLOAT: return kUnityRenderingExtFormatR32_SFloat;
        case VK_FORMAT_R32G32_UINT: return kUnityRenderingExtFormatR32G32_UInt;
        case VK_FORMAT_R32G32_SINT: return kUnityRenderingExtFormatR32G32_SInt;
        case VK_FORMAT_R32G32_SFLOAT: return kUnityRenderingExtFormatR32G32_SFloat;
        case VK_FORMAT_R32G32B32_UINT: return kUnityRenderingExtFormatR32G32B32_UInt;
        case VK_FORMAT_R32G32B32_SINT: return kUnityRenderingExtFormatR32G32B32_SInt;
        case VK_FORMAT_R32G32B32_SFLOAT: return kUnityRenderingExtFormatR32G32B32_SFloat;
        case VK_FORMAT_R32G32B32A32_UINT: return kUnityRenderingExtFormatR32G32B32A32_UInt;
        case VK_FORMAT_R32G32B32A32_SINT: return kUnityRenderingExtFormatR32G32B32A32_SInt;
        case VK_FORMAT_R32G32B32A32_SFLOAT: return kUnityRenderingExtFormatR32G32B32A32_SFloat;
        case VK_FORMAT_R4G4B4A4_UNORM_PACK16: return kUnityRenderingExtFormatR4G4B4A4_UNormPack16;
        case VK_FORMAT_B4G4R4A4_UNORM_PACK16: return kUnityRenderingExtFormatB4G4R4A4_UNormPack16;
        case VK_FORMAT_R5G6B5_UNORM_PACK16: return kUnityRenderingExtFormatR5G6B5_UNormPack16;
        case VK_FORMAT_B5G6R5_UNORM_PACK16: return kUnityRenderingExtFormatB5G6R5_UNormPack16;
        case VK_FORMAT_R5G5B5A1_UNORM_PACK16: return kUnityRenderingExtFormatR5G5B5A1_UNormPack16;
        case VK_FORMAT_B5G5R5A1_UNORM_PACK16: return kUnityRenderingExtFormatB5G5R5A1_UNormPack16;
        case VK_FORMAT_A1R5G5B5_UNORM_PACK16: return kUnityRenderingExtFormatA1R5G5B5_UNormPack16;
        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: return kUnityRenderingExtFormatE5B9G9R9_UFloatPack32;
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return kUnityRenderingExtFormatB10G11R11_UFloatPack32;
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return kUnityRenderingExtFormatA2R10G10B10_UNormPack32;
        case VK_FORMAT_A2R10G10B10_UINT_PACK32: return kUnityRenderingExtFormatA2R10G10B10_UIntPack32;
        case VK_FORMAT_A2R10G10B10_SINT_PACK32: return kUnityRenderingExtFormatA2R10G10B10_SIntPack32;
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return kUnityRenderingExtFormatA2B10G10R10_UNormPack32;
        case VK_FORMAT_A2B10G10R10_UINT_PACK32: return kUnityRenderingExtFormatA2B10G10R10_UIntPack32;
        case VK_FORMAT_A2B10G10R10_SINT_PACK32: return kUnityRenderingExtFormatA2B10G10R10_SIntPack32;
        default: return kUnityRenderingExtFormatNone;
    }
}

VkFormat Vulkan::toVulkanFormat(const UnityRenderingExtTextureFormat format) {
    switch (format) {
        case kUnityRenderingExtFormatR8_UNorm: return VK_FORMAT_R8_UNORM;
        case kUnityRenderingExtFormatR8_SNorm: return VK_FORMAT_R8_SNORM;
        case kUnityRenderingExtFormatR8_UInt: return VK_FORMAT_R8_UINT;
        case kUnityRenderingExtFormatR8_SInt: return VK_FORMAT_R8_SINT;
        case kUnityRenderingExtFormatR8_SRGB: return VK_FORMAT_R8_SRGB;
        case kUnityRenderingExtFormatR8G8_UNorm: return VK_FORMAT_R8G8_UNORM;
        case kUnityRenderingExtFormatR8G8_SNorm: return VK_FORMAT_R8G8_SNORM;
        case kUnityRenderingExtFormatR8G8_UInt: return VK_FORMAT_R8G8_UINT;
        case kUnityRenderingExtFormatR8G8_SInt: return VK_FORMAT_R8G8_SINT;
        case kUnityRenderingExtFormatR8G8_SRGB: return VK_FORMAT_R8G8_SRGB;
        case kUnityRenderingExtFormatR8G8B8_UNorm: return VK_FORMAT_R8G8B8_UNORM;
        case kUnityRenderingExtFormatR8G8B8_SNorm: return VK_FORMAT_R8G8B8_SNORM;
        case kUnityRenderingExtFormatR8G8B8_UInt: return VK_FORMAT_R8G8B8_UINT;
        case kUnityRenderingExtFormatR8G8B8_SInt: return VK_FORMAT_R8G8B8_SINT;
        case kUnityRenderingExtFormatR8G8B8_SRGB: return VK_FORMAT_R8G8B8_SRGB;
        case kUnityRenderingExtFormatB8G8R8_UNorm: return VK_FORMAT_B8G8R8_UNORM;
        case kUnityRenderingExtFormatB8G8R8_SNorm: return VK_FORMAT_B8G8R8_SNORM;
        case kUnityRenderingExtFormatB8G8R8_UInt: return VK_FORMAT_B8G8R8_UINT;
        case kUnityRenderingExtFormatB8G8R8_SInt: return VK_FORMAT_B8G8R8_SINT;
        case kUnityRenderingExtFormatB8G8R8_SRGB: return VK_FORMAT_B8G8R8_SRGB;
        case kUnityRenderingExtFormatR8G8B8A8_UNorm: return VK_FORMAT_R8G8B8A8_UNORM;
        case kUnityRenderingExtFormatR8G8B8A8_SNorm: return VK_FORMAT_R8G8B8A8_SNORM;
        case kUnityRenderingExtFormatR8G8B8A8_UInt: return VK_FORMAT_R8G8B8A8_UINT;
        case kUnityRenderingExtFormatR8G8B8A8_SInt: return VK_FORMAT_R8G8B8A8_SINT;
        case kUnityRenderingExtFormatR8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
        case kUnityRenderingExtFormatB8G8R8A8_UNorm: return VK_FORMAT_B8G8R8A8_UNORM;
        case kUnityRenderingExtFormatB8G8R8A8_SNorm: return VK_FORMAT_B8G8R8A8_SNORM;
        case kUnityRenderingExtFormatB8G8R8A8_UInt: return VK_FORMAT_B8G8R8A8_UINT;
        case kUnityRenderingExtFormatB8G8R8A8_SInt: return VK_FORMAT_B8G8R8A8_SINT;
        case kUnityRenderingExtFormatB8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
        case kUnityRenderingExtFormatR16_UNorm: return VK_FORMAT_R16_UNORM;
        case kUnityRenderingExtFormatR16_SNorm: return VK_FORMAT_R16_SNORM;
        case kUnityRenderingExtFormatR16_UInt: return VK_FORMAT_R16_UINT;
        case kUnityRenderingExtFormatR16_SInt: return VK_FORMAT_R16_SINT;
        case kUnityRenderingExtFormatR16_SFloat: return VK_FORMAT_R16_SFLOAT;
        case kUnityRenderingExtFormatR16G16_UNorm: return VK_FORMAT_R16G16_UNORM;
        case kUnityRenderingExtFormatR16G16_SNorm: return VK_FORMAT_R16G16_SNORM;
        case kUnityRenderingExtFormatR16G16_UInt: return VK_FORMAT_R16G16_UINT;
        case kUnityRenderingExtFormatR16G16_SInt: return VK_FORMAT_R16G16_SINT;
        case kUnityRenderingExtFormatR16G16_SFloat: return VK_FORMAT_R16G16_SFLOAT;
        case kUnityRenderingExtFormatR16G16B16_UNorm: return VK_FORMAT_R16G16B16_UNORM;
        case kUnityRenderingExtFormatR16G16B16_SNorm: return VK_FORMAT_R16G16B16_SNORM;
        case kUnityRenderingExtFormatR16G16B16_UInt: return VK_FORMAT_R16G16B16_UINT;
        case kUnityRenderingExtFormatR16G16B16_SInt: return VK_FORMAT_R16G16B16_SINT;
        case kUnityRenderingExtFormatR16G16B16_SFloat: return VK_FORMAT_R16G16B16_SFLOAT;
        case kUnityRenderingExtFormatR16G16B16A16_UNorm: return VK_FORMAT_R16G16B16A16_UNORM;
        case kUnityRenderingExtFormatR16G16B16A16_SNorm: return VK_FORMAT_R16G16B16A16_SNORM;
        case kUnityRenderingExtFormatR16G16B16A16_UInt: return VK_FORMAT_R16G16B16A16_UINT;
        case kUnityRenderingExtFormatR16G16B16A16_SInt: return VK_FORMAT_R16G16B16A16_SINT;
        case kUnityRenderingExtFormatR16G16B16A16_SFloat: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case kUnityRenderingExtFormatR32_UInt: return VK_FORMAT_R32_UINT;
        case kUnityRenderingExtFormatR32_SInt: return VK_FORMAT_R32_SINT;
        case kUnityRenderingExtFormatR32_SFloat: return VK_FORMAT_R32_SFLOAT;
        case kUnityRenderingExtFormatR32G32_UInt: return VK_FORMAT_R32G32_UINT;
        case kUnityRenderingExtFormatR32G32_SInt: return VK_FORMAT_R32G32_SINT;
        case kUnityRenderingExtFormatR32G32_SFloat: return VK_FORMAT_R32G32_SFLOAT;
        case kUnityRenderingExtFormatR32G32B32_UInt: return VK_FORMAT_R32G32B32_UINT;
        case kUnityRenderingExtFormatR32G32B32_SInt: return VK_FORMAT_R32G32B32_SINT;
        case kUnityRenderingExtFormatR32G32B32_SFloat: return VK_FORMAT_R32G32B32_SFLOAT;
        case kUnityRenderingExtFormatR32G32B32A32_UInt: return VK_FORMAT_R32G32B32A32_UINT;
        case kUnityRenderingExtFormatR32G32B32A32_SInt: return VK_FORMAT_R32G32B32A32_SINT;
        case kUnityRenderingExtFormatR32G32B32A32_SFloat: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case kUnityRenderingExtFormatR4G4B4A4_UNormPack16: return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
        case kUnityRenderingExtFormatB4G4R4A4_UNormPack16: return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
        case kUnityRenderingExtFormatR5G6B5_UNormPack16: return VK_FORMAT_R5G6B5_UNORM_PACK16;
        case kUnityRenderingExtFormatB5G6R5_UNormPack16: return VK_FORMAT_B5G6R5_UNORM_PACK16;
        case kUnityRenderingExtFormatR5G5B5A1_UNormPack16: return VK_FORMAT_R5G5B5A1_UNORM_PACK16;
        case kUnityRenderingExtFormatB5G5R5A1_UNormPack16: return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
        case kUnityRenderingExtFormatA1R5G5B5_UNormPack16: return VK_FORMAT_A1R5G5B5_UNORM_PACK16;
        case kUnityRenderingExtFormatE5B9G9R9_UFloatPack32: return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
        case kUnityRenderingExtFormatB10G11R11_UFloatPack32: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        case kUnityRenderingExtFormatA2R10G10B10_UNormPack32: return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
        case kUnityRenderingExtFormatA2R10G10B10_UIntPack32: return VK_FORMAT_A2R10G10B10_UINT_PACK32;
        case kUnityRenderingExtFormatA2R10G10B10_SIntPack32: return VK_FORMAT_A2R10G10B10_SINT_PACK32;
        case kUnityRenderingExtFormatA2B10G10R10_UNormPack32: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case kUnityRenderingExtFormatA2B10G10R10_UIntPack32: return VK_FORMAT_A2B10G10R10_UINT_PACK32;
        case kUnityRenderingExtFormatA2B10G10R10_SIntPack32: return VK_FORMAT_A2B10G10R10_SINT_PACK32;
        default: return VK_FORMAT_UNDEFINED;
    }
}
#pragma endregion
#endif
#endif