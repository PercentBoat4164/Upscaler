#pragma once
#ifdef ENABLE_XESS
#    include "GraphicsAPI/GraphicsAPI.hpp"
#    include "Upscaler.hpp"

#    include <xess/xess.h>
#    ifdef ENABLE_DX12
#        include <xess/xess_d3d12.h>
#    endif

#    include <windows.h>

class XeSS_Upscaler final : public Upscaler {
    static HMODULE library;
    static SupportState supported;

    static Status (XeSS_Upscaler::*fpCreate)(const xess_d3d12_init_params_t*);
    static Status (XeSS_Upscaler::*fpEvaluate)(const Settings::Resolution);

    xess_context_handle_t context{nullptr};
    std::array<void*, Plugin::NumBaseImages> resources{};

    static decltype(&xessGetOptimalInputResolution) xessGetOptimalInputResolution;
    static decltype(&xessDestroyContext) xessDestroyContext;
    static decltype(&xessSetVelocityScale) xessSetVelocityScale;
    static decltype(&xessSetLoggingCallback) xessSetLoggingCallback;
    static decltype(&xessD3D12CreateContext) xessD3D12CreateContext;
    static decltype(&xessD3D12BuildPipelines) xessD3D12BuildPipelines;
    static decltype(&xessD3D12Init) xessD3D12Init;
    static decltype(&xessD3D12Execute) xessD3D12Execute;
    Status setStatus(xess_result_t t_error, const std::string &t_msg);

    static void log(const char* msg, xess_logging_level_t loggingLevel);

#    ifdef ENABLE_DX12
    Status DX12Create(const xess_d3d12_init_params_t*);
    Status DX12Evaluate(Settings::Resolution inputResolution);
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
