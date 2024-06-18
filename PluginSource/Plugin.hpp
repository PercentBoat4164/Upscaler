#pragma once
#include <IUnityGraphics.h>
#include <cstdint>

namespace Plugin {
namespace Unity {
static IUnityGraphics* graphicsInterface;
static int eventIDBase;
}  // namespace Unity

enum ImageID : uint16_t {
    SourceColor,
    Depth,
    Motion,
    OutputColor,
    ReactiveMask,
    OpaqueColor,
    IMAGE_ID_MAX_ENUM
};
}  // namespace Plugin