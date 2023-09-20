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
#include <cstdint>
#include <string>
#include <vector>

class Upscaler {
private:
    static Upscaler         *upscalerInUse;
    constexpr static uint8_t ERROR_TYPE_OFFSET = 30;
    constexpr static uint8_t ERROR_CODE_OFFSET = 16;
    constexpr static uint8_t ERROR_RECOVERABLE = 1;

protected:
    template<typename... Args>
    constexpr bool safeFail(Args... /* unused */) {
        return false;
    };

    virtual void setFunctionPointers(GraphicsAPI::Type graphicsAPI) = 0;

public:
    // clang-format off

    // bit range = meaning
    // =======================
    // [31-30]   = Error type
    // [29-16]   = Error code
    // [15-1]    = RESERVED
    // [0]       = Recoverable
    enum ErrorReason {
        NO_ERROR = 0,
        HARDWARE_ERROR                                            = 0U << ERROR_TYPE_OFFSET,
        HARDWARE_ERROR_DEVICE_EXTENSIONS_NOT_SUPPORTED            = HARDWARE_ERROR | 1U << ERROR_CODE_OFFSET,
        HARDWARE_ERROR_DEVICE_NOT_SUPPORTED                       = HARDWARE_ERROR | 2U << ERROR_CODE_OFFSET,
        SOFTWARE_ERROR                                            = 1U << ERROR_TYPE_OFFSET,
        SOFTWARE_ERROR_INSTANCE_EXTENSIONS_NOT_SUPPORTED          = SOFTWARE_ERROR | 1U << ERROR_CODE_OFFSET,
        SOFTWARE_ERROR_DEVICE_DRIVERS_OUT_OF_DATE                 = SOFTWARE_ERROR | 2U << ERROR_CODE_OFFSET,
        SOFTWARE_ERROR_OPERATING_SYSTEM_NOT_SUPPORTED             = SOFTWARE_ERROR | 3U << ERROR_CODE_OFFSET,
        SOFTWARE_ERROR_INVALID_WRITE_PERMISSIONS                  = SOFTWARE_ERROR | 4U << ERROR_CODE_OFFSET,  // Should be marked as recoverable?
        SOFTWARE_ERROR_FEATURE_DENIED                             = SOFTWARE_ERROR | 5U << ERROR_CODE_OFFSET,
        SOFTWARE_ERROR_OUT_OF_GPU_MEMORY                          = SOFTWARE_ERROR | 6U << ERROR_CODE_OFFSET | ERROR_RECOVERABLE,
        /// This likely indicates that a segfault has happened or is about to happen. Abort and avoid the crash if at all possible.
        SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR                    = SOFTWARE_ERROR | 7U << ERROR_CODE_OFFSET,
        /// The safest solution to handling this error is to stop using the upscaler. It may still work, but all guarantees are void.
        SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING                  = SOFTWARE_ERROR | 8U << ERROR_CODE_OFFSET,
        /// This is an internal error that may have been caused by the user forgetting to call some function. Typically the initialization functions.
        SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING               = SOFTWARE_ERROR | 9U << ERROR_CODE_OFFSET | ERROR_RECOVERABLE,
        SETTINGS_ERROR                                            = 2U << ERROR_TYPE_OFFSET | ERROR_RECOVERABLE,
        SETTINGS_ERROR_INPUT_RESOLUTION_TOO_SMALL                 = SETTINGS_ERROR | 1U << ERROR_CODE_OFFSET,
        SETTINGS_ERROR_INPUT_RESOLUTION_TOO_BIG                   = SETTINGS_ERROR | 2U << ERROR_CODE_OFFSET,
        SETTINGS_ERROR_UPSCALER_NOT_AVAILABLE                     = SETTINGS_ERROR | 3U << ERROR_CODE_OFFSET,
        SETTINGS_ERROR_QUALITY_MODE_NOT_AVAILABLE                 = SETTINGS_ERROR | 4U << ERROR_CODE_OFFSET,
        /// A GENERIC_ERROR_* is thrown when a most likely cause has been found but it is not certain. A plain GENERIC_ERROR is thrown when there are many possible known errors.
        GENERIC_ERROR                                             = 3U << ERROR_TYPE_OFFSET,
        GENERIC_ERROR_DEVICE_OR_INSTANCE_EXTENSIONS_NOT_SUPPORTED = GENERIC_ERROR | 1U << ERROR_CODE_OFFSET,
        UNKNOWN_ERROR = 0xFFFFFFFF,
    };
    // clang-format on

private:
    ErrorReason error{NO_ERROR};
    std::string detailedErrorMessage{};

public:
    enum Type {
        NONE,
        DLSS,
    };

    static struct Settings {
        enum Quality {
            AUTO,
            ULTRA_QUALITY,
            QUALITY,
            BALANCED,
            PERFORMANCE,
            ULTRA_PERFORMANCE,
        };

        struct Resolution {
            unsigned int width;
            unsigned int height;

            [[nodiscard]] uint64_t asLong() const;
        };

        Quality    quality{QUALITY};
        Resolution inputResolution{};
        Resolution dynamicMaximumInputResolution{};
        Resolution dynamicMinimumInputResolution{};
        Resolution outputResolution{};
        float      jitter[2] = {0.F, 0.F};
        float      sharpness{};
        bool       HDR{};
        bool       autoExposure{};

        template<Type T, typename _ = std::enable_if_t<T == Upscaler::NONE>>
        Quality getQuality() {
            return quality;
        };

#ifdef ENABLE_DLSS
        template<Type T, typename _ = std::enable_if_t<T == Upscaler::DLSS>>
        NVSDK_NGX_PerfQuality_Value getQuality() {
            switch (quality) {
                case AUTO: return NVSDK_NGX_PerfQuality_Value_Balanced;
                case ULTRA_QUALITY: return NVSDK_NGX_PerfQuality_Value_UltraQuality;
                case QUALITY: return NVSDK_NGX_PerfQuality_Value_MaxQuality;
                case BALANCED: return NVSDK_NGX_PerfQuality_Value_Balanced;
                case PERFORMANCE: return NVSDK_NGX_PerfQuality_Value_MaxPerf;
                case ULTRA_PERFORMANCE: return NVSDK_NGX_PerfQuality_Value_UltraPerformance;
            }
            return NVSDK_NGX_PerfQuality_Value_Balanced;
        };
#endif
    } settings;

    Upscaler()                            = default;
    Upscaler(const Upscaler &)            = delete;
    Upscaler(Upscaler &&)                 = default;
    Upscaler &operator=(const Upscaler &) = delete;
    Upscaler &operator=(Upscaler &&)      = default;

    template<typename T>
        requires std::derived_from<T, Upscaler>
    constexpr static T *get() {
        return T::get();
    }

    static Upscaler *get(Type upscaler);

    static Upscaler *get();

    static std::vector<Upscaler *> getAllUpscalers();

    static std::vector<Upscaler *> getUpscalersWithoutErrors();

    template<typename T>
        requires std::derived_from<T, Upscaler>
    constexpr static void set() {
        set(T::get());
    }

    static void set(Type upscaler);

    static void set(Upscaler *upscaler);

    static void setGraphicsAPI(GraphicsAPI::Type graphicsAPI);

    /// Returns the current error.
    ErrorReason getError();

    /// Sets current error to t_error if there is no current error. Use resetError to clear the current error.
    bool setError(ErrorReason);

    /// Sets current error to t_error if t_shouldApplyError == true AND there is no current error. Use resetError to clear the current error.
    bool setErrorIf(bool, ErrorReason);

    /// Returns false and does not modify the error if the current error is non-recoverable. Returns true if the error has been cleared.
    bool resetError();

    bool setErrorMessage(std::string);

    std::string &getErrorMessage();

    virtual Type getType() = 0;

    virtual std::string getName() = 0;

    virtual std::vector<std::string> getRequiredVulkanInstanceExtensions() = 0;

    virtual std::vector<std::string> getRequiredVulkanDeviceExtensions(VkInstance, VkPhysicalDevice) = 0;

    virtual Settings getOptimalSettings(Settings::Resolution, bool) = 0;

    virtual bool initialize() = 0;

    virtual bool createFeature() = 0;

    virtual bool setImageResources(
      void                          *nativeDepthBuffer,
      UnityRenderingExtTextureFormat unityDepthFormat,
      void                          *nativeMotionVectors,
      UnityRenderingExtTextureFormat unityMotionVectorFormat,
      void                          *nativeInColor,
      UnityRenderingExtTextureFormat unityInColorFormat,
      void                          *nativeOutColor,
      UnityRenderingExtTextureFormat unityOutColorFormat
    ) = 0;

    virtual bool evaluate() = 0;

    virtual bool releaseFeature() = 0;

    virtual bool shutdown() = 0;

    virtual ~Upscaler() = default;
};