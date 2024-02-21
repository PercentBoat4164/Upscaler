#include "GraphicsAPI.hpp"

GraphicsAPI::Type GraphicsAPI::type = NONE;

void GraphicsAPI::set(const UnityGfxRenderer renderer) {
    switch (renderer) {
#ifdef ENABLE_VULKAN
        case kUnityGfxRendererVulkan: type = VULKAN; break;
#endif
#ifdef ENABLE_DX12
        case kUnityGfxRendererD3D12: set<::DX12>(); break;
#endif
#ifdef ENABLE_DX11
        case kUnityGfxRendererD3D11: set<::DX11>(); break;
#endif
        default: type = NONE; break;
    }
}

GraphicsAPI::Type GraphicsAPI::getType() {
    return type;
}
