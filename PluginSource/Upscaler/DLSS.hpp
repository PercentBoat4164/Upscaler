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
        RAII_NGXVulkanResource(const RAII_NGXVulkanResource &other) = default;
        RAII_NGXVulkanResource(RAII_NGXVulkanResource &&other)      = default;

        RAII_NGXVulkanResource &operator=(const RAII_NGXVulkanResource &other) = default;
        RAII_NGXVulkanResource &operator=(RAII_NGXVulkanResource &&other)      = default;

        void                   ChangeResource(const NVSDK_NGX_ImageViewInfo_VK &info);
        NVSDK_NGX_Resource_VK &GetResource();
        void                   Destroy();

        ~RAII_NGXVulkanResource();

    private:
        NVSDK_NGX_Resource_VK resource{};
    };
#    endif

    union Resource {
#    ifdef ENABLE_VULKAN
        RAII_NGXVulkanResource *vulkan;
#    endif
#    ifdef ENABLE_DX12
        ID3D12Resource *dx12;
#    endif
#    ifdef ENABLE_DX11
        ID3D11Resource *dx11;
#    endif
    };

    Application applicationInfo;

    NVSDK_NGX_Handle            *featureHandle{};
    NVSDK_NGX_Parameter         *parameters{};
    NVSDK_NGX_DLSS_Create_Params DLSSCreateParams;

    Resource inColor{nullptr};
    Resource outColor{nullptr};
    Resource depth{nullptr};
    Resource motion{nullptr};

    static Status (DLSS::*graphicsAPIIndependentInitializeFunctionPointer)();
    static Status (DLSS::*graphicsAPIIndependentGetParametersFunctionPointer)();
    static Status (DLSS::*graphicsAPIIndependentCreateFunctionPointer)();
    static Status (DLSS::*graphicsAPIIndependentSetDepthBufferFunctionPointer)(
      void *,
      UnityRenderingExtTextureFormat
    );
    static Status (DLSS::*graphicsAPIIndependentSetInputColorFunctionPointer)(
      void *,
      UnityRenderingExtTextureFormat
    );
    static Status (DLSS::*graphicsAPIIndependentSetMotionVectorsFunctionPointer)(
      void *,
      UnityRenderingExtTextureFormat
    );
    static Status (DLSS::*graphicsAPIIndependentSetOutputColorFunctionPointer)(
      void *,
      UnityRenderingExtTextureFormat
    );
    static Status (DLSS::*graphicsAPIIndependentEvaluateFunctionPointer)();
    static Status (DLSS::*graphicsAPIIndependentReleaseFunctionPointer)();
    static Status (DLSS::*graphicsAPIIndependentShutdownFunctionPointer)();

#    ifdef ENABLE_VULKAN
    Status VulkanInitialize();
    Status VulkanGetParameters();
    Status VulkanCreate();
    Status VulkanSetDepthBuffer(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status VulkanSetInputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status VulkanSetMotionVectors(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status VulkanSetOutputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status VulkanEvaluate();
    Status VulkanRelease();
    Status VulkanDestroyParameters();
    Status VulkanShutdown();
#    endif

#    ifdef ENABLE_DX12
    Status DX12Initialize();
    Status DX12GetParameters();
    Status DX12CreateFeature();
    Status DX12SetDepthBuffer(void *nativeHandle, UnityRenderingExtTextureFormat /* unused */);
    Status DX12SetInputColor(void *nativeHandle, UnityRenderingExtTextureFormat /* unused */);
    Status DX12SetMotionVectors(void *nativeHandle, UnityRenderingExtTextureFormat /* unused */);
    Status DX12SetOutputColor(void *nativeHandle, UnityRenderingExtTextureFormat /* unused */);
    Status DX12Evaluate();
    Status DX12ReleaseFeature();
    Status DX12DestroyParameters();
    Status DX12Shutdown();
#    endif

#    ifdef ENABLE_DX11
    Status DX11Initialize();
    Status DX11GetParameters();
    Status DX11CreateFeature();
    Status DX11SetDepthBuffer(void *nativeHandle, UnityRenderingExtTextureFormat /* unused */);
    Status DX11SetInputColor(void *nativeHandle, UnityRenderingExtTextureFormat /* unused */);
    Status DX11SetMotionVectors(void *nativeHandle, UnityRenderingExtTextureFormat /* unused */);
    Status DX11SetOutputColor(void *nativeHandle, UnityRenderingExtTextureFormat /* unused */);
    Status DX11Evaluate();
    Status DX11ReleaseFeature();
    Status DX11DestroyParameters();
    Status DX11Shutdown();
#    endif

    void setFunctionPointers(GraphicsAPI::Type graphicsAPI) override;

    /// Sets current status to the status represented by t_error if there is no current status. Use resetStatus to
    /// clear the current status.
    Status setStatus(NVSDK_NGX_Result t_error, std::string t_msg);

    static void log(const char* message, NVSDK_NGX_Logging_Level loggingLevel, NVSDK_NGX_Feature sourceComponent);

public:
    static DLSS *get();

    Type        getType() override;
    std::string getName() override;

#    ifdef ENABLE_VULKAN
    std::vector<std::string> getRequiredVulkanInstanceExtensions() override;
    std::vector<std::string>
    getRequiredVulkanDeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice) override;
#    endif

    Settings getOptimalSettings(Settings::Resolution resolution, Settings::QualityMode mode, bool hdr) override;

    Status initialize() override;
    Status create() override;
    Status setDepthBuffer(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) override;
    Status setInputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) override;
    Status setMotionVectors(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) override;
    Status setOutputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) override;
    Status evaluate() override;
    Status release() override;
    Status shutdown() override;
};
#endif