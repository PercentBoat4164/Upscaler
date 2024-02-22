#pragma once
#ifdef ENABLE_DLSS
// Project
#    include "Upscaler.hpp"

// Upscaler
#    include <nvsdk_ngx_helpers.h>
#    ifdef ENABLE_VULKAN
#        include <nvsdk_ngx_helpers_vk.h>
#    endif

class DLSS final : public Upscaler {
    struct Application {
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
    };

#    ifdef ENABLE_VULKAN
    struct RAII_NGXVulkanResource {
        explicit RAII_NGXVulkanResource()                           = default;
        RAII_NGXVulkanResource(const RAII_NGXVulkanResource& other) = default;
        RAII_NGXVulkanResource(RAII_NGXVulkanResource&& other)      = default;

        RAII_NGXVulkanResource& operator=(const RAII_NGXVulkanResource& other) = default;
        RAII_NGXVulkanResource& operator=(RAII_NGXVulkanResource&& other)      = default;

        void                   ChangeResource(const NVSDK_NGX_ImageViewInfo_VK& info);
        NVSDK_NGX_Resource_VK& GetResource();
        void                   Destroy();

        ~RAII_NGXVulkanResource();

    private:
        NVSDK_NGX_Resource_VK resource{};
    };
#    endif

    union Resource {
#    ifdef ENABLE_VULKAN
        RAII_NGXVulkanResource* vulkan;
#    endif
#    ifdef ENABLE_DX12
        ID3D12Resource* dx12;
#    endif
#    ifdef ENABLE_DX11
        ID3D11Resource* dx11;
#    endif
    };

    static Application applicationInfo;

    NVSDK_NGX_Handle*            featureHandle{};
    NVSDK_NGX_Parameter*         parameters{};
    NVSDK_NGX_DLSS_Create_Params DLSSCreateParams;

    Resource inColor{nullptr};
    Resource outColor{nullptr};
    Resource depth{nullptr};
    Resource motion{nullptr};

    static Status (DLSS::*fpInitialize)();
    static Status (DLSS::*fpGetParameters)();
    static Status (DLSS::*fpCreate)();
    static Status (DLSS::*fpSetDepth)(void*, UnityRenderingExtTextureFormat);
    static Status (DLSS::*fpSetInputColor)(void*, UnityRenderingExtTextureFormat);
    static Status (DLSS::*fpSetMotionVectors)(void*, UnityRenderingExtTextureFormat);
    static Status (DLSS::*fpSetOutputColor)(void*, UnityRenderingExtTextureFormat);
    static Status (DLSS::*fpEvaluate)();
    static Status (DLSS::*fpRelease)();
    static Status (DLSS::*fpShutdown)();

    static SupportState supported;
#    ifdef ENABLE_VULKAN
    static SupportState instanceExtensionsSupported;
    static SupportState deviceExtensionsSupported;
#    endif

#    ifdef ENABLE_VULKAN
    Status VulkanInitialize();
    Status VulkanGetParameters();
    Status VulkanCreate();
    Status VulkanSetDepth(void* nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status VulkanSetInputColor(void* nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status VulkanSetMotionVectors(void* nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status VulkanSetOutputColor(void* nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status VulkanEvaluate();
    Status VulkanRelease();
    Status VulkanDestroyParameters();
    Status VulkanShutdown();
#    endif

#    ifdef ENABLE_DX12
    Status DX12Initialize();
    Status DX12GetParameters();
    Status DX12CreateFeature();
    Status DX12SetDepth(void* nativeHandle, UnityRenderingExtTextureFormat /*unused*/);
    Status DX12SetInputColor(void* nativeHandle, UnityRenderingExtTextureFormat /*unused*/);
    Status DX12SetMotionVectors(void* nativeHandle, UnityRenderingExtTextureFormat /*unused*/);
    Status DX12SetOutputColor(void* nativeHandle, UnityRenderingExtTextureFormat /*unused*/);
    Status DX12Evaluate();
    Status DX12Release();
    Status DX12DestroyParameters();
    Status DX12Shutdown();
#    endif

#    ifdef ENABLE_DX11
    Status DX11Initialize();
    Status DX11GetParameters();
    Status DX11CreateFeature();
    Status DX11SetDepthBuffer(void* nativeHandle, UnityRenderingExtTextureFormat /*unused*/);
    Status DX11SetInputColor(void* nativeHandle, UnityRenderingExtTextureFormat /*unused*/);
    Status DX11SetMotionVectors(void* nativeHandle, UnityRenderingExtTextureFormat /*unused*/);
    Status DX11SetOutputColor(void* nativeHandle, UnityRenderingExtTextureFormat /*unused*/);
    Status DX11Evaluate();
    Status DX11Release();
    Status DX11DestroyParameters();
    Status DX11Shutdown();
#    endif

    /// Sets current status to the status represented by t_error if there is no current status. Use resetStatus to
    /// clear the current status.
    Status setStatus(NVSDK_NGX_Result t_error, std::string t_msg);

    static void log(const char* message, NVSDK_NGX_Logging_Level loggingLevel, NVSDK_NGX_Feature sourceComponent);

public:
    explicit DLSS(GraphicsAPI::Type);

#    ifdef ENABLE_VULKAN
    static std::vector<std::string> requestVulkanInstanceExtensions(const std::vector<std::string>&);
    static std::vector<std::string> requestVulkanDeviceExtensions(VkInstance, VkPhysicalDevice, const std::vector<std::string>&);
#    endif

    Type getType() override;
    std::string getName() override;
    bool isSupported() override;
    Settings getOptimalSettings(Settings::Resolution resolution, Settings::QualityMode mode, bool hdr) override;

    Status initialize() override;
    Status create() override;
    Status setDepth(void*, UnityRenderingExtTextureFormat) override;
    Status setInputColor(void*, UnityRenderingExtTextureFormat) override;
    Status setMotionVectors(void*, UnityRenderingExtTextureFormat) override;
    Status setOutputColor(void*, UnityRenderingExtTextureFormat) override;
    Status evaluate() override;
    Status release() override;
    Status shutdown() override;
};
#endif