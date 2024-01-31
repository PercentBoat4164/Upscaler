#pragma once

// Project
#include "GraphicsAPI/GraphicsAPI.hpp"

// Graphics API
#include <vulkan/vulkan.h>

// Upscaler
#ifdef ENABLE_DLSS
#    include <nvsdk_ngx_defs.h>
#endif

// Unity
#include <IUnityRenderingExtensions.h>

// System
#include <array>
#include <cstdint>
#include <string>
#include <vector>

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
        /// This likely indicates that a segfault has happened or is about to happen. Abort and avoid the crash if at all possible.
        SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR                    = SOFTWARE_ERROR | 7U << ERROR_CODE_OFFSET,
        /// The safest solution to handling this status is to stop using the upscaler. It may still work, but all guarantees are void.
        SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING                  = SOFTWARE_ERROR | 8U << ERROR_CODE_OFFSET,
        /// This is an internal status that may have been caused by the user forgetting to call some function. Typically one or more of the initialization functions.
        SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING               = SOFTWARE_ERROR | 9U << ERROR_CODE_OFFSET | ERROR_RECOVERABLE,
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
        DLSS,
    };

    static struct Settings {
        enum QualityMode {
            Auto,  // Chooses a performance quality mode based on output resolution
//            UltraQuality,
            Quality,
            Balanced,
            Performance,
            UltraPerformance,
        };

        struct Resolution {
            uint32_t width;
            uint32_t height;

            [[nodiscard]] uint64_t asLong() const;
        };

        QualityMode          quality{Auto};
        Resolution           recommendedInputResolution{};
        Resolution           dynamicMaximumInputResolution{};
        Resolution           dynamicMinimumInputResolution{};
        Resolution           outputResolution{};
        std::array<float, 2> jitter{0.F, 0.F};
        float                sharpness{};
        bool                 HDR{};
        bool                 autoExposure{};
        bool                 resetHistory{};

        template<Type T, typename _ = std::enable_if_t<T == NONE>>
        [[nodiscard]] QualityMode getQuality() const {
            return quality;
        }

#ifdef ENABLE_DLSS
        template<Type T, typename _ = std::enable_if_t<T == DLSS>>
        NVSDK_NGX_PerfQuality_Value getQuality() {
            switch (quality) {
                case Auto: {  // See page 7 of 'RTX UI Developer Guidelines .pdf'
                    const uint64_t pixelCount{
                      static_cast<uint64_t>(outputResolution.width) * outputResolution.height
                    };
                    if (pixelCount <= static_cast<uint64_t>(2560) * 1440) return NVSDK_NGX_PerfQuality_Value_MaxQuality;
                    if (pixelCount <= static_cast<uint64_t>(3840) * 2160) return NVSDK_NGX_PerfQuality_Value_MaxPerf;
                    return NVSDK_NGX_PerfQuality_Value_UltraPerformance;
                }
//                case UltraQuality: return NVSDK_NGX_PerfQuality_Value_UltraQuality;
                case Quality: return NVSDK_NGX_PerfQuality_Value_MaxQuality;
                case Balanced: return NVSDK_NGX_PerfQuality_Value_Balanced;
                case Performance: return NVSDK_NGX_PerfQuality_Value_MaxPerf;
                case UltraPerformance: return NVSDK_NGX_PerfQuality_Value_UltraPerformance;
            }
            return NVSDK_NGX_PerfQuality_Value_Balanced;
        }
#endif
    } settings;

private:
    static void      (*errorCallback)(void *, Status, const char *);
    static void     *userData;
    static Upscaler *upscalerInUse;
    Status           status{SUCCESS};
    std::string      detailedErrorMessage{};

protected:
    template<typename T, typename... Args>
    constexpr T safeFail(Args... /* unused */) {
        return UNKNOWN_ERROR;
    }

    virtual void setFunctionPointers(GraphicsAPI::Type graphicsAPI) = 0;

    bool initialized{false};

public:
    Upscaler()                 = default;
    Upscaler(const Upscaler &) = delete;
    Upscaler(Upscaler &&)      = default;

    Upscaler &operator=(const Upscaler &) = delete;
    Upscaler &operator=(Upscaler &&)      = default;

    template<typename T>
        requires std::derived_from<T, Upscaler>
    constexpr static void set() {
        set(T::get());
    }

    static void set(Type upscaler);
    static void set(Upscaler *upscaler);
    static void setGraphicsAPI(GraphicsAPI::Type graphicsAPI);

    template<typename T>
        requires std::derived_from<T, Upscaler>
    constexpr static T *get() {
        return T::get();
    }

    static std::vector<Upscaler *> getAllUpscalers();
    static std::vector<Upscaler *> getUpscalersWithoutErrors();
    static Upscaler               *get(Type upscaler);
    static Upscaler               *get();

    virtual Type        getType() = 0;
    virtual std::string getName() = 0;

    virtual std::vector<std::string> getRequiredVulkanInstanceExtensions()                           = 0;
    virtual std::vector<std::string> getRequiredVulkanDeviceExtensions(VkInstance, VkPhysicalDevice) = 0;

    virtual Settings getOptimalSettings(Settings::Resolution, Settings::QualityMode, bool) = 0;

    virtual Status initialize()                                                                     = 0;
    virtual Status createFeature()                                                                  = 0;
    virtual Status setDepthBuffer(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat)   = 0;
    virtual Status setInputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat)    = 0;
    virtual Status setMotionVectors(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) = 0;
    virtual Status setOutputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat)   = 0;
    virtual void   updateImages()                                                                   = 0;
    virtual Status evaluate()                                                                       = 0;
    virtual Status releaseFeature()                                                                 = 0;
    virtual Status shutdown();

    /// Returns the current status.
    [[nodiscard]] Status getStatus() const;
    /// Sets current status to t_error if there is no current status. Use resetStatus to clear the current status.
    /// Returns the current status.
    Status               setStatus(Status, const std::string&);
    /// Sets current status to t_error if t_shouldApplyError == true AND there is no current status. Use
    /// resetStatus to clear the current status. Returns the current status
    Status               setStatusIf(bool, Status, std::string);
    /// Returns false and does not modify the status if the current status is non-recoverable. Returns true if the
    /// status has been cleared.
    bool                 resetStatus();
    std::string         &getErrorMessage();
    static void (*setErrorCallback(void *data, void (*t_errorCallback)(void *, Status, const char *)))(void *, Status, const char *);

    virtual ~Upscaler() = default;
};