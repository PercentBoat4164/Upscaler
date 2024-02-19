#pragma once
#ifdef ENABLE_FSR2
// Project
#    include "Upscaler.hpp"

// Upscaler
#    include <ffx_fsr2.h>

class FSR2 final : public Upscaler {
    FfxFsr2Interface          interface;
    FfxFsr2ContextDescription description;
    FfxFsr2Context            context;
    FfxResource               depth;
    FfxResource               inColor;
    FfxResource               motionVectors;
    FfxResource               outColor;

    static Status (FSR2::*graphicsAPIIndependentInitializeFunctionPointer)();
    static Status (FSR2::*graphicsAPIIndependentSetDepthBufferFunctionPointer)(
      void *,
      UnityRenderingExtTextureFormat
    );
    static Status (FSR2::*graphicsAPIIndependentSetInputColorFunctionPointer)(
      void *,
      UnityRenderingExtTextureFormat
    );
    static Status (FSR2::*graphicsAPIIndependentSetMotionVectorsFunctionPointer)(
      void *,
      UnityRenderingExtTextureFormat
    );
    static Status (FSR2::*graphicsAPIIndependentSetOutputColorFunctionPointer)(
      void *,
      UnityRenderingExtTextureFormat
    );
    static Status (FSR2::*graphicsAPIIndependentEvaluateFunctionPointer)();
    static Status (FSR2::*graphicsAPIIndependentReleaseFunctionPointer)();
    static Status (FSR2::*graphicsAPIIndependentShutdownFunctionPointer)();

#    ifdef ENABLE_VULKAN
    Status VulkanInitialize();
    Status VulkanSetDepthBuffer(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status VulkanSetInputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status VulkanSetMotionVectors(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status VulkanSetOutputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status VulkanEvaluate();
    Status VulkanRelease();
    Status VulkanShutdown();
#    endif

    void setFunctionPointers(GraphicsAPI::Type graphicsAPI) override;

    static void log(FfxFsr2MsgType type, const wchar_t* message);

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
    Status setDepthBuffer(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) override;
    Status setInputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) override;
    Status setMotionVectors(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) override;
    Status setOutputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) override;
    Status evaluate() override;
    Status release() override;
};
#endif