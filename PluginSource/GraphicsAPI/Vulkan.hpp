#pragma once
#ifdef ENABLE_VULKAN
#    include "GraphicsAPI.hpp"

#    include <vulkan/vulkan.h>

#    include <IUnityRenderingExtensions.h>

#    include <sstream>
#    include <vector>

struct IUnityGraphicsVulkanV2;

class Vulkan final : public GraphicsAPI {
    static VkInstance temporaryInstance;

    static PFN_vkGetInstanceProcAddr                  m_vkGetInstanceProcAddr;
    static PFN_vkGetDeviceProcAddr                    m_vkGetDeviceProcAddr;
    static PFN_vkCreateInstance                       m_vkCreateInstance;
    static PFN_vkEnumerateInstanceExtensionProperties m_vkEnumerateInstanceExtensionProperties;
    static PFN_vkCreateDevice                         m_vkCreateDevice;
    static PFN_vkEnumerateDeviceExtensionProperties   m_vkEnumerateDeviceExtensionProperties;

    static PFN_vkCreateImageView  m_vkCreateImageView;
    static PFN_vkDestroyImageView m_vkDestroyImageView;

    static IUnityGraphicsVulkanV2* graphicsInterface;

    static void loadEarlyFunctionPointers();
    static void loadInstanceFunctionPointers(VkInstance instance);
    static void loadDeviceFunctionPointers(VkDevice device);

    static std::vector<std::string>       getSupportedInstanceExtensions();
    static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateInstance(
      const VkInstanceCreateInfo*  pCreateInfo,
      const VkAllocationCallbacks* pAllocator,
      VkInstance*                  pInstance
    );

    static std::vector<std::string>       getSupportedDeviceExtensions(VkPhysicalDevice physicalDevice);
    static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateDevice(
      VkPhysicalDevice             physicalDevice,
      const VkDeviceCreateInfo*    pCreateInfo,
      const VkAllocationCallbacks* pAllocator,
      VkDevice*                    pDevice
    );

    static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
    Hook_vkGetInstanceProcAddr(VkInstance t_instance, const char* pName);

    static UNITY_INTERFACE_EXPORT PFN_vkGetInstanceProcAddr UNITY_INTERFACE_API
    interceptInitialization(PFN_vkGetInstanceProcAddr t_getInstanceProcAddr, void* /*unused*/);

public:
    Vulkan() = delete;
    ~Vulkan() = delete;

    static bool                    registerUnityInterfaces(IUnityInterfaces* t_unityInterfaces);
    static IUnityGraphicsVulkanV2* getGraphicsInterface();
    static bool                    unregisterUnityInterfaces();

    static VkFormat    getFormat(UnityRenderingExtTextureFormat format);
    static VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags flags);
    static void        destroyImageView(VkImageView viewToDestroy);
};
#endif