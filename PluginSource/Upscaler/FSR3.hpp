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
    static Status (FSR3::*fpSetResources)(const std::array<void*, Plugin::NumImages>&);
    static Status (FSR3::*fpGetCommandBuffer)(void*&);

    ffx::Context context{};
    std::array<FfxApiResource, Plugin::NumImages> resources{};

#    ifdef ENABLE_VULKAN
    Status VulkanCreate(ffx::CreateContextDescUpscale& createContextDescUpscale);
    Status VulkanSetResources(const std::array<void*, Plugin::NumImages>& images);
    Status VulkanGetCommandBuffer(void*& commandBuffer);
#    endif

#    ifdef ENABLE_DX12
    Status DX12Create(ffx::CreateContextDescUpscale& createContextDescUpscale);
    Status DX12SetResources(const std::array<void*, Plugin::NumImages>& images);
    Status DX12GetCommandBuffer(void*& commandList);
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
    Status useImages(const std::array<void*, Plugin::NumImages>& images) override;
    Status evaluate() override;
};
#endif