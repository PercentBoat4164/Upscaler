#pragma once
#ifdef ENABLE_XESS
#    include "GraphicsAPI/GraphicsAPI.hpp"
#    include "Upscaler.hpp"
#    include "Plugin.hpp"

#    ifdef ENABLE_VULKAN
#        define NOMINMAX
#        include <xess/xess_vk.h>
#    endif
#    ifdef ENABLE_DX12
#        define NOMINMAX
#        include <xess/xess_d3d12.h>
#    endif
#    ifdef ENABLE_DX11
#        define NOMINMAX
#        include <xess/xess_d3d11.h>
#    endif

#    define NOMINMAX
#    include <Windows.h>

#    include <array>

class XeSS_Upscaler final : public Upscaler {
    union XeSSResource {
        xess_vk_image_view_info vulkan;
        ID3D12Resource*         dx12;
        ID3D11Texture2D*        dx11;
    };

    static HMODULE library;
    static HMODULE dx11library;
    static bool    loaded;

    static Status (XeSS_Upscaler::*fpCreate)(const void*);
    static Status (XeSS_Upscaler::*fpSetImages)(const std::array<void*, 4>&);
    static Status (XeSS_Upscaler::*fpEvaluate)(Resolution) const;

    xess_context_handle_t       context{nullptr};
    std::array<XeSSResource, 4> resources{};

    static decltype(&xessGetOptimalInputResolution) xessGetOptimalInputResolution;
    static decltype(&xessDestroyContext)            xessDestroyContext;
    static decltype(&xessSetVelocityScale)          xessSetVelocityScale;
    static decltype(&xessSetLoggingCallback)        xessSetLoggingCallback;
#    ifdef ENABLE_VULKAN
    static decltype(&xessVKGetRequiredInstanceExtensions) xessVKGetRequiredInstanceExtensions;
    static decltype(&xessVKCreateContext)                 xessVKCreateContext;
    static decltype(&xessVKBuildPipelines)                xessVKBuildPipelines;
    static decltype(&xessVKInit)                          xessVKInit;
    static decltype(&xessVKExecute)                       xessVKExecute;
#    endif
#    ifdef ENABLE_DX12
    static decltype(&xessD3D12CreateContext)  xessD3D12CreateContext;
    static decltype(&xessD3D12BuildPipelines) xessD3D12BuildPipelines;
    static decltype(&xessD3D12Init)           xessD3D12Init;
    static decltype(&xessD3D12Execute)        xessD3D12Execute;
#    endif
#    ifdef ENABLE_DX11
    static decltype(&xessD3D11CreateContext) xessD3D11CreateContext;
    static decltype(&xessD3D11Init)          xessD3D11Init;
    static decltype(&xessD3D11Execute)       xessD3D11Execute;
#    endif

#    ifdef ENABLE_VULKAN
    Status               VulkanCreate(const void*);
    Status               VulkanSetImages(const std::array<void*, 4>&);
    [[nodiscard]] Status VulkanEvaluate(Resolution inputResolution) const;
#    endif
#    ifdef ENABLE_DX12
    Status               DX12Create(const void*);
    Status               DX12SetImages(const std::array<void*, 4>&);
    [[nodiscard]] Status DX12Evaluate(Resolution inputResolution) const;
#    endif
#    ifdef ENABLE_DX11
    Status               DX11Create(const void*);
    Status               DX11SetImages(const std::array<void*, 4>&);
    [[nodiscard]] Status DX11Evaluate(Resolution inputResolution) const;
#    endif

    static Status setStatus(xess_result_t t_error);
    static void log(const char* msg, xess_logging_level_t /*unused*/);

    [[nodiscard]] xess_quality_settings_t getQuality(enum Quality quality) const;

public:
    static bool loadedCorrectly();
    static void load(GraphicsAPI::Type type, void*);
    static void unload();
    static void useGraphicsAPI(GraphicsAPI::Type type);

    XeSS_Upscaler()                                = default;
    XeSS_Upscaler(const XeSS_Upscaler&)            = delete;
    XeSS_Upscaler(XeSS_Upscaler&&)                 = delete;
    XeSS_Upscaler& operator=(const XeSS_Upscaler&) = delete;
    XeSS_Upscaler& operator=(XeSS_Upscaler&&)      = delete;
    ~XeSS_Upscaler() override;

    Status useSettings(Resolution resolution, enum Quality mode, Flags flags);
    Status useImages(const std::array<void*, 4>& images);
    Status evaluate(Resolution inputResolution);
};
#endif
