#pragma once
#ifdef ENABLE_DLSS
#    include "GraphicsAPI/GraphicsAPI.hpp"
#    include "Upscaler.hpp"

#    define NOMINMAX
#    include <windows.h>

struct NVSDK_NGX_Resource_VK;
struct NVSDK_NGX_Parameter;
struct NVSDK_NGX_DLSS_Create_Params;

class DLSS_Upscaler final : public Upscaler {
    static HMODULE library;
    static SupportState supported;

    static uint64_t applicationID;
    static uint32_t users;

    static void* (*fpGetDevice)();
    static Status (DLSS_Upscaler::*fpSetResources)(const std::array<void*, Plugin::NumImages>& images);
    static Status (DLSS_Upscaler::*fpGetCommandBuffer)(void*&);

    sl::ViewportHandle handle{0};
    std::array<sl::Resource, Plugin::NumBaseImages> resources{};

    static decltype(&slInit)                   slInit;
    static decltype(&slSetD3DDevice)           slSetD3DDevice;
    static decltype(&slSetFeatureLoaded)       slSetFeatureLoaded;
    static decltype(&slGetFeatureFunction)     slGetFeatureFunction;
    static decltype(&slDLSSGetOptimalSettings) slDLSSGetOptimalSettings;
    static decltype(&slDLSSSetOptions)         slDLSSSetOptions;
    static decltype(&slSetTag)                 slSetTag;
    static decltype(&slGetNewFrameToken)       slGetNewFrameToken;
    static decltype(&slSetConstants)           slSetConstants;
    static decltype(&slEvaluateFeature)        slEvaluateFeature;
    static decltype(&slFreeResources)          slFreeResources;
    static decltype(&slShutdown)               slShutdown;

    Status setStatus(sl::Result t_error, const std::string& t_msg);

    static void log(sl::LogType type, const char* msg);

#    ifdef ENABLE_VULKAN
    Status VulkanSetResources(const std::array<void*, Plugin::NumImages>& images);
    Status VulkanGetCommandBuffer(void*& commandBuffer);
#    endif

#    ifdef ENABLE_DX12
    static void* DX12GetDevice();
    Status       DX12SetResources(const std::array<void*, Plugin::NumImages>& images);
    Status       DX12GetCommandBuffer(void*& commandList);
#    endif

#    ifdef ENABLE_DX11
    static void* DX11GetDevice();
    Status       DX11SetResources(const std::array<void*, Plugin::NumImages>& images);
    Status       DX11GetCommandBuffer(void*& deviceContext);
#    endif

public:
    static void load(GraphicsAPI::Type type, void* vkGetProcAddrFunc);
    static void shutdown();
    static void unload();
    static bool isSupported();
    static bool isSupported(enum Settings::Quality mode);
    static void useGraphicsAPI(GraphicsAPI::Type type);

    DLSS_Upscaler();
    DLSS_Upscaler(const DLSS_Upscaler&)            = delete;
    DLSS_Upscaler(DLSS_Upscaler&&)                 = delete;
    DLSS_Upscaler& operator=(const DLSS_Upscaler&) = delete;
    DLSS_Upscaler& operator=(DLSS_Upscaler&&)      = delete;
    ~DLSS_Upscaler() override;

    constexpr Type getType() override {
        return DLSS;
    }

    constexpr std::string getName() override {
        return "NVIDIA Deep Learning Super Sampling";
    }

    Status useSettings(Settings::Resolution resolution, Settings::DLSSPreset preset, enum Settings::Quality mode, bool hdr) override;
    Status useImages(const std::array<void*, Plugin::NumImages>& images) override;
    Status evaluate(Settings::Resolution inputResolution) override;
};
#endif