#include "FrameGenerator.hpp"

#include "FSR_FrameGenerator.hpp"

#include <ranges>

std::unordered_map<uint64_t, VkSwapchainKHR> FrameGenerator::SizeToVkSwapchainKHR{};
FrameGenerator::Swapchain FrameGenerator::swapchain{};
UnityRenderingExtTextureFormat FrameGenerator::backBufferFormat{};

void FrameGenerator::addMapping(const uint64_t size, VkSwapchainKHR swapchain) {
    SizeToVkSwapchainKHR.emplace(size, swapchain);
}

void FrameGenerator::removeMapping(VkSwapchainKHR swapchain) {
    for (auto [size, swp] : SizeToVkSwapchainKHR) {
        if (swapchain == swp) {
            SizeToVkSwapchainKHR.erase(size);
            break;
        }
    }
}

VkSwapchainKHR FrameGenerator::getSwapchain(const uint64_t size) {
    const auto it = SizeToVkSwapchainKHR.find(size);
    if (it != SizeToVkSwapchainKHR.end()) return it->second;
    return VK_NULL_HANDLE;
}

UnityRenderingExtTextureFormat FrameGenerator::getBackBufferFormat() {
    return backBufferFormat;
}