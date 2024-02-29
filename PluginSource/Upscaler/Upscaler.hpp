#pragma once

#include "GraphicsAPI/GraphicsAPI.hpp"
#include "Plugin.hpp"

#ifdef ENABLE_VULKAN
#    include <vulkan/vulkan.h>
#endif

#ifdef ENABLE_DLSS
#    include <nvsdk_ngx_defs.h>
#endif

#include <IUnityRenderingExtensions.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

#ifndef NDEBUG
#define RETURN_ON_FAILURE(x)                \
    {                                       \
        Upscaler::Status _ = x;             \
        if (Upscaler::failure(_)) return _; \
    }                                       \
    0
#else
#define RETURN_ON_FAILURE(x) x
#endif

class Upscaler {
    constexpr static uint8_t ERROR_TYPE_OFFSET = 29;
    constexpr static uint8_t ERROR_CODE_OFFSET = 16;
    constexpr static uint8_t ERROR_RECOVERABLE = 1;

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
        SUCCESS = 0U,
        NO_UPSCALER_SET = 2U,
        HARDWARE_ERROR                                            = 1U << ERROR_TYPE_OFFSET,
        HARDWARE_ERROR_DEVICE_EXTENSIONS_NOT_SUPPORTED            = HARDWARE_ERROR | 1U << ERROR_CODE_OFFSET,
        HARDWARE_ERROR_DEVICE_NOT_SUPPORTED                       = HARDWARE_ERROR | 2U << ERROR_CODE_OFFSET,
        SOFTWARE_ERROR                                            = 2U << ERROR_TYPE_OFFSET,
        SOFTWARE_ERROR_INSTANCE_EXTENSIONS_NOT_SUPPORTED          = SOFTWARE_ERROR | 1U << ERROR_CODE_OFFSET,
        SOFTWARE_ERROR_DEVICE_DRIVERS_OUT_OF_DATE                 = SOFTWARE_ERROR | 2U << ERROR_CODE_OFFSET,
        SOFTWARE_ERROR_OPERATING_SYSTEM_NOT_SUPPORTED             = SOFTWARE_ERROR | 3U << ERROR_CODE_OFFSET,
        SOFTWARE_ERROR_INVALID_WRITE_PERMISSIONS                  = SOFTWARE_ERROR | 4U << ERROR_CODE_OFFSET,  // Should be marked as recoverable?
        SOFTWARE_ERROR_FEATURE_DENIED                             = SOFTWARE_ERROR | 5U << ERROR_CODE_OFFSET,
        SOFTWARE_ERROR_OUT_OF_GPU_MEMORY                          = SOFTWARE_ERROR | 6U << ERROR_CODE_OFFSET | ERROR_RECOVERABLE,
        SOFTWARE_ERROR_OUT_OF_SYSTEM_MEMORY                       = SOFTWARE_ERROR | 7U << ERROR_CODE_OFFSET | ERROR_RECOVERABLE,
        /// This likely indicates that a segfault has happened or is about to happen. Abort and avoid the crash if at all possible.
        SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR                    = SOFTWARE_ERROR | 8U << ERROR_CODE_OFFSET,
        /// The safest solution to handling this status is to stop using the upscaler. It may still work, but all guarantees are void.
        SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING                  = SOFTWARE_ERROR | 9U << ERROR_CODE_OFFSET,
        /// This is an internal status that may have been caused by the user forgetting to call some function. Typically one or more of the initialization functions.
        SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING               = SOFTWARE_ERROR | 10U << ERROR_CODE_OFFSET | ERROR_RECOVERABLE,
        SETTINGS_ERROR                                            = 3U << ERROR_TYPE_OFFSET | ERROR_RECOVERABLE,
        SETTINGS_ERROR_INVALID_INPUT_RESOLUTION                   = SETTINGS_ERROR | 1U << ERROR_CODE_OFFSET,
        SETTINGS_ERROR_INVALID_OUTPUT_RESOLUTION                  = SETTINGS_ERROR | 2U << ERROR_CODE_OFFSET,
        SETTINGS_ERROR_INVALID_SHARPNESS_VALUE                    = SETTINGS_ERROR | 3U << ERROR_CODE_OFFSET,
        SETTINGS_ERROR_UPSCALER_NOT_AVAILABLE                     = SETTINGS_ERROR | 4U << ERROR_CODE_OFFSET,
        SETTINGS_ERROR_QUALITY_MODE_NOT_AVAILABLE                 = SETTINGS_ERROR | 5U << ERROR_CODE_OFFSET,
        /// A GENERIC_ERROR_* is thrown when a most likely cause has been found but it is not certain. A plain GENERIC_ERROR is thrown when there are many possible known errors.
        GENERIC_ERROR                                             = 4U << ERROR_TYPE_OFFSET,
        GENERIC_ERROR_DEVICE_OR_INSTANCE_EXTENSIONS_NOT_SUPPORTED = GENERIC_ERROR | 1U << ERROR_CODE_OFFSET,
        UNKNOWN_ERROR = 0xFFFFFFFE,
    };

    // clang-format on

    static bool success(Status);
    static bool failure(Status);
    static bool recoverable(Status);
    static bool nonrecoverable(Status);

    enum Type {
        NONE,
#ifdef ENABLE_DLSS
        DLSS,
#endif
    };

    enum SupportState {
        UNTESTED,
        UNSUPPORTED,
        SUPPORTED
    };

    class Settings {
    public:
        enum QualityMode {
            Auto,
            DLAA,
            Quality,
            Balanced,
            Performance,
            UltraPerformance,
            QUALITY_MODE_MAX_ENUM
        };

        struct Resolution {
            uint32_t width;
            uint32_t height;
        };

        struct Jitter {
            float x;
            float y;
        };

    private:
        class Halton {
            constexpr static uint8_t      SamplesPerPixel = 8;
            std::vector<Jitter>           sequence;
            decltype(sequence)::size_type index;

        public:
            void generate(
              Upscaler::Settings::Resolution renderResolution,
              Upscaler::Settings::Resolution presentResolution
            ) {
                float scalingFactor = static_cast<float>(presentResolution.width) / static_cast<float>(renderResolution.width);
                sequence.resize(static_cast<decltype(sequence)::size_type>(std::ceil(SamplesPerPixel * scalingFactor * scalingFactor)));

                for (uint8_t i = 0; i < 2; ++i) {
                    uint32_t base = i + 2;
                    uint32_t n    = 0;
                    uint32_t d    = 1;
                    for (Jitter& element : sequence) {
                        uint32_t x = d - n;
                        if (x == 1) {
                            n = 1;
                            d *= base;
                        } else {
                            uint32_t y = d / base;
                            while (x <= y) y /= base;
                            n = (base + 1) * y - x;
                        }
                        float* subElement = i == 0 ? &element.x : &element.y;
                        *subElement       = static_cast<float>(n) / static_cast<float>(d) - .5f;
                    }
                }
            }

            Jitter getNextJitter() {
                if (sequence.empty()) return {0, 0};
                index %= sequence.size();
                return sequence[index++];
            }
        };

    public:
        QualityMode quality{Auto};
        Resolution  renderingResolution{};
        Resolution  dynamicMaximumInputResolution{};
        Resolution  dynamicMinimumInputResolution{};
        Resolution  outputResolution{};
        Halton      jitterGenerator;
        Jitter      jitter{0.F, 0.F};
        float       sharpness{};
        bool        HDR{};
        float       frameTime{};
        bool        resetHistory{};

        struct Camera {
            float farPlane;
            float nearPlane;
            float verticalFOV;
        } camera;

        virtual Jitter getNextJitter() {
            jitter = jitterGenerator.getNextJitter();
            return jitter;
        }

        template<Type T, typename _ = std::enable_if_t<T == NONE>>
        [[nodiscard]] QualityMode getQuality() const {
            return quality;
        }

#ifdef ENABLE_DLSS
        template<Type T, typename _ = std::enable_if_t<T == DLSS>>
        NVSDK_NGX_PerfQuality_Value getQuality() {
            switch (quality) {
                case Auto: {  // See page 7 of 'RTX UI Developer Guidelines.pdf'
                    const uint64_t pixelCount{
                      static_cast<uint64_t>(outputResolution.width) * outputResolution.height
                    };
                    if (pixelCount <= static_cast<uint64_t>(2560) * 1440)
                        return NVSDK_NGX_PerfQuality_Value_MaxQuality;
                    if (pixelCount <= static_cast<uint64_t>(3840) * 2160)
                        return NVSDK_NGX_PerfQuality_Value_MaxPerf;
                    return NVSDK_NGX_PerfQuality_Value_UltraPerformance;
                }
                case DLAA: return NVSDK_NGX_PerfQuality_Value_DLAA;
                case Quality: return NVSDK_NGX_PerfQuality_Value_MaxQuality;
                case Balanced: return NVSDK_NGX_PerfQuality_Value_Balanced;
                case Performance: return NVSDK_NGX_PerfQuality_Value_MaxPerf;
                case UltraPerformance: return NVSDK_NGX_PerfQuality_Value_UltraPerformance;
                default: return NVSDK_NGX_PerfQuality_Value_Balanced;
            }
        }
#endif
    } settings;

private:
    void*       userData{nullptr};
    Status      status{SUCCESS};
    std::string statusMessage;

protected:
    template<typename T, typename... Args>
    constexpr T safeFail(Args... /*unused*/) {
        return setStatus(UNKNOWN_ERROR, "`safeFail` was called!");
    }

    static void (*logCallback)(const char* msg);

    bool initialized{false};
    std::array<UnityTextureID, Plugin::IMAGE_ID_MAX_ENUM> textureIDs{};

public:
#ifdef ENABLE_VULKAN
    static std::vector<std::string> requestVulkanInstanceExtensions(const std::vector<std::string>&);
    static std::vector<std::string> requestVulkanDeviceExtensions(VkInstance, VkPhysicalDevice, const std::vector<std::string>&);
#endif

    Upscaler()                           = default;
    Upscaler(const Upscaler&)            = delete;
    Upscaler(Upscaler&&)                 = delete;
    Upscaler& operator=(const Upscaler&) = delete;
    Upscaler& operator=(Upscaler&&)      = delete;
    virtual ~Upscaler()                  = default;

    constexpr virtual Type        getType()                                                   = 0;
    constexpr virtual std::string getName()                                                   = 0;
    virtual bool        isSupported()                                                         = 0;
    virtual Status      getOptimalSettings(Settings::Resolution, Settings::QualityMode, bool) = 0;

    virtual Status initialize() = 0;
    virtual Status create()     = 0;
    Status useImage(Plugin::ImageID imageID, UnityTextureID unityID);
    virtual Status evaluate() = 0;
    virtual Status shutdown() = 0;

    /// Returns the current status.
    [[nodiscard]] Status getStatus() const;
    std::string&         getErrorMessage();
    /// Sets current status to t_error if there is no current status. Use resetStatus to clear the current status.
    /// Returns the current status.
    Status               setStatus(Status, const std::string&);
    /// Sets current status to t_error if t_shouldApplyError == true AND there is no current status. Use
    /// resetStatus to clear the current status. Returns the current status
    Status               setStatusIf(bool, Status, std::string);
    /// Returns false and does not modify the status if the current status is non-recoverable. Returns true if the
    /// status has been cleared.
    bool                 resetStatus();

    std::unique_ptr<Upscaler>        copyFromType(Type type);
    static std::unique_ptr<Upscaler> fromType(Type type);

    static void setLogCallback(void (*pFunction)(const char*));
};