#pragma once
#ifdef ENABLE_FSR3
#    include "GraphicsAPI/GraphicsAPI.hpp"
#    include "Upscaler.hpp"

#    include <ffx_api/ffx_api.hpp>

#    include <Windows.h>

namespace ffx { struct CreateContextDescUpscale; }  // namespace ffx
struct FfxApiResource;

class FSR3 final : public Upscaler {
    static HMODULE library;
    static uint32_t users;
    static SupportState supported;

    static Status (FSR3::*fpCreate)(ffx::CreateContextDescUpscale&);
    static Status (FSR3::*fpGetResources)(std::array<FfxApiResource, 6>&, void*&);

    ffx::Context context{};

#    ifdef ENABLE_VULKAN
    Status VulkanCreate(ffx::CreateContextDescUpscale& createContextDescUpscale);
    Status VulkanGetResource(FfxApiResource& resource, Plugin::ImageID imageID);
    Status VulkanGetResources(std::array<FfxApiResource, 6>& resources, void*& commandBuffer);
    Status VulkanEvaluate();
#    endif

#    ifdef ENABLE_DX12
    Status DX12Create(ffx::CreateContextDescUpscale& createContextDescUpscale);
    Status DX12GetResources(std::array<FfxApiResource, 6>& resources, void*& commandList);
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
        return "AMD FidelityFx Super Resolution";
    }

    Status useSettings(Settings::Resolution resolution, Settings::DLSSPreset /*unused*/, enum Settings::Quality mode, bool hdr) override;
    Status evaluate() override;
};
#endif