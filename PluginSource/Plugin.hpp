#pragma once
#include <IUnityGraphics.h>
#include <IUnityLog.h>

#include <filesystem>
#include <string_view>

namespace Plugin {
namespace Unity {
inline IUnityInterfaces* interfaces        = nullptr;
inline IUnityGraphics*   graphicsInterface = nullptr;
inline IUnityLog*        logInterface      = nullptr;
}  // namespace Unity

inline std::filesystem::path path = "";

inline void log(const std::string_view msg) { if (!msg.empty()) Unity::logInterface->Log(kUnityLogTypeWarning, msg.data(), "Upscaler native library: 'GfxPluginUpscaler.dll'", 0); }

enum ImageID : uint8_t {
    Color,
    Depth,
    Motion,
    Output,
    Reactive,
    Opaque
};

enum Events {
    Upscale,
    Generate
};

inline enum FrameGenerationProvider : uint8_t {
    None,
    FSR,
} frameGenerationProvider = None;

inline bool loadedCorrectly = false;
}  // namespace Plugin