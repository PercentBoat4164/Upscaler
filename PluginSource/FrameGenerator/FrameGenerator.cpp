#include "FrameGenerator.hpp"

#include "FSR_FrameGenerator.hpp"

#include <ranges>

std::unordered_map<VkSurfaceKHR, HWND> FrameGenerator::VkSurfaceKHR_HWND{};
std::unordered_map<HWND, VkSwapchainKHR> FrameGenerator::HWND_VkSwapchainKHR{};
FrameGenerator::Swapchain FrameGenerator::swapchain{};

void FrameGenerator::addMapping(HWND hwnd, VkSurfaceKHR surface) {
    VkSurfaceKHR_HWND.emplace(surface, hwnd);
}

void FrameGenerator::addMapping(VkSurfaceKHR surface, VkSwapchainKHR swapchain) {
    const auto it = VkSurfaceKHR_HWND.find(surface);
    if (it != VkSurfaceKHR_HWND.end())
        HWND_VkSwapchainKHR.emplace(it->second, swapchain);
}

void FrameGenerator::removeMapping(VkSurfaceKHR surface) {
    VkSurfaceKHR_HWND.erase(surface);
}

void FrameGenerator::removeMapping(VkSwapchainKHR swapchain) {
    for (auto& [hwnd, swp] : HWND_VkSwapchainKHR)
        if (swapchain == swp) return (void)HWND_VkSwapchainKHR.erase(hwnd);
}

VkSwapchainKHR FrameGenerator::getSwapchain(HWND hwnd) {
    const auto it = HWND_VkSwapchainKHR.find(hwnd);
    if (it != HWND_VkSwapchainKHR.end()) return it->second;
    return VK_NULL_HANDLE;
}

std::vector<HWND> FrameGenerator::getHWNDs() {
    auto range = std::ranges::views::keys(HWND_VkSwapchainKHR);
    return {range.cbegin(), range.cend()};
}