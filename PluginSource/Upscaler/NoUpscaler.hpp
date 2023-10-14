#pragma once

#include "Upscaler.hpp"

class NoUpscaler : public Upscaler {
private:
    NoUpscaler() = default;

public:
    static NoUpscaler       *get();
    Type                     getType() override;
    std::string              getName() override;
    std::vector<std::string> getRequiredVulkanInstanceExtensions() override;
    std::vector<std::string>
    getRequiredVulkanDeviceExtensions(VkInstance instance, VkPhysicalDevice device) override;
    Settings
    getOptimalSettings(Settings::Resolution /* unused */, Settings::Quality /* unused */, bool /* unused */)
      override;
    Status initialize() override;
    Status createFeature() override;
    Status setDepthBuffer(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) override;
    Status setInputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) override;
    Status setMotionVectors(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) override;
    Status setOutputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) override;
    Status evaluate() override;
    Status releaseFeature() override;
    Status shutdown() override;

protected:
    void setFunctionPointers(GraphicsAPI::Type graphicsAPI) override;
};