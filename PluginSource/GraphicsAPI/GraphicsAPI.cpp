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
#endif

#include <Upscaler/Upscaler.hpp>

GraphicsAPI::Type GraphicsAPI::type = NONE;

void GraphicsAPI::initialize(const UnityGfxRenderer renderer) {
    switch (renderer) {
#ifdef ENABLE_VULKAN
        case kUnityGfxRendererVulkan: {
            constexpr UnityVulkanPluginEventConfig vulkanEventConfig{
              .renderPassPrecondition = kUnityVulkanRenderPass_DontCare,
              .graphicsQueueAccess    = kUnityVulkanGraphicsQueueAccess_DontCare,
              .flags                  = 0
            };
            Vulkan::getGraphicsInterface()->ConfigureEvent(Plugin::Unity::eventIDBase, &vulkanEventConfig);
            (void)Vulkan::initializeOneTimeSubmits();
            type = VULKAN;
            break;
        }
#endif
#ifdef ENABLE_DX12
        case kUnityGfxRendererD3D12: {
            constexpr UnityD3D12PluginEventConfig d3d12EventConfig{
              .graphicsQueueAccess              = kUnityD3D12GraphicsQueueAccess_DontCare,
              .flags                            = kUnityD3D12EventConfigFlag_ModifiesCommandBuffersState,
              .ensureActiveRenderTextureIsBound = false
            };
            DX12::getGraphicsInterface()->ConfigureEvent(Plugin::Unity::eventIDBase, &d3d12EventConfig);
            (void)DX12::initializeOneTimeSubmits();
            type = DX12;
            break;
        }
#endif
#ifdef ENABLE_DX11
        case kUnityGfxRendererD3D11: {
            DX11::createOneTimeSubmitContext();
            type = DX11;
            break;
        }
#endif
        default: type = NONE; break;
    }
    Upscaler::useGraphicsAPI(type);
}

void GraphicsAPI::shutdown() {
    switch (type) {
#ifdef ENABLE_VULKAN
        case VULKAN: Vulkan::shutdownOneTimeSubmits(); break;
#endif
#ifdef ENABLE_DX12
        case DX12: (void)DX12::shutdownOneTimeSubmits(); break;
#endif
#ifdef ENABLE_DX11
        case DX11: DX11::destroyOneTimeSubmitContext(); break;
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
