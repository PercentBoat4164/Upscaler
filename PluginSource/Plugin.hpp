#pragma once
#include <IUnityGraphics.h>

#include <cstddef>
#include <cstdint>

namespace Plugin {
namespace Unity {
static IUnityGraphics* graphicsInterface;
static int eventIDBase;
}  // namespace Unity

enum ImageID : uint8_t {
    Color,
    Depth,
    Motion,
    Output,
    Reactive,
    Opaque
};

constexpr std::size_t NumBaseImages = 4;
constexpr std::size_t NumImages = 6;
}  // namespace Plugin