#ifdef ENABLE_XESS
#    include "XeSS_Upscaler.hpp"

#    include <xess/xess.h>
#    ifdef ENABLE_VULKAN
#        include <GraphicsAPI/Vulkan.hpp>

#        include <IUnityGraphicsVulkan.h>
#    endif
#    ifdef ENABLE_DX12
#        include "GraphicsAPI/DX12.hpp"

#        include <d3d12compatibility.h>

#        include <IUnityGraphicsD3D12.h>

#        include <xess/xess_d3d12.h>
#    endif
#    ifdef ENABLE_DX11
#        include "GraphicsAPI/DX11.hpp"

#        include <d3d11.h>

#        include <IUnityGraphicsD3D11.h>

#        include <xess/xess_d3d11.h>
#    endif

HMODULE XeSS_Upscaler::library{nullptr};
HMODULE XeSS_Upscaler::dx11library{nullptr};
bool    XeSS_Upscaler::loaded{false};

Upscaler::Status (XeSS_Upscaler::* XeSS_Upscaler::fpCreate)(const void*){&safeFail};
Upscaler::Status (XeSS_Upscaler::* XeSS_Upscaler::fpSetImages)(const std::array<void*, 4>&){&safeFail};
Upscaler::Status (XeSS_Upscaler::* XeSS_Upscaler::fpEvaluate)(Resolution) const {&safeFail};

decltype(&xessGetOptimalInputResolution) XeSS_Upscaler::xessGetOptimalInputResolution{nullptr};
decltype(&xessDestroyContext)            XeSS_Upscaler::xessDestroyContext{nullptr};
decltype(&xessSetVelocityScale)          XeSS_Upscaler::xessSetVelocityScale{nullptr};
decltype(&xessSetLoggingCallback)        XeSS_Upscaler::xessSetLoggingCallback{nullptr};
#    ifdef ENABLE_VULKAN
decltype(&xessVKGetRequiredInstanceExtensions) XeSS_Upscaler::xessVKGetRequiredInstanceExtensions{nullptr};
decltype(&xessVKCreateContext)                 XeSS_Upscaler::xessVKCreateContext{nullptr};
decltype(&xessVKBuildPipelines)                XeSS_Upscaler::xessVKBuildPipelines{nullptr};
decltype(&xessVKInit)                          XeSS_Upscaler::xessVKInit{nullptr};
decltype(&xessVKExecute)                       XeSS_Upscaler::xessVKExecute{nullptr};
#    endif
#    ifdef ENABLE_DX12
decltype(&xessD3D12CreateContext)  XeSS_Upscaler::xessD3D12CreateContext{nullptr};
decltype(&xessD3D12BuildPipelines) XeSS_Upscaler::xessD3D12BuildPipelines{nullptr};
decltype(&xessD3D12Init)           XeSS_Upscaler::xessD3D12Init{nullptr};
decltype(&xessD3D12Execute)        XeSS_Upscaler::xessD3D12Execute{nullptr};
#    endif
#    ifdef ENABLE_DX11
decltype(&xessD3D11CreateContext) XeSS_Upscaler::xessD3D11CreateContext{nullptr};
decltype(&xessD3D11Init)          XeSS_Upscaler::xessD3D11Init{nullptr};
decltype(&xessD3D11Execute)       XeSS_Upscaler::xessD3D11Execute{nullptr};
#    endif

Upscaler::Status XeSS_Upscaler::setStatus(const xess_result_t t_error) {
    switch (t_error) {
        case XESS_RESULT_WARNING_NONEXISTING_FOLDER:
        case XESS_RESULT_WARNING_OLD_DRIVER:
        case XESS_RESULT_SUCCESS: return Success;
        case XESS_RESULT_ERROR_UNSUPPORTED_DEVICE: return DeviceNotSupported;
        case XESS_RESULT_ERROR_UNSUPPORTED_DRIVER: return DriversOutOfDate;
        case XESS_RESULT_ERROR_UNINITIALIZED:
        case XESS_RESULT_ERROR_INVALID_ARGUMENT: return FatalRuntimeError;
        case XESS_RESULT_ERROR_DEVICE_OUT_OF_MEMORY: return OutOfMemory;
        case XESS_RESULT_ERROR_DEVICE:
        case XESS_RESULT_ERROR_NOT_IMPLEMENTED:
        case XESS_RESULT_ERROR_INVALID_CONTEXT:
        case XESS_RESULT_ERROR_OPERATION_IN_PROGRESS:
        case XESS_RESULT_ERROR_UNSUPPORTED:
        case XESS_RESULT_ERROR_CANT_LOAD_LIBRARY:
        case XESS_RESULT_ERROR_UNKNOWN:
        default: return FatalRuntimeError;
    }
}

void XeSS_Upscaler::log(const char* msg, const xess_logging_level_t /*unused*/) {
    Plugin::log(msg);
}

xess_quality_settings_t XeSS_Upscaler::getQuality(const enum Quality quality) const {
    switch (quality) {
        case Auto: {
            const uint32_t pixelCount{outputResolution.width * outputResolution.height};
            if (pixelCount <= 2560U * 1440U) return XESS_QUALITY_SETTING_QUALITY;
            if (pixelCount <= 3840U * 2160U) return XESS_QUALITY_SETTING_PERFORMANCE;
            return XESS_QUALITY_SETTING_ULTRA_PERFORMANCE;
        }
        case AntiAliasing: return XESS_QUALITY_SETTING_AA;
        case UltraQualityPlus: return XESS_QUALITY_SETTING_ULTRA_QUALITY_PLUS;
        case UltraQuality: return XESS_QUALITY_SETTING_ULTRA_QUALITY;
        case Quality: return XESS_QUALITY_SETTING_QUALITY;
        case Balanced: return XESS_QUALITY_SETTING_BALANCED;
        case Performance: return XESS_QUALITY_SETTING_PERFORMANCE;
        case UltraPerformance: return XESS_QUALITY_SETTING_ULTRA_PERFORMANCE;
        default: return static_cast<xess_quality_settings_t>(-1);
    }
}

#    ifdef ENABLE_VULKAN
Upscaler::Status XeSS_Upscaler::VulkanCreate(const void* params) {
    const auto*               vkParams = static_cast<const xess_vk_init_params_t*>(params);
    const UnityVulkanInstance instance = Vulkan::getGraphicsInterface()->Instance();
    RETURN_WITH_MESSAGE_IF(setStatus(xessVKCreateContext(instance.instance, instance.physicalDevice, instance.device, &context)), "Failed to create the Intel Xe Super Sampling context.");
    RETURN_WITH_MESSAGE_IF(setStatus(xessVKBuildPipelines(context, vkParams->pipelineCache, true, vkParams->initFlags)), "Failed to build Xe Super Sampling pipelines.");
    RETURN_WITH_MESSAGE_IF(setStatus(xessVKInit(context, vkParams)), "Failed to initialize the Intel Xe Super Sampling context.");
    return Success;
}

Upscaler::Status XeSS_Upscaler::VulkanSetImages(const std::array<void*, 4>& images) {
    for (Plugin::ImageID id{0}; id < images.size(); ++reinterpret_cast<uint8_t&>(id)) {
        UnityVulkanImage image {};
        Vulkan::getGraphicsInterface()->AccessTexture(images.at(id), UnityVulkanWholeImage, id == Plugin::Output ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | (id == Plugin::Output ? VK_ACCESS_SHADER_WRITE_BIT : 0), kUnityVulkanResourceAccess_PipelineBarrier, &image);
        RETURN_STATUS_WITH_MESSAGE_IF(image.image == VK_NULL_HANDLE, RecoverableRuntimeError, "Unity provided a `VK_NULL_HANDLE` image.");
        XeSSResource& resource = resources.at(id);
        Vulkan::destroyImageView(resource.vulkan.imageView);
        resource = XeSSResource{.vulkan = {
            .imageView = Vulkan::createImageView(image.image, image.format, image.aspect),
            .image = image.image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .format = image.format,
            .width = image.extent.width,
            .height = image.extent.height
        }};
    }
    return Success;
}

Upscaler::Status XeSS_Upscaler::VulkanEvaluate(const Resolution inputResolution) const {
    const xess_vk_image_view_info  motion = resources.at(Plugin::Motion).vulkan;
    const xess_vk_execute_params_t params {
      .colorTexture    = resources.at(Plugin::Color).vulkan,
      .velocityTexture = motion,
      .depthTexture    = resources.at(Plugin::Depth).vulkan,
      .outputTexture   = resources.at(Plugin::Output).vulkan,
      .jitterOffsetX   = jitter.x,
      .jitterOffsetY   = jitter.y,
      .exposureScale   = 1.0F,
      .resetHistory    = static_cast<uint32_t>(resetHistory),
      .inputWidth      = inputResolution.width,
      .inputHeight     = inputResolution.height,
    };
    UnityVulkanRecordingState state{};
    Vulkan::getGraphicsInterface()->EnsureOutsideRenderPass();
    RETURN_STATUS_WITH_MESSAGE_IF(!Vulkan::getGraphicsInterface()->CommandRecordingState(&state, kUnityVulkanGraphicsQueueAccess_DontCare), FatalRuntimeError, "Unable to obtain a command recording state from Unity. This is fatal.");
    RETURN_WITH_MESSAGE_IF(setStatus(xessSetVelocityScale(context, -static_cast<float>(motion.width), -static_cast<float>(motion.height))), "Failed to set motion scale.");
    RETURN_WITH_MESSAGE_IF(setStatus(xessVKExecute(context, state.commandBuffer, &params)), "Failed to execute Intel Xe Super Sampling.");
    return Success;
}
#endif

#    ifdef ENABLE_DX12
Upscaler::Status XeSS_Upscaler::DX12Create(const void* params) {
    const auto* dx12Params = static_cast<const xess_d3d12_init_params_t*>(params);
    RETURN_WITH_MESSAGE_IF(setStatus(xessD3D12CreateContext(DX12::getGraphicsInterface()->GetDevice(), &context)), "Failed to create the Intel Xe Super Sampling context.");
    RETURN_WITH_MESSAGE_IF(setStatus(xessD3D12BuildPipelines(context, dx12Params->pPipelineLibrary, true, dx12Params->initFlags)), "Failed to build Xe Super Sampling pipelines.");
    RETURN_WITH_MESSAGE_IF(setStatus(xessD3D12Init(context, dx12Params)), "Failed to initialize the Intel Xe Super Sampling context.");
    return Success;
}

Upscaler::Status XeSS_Upscaler::DX12SetImages(const std::array<void*, 4>& images) {
    for (const void* image : images) RETURN_STATUS_WITH_MESSAGE_IF(image == nullptr, RecoverableRuntimeError, "Unity provided a `nullptr` image.");
    resources.at(Plugin::Color).dx12  = static_cast<ID3D12Resource*>(images.at(Plugin::Color));
    resources.at(Plugin::Depth).dx12  = static_cast<ID3D12Resource*>(images.at(Plugin::Depth));
    resources.at(Plugin::Motion).dx12 = static_cast<ID3D12Resource*>(images.at(Plugin::Motion));
    resources.at(Plugin::Output).dx12 = static_cast<ID3D12Resource*>(images.at(Plugin::Output));
    return Success;
}

Upscaler::Status XeSS_Upscaler::DX12Evaluate(const Resolution inputResolution) const {
    const D3D12_RESOURCE_DESC motionDescription = resources.at(Plugin::Motion).dx12->GetDesc();
    const xess_d3d12_execute_params_t params {
      .pColorTexture    = resources.at(Plugin::Color).dx12,
      .pVelocityTexture = resources.at(Plugin::Motion).dx12,
      .pDepthTexture    = resources.at(Plugin::Depth).dx12,
      .pOutputTexture   = resources.at(Plugin::Output).dx12,
      .jitterOffsetX    = jitter.x,
      .jitterOffsetY    = jitter.y,
      .exposureScale    = 1.0F,
      .resetHistory     = static_cast<uint32_t>(resetHistory),
      .inputWidth       = inputResolution.width,
      .inputHeight      = inputResolution.height
    };
    UnityGraphicsD3D12RecordingState state{};
    RETURN_STATUS_WITH_MESSAGE_IF(!DX12::getGraphicsInterface()->CommandRecordingState(&state), FatalRuntimeError, "Unable to obtain a command recording state from Unity. This is fatal.");
    RETURN_WITH_MESSAGE_IF(setStatus(xessSetVelocityScale(context, -static_cast<float>(motionDescription.Width), -static_cast<float>(motionDescription.Height))), "Failed to set motion scale.");
    RETURN_WITH_MESSAGE_IF(setStatus(xessD3D12Execute(context, state.commandList, &params)), "Failed to execute Intel Xe Super Sampling.");
    return Success;
}
#endif

#    ifdef ENABLE_DX11
Upscaler::Status XeSS_Upscaler::DX11Create(const void* params) {
    RETURN_WITH_MESSAGE_IF(setStatus(xessD3D11CreateContext(DX11::getGraphicsInterface()->GetDevice(), &context)), "Failed to create the Intel Xe Super Sampling context.");
    RETURN_WITH_MESSAGE_IF(setStatus(xessD3D11Init(context, static_cast<const xess_d3d11_init_params_t*>(params))), "Failed to initialize the Intel Xe Super Sampling context.");
    return Success;
}

Upscaler::Status XeSS_Upscaler::DX11SetImages(const std::array<void*, 4>& images) {
    for (const void* image : images) RETURN_STATUS_WITH_MESSAGE_IF(image == nullptr, RecoverableRuntimeError, "Unity provided a `nullptr` image.");
    resources.at(Plugin::Color).dx11 = static_cast<ID3D11Texture2D*>(images.at(Plugin::Color));
    resources.at(Plugin::Depth).dx11 = static_cast<ID3D11Texture2D*>(images.at(Plugin::Depth));
    resources.at(Plugin::Motion).dx11 = static_cast<ID3D11Texture2D*>(images.at(Plugin::Motion));
    resources.at(Plugin::Output).dx11 = static_cast<ID3D11Texture2D*>(images.at(Plugin::Output));
    return Success;
}

Upscaler::Status XeSS_Upscaler::DX11Evaluate(const Resolution inputResolution) const {
    D3D11_TEXTURE2D_DESC motionDescription;
    resources.at(Plugin::Motion).dx11->GetDesc(&motionDescription);
    const xess_d3d11_execute_params_t params {
      .pColorTexture    = resources.at(Plugin::Color).dx11,
      .pVelocityTexture = resources.at(Plugin::Motion).dx11,
      .pDepthTexture    = resources.at(Plugin::Depth).dx11,
      .pOutputTexture   = resources.at(Plugin::Output).dx11,
      .jitterOffsetX    = jitter.x,
      .jitterOffsetY    = jitter.y,
      .exposureScale    = 1.0F,
      .resetHistory     = static_cast<uint32_t>(resetHistory),
      .inputWidth       = inputResolution.width,
      .inputHeight      = inputResolution.height
    };
    RETURN_WITH_MESSAGE_IF(setStatus(xessSetVelocityScale(context, -static_cast<float>(motionDescription.Width), -static_cast<float>(motionDescription.Height))), "Failed to set motion scale.");
    RETURN_WITH_MESSAGE_IF(setStatus(xessD3D11Execute(context, &params)), "Failed to execute Intel Xe Super Sampling.");
    return Success;
}
#    endif

bool XeSS_Upscaler::loadedCorrectly() {
    return loaded;
}

void XeSS_Upscaler::load(const GraphicsAPI::Type type, void* /*unused*/) {
    library = LoadLibrary((Plugin::path / "libxess.dll").string().c_str());
    if (library == nullptr) return (void)(loaded = false);
    xessGetOptimalInputResolution = reinterpret_cast<decltype(xessGetOptimalInputResolution)>(GetProcAddress(library, "xessGetOptimalInputResolution"));
    xessDestroyContext            = reinterpret_cast<decltype(xessDestroyContext)>(GetProcAddress(library, "xessDestroyContext"));
    xessSetVelocityScale          = reinterpret_cast<decltype(xessSetVelocityScale)>(GetProcAddress(library, "xessSetVelocityScale"));
    xessSetLoggingCallback        = reinterpret_cast<decltype(xessSetLoggingCallback)>(GetProcAddress(library, "xessSetLoggingCallback"));
    if (xessGetOptimalInputResolution == nullptr || xessDestroyContext == nullptr || xessSetVelocityScale == nullptr || xessSetLoggingCallback == nullptr) return (void)(loaded = false);
    switch (type) {
#    ifdef ENABLE_VULKAN
        case GraphicsAPI::VULKAN:
            xessVKGetRequiredInstanceExtensions = reinterpret_cast<decltype(xessVKGetRequiredInstanceExtensions)>(GetProcAddress(library, "xessVKGetRequiredInstanceExtensions"));
            xessVKCreateContext                 = reinterpret_cast<decltype(xessVKCreateContext)>(GetProcAddress(library, "xessVKCreateContext"));
            xessVKBuildPipelines                = reinterpret_cast<decltype(xessVKBuildPipelines)>(GetProcAddress(library, "xessVKBuildPipelines"));
            xessVKInit                          = reinterpret_cast<decltype(xessVKInit)>(GetProcAddress(library, "xessVKInit"));
            xessVKExecute                       = reinterpret_cast<decltype(xessVKExecute)>(GetProcAddress(library, "xessVKExecute"));
            if (xessVKGetRequiredInstanceExtensions == nullptr || xessVKCreateContext == nullptr || xessVKBuildPipelines == nullptr || xessVKInit == nullptr || xessVKExecute == nullptr) return (void) (loaded = false);
            break;
#    endif
#    ifdef ENABLE_DX12
        case GraphicsAPI::DX12:
            xessD3D12CreateContext  = reinterpret_cast<decltype(xessD3D12CreateContext)>(GetProcAddress(library, "xessD3D12CreateContext"));
            xessD3D12BuildPipelines = reinterpret_cast<decltype(xessD3D12BuildPipelines)>(GetProcAddress(library, "xessD3D12BuildPipelines"));
            xessD3D12Init           = reinterpret_cast<decltype(xessD3D12Init)>(GetProcAddress(library, "xessD3D12Init"));
            xessD3D12Execute        = reinterpret_cast<decltype(xessD3D12Execute)>(GetProcAddress(library, "xessD3D12Execute"));
            if (xessD3D12CreateContext == nullptr || xessD3D12BuildPipelines == nullptr || xessD3D12Init == nullptr || xessD3D12Execute == nullptr) return (void)(loaded = false);
            break;
#    endif
#    ifdef ENABLE_DX11
        case GraphicsAPI::DX11:
            dx11library = LoadLibrary((Plugin::path / "libxess_dx11.dll").string().c_str());
            if (dx11library == nullptr) return (void)(loaded = false);
            xessD3D11CreateContext = reinterpret_cast<decltype(xessD3D11CreateContext)>(GetProcAddress(dx11library, "xessD3D11CreateContext"));
            xessD3D11Init          = reinterpret_cast<decltype(xessD3D11Init)>(GetProcAddress(dx11library, "xessD3D11Init"));
            xessD3D11Execute       = reinterpret_cast<decltype(xessD3D11Execute)>(GetProcAddress(dx11library, "xessD3D11Execute"));
            if (xessD3D11CreateContext == nullptr || xessD3D11Init == nullptr || xessD3D11Execute == nullptr) return (void)(loaded = false);
            break;
#    endif
        default: return (void)(loaded = false);
    }
    loaded = true;
}

void XeSS_Upscaler::unload() {
    xessGetOptimalInputResolution = nullptr;
    xessDestroyContext            = nullptr;
    xessSetVelocityScale          = nullptr;
    xessSetLoggingCallback        = nullptr;
#    ifdef ENABLE_VULKAN
    xessVKCreateContext  = nullptr;
    xessVKBuildPipelines = nullptr;
    xessVKInit           = nullptr;
    xessVKExecute        = nullptr;
#    endif
#    ifdef ENABLE_DX12
    xessD3D12CreateContext  = nullptr;
    xessD3D12BuildPipelines = nullptr;
    xessD3D12Init           = nullptr;
    xessD3D12Execute        = nullptr;
#    endif
#    ifdef ENABLE_DX11
    xessD3D11CreateContext = nullptr;
    xessD3D11Init          = nullptr;
    xessD3D11Execute       = nullptr;
    if (dx11library != nullptr) FreeLibrary(dx11library);
    dx11library = nullptr;
#    endif
    if (library != nullptr) FreeLibrary(library);
    library = nullptr;
}

void XeSS_Upscaler::useGraphicsAPI(const GraphicsAPI::Type type) {
    switch (type) {
#    ifdef ENABLE_VULKAN
        case GraphicsAPI::VULKAN: {
            fpCreate    = &XeSS_Upscaler::VulkanCreate;
            fpSetImages = &XeSS_Upscaler::VulkanSetImages;
            fpEvaluate  = &XeSS_Upscaler::VulkanEvaluate;
            break;
        }
#    endif
#    ifdef ENABLE_DX12
        case GraphicsAPI::DX12: {
            fpCreate    = &XeSS_Upscaler::DX12Create;
            fpSetImages = &XeSS_Upscaler::DX12SetImages;
            fpEvaluate  = &XeSS_Upscaler::DX12Evaluate;
            break;
        }
#    endif
#    ifdef ENABLE_DX11
        case GraphicsAPI::DX11: {
            fpCreate    = &XeSS_Upscaler::DX11Create;
            fpSetImages = &XeSS_Upscaler::DX11SetImages;
            fpEvaluate  = &XeSS_Upscaler::DX11Evaluate;
            break;
        }
#    endif
        default: {
            fpCreate    = &safeFail<UnsupportedGraphicsApi>;
            fpSetImages = &safeFail<UnsupportedGraphicsApi>;
            fpEvaluate  = &safeFail<UnsupportedGraphicsApi>;
            break;
        }
    }
}

XeSS_Upscaler::~XeSS_Upscaler() {
    if (context != nullptr) RETURN_VOID_WITH_MESSAGE_IF(setStatus(xessDestroyContext(context)), "Failed to destroy the Intel Xe Super Sampling context.");
    context = nullptr;
}

Upscaler::Status XeSS_Upscaler::useSettings(const Resolution resolution, const enum Quality mode, const Flags flags) {
    outputResolution = resolution;
    const xess_d3d12_init_params_t params{
      .outputResolution = {.x = outputResolution.width, .y = outputResolution.height},
      .qualitySetting   = getQuality(mode),
      .initFlags =
        static_cast<uint32_t>(XESS_INIT_FLAG_ENABLE_AUTOEXPOSURE) |
        static_cast<uint32_t>(XESS_INIT_FLAG_INVERTED_DEPTH) |
        ((flags & OutputResolutionMotionVectors) == OutputResolutionMotionVectors ? XESS_INIT_FLAG_HIGH_RES_MV : XESS_INIT_FLAG_NONE) |
        ((flags & EnableHDR) == EnableHDR ? XESS_INIT_FLAG_LDR_INPUT_COLOR : XESS_INIT_FLAG_NONE)
    };
    if (context != nullptr) RETURN_WITH_MESSAGE_IF(setStatus(xessDestroyContext(context)), "Failed to destroy the Intel Xe Super Sampling context.");
    context = nullptr;
    RETURN_IF((this->*fpCreate)(&params));
#    ifndef NDEBUG
    RETURN_WITH_MESSAGE_IF(setStatus(xessSetLoggingCallback(context, XESS_LOGGING_LEVEL_DEBUG, &XeSS_Upscaler::log)), "Failed to set logging callback.");
#    endif
    const xess_2d_t dstRes{outputResolution.width, outputResolution.height};
    xess_2d_t       optimal, min, max;
    RETURN_WITH_MESSAGE_IF(setStatus(xessGetOptimalInputResolution(context, &dstRes, params.qualitySetting, &optimal, &min, &max)), "Failed to get dynamic resolution parameters.");
    recommendedInputResolution    = Resolution{optimal.x, optimal.y};
    dynamicMinimumInputResolution = Resolution{min.x, min.y};
    dynamicMaximumInputResolution = Resolution{max.x, max.y};
    return Success;
}

Upscaler::Status XeSS_Upscaler::useImages(const std::array<void*, 4>& images) {
    return (this->*fpSetImages)(images);
}

Upscaler::Status XeSS_Upscaler::evaluate(const Resolution inputResolution) {
    return (this->*fpEvaluate)(inputResolution);
}
#endif