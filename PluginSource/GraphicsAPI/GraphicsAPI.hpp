#pragma once

// Unity
#include <IUnityGraphics.h>

class GraphicsAPI {
public:
    enum Type {
        NONE,
        VULKAN,
        DX12,
        DX11,
    };

protected:
    static Type type;

public:
    GraphicsAPI()                              = default;
    GraphicsAPI(const GraphicsAPI&)            = delete;
    GraphicsAPI(GraphicsAPI&&)                 = delete;
    GraphicsAPI& operator=(const GraphicsAPI&) = delete;
    GraphicsAPI& operator=(GraphicsAPI&&)      = delete;

    static void set(UnityGfxRenderer);
    static Type getType();

    virtual ~GraphicsAPI() = default;
};
