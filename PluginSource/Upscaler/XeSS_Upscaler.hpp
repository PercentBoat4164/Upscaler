#pragma once
#ifdef ENABLE_XESS
#    include "GraphicsAPI/GraphicsAPI.hpp"
#    include "Upscaler.hpp"

#    include <xess/xess.h>
#    ifdef ENABLE_VULKAN
#        include <xess/xess_vk.h>
#    endif
#    ifdef ENABLE_DX12
#        include <xess/xess_d3d12.h>
#    endif
#    ifdef ENABLE_DX11
#        include <xess/xess_d3d11.h>
#    endif

#    include <windows.h>

class XeSS_Upscaler final : public Upscaler {
    union XeSSResource {
        xess_vk_image_view_info vulkan;
        ID3D12Resource* dx12;
        ID3D11Resource* dx11;
    };
    static HMODULE library;
    static HMODULE dx11library;
    static SupportState supported;

    static Status (XeSS_Upscaler::*fpCreate)(const void*);
    static Status (XeSS_Upscaler::*fpSetImages)(const std::array<void*, Plugin::NumImages>&);
    static Status (XeSS_Upscaler::*fpEvaluate)(Settings::Resolution);

    xess_context_handle_t context{nullptr};
    std::array<XeSSResource, Plugin::NumBaseImages> resources{};

    static decltype(&xessGetOptimalInputResolution) xessGetOptimalInputResolution;
    static decltype(&xessDestroyContext) xessDestroyContext;
    static decltype(&xessSetVelocityScale) xessSetVelocityScale;
    static decltype(&xessSetLoggingCallback) xessSetLoggingCallback;
#ifdef ENABLE_VULKAN
    static decltype(&xessVKCreateContext) xessVKCreateContext;
    static decltype(&xessVKBuildPipelines) xessVKBuildPipelines;
    static decltype(&xessVKInit) xessVKInit;
    static decltype(&xessVKExecute) xessVKExecute;
#endif
#ifdef ENABLE_DX12
    static decltype(&xessD3D12CreateContext) xessD3D12CreateContext;
    static decltype(&xessD3D12BuildPipelines) xessD3D12BuildPipelines;
    static decltype(&xessD3D12Init) xessD3D12Init;
    static decltype(&xessD3D12Execute) xessD3D12Execute;
#endif
#ifdef ENABLE_DX11
    static decltype(&xessD3D11CreateContext) xessD3D11CreateContext;
    static decltype(&xessD3D11Init) xessD3D11Init;
    static decltype(&xessD3D11Execute) xessD3D11Execute;
#endif
    Status setStatus(xess_result_t t_error, const std::string &t_msg);

    static void log(const char* msg, xess_logging_level_t loggingLevel);

#    ifdef ENABLE_VULKAN
    Status VulkanCreate(const void*);
    Status VulkanSetImages(const std::array<void*, Plugin::NumImages>&);
    Status VulkanEvaluate(Settings::Resolution inputResolution);
#    endif
#    ifdef ENABLE_DX12
    Status DX12Create(const void*);
    Status DX12SetImages(const std::array<void*, Plugin::NumImages>&);
    Status DX12Evaluate(Settings::Resolution inputResolution);
#    endif
#    ifdef ENABLE_DX11
    Status DX11Create(const void*);
    Status DX11SetImages(const std::array<void*, Plugin::NumImages>&);
    Status DX11Evaluate(Settings::Resolution inputResolution);
#    endif

public:
    static void load(GraphicsAPI::Type type, void*);
    static void shutdown();
    static void unload();
    static bool isSupported();
    static bool isSupported(enum Settings::Quality mode);
    static void useGraphicsAPI(GraphicsAPI::Type type);

    XeSS_Upscaler()                                = default;
    XeSS_Upscaler(const XeSS_Upscaler&)            = delete;
    XeSS_Upscaler(XeSS_Upscaler&&)                 = delete;
    XeSS_Upscaler& operator=(const XeSS_Upscaler&) = delete;
    XeSS_Upscaler& operator=(XeSS_Upscaler&&)      = delete;
    ~XeSS_Upscaler() override;

    constexpr Type getType() override {
        return XESS;
    }

    constexpr std::string getName() override {
        return "Intel Xe Super Sampling";
    }

    Status useSettings(Settings::Resolution resolution, Settings::DLSSPreset /*unused*/, enum Settings::Quality mode, bool hdr) override;
    Status useImages(const std::array<void*, Plugin::NumImages>& images) override;
    Status evaluate(Settings::Resolution inputResolution) override;
};
#endif
