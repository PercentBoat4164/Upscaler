#pragma once
#ifdef ENABLE_DLSS
#    include "GraphicsAPI/GraphicsAPI.hpp"
#    include "Plugin.hpp"
#    include "Upscaler.hpp"

struct NVSDK_NGX_Resource_VK;
struct NVSDK_NGX_Parameter;
struct NVSDK_NGX_DLSS_Create_Params;

class DLSS final : public Upscaler {
    static struct alignas(128) Application {
        NVSDK_NGX_Application_Identifier ngxIdentifier {
          .IdentifierType = NVSDK_NGX_Application_Identifier_Type_Application_Id,
          .v              = {
            .ApplicationId = 0xDC98EECU,
          }
        };
        NVSDK_NGX_FeatureCommonInfo featureCommonInfo{
          .PathListInfo {
            .Path   = new const wchar_t *{L"./Assets/Plugins"},
            .Length = 1U,
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
    } applicationInfo;

    NVSDK_NGX_Handle*            handle{};
    NVSDK_NGX_Parameter*         parameters{};

    static Status (DLSS::*fpInitialize)();
    static Status (DLSS::*fpCreate)(NVSDK_NGX_DLSS_Create_Params*);
    static Status (DLSS::*fpEvaluate)();
    static Status (DLSS::*fpGetParameters)();
    static Status (DLSS::*fpShutdown)();

    static SupportState supported;
    static std::atomic<uint32_t> users;

#    ifdef ENABLE_VULKAN
    Status VulkanGetParameters();
    Status VulkanInitialize();
    Status VulkanCreate(NVSDK_NGX_DLSS_Create_Params* createParams);
    Status VulkanGetResource(NVSDK_NGX_Resource_VK& resource, Plugin::ImageID imageID);
    Status VulkanEvaluate();
    Status VulkanRelease();
    Status VulkanShutdown();
#    endif

#    ifdef ENABLE_DX12
    Status DX12GetParameters();
    Status DX12Initialize();
    Status DX12Create(NVSDK_NGX_DLSS_Create_Params* createParams);
    Status DX12Evaluate();
    Status DX12Release();
    Status DX12Shutdown();
#    endif

#    ifdef ENABLE_DX11
    Status DX11GetParameters();
    Status DX11Initialize();
    Status DX11Create(NVSDK_NGX_DLSS_Create_Params* createParams);
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
    static bool isSupported(enum Settings::Quality mode);

    explicit DLSS(GraphicsAPI::Type type);
    DLSS(const DLSS&)            = delete;
    DLSS(DLSS&&)                 = delete;
    DLSS& operator=(const DLSS&) = delete;
    DLSS& operator=(DLSS&&)      = delete;
    ~DLSS() override;

    constexpr Type getType() override {
        return Upscaler::DLSS;
    }

    constexpr std::string getName() override {
        return "NVIDIA Deep Learning Super Sampling";
    }

    Status useSettings(Settings::Resolution resolution, Settings::DLSSPreset preset, enum Settings::Quality mode, bool hdr) override;
    Status evaluate() override;
};
#endif