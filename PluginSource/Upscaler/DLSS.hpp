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

    static Upscaler::Status (DLSS::*graphicsAPIIndependentInitializeFunctionPointer)();
    static Upscaler::Status (DLSS::*graphicsAPIIndependentGetParametersFunctionPointer)();
    static Upscaler::Status (DLSS::*graphicsAPIIndependentCreateFeatureFunctionPointer)(
      NVSDK_NGX_DLSS_Create_Params
    );
    static Upscaler::Status (DLSS::*graphicsAPIIndependentSetImageResourcesFunctionPointer)(
      void *,
      UnityRenderingExtTextureFormat,
      void *,
      UnityRenderingExtTextureFormat,
      void *,
      UnityRenderingExtTextureFormat,
      void *,
      UnityRenderingExtTextureFormat
    );
    static Upscaler::Status (DLSS::*graphicsAPIIndependentEvaluateFunctionPointer)();
    static Upscaler::Status (DLSS::*graphicsAPIIndependentReleaseFeatureFunctionPointer)();
    static Upscaler::Status (DLSS::*graphicsAPIIndependentShutdownFunctionPointer)();

    Upscaler::Status         VulkanInitialize();
    Upscaler::Status         VulkanGetParameters();
    Upscaler::Status         VulkanCreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams);
    void                     VulkanDestroyImageViews();
    Upscaler::Status         VulkanSetImageResources(
      void                          *nativeDepthBuffer,
      UnityRenderingExtTextureFormat unityDepthFormat,
      void                          *nativeMotionVectors,
      UnityRenderingExtTextureFormat unityMotionVectorFormat,
      void                          *nativeInColor,
      UnityRenderingExtTextureFormat unityInColorFormat,
      void                          *nativeOutColor,
      UnityRenderingExtTextureFormat unityOutColorFormat
    );
    Upscaler::Status VulkanEvaluate();
    Upscaler::Status VulkanReleaseFeature();
    Upscaler::Status VulkanDestroyParameters();
    Upscaler::Status VulkanShutdown();
#ifdef ENABLE_DX12
    Upscaler::Status DX12Initialize();
    Upscaler::Status DX12GetParameters();
    Upscaler::Status DX12CreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams);
    Upscaler::Status DX12SetImageResources(
      void *nativeDepthBuffer,
      UnityRenderingExtTextureFormat,
      void *nativeMotionVectors,
      UnityRenderingExtTextureFormat,
      void *nativeInColor,
      UnityRenderingExtTextureFormat,
      void *nativeOutColor,
      UnityRenderingExtTextureFormat
    );
    Upscaler::Status DX12Evaluate();
    Upscaler::Status DX12ReleaseFeature();
    Upscaler::Status DX12DestroyParameters();
    Upscaler::Status DX12Shutdown();
#endif

#ifdef ENABLE_DX11
    Upscaler::Status DX11Initialize();
    Upscaler::Status DX11GetParameters();
    Upscaler::Status DX11CreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams);
    Upscaler::Status DX11SetImageResources(
      void *nativeDepthBuffer,
      UnityRenderingExtTextureFormat,
      void *nativeMotionVectors,
      UnityRenderingExtTextureFormat,
      void *nativeInColor,
      UnityRenderingExtTextureFormat,
      void *nativeOutColor,
      UnityRenderingExtTextureFormat
    );
    Upscaler::Status DX11Evaluate();
    Upscaler::Status DX11ReleaseFeature();
    Upscaler::Status DX11DestroyParameters();
    Upscaler::Status DX11Shutdown();
#endif

    void setFunctionPointers(GraphicsAPI::Type graphicsAPI) override;

    DLSS() = default;

    /// Sets current error to the error represented by t_error if there is no current error. Use resetError to
    /// clear the current error.
    Status setError(NVSDK_NGX_Result, std::string);

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

    Status setImageResources(
      void                          *nativeDepthBuffer,
      UnityRenderingExtTextureFormat unityDepthFormat,
      void                          *nativeMotionVectors,
      UnityRenderingExtTextureFormat unityMotionVectorFormat,
      void                          *nativeInColor,
      UnityRenderingExtTextureFormat unityInColorFormat,
      void                          *nativeOutColor,
      UnityRenderingExtTextureFormat unityOutColorFormat
    ) override;

    Status evaluate() override;

    Status releaseFeature() override;

    Status shutdown() override;
};