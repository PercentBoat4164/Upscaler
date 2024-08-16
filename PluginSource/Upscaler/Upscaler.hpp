#pragma once

#include "Plugin.hpp"

#include "GraphicsAPI/GraphicsAPI.hpp"

#ifdef ENABLE_VULKAN
#    include <vulkan/vulkan.h>
#endif

#ifdef ENABLE_DLSS
#    include <sl.h>
#    include <sl_dlss.h>
#endif
#ifdef ENABLE_FSR3
#    include <ffx_api/ffx_upscale.h>
#endif
#ifdef ENABLE_XESS
#    include <xess/xess.h>
#endif

#include "IUnityLog.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#define RETURN_ON_FAILURE(x)            \
{                                       \
    Upscaler::Status _ = x;             \
    if (Upscaler::failure(_)) return _; \
}                                       \
0
#define RETURN_VOID_ON_FAILURE(x) if (Upscaler::failure(x)) return

struct alignas(512) UpscalerBase {
    constexpr static uint8_t SamplesPerPixel = 8U;

    enum Type {
        NONE,
        DLSS,
        FSR3,
        XESS,
        TYPE_MAX_ENUM
    };

    struct alignas(128) Settings final {
        enum DLSSPreset : uint8_t {
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

        struct alignas(8) Resolution {
            uint32_t width;
            uint32_t height;
        } recommendedInputResolution, dynamicMaximumInputResolution, dynamicMinimumInputResolution, outputResolution;

        struct alignas(16) JitterState {
            uint8_t  base{};
            uint32_t n = 0U;
            uint32_t d = 1U;
            uint32_t iterations{-1U};

            float advance(uint32_t maxIterations);
        } x{2U}, y{3U};

        struct alignas(8) Jitter {
            float x, y;
        } jitter;

        float reactiveValue{};
        float reactiveScale{};
        float reactiveThreshold{};
        float sharpness{};
        float frameTime{};
        float farPlane;
        float nearPlane;
        float verticalFOV;
        std::array<float, 16> viewToClip;
        std::array<float, 16> clipToView;
        std::array<float, 16> clipToPrevClip;
        std::array<float, 16> prevClipToClip;
        std::array<float, 3> position;
        std::array<float, 3> up;
        std::array<float, 3> right;
        std::array<float, 3> forward;
        bool autoReactive{};
        bool orthographic{};
        bool debugView{};
        bool hdr{};
        bool resetHistory{};

        Jitter& getNextJitter(float inputWidth);

#ifdef ENABLE_DLSS
        template<Type T, typename = std::enable_if_t<T == DLSS>>
        [[nodiscard]] sl::DLSSMode getQuality(const enum Quality quality) const {
            switch (quality) {
                case Auto: {  // See page 7 of 'RTX UI Developer Guidelines.pdf'
                    const uint32_t pixelCount {outputResolution.width * outputResolution.height};
                    if (pixelCount <= 2560U * 1440U) return sl::DLSSMode::eMaxQuality;
                    if (pixelCount <= 3840U * 2160U) return sl::DLSSMode::eMaxPerformance;
                    return sl::DLSSMode::eUltraPerformance;
                }
                case AntiAliasing: return sl::DLSSMode::eDLAA;
                case Quality: return sl::DLSSMode::eMaxQuality;
                case Balanced: return sl::DLSSMode::eBalanced;
                case Performance: return sl::DLSSMode::eMaxPerformance;
                case UltraPerformance: return sl::DLSSMode::eUltraPerformance;
                default: return static_cast<sl::DLSSMode>(-1);
            }
        }
#endif
#ifdef ENABLE_FSR3
        template<Type T, typename = std::enable_if_t<T == FSR3>>
        [[nodiscard]] FfxApiUpscaleQualityMode getQuality(const enum Quality quality) const {
            switch (quality) {
                case Auto: {
                    const uint32_t pixelCount {outputResolution.width * outputResolution.height};
                    if (pixelCount <= 2560U * 1440U) return FFX_UPSCALE_QUALITY_MODE_QUALITY;
                    if (pixelCount <= 3840U * 2160U) return FFX_UPSCALE_QUALITY_MODE_PERFORMANCE;
                    return FFX_UPSCALE_QUALITY_MODE_ULTRA_PERFORMANCE;
                }
                case AntiAliasing: return FFX_UPSCALE_QUALITY_MODE_NATIVEAA;
                case Quality: return FFX_UPSCALE_QUALITY_MODE_QUALITY;
                case Balanced: return FFX_UPSCALE_QUALITY_MODE_BALANCED;
                case Performance: return FFX_UPSCALE_QUALITY_MODE_PERFORMANCE;
                case UltraPerformance: return FFX_UPSCALE_QUALITY_MODE_ULTRA_PERFORMANCE;
                default: return static_cast<FfxApiUpscaleQualityMode>(-1);
            }
        }
#endif
#ifdef ENABLE_XESS
        template<Type T, typename = std::enable_if_t<T == XESS>>
        [[nodiscard]] xess_quality_settings_t getQuality(const enum Quality quality) const {
            switch (quality) {
                case Auto: {
                    const uint32_t pixelCount {outputResolution.width * outputResolution.height};
                    if (pixelCount <= 2560U * 1440U) return XESS_QUALITY_SETTING_QUALITY;
                    if (pixelCount <= 3840U * 2160U) return XESS_QUALITY_SETTING_PERFORMANCE;
                    return XESS_QUALITY_SETTING_ULTRA_PERFORMANCE;
                }
                case AntiAliasing: return XESS_QUALITY_SETTING_AA;
                case UltraQualityPlus: return XESS_QUALITY_SETTING_ULTRA_QUALITY_PLUS;
                case UltraQuality: return XESS_QUALITY_SETTING_ULTRA_QUALITY;
                case Quality: return XESS_QUALITY_SETTING_QUALITY;
                case Balanced: return XESS_QUALITY_SETTING_BALANCED;
                case Performance: return XESS_QUALITY_SETTING_PERFORMANCE;
                case UltraPerformance: return XESS_QUALITY_SETTING_ULTRA_PERFORMANCE;
                default: return static_cast<xess_quality_settings_t>(-1);
            }
        }
#endif
    } settings;
};

class Upscaler : public UpscalerBase {
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

    static bool success(Status);
    static bool failure(Status);
    static bool recoverable(Status);

    enum SupportState {
        Untested,
        Unsupported,
        Supported
    };

private:
    Status      status{Success};
    std::string statusMessage;

protected:
    template<typename... Args> constexpr Status safeFail(Args... /*unused*/) {return setStatus(RecoverableRuntimeError, "Graphics initialization failed: `safeFail` was called!");}
    template<typename... Args> constexpr Status invalidGraphicsAPIFail(Args... /*unused*/) {return setStatus(UnsupportedGraphicsApi, getName() + " does not support the current graphics API.");}
    template<typename... Args> constexpr Status succeed(Args... /*unused*/) {return Success;}
    template<typename T, typename... Args> static constexpr T nullFunc(Args... /*unused*/) {return {};}

public:
#ifdef ENABLE_VULKAN
    static std::vector<std::string> requestVulkanInstanceExtensions(const std::vector<std::string>&);
    static std::vector<std::string> requestVulkanDeviceExtensions(VkInstance, VkPhysicalDevice, const std::vector<std::string>&);
#endif

    static bool                      isSupported(Type type);
    static bool                      isSupported(Type type, enum Settings::Quality mode);
    static std::unique_ptr<Upscaler> fromType(Type type);
    static void                      useGraphicsAPI(GraphicsAPI::Type type);
    static void                      setSupported();

    Upscaler()                           = default;
    Upscaler(const Upscaler&)            = delete;
    Upscaler(Upscaler&&)                 = delete;
    Upscaler& operator=(const Upscaler&) = delete;
    Upscaler& operator=(Upscaler&&)      = delete;
    virtual ~Upscaler()                  = default;

    constexpr virtual Type        getType()                                                                                = 0;
    constexpr virtual std::string getName()                                                                                = 0;

    virtual Status useSettings(Settings::Resolution, Settings::DLSSPreset, enum Settings::Quality, bool) = 0;
    virtual Status useImages(const std::array<void*, Plugin::NumImages>&)                                = 0;
    virtual Status evaluate()                                                                            = 0;

    [[nodiscard]] Status getStatus() const;
    std::string&         getErrorMessage();
    Status               setStatus(Status, const std::string&);
    Status               setStatusIf(bool, Status, std::string);
    virtual bool         resetStatus();
    void                 forceStatus(Status, std::string);
};