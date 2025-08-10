#pragma once
#ifdef ENABLE_DLSS
#    include "GraphicsAPI/GraphicsAPI.hpp"
#    include "Upscaler.hpp"
#    include "Plugin.hpp"

#    include <sl.h>
#    include <sl_dlss.h>

#    include <Windows.h>

#    include <array>

class DLSS_Upscaler final : public Upscaler {
    static HMODULE library;
    static bool loaded;

    static uint64_t applicationID;
    static uint32_t users;

    static void* (*fpGetDevice)();
    static Status (DLSS_Upscaler::*fpSetResources)(const std::array<void*, 4>& images);
    static Status (*fpGetCommandBuffer)(void*&);

    sl::ViewportHandle handle{0};
    std::array<sl::Resource, 4> resources{};

    static decltype(&slInit)                   slInit;
    static decltype(&slSetD3DDevice)           slSetD3DDevice;
    static decltype(&slSetFeatureLoaded)       slSetFeatureLoaded;
    static decltype(&slGetFeatureFunction)     slGetFeatureFunction;
    static decltype(&slDLSSGetOptimalSettings) slDLSSGetOptimalSettings;
    static decltype(&slDLSSSetOptions)         slDLSSSetOptions;
    static decltype(&slSetTagForFrame)         slSetTagForFrame;
    static decltype(&slGetNewFrameToken)       slGetNewFrameToken;
    static decltype(&slSetConstants)           slSetConstants;
    static decltype(&slEvaluateFeature)        slEvaluateFeature;
    static decltype(&slFreeResources)          slFreeResources;
    static decltype(&slShutdown)               slShutdown;

#    ifdef ENABLE_VULKAN
    Status        VulkanSetResources(const std::array<void*, 4>& images);
    static Status VulkanGetCommandBuffer(void*& commandBuffer);
#    endif

#    ifdef ENABLE_DX12
    static void*  DX12GetDevice();
    Status        DX12SetResources(const std::array<void*, 4>& images);
    static Status DX12GetCommandBuffer(void*& commandList);
#    endif

#    ifdef ENABLE_DX11
    static void*  DX11GetDevice();
    Status        DX11SetResources(const std::array<void*, 4>& images);
    static Status DX11GetCommandBuffer(void*& deviceContext);
#    endif

    static Status setStatus(sl::Result t_error);
    static void log(sl::LogType type, const char* msg);

    [[nodiscard]] sl::DLSSMode getQuality(enum Quality quality) const;

public:
    std::array<float, 16> viewToClip;
    std::array<float, 16> clipToView;
    std::array<float, 16> clipToPrevClip;
    std::array<float, 16> prevClipToClip;
    std::array<float, 3> position;
    std::array<float, 3> up;
    std::array<float, 3> right;
    std::array<float, 3> forward;
    float farPlane;
    float nearPlane;
    float verticalFOV;

    static bool loadedCorrectly();
    static void load(GraphicsAPI::Type type, void* vkGetProcAddrFunc);
    static void shutdown();
    static void unload();
    static void useGraphicsAPI(GraphicsAPI::Type type);

    DLSS_Upscaler();
    DLSS_Upscaler(const DLSS_Upscaler&)            = delete;
    DLSS_Upscaler(DLSS_Upscaler&&)                 = delete;
    DLSS_Upscaler& operator=(const DLSS_Upscaler&) = delete;
    DLSS_Upscaler& operator=(DLSS_Upscaler&&)      = delete;
    ~DLSS_Upscaler() override;

    Status useSettings(Resolution resolution, Preset preset, enum Quality mode, Flags flags);
    Status useImages(const std::array<void*, 4>& images);
    Status evaluate(Resolution inputResolution);
};
#endif