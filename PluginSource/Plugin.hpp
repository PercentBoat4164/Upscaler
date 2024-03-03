#pragma once
#include <IUnityGraphics.h>
#include <cstdint>

namespace Plugin {
namespace Unity {
static IUnityGraphics* graphicsInterface;
static int eventIDBase;
}  // namespace Unity

enum Event {
    Prepare,
    Upscale,
};

enum ImageID : uint16_t {
    SourceColor,
    SourceDepth,
    Motion,
    OutputColor,
    IMAGE_ID_MAX_ENUM
};
}  // namespace Plugin