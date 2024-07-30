#pragma once
#ifdef ENABLE_DLSS
#    include "GraphicsAPI/GraphicsAPI.hpp"
#    include "Upscaler.hpp"

#    include <Windows.h>

struct NVSDK_NGX_Resource_VK;
struct NVSDK_NGX_Parameter;
struct NVSDK_NGX_DLSS_Create_Params;

class DLSS final : public Upscaler {
    static HMODULE library;
    static uint32_t users;
    static SupportState supported;

    static uint64_t applicationID;

    static Status (DLSS::*fpSetDevice)();
    static Status (DLSS::*fpGetResources)(std::array<sl::Resource, 4>&, void*&);

    sl::ViewportHandle handle{0};

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
    Status VulkanGetResources(std::array<sl::Resource, 4>& resources, void*& commandBuffer);
#    endif

#    ifdef ENABLE_DX12
    Status DX12GetResources(std::array<sl::Resource, 4>& resources, void*& commandList);
    Status DX12SetDevice();
#    endif

#    ifdef ENABLE_DX11
    Status DX11GetResources(std::array<sl::Resource, 4>& resources, void*& deviceContext);
    Status DX11SetDevice();
#    endif

public:
    static void load(void*& vkGetProcAddrFunc, GraphicsAPI::Type);
    static void unload();
    static bool isSupported();
    static bool isSupported(enum Settings::Quality mode);
    static void useGraphicsAPI(GraphicsAPI::Type type);

    DLSS();
    DLSS(const DLSS&)            = delete;
    DLSS(DLSS&&)                 = delete;
    DLSS& operator=(const DLSS&) = delete;
    DLSS& operator=(DLSS&&)      = delete;
    ~DLSS() override;

    constexpr Type getType() override {
        return Upscaler::DLSS;
    }

    constexpr std::string getName() override {
        return "NVIDIA Deep Learning Super Sampling";
    }

    Status useSettings(Settings::Resolution resolution, Settings::DLSSPreset preset, enum Settings::Quality mode, bool hdr) override;
    Status evaluate() override;
};
#endif