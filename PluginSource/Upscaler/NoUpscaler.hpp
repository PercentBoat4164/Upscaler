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
    UpscalerStatus initialize() override;
    UpscalerStatus createFeature() override;
    UpscalerStatus setImageResources(
      void                          *nativeDepthBuffer,
      UnityRenderingExtTextureFormat unityDepthFormat,
      void                          *nativeMotionVectors,
      UnityRenderingExtTextureFormat unityMotionVectorFormat,
      void                          *nativeInColor,
      UnityRenderingExtTextureFormat unityInColorFormat,
      void                          *nativeOutColor,
      UnityRenderingExtTextureFormat unityOutColorFormat
    ) override;
    UpscalerStatus evaluate() override;
    UpscalerStatus releaseFeature() override;
    UpscalerStatus shutdown() override;

protected:
    void setFunctionPointers(GraphicsAPI::Type graphicsAPI) override;
};