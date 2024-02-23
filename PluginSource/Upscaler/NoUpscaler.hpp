#pragma once

#include "Upscaler.hpp"

class NoUpscaler final : public Upscaler {
public:
#ifdef ENABLE_VULKAN
    static std::vector<std::string> requestVulkanInstanceExtensions(const std::vector<std::string>& /*unused*/);
    static std::vector<std::string> requestVulkanDeviceExtensions(const std::vector<std::string>& /*unused*/);
#endif

    Type        getType() final;
    std::string getName() final;
    bool        isSupported() final;
    Status      getOptimalSettings(Settings::Resolution /*unused*/, Settings::QualityMode /*unused*/, bool /*unused*/) final;

    Status initialize() final;
    Status create() final;
    Status setDepth(void* /*unused*/, UnityRenderingExtTextureFormat /*unused*/) final;
    Status setInputColor(void* /*unused*/, UnityRenderingExtTextureFormat /*unused*/) final;
    Status setMotionVectors(void* /*unused*/, UnityRenderingExtTextureFormat /*unused*/) final;
    Status setOutputColor(void* /*unused*/, UnityRenderingExtTextureFormat /*unused*/) final;
    Status evaluate() final;
    Status shutdown() final;
};