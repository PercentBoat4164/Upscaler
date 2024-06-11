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

Upscaler::Status (XeSS::*XeSS::fpInitialize)(){&XeSS::safeFail};
Upscaler::Status (XeSS::*XeSS::fpCreate)(){&XeSS::safeFail};
Upscaler::Status (XeSS::*XeSS::fpEvaluate)(){&XeSS::safeFail};

Upscaler::SupportState XeSS::supported{UNTESTED};

#    ifdef ENABLE_DX12
Upscaler::Status XeSS::DX12Initialize() {
    return setStatus(xessD3D12CreateContext(DX12::getGraphicsInterface()->GetDevice(), &context), "Failed to create the " + getName() + " context.");
}

Upscaler::Status XeSS::DX12Create() {
    const xess_d3d12_init_params_t params {
        .outputResolution = {.x = settings.outputResolution.width, .y = settings.outputResolution.height},
        .qualitySetting = settings.getQuality<XESS>(),
        .initFlags =
            static_cast<uint32_t>(XESS_INIT_FLAG_INVERTED_DEPTH) |
            static_cast<uint32_t>(XESS_INIT_FLAG_ENABLE_AUTOEXPOSURE) |
            (settings.hdr ? 0U : XESS_INIT_FLAG_LDR_INPUT_COLOR),
    };
    RETURN_ON_FAILURE(setStatus(xessD3D12Init(context, &params), "Failed to initialize the " + getName() + " context."));
    return SUCCESS;
}

Upscaler::Status XeSS::DX12Evaluate() {
    ID3D12Resource*           inColor            = DX12::getGraphicsInterface()->TextureFromNativeTexture(textureIDs[Plugin::ImageID::SourceColor]);
    const D3D12_RESOURCE_DESC inColorDescription = inColor->GetDesc();
    ID3D12Resource*           motion             = DX12::getGraphicsInterface()->TextureFromNativeTexture(textureIDs[Plugin::ImageID::Motion]);
    const D3D12_RESOURCE_DESC motionDescription = motion->GetDesc();

    const xess_d3d12_execute_params_t params {
        .pColorTexture = inColor,
        .pVelocityTexture = motion,
        .pDepthTexture = DX12::getGraphicsInterface()->TextureFromNativeTexture(textureIDs[Plugin::ImageID::Depth]),
        .pOutputTexture = DX12::getGraphicsInterface()->TextureFromNativeTexture(textureIDs[Plugin::ImageID::OutputColor]),
        .jitterOffsetX = settings.jitter.x,
        .jitterOffsetY = settings.jitter.y,
        .exposureScale = 1.0F,
        .resetHistory = static_cast<uint32_t>(settings.resetHistory),
        .inputWidth = static_cast<uint32_t>(inColorDescription.Width),
        .inputHeight = static_cast<uint32_t>(inColorDescription.Height),
    };
    UnityGraphicsD3D12RecordingState state{};
    RETURN_ON_FAILURE(setStatusIf(!DX12::getGraphicsInterface()->CommandRecordingState(&state), SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, "Unable to obtain a command recording state from Unity. This is fatal."));
    RETURN_ON_FAILURE(setStatus(xessSetVelocityScale(context, -static_cast<float>(motionDescription.Width), -static_cast<float>(motionDescription.Height)), "Failed to set motion scale"));
    RETURN_ON_FAILURE(setStatus(xessD3D12Execute(context, state.commandList, &params), "Failed to execute " + getName() + "."));
    return SUCCESS;
}
#endif

Upscaler::Status XeSS::setStatus(const xess_result_t t_error, const std::string& t_msg) {
    switch (t_error) {
        case XESS_RESULT_WARNING_NONEXISTING_FOLDER: return Upscaler::setStatus(SUCCESS, t_msg + " | XESS_RESULT_WARNING_NONEXISTING_FOLDER");
        case XESS_RESULT_WARNING_OLD_DRIVER: return Upscaler::setStatus(SUCCESS, t_msg + " | XESS_RESULT_WARNING_OLD_DRIVER");
        case XESS_RESULT_SUCCESS: return Upscaler::setStatus(SUCCESS, t_msg + " | XESS_RESULT_SUCCESS");
        case XESS_RESULT_ERROR_UNSUPPORTED_DEVICE: return Upscaler::setStatus(HARDWARE_ERROR_DEVICE_NOT_SUPPORTED, t_msg + " | XESS_RESULT_ERROR_UNSUPPORTED_DEVICE");
        case XESS_RESULT_ERROR_UNSUPPORTED_DRIVER: return Upscaler::setStatus(SOFTWARE_ERROR_DEVICE_DRIVERS_OUT_OF_DATE, t_msg + " | XESS_RESULT_ERROR_UNSUPPORTED_DRIVER");
        case XESS_RESULT_ERROR_UNINITIALIZED: return Upscaler::setStatus(SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, t_msg + " | XESS_RESULT_ERROR_UNINITIALIZED");
        case XESS_RESULT_ERROR_INVALID_ARGUMENT: return Upscaler::setStatus(SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, t_msg + " | XESS_RESULT_ERROR_INVALID_ARGUMENT");
        case XESS_RESULT_ERROR_DEVICE_OUT_OF_MEMORY: return Upscaler::setStatus(SOFTWARE_ERROR_OUT_OF_GPU_MEMORY, t_msg + " | XESS_RESULT_ERROR_DEVICE_OUT_OF_MEMORY");
        case XESS_RESULT_ERROR_DEVICE: return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | XESS_RESULT_ERROR_DEVICE");
        case XESS_RESULT_ERROR_NOT_IMPLEMENTED: return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | XESS_RESULT_ERROR_NOT_IMPLEMENTED");
        case XESS_RESULT_ERROR_INVALID_CONTEXT: return Upscaler::setStatus(SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, t_msg + " | XESS_RESULT_ERROR_INVALID_CONTEXT");
        case XESS_RESULT_ERROR_OPERATION_IN_PROGRESS: return Upscaler::setStatus(SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, t_msg + " | XESS_RESULT_ERROR_OPERATION_IN_PROGRESS");
        case XESS_RESULT_ERROR_UNSUPPORTED: return Upscaler::setStatus(SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, t_msg + " | XESS_RESULT_ERROR_UNSUPPORTED");
        case XESS_RESULT_ERROR_CANT_LOAD_LIBRARY: return Upscaler::setStatus(SOFTWARE_ERROR_CRITICAL_INTERNAL_ERROR, t_msg + " | XESS_RESULT_ERROR_CANT_LOAD_LIBRARY");
        case XESS_RESULT_ERROR_UNKNOWN: return Upscaler::setStatus(UNKNOWN_ERROR, t_msg + " | XESS_RESULT_ERROR_UNKNOWN");
        default: return Upscaler::setStatus(UNKNOWN_ERROR, t_msg + " | Unknown");
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
    if (supported != UNTESTED)
        return supported == SUPPORTED;
    const XeSS xess(GraphicsAPI::getType());
    return (supported = success(xess.getStatus()) ? SUPPORTED : UNSUPPORTED) == SUPPORTED;
}

bool XeSS::isSupported(const enum Settings::Quality mode){
  return mode == Settings::Auto || mode == Settings::AntiAliasing || mode == Settings::UltraQualityPlus || mode == Settings::UltraQuality || mode == Settings::Quality || mode == Settings::Balanced || mode == Settings::Performance || mode == Settings::UltraPerformance;
}

XeSS::XeSS(const GraphicsAPI::Type type) {
    switch (type) {
        case GraphicsAPI::NONE: {
            fpInitialize = &XeSS::safeFail;
            fpCreate     = &XeSS::safeFail;
            fpEvaluate   = &XeSS::safeFail;
            break;
        }
#    ifdef ENABLE_VULKAN
        case GraphicsAPI::VULKAN: {
            fpInitialize = &XeSS::invalidGraphicsAPIFail;
            fpCreate     = &XeSS::invalidGraphicsAPIFail;
            fpEvaluate   = &XeSS::invalidGraphicsAPIFail;
            break;
        }
#    endif
#    ifdef ENABLE_DX12
        case GraphicsAPI::DX12: {
            fpInitialize = &XeSS::DX12Initialize;
            fpCreate     = &XeSS::DX12Create;
            fpEvaluate   = &XeSS::DX12Evaluate;
            break;
        }
#    endif
#    ifdef ENABLE_DX11
        case GraphicsAPI::DX11: {
            fpInitialize = &XeSS::invalidGraphicsAPIFail;
            fpCreate     = &XeSS::invalidGraphicsAPIFail;
            fpEvaluate   = &XeSS::invalidGraphicsAPIFail;
            break;
        }
#    endif
        default: {
            fpInitialize = &XeSS::safeFail;
            fpCreate     = &XeSS::safeFail;
            fpEvaluate   = &XeSS::safeFail;
            break;
        }
    }
    initialize();
}

XeSS::~XeSS() {
    shutdown();
}

Upscaler::Status XeSS::getOptimalSettings(const Settings::Resolution resolution, Settings::Preset /*unused*/, const enum Settings::Quality mode, const bool hdr) {
    settings.outputResolution              = resolution;
    settings.quality                       = mode;
    settings.hdr                           = hdr;
    const xess_2d_t params {
        .x = resolution.width,
        .y = resolution.height
    };
    xess_2d_t optimal, min, max;
    RETURN_ON_FAILURE(setStatus(xessGetOptimalInputResolution(context, &params, settings.getQuality<XESS>(), &optimal, &min, &max), "Failed to get dynamic resolution parameters."));
    settings.renderingResolution = {optimal.x, optimal.y};
    settings.dynamicMinimumInputResolution = {min.x, min.y};
    settings.dynamicMaximumInputResolution = {max.x, max.y};
    return SUCCESS;
}

Upscaler::Status XeSS::initialize() {
    if (!resetStatus()) return getStatus();
    RETURN_ON_FAILURE((this->*fpInitialize)());
#    ifndef NDEBUG
    RETURN_ON_FAILURE(setStatus(xessSetLoggingCallback(context, XESS_LOGGING_LEVEL_DEBUG, &XeSS::log), "Failed to set logging callback."));
#    else
    RETURN_ON_FAILURE(setStatus(xessSetLoggingCallback(context, XESS_LOGGING_LEVEL_INFO, &XeSS::log), "Failed to set logging callback."));
#    endif
    return SUCCESS;
}

Upscaler::Status XeSS::create() {
    RETURN_ON_FAILURE(setStatusIf(context == nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, getName() + " context does not exist."));
    RETURN_ON_FAILURE((this->*fpCreate)());
    return SUCCESS;
}

Upscaler::Status XeSS::evaluate() {
    RETURN_ON_FAILURE(setStatusIf(context == nullptr, SOFTWARE_ERROR_RECOVERABLE_INTERNAL_WARNING, getName() + " context does not exist."));
    RETURN_ON_FAILURE((this->*fpEvaluate)());
    settings.resetHistory = false;
    return SUCCESS;
}

Upscaler::Status XeSS::shutdown() {
    if (context == nullptr) return SUCCESS;
    RETURN_ON_FAILURE(setStatus(xessDestroyContext(context), "Failed to destroy the " + getName() + " context."));
    context = nullptr;
    return SUCCESS;
}
#endif