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
    Settings getOptimalSettings(Settings::Resolution /* unused */, Settings::Quality /* unused */, bool /* unused */) override;
    ErrorReason     initialize() override;
    ErrorReason     createFeature() override;
    ErrorReason     setImageResources(
          void                          *nativeDepthBuffer,
          UnityRenderingExtTextureFormat unityDepthFormat,
          void                          *nativeMotionVectors,
          UnityRenderingExtTextureFormat unityMotionVectorFormat,
          void                          *nativeInColor,
          UnityRenderingExtTextureFormat unityInColorFormat,
          void                          *nativeOutColor,
          UnityRenderingExtTextureFormat unityOutColorFormat
        ) override;
    ErrorReason evaluate() override;
    ErrorReason releaseFeature() override;
    ErrorReason shutdown() override;

protected:
    void setFunctionPointers(GraphicsAPI::Type graphicsAPI) override;
};