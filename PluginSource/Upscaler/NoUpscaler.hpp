#pragma once

#include "Upscaler.hpp"

class NoUpscaler final : public Upscaler {
public:
#ifdef ENABLE_VULKAN
    static std::vector<std::string> requestVulkanInstanceExtensions(const std::vector<std::string>& /*unused*/);
    static std::vector<std::string> requestVulkanDeviceExtensions(const std::vector<std::string>& /*unused*/);
#endif

    Type        getType() override;
    std::string getName() override;
    bool isSupported() override;
    Settings getOptimalSettings(Settings::Resolution /*unused*/, Settings::QualityMode /*unused*/, bool /*unused*/) override;

    Status initialize() override;
    Status create() override;
    Status setDepth(void* /*unused*/, UnityRenderingExtTextureFormat /*unused*/) override;
    Status setInputColor(void* /*unused*/, UnityRenderingExtTextureFormat /*unused*/) override;
    Status setMotionVectors(void* /*unused*/, UnityRenderingExtTextureFormat /*unused*/) override;
    Status setOutputColor(void* /*unused*/, UnityRenderingExtTextureFormat /*unused*/) override;
    Status evaluate() override;
    Status release() override;
    Status shutdown() override;
};