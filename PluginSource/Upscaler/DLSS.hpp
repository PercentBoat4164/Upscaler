#pragma once
#ifdef ENABLE_DLSS
#    include "Plugin.hpp"
#    include "Upscaler.hpp"

#    include <nvsdk_ngx_helpers.h>
#    ifdef ENABLE_VULKAN
#        include <nvsdk_ngx_helpers_vk.h>
#    endif

class DLSS final : public Upscaler {
    static struct Application {
        // clang-format off
        NVSDK_NGX_Application_Identifier ngxIdentifier {
          .IdentifierType = NVSDK_NGX_Application_Identifier_Type_Application_Id,
          .v              = {
            .ApplicationId = 0xDC98EEC,
          }
        };
        NVSDK_NGX_FeatureCommonInfo featureCommonInfo{
          .PathListInfo {
            .Path   = new const wchar_t *{L"./Assets/Plugins"},
            .Length = 1,
          },
          .InternalData = nullptr,
          .LoggingInfo {
#   ifndef NDEBUG
          .LoggingCallback = &DLSS::log,
          .MinimumLoggingLevel      = NVSDK_NGX_LOGGING_LEVEL_VERBOSE,
          .DisableOtherLoggingSinks = false,
#   else
          .LoggingCallback = nullptr,
          .MinimumLoggingLevel      = NVSDK_NGX_LOGGING_LEVEL_OFF,
          .DisableOtherLoggingSinks = true,
#   endif
          }
        };
        NVSDK_NGX_FeatureDiscoveryInfo featureDiscoveryInfo {
          .SDKVersion          = NVSDK_NGX_Version_API,
          .FeatureID           = NVSDK_NGX_Feature_SuperSampling,
          .Identifier          = ngxIdentifier,
          .ApplicationDataPath = L"./",
          .FeatureInfo         = &featureCommonInfo,
        };
        // clang-format on
    } applicationInfo;

#    ifdef ENABLE_VULKAN
    struct RAII_NGXVulkanResource {
        explicit RAII_NGXVulkanResource()                           = default;
        RAII_NGXVulkanResource(const RAII_NGXVulkanResource& other) = default;
        RAII_NGXVulkanResource(RAII_NGXVulkanResource&& other)      = default;

        RAII_NGXVulkanResource& operator=(const RAII_NGXVulkanResource& other) = default;
        RAII_NGXVulkanResource& operator=(RAII_NGXVulkanResource&& other)      = default;

        void                   ChangeResource(VkImageView view, VkImage image, VkImageAspectFlags aspect, VkFormat format, Settings::Resolution resolution);
        NVSDK_NGX_Resource_VK& GetResource();
        void                   Destroy();

        ~RAII_NGXVulkanResource();

    private:
        NVSDK_NGX_Resource_VK resource{};
    } *color, *depth, *motion, *output{nullptr};
#    endif

    NVSDK_NGX_Handle*            featureHandle{};
    NVSDK_NGX_Parameter*         parameters{};
    NVSDK_NGX_DLSS_Create_Params DLSSCreateParams{};

    static Status (DLSS::*fpInitialize)();
    static Status (DLSS::*fpCreate)();
    static Status (DLSS::*fpEvaluate)();
    static Status (DLSS::*fpRelease)();
    static Status (DLSS::*fpShutdown)();

    static SupportState supported;
#    ifdef ENABLE_VULKAN
    static SupportState instanceExtensionsSupported;
    static SupportState deviceExtensionsSupported;
#    endif

    static uint32_t users;

#    ifdef ENABLE_VULKAN
    Status VulkanInitialize();
    Status VulkanCreate();
    Status VulkanUpdateResource(RAII_NGXVulkanResource* resource, Plugin::ImageID imageID);
    Status VulkanEvaluate();
    Status VulkanRelease();
    Status VulkanShutdown();
#    endif

#    ifdef ENABLE_DX12
    Status DX12Initialize();
    Status DX12Create();
    Status DX12Evaluate();
    Status DX12Release();
    Status DX12Shutdown();
#    endif

#    ifdef ENABLE_DX11
    Status DX11Initialize();
    Status DX11Create();
    Status DX11Evaluate();
    Status DX11Release();
    Status DX11Shutdown();
#    endif

    /// Sets current status to the status represented by t_error if there is no current status. Use resetStatus to
    /// clear the current status.
    Status setStatus(NVSDK_NGX_Result t_error, std::string t_msg);

    static void log(const char* message, NVSDK_NGX_Logging_Level loggingLevel, NVSDK_NGX_Feature sourceComponent);

public:
#    ifdef ENABLE_VULKAN
    static std::vector<std::string> requestVulkanInstanceExtensions(const std::vector<std::string>& supportedExtensions);
    static std::vector<std::string> requestVulkanDeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice, const std::vector<std::string>& supportedExtensions);
#    endif

    static bool isSupported();

    explicit DLSS(GraphicsAPI::Type type);
    ~DLSS() final;

    constexpr Type getType() final {
        return Upscaler::DLSS;
    };

    constexpr std::string getName() final {
        return "NVIDIA Deep Learning Super Sampling";
    };

    Status getOptimalSettings(Settings::Resolution resolution, Settings::Preset preset, enum Settings::Quality mode, bool hdr) final;

    Status initialize() final;
    Status create() final;
    Status evaluate() final;
    Status shutdown() final;
};
#endif