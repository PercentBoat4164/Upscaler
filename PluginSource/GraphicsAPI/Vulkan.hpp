#pragma once
#ifdef ENABLE_VULKAN
#    include "GraphicsAPI.hpp"

#    include <vulkan/vulkan.h>

#    include <IUnityRenderingExtensions.h>

struct IUnityGraphicsVulkanV2;

class Vulkan final : public GraphicsAPI {
    static PFN_vkGetInstanceProcAddr m_vkGetInstanceProcAddr;
    static PFN_vkGetInstanceProcAddr m_nvGetInstanceProcAddr;
    static PFN_vkGetDeviceProcAddr   m_vkGetDeviceProcAddr;
    static PFN_vkGetDeviceProcAddr   m_nvGetDeviceProcAddr;
    static PFN_vkCreateSwapchainKHR  m_vkCreateSwapchainKHR;
    static PFN_vkCreateSwapchainKHR  m_nvCreateSwapchainKHR;

    static PFN_vkCreateImageView  m_vkCreateImageView;
    static PFN_vkDestroyImageView m_vkDestroyImageView;

    static IUnityGraphicsVulkanV2* graphicsInterface;

    static PFN_vkVoidFunction hook_vkGetInstanceProcAddr(VkInstance instance, const char* name);
    static PFN_vkVoidFunction hook_vkGetDeviceProcAddr(VkDevice device, const char* name);
    static VkResult           hook_vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain);
    static VkResult           hook_vkDestroySwapchainKHR();
    static VkResult           hook_vkGetSwapchainImagesKHR();
    static VkResult           hook_vkAcquireNexImageKHR();
    static VkResult           hook_vkDeviceWaitIdle();
    static VkResult           hook_vkCreateWin32SurfaceKHR();
    static VkResult           hook_vkDestroySurfaceKHR();

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

    static VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags flags);
    static void        destroyImageView(VkImageView viewToDestroy);

    static PFN_vkGetDeviceProcAddr getDeviceProcAddr();
};
#endif