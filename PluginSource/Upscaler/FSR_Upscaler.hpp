#pragma once
#ifdef ENABLE_FSR
#    include "GraphicsAPI/GraphicsAPI.hpp"
#    include "Upscaler.hpp"
#    include "Plugin.hpp"

#    include <ffx_upscale.h>
#    include <ffx_api.h>

#    define NOMINMAX
#    include <Windows.h>

#    include <array>

namespace ffx { struct CreateContextDescUpscale; }  // namespace ffx
struct FfxApiResource;

class FSR_Upscaler final : public Upscaler {
    static HMODULE library;
    static bool loaded;

    static Status (FSR_Upscaler::*fpCreate)(ffxCreateContextDescUpscale&);
    static Status (FSR_Upscaler::*fpSetResources)(const std::array<void*, 6>&);
    static Status (*fpGetCommandBuffer)(void*&);

    ffxContext context{};
    std::array<FfxApiResource, 6> resources{};

public:
    static PfnFfxCreateContext ffxCreateContext;
    static PfnFfxDestroyContext ffxDestroyContext;
    static PfnFfxConfigure ffxConfigure;
    static PfnFfxQuery ffxQuery;
    static PfnFfxDispatch ffxDispatch;

private:
#    ifdef ENABLE_VULKAN
    Status        VulkanCreate(ffxCreateContextDescUpscale& createContextDescUpscale);
    Status        VulkanSetResources(const std::array<void*, 6>& images);
    static Status VulkanGetCommandBuffer(void*& commandBuffer);
#    endif

#    ifdef ENABLE_DX12
    Status        DX12Create(ffxCreateContextDescUpscale& createContextDescUpscale);
    Status        DX12SetResources(const std::array<void*, 6>& images);
    static Status DX12GetCommandBuffer(void*& commandList);
#    endif

    static Status setStatus(ffxReturnCode_t t_error);
    static void log(FfxApiMsgType /*unused*/, const wchar_t *t_msg);

    [[nodiscard]] FfxApiUpscaleQualityMode getQuality(enum Quality quality) const;

public:
    float frameTime;
    float sharpness;
    float reactiveValue;
    float reactiveScale;
    float reactiveThreshold;
    float farPlane;
    float nearPlane;
    float verticalFOV;
    bool debugView;
    bool autoReactive;

    static bool loadedCorrectly();
    static void load(GraphicsAPI::Type type, void*);
    static void unload();
    static void useGraphicsAPI(GraphicsAPI::Type type);

    ~FSR_Upscaler() override;

    Status useSettings(Resolution resolution, enum Quality mode, bool hdr);
    Status useImages(const std::array<void*, 6>& images);
    Status evaluate(Resolution inputResolution);
};
#endif