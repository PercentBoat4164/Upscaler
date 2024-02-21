#pragma once

#include "GraphicsAPI.hpp"

class NoGraphicsAPI final : public GraphicsAPI {
public:
    NoGraphicsAPI() = delete;
};
