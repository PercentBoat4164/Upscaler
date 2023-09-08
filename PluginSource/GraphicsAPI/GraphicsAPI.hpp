#pragma once

namespace GraphicsAPI {
class GraphicsAPI {
public:
    GraphicsAPI(const GraphicsAPI &) = delete;
    GraphicsAPI(GraphicsAPI &&) = default;
    GraphicsAPI &operator =(const GraphicsAPI &) = delete;
    GraphicsAPI &operator =(GraphicsAPI &&) = default;
    virtual ~GraphicsAPI() = default;
};
} // namespace GraphicsAPI