#pragma once

#include "GraphicsAPI.hpp"

class NoGraphicsAPI final : public GraphicsAPI {
    NoGraphicsAPI() = default;

public:
    static NoGraphicsAPI *get();

    Type                  getType() override;

    bool                  useUnityInterfaces(IUnityInterfaces */* unused */) override;
};
