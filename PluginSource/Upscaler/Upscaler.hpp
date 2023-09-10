#pragma once

#include "Plugin.hpp"

class Upscaler {
private:
    static Upscaler *upscalerInUse;

protected:
    bool supported{true};
    bool selected{};
    bool available{};
    bool active{};

public:
    enum Type {
        NONE,
        DLSS,
    };

    Upscaler() = default;
    Upscaler(const Upscaler &) = delete;
    Upscaler(Upscaler &&) = default;
    Upscaler &operator =(const Upscaler &) = delete;
    Upscaler &operator =(Upscaler &&) = default;

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

    static void disableAllUpscalers();

    virtual Type getType() = 0;

    virtual std::string getName() = 0;

    virtual ExtensionGroup getRequiredVulkanInstanceExtensions() = 0;

    virtual ExtensionGroup getRequiredVulkanDeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice) = 0;

    virtual void setDepthBuffer(VkImage, VkImageView) = 0;

    virtual void initialize() = 0;

    virtual bool setIsSupported(bool) = 0;

    virtual bool setIsAvailable(bool) = 0;

    virtual bool isSupported() = 0;

    virtual bool isAvailable() = 0;

    virtual ~Upscaler() = default;
};