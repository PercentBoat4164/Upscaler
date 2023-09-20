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
    /// This hardware supports, and Unity was successfully initialized with this upscaler.
    bool supported{true};
    /// This upscaler has been initialized and is trying to be active.
    bool available{};
    /// This upscaler is currently operating on each frame. An upscaler may be available but not active in cases
    /// such as the rendering resolution being too small or too big for the upscaler to work with.
    bool active{};

    template<typename... Args>
    constexpr bool safeFail(Args... /* unused */) {
        return false;
    };

    virtual void setFunctionPointers(GraphicsAPI::Type graphicsAPI) = 0;

public:
    // [31-30] = Error type
    // [29-16] = Error code
    // [15-1] = RESERVED
    // [0] = Recoverable
    enum ErrorReason {
        HARDWARE_ISSUE                                   = 1U << ERROR_TYPE_OFFSET,
        HARDWARE_ISSUE_DEVICE_EXTENSIONS_NOT_SUPPORTED   = HARDWARE_ISSUE | 1U << ERROR_CODE_OFFSET,
        HARDWARE_ISSUE_DEVICE_NOT_SUPPORTED              = HARDWARE_ISSUE | 2U << ERROR_CODE_OFFSET,
        SOFTWARE_ISSUE                                   = 2U << ERROR_TYPE_OFFSET,
        SOFTWARE_ISSUE_INSTANCE_EXTENSIONS_NOT_SUPPORTED = SOFTWARE_ISSUE | 1U << ERROR_CODE_OFFSET,
        SOFTWARE_ISSUE_DEVICE_DRIVERS_OUT_OF_DATE        = SOFTWARE_ISSUE | 2U << ERROR_CODE_OFFSET,
        SOFTWARE_ISSUE_OPERATING_SYSTEM_NOT_SUPPORTED    = SOFTWARE_ISSUE | 3U << ERROR_CODE_OFFSET,
        SOFTWARE_ISSUE_INVALID_WRITE_PERMISSIONS         = SOFTWARE_ISSUE | 4U << ERROR_CODE_OFFSET,
        SOFTWARE_ISSUE_FEATURE_DENIED                    = SOFTWARE_ISSUE | 5U << ERROR_CODE_OFFSET,
        SOFTWARE_ISSUE_OUT_OF_GPU_MEMORY                 = SOFTWARE_ISSUE | 6U << ERROR_CODE_OFFSET,
        SETTINGS_ISSUE                                   = 3U << ERROR_TYPE_OFFSET | ERROR_RECOVERABLE,
        SETTINGS_ISSUE_INPUT_RESOLUTION_TOO_SMALL        = SETTINGS_ISSUE | 1U << ERROR_CODE_OFFSET,
        SETTINGS_ISSUE_INPUT_RESOLUTION_TOO_BIG          = SETTINGS_ISSUE | 2U << ERROR_CODE_OFFSET,
        SETTINGS_ISSUE_UPSCALER_NOT_AVAILABLE            = SETTINGS_ISSUE | 3U << ERROR_CODE_OFFSET,
        SETTINGS_ISSUE_QUALITY_MODE_NOT_AVAILABLE        = SETTINGS_ISSUE | 4U << ERROR_CODE_OFFSET,
    };

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

            uint64_t asLong() const;
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

    static std::vector<Upscaler *> getSupportedUpscalers();

    template<typename T>
        requires std::derived_from<T, Upscaler>
    constexpr static void set() {
        set(T::get());
    }

    static void set(Type upscaler);

    static void set(Upscaler *upscaler);

    static void setGraphicsAPI(GraphicsAPI::Type graphicsAPI);

    static void disableAllUpscalers();

    virtual Type getType() = 0;

    virtual std::string getName() = 0;

    virtual std::vector<std::string> getRequiredVulkanInstanceExtensions() = 0;

    virtual std::vector<std::string> getRequiredVulkanDeviceExtensions(VkInstance, VkPhysicalDevice) = 0;

    virtual Settings getOptimalSettings(Settings::Resolution, bool) = 0;

    virtual bool isSupportedAfter(bool) = 0;

    virtual void setSupported(bool) = 0;

    virtual bool isAvailableAfter(bool) = 0;

    virtual void setAvailable(bool) = 0;

    virtual bool isSupported() = 0;

    virtual bool isAvailable() = 0;

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