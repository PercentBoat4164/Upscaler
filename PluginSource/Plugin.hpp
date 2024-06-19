#pragma once
#include <IUnityGraphics.h>
#include <cstdint>

namespace Plugin {
namespace Unity {
static IUnityGraphics* graphicsInterface;
static int eventIDBase;
}  // namespace Unity

enum ImageID : uint16_t {
    Color,
    Depth,
    Motion,
    Output,
    ReactiveMask,
    OpaqueColor,
    IMAGE_ID_MAX_ENUM
};
}  // namespace Plugin