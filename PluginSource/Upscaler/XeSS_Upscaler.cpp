#ifdef ENABLE_XESS
#    include "XeSS_Upscaler.hpp"

#    include <xess/xess.h>
#    ifdef ENABLE_DX12
#        include "GraphicsAPI/DX12.hpp"

#        include <d3d12compatibility.h>

#        include <IUnityGraphicsD3D12.h>

#        include <xess/xess_d3d12.h>
#    endif

HMODULE XeSS_Upscaler::library{nullptr};
Upscaler::SupportState XeSS_Upscaler::supported{Untested};

Upscaler::Status (XeSS_Upscaler::*XeSS_Upscaler::fpCreate)(const xess_d3d12_init_params_t*){&XeSS_Upscaler::safeFail};
Upscaler::Status (XeSS_Upscaler::*XeSS_Upscaler::fpEvaluate)(const Settings::Resolution){&XeSS_Upscaler::safeFail};

decltype(&xessGetOptimalInputResolution) XeSS_Upscaler::xessGetOptimalInputResolution{nullptr};
decltype(&xessDestroyContext) XeSS_Upscaler::xessDestroyContext{nullptr};
decltype(&xessSetVelocityScale) XeSS_Upscaler::xessSetVelocityScale{nullptr};
decltype(&xessSetLoggingCallback) XeSS_Upscaler::xessSetLoggingCallback{nullptr};
decltype(&xessD3D12CreateContext) XeSS_Upscaler::xessD3D12CreateContext{nullptr};
decltype(&xessD3D12BuildPipelines) XeSS_Upscaler::xessD3D12BuildPipelines{nullptr};
decltype(&xessD3D12Init) XeSS_Upscaler::xessD3D12Init{nullptr};
decltype(&xessD3D12Execute) XeSS_Upscaler::xessD3D12Execute{nullptr};

Upscaler::Status XeSS_Upscaler::setStatus(const xess_result_t t_error, const std::string& t_msg) {
    switch (t_error) {
        case XESS_RESULT_WARNING_NONEXISTING_FOLDER: return Upscaler::setStatus(Success, t_msg + " | XESS_RESULT_WARNING_NONEXISTING_FOLDER");
        case XESS_RESULT_WARNING_OLD_DRIVER: return Upscaler::setStatus(Success, t_msg + " | XESS_RESULT_WARNING_OLD_DRIVER");
        case XESS_RESULT_SUCCESS: return Upscaler::setStatus(Success, t_msg + " | XESS_RESULT_SUCCESS");
        case XESS_RESULT_ERROR_UNSUPPORTED_DEVICE: return Upscaler::setStatus(DeviceNotSupported, t_msg + " | XESS_RESULT_ERROR_UNSUPPORTED_DEVICE");
        case XESS_RESULT_ERROR_UNSUPPORTED_DRIVER: return Upscaler::setStatus(DriversOutOfDate, t_msg + " | XESS_RESULT_ERROR_UNSUPPORTED_DRIVER");
        case XESS_RESULT_ERROR_UNINITIALIZED: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | XESS_RESULT_ERROR_UNINITIALIZED");
        case XESS_RESULT_ERROR_INVALID_ARGUMENT: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | XESS_RESULT_ERROR_INVALID_ARGUMENT");
        case XESS_RESULT_ERROR_DEVICE_OUT_OF_MEMORY: return Upscaler::setStatus(OutOfMemory, t_msg + " | XESS_RESULT_ERROR_DEVICE_OUT_OF_MEMORY");
        case XESS_RESULT_ERROR_DEVICE: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | XESS_RESULT_ERROR_DEVICE");
        case XESS_RESULT_ERROR_NOT_IMPLEMENTED: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | XESS_RESULT_ERROR_NOT_IMPLEMENTED");
        case XESS_RESULT_ERROR_INVALID_CONTEXT: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | XESS_RESULT_ERROR_INVALID_CONTEXT");
        case XESS_RESULT_ERROR_OPERATION_IN_PROGRESS: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | XESS_RESULT_ERROR_OPERATION_IN_PROGRESS");
        case XESS_RESULT_ERROR_UNSUPPORTED: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | XESS_RESULT_ERROR_UNSUPPORTED");
        case XESS_RESULT_ERROR_CANT_LOAD_LIBRARY: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | XESS_RESULT_ERROR_CANT_LOAD_LIBRARY");
        case XESS_RESULT_ERROR_UNKNOWN: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | XESS_RESULT_ERROR_UNKNOWN");
        default: return Upscaler::setStatus(FatalRuntimeError, t_msg + " | Unknown");
    }
}


void XeSS_Upscaler::log(const char* msg, const xess_logging_level_t loggingLevel) {
    UnityLogType unityType = kUnityLogTypeLog;
    switch (loggingLevel) {
        case XESS_LOGGING_LEVEL_DEBUG:
        case XESS_LOGGING_LEVEL_INFO: unityType = kUnityLogTypeLog; break;
        case XESS_LOGGING_LEVEL_WARNING: unityType = kUnityLogTypeWarning; break;
        case XESS_LOGGING_LEVEL_ERROR: unityType = kUnityLogTypeError; break;
        default: break;
    }
    Plugin::log(msg, unityType);
}

#    ifdef ENABLE_DX12
Upscaler::Status XeSS_Upscaler::DX12Create(const xess_d3d12_init_params_t* params) {
    RETURN_ON_FAILURE(setStatus(xessD3D12CreateContext(DX12::getGraphicsInterface()->GetDevice(), &context), "Failed to create the Intel Xe Super Sampling context."));
    RETURN_ON_FAILURE(setStatus(xessD3D12BuildPipelines(context, params->pPipelineLibrary, true, params->initFlags), "Failed to build Xe Super Sampling pipelines."));
    return setStatus(xessD3D12Init(context, params), "Failed to initialize the Intel Xe Super Sampling context.");
}

Upscaler::Status XeSS_Upscaler::DX12Evaluate(const Settings::Resolution inputResolution) {
    const D3D12_RESOURCE_DESC motionDescription = static_cast<ID3D12Resource*>(resources.at(Plugin::Motion))->GetDesc();
    const xess_d3d12_execute_params_t params {
        .pColorTexture = static_cast<ID3D12Resource*>(resources.at(Plugin::Color)),
        .pVelocityTexture = static_cast<ID3D12Resource*>(resources.at(Plugin::Motion)),
        .pDepthTexture = static_cast<ID3D12Resource*>(resources.at(Plugin::Depth)),
        .pOutputTexture = static_cast<ID3D12Resource*>(resources.at(Plugin::Output)),
        .jitterOffsetX = settings.jitter.x - 0.5F,
        .jitterOffsetY = settings.jitter.y - 0.5F,
        .exposureScale = 1.0F,
        .resetHistory = static_cast<uint32_t>(settings.resetHistory),
        .inputWidth = static_cast<uint32_t>(inputResolution.width),
        .inputHeight = static_cast<uint32_t>(inputResolution.height),
    };
    UnityGraphicsD3D12RecordingState state{};
    RETURN_ON_FAILURE(setStatusIf(!DX12::getGraphicsInterface()->CommandRecordingState(&state), FatalRuntimeError, "Unable to obtain a command recording state from Unity. This is fatal."));
    RETURN_ON_FAILURE(setStatus(xessSetVelocityScale(context, -static_cast<float>(motionDescription.Width), -static_cast<float>(motionDescription.Height)), "Failed to set motion scale."));
    return setStatus(xessD3D12Execute(context, state.commandList, &params), "Failed to execute Intel Xe Super Sampling.");
}
#endif

void XeSS_Upscaler::load(const GraphicsAPI::Type type, void* /*unused*/) {
    if (type != GraphicsAPI::DX12) return (void)(supported = Unsupported);
    library = LoadLibrary((Plugin::path / "libxess.dll").string().c_str());
    if (library == nullptr) return (void)(supported = Unsupported);
    xessGetOptimalInputResolution = reinterpret_cast<decltype(xessGetOptimalInputResolution)>(GetProcAddress(library, "xessGetOptimalInputResolution"));
    xessDestroyContext            = reinterpret_cast<decltype(xessDestroyContext)>(GetProcAddress(library, "xessDestroyContext"));
    xessSetVelocityScale          = reinterpret_cast<decltype(xessSetVelocityScale)>(GetProcAddress(library, "xessSetVelocityScale"));
    xessSetLoggingCallback        = reinterpret_cast<decltype(xessSetLoggingCallback)>(GetProcAddress(library, "xessSetLoggingCallback"));
    xessD3D12CreateContext        = reinterpret_cast<decltype(xessD3D12CreateContext)>(GetProcAddress(library, "xessD3D12CreateContext"));
    xessD3D12BuildPipelines        = reinterpret_cast<decltype(xessD3D12BuildPipelines)>(GetProcAddress(library, "xessD3D12BuildPipelines"));
    xessD3D12Init                 = reinterpret_cast<decltype(xessD3D12Init)>(GetProcAddress(library, "xessD3D12Init"));
    xessD3D12Execute              = reinterpret_cast<decltype(xessD3D12Execute)>(GetProcAddress(library, "xessD3D12Execute"));
    if (xessGetOptimalInputResolution == nullptr || xessDestroyContext == nullptr || xessSetVelocityScale == nullptr || xessSetLoggingCallback == nullptr || xessD3D12CreateContext == nullptr || xessD3D12Init == nullptr || xessD3D12Execute == nullptr) supported = Unsupported;
}

void XeSS_Upscaler::unload() {
    xessGetOptimalInputResolution = nullptr;
    xessDestroyContext = nullptr;
    xessSetVelocityScale = nullptr;
    xessSetLoggingCallback = nullptr;
    xessD3D12CreateContext = nullptr;
    xessD3D12BuildPipelines = nullptr;
    xessD3D12Init = nullptr;
    xessD3D12Execute = nullptr;
    if (library != nullptr) FreeLibrary(library);
    library = nullptr;
}

bool XeSS_Upscaler::isSupported() {
    if (supported != Untested) return supported == Supported;
    return (supported = success(XeSS_Upscaler().useSettings({32, 32}, Settings::DLSSPreset::Default, Settings::Quality::Auto, false)) ? Supported : Unsupported) == Supported;
}

bool XeSS_Upscaler::isSupported(const enum Settings::Quality mode) {
    return mode == Settings::Auto || mode == Settings::AntiAliasing || mode == Settings::UltraQualityPlus || mode == Settings::UltraQuality || mode == Settings::Quality || mode == Settings::Balanced || mode == Settings::Performance || mode == Settings::UltraPerformance;
}

void XeSS_Upscaler::useGraphicsAPI(const GraphicsAPI::Type type) {
    switch (type) {
#    ifdef ENABLE_DX12
        case GraphicsAPI::DX12: {
            fpCreate   = &XeSS_Upscaler::DX12Create;
            fpEvaluate = &XeSS_Upscaler::DX12Evaluate;
            break;
        }
#    endif
        default: {
            fpCreate   = &XeSS_Upscaler::invalidGraphicsAPIFail;
            fpEvaluate = &XeSS_Upscaler::invalidGraphicsAPIFail;
            break;
        }
    }
}

XeSS_Upscaler::~XeSS_Upscaler() {
    if (context != nullptr) setStatus(xessDestroyContext(context), "Failed to destroy the Intel Xe Super Sampling context.");
    context = nullptr;
}

Upscaler::Status XeSS_Upscaler::useSettings(const Settings::Resolution resolution, const Settings::DLSSPreset /*unused*/, const enum Settings::Quality mode, const bool hdr) {
    RETURN_ON_FAILURE(getStatus());
    Settings optimalSettings;
    optimalSettings.outputResolution = resolution;
    optimalSettings.hdr              = hdr;

    const xess_d3d12_init_params_t params{
      .outputResolution = {.x = optimalSettings.outputResolution.width, .y = optimalSettings.outputResolution.height},
      .qualitySetting   = optimalSettings.getQuality<XESS>(mode),
      .initFlags =
        static_cast<uint32_t>(XESS_INIT_FLAG_ENABLE_AUTOEXPOSURE) |
        static_cast<uint32_t>(XESS_INIT_FLAG_INVERTED_DEPTH) |
        static_cast<uint32_t>(XESS_INIT_FLAG_HIGH_RES_MV) |
        (optimalSettings.hdr ? 0U : XESS_INIT_FLAG_LDR_INPUT_COLOR)
    };
    if (context != nullptr) RETURN_ON_FAILURE(setStatus(xessDestroyContext(context), "Failed to destroy the Intel Xe Super Sampling context."));
    context = nullptr;
    RETURN_ON_FAILURE((this->*fpCreate)(&params));
#ifndef NDEBUG
    RETURN_ON_FAILURE(setStatus(xessSetLoggingCallback(context, XESS_LOGGING_LEVEL_DEBUG, &XeSS_Upscaler::log), "Failed to set logging callback."));
#endif
    const xess_2d_t inputResolution {resolution.width, resolution.height};
    xess_2d_t       optimal, min, max;
    RETURN_ON_FAILURE(setStatus(xessGetOptimalInputResolution(context, &inputResolution, params.qualitySetting, &optimal, &min, &max), "Failed to get dynamic resolution parameters."));
    optimalSettings.recommendedInputResolution    = Settings::Resolution {optimal.x, optimal.y};
    optimalSettings.dynamicMinimumInputResolution = Settings::Resolution {min.x, min.y};
    optimalSettings.dynamicMaximumInputResolution = Settings::Resolution {max.x, max.y};
    settings                                      = optimalSettings;
    return Success;
}

Upscaler::Status XeSS_Upscaler::useImages(const std::array<void*, Plugin::NumImages>& images) {
    std::copy_n(images.begin(), resources.size(), resources.begin());
    for (void*& resource : resources) RETURN_ON_FAILURE(setStatusIf(resource == nullptr, RecoverableRuntimeError, "Unity provided a `nullptr` image."));
    return Success;
}

Upscaler::Status XeSS_Upscaler::evaluate(const Settings::Resolution inputResolution) {
    return (this->*fpEvaluate)(inputResolution);
}
#endif