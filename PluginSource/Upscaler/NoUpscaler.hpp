#pragma once

#include "Plugin.hpp"
#include "Upscaler.hpp"

class NoUpscaler final : public Upscaler {
public:
#ifdef ENABLE_VULKAN
    static std::vector<std::string> requestVulkanInstanceExtensions(const std::vector<std::string>& /*unused*/);
    static std::vector<std::string> requestVulkanDeviceExtensions(const std::vector<std::string>& /*unused*/);
#endif

    constexpr Upscaler::Type getType() final {
        return Upscaler::NONE;
    }

    constexpr std::string getName() final {
        return "Dummy upscaler";
    }

    bool   isSupported() final;
    Status getOptimalSettings(Settings::Resolution /*unused*/, Settings::Preset /*unused*/, enum Settings::Quality /*unused*/, bool /*unused*/) final;

    Status initialize() final;
    Status create() final;
    Status evaluate() final;
    Status shutdown() final;
};