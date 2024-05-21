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

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#    define RETURN_ON_FAILURE(x)        \
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
        TYPE_MAX_ENUM
    };

    struct alignas(128) Settings final {
        enum Quality : uint8_t {
            Auto,
            AntiAliasing,
            Quality,
            Balanced,
            Performance,
            UltraPerformance,
            QUALITY_MODE_MAX_ENUM
        } quality{};

        enum Preset : uint8_t {
            Default,
            Stable,
            FastPaced,
            AnitGhosting,
            PRESET_MAX_ENUM
        } preset{};

        struct alignas(8) Resolution {
            uint32_t width;
            uint32_t height;
        } renderingResolution{}, dynamicMaximumInputResolution{}, dynamicMinimumInputResolution{}, outputResolution{};

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

        float tcThreshold;
        float tcScale;
        float reactiveScale;
        float reactiveMax;
        float sharpness{};
        float frameTime{};
        bool  autoReactive;
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

            const float scalingFactor = static_cast<float>(outputResolution.width) / static_cast<float>(renderingResolution.width);
            const auto  jitterSamples = static_cast<uint32_t>(std::ceil(static_cast<float>(SamplesPerPixel) * scalingFactor * scalingFactor));
            jitter                    = {x.advance(jitterSamples), y.advance(jitterSamples)};
            return jitter;
        }

        template<Type, typename>
        [[nodiscard]] constexpr enum Quality getQuality() {
            return static_cast<enum Quality>(-1);
        }

#ifdef ENABLE_DLSS
        template<Type T, typename = std::enable_if_t<T == DLSS>>
        [[nodiscard]] NVSDK_NGX_PerfQuality_Value getQuality() const {
            switch (quality) {
                case Auto: {  // See page 7 of 'RTX UI Developer Guidelines.pdf'
                    const uint32_t pixelCount {outputResolution.width * outputResolution.height};
                    if (pixelCount <= 2560U * 1440U)
                        return NVSDK_NGX_PerfQuality_Value_MaxQuality;
                    if (pixelCount <= 3840U * 2160U)
                        return NVSDK_NGX_PerfQuality_Value_MaxPerf;
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
                    if (pixelCount <= 2560U * 1440U)
                        return FFX_FSR2_QUALITY_MODE_QUALITY;
                    if (pixelCount <= 3840U * 2160U)
                        return FFX_FSR2_QUALITY_MODE_PERFORMANCE;
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
    } settings;

protected:
    bool                                                  initialized{};
    std::array<UnityTextureID, Plugin::IMAGE_ID_MAX_ENUM> textureIDs{};
};

class Upscaler : public UpscalerBase {
    constexpr static uint8_t ERROR_TYPE_OFFSET = 29U;
    constexpr static uint8_t ERROR_CODE_OFFSET = 16U;
    constexpr static uint8_t ERROR_RECOVERABLE = 1U;

public:
    // clang-format off

    // bit range = meaning
    // =======================
    // [31-29]   = Error type
    // [28-16]   = Error code
    // [15-2]    = RESERVED
    // [1]       = Attempting to use a Dummy Upscaler
    // [0]       = Recoverable
    enum Status : uint32_t {
        SUCCESS                                                   =                  0U,
        NO_UPSCALER_SET                                           =                  2U,
        HARDWARE_ERROR                                            =                  1U  << ERROR_TYPE_OFFSET,
        HARDWARE_ERROR_DEVICE_EXTENSIONS_NOT_SUPPORTED            = HARDWARE_ERROR | 1U  << ERROR_CODE_OFFSET,
        HARDWARE_ERROR_DEVICE_NOT_SUPPORTED                       = HARDWARE_ERROR | 2U  << ERROR_CODE_OFFSET,
        SOFTWARE_ERROR                                            =                  2U  << ERROR_TYPE_OFFSET,
        SOFTWARE_ERROR_INSTANCE_EXTENSIONS_NOT_SUPPORTED          = SOFTWARE_ERROR | 1U  << ERROR_CODE_OFFSET,
        SOFTWARE_ERROR_DEVICE_DRIVERS_OUT_OF_DATE                 = SOFTWARE_ERROR | 2U  << ERROR_CODE_OFFSET,
        SOFTWARE_ERROR_OPERATING_SYSTEM_NOT_SUPPORTED             = SOFTWARE_ERROR | 3U  << ERROR_CODE_OFFSET,
        SOFTWARE_ERROR_INVALID_WRITE_PERMISSIONS                  = SOFTWARE_ERROR | 4U  << ERROR_CODE_OFFSET,  // Should be marked as recoverable?
        SOFTWARE_ERROR_FEATURE_DENIED                             = SOFTWARE_ERROR | 5U  << ERROR_CODE_OFFSET,
        SOFTWARE_ERROR_OUT_OF_GPU_MEMORY                          = SOFTWARE_ERROR | 6U  << ERROR_CODE_OFFSET | ERROR_RECOVERABLE,
        SOFTWARE_ERROR_OUT_OF_SYSTEM_MEMORY                       = SOFTWARE_ERROR | 7U  << ERROR_CODE_OFFSET | ERROR_RECOVERABLE,
        SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR                    = SOFTWARE_ERROR | 8U  << ERROR_CODE_OFFSET,
        SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING               = SOFTWARE_ERROR | 9U  << ERROR_CODE_OFFSET | ERROR_RECOVERABLE,
        SOFTWARE_ERROR_UNSUPPORTED_GRAPHICS_API                   = SOFTWARE_ERROR | 10U << ERROR_CODE_OFFSET,
        SETTINGS_ERROR                                            =                  3U  << ERROR_TYPE_OFFSET | ERROR_RECOVERABLE,
        SETTINGS_ERROR_INVALID_INPUT_RESOLUTION                   = SETTINGS_ERROR | 1U  << ERROR_CODE_OFFSET,
        SETTINGS_ERROR_INVALID_OUTPUT_RESOLUTION                  = SETTINGS_ERROR | 2U  << ERROR_CODE_OFFSET,
        SETTINGS_ERROR_INVALID_SHARPNESS_VALUE                    = SETTINGS_ERROR | 3U  << ERROR_CODE_OFFSET,
        SETTINGS_ERROR_QUALITY_MODE_NOT_AVAILABLE                 = SETTINGS_ERROR | 4U  << ERROR_CODE_OFFSET,
        SETTINGS_ERROR_PRESET_NOT_AVAILABLE                       = SETTINGS_ERROR | 5U  << ERROR_CODE_OFFSET,
        SETTINGS_ERROR_UPSCALER_NOT_AVAILABLE                     = SETTINGS_ERROR | 6U  << ERROR_CODE_OFFSET,
        GENERIC_ERROR                                             =                  4U  << ERROR_TYPE_OFFSET,
        GENERIC_ERROR_DEVICE_OR_INSTANCE_EXTENSIONS_NOT_SUPPORTED = GENERIC_ERROR  | 1U  << ERROR_CODE_OFFSET,
        UNKNOWN_ERROR                                             =                  0xFFFFFFFE,
    };

    // clang-format on

    static bool success(Status);
    static bool failure(Status);
    static bool recoverable(Status);

    enum SupportState {
        UNTESTED,
        UNSUPPORTED,
        SUPPORTED
    };

private:
    Status      status{SUCCESS};
    std::string statusMessage;

protected:
    template<typename... Args>
    constexpr Status safeFail(Args... /*unused*/) {
        return setStatus(UNKNOWN_ERROR, "`safeFail` was called!");
    }

    template<typename... Args>
    constexpr Status invalidGraphicsAPIFail(Args... /*unused*/) {
        return setStatus(SOFTWARE_ERROR_UNSUPPORTED_GRAPHICS_API, getName() + " does not support the current graphics API.");
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
    virtual Status                getOptimalSettings(Settings::Resolution, Settings::Preset, enum Settings::Quality, bool) = 0;

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