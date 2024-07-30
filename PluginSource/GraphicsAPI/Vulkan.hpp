#pragma once
#ifdef ENABLE_VULKAN
#    include "GraphicsAPI.hpp"

#    include <vulkan/vulkan.h>

#    include <IUnityRenderingExtensions.h>

struct IUnityGraphicsVulkanV2;

class Vulkan final : public GraphicsAPI {
    static PFN_vkGetInstanceProcAddr m_vkGetInstanceProcAddr;
    static PFN_vkGetDeviceProcAddr   m_vkGetDeviceProcAddr;

    static PFN_vkCreateImageView  m_vkCreateImageView;
    static PFN_vkDestroyImageView m_vkDestroyImageView;

    static IUnityGraphicsVulkanV2* graphicsInterface;

    static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Hook_vkGetInstanceProcAddr(VkInstance t_instance, const char* pName);

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