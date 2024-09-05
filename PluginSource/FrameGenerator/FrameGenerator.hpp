#pragma once
#include <GraphicsAPI/GraphicsAPI.hpp>
#ifdef ENABLE_FRAME_GENERATION
#    ifdef ENABLE_VULKAN
#        include <vulkan/vulkan.h>
#    endif

class FrameGenerator {
protected:
    static union Swapchain {
#    ifdef ENABLE_VULKAN
        VkSwapchainKHR vulkan{VK_NULL_HANDLE};
#    endif
    } swapchain;

public:
    static void useGraphicsAPI(GraphicsAPI::Type type);

    FrameGenerator()                                 = default;
    FrameGenerator(const FrameGenerator&)            = delete;
    FrameGenerator(FrameGenerator&&)                 = delete;
    FrameGenerator& operator=(const FrameGenerator&) = delete;
    FrameGenerator& operator=(FrameGenerator&&)      = delete;
    virtual ~FrameGenerator()                        = default;
};
#endif