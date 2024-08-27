#pragma once

#include "Upscaler.hpp"

class None_Upscaler final : public Upscaler {
public:
    static bool isSupported();
    static bool isSupported(enum Settings::Quality mode);

    None_Upscaler();
    None_Upscaler(const None_Upscaler&)            = delete;
    None_Upscaler(None_Upscaler&&)                 = delete;
    None_Upscaler& operator=(const None_Upscaler&) = delete;
    None_Upscaler& operator=(None_Upscaler&&)      = delete;
    ~None_Upscaler() override                   = default;

    constexpr Type getType() override {
        return NONE;
    }

    constexpr std::string getName() override {
        return "No upscaler";
    }

    Status useSettings(Settings::Resolution resolution, Settings::DLSSPreset /*unused*/, enum Settings::Quality /*unused*/, bool /*unused*/) override;
    Status useImages(const std::array<void*, Plugin::NumImages>& /*unused*/) override;
    Status evaluate() override;

    bool resetStatus() override;
};