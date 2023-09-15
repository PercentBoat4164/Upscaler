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
    Settings getOptimalSettings(Settings::Resolution resolution) override;
    bool     isSupportedAfter(bool b) override;
    void     setSupported(bool b) override;
    bool     isAvailableAfter(bool b) override;
    void     setAvailable(bool b) override;
    bool     isSupported() override;
    bool     isAvailable() override;
    bool     initialize() override;
    bool     createFeature() override;
    bool     setImageResources(
          void                          *nativeDepthBuffer,
          UnityRenderingExtTextureFormat unityDepthFormat,
          void                          *nativeMotionVectors,
          UnityRenderingExtTextureFormat unityMotionVectorFormat,
          void                          *nativeInColor,
          UnityRenderingExtTextureFormat unityInColorFormat,
          void                          *nativeOutColor,
          UnityRenderingExtTextureFormat unityOutColorFormat
        ) override;
    bool evaluate() override;
    bool releaseFeature() override;
    bool shutdown() override;

protected:
    void setFunctionPointers(GraphicsAPI::Type graphicsAPI) override;
};