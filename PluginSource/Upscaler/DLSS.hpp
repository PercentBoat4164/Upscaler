#pragma once

// Project
#include "Upscaler.hpp"

// Upscaler
#include <nvsdk_ngx_helpers.h>
#include <nvsdk_ngx_helpers_vk.h>

class DLSS : public Upscaler {
private:
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
    } applicationInfo;

    NVSDK_NGX_Handle    *featureHandle{};
    NVSDK_NGX_Parameter *parameters{};

    union {
        NVSDK_NGX_Resource_VK *vulkan{VK_NULL_HANDLE};
        ID3D12Resource        *dx12;
        ID3D11Resource        *dx11;
    } depth;

    union {
        NVSDK_NGX_Resource_VK *vulkan{VK_NULL_HANDLE};
        ID3D12Resource        *dx12;
        ID3D11Resource        *dx11;
    } motion;

    union {
        NVSDK_NGX_Resource_VK *vulkan{VK_NULL_HANDLE};
        ID3D12Resource        *dx12;
        ID3D11Resource        *dx11;
    } inColor;

    union {
        NVSDK_NGX_Resource_VK *vulkan{VK_NULL_HANDLE};
        ID3D12Resource        *dx12;
        ID3D11Resource        *dx11;
    } outColor;

    static Upscaler::UpscalerStatus (DLSS::*graphicsAPIIndependentInitializeFunctionPointer)();
    static Upscaler::UpscalerStatus (DLSS::*graphicsAPIIndependentGetParametersFunctionPointer)();
    static Upscaler::UpscalerStatus (DLSS::*graphicsAPIIndependentCreateFeatureFunctionPointer)(
      NVSDK_NGX_DLSS_Create_Params
    );
    static Upscaler::UpscalerStatus (DLSS::*graphicsAPIIndependentSetImageResourcesFunctionPointer)(
      void *,
      UnityRenderingExtTextureFormat,
      void *,
      UnityRenderingExtTextureFormat,
      void *,
      UnityRenderingExtTextureFormat,
      void *,
      UnityRenderingExtTextureFormat
    );
    static Upscaler::UpscalerStatus (DLSS::*graphicsAPIIndependentEvaluateFunctionPointer)();
    static Upscaler::UpscalerStatus (DLSS::*graphicsAPIIndependentReleaseFeatureFunctionPointer)();
    static Upscaler::UpscalerStatus (DLSS::*graphicsAPIIndependentShutdownFunctionPointer)();

    Upscaler::UpscalerStatus VulkanInitialize();
    Upscaler::UpscalerStatus VulkanGetParameters();
    Upscaler::UpscalerStatus VulkanCreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams);
    void                  VulkanDestroyImageViews();
    Upscaler::UpscalerStatus VulkanSetImageResources(
      void                          *nativeDepthBuffer,
      UnityRenderingExtTextureFormat unityDepthFormat,
      void                          *nativeMotionVectors,
      UnityRenderingExtTextureFormat unityMotionVectorFormat,
      void                          *nativeInColor,
      UnityRenderingExtTextureFormat unityInColorFormat,
      void                          *nativeOutColor,
      UnityRenderingExtTextureFormat unityOutColorFormat
    );
    Upscaler::UpscalerStatus VulkanEvaluate();
    Upscaler::UpscalerStatus VulkanReleaseFeature();
    Upscaler::UpscalerStatus VulkanDestroyParameters();
    Upscaler::UpscalerStatus VulkanShutdown();
#ifdef ENABLE_DX12
    Upscaler::UpscalerStatus DX12Initialize();
    Upscaler::UpscalerStatus DX12GetParameters();
    Upscaler::UpscalerStatus DX12CreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams);
    Upscaler::UpscalerStatus DX12SetImageResources(
      void *nativeDepthBuffer,
      UnityRenderingExtTextureFormat,
      void *nativeMotionVectors,
      UnityRenderingExtTextureFormat,
      void *nativeInColor,
      UnityRenderingExtTextureFormat,
      void *nativeOutColor,
      UnityRenderingExtTextureFormat
    );
    Upscaler::UpscalerStatus DX12Evaluate();
    Upscaler::UpscalerStatus DX12ReleaseFeature();
    Upscaler::UpscalerStatus DX12DestroyParameters();
    Upscaler::UpscalerStatus DX12Shutdown();
#endif

#ifdef ENABLE_DX11
    Upscaler::UpscalerStatus DX11Initialize();
    Upscaler::UpscalerStatus DX11GetParameters();
    Upscaler::UpscalerStatus DX11CreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams);
    Upscaler::UpscalerStatus DX11SetImageResources(
      void *nativeDepthBuffer,
      UnityRenderingExtTextureFormat,
      void *nativeMotionVectors,
      UnityRenderingExtTextureFormat,
      void *nativeInColor,
      UnityRenderingExtTextureFormat,
      void *nativeOutColor,
      UnityRenderingExtTextureFormat
    );
    Upscaler::UpscalerStatus DX11Evaluate();
    Upscaler::UpscalerStatus DX11ReleaseFeature();
    Upscaler::UpscalerStatus DX11DestroyParameters();
    Upscaler::UpscalerStatus DX11Shutdown();
#endif

    void setFunctionPointers(GraphicsAPI::Type graphicsAPI) override;

    DLSS() = default;

    /// Sets current error to the error represented by t_error if there is no current error. Use resetError to
    /// clear the current error.
    UpscalerStatus setError(NVSDK_NGX_Result, std::string);

public:
    static DLSS *get();

    std::vector<std::string> getRequiredVulkanInstanceExtensions() override;

    std::vector<std::string>
    getRequiredVulkanDeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice) override;

    Settings
    getOptimalSettings(Settings::Resolution t_outputResolution, Settings::Quality t_quality, bool t_HDR) override;

    Type getType() override;

    std::string getName() override;

    UpscalerStatus initialize() override;

    UpscalerStatus createFeature() override;

    UpscalerStatus setImageResources(
      void                          *nativeDepthBuffer,
      UnityRenderingExtTextureFormat unityDepthFormat,
      void                          *nativeMotionVectors,
      UnityRenderingExtTextureFormat unityMotionVectorFormat,
      void                          *nativeInColor,
      UnityRenderingExtTextureFormat unityInColorFormat,
      void                          *nativeOutColor,
      UnityRenderingExtTextureFormat unityOutColorFormat
    ) override;

    UpscalerStatus evaluate() override;

    UpscalerStatus releaseFeature() override;

    UpscalerStatus shutdown() override;
};