#pragma once
#ifdef ENABLE_XESS
#    include "GraphicsAPI/GraphicsAPI.hpp"
#    include "Upscaler.hpp"

#    include <xess/xess.h>

using xess_d3d12_init_params_t = struct _xess_d3d12_init_params_t;

class XeSS final : public Upscaler {
    xess_context_handle_t context{nullptr};

    static Status (XeSS::*fpCreate)(const xess_d3d12_init_params_t*);
    static Status (XeSS::*fpEvaluate)();

    static SupportState supported;

#    ifdef ENABLE_DX12
    Status DX12Create(const xess_d3d12_init_params_t*);
    Status DX12Evaluate();
#    endif

    Status setStatus(xess_result_t t_error, const std::string &t_msg);

    static void log(const char* message, xess_logging_level_t loggingLevel);

public:
    static bool isSupported();
    static bool isSupported(enum Settings::Quality mode);

    explicit XeSS(GraphicsAPI::Type type);
    XeSS(const XeSS&)            = delete;
    XeSS(XeSS&&)                 = delete;
    XeSS& operator=(const XeSS&) = delete;
    XeSS& operator=(XeSS&&)      = delete;
    ~XeSS() override;

    constexpr Type getType() override {
        return XESS;
    }

    constexpr std::string getName() override {
        return "Intel Xe Super Sampling";
    }

    Status useSettings(Settings::Resolution resolution, Settings::DLSSPreset /*unused*/, enum Settings::Quality mode, bool hdr) override;

    Status evaluate() override;
};
#endif
