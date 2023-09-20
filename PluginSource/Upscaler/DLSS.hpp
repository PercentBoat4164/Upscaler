#pragma once

// Project
#include "Logger.hpp"
#include "Upscaler.hpp"

// Upscaler
#include <nvsdk_ngx_helpers.h>
#include <nvsdk_ngx_helpers_vk.h>

class DLSS : public Upscaler {
    struct Application {
        uint64_t                         id{231313132};
        std::wstring                     dataPath{L"./"};
        NVSDK_NGX_Application_Identifier ngxIdentifier{
          .IdentifierType = NVSDK_NGX_Application_Identifier_Type_Application_Id,
          .v              = {
                             .ApplicationId = id,
                             }
        };
        NVSDK_NGX_FeatureCommonInfo featureCommonInfo{
          .PathListInfo =
            {
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
        NVSDK_NGX_FeatureDiscoveryInfo featureDiscoveryInfo{
          .SDKVersion          = NVSDK_NGX_Version_API,
          .FeatureID           = NVSDK_NGX_Feature_SuperSampling,
          .Identifier          = ngxIdentifier,
          .ApplicationDataPath = dataPath.c_str(),
          .FeatureInfo         = &featureCommonInfo,
        };
    } applicationInfo;

    NVSDK_NGX_Handle    *featureHandle{};
    NVSDK_NGX_Parameter *parameters{};

    union {
        NVSDK_NGX_Resource_VK *vulkan;
        ID3D12Resource        *dx12;
        ID3D11Resource        *dx11;
    } depth;

    union {
        NVSDK_NGX_Resource_VK *vulkan;
        ID3D12Resource        *dx12;
        ID3D11Resource        *dx11;
    } motion;

    union {
        NVSDK_NGX_Resource_VK *vulkan;
        ID3D12Resource        *dx12;
        ID3D11Resource        *dx11;
    } inColor;

    union {
        NVSDK_NGX_Resource_VK *vulkan;
        ID3D12Resource        *dx12;
        ID3D11Resource        *dx11;
    } outColor;

    static bool (DLSS::*graphicsAPIIndependentInitializeFunctionPointer)();
    static bool (DLSS::*graphicsAPIIndependentGetParametersFunctionPointer)();
    static bool (DLSS::*graphicsAPIIndependentCreateFeatureFunctionPointer)(NVSDK_NGX_DLSS_Create_Params);
    static bool (DLSS::*graphicsAPIIndependentSetImageResourcesFunctionPointer)(
      void *,
      UnityRenderingExtTextureFormat,
      void *,
      UnityRenderingExtTextureFormat,
      void *,
      UnityRenderingExtTextureFormat,
      void *,
      UnityRenderingExtTextureFormat
    );
    static bool (DLSS::*graphicsAPIIndependentEvaluateFunctionPointer)();
    static bool (DLSS::*graphicsAPIIndependentReleaseFeatureFunctionPointer)();
    static bool (DLSS::*graphicsAPIIndependentShutdownFunctionPointer)();

    bool VulkanInitialize();

    bool VulkanGetParameters();

    bool VulkanCreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams);

    bool VulkanSetImageResources(
      void                          *nativeDepthBuffer,
      UnityRenderingExtTextureFormat unityDepthFormat,
      void                          *nativeMotionVectors,
      UnityRenderingExtTextureFormat unityMotionVectorFormat,
      void                          *nativeInColor,
      UnityRenderingExtTextureFormat unityInColorFormat,
      void                          *nativeOutColor,
      UnityRenderingExtTextureFormat unityOutColorFormat
    );

    bool VulkanEvaluate();

    bool VulkanReleaseFeature();

    bool VulkanDestroyParameters();

    bool VulkanShutdown();

#ifdef ENABLE_DX12
    bool DX12Initialize();

    bool DX12GetParameters();

    bool DX12CreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams);

    bool DX12SetDepthBuffer(
      void *,
      UnityRenderingExtTextureFormat,
      void *,
      UnityRenderingExtTextureFormat,
      void *,
      UnityRenderingExtTextureFormat,
      void *,
      UnityRenderingExtTextureFormat
    );

    bool DX12Evaluate();

    bool DX12ReleaseFeature();

    bool DX12DestroyParameters();

    bool DX12Shutdown();
#endif

#ifdef ENABLE_DX11
    bool DX11Initialize();

    bool DX11GetParameters();

    bool DX11CreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams);

    bool DX11SetDepthBuffer(
      void *,
      UnityRenderingExtTextureFormat,
      void *,
      UnityRenderingExtTextureFormat,
      void *,
      UnityRenderingExtTextureFormat,
      void *,
      UnityRenderingExtTextureFormat
    );

    bool DX11Evaluate();

    bool DX11ReleaseFeature();

    bool DX11DestroyParameters();

    bool DX11Shutdown();
#endif

    void setFunctionPointers(GraphicsAPI::Type graphicsAPI) override;

    DLSS() = default;

public:
    static DLSS *get();

    bool isSupported() override;

    bool isAvailable() override;

    /// Note: Can only set the value from true to false.
    bool isSupportedAfter(bool isSupported) override;

    void setSupported(bool isSupported) override;

    /// Note: Can only set the value from true to false.
    bool isAvailableAfter(bool isAvailable) override;

    void setAvailable(bool isAvailable) override;

    std::vector<std::string> getRequiredVulkanInstanceExtensions() override;

    std::vector<std::string>
    getRequiredVulkanDeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice) override;

    Settings getOptimalSettings(Settings::Resolution t_presentResolution) override;

    Type getType() override;

    std::string getName() override;

    bool initialize() override;

    bool createFeature() override;

    bool setImageResources(
      void                          *nativeDepthBuffer,
      UnityRenderingExtTextureFormat unityDepthFormat,
      void                          *nativeMotionVectors,
      UnityRenderingExtTextureFormat unityMotionVectorFormat,
      void                          *nativeInColor,
      UnityRenderingExtTextureFormat unityInColorFormat,
      void                          *nativeOutColor,
      UnityRenderingExtTextureFormat unityOutColorFormat
    ) override;

    bool evaluate() override;

    bool releaseFeature() override;

    bool shutdown() override;
};