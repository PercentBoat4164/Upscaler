#pragma once

// Project
#include "GraphicsAPI.hpp"

// Unity
#include <IUnityGraphics.h>
#include <IUnityGraphicsVulkan.h>

// Upscaler
#include <nvsdk_ngx_defs.h>

// Standard library
#include <cstring>
#include <sstream>
#include <unordered_map>
#include <vector>

class Vulkan : public GraphicsAPI {
public:
    Vulkan(const Vulkan &)            = delete;
    Vulkan(Vulkan &&)                 = default;
    Vulkan &operator=(const Vulkan &) = delete;
    Vulkan &operator=(Vulkan &&)      = default;

    struct Device {
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

        VkCommandPool   _oneTimeSubmitCommandPool{};
        VkCommandBuffer _oneTimeSubmitCommandBuffer{};
        bool            _oneTimeSubmitRecording{false};

    public:
        Device(VkDevice device, PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr);

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

        VkResult vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo *pSubmits, VkFence fence);

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

        void prepareForOneTimeSubmits();

        VkCommandBuffer beginOneTimeSubmitRecording();

        void endOneTimeSubmitRecording();

        void cancelOneTimeSubmitRecording();

        void finishOneTimeSubmits();
    };

private:
    static PFN_vkGetInstanceProcAddr                  m_vkGetInstanceProcAddr;
    static PFN_vkGetDeviceProcAddr                    m_vkGetDeviceProcAddr;
    static PFN_vkCreateInstance                       m_vkCreateInstance;
    static PFN_vkEnumerateInstanceExtensionProperties m_vkEnumerateInstanceExtensionProperties;
    static PFN_vkCreateDevice                         m_vkCreateDevice;
    static PFN_vkEnumerateDeviceExtensionProperties   m_vkEnumerateDeviceExtensionProperties;

    static VkInstance                           instance;
    static std::unordered_map<VkDevice, Device> devices;
    static IUnityGraphicsVulkanV2              *vulkanInterface;

    static bool loadEarlyFunctionPointers();

    static bool loadLateFunctionPointers();

    static std::vector<std::string> getSupportedInstanceExtensions();

    static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateInstance(
      const VkInstanceCreateInfo  *pCreateInfo,
      const VkAllocationCallbacks *pAllocator,
      VkInstance                  *pInstance
    );

    static std::vector<std::string> getSupportedDeviceExtensions(VkPhysicalDevice physicalDevice);

    static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateDevice(
      VkPhysicalDevice          physicalDevice,
      const VkDeviceCreateInfo *pCreateInfo,
      VkAllocationCallbacks    *pAllocator,
      VkDevice                 *pDevice
    );

    static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
    Hook_vkGetInstanceProcAddr(VkInstance t_instance, const char *pName);

    static PFN_vkGetInstanceProcAddr
    interceptInitialization(PFN_vkGetInstanceProcAddr t_getInstanceProcAddr, void * /*unused*/);

public:
    static Device getDevice(VkDevice device);

    static void setVkGetInstanceProcAddr(PFN_vkGetInstanceProcAddr t_vkGetInstanceProcAddr);

    static PFN_vkGetInstanceProcAddr getVkGetInstanceProcAddr();

    static bool interceptInitialization(IUnityGraphicsVulkanV2 *t_vulkanInterface);

    static bool RemoveInterceptInitialization();

    static IUnityGraphicsVulkanV2 *getVulkanInterface();

    Type getType() override;

    ~Vulkan() override = default;
};