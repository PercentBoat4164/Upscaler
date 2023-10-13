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
    constexpr static uint8_t ERROR_TYPE_OFFSET = 29;
    constexpr static uint8_t ERROR_CODE_OFFSET = 16;
    constexpr static uint8_t ERROR_RECOVERABLE = 1;

public:
    // clang-format off

    /**@todo Add helper functions for me. */
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
        /// The safest solution to handling this error is to stop using the upscaler. It may still work, but all guarantees are void.
        SOFTWARE_ERROR_CRITICAL_INTERNAL_WARNING                  = SOFTWARE_ERROR | 8U << ERROR_CODE_OFFSET,
        /// This is an internal error that may have been caused by the user forgetting to call some function. Typically one or more of the initialization functions.
        SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING               = SOFTWARE_ERROR | 9U << ERROR_CODE_OFFSET | ERROR_RECOVERABLE,
        SETTINGS_ERROR                                            = 3U << ERROR_TYPE_OFFSET | ERROR_RECOVERABLE,
        SETTINGS_ERROR_INVALID_INPUT_RESOLUTION                   = SETTINGS_ERROR | 1U << ERROR_CODE_OFFSET,
        SETTINGS_ERROR_INVALID_SHARPNESS_VALUE                    = SETTINGS_ERROR | 2U << ERROR_CODE_OFFSET,
        SETTINGS_ERROR_UPSCALER_NOT_AVAILABLE                     = SETTINGS_ERROR | 3U << ERROR_CODE_OFFSET,
        SETTINGS_ERROR_QUALITY_MODE_NOT_AVAILABLE                 = SETTINGS_ERROR | 4U << ERROR_CODE_OFFSET,
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

protected:
    template<typename... Args>
    constexpr Status safeFail(Args... /* unused */) {
        return SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR;
    };

    virtual void setFunctionPointers(GraphicsAPI::Type graphicsAPI) = 0;

    bool initialized{false};

private:
    static void(*errorCallback)(void *, Upscaler::Status, const char *);
    static void *userData;
    static Upscaler *upscalerInUse;
    Status           error{SUCCESS};
    std::string      detailedErrorMessage{};

public:
    enum Type {
        NONE,
        DLSS,
    };

    static struct Settings {
        enum Quality {
            AUTO,  // Chooses a performance quality mode based on output resolution
            ULTRA_QUALITY,
            QUALITY,
            BALANCED,
            PERFORMANCE,
            ULTRA_PERFORMANCE,
            DYNAMIC_AUTO,    // Enables dynamic resolution and automatically handles changing scale factors
            DYNAMIC_MANUAL,  // Enables dynamic resolution and lets the user handle changing scale factors
        };

        struct Resolution {
            unsigned int width;
            unsigned int height;

            [[nodiscard]] uint64_t asLong() const;
        };

        Quality    quality{AUTO};
        Resolution recommendedInputResolution{};
        Resolution dynamicMaximumInputResolution{};
        Resolution dynamicMinimumInputResolution{};
        Resolution outputResolution{};
        Resolution currentInputResolution{};
        float      jitter[2] = {0.F, 0.F};
        float      sharpness{};
        bool       HDR{};
        bool       autoExposure{};
        bool       resetHistory{};

        template<Type T, typename _ = std::enable_if_t<T == Upscaler::NONE>>
        Quality getQuality() {
            return quality;
        };

#ifdef ENABLE_DLSS
        template<Type T, typename _ = std::enable_if_t<T == Upscaler::DLSS>>
        NVSDK_NGX_PerfQuality_Value getQuality() {
            switch (quality) {
                case AUTO: {  // See page 7 of 'RTX UI Developer Guidelines .pdf'
                    uint64_t pixelCount{
                      (uint64_t) recommendedInputResolution.width * recommendedInputResolution.height};
                    if (pixelCount <= (uint64_t) 2560 * 1440) return NVSDK_NGX_PerfQuality_Value_MaxQuality;
                    if (pixelCount <= (uint64_t) 3840 * 2160) return NVSDK_NGX_PerfQuality_Value_MaxPerf;
                    return NVSDK_NGX_PerfQuality_Value_UltraPerformance;
                }
                case ULTRA_QUALITY: return NVSDK_NGX_PerfQuality_Value_UltraQuality;
                case QUALITY: return NVSDK_NGX_PerfQuality_Value_MaxQuality;
                case BALANCED: return NVSDK_NGX_PerfQuality_Value_Balanced;
                case PERFORMANCE: return NVSDK_NGX_PerfQuality_Value_MaxPerf;
                case ULTRA_PERFORMANCE: return NVSDK_NGX_PerfQuality_Value_UltraPerformance;
                case DYNAMIC_AUTO:
                case DYNAMIC_MANUAL: return NVSDK_NGX_PerfQuality_Value_MaxQuality;
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

    static auto setErrorCallback(void *data, void(*t_errorCallback)(void *, Upscaler::Status, const char *)) -> void(*)(void *, Upscaler::Status, const char *);

    /// Returns the current error.
    Status getError();

    /// Sets current error to t_error if there is no current error. Use resetError to clear the current error.
    /// Returns the current error.
    Status setError(Status, std::string);

    /// Sets current error to t_error if t_shouldApplyError == true AND there is no current error. Use resetError
    /// to clear the current error. Returns the current error
    Status setErrorIf(bool, Status, std::string);

    /// Returns false and does not modify the error if the current error is non-recoverable. Returns true if the
    /// error has been cleared.
    bool resetError();

    std::string &getErrorMessage();

    virtual Type getType() = 0;

    virtual std::string getName() = 0;

    virtual std::vector<std::string> getRequiredVulkanInstanceExtensions() = 0;

    virtual std::vector<std::string> getRequiredVulkanDeviceExtensions(VkInstance, VkPhysicalDevice) = 0;

    virtual Settings getOptimalSettings(Settings::Resolution, Settings::Quality, bool) = 0;

    virtual Status initialize() = 0;

    virtual Status createFeature() = 0;

    virtual Status setImageResources(
      void                          *nativeDepthBuffer,
      UnityRenderingExtTextureFormat unityDepthFormat,
      void                          *nativeMotionVectors,
      UnityRenderingExtTextureFormat unityMotionVectorFormat,
      void                          *nativeInColor,
      UnityRenderingExtTextureFormat unityInColorFormat,
      void                          *nativeOutColor,
      UnityRenderingExtTextureFormat unityOutColorFormat
    ) = 0;

    virtual Status evaluate() = 0;

    virtual Status releaseFeature() = 0;

    virtual Status shutdown();

    virtual ~Upscaler() = default;
};