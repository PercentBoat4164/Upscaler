#pragma once
#ifdef ENABLE_FSR3
#    include "GraphicsAPI/GraphicsAPI.hpp"
#    include "Upscaler.hpp"

#    include <ffx_fsr3.h>

class FSR3 final : public Upscaler {
    static uint32_t                     users;
    static std::array<FfxInterface*, 3> ffxInterfaces;
    FfxFsr3Context*                     context{};
    FfxSwapchain                        swapchain{};
    FfxDevice                           device{};

    static Status (FSR3::*fpInitialize)();
    static Status (FSR3::*fpEvaluate)();

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
    static bool isSupported();
    static bool isSupported(enum Settings::Quality mode);

    explicit FSR3(GraphicsAPI::Type type);
    FSR3(const FSR3&)            = delete;
    FSR3(FSR3&&)                 = delete;
    FSR3& operator=(const FSR3&) = delete;
    FSR3& operator=(FSR3&&)      = delete;
    ~FSR3() override;

    constexpr Type getType() override {
        return Upscaler::FSR3;
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