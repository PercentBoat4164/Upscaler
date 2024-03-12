#pragma once
#ifdef ENABLE_FSR2
// Project
#    include "Upscaler.hpp"

// Upscaler
#    include <ffx_fsr2.h>

class FSR2 final : public Upscaler {
    FfxInterface     ffxInterface;
    FfxFsr2Context   context;
    FfxDevice        device;

    static Status (FSR2::*fpInitialize)();
    static Status (FSR2::*fpEvaluate)();

    static SupportState supported;

#    ifdef ENABLE_VULKAN
    Status VulkanInitialize();
    Status VulkanGetResource(FfxResource& resource, Plugin::ImageID imageID);
    Status VulkanEvaluate();
#    endif

#    ifdef ENABLE_DX12
    Status DX12Initialize();
    Status DX12GetResource(FfxResource& resource, Plugin::ImageID imageID);
    Status DX12Evaluate();
#    endif

    Status setStatus(FfxErrorCode t_error, const std::string &t_msg);

    static void log(FfxMsgType type, const wchar_t *t_msg);

public:
    explicit FSR2(GraphicsAPI::Type);
    ~FSR2() final;

#    ifdef ENABLE_VULKAN
    static std::vector<std::string> requestVulkanInstanceExtensions(const std::vector<std::string>& /*unused*/);
    static std::vector<std::string> requestVulkanDeviceExtensions(VkInstance /*unused*/, VkPhysicalDevice /*unused*/, const std::vector<std::string>& /*unused*/);
#    endif

    constexpr Type getType() final {
        return Upscaler::FSR2;
    };

    constexpr std::string getName() final {
        return "AMD FidelityFX Super Resolution";
    };

    bool   isSupported() final;
    Status getOptimalSettings(Settings::Resolution resolution, Settings::Preset /*unused*/, enum Settings::Quality mode, bool hdr) override;

    Status initialize() final;
    Status create() final;
    Status evaluate() final;
    Status shutdown() final;
};
#endif