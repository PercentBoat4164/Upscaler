#pragma once
#ifdef ENABLE_FRAME_GENERATION
#    ifdef ENABLE_VULKAN
#        include <vulkan/vulkan.h>
#    endif

#    include <unordered_map>

class FrameGenerator {
protected:
    static std::unordered_map<uint64_t, VkSwapchainKHR> SizeToVkSwapchainKHR;

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

    static void           addMapping(uint64_t size, VkSwapchainKHR swapchain);
    static void           removeMapping(VkSwapchainKHR swapchain);
    static VkSwapchainKHR getSwapchain(uint64_t size);
};
#endif