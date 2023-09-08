#pragma once

#include "GraphicsAPI.hpp"
#include "Plugin.hpp"

#include <IUnityGraphics.h>
#include <IUnityGraphicsVulkan.h>

#include <nvsdk_ngx_defs.h>

#include <cstring>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace GraphicsAPI {
class Vulkan : public GraphicsAPI {
public:
    Vulkan(const Vulkan &) = delete;
    Vulkan(Vulkan &&) = default;
    Vulkan &operator =(const Vulkan &) = delete;
    Vulkan &operator =(Vulkan &&) = default;

    struct DeviceFunctions {
    private:
        VkDevice device;

        PFN_vkCreateImageView        m_vkCreateImageView;
        PFN_vkCreateCommandPool      m_vkCreateCommandPool;
        PFN_vkAllocateCommandBuffers m_vkAllocateCommandBuffers;
        PFN_vkBeginCommandBuffer     m_vkBeginCommandBuffer;
        PFN_vkEndCommandBuffer       m_vkEndCommandBuffer;
        PFN_vkQueueSubmit            m_vkQueueSubmit;
        PFN_vkQueueWaitIdle          m_vkQueueWaitIdle;
        PFN_vkResetCommandBuffer     m_vkResetCommandBuffer;
        PFN_vkFreeCommandBuffers     m_vkFreeCommandBuffers;
        PFN_vkDestroyCommandPool     m_vkDestroyCommandPool;

    public:
        DeviceFunctions(VkDevice device, PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr);

        VkResult vkCreateCommandPool(
          const VkCommandPoolCreateInfo *pCreateInfo,
          const VkAllocationCallbacks   *pAllocator,
          VkCommandPool                 *pCommandPool
        );

        VkResult vkAllocateCommandBuffers(
          const VkCommandBufferAllocateInfo *pAllocateInfo,
          VkCommandBuffer                   *pCommandBuffers
        );

        VkResult vkBeginCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferBeginInfo *pBeginInfo);

        VkResult vkEndCommandBuffer(VkCommandBuffer commandBuffer);

        VkResult
        vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo *pSubmits, VkFence fence);

        VkResult vkQueueWaitIdle(VkQueue queue);

        VkResult vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags);

        void vkFreeCommandBuffers(
          VkCommandPool    commandPool,
          uint32_t         commandBufferCount,
          VkCommandBuffer *pCommandBuffers
        );

        void vkDestroyCommandPool(VkCommandPool commandPool, const VkAllocationCallbacks *pAllocator);

        VkResult vkCreateImageView(
          const VkImageViewCreateInfo *pCreateInfo,
          const VkAllocationCallbacks *pAllocator,
          VkImageView                 *pView
        );
    };

private:
    static PFN_vkGetInstanceProcAddr                  m_vkGetInstanceProcAddr;
    static PFN_vkGetDeviceProcAddr                    m_vkGetDeviceProcAddr;
    static PFN_vkCreateInstance                       m_vkCreateInstance;
    static PFN_vkEnumerateInstanceExtensionProperties m_vkEnumerateInstanceExtensionProperties;
    static PFN_vkCreateDevice                         m_vkCreateDevice;
    static PFN_vkEnumerateDeviceExtensionProperties   m_vkEnumerateDeviceExtensionProperties;

    static VkInstance                                    instance;
    static std::unordered_map<VkDevice, DeviceFunctions> deviceFunctions;
    static IUnityGraphicsVulkanV2                       *vulkanInterface;

    static bool loadEarlyFunctionPointers();

    static bool loadLateFunctionPointers();

    static ExtensionGroup getSupportedInstanceExtensions();

    static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateInstance(
      const VkInstanceCreateInfo  *pCreateInfo,
      const VkAllocationCallbacks *pAllocator,
      VkInstance                  *pInstance
    );

    static ExtensionGroup getSupportedDeviceExtensions(VkPhysicalDevice physicalDevice);

    static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateDevice(
      VkPhysicalDevice          physicalDevice,
      const VkDeviceCreateInfo *pCreateInfo,
      VkAllocationCallbacks    *pAllocator,
      VkDevice                 *pDevice
    );

    static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
    Hook_vkGetInstanceProcAddr(VkInstance t_instance, const char *pName) {
        if (pName == nullptr)
            return nullptr;
        if (strcmp(pName, "vkCreateInstance") == 0)
            return reinterpret_cast<PFN_vkVoidFunction>(&Hook_vkCreateInstance);
        if (strcmp(pName, "vkCreateDevice") == 0)
            return reinterpret_cast<PFN_vkVoidFunction>(&Hook_vkCreateDevice);
        return m_vkGetInstanceProcAddr(t_instance, pName);
    }

    static PFN_vkGetInstanceProcAddr
    interceptInitialization(PFN_vkGetInstanceProcAddr t_getInstanceProcAddr, void * /*unused*/) {
        setVkGetInstanceProcAddr(t_getInstanceProcAddr);
        loadEarlyFunctionPointers();
        return Hook_vkGetInstanceProcAddr;
    }

public:
    static DeviceFunctions get(VkDevice device);

    static void setVkGetInstanceProcAddr(PFN_vkGetInstanceProcAddr t_vkGetInstanceProcAddr);

    static PFN_vkGetInstanceProcAddr getVkGetInstanceProcAddr();

    static bool interceptInitialization(IUnityGraphicsVulkanV2 *t_vulkanInterface);

    static bool RemoveInterceptInitialization();

    static IUnityGraphicsVulkanV2 *getVulkanInterface();

    ~Vulkan() override = default;
};
}  // namespace GraphicsAPI