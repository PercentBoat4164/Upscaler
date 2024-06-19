#pragma once
#ifdef ENABLE_VULKAN
#    include "GraphicsAPI.hpp"

#    include <vulkan/vulkan.h>

#    include <IUnityRenderingExtensions.h>

#    include <string>
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

    static PFN_vkCreateImageView        m_vkCreateImageView;
    static PFN_vkDestroyImageView       m_vkDestroyImageView;
    static PFN_vkCreateCommandPool      m_vkCreateCommandPool;
    static PFN_vkAllocateCommandBuffers m_vkAllocateCommandBuffers;
    static PFN_vkBeginCommandBuffer     m_vkBeginCommandBuffer;
    static PFN_vkEndCommandBuffer       m_vkEndCommandBuffer;
    static PFN_vkQueueSubmit            m_vkQueueSubmit;
    static PFN_vkCreateFence            m_vkCreateFence;
    static PFN_vkWaitForFences          m_vkWaitForFences;
    static PFN_vkFreeCommandBuffers     m_vkFreeCommandBuffers;
    static PFN_vkDestroyCommandPool     m_vkDestroyCommandPool;

    static IUnityGraphicsVulkanV2* graphicsInterface;

    static VkCommandPool commandPool;

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

    [[nodiscard]] static bool initializeOneTimeSubmits();
    [[nodiscard]] static VkCommandBuffer getOneTimeSubmitCommandBuffer();
    static void _submitOneTimeCommandBuffer(int /*unused*/, void* data);
    [[nodiscard]] static bool submitOneTimeSubmitCommandBuffer(VkCommandBuffer commandBuffer);
    static void shutdownOneTimeSubmits();

    static PFN_vkGetDeviceProcAddr getDeviceProcAddr();
};
#endif