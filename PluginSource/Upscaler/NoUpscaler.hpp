#pragma once

#include "Upscaler.hpp"

class NoUpscaler final : public Upscaler {
public:
#ifdef ENABLE_VULKAN
    static std::vector<std::string> requestVulkanInstanceExtensions(const std::vector<std::string>& /*unused*/);
    static std::vector<std::string> requestVulkanDeviceExtensions(VkInstance /*unused*/, VkPhysicalDevice /*unused*/, const std::vector<std::string>& /*unused*/);
#endif

    static bool isSupported();
    static bool isSupported(enum Settings::Quality mode);

    NoUpscaler();
    NoUpscaler(const NoUpscaler&)            = delete;
    NoUpscaler(NoUpscaler&&)                 = delete;
    NoUpscaler& operator=(const NoUpscaler&) = delete;
    NoUpscaler& operator=(NoUpscaler&&)      = delete;
    ~NoUpscaler() override                   = default;

    constexpr Type getType() override {
        return NONE;
    }

    constexpr std::string getName() override {
        return "Dummy upscaler";
    }

    Status useSettings(Settings::Resolution resolution, Settings::DLSSPreset /*unused*/, enum Settings::Quality /*unused*/, bool /*unused*/) override;
    Status evaluate() override;

    bool resetStatus() override;
};