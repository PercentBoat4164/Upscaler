#include "FrameGenerator.hpp"

#ifdef ENABLE_FSR
#    include "FSR_FrameGenerator.hpp"
#endif

std::unordered_map<HWND, VkSurfaceKHR> FrameGenerator::HWNDToSurface{};
std::unordered_map<VkSurfaceKHR, VkSwapchainKHR> FrameGenerator::SurfaceToSwapchain{};
FrameGenerator::Swapchain FrameGenerator::swapchain{};
UnityRenderingExtTextureFormat                   FrameGenerator::backBufferFormat{};

void FrameGenerator::load(const GraphicsAPI::Type type) {
#ifdef ENABLE_FSR
    FSR_FrameGenerator::load(type);
#endif
}

void FrameGenerator::unload() {
#ifdef ENABLE_FSR
    FSR_FrameGenerator::unload();
#endif
}

void FrameGenerator::addMapping(HWND hWnd, VkSurfaceKHR surface) {
    HWNDToSurface[hWnd] = surface;
}

void FrameGenerator::addMapping(VkSurfaceKHR surface, VkSwapchainKHR swapchain) {
    SurfaceToSwapchain[surface] = swapchain;
}

void FrameGenerator::removeMapping(VkSurfaceKHR surface) {
    for (auto [hWnd, surf] : HWNDToSurface) {
        if (surface == surf) {
            HWNDToSurface.erase(hWnd);
            break;
        }
    }
}

void FrameGenerator::removeMapping(VkSwapchainKHR swapchain) {
    for (auto [surf, swap] : SurfaceToSwapchain) {
        if (swapchain == swap) {
            SurfaceToSwapchain.erase(surf);
            break;
        }
    }
}

VkSurfaceKHR FrameGenerator::getSurface(HWND hWnd) {
    const auto it = HWNDToSurface.find(hWnd);
    if (it == HWNDToSurface.end()) return VK_NULL_HANDLE;
    return it->second;
}

VkSwapchainKHR FrameGenerator::getSwapchain(HWND hWnd) {
    const auto it = HWNDToSurface.find(hWnd);
    if (it == HWNDToSurface.end()) return VK_NULL_HANDLE;
    const auto it1 = SurfaceToSwapchain.find(it->second);
    if (it1 == SurfaceToSwapchain.end()) return VK_NULL_HANDLE;
    return it1->second;
}

VkSwapchainKHR FrameGenerator::getSwapchain(VkSurfaceKHR surface) {
    const auto it = SurfaceToSwapchain.find(surface);
    if (it == SurfaceToSwapchain.end()) return VK_NULL_HANDLE;
    return it->second;
}

UnityRenderingExtTextureFormat FrameGenerator::getBackBufferFormat() {
    return backBufferFormat;
}

bool FrameGenerator::ownsSwapchain(VkSwapchainKHR swapchain) {
    return FrameGenerator::swapchain.vulkan != VK_NULL_HANDLE && swapchain == FrameGenerator::swapchain.vulkan;
}