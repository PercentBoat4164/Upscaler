#pragma once

#include "GraphicsAPI/GraphicsAPI.hpp"

#include <vector>

#define RETURN_STATUS_WITH_MESSAGE_IF(x, status, message) \
if ((bool)(x)) {                                          \
    Plugin::log(message);                                 \
    return status;                                        \
}
#define RETURN_WITH_MESSAGE_IF(x, message) \
{                                          \
    Upscaler::Status status = x;           \
    if (status != Success) {               \
        Plugin::log(message);              \
        return status;                     \
    }                                      \
}
#define RETURN_VOID_WITH_MESSAGE_IF(x, message) \
if ((x) != Success) {                           \
    Plugin::log(message);                       \
    return;                                     \
}
#define RETURN_IF(x)                       \
    {                                      \
        Upscaler::Status status = x;       \
        if ((x) != Success) return status; \
    }

class Upscaler {
    constexpr static uint8_t ERROR_RECOVERABLE = 1U << 7U;

public:
    enum Status : uint8_t {
        Success                     = 0U | ERROR_RECOVERABLE,
        DeviceNotSupported          = 1U,
        DriversOutOfDate            = 2U,
        UnsupportedGraphicsApi      = 3U,
        OperatingSystemNotSupported = 4U,
        FeatureDenied               = 5U,
        OutOfMemory                 = 6U | ERROR_RECOVERABLE,
        LibraryNotLoaded            = 7U,
        RecoverableRuntimeError     = 8U | ERROR_RECOVERABLE,
        FatalRuntimeError           = 9U,
    };

    enum Preset : uint8_t {
        Default,
        Stable,
        FastPaced,
        AnitGhosting,
    };

    enum Quality : uint8_t {
        Auto,
        AntiAliasing,
        UltraQualityPlus,
        UltraQuality,
        Quality,
        Balanced,
        Performance,
        UltraPerformance,
    };

    struct Resolution {
        uint32_t width;
        uint32_t height;
    } recommendedInputResolution{}, dynamicMaximumInputResolution{}, dynamicMinimumInputResolution{}, outputResolution{};

    struct Jitter {
        float x;
        float y;
    } jitter{};

    enum Flags : uint8_t {
        None = 0U,
        OutputResolutionMotionVectors = 1U << 0U,
        EnableHDR = 1U << 1U
    };

    bool resetHistory{};

protected:
    template<auto val = FatalRuntimeError, typename... Args> constexpr auto safeFail(Args... /*unused*/) { return val; }
    template<auto val = FatalRuntimeError, typename... Args> constexpr auto safeFail(Args... /*unused*/) const { return val; }
    template<auto val = FatalRuntimeError, typename... Args> static constexpr auto staticSafeFail(Args... /*unused*/) { return val; }

public:
    static void load(GraphicsAPI::Type type, void* vkGetProcAddrFunc=nullptr);
    static void unload();
    static void useGraphicsAPI(GraphicsAPI::Type type);

    virtual ~Upscaler() = default;
};