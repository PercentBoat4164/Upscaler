#pragma once

// Unity
#include <IUnityGraphics.h>

// System
#include <concepts>

class GraphicsAPI {
private:
    static GraphicsAPI    *graphicsAPIInUse;

public:
    enum Type {
        NONE,
        VULKAN,
    };

    GraphicsAPI()                               = default;
    GraphicsAPI(const GraphicsAPI &)            = delete;
    GraphicsAPI(GraphicsAPI &&)                 = default;
    GraphicsAPI &operator=(const GraphicsAPI &) = delete;
    GraphicsAPI &operator=(GraphicsAPI &&)      = default;

    template<typename T>
        requires std::derived_from<T, GraphicsAPI>
    constexpr static T *get() {
        return T::get();
    }

    static GraphicsAPI *get(Type graphicsAPI);

    static GraphicsAPI *get();

    template<typename T>
        requires std::derived_from<T, GraphicsAPI>
    constexpr static void set() {
        set(T::get());
    }

    static void set(Type graphicsAPI);

    static void set(GraphicsAPI *graphicsAPI);

    virtual Type getType() = 0;

    virtual void prepareForOneTimeSubmits() = 0;

    virtual void finishOneTimeSubmits() = 0;

    virtual ~GraphicsAPI() = default;
};
