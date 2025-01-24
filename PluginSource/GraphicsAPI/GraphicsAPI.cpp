#include "GraphicsAPI.hpp"

#include "Plugin.hpp"

#ifdef ENABLE_VULKAN
#    include "Vulkan.hpp"

#    include <IUnityGraphicsVulkan.h>
#endif
#ifdef ENABLE_DX12
#    include "DX12.hpp"

#    include <d3d12compatibility.h>

#    include <IUnityGraphicsD3D12.h>
#endif
#ifdef ENABLE_DX11
#    include "DX11.hpp"
#endif

#include "Upscaler/Upscaler.hpp"

GraphicsAPI::Type GraphicsAPI::type = NONE;

void GraphicsAPI::initialize(const UnityGfxRenderer renderer) {
    switch (renderer) {
#ifdef ENABLE_VULKAN
        case kUnityGfxRendererVulkan: {
            type = VULKAN;
            Upscaler::useGraphicsAPI(type);
            constexpr UnityVulkanPluginEventConfig eventConfig {
              .renderPassPrecondition = kUnityVulkanRenderPass_EnsureOutside,
              .graphicsQueueAccess    = kUnityVulkanGraphicsQueueAccess_DontCare,
              .flags                  = kUnityVulkanEventConfigFlag_ModifiesCommandBuffersState
            };
            Vulkan::getGraphicsInterface()->ConfigureEvent(Plugin::Unity::eventIDBase + Plugin::Events::Upscale, &eventConfig);
            Vulkan::getGraphicsInterface()->ConfigureEvent(Plugin::Unity::eventIDBase + Plugin::Events::FrameGenerate, &eventConfig);
            break;
        }
#endif
#ifdef ENABLE_DX12
        case kUnityGfxRendererD3D12: {
            type = DX12;
            Upscaler::useGraphicsAPI(type);
            constexpr UnityD3D12PluginEventConfig eventConfig {
              .graphicsQueueAccess              = kUnityD3D12GraphicsQueueAccess_DontCare,
              .flags                            = kUnityD3D12EventConfigFlag_ModifiesCommandBuffersState,
              .ensureActiveRenderTextureIsBound = false
            };
            DX12::getGraphicsInterface()->ConfigureEvent(Plugin::Unity::eventIDBase + Plugin::Events::Upscale, &eventConfig);
            DX12::getGraphicsInterface()->ConfigureEvent(Plugin::Unity::eventIDBase + Plugin::Events::FrameGenerate, &eventConfig);
            Upscaler::load(DX12);
            break;
        }
#endif
#ifdef ENABLE_DX11
        case kUnityGfxRendererD3D11: {
            type = DX11;
            Upscaler::useGraphicsAPI(type);
            Upscaler::load(DX11);
            break;
        }
#endif
        default: {
            type = NONE;
            Upscaler::useGraphicsAPI(type);
            break;
        }
    }
}

void GraphicsAPI::shutdown() {
    Upscaler::shutdown();
    type = NONE;
}

GraphicsAPI::Type GraphicsAPI::getType() {
    return type;
}

bool GraphicsAPI::registerUnityInterfaces(IUnityInterfaces* interfaces) {
    bool result = true;
#ifdef ENABLE_VULKAN
    result &= Vulkan::registerUnityInterfaces(interfaces);
#endif
#ifdef ENABLE_DX12
    result &= DX12::registerUnityInterfaces(interfaces);
#endif
#ifdef ENABLE_DX11
    result &= DX11::registerUnityInterfaces(interfaces);
#endif
    return result;
}

bool GraphicsAPI::unregisterUnityInterfaces() {
    bool result = true;
#ifdef ENABLE_VULKAN
    result &= Vulkan::unregisterUnityInterfaces();
#endif
#ifdef ENABLE_DX12
    result &= DX12::unregisterUnityInterfaces();
#endif
#ifdef ENABLE_DX11
    result &= DX11::unregisterUnityInterfaces();
#endif
    Upscaler::unload();
    return result;
}
