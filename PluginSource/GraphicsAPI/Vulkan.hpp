#pragma once
#ifdef ENABLE_VULKAN
#    include "GraphicsAPI.hpp"

#    include <ffx_api_vk.h>

#    include <vulkan/vulkan.h>

#    include <IUnityRenderingExtensions.h>

#    include <vector>

struct IUnityGraphicsVulkanV2;

class Vulkan final : public GraphicsAPI {
    static PFN_vkGetInstanceProcAddr    m_vkGetInstanceProcAddr;
    static PFN_vkGetInstanceProcAddr    m_slGetInstanceProcAddr;
    static PFN_vkCreateDevice           m_vkCreateDevice;
    static PFN_vkGetDeviceProcAddr      m_vkGetDeviceProcAddr;
    static PFN_vkGetDeviceProcAddr      m_slGetDeviceProcAddr;
    static PFN_vkCreateSwapchainKHR     m_vkCreateSwapchainKHR;
    static PFN_vkCreateSwapchainKHR     m_slCreateSwapchainKHR;
    static PFN_vkCreateSwapchainFFXAPI  m_fxCreateSwapchainKHR;
    static PFN_vkDestroySwapchainKHR    m_vkDestroySwapchainKHR;
    static PFN_vkDestroySwapchainKHR    m_slDestroySwapchainKHR;
    static PFN_vkDestroySwapchainFFXAPI m_fxDestroySwapchainKHR;
    static PFN_vkGetSwapchainImagesKHR  m_vkGetSwapchainImagesKHR;
    static PFN_vkGetSwapchainImagesKHR  m_slGetSwapchainImagesKHR;
    static PFN_vkGetSwapchainImagesKHR  m_fxGetSwapchainImagesKHR;
    static PFN_vkAcquireNextImageKHR    m_vkAcquireNextImageKHR;
    static PFN_vkAcquireNextImageKHR    m_slAcquireNextImageKHR;
    static PFN_vkAcquireNextImageKHR    m_fxAcquireNextImageKHR;
    static PFN_vkQueuePresentKHR        m_vkQueuePresentKHR;
    static PFN_vkQueuePresentKHR        m_slQueuePresentKHR;
    static PFN_vkQueuePresentKHR        m_fxQueuePresentKHR;
    static PFN_vkSetHdrMetadataEXT      m_vkSetHdrMetadataEXT;
    static PFN_vkSetHdrMetadataEXT      m_fxSetHdrMetadataEXT;
    // static PFN_vkCreateWin32SurfaceKHR  m_vkCreateWin32SurfaceKHR;
    // static PFN_vkDestroySurfaceKHR      m_vkDestroySurfaceKHR;

    static PFN_vkGetPhysicalDeviceQueueFamilyProperties m_vkGetPhysicalDeviceQueueFamilyProperties;
    static PFN_vkDestroyImage                           m_vkDestroyImage;
    static PFN_vkGetDeviceQueue                         m_vkGetDeviceQueue;
    static PFN_vkCreateImageView                        m_vkCreateImageView;
    static PFN_vkDestroyImageView                       m_vkDestroyImageView;
    static PFN_vkQueueSubmit                            m_vkQueueSubmit;

    static IUnityGraphicsVulkanV2* graphicsInterface;
    static uint64_t                SizeOfSwapchainToRecreate;

public:
    static PFN_vkVoidFunction monitor_vkGetDeviceProcAddr(VkDevice device, const char* name);

private:
    static PFN_vkVoidFunction hook_vkGetInstanceProcAddr(VkInstance instance, const char* name);
    static PFN_vkVoidFunction hook_vkGetDeviceProcAddr(VkDevice device, const char* name);
    static VkResult           hook_vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice);
    static VkResult           hook_vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain);
    static void               hook_vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator);
    static VkResult           hook_vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages);
    static VkResult           hook_vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex);
    static VkResult           hook_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo);
    static void               hook_vkSetHdrMetadataEXT(VkDevice device, uint32_t swapchainCount, const VkSwapchainKHR* pSwapchains, const VkHdrMetadataEXT* pMetadata);
    // static VkResult           hook_vkDeviceWaitIdle();
    // static VkResult           hook_vkCreateWin32SurfaceKHR(VkInstance instance, const VkWin32SurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);
    // static void               hook_vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator);

    static UNITY_INTERFACE_EXPORT PFN_vkGetInstanceProcAddr UNITY_INTERFACE_API interceptInitialization(PFN_vkGetInstanceProcAddr t_getInstanceProcAddr, void* /*unused*/);

public:
    Vulkan()                         = delete;
    Vulkan(const Vulkan&)            = delete;
    Vulkan(Vulkan&&)                 = delete;
    Vulkan& operator=(const Vulkan&) = delete;
    Vulkan& operator=(Vulkan&&)      = delete;
    ~Vulkan()                        = delete;

    static bool                    registerUnityInterfaces(IUnityInterfaces* t_unityInterfaces);
    static IUnityGraphicsVulkanV2* getGraphicsInterface();
    static bool                    unregisterUnityInterfaces();

    static void                                      requestSwapchainRecreationBySize(uint64_t size);
    static std::vector<std::pair<VkQueue, uint32_t>> getQueues(const std::vector<VkQueueFlags>& queueTypes);
    static VkResult                                  createSwapchain(const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain);
    static VkImageView                               createImageView(VkImage image, VkFormat format, VkImageAspectFlags flags);
    static void                                      destroyImageView(VkImageView viewToDestroy);
    static VkResult                                  submit(uint32_t submitCount, const VkSubmitInfo* submitInfo, VkFence fence);

    static PFN_vkGetDeviceProcAddr getDeviceProcAddr();
};
#endif