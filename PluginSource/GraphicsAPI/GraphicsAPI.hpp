#pragma once

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
    GraphicsAPI()                              = delete;
    GraphicsAPI(const GraphicsAPI&)            = delete;
    GraphicsAPI(GraphicsAPI&&)                 = delete;
    GraphicsAPI& operator=(const GraphicsAPI&) = delete;
    GraphicsAPI& operator=(GraphicsAPI&&)      = delete;
    ~GraphicsAPI()                             = delete;

    static void set(UnityGfxRenderer);
    static Type getType();

    static bool registerUnityInterfaces(IUnityInterfaces* interfaces);
    static bool unregisterUnityInterfaces();
};
