#pragma once
#include <IUnityGraphics.h>
#include <IUnityLog.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>

namespace Plugin {
namespace Unity {
inline IUnityInterfaces* interfaces{nullptr};
inline IUnityGraphics*   graphicsInterface{nullptr};
inline IUnityLog*        logInterface{nullptr};

static int eventIDBase;
}  // namespace Unity

inline auto                  logLevel{static_cast<UnityLogType>(-1U)};
inline std::filesystem::path path;

inline void log(const std::string_view msg, const UnityLogType severity) {
    Unity::logInterface->Log(severity, msg.data(), "Conifer - Upscaler native library: 'GfxPluginUpscaler.dll'", 0);
}

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
    FrameGenerate,
    EndFrame
};

inline enum FrameGenerationProvider : uint8_t {
    None,
    FSR,
} frameGenerationProvider = None;

constexpr std::size_t NumBaseImages = 4;
constexpr std::size_t NumImages = 6;
inline bool loadedCorrectly = false;
}  // namespace Plugin