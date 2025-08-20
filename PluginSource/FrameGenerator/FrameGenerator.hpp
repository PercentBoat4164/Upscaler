#pragma once

#include <vcruntime.h>

#include <IUnityRenderingExtensions.h>
#ifdef ENABLE_FRAME_GENERATION
#    ifdef ENABLE_VULKAN
#        include <vulkan/vulkan.h>
#    endif

#    include <unordered_map>

class FrameGenerator {
protected:
    static std::unordered_map<HWND, VkSurfaceKHR> HWNDToSurface;
    static std::unordered_map<VkSurfaceKHR, VkSwapchainKHR> SurfaceToSwapchain;
    static std::unordered_map<VkSwapchainKHR, UnityRenderingExtTextureFormat> swapchainToBackbufferFormat;

    static union Swapchain {
#    ifdef ENABLE_VULKAN
        VkSwapchainKHR vulkan{VK_NULL_HANDLE};
#    endif
    } swapchain;

public:
    FrameGenerator()                                 = default;
    FrameGenerator(const FrameGenerator&)            = delete;
    FrameGenerator(FrameGenerator&&)                 = delete;
    FrameGenerator& operator=(const FrameGenerator&) = delete;
    FrameGenerator& operator=(FrameGenerator&&)      = delete;
    virtual ~FrameGenerator()                        = default;

    static void                           addMapping(HWND hWnd, VkSurfaceKHR surface);
    static void                           addMapping(VkSurfaceKHR surface, VkSwapchainKHR swapchain, UnityRenderingExtTextureFormat format);
    static void                           removeMapping(VkSurfaceKHR surface);
    static void                           removeMapping(VkSwapchainKHR swapchain);
    static VkSurfaceKHR                   getSurface(HWND hWnd);
    static VkSwapchainKHR                 getSwapchain(HWND hWnd);
    static VkSwapchainKHR                 getSwapchain(VkSurfaceKHR hWnd);
    static UnityRenderingExtTextureFormat getBackBufferFormat(HWND hWnd);
    static bool                           ownsSwapchain(VkSwapchainKHR swapchain);
};
#endif