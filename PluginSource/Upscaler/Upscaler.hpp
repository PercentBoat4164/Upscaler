#pragma once

#include "Plugin.hpp"

#ifdef ENABLE_VULKAN
#    include <vulkan/vulkan.h>
#endif

#ifdef ENABLE_DLSS
#    include <nvsdk_ngx_defs.h>
#endif
#ifdef ENABLE_FSR2
#    include <ffx_fsr2.h>
#endif
#ifdef ENABLE_XESS
#    include <xess/xess.h>
#endif

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#define RETURN_ON_FAILURE(x)        \
{                                       \
    Upscaler::Status _ = x;             \
    if (Upscaler::failure(_)) return _; \
}                                       \
0

struct alignas(128) UpscalerBase {
    constexpr static uint8_t SamplesPerPixel = 8U;

    enum Type {
        NONE,
        DLSS,
        FSR2,
        XESS,
        TYPE_MAX_ENUM
    };

    struct alignas(128) Settings final {
        enum Quality : uint8_t {
            Auto,
            AntiAliasing,
            UltraQualityPlus,
            UltraQuality,
            Quality,
            Balanced,
            Performance,
            UltraPerformance,
            QUALITY_MODE_MAX_ENUM
        } quality{};

        enum DLSSPreset : uint8_t {
            Default,
            Stable,
            FastPaced,
            AnitGhosting,
            PRESET_MAX_ENUM
        } preset{};

        struct alignas(8) Resolution {
            uint32_t width;
            uint32_t height;
        } recommendedInputResolution{}, dynamicMaximumInputResolution{}, dynamicMinimumInputResolution{}, outputResolution{};

        struct alignas(8) Jitter {
            float x;
            float y;
        } jitter{};

        // DO NOT CHANGE THE ALIGNMENT FROM 4!
        // This breaks Release mode builds.
        struct alignas(4) Camera {
            float farPlane;
            float nearPlane;
            float verticalFOV;
        } camera;

        float tcThreshold{};
        float tcScale{};
        float reactiveScale{};
        float reactiveMax{};
        float sharpness{};
        float frameTime{};
        bool  autoReactive{};
        bool  hdr{};
        bool  resetHistory{};

        Jitter getNextJitter() {
            static struct alignas(16) JitterState {
                uint8_t  base{};
                uint32_t n = 0U;
                uint32_t d = 1U;
                uint32_t iterations{};

                float advance(const uint32_t maxIterations) {
                    if (iterations >= maxIterations) {
                        n = 0U;
                        d = 1U;
                    }
                    const uint32_t x = d - n;
                    if (x == 1U) {
                        n = 1U;
                        d *= base;
                    } else {
                        uint32_t y = d / base;
                        while (x <= y) y /= base;
                        n = (base + 1U) * y - x;
                    }
                    return static_cast<float>(n) / static_cast<float>(d) - 0.5F;
                }
            } x{2U}, y{3U};

            const float scalingFactor = static_cast<float>(outputResolution.width) / static_cast<float>(recommendedInputResolution.width);
            const auto  jitterSamples = static_cast<uint32_t>(std::ceil(static_cast<float>(SamplesPerPixel) * scalingFactor * scalingFactor));
            jitter                    = {x.advance(jitterSamples), y.advance(jitterSamples)};
            return jitter;
        }

#ifdef ENABLE_DLSS
        template<Type T, typename = std::enable_if_t<T == DLSS>>
        [[nodiscard]] NVSDK_NGX_PerfQuality_Value getQuality() const {
            switch (quality) {
                case Auto: {  // See page 7 of 'RTX UI Developer Guidelines.pdf'
                    const uint32_t pixelCount {outputResolution.width * outputResolution.height};
                    if (pixelCount <= 2560U * 1440U) return NVSDK_NGX_PerfQuality_Value_MaxQuality;
                    if (pixelCount <= 3840U * 2160U) return NVSDK_NGX_PerfQuality_Value_MaxPerf;
                    return NVSDK_NGX_PerfQuality_Value_UltraPerformance;
                }
                case AntiAliasing: return NVSDK_NGX_PerfQuality_Value_DLAA;
                case Quality: return NVSDK_NGX_PerfQuality_Value_MaxQuality;
                case Balanced: return NVSDK_NGX_PerfQuality_Value_Balanced;
                case Performance: return NVSDK_NGX_PerfQuality_Value_MaxPerf;
                case UltraPerformance: return NVSDK_NGX_PerfQuality_Value_UltraPerformance;
                default: return static_cast<NVSDK_NGX_PerfQuality_Value>(-1);
            }
        }
#endif
#ifdef ENABLE_FSR2
        template<Type T, typename = std::enable_if_t<T == FSR2>>
        [[nodiscard]] FfxFsr2QualityMode getQuality() const {
            switch (quality) {
                case Auto: {
                    const uint32_t pixelCount {outputResolution.width * outputResolution.height};
                    if (pixelCount <= 2560U * 1440U) return FFX_FSR2_QUALITY_MODE_QUALITY;
                    if (pixelCount <= 3840U * 2160U) return FFX_FSR2_QUALITY_MODE_PERFORMANCE;
                    return FFX_FSR2_QUALITY_MODE_ULTRA_PERFORMANCE;
                }
                case Quality: return FFX_FSR2_QUALITY_MODE_QUALITY;
                case Balanced: return FFX_FSR2_QUALITY_MODE_BALANCED;
                case Performance: return FFX_FSR2_QUALITY_MODE_PERFORMANCE;
                case UltraPerformance: return FFX_FSR2_QUALITY_MODE_ULTRA_PERFORMANCE;
                default: return static_cast<FfxFsr2QualityMode>(-1);
            }
        }
#endif
#ifdef ENABLE_XESS
        template<Type T, typename = std::enable_if_t<T == XESS>>
        [[nodiscard]] xess_quality_settings_t getQuality() const {
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

protected:
    std::array<UnityTextureID, Plugin::IMAGE_ID_MAX_ENUM> textureIDs{};
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
        RecoverableRuntimeError     = 7U | ERROR_RECOVERABLE,
        FatalRuntimeError           = 8U,
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
    template<typename... Args>
    constexpr Status safeFail(Args... /*unused*/) {
        return setStatus(RecoverableRuntimeError, "Graphics initialization failed: `safeFail` was called!");
    }

    template<typename... Args>
    constexpr Status invalidGraphicsAPIFail(Args... /*unused*/) {
        return setStatus(UnsupportedGraphicsApi, getName() + " does not support the current graphics API.");
    }

    static void (*logCallback)(const char* msg);

public:
#ifdef ENABLE_VULKAN
    static std::vector<std::string> requestVulkanInstanceExtensions(const std::vector<std::string>&);
    static std::vector<std::string> requestVulkanDeviceExtensions(VkInstance, VkPhysicalDevice, const std::vector<std::string>&);
#endif

    static bool                      isSupported(Type type);
    static bool                      isSupported(Type type, enum Settings::Quality mode);
    static std::unique_ptr<Upscaler> fromType(Type type);
    static void                      setLogCallback(void (*pFunction)(const char*));

    Upscaler()                           = default;
    Upscaler(const Upscaler&)            = delete;
    Upscaler(Upscaler&&)                 = delete;
    Upscaler& operator=(const Upscaler&) = delete;
    Upscaler& operator=(Upscaler&&)      = delete;
    virtual ~Upscaler()                  = default;

    constexpr virtual Type        getType()                                                                                = 0;
    constexpr virtual std::string getName()                                                                                = 0;
    virtual Status                getOptimalSettings(Settings::Resolution, Settings::DLSSPreset, enum Settings::Quality, bool) = 0;

    virtual Status initialize() = 0;
    virtual Status create()     = 0;
    Status         useImage(Plugin::ImageID imageID, UnityTextureID unityID);
    virtual Status evaluate() = 0;
    virtual Status shutdown() = 0;

    [[nodiscard]] Status getStatus() const;
    std::string&         getErrorMessage();
    Status               setStatus(Status, const std::string&);
    Status               setStatusIf(bool, Status, std::string);
    virtual bool         resetStatus();
    void                 forceStatus(Status, std::string);
};