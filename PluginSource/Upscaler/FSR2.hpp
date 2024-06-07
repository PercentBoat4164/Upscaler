#pragma once
#ifdef ENABLE_FSR2
#    include "GraphicsAPI/GraphicsAPI.hpp"
#    include "Upscaler.hpp"

#    include <ffx_fsr2.h>

class FSR2 final : public Upscaler {
    static uint32_t         users;
    static FfxInterface*    ffxInterface;
    FfxFsr2Context*         context{};
    FfxDevice               device{};

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
#    ifdef ENABLE_VULKAN
    static std::vector<std::string> requestVulkanInstanceExtensions(const std::vector<std::string>& /*unused*/);
    static std::vector<std::string> requestVulkanDeviceExtensions(VkInstance /*unused*/, VkPhysicalDevice /*unused*/, const std::vector<std::string>& /*unused*/);
#    endif

    static bool isSupported();
    static bool isSupported(enum Settings::Quality mode);

    explicit FSR2(GraphicsAPI::Type type);
    FSR2(const FSR2&)            = delete;
    FSR2(FSR2&&)                 = delete;
    FSR2& operator=(const FSR2&) = delete;
    FSR2& operator=(FSR2&&)      = delete;
    ~FSR2() override;

    constexpr Type getType() override {
        return Upscaler::FSR2;
    }

    constexpr std::string getName() override {
        return "AMD FidelityFX Super Resolution";
    }

    Status getOptimalSettings(Settings::Resolution resolution, Settings::Preset /*unused*/, enum Settings::Quality mode, bool hdr) override;

    Status initialize() override;
    Status create() override;
    Status evaluate() override;
    Status shutdown() override;
};
#endif