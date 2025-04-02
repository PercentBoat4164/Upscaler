#ifdef ENABLE_FRAME_GENERATION
#    include "FrameGenerator/FrameGenerator.hpp"
#    ifdef ENABLE_FSR
#        include "FrameGenerator/FSR_FrameGenerator.hpp"
#    endif
#endif
#ifdef ENABLE_VULKAN
#    include "GraphicsAPI/Vulkan.hpp"
#endif
#include "LatencyFleX.hpp"
#include "Plugin.hpp"
#include "Upscaler/Upscaler.hpp"

#include "IUnityRenderingExtensions.h"
#include "IUnityShaderCompilerAccess.h"

#include <memory>
#include <vector>
#include <chrono>
#include <unordered_set>

// Use 'handle SIGXCPU SIGPWR SIG35 SIG36 SIG37 nostop noprint' to prevent Unity's signals with GDB on Linux.
// Use 'pro hand -p true -s false SIGXCPU SIGPWR' for LLDB on Linux.

static std::vector<std::unique_ptr<Upscaler>> upscalers = {};
static std::vector<std::unique_ptr<lfx::LatencyFleX>> latencyReducers = {};

struct alignas(128) UpscaleData {
    float frameTime;
    float sharpness;
    float reactiveValue;
    float reactiveScale;
    float reactiveThreshold;
    uint16_t camera;
    float viewToClip[16];
    float clipToView[16];
    float clipToPrevClip[16];
    float prevClipToClip[16];
    float farPlane;
    float nearPlane;
    float verticalFOV;
    float position[3];
    float up[3];
    float right[3];
    float forward[3];
    float jitter[2];
    int inputResolution[2];
    unsigned options;
};

struct alignas(64) FrameGenerateData {
    bool enable;
    float rect[4];
    float renderSize[2];
    float jitter[2];
    float frameTime;
    float farPlane;
    float nearPlane;
    float verticalFOV;
    unsigned index;
    unsigned options;
};

struct alignas(8) EndFrameData {
    unsigned long frameId;
    uint16_t camera;
};

#define exp7           10000000i64     //1E+7     //C-file part
#define exp9         1000000000i64     //1E+9
#define w2ux 116444736000000000i64     //1.jan1601 to 1.jan1970

void unix_time(struct timespec* spec) {
    __int64 wintime;
    GetSystemTimePreciseAsFileTime(reinterpret_cast<FILETIME*>(&wintime));
    wintime -= w2ux;
    spec->tv_sec  = wintime / exp7;
    spec->tv_nsec = wintime % exp7 * 100;
}

int clock_gettime(int, timespec* spec) {
    static struct timespec startspec;
    static double          ticks2nano;
    static __int64         startticks, tps = 0;
    __int64                tmp, curticks;
    QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&tmp));
    if (tps != tmp) {
        tps = tmp;
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&startticks));
        unix_time(&startspec);
        ticks2nano = static_cast<double>(exp9) / tps;
    }
    QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&curticks));
    curticks -= startticks;
    spec->tv_sec  = startspec.tv_sec + curticks / tps;
    spec->tv_nsec = startspec.tv_nsec + static_cast<double>(curticks % tps) * ticks2nano;
    if (spec->tv_nsec >= exp9) {
        spec->tv_sec++;
        spec->tv_nsec -= exp9;
    }
    return 0;
}

uint64_t getTime() {
    timespec spec{};
    (void)clock_gettime(0, &spec);
    return spec.tv_nsec + spec.tv_sec * 1000000000U;
}

void UNITY_INTERFACE_API UpscaleCallback(const int event, void* d) {
    if (d == nullptr) return;
    switch (event - Plugin::Unity::eventIDBase) {
        case Plugin::Upscale: {
            const auto& data              = *static_cast<UpscaleData*>(d);
            Upscaler&   upscaler          = *upscalers[data.camera];
            upscaler.settings.farPlane    = data.farPlane;
            upscaler.settings.nearPlane   = data.nearPlane;
            upscaler.settings.verticalFOV = data.verticalFOV;
            std::ranges::copy(data.viewToClip,     upscaler.settings.viewToClip.begin());
            std::ranges::copy(data.clipToView,     upscaler.settings.clipToView.begin());
            std::ranges::copy(data.clipToPrevClip, upscaler.settings.clipToPrevClip.begin());
            std::ranges::copy(data.prevClipToClip, upscaler.settings.prevClipToClip.begin());
            std::ranges::copy(data.position,       upscaler.settings.position.begin());
            std::ranges::copy(data.up,             upscaler.settings.up.begin());
            std::ranges::copy(data.right,          upscaler.settings.right.begin());
            std::ranges::copy(data.forward,        upscaler.settings.forward.begin());
            upscaler.settings.frameTime         = data.frameTime;
            upscaler.settings.sharpness         = data.sharpness;
            upscaler.settings.reactiveValue     = data.reactiveValue;
            upscaler.settings.reactiveScale     = data.reactiveScale;
            upscaler.settings.reactiveThreshold = data.reactiveThreshold;
            upscaler.settings.orthographic      = (data.options & 0x1U) != 0U;
            upscaler.settings.debugView         = (data.options & 0x2U) != 0U;
            upscaler.settings.resetHistory      = (data.options & 0x4U) != 0U;
            std::construct_at(&upscaler.settings.jitter, data.jitter[0], data.jitter[1]);
            upscaler.evaluate({static_cast<uint32_t>(data.inputResolution[0]), static_cast<uint32_t>(data.inputResolution[1])});
            break;
        }
#ifdef ENABLE_FRAME_GENERATION
        case Plugin::FrameGenerate: {
            const auto& data = *static_cast<FrameGenerateData*>(d);
#    ifdef ENABLE_FSR
            FSR_FrameGenerator::evaluate(
                data.enable,
                FfxApiRect2D{static_cast<int32_t>(data.rect[0]), static_cast<int32_t>(data.rect[1]), static_cast<int32_t>(data.rect[2]), static_cast<int32_t>(data.rect[3])},
                FfxApiFloatCoords2D{data.renderSize[0], data.renderSize[1]},
                FfxApiFloatCoords2D{data.jitter[0], data.jitter[1]},
                data.frameTime,
                data.farPlane,
                data.nearPlane,
                data.verticalFOV,
                data.index,
                data.options
            );
#    endif
            break;
        }
#endif
        case Plugin::EndFrame: {
            const auto& data = *static_cast<EndFrameData*>(d);
            lfx::LatencyFleX& lowLatency = *latencyReducers[data.camera];
            uint64_t latency{};
            uint64_t frameTime{};
            lowLatency.EndFrame(data.frameId, getTime(), &latency, &frameTime);
        }
        default: break;
    }
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API LoadedCorrectly() {
    return Plugin::loadedCorrectly;
}

#ifdef ENABLE_FRAME_GENERATION
extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SetFrameGeneration(HWND hWnd) {
    if (hWnd == nullptr) Plugin::frameGenerationProvider = Plugin::None;
    else Plugin::frameGenerationProvider = Plugin::FSR;
#ifdef ENABLE_VULKAN
    Vulkan::setFrameGenerationHWND(hWnd);
#endif
}
#endif

extern "C" UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API GetEventIDBase() {
    return Plugin::Unity::eventIDBase;
}

extern "C" UNITY_INTERFACE_EXPORT UnityRenderingEventAndData UNITY_INTERFACE_API GetRenderingEventCallback() {
    return UpscaleCallback;
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API IsUpscalerSupported(const Upscaler::Type type) {
    return Upscaler::isSupported(type);
}

extern "C" UNITY_INTERFACE_EXPORT bool UNITY_INTERFACE_API IsQualitySupported(const Upscaler::Type type, const enum Upscaler::Settings::Quality mode) {
    return Upscaler::isSupported(type, mode);
}

extern "C" UNITY_INTERFACE_EXPORT uint16_t UNITY_INTERFACE_API RegisterCameraUpscaler() {
    const auto     iter = std::ranges::find_if(upscalers, [](const std::unique_ptr<Upscaler>& upscaler) { return !upscaler; });
    const uint16_t id = std::distance(upscalers.begin(), iter);
    if (iter == upscalers.end()) upscalers.push_back(Upscaler::fromType(Upscaler::NONE));
    else *iter = Upscaler::fromType(Upscaler::NONE);
    return id;
}

extern "C" UNITY_INTERFACE_EXPORT uint16_t UNITY_INTERFACE_API RegisterCameraLatencyReducer() {
    const auto     iter = std::ranges::find_if(latencyReducers, [](const std::unique_ptr<lfx::LatencyFleX>& latencyReducer) { return !latencyReducer; });
    const uint16_t id = std::distance(latencyReducers.begin(), iter);
    if (iter == latencyReducers.end()) latencyReducers.emplace_back(std::make_unique<lfx::LatencyFleX>());
    else *iter = std::make_unique<lfx::LatencyFleX>();
    return id;
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API GetCameraUpscalerStatus(const uint16_t camera) {
    return upscalers[camera]->getStatus();
}

extern "C" UNITY_INTERFACE_EXPORT const char* UNITY_INTERFACE_API GetCameraUpscalerStatusMessage(const uint16_t camera) {
    return upscalers[camera]->getErrorMessage().c_str();
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API SetCameraUpscalerStatus(const uint16_t camera, Upscaler::Status status, const char* message) {
    return upscalers[camera]->setStatus(status, message);
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Status UNITY_INTERFACE_API SetCameraPerFeatureSettings(
  const uint16_t                         camera,
  const Upscaler::Settings::Resolution   resolution,
  const Upscaler::Type                   type,
  const Upscaler::Settings::DLSSPreset   preset,
  const enum Upscaler::Settings::Quality quality,
  const bool                             hdr
) {
    std::unique_ptr<Upscaler>& upscaler = upscalers[camera];
    if (upscaler->getType() != type) upscaler = std::move(Upscaler::fromType(type));
    else upscaler->resetStatus();
    return upscaler->useSettings(resolution, preset, quality, hdr);
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Settings::Resolution UNITY_INTERFACE_API GetRecommendedCameraResolution(const uint16_t camera) {
    return upscalers[camera]->settings.recommendedInputResolution;
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Settings::Resolution UNITY_INTERFACE_API GetMaximumCameraResolution(const uint16_t camera) {
    return upscalers[camera]->settings.dynamicMaximumInputResolution;
}

extern "C" UNITY_INTERFACE_EXPORT Upscaler::Settings::Resolution UNITY_INTERFACE_API GetMinimumCameraResolution(const uint16_t camera) {
    return upscalers[camera]->settings.dynamicMinimumInputResolution;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SetUpscalingImages(const uint16_t camera, void* color, void* depth, void* motion, void* output, void* reactive, void* opaque, const bool autoReactive) {
    Upscaler& upscaler = *upscalers[camera];
    upscaler.settings.autoReactive = autoReactive;
    upscaler.useImages({color, depth, motion, output, reactive, opaque});
}

#ifdef ENABLE_FRAME_GENERATION
extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SetFrameGenerationImages(void* color0, void* color1, void* depth, void* motion) {
#ifdef ENABLE_FSR
    FSR_FrameGenerator::useImages(static_cast<VkImage>(color0), static_cast<VkImage>(color1), static_cast<VkImage>(depth), static_cast<VkImage>(motion));
#endif
}

extern "C" UNITY_INTERFACE_EXPORT UnityRenderingExtTextureFormat UNITY_INTERFACE_API GetBackBufferFormat() {
    return FrameGenerator::getBackBufferFormat();
}
#endif

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API BeginFrame(const uint16_t camera, const uint64_t frameId) {
    lfx::LatencyFleX& lowLatency = *latencyReducers[camera];
    const uint64_t target = lowLatency.GetWaitTarget(frameId);
    uint64_t timestamp = getTime();
    if (timestamp < target) {
        std::this_thread::sleep_for(std::chrono::nanoseconds(target - timestamp));
        timestamp = target;
    }
    lowLatency.BeginFrame(frameId, target, timestamp);
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API EndFrame(const uint16_t camera, const uint64_t frameId) {
    latencyReducers[camera]->EndFrame(frameId, getTime(), nullptr, nullptr);
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnregisterCameraUpscaler(const uint16_t camera) {
    if (upscalers.size() > camera) upscalers[camera].reset();
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnregisterCameraLatencyReducer(const uint16_t camera) {
    if (latencyReducers.size() > camera) latencyReducers[camera].reset();
}

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(const UnityGfxDeviceEventType eventType) {
    switch (eventType) {
        case kUnityGfxDeviceEventInitialize:
            Plugin::loadedCorrectly = true;
            GraphicsAPI::initialize(Plugin::Unity::graphicsInterface->GetRenderer());
            break;
        case kUnityGfxDeviceEventShutdown:
            upscalers.clear();
            GraphicsAPI::shutdown();
            break;
        default: break;
    }
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces) {
    GraphicsAPI::registerUnityInterfaces(unityInterfaces);
    Plugin::Unity::interfaces        = unityInterfaces;
    Plugin::Unity::logInterface      = unityInterfaces->Get<IUnityLog>();
    Plugin::Unity::graphicsInterface = unityInterfaces->Get<IUnityGraphics>();
    Plugin::Unity::graphicsInterface->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
    Plugin::Unity::eventIDBase = Plugin::Unity::graphicsInterface->ReserveEventIDRange(1);
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginUnload() {
    GraphicsAPI::unregisterUnityInterfaces();
    Plugin::Unity::graphicsInterface->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
    Plugin::Unity::interfaces        = nullptr;
    Plugin::Unity::graphicsInterface = nullptr;
}

extern "C" BOOL WINAPI DllMain(HINSTANCE dllInstance, const DWORD reason, LPVOID reserved) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    char path[MAX_PATH + 1];
    GetModuleFileName(dllInstance, path, std::extent_v<decltype(path)>);
    Plugin::path = std::filesystem::path(path).parent_path();
    return TRUE;
}