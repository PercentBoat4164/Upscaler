#pragma once

#include <concepts>

class GraphicsAPI {
private:
    static GraphicsAPI *graphicsAPIInUse;

public:
    enum Type {
        NONE,
        VULKAN,
    };

    GraphicsAPI(const GraphicsAPI &)            = delete;
    GraphicsAPI(GraphicsAPI &&)                 = default;
    GraphicsAPI &operator=(const GraphicsAPI &) = delete;
    GraphicsAPI &operator=(GraphicsAPI &&)      = default;

    template<typename T>
        requires std::derived_from<T, GraphicsAPI>
    static GraphicsAPI *get();

    static GraphicsAPI *get(Type graphicsAPI);

    static GraphicsAPI *get();

    virtual Type getType() = 0;

    virtual ~GraphicsAPI() = default;
};
