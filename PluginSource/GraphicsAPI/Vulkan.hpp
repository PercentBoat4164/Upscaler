#pragma once

// Project
#include "GraphicsAPI.hpp"

// Unity
#include <IUnityGraphicsVulkan.h>
#include <IUnityRenderingExtensions.h>

// Standard library
#include <sstream>
#include <vector>

class Vulkan final : public GraphicsAPI {
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
    static PFN_vkQueueWaitIdle          m_vkQueueWaitIdle;
    static PFN_vkResetCommandBuffer     m_vkResetCommandBuffer;
    static PFN_vkFreeCommandBuffers     m_vkFreeCommandBuffers;
    static PFN_vkDestroyCommandPool     m_vkDestroyCommandPool;
    static PFN_vkCreateFence            m_vkCreateFence;
    static PFN_vkWaitForFences          m_vkWaitForFences;
    static PFN_vkResetFences            m_vkResetFences;
    static PFN_vkDestroyFence           m_vkDestroyFence;

    IUnityGraphicsVulkanV2 *_vulkanInterface{};
    VkInstance              _instance{};
    VkDevice                _device{};
    VkCommandPool           _oneTimeSubmitCommandPool{VK_NULL_HANDLE};
    VkCommandBuffer         _oneTimeSubmitCommandBuffer{VK_NULL_HANDLE};
    VkFence                 _oneTimeSubmitFence{VK_NULL_HANDLE};
    bool                    _oneTimeSubmitRecording{false};

    static bool        loadEarlyFunctionPointers();
    [[nodiscard]] bool loadInstanceFunctionPointers() const;
    [[nodiscard]] bool loadLateFunctionPointers() const;

    static std::vector<std::string>       getSupportedInstanceExtensions();
    static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateInstance(
      const VkInstanceCreateInfo  *pCreateInfo,
      const VkAllocationCallbacks *pAllocator,
      VkInstance                  *pInstance
    );

    static std::vector<std::string>       getSupportedDeviceExtensions(VkPhysicalDevice physicalDevice);
    static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateDevice(
      VkPhysicalDevice             physicalDevice,
      const VkDeviceCreateInfo    *pCreateInfo,
      const VkAllocationCallbacks *pAllocator,
      VkDevice                    *pDevice
    );

    static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
    Hook_vkGetInstanceProcAddr(VkInstance t_instance, const char *pName);

    static UNITY_INTERFACE_EXPORT PFN_vkGetInstanceProcAddr UNITY_INTERFACE_API
    interceptInitialization(PFN_vkGetInstanceProcAddr t_getInstanceProcAddr, void * /*unused*/);

    Vulkan() = default;

public:
            Vulkan(const Vulkan &)    = delete;
            Vulkan(Vulkan &&)         = default;
    Vulkan &operator=(const Vulkan &) = delete;
    Vulkan &operator=(Vulkan &&)      = default;

    static bool RemoveInterceptInitialization();

    static VkFormat getFormat(UnityRenderingExtTextureFormat format);

    static PFN_vkGetInstanceProcAddr getVkGetInstanceProcAddr();
    static PFN_vkGetDeviceProcAddr   getVkGetDeviceProcAddr();

    static Vulkan *get();

    Type getType() override;

    bool                                  useUnityInterfaces(IUnityInterfaces *t_unityInterfaces) override;
    [[nodiscard]] IUnityGraphicsVulkanV2 *getUnityInterface() const;

    [[nodiscard]] VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags flags) const;
    void                      destroyImageView(VkImageView viewToDestroy) const;

    [[nodiscard]] VkDevice getDevice() const;

    ~Vulkan() override = default;
};