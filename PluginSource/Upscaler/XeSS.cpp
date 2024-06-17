#include "XeSS.hpp"
#ifdef ENABLE_XESS
#    include <xess/xess.h>
#    ifdef ENABLE_DX12
#        include "GraphicsAPI/DX12.hpp"

#        include <d3d12compatibility.h>
#        include <d3dx12.h>

#        include <IUnityGraphicsD3D12.h>

#        include <xess/xess_d3d12.h>
#    endif

Upscaler::Status (XeSS::*XeSS::fpCreate)(const xess_d3d12_init_params_t*){&XeSS::safeFail};
Upscaler::Status (XeSS::*XeSS::fpEvaluate)(){&XeSS::safeFail};

Upscaler::SupportState XeSS::supported{Untested};

#    ifdef ENABLE_DX12
Upscaler::Status XeSS::DX12Create(const xess_d3d12_init_params_t* params) {
    RETURN_ON_FAILURE(setStatus(xessD3D12CreateContext(DX12::getGraphicsInterface()->GetDevice(), &context), "Failed to create the " + getName() + " context."));
    return setStatus(xessD3D12Init(context, params), "Failed to initialize the " + getName() + " context.");
}

Upscaler::Status XeSS::DX12Evaluate() {
    ID3D12Resource*           color              = DX12::getGraphicsInterface()->TextureFromNativeTexture(textureIDs[Plugin::ImageID::SourceColor]);
    const D3D12_RESOURCE_DESC colorDescription   = color->GetDesc();
    const xess_d3d12_execute_params_t params {
        .pColorTexture = color,
        .pVelocityTexture = DX12::getGraphicsInterface()->TextureFromNativeTexture(textureIDs[Plugin::ImageID::Motion]),
        .pDepthTexture = DX12::getGraphicsInterface()->TextureFromNativeTexture(textureIDs[Plugin::ImageID::Depth]),
        .pOutputTexture = DX12::getGraphicsInterface()->TextureFromNativeTexture(textureIDs[Plugin::ImageID::OutputColor]),
        .jitterOffsetX = settings.jitter.x,
        .jitterOffsetY = settings.jitter.y,
        .exposureScale = 1.0F,
        .resetHistory = static_cast<uint32_t>(settings.resetHistory),
        .inputWidth = static_cast<uint32_t>(colorDescription.Width),
        .inputHeight = static_cast<uint32_t>(colorDescription.Height),
    };
    UnityGraphicsD3D12RecordingState state{};
    RETURN_ON_FAILURE(setStatusIf(!DX12::getGraphicsInterface()->CommandRecordingState(&state), FatalRuntimeError, "Unable to obtain a command recording state from Unity. This is fatal."));
    RETURN_ON_FAILURE(setStatus(xessSetVelocityScale(context, -static_cast<float>(colorDescription.Width), -static_cast<float>(colorDescription.Height)), "Failed to set motion scale"));
    return setStatus(xessD3D12Execute(context, state.commandList, &params), "Failed to execute " + getName() + ".");
}
#endif

Upscaler::Status XeSS::setStatus(const xess_result_t t_error, const std::string& t_msg) {
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


void XeSS::log(const char* message, const xess_logging_level_t loggingLevel) {
    std::string msg;
    switch (loggingLevel) {
        case XESS_LOGGING_LEVEL_DEBUG: msg = "XeSS Debug ---> "; break;
        case XESS_LOGGING_LEVEL_INFO: msg = "XeSS Info ----> "; break;
        case XESS_LOGGING_LEVEL_WARNING: msg = "XeSS Warning -> "; break;
        case XESS_LOGGING_LEVEL_ERROR: msg = "XeSS Error ---> "; break;
    }
    if (logCallback != nullptr) logCallback((msg + message).c_str());
}

bool XeSS::isSupported() {
    if (supported != Untested) return supported == Supported;
    XeSS xess(GraphicsAPI::getType());
    return (supported = success(xess.useSettings({32, 32}, Settings::DLSSPreset::Default, Settings::Quality::Auto, false)) ? Supported : Unsupported) == Supported;
}

bool XeSS::isSupported(const enum Settings::Quality mode){
  return mode == Settings::Auto || mode == Settings::AntiAliasing || mode == Settings::UltraQualityPlus || mode == Settings::UltraQuality || mode == Settings::Quality || mode == Settings::Balanced || mode == Settings::Performance || mode == Settings::UltraPerformance;
}

XeSS::XeSS(const GraphicsAPI::Type type) {
    switch (type) {
        case GraphicsAPI::NONE: {
            fpCreate     = &XeSS::invalidGraphicsAPIFail;
            fpEvaluate   = &XeSS::invalidGraphicsAPIFail;
            break;
        }
#    ifdef ENABLE_VULKAN
        case GraphicsAPI::VULKAN: {
            fpCreate     = &XeSS::invalidGraphicsAPIFail;
            fpEvaluate   = &XeSS::invalidGraphicsAPIFail;
            break;
        }
#    endif
#    ifdef ENABLE_DX12
        case GraphicsAPI::DX12: {
            fpCreate     = &XeSS::DX12Create;
            fpEvaluate   = &XeSS::DX12Evaluate;
            break;
        }
#    endif
#    ifdef ENABLE_DX11
        case GraphicsAPI::DX11:
#    endif
        default: {
            fpCreate     = &XeSS::invalidGraphicsAPIFail;
            fpEvaluate   = &XeSS::invalidGraphicsAPIFail;
            break;
        }
    }
}

XeSS::~XeSS() {
    if (context != nullptr) setStatus(xessDestroyContext(context), "Failed to destroy the " + getName() + " context.");
    context = nullptr;
}

Upscaler::Status XeSS::useSettings(Settings::Resolution resolution, Settings::DLSSPreset /*unused*/, enum Settings::Quality mode, bool hdr) {
    RETURN_ON_FAILURE(getStatus());
    Settings optimalSettings;
    optimalSettings.outputResolution              = resolution;
    optimalSettings.quality                       = mode;
    optimalSettings.hdr                           = hdr;
    const xess_d3d12_init_params_t params {
      .outputResolution = {.x = optimalSettings.outputResolution.width, .y = optimalSettings.outputResolution.height},
      .qualitySetting = optimalSettings.getQuality<XESS>(),
      .initFlags =
        static_cast<uint32_t>(XESS_INIT_FLAG_INVERTED_DEPTH) |
        static_cast<uint32_t>(XESS_INIT_FLAG_ENABLE_AUTOEXPOSURE) |
        (optimalSettings.hdr ? 0U : XESS_INIT_FLAG_LDR_INPUT_COLOR),
    };
    if (Upscaler::failure((this->*fpCreate)(&params))) {
        Status status = setStatus(xessDestroyContext(context), "Failed to destroy XeSS context that failed to create.");
        context = nullptr;
        return status;
    }
#    ifndef NDEBUG
    if (Upscaler::failure(setStatus(xessSetLoggingCallback(context, XESS_LOGGING_LEVEL_DEBUG, &XeSS::log), "Failed to set logging callback."))) {
        Status status = setStatus(xessDestroyContext(context), "Failed to destroy XeSS context that failed to create.");
        context = nullptr;
        return status;
    }
#    else
    if (Upscaler::failure(setStatus(xessSetLoggingCallback(context, XESS_LOGGING_LEVEL_INFO, &XeSS::log), "Failed to set logging callback."))) {
        Status status = setStatus(xessDestroyContext(context), "Failed to destroy XeSS context that failed to create.");
        context = nullptr;
        return status;
    }
#    endif
    const xess_2d_t inputResolution{resolution.width, resolution.height};
    xess_2d_t optimal, min, max;
    RETURN_ON_FAILURE(setStatus(xessGetOptimalInputResolution(context, &inputResolution, optimalSettings.getQuality<XESS>(), &optimal, &min, &max), "Failed to get dynamic resolution parameters."));
    optimalSettings.recommendedInputResolution    = {optimal.x, optimal.y};
    optimalSettings.dynamicMinimumInputResolution = {min.x, min.y};
    optimalSettings.dynamicMaximumInputResolution = {max.x, max.y};
    settings = optimalSettings;
    return Success;
}

Upscaler::Status XeSS::evaluate() {
    RETURN_ON_FAILURE((this->*fpEvaluate)());
    settings.resetHistory = false;
    return Success;
}
#endif