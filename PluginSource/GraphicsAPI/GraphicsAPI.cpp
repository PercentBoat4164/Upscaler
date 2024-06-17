#include "GraphicsAPI.hpp"

#include "Plugin.hpp"

#ifdef ENABLE_VULKAN
#    include "Vulkan.hpp"

#    include "IUnityGraphicsVulkan.h"
#endif
#ifdef ENABLE_DX12
#    include "DX12.hpp"

#    include "d3d12compatibility.h"

#    include "IUnityGraphicsD3D12.h"
#endif
#ifdef ENABLE_DX11
#    include "DX11.hpp"

#    include "IUnityGraphicsD3D11.h"
#endif

GraphicsAPI::Type GraphicsAPI::type = NONE;

void GraphicsAPI::set(const UnityGfxRenderer renderer) {
    switch (renderer) {
#ifdef ENABLE_VULKAN
        case kUnityGfxRendererVulkan: {
            UnityVulkanPluginEventConfig vulkanEventConfig {
              .renderPassPrecondition = kUnityVulkanRenderPass_DontCare,
              .graphicsQueueAccess = kUnityVulkanGraphicsQueueAccess_DontCare,
              .flags = 0
            };
            Vulkan::getGraphicsInterface()->ConfigureEvent(Plugin::Unity::eventIDBase + Plugin::Event::Upscale, &vulkanEventConfig);
            type = VULKAN;
            break;
        }
#endif
#ifdef ENABLE_DX12
        case kUnityGfxRendererD3D12: {
            UnityD3D12PluginEventConfig d3d12EventConfig {
              .graphicsQueueAccess = kUnityD3D12GraphicsQueueAccess_DontCare,
              .flags = 0U,
              .ensureActiveRenderTextureIsBound = false
            };
            DX12::getGraphicsInterface()->ConfigureEvent(Plugin::Unity::eventIDBase + Plugin::Event::Upscale, &d3d12EventConfig);
            type = DX12;
            break;
        }
#endif
#ifdef ENABLE_DX11
        case kUnityGfxRendererD3D11: type = DX11; break;
#endif
        default: type = NONE; break;
    }
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
    return result;
}
