#pragma once

// Project
#include "Upscaler.hpp"

// Upscaler
#include <nvsdk_ngx_helpers.h>
#include <nvsdk_ngx_helpers_vk.h>

class DLSS final : public Upscaler {
    struct Application {
        uint64_t     id{231313132};
        std::wstring dataPath{L"./"};
        // clang-format off
        NVSDK_NGX_Application_Identifier ngxIdentifier {
          .IdentifierType = NVSDK_NGX_Application_Identifier_Type_Application_Id,
          .v              = {
            .ApplicationId = id,
          }
        };
        NVSDK_NGX_FeatureCommonInfo featureCommonInfo{
          .PathListInfo {
            .Path   = new const wchar_t *{L"./Assets/Plugins"},
            .Length = 1,
          },
          .InternalData = nullptr,
          .LoggingInfo {
            .LoggingCallback = nullptr,
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
        // clang-format on
    } __attribute__((aligned(128))) applicationInfo;

    struct RAII_NGXVulkanResource {
        explicit RAII_NGXVulkanResource()=default;
        void                  ChangeResource(const NVSDK_NGX_ImageViewInfo_VK &info);
        [[nodiscard]] NVSDK_NGX_Resource_VK GetResource() const;
        void Destroy() const;
        ~RAII_NGXVulkanResource();

        RAII_NGXVulkanResource(const RAII_NGXVulkanResource &other)=default;
        RAII_NGXVulkanResource &operator=(const RAII_NGXVulkanResource &other)=default;
        RAII_NGXVulkanResource(RAII_NGXVulkanResource &&other)=default;
        RAII_NGXVulkanResource &operator=(RAII_NGXVulkanResource &&other)=default;

    private:
        NVSDK_NGX_Resource_VK resource{};
    } __attribute__((aligned(64)));

    NVSDK_NGX_Handle    *featureHandle{};
    NVSDK_NGX_Parameter *parameters{};

    union {
        RAII_NGXVulkanResource *vulkan;
        ID3D12Resource         *dx12;
        ID3D11Resource         *dx11;
    } inColor;

    union {
        RAII_NGXVulkanResource *vulkan;
        ID3D12Resource        *dx12;
        ID3D11Resource        *dx11;
    } outColor;

    union {
        RAII_NGXVulkanResource *vulkan;
        ID3D12Resource         *dx12;
        ID3D11Resource         *dx11;
    } depth;

    union {
        RAII_NGXVulkanResource *vulkan;
        ID3D12Resource         *dx12;
        ID3D11Resource         *dx11;
    } motion;

    static Status (DLSS::*graphicsAPIIndependentInitializeFunctionPointer)();
    static Status (DLSS::*graphicsAPIIndependentGetParametersFunctionPointer)();
    static Status (DLSS::*graphicsAPIIndependentCreateFeatureFunctionPointer)(
      NVSDK_NGX_DLSS_Create_Params
    );
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
    static Status (DLSS::*graphicsAPIIndependentReleaseFeatureFunctionPointer)();
    static Status (DLSS::*graphicsAPIIndependentShutdownFunctionPointer)();

#ifdef ENABLE_VULKAN
    Status VulkanInitialize();
    Status VulkanGetParameters();
    Status VulkanCreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams);
    Status VulkanSetDepthBuffer(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status VulkanSetInputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status VulkanSetMotionVectors(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status VulkanSetOutputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status VulkanEvaluate();
    Status VulkanReleaseFeature();
    Status VulkanDestroyParameters();
    Status VulkanShutdown();
#endif

#ifdef ENABLE_DX12
    Status DX12Initialize();
    Status DX12GetParameters();
    Status DX12CreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams);
    Status DX12SetDepthBuffer(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status DX12SetInputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status DX12SetMotionVectors(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status DX12SetOutputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status DX12Evaluate();
    Status DX12ReleaseFeature();
    Status DX12DestroyParameters();
    Status DX12Shutdown();
#endif

#ifdef ENABLE_DX11
    Status DX11Initialize();
    Status DX11GetParameters();
    Status DX11CreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams);
    Status DX11SetDepthBuffer(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status DX11SetInputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status DX11SetMotionVectors(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status DX11SetOutputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat);
    Status DX11Evaluate();
    Status DX11ReleaseFeature();
    Status DX11DestroyParameters();
    Status DX11Shutdown();
#endif

    void setFunctionPointers(GraphicsAPI::Type graphicsAPI) override;

    DLSS() = default;

    /// Sets current status to the status represented by t_error if there is no current status. Use resetStatus to
    /// clear the current status.
    Status setStatus(NVSDK_NGX_Result, std::string);

public:
    static DLSS *get();
    std::vector<std::string> getRequiredVulkanInstanceExtensions() override;
    std::vector<std::string>
    getRequiredVulkanDeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice) override;
    Settings
    getOptimalSettings(Settings::Resolution t_outputResolution, Settings::Quality t_quality, bool t_HDR) override;
    Type getType() override;
    std::string getName() override;
    Status initialize() override;
    Status createFeature() override;
    Status setDepthBuffer(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) override;
    Status setInputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) override;
    Status setMotionVectors(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) override;
    Status setOutputColor(void *nativeHandle, UnityRenderingExtTextureFormat unityFormat) override;
    Status evaluate() override;
    Status releaseFeature() override;
    Status shutdown() override;
    ~DLSS() override=default;
};