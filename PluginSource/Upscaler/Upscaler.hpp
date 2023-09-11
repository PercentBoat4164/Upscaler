#pragma once

// Project
#include "GraphicsAPI/GraphicsAPI.hpp"

/** @todo Remove this after removing all ties to Vulkan from this file. */
#include <vulkan/vulkan.h>

// Upscaler
#include <nvsdk_ngx_defs.h>

// System
#include <cstdint>
#include <string>
#include <vector>

class Upscaler {
private:
    static Upscaler *upscalerInUse;

protected:
    /// This hardware supports, and Unity was successfully initialized with this upscaler.
    bool supported{true};
    /// This upscaler has been initialized and is trying to be active.
    bool available{};
    /// This upscaler is currently operating on each frame. An upscaler may be available but not active in cases
    /// such as the rendering resolution being too small or too big for the upscaler to work with.
    bool active{};

    template<typename... Args>
    bool SafeFail(Args...) {
        return false;
    };

    virtual void setFunctionPointers(GraphicsAPI::Type graphicsAPI) = 0;

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

            uint64_t asLong() const;
        };

        Quality    quality;
        Resolution renderResolution{};
        Resolution dynamicMaximumRenderResolution{};
        Resolution dynamicMinimumRenderResolution{};
        Resolution presentResolution{};
        float      sharpness{};
        bool       lowResolutionMotionVectors{};
        bool       jitteredMotionVectors{};
        bool       HDR{};
        bool       invertedDepth{};
        bool       autoExposure{};

        template<Type T, typename _ = std::enable_if_t<T == Upscaler::NONE>>
        Quality getQuality() {
            return quality;
        };

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
    } settings;

    Upscaler()                            = default;
    Upscaler(const Upscaler &)            = delete;
    Upscaler(Upscaler &&)                 = default;
    Upscaler &operator=(const Upscaler &) = delete;
    Upscaler &operator=(Upscaler &&)      = default;

    template<typename T>
        requires std::derived_from<T, Upscaler>
    static Upscaler *get();

    static Upscaler *get(Type upscaler);

    static Upscaler *get();

    static std::vector<Upscaler *> getAllUpscalers();

    static std::vector<Upscaler *> getSupportedUpscalers();

    template<typename T>
        requires std::derived_from<T, Upscaler>
    static void set();

    static void set(Type upscaler);

    static void set(Upscaler *upscaler);

    static void setGraphicsAPI(GraphicsAPI::Type graphicsAPI);

    static void disableAllUpscalers();

    virtual Type getType() = 0;

    virtual std::string getName() = 0;

    virtual std::vector<std::string> getRequiredVulkanInstanceExtensions() = 0;

    virtual std::vector<std::string> getRequiredVulkanDeviceExtensions(VkInstance, VkPhysicalDevice) = 0;

    virtual Settings getOptimalSettings(Settings::Resolution) = 0;

    virtual void setDepthBuffer(VkImage, VkImageView) = 0;

    virtual bool isSupportedAfter(bool) = 0;

    virtual void setSupported(bool) = 0;

    virtual bool isAvailableAfter(bool) = 0;

    virtual void setAvailable(bool) = 0;

    virtual bool isSupported() = 0;

    virtual bool isAvailable() = 0;

    virtual bool initialize() = 0;

    virtual bool createFeature() = 0;

    virtual bool evaluate() = 0;

    virtual bool releaseFeature() = 0;

    virtual bool shutdown() = 0;

    virtual ~Upscaler() = default;
};