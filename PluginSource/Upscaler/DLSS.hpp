#pragma once

#include "GraphicsAPI/Vulkan.hpp"
#include "Logger.hpp"
#include "Plugin.hpp"
#include "Upscaler.hpp"

#include <nvsdk_ngx_helpers_vk.h>

class DLSS : public Upscaler {
    struct Application {
    uint64_t                         id{231313132};
    std::wstring                     dataPath{L"./"};
    NVSDK_NGX_Application_Identifier ngxIdentifier {
      .IdentifierType = NVSDK_NGX_Application_Identifier_Type_Application_Id,
      .v              = {
        .ApplicationId = id,
      }
    };
    NVSDK_NGX_FeatureCommonInfo featureCommonInfo {
      .PathListInfo = {
        .Path   = new const wchar_t *{L"./Assets/Plugins"},
        .Length = 1,
      },
      .InternalData = nullptr,
      .LoggingInfo  = {
        .LoggingCallback          = Logger::log,
//        .LoggingCallback = nullptr,
        .MinimumLoggingLevel      = NVSDK_NGX_LOGGING_LEVEL_VERBOSE,
        .DisableOtherLoggingSinks = false,
      }
    };
    NVSDK_NGX_FeatureDiscoveryInfo featureDiscoveryInfo {
      .SDKVersion          = NVSDK_NGX_Version_API,
      .FeatureID           = NVSDK_NGX_Feature_SuperSampling,
      .Identifier          = ngxIdentifier,
      .ApplicationDataPath = dataPath.c_str(),
      .FeatureInfo         = &featureCommonInfo,
    };
    } applicationInfo;

    DLSS() = default;

public:
    static Upscaler *get();

    bool isSupported() override;

    bool isAvailable() override;

    /// Note: Can only set the value from true to false.
    bool setIsSupported(bool isSupported) override;

    /// Note: Can only set the value from true to false.
    bool setIsAvailable(bool isAvailable) override;

    ExtensionGroup getRequiredVulkanInstanceExtensions() override;

    ExtensionGroup getRequiredVulkanDeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice) override;

    void initialize() override;

    void setDepthBuffer(VkImage depthBuffer, VkImageView depthBufferView) override;

    Type getType() override;

    std::string getName() override;
};