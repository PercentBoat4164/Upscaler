#include "GraphicsAPI.hpp"

#include "Vulkan.hpp"

GraphicsAPI *GraphicsAPI::graphicsAPIInUse{nullptr};

template<typename T>
    requires std::derived_from<T, GraphicsAPI>
GraphicsAPI *GraphicsAPI::get() {
    return T::get();
}

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
