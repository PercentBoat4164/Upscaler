#pragma once

// Project
#include "Upscaler.hpp"

// Upscaler
#include <nvsdk_ngx_helpers.h>
#include <nvsdk_ngx_helpers_vk.h>

class DLSS : public Upscaler {
    struct Application {
        uint64_t                         id{231313132};
        std::wstring                     dataPath{L"./"};
        // clang-format off
        NVSDK_NGX_Application_Identifier ngxIdentifier {
          .IdentifierType = NVSDK_NGX_Application_Identifier_Type_Application_Id,
          .v              = {
            .ApplicationId = id,
          }
        };
        NVSDK_NGX_FeatureCommonInfo featureCommonInfo{
          .PathListInfo ={
            .Path   = new const wchar_t *{L"./Assets/Plugins"},
            .Length = 1,
            },
          .InternalData = nullptr,
          .LoggingInfo  = {
            .LoggingCallback = nullptr,
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

    static Upscaler::ErrorReason (DLSS::*graphicsAPIIndependentInitializeFunctionPointer)();
    static Upscaler::ErrorReason (DLSS::*graphicsAPIIndependentGetParametersFunctionPointer)();
    static Upscaler::ErrorReason (DLSS::*graphicsAPIIndependentCreateFeatureFunctionPointer)(NVSDK_NGX_DLSS_Create_Params);
    static Upscaler::ErrorReason (DLSS::*graphicsAPIIndependentSetImageResourcesFunctionPointer)(
      void *,
      UnityRenderingExtTextureFormat,
      void *,
      UnityRenderingExtTextureFormat,
      void *,
      UnityRenderingExtTextureFormat,
      void *,
      UnityRenderingExtTextureFormat
    );
    static Upscaler::ErrorReason (DLSS::*graphicsAPIIndependentEvaluateFunctionPointer)();
    static Upscaler::ErrorReason (DLSS::*graphicsAPIIndependentReleaseFeatureFunctionPointer)();
    static Upscaler::ErrorReason (DLSS::*graphicsAPIIndependentShutdownFunctionPointer)();

    Upscaler::ErrorReason VulkanInitialize();
    Upscaler::ErrorReason VulkanGetParameters();
    Upscaler::ErrorReason VulkanCreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams);
    void        VulkanDestroyImageViews();
    Upscaler::ErrorReason VulkanSetImageResources(
      void                          *nativeDepthBuffer,
      UnityRenderingExtTextureFormat unityDepthFormat,
      void                          *nativeMotionVectors,
      UnityRenderingExtTextureFormat unityMotionVectorFormat,
      void                          *nativeInColor,
      UnityRenderingExtTextureFormat unityInColorFormat,
      void                          *nativeOutColor,
      UnityRenderingExtTextureFormat unityOutColorFormat
    );
    Upscaler::ErrorReason VulkanEvaluate();
    Upscaler::ErrorReason VulkanReleaseFeature();
    Upscaler::ErrorReason VulkanDestroyParameters();
    Upscaler::ErrorReason VulkanShutdown();
#ifdef ENABLE_DX12
    Upscaler::ErrorReason DX12Initialize();
    Upscaler::ErrorReason DX12GetParameters();
    Upscaler::ErrorReason DX12CreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams);
    Upscaler::ErrorReason DX12SetImageResources(
      void *nativeDepthBuffer,
      UnityRenderingExtTextureFormat,
      void *nativeMotionVectors,
      UnityRenderingExtTextureFormat,
      void *nativeInColor,
      UnityRenderingExtTextureFormat,
      void *nativeOutColor,
      UnityRenderingExtTextureFormat
    );
    Upscaler::ErrorReason DX12Evaluate();
    Upscaler::ErrorReason DX12ReleaseFeature();
    Upscaler::ErrorReason DX12DestroyParameters();
    Upscaler::ErrorReason DX12Shutdown();
#endif

#ifdef ENABLE_DX11
    Upscaler::ErrorReason DX11Initialize();
    Upscaler::ErrorReason DX11GetParameters();
    Upscaler::ErrorReason DX11CreateFeature(NVSDK_NGX_DLSS_Create_Params DLSSCreateParams);
    Upscaler::ErrorReason DX11SetImageResources(
      void *nativeDepthBuffer,
      UnityRenderingExtTextureFormat,
      void *nativeMotionVectors,
      UnityRenderingExtTextureFormat,
      void *nativeInColor,
      UnityRenderingExtTextureFormat,
      void *nativeOutColor,
      UnityRenderingExtTextureFormat
    );
    Upscaler::ErrorReason DX11Evaluate();
    Upscaler::ErrorReason DX11ReleaseFeature();
    Upscaler::ErrorReason DX11DestroyParameters();
    Upscaler::ErrorReason DX11Shutdown();
#endif

    void setFunctionPointers(GraphicsAPI::Type graphicsAPI) override;

    DLSS() = default;

    /// Sets current error to the error represented by t_error if there is no current error. Use resetError to clear the current error.
    ErrorReason setError(NVSDK_NGX_Result);

public:
    static DLSS *get();

    std::vector<std::string> getRequiredVulkanInstanceExtensions() override;

    std::vector<std::string>
    getRequiredVulkanDeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice) override;

    Settings getOptimalSettings(Settings::Resolution t_outputResolution, Settings::Quality t_quality, bool t_HDR) override;

    Type getType() override;

    std::string getName() override;

    ErrorReason initialize() override;

    ErrorReason createFeature() override;

    ErrorReason setImageResources(
      void                          *nativeDepthBuffer,
      UnityRenderingExtTextureFormat unityDepthFormat,
      void                          *nativeMotionVectors,
      UnityRenderingExtTextureFormat unityMotionVectorFormat,
      void                          *nativeInColor,
      UnityRenderingExtTextureFormat unityInColorFormat,
      void                          *nativeOutColor,
      UnityRenderingExtTextureFormat unityOutColorFormat
    ) override;

    ErrorReason evaluate() override;

    ErrorReason releaseFeature() override;

    ErrorReason shutdown() override;
};