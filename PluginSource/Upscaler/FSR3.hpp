#pragma once
#ifdef ENABLE_FSR3
#    include "GraphicsAPI/GraphicsAPI.hpp"
#    include "Upscaler.hpp"

#    include <ffx_api/ffx_api.hpp>

#    include <windows.h>

namespace ffx { struct CreateContextDescUpscale; }  // namespace ffx
struct FfxApiResource;

class FSR3 final : public Upscaler {
    ffx::Context   context{};

    static HMODULE library;
    static std::atomic<uint32_t> users;

    static Status (FSR3::*fpCreate)(ffx::CreateContextDescUpscale&);
    static Status (FSR3::*fpEvaluate)();

    static SupportState supported;

#    ifdef ENABLE_VULKAN
    Status VulkanCreate(ffx::CreateContextDescUpscale& createContextDescUpscale);
    Status VulkanGetResource(FfxApiResource& resource, Plugin::ImageID imageID);
    Status VulkanEvaluate();
#    endif

#    ifdef ENABLE_DX12
    Status DX12Create(ffx::CreateContextDescUpscale& createContextDescUpscale);
    Status DX12GetResource(FfxApiResource& resource, Plugin::ImageID imageID);
    Status DX12Evaluate();
#    endif

    Status setStatus(ffx::ReturnCode t_error, const std::string &t_msg);

    static void log(FfxApiMsgType type, const wchar_t *t_msg);

public:
    static bool isSupported();
    static bool isSupported(enum Settings::Quality mode);
    static void useGraphicsAPI(GraphicsAPI::Type type);

    FSR3();
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

    Status useSettings(Settings::Resolution resolution, Settings::DLSSPreset /*unused*/, enum Settings::Quality mode, bool hdr) override;
    Status evaluate() override;
};
#endif