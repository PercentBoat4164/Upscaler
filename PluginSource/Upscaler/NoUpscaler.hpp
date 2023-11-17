#pragma once

#include "Upscaler.hpp"

class NoUpscaler final : public Upscaler {
    NoUpscaler() = default;

protected:
    void setFunctionPointers(GraphicsAPI::Type /* unused */) override;

public:
    static NoUpscaler       *get();

    Type                     getType() override;
    std::string              getName() override;

    std::vector<std::string> getRequiredVulkanInstanceExtensions() override;
    std::vector<std::string>
    getRequiredVulkanDeviceExtensions(VkInstance /* unused */, VkPhysicalDevice /* unused */) override;

    Settings
    getOptimalSettings(Settings::Resolution /* unused */, Settings::Quality /* unused */, bool /* unused */)
      override;

    Status initialize() override;
    Status createFeature() override;
    Status setDepthBuffer(void */* unused */, UnityRenderingExtTextureFormat /* unused */) override;
    Status setInputColor(void */* unused */, UnityRenderingExtTextureFormat /* unused */) override;
    Status setMotionVectors(void */* unused */, UnityRenderingExtTextureFormat /* unused */) override;
    Status setOutputColor(void */* unused */, UnityRenderingExtTextureFormat /* unused */) override;
    void updateImages() override;
    Status evaluate() override;
    Status releaseFeature() override;
    Status shutdown() override;
};