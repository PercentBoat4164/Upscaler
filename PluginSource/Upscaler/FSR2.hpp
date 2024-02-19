#pragma once
#ifdef ENABLE_FSR2
// Project
#    include "Upscaler.hpp"

// Upscaler
#    include <ffx_fsr2.h>

class FSR2 final : public Upscaler {
    FfxFsr2Interface interface;
    FfxFsr2Context   context;
    FfxResource      depth;
    FfxResource      inColor;
    FfxResource      motionVectors;
    FfxResource      outColor;

    static Status (FSR2::*fpInitialize)();
    static Status (FSR2::*fpSetDepth)(void *, UnityRenderingExtTextureFormat);
    static Status (FSR2::*fpSetInputColor)(void *, UnityRenderingExtTextureFormat);
    static Status (FSR2::*fpSetMotionVectors)(void *, UnityRenderingExtTextureFormat);
    static Status (FSR2::*fpSetOutputColor)(void *, UnityRenderingExtTextureFormat);
    static Status (FSR2::*fpEvaluate)();
    static Status (FSR2::*fpRelease)();
    static Status (FSR2::*fpShutdown)();

#    ifdef ENABLE_VULKAN
    Status VulkanInitialize();
    Status VulkanSetDepth(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status VulkanSetInputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status VulkanSetMotionVectors(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status VulkanSetOutputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status VulkanEvaluate();
    Status VulkanRelease();
    Status VulkanShutdown();
#    endif

    void setFunctionPointers(GraphicsAPI::Type graphicsAPI) override;

    static void log(FfxFsr2MsgType type, const wchar_t *t_msg);

    Status setStatus(FfxErrorCode t_error, const std::string &t_msg);

public:
    static FSR2 *get();
    Type         getType() override;
    std::string  getName() override;

#    ifdef ENABLE_VULKAN
    std::vector<std::string> getRequiredVulkanInstanceExtensions() override;
    std::vector<std::string>
    getRequiredVulkanDeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice) override;
#    endif

    Settings getOptimalSettings(Settings::Resolution resolution, Settings::QualityMode mode, bool hdr) override;

    Status initialize() override;
    Status create() override;
    Status setDepth(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) override;
    Status setInputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) override;
    Status setMotionVectors(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) override;
    Status setOutputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) override;
    Status evaluate() override;
    Status release() override;
    Status shutdown() override;
};
#endif