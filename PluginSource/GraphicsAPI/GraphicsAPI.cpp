#include "GraphicsAPI.hpp"

// Project
#include "DX11.hpp"
#include "DX12.hpp"
#include "NoGraphicsAPI.hpp"
#include "Upscaler/Upscaler.hpp"
#include "Vulkan.hpp"

GraphicsAPI *GraphicsAPI::graphicsAPIInUse{(GraphicsAPI *) get<NoGraphicsAPI>()};

GraphicsAPI *GraphicsAPI::get(GraphicsAPI::Type graphicsAPI) {
    switch (graphicsAPI) {
        case NONE: return get<NoGraphicsAPI>();
#ifdef ENABLE_VULKAN
        case VULKAN: return get<Vulkan>();
#endif
#ifdef ENABLE_DX12
        case DX12: return get<::DX12>();
#endif
#ifdef ENABLE_DX11
        case DX11: return get<::DX11>();
#endif
    }
    return get<NoGraphicsAPI>();
}

GraphicsAPI *GraphicsAPI::get() {
    return graphicsAPIInUse;
}

void GraphicsAPI::set(GraphicsAPI::Type graphicsAPI) {
    set(get(graphicsAPI));
}

void GraphicsAPI::set(GraphicsAPI *graphicsAPI) {
    graphicsAPIInUse = graphicsAPI;
    if (graphicsAPI == nullptr) Upscaler::setGraphicsAPI(NONE);
    else Upscaler::setGraphicsAPI(graphicsAPI->getType());
}

void GraphicsAPI::set(UnityGfxRenderer renderer) {
    switch (renderer) {
#ifdef ENABLE_VULKAN
        case kUnityGfxRendererVulkan: GraphicsAPI::set<Vulkan>(); break;
#endif
#ifdef ENABLE_DX12
        case kUnityGfxRendererD3D12: GraphicsAPI::set<::DX12>(); break;
#endif
#ifdef ENABLE_DX11
        case kUnityGfxRendererD3D11: GraphicsAPI::set<::DX11>(); break;
#endif
        default: GraphicsAPI::set<NoGraphicsAPI>(); break;
    }
}

std::vector<GraphicsAPI *> GraphicsAPI::getAllGraphicsAPIs() {
    return {
      get<NoGraphicsAPI>(),
#ifdef ENABLE_VULKAN
      get<Vulkan>(),
#endif
#ifdef ENABLE_DX12
      get<::DX12>(),
#endif
#ifdef ENABLE_DX11
      get<::DX11>(),
#endif
    };
}
