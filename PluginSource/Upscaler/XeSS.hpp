#pragma once
#ifdef ENABLE_XESS
#    include "GraphicsAPI/GraphicsAPI.hpp"
#    include "Upscaler.hpp"

#    include <xess/xess.h>

struct ID3D12Resource;

class XeSS final : public Upscaler {
    xess_context_handle_t context{};

    static Status (XeSS::*fpInitialize)();
    static Status (XeSS::*fpCreate)();
    static Status (XeSS::*fpEvaluate)();

    static SupportState supported;

#    ifdef ENABLE_DX12
    Status DX12Initialize();
    Status DX12Create();
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

    Status getOptimalSettings(Settings::Resolution resolution, Settings::Preset /*unused*/, enum Settings::Quality mode, bool hdr) override;

    Status initialize() override;
    Status create() override;
    Status evaluate() override;
    Status shutdown() override;
};
#endif
