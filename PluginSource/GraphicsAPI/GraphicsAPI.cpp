#include "GraphicsAPI.hpp"

#include "Upscaler/Upscaler.hpp"
#include "Vulkan.hpp"

GraphicsAPI    *GraphicsAPI::graphicsAPIInUse{nullptr};
IUnityGraphics *GraphicsAPI::unityGraphics{nullptr};

GraphicsAPI *GraphicsAPI::get(GraphicsAPI::Type graphicsAPI) {
    switch (graphicsAPI) {
        case NONE: return nullptr;
        case VULKAN: return get<::Vulkan>();
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
