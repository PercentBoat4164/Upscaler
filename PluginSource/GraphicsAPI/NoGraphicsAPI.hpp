#pragma once

#include "GraphicsAPI.hpp"

class NoGraphicsAPI final : public GraphicsAPI {
    NoGraphicsAPI() = default;

public:
    static NoGraphicsAPI *get();

    Type                  getType() override;

    void                  prepareForOneTimeSubmits() override;
    void                  finishOneTimeSubmits() override;

    bool                  useUnityInterfaces(IUnityInterfaces */* unused */) override;
};
