#include "GraphicsAPI.hpp"
#ifdef ENABLE_VULKAN
#    include "Vulkan.hpp"
#endif
#ifdef ENABLE_DX12
#    include "DX12.hpp"
#endif
#ifdef ENABLE_DX11
#    include "DX11.hpp"
#endif

GraphicsAPI::Type GraphicsAPI::type = NONE;

void GraphicsAPI::set(const UnityGfxRenderer renderer) {
    switch (renderer) {
#ifdef ENABLE_VULKAN
        case kUnityGfxRendererVulkan: type = VULKAN; break;
#endif
#ifdef ENABLE_DX12
        case kUnityGfxRendererD3D12: type = DX12; break;
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
