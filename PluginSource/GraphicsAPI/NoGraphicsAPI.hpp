#pragma once

#include "GraphicsAPI.hpp"

class NoGraphicsAPI : public GraphicsAPI {
private:
    NoGraphicsAPI() = default;

public:
    static NoGraphicsAPI *get();
    Type                  getType() override;
    void                  prepareForOneTimeSubmits() override;
    void                  finishOneTimeSubmits() override;
};
