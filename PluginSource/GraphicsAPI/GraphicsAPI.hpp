#pragma once

// Unity
#include <IUnityGraphics.h>

// System
#include <concepts>
#include <vector>

class GraphicsAPI {
    static GraphicsAPI *graphicsAPIInUse;

public:
    enum Type {
        NONE,
        VULKAN,
        DX12,
        DX11,
    };

    GraphicsAPI()                    = default;
    GraphicsAPI(const GraphicsAPI &) = delete;
    GraphicsAPI(GraphicsAPI &&)      = default;

    GraphicsAPI &operator=(const GraphicsAPI &) = delete;
    GraphicsAPI &operator=(GraphicsAPI &&)      = default;

    template<typename T>
        requires std::derived_from<T, GraphicsAPI>
    constexpr static void set() {
        set(T::get());
    }

    static void set(UnityGfxRenderer);
    static void set(Type graphicsAPI);
    static void set(GraphicsAPI *graphicsAPI);

    template<typename T>
        requires std::derived_from<T, GraphicsAPI>
    static T *get() {
        return T::get();
    }

    static GraphicsAPI               *get(Type graphicsAPI);
    static GraphicsAPI               *get();
    static std::vector<GraphicsAPI *> getAllGraphicsAPIs();

    virtual Type getType() = 0;

    virtual bool useUnityInterfaces(IUnityInterfaces *) = 0;

    virtual ~GraphicsAPI() = default;
};
