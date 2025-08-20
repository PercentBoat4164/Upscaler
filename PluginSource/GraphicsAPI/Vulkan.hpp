#pragma once
#ifdef ENABLE_VULKAN
#    include "GraphicsAPI.hpp"

#    ifdef ENABLE_FSR
#        include <vk/ffx_api_vk.h>
#    endif

#    ifdef ENABLE_FRAME_GENERATION
#        include <IUnityRenderingExtensions.h>
#    endif

#    include <vulkan/vulkan.h>

struct IUnityGraphicsVulkanV2;

class Vulkan final : public GraphicsAPI {
    static PFN_vkGetInstanceProcAddr    m_vkGetInstanceProcAddr;
    static PFN_vkCreateInstance         m_vkCreateInstance;
    static PFN_vkCreateDevice           m_vkCreateDevice;
    static PFN_vkGetDeviceProcAddr      m_vkGetDeviceProcAddr;
    static PFN_vkCreateSwapchainKHR     m_vkCreateSwapchainKHR;
    static PFN_vkDestroySwapchainKHR    m_vkDestroySwapchainKHR;
    static PFN_vkGetSwapchainImagesKHR  m_vkGetSwapchainImagesKHR;
    static PFN_vkAcquireNextImageKHR    m_vkAcquireNextImageKHR;
    static PFN_vkQueuePresentKHR        m_vkQueuePresentKHR;
    static PFN_vkSetHdrMetadataEXT      m_vkSetHdrMetadataEXT;
    static PFN_vkCreateWin32SurfaceKHR  m_vkCreateWin32SurfaceKHR;
    static PFN_vkDestroySurfaceKHR      m_vkDestroySurfaceKHR;
#    ifdef ENABLE_FSR
    static PFN_vkCreateSwapchainFFXAPI  m_fxCreateSwapchainKHR;
    static PFN_vkDestroySwapchainFFXAPI m_fxDestroySwapchainKHR;
    static PFN_vkGetSwapchainImagesKHR  m_fxGetSwapchainImagesKHR;
    static PFN_vkAcquireNextImageKHR    m_fxAcquireNextImageKHR;
    static PFN_vkQueuePresentKHR        m_fxQueuePresentKHR;
    static PFN_vkSetHdrMetadataEXT      m_fxSetHdrMetadataEXT;
#    endif
#    ifdef ENABLE_DLSS
    static PFN_vkGetInstanceProcAddr   m_slGetInstanceProcAddr;
    static PFN_vkCreateInstance        m_slCreateInstance;
    static PFN_vkCreateDevice          m_slCreateDevice;
    static PFN_vkGetDeviceProcAddr     m_slGetDeviceProcAddr;
    static PFN_vkCreateSwapchainKHR    m_slCreateSwapchainKHR;
    static PFN_vkDestroySwapchainKHR   m_slDestroySwapchainKHR;
    static PFN_vkGetSwapchainImagesKHR m_slGetSwapchainImagesKHR;
    static PFN_vkAcquireNextImageKHR   m_slAcquireNextImageKHR;
    static PFN_vkQueuePresentKHR       m_slQueuePresentKHR;
#    endif
    static PFN_vkGetPhysicalDeviceQueueFamilyProperties m_vkGetPhysicalDeviceQueueFamilyProperties;
    static PFN_vkGetPhysicalDeviceSurfaceSupportKHR     m_vkGetPhysicalDeviceSurfaceSupportKHR;
    static PFN_vkDestroyImage                           m_vkDestroyImage;
    static PFN_vkGetDeviceQueue                         m_vkGetDeviceQueue;
    static PFN_vkCreateImageView                        m_vkCreateImageView;
    static PFN_vkDestroyImageView                       m_vkDestroyImageView;

    static VkInstance instance;
    static IUnityGraphicsVulkanV2* graphicsInterface;
    static HWND                    HWNDToIntercept;
    static VkSurfaceKHR            surfaceToIntercept;
    static VkSwapchainKHR          swapchainToIntercept;

    static VkSurfaceKHR createDummySurface(void*& hWnd);
    static void         destroyDummySurface(void* hWnd, VkSurfaceKHR dummySurface);

    static VkResult           hook_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance);
    static PFN_vkVoidFunction hook_vkGetInstanceProcAddr(VkInstance instance, const char* name);
    static VkResult           hook_vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice);
    static PFN_vkVoidFunction hook_vkGetDeviceProcAddr(VkDevice device, const char* name);
    static VkResult           hook_vkCreateWin32SurfaceKHR(VkInstance instance, const VkWin32SurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);
    static void               hook_vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAllocator);
    static VkResult           hook_vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain);
    static void               hook_vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks* pAllocator);
    static VkResult           hook_vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t* pSwapchainImageCount, VkImage* pSwapchainImages);
    static VkResult           hook_vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex);
    static VkResult           hook_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo);
    static void               hook_vkSetHdrMetadataEXT(VkDevice device, uint32_t swapchainCount, const VkSwapchainKHR* pSwapchains, const VkHdrMetadataEXT* pMetadata);

    static PFN_vkGetInstanceProcAddr interceptInitialization(PFN_vkGetInstanceProcAddr t_getInstanceProcAddr, void* /*unused*/);

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

    static void        setFrameGenerationHWND(HWND hWnd);
    static VkQueue     getQueue(uint32_t family, uint32_t index);
    static VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags flags);
    static void        destroyImageView(VkImageView viewToDestroy);

    static PFN_vkGetDeviceProcAddr getDeviceProcAddr();

#    ifdef ENABLE_FRAME_GENERATION
#    pragma region Format Conversions
    static UnityRenderingExtTextureFormat toUnityFormat(VkFormat format);
    static VkFormat                       toVulkanFormat(UnityRenderingExtTextureFormat format);
#    pragma endregion
#    endif
};
#endif