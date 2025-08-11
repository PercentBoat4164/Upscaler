#pragma once
#include "Upscaler/Upscaler.hpp"

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

inline void log(const UnityLogType type, const std::string_view msg) {
    if (type == kUnityLogTypeLog) return;
    Unity::logInterface->Log(type, msg.data(), "Upscaler native library: 'GfxPluginUpscaler.dll'", 0);
}

inline void log(const Upscaler::Status status, const std::string_view msg) {
    switch (status) {
        case Upscaler::Success: log(kUnityLogTypeLog, msg); break;
        case Upscaler::DeviceNotSupported: log(kUnityLogTypeError, msg); break;
        case Upscaler::DriversOutOfDate: log(kUnityLogTypeWarning, msg); break;
        case Upscaler::UnsupportedGraphicsApi: log(kUnityLogTypeError, msg); break;
        case Upscaler::OperatingSystemNotSupported: log(kUnityLogTypeError, msg); break;
        case Upscaler::FeatureDenied: log(kUnityLogTypeError, msg); break;
        case Upscaler::OutOfMemory: log(kUnityLogTypeError, msg); break;
        case Upscaler::LibraryNotLoaded: log(kUnityLogTypeError, msg); break;
        case Upscaler::RecoverableRuntimeError: log(kUnityLogTypeWarning, msg); break;
        case Upscaler::FatalRuntimeError: log(kUnityLogTypeError, msg); break;
    }
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
    Generate
};

inline enum FrameGenerationProvider : uint8_t {
    None,
    FSR,
} frameGenerationProvider = None;

inline bool loadedCorrectly = false;
}  // namespace Plugin