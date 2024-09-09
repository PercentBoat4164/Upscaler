#pragma once
#ifdef ENABLE_FRAME_GENERATION
#    ifdef ENABLE_VULKAN
#        include <vulkan/vulkan.h>
#    endif

#    include <unordered_map>

class FrameGenerator {
protected:
    static std::unordered_map<VkSurfaceKHR, HWND> VkSurfaceKHR_HWND;
    static std::unordered_map<HWND, VkSwapchainKHR> HWND_VkSwapchainKHR;

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

    static void              addMapping(HWND hwnd, VkSurfaceKHR surface);
    static void              addMapping(VkSurfaceKHR surface, VkSwapchainKHR swapchain);
    static void              removeMapping(VkSurfaceKHR surface);
    static void              removeMapping(VkSwapchainKHR swapchain);
    static VkSwapchainKHR    getSwapchain(HWND hwnd);
    static std::vector<HWND> getHWNDs();
};
#endif