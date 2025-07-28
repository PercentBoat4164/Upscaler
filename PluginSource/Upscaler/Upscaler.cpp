#include "Upscaler.hpp"

#include "DLSS_Upscaler.hpp"
#include "FSR_Upscaler.hpp"
#include "XeSS_UPscaler.hpp"

#include "GraphicsAPI/GraphicsAPI.hpp"

void Upscaler::load(const GraphicsAPI::Type type, void* vkGetProcAddrFunc) {
#    ifdef ENABLE_DLSS
    DLSS_Upscaler::load(type, vkGetProcAddrFunc);
#    endif
#    ifdef ENABLE_FSR
    FSR_Upscaler::load(type, nullptr);
#    endif
#    ifdef ENABLE_XESS
    XeSS_Upscaler::load(type, nullptr);
#    endif
}

void Upscaler::unload() {
#    ifdef ENABLE_DLSS
    DLSS_Upscaler::unload();
#    endif
#    ifdef ENABLE_FSR
    FSR_Upscaler::unload();
#    endif
#    ifdef ENABLE_XESS
    XeSS_Upscaler::unload();
#    endif
}

void Upscaler::useGraphicsAPI(const GraphicsAPI::Type type) {
#ifdef ENABLE_DLSS
    DLSS_Upscaler::useGraphicsAPI(type);
#endif
#ifdef ENABLE_FSR
    FSR_Upscaler::useGraphicsAPI(type);
#endif
#ifdef ENABLE_XESS
    XeSS_Upscaler::useGraphicsAPI(type);
#endif
}