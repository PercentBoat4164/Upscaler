#include "GraphicsAPI.hpp"

// Project
#include "DX11.hpp"
#include "DX12.hpp"
#include "NoGraphicsAPI.hpp"
#include "Upscaler/Upscaler.hpp"
#include "Vulkan.hpp"

GraphicsAPI *GraphicsAPI::graphicsAPIInUse{(GraphicsAPI *)get<NoGraphicsAPI>()};

GraphicsAPI *GraphicsAPI::get(GraphicsAPI::Type graphicsAPI) {
    switch (graphicsAPI) {
        case NONE: return get<NoGraphicsAPI>();
        case VULKAN: return get<Vulkan>();
        case DX12: return get<::DX12>();
        case DX11: return get<::DX11>();
    }
    return nullptr;
}

GraphicsAPI *GraphicsAPI::get() {
    return graphicsAPIInUse;
}

void GraphicsAPI::set(GraphicsAPI::Type graphicsAPI) {
    set(get(graphicsAPI));
}

void GraphicsAPI::set(GraphicsAPI *graphicsAPI) {
    graphicsAPIInUse = graphicsAPI;
    if (graphicsAPI == nullptr)
        Upscaler::setGraphicsAPI(NONE);
    else
        Upscaler::setGraphicsAPI(graphicsAPI->getType());
}

std::vector<GraphicsAPI *> GraphicsAPI::getAllGraphicsAPIs() {
    return {
      get<NoGraphicsAPI>(),
      get<Vulkan>(),
      get<::DX12>(),
      get<::DX11>(),
    };
}
