#pragma once
#ifdef ENABLE_FSR
#    include "GraphicsAPI/GraphicsAPI.hpp"
#    include "Upscaler.hpp"

#    include <ffx_api.h>

namespace ffx { struct CreateContextDescUpscale; }  // namespace ffx
struct FfxApiResource;

class FSR_Upscaler final : public Upscaler {
    static HMODULE library;
    static SupportState supported;

    static Status (FSR_Upscaler::*fpCreate)(ffxCreateContextDescUpscale&);
    static Status (FSR_Upscaler::*fpSetResources)(const std::array<void*, Plugin::NumImages>&);
    static Status (FSR_Upscaler::*fpGetCommandBuffer)(void*&);

    ffxContext context{};
    std::array<FfxApiResource, Plugin::NumImages> resources{};

    static PfnFfxCreateContext ffxCreateContext;
    static PfnFfxDestroyContext ffxDestroyContext;
    static PfnFfxConfigure ffxConfigure;
    static PfnFfxQuery ffxQuery;
    static PfnFfxDispatch ffxDispatch;

#    ifdef ENABLE_VULKAN
    Status VulkanCreate(ffxCreateContextDescUpscale& createContextDescUpscale);
    Status VulkanSetResources(const std::array<void*, Plugin::NumImages>& images);
    Status VulkanGetCommandBuffer(void*& commandBuffer);
#    endif

#    ifdef ENABLE_DX12
    Status DX12Create(ffxCreateContextDescUpscale& createContextDescUpscale);
    Status DX12SetResources(const std::array<void*, Plugin::NumImages>& images);
    Status DX12GetCommandBuffer(void*& commandList);
#    endif

    Status setStatus(ffxReturnCode_t t_error, const std::string& t_msg);

    static void log(FfxApiMsgType type, const wchar_t *t_msg);

public:
    static void load(GraphicsAPI::Type type, void*);
    static void unload();
    static bool isSupported();
    static bool isSupported(enum Settings::Quality mode);
    static void useGraphicsAPI(GraphicsAPI::Type type);

    FSR_Upscaler()                               = default;
    FSR_Upscaler(const FSR_Upscaler&)            = delete;
    FSR_Upscaler(FSR_Upscaler&&)                 = delete;
    FSR_Upscaler& operator=(const FSR_Upscaler&) = delete;
    FSR_Upscaler& operator=(FSR_Upscaler&&)      = delete;
    ~FSR_Upscaler() override;

    constexpr Type getType() override { return FSR; }
    constexpr std::string getName() override { return "AMD FidelityFX Super Resolution"; }

    Status useSettings(Settings::Resolution resolution, Settings::DLSSPreset /*unused*/, enum Settings::Quality mode, bool hdr) override;
    Status useImages(const std::array<void*, Plugin::NumImages>& images) override;
    Status evaluate(Settings::Resolution inputResolution) override;
};
#endif