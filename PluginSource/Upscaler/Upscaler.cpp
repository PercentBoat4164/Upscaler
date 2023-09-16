#include "Upscaler.hpp"

#include "DLSS.hpp"
#include "NoUpscaler.hpp"

Upscaler          *Upscaler::upscalerInUse{get<NoUpscaler>()};
Upscaler::Settings Upscaler::settings{};

uint64_t Upscaler::Settings::Resolution::asLong() const {
    return (uint64_t) width << 32U | height;
}

Upscaler *Upscaler::get(Type upscaler) {
    switch (upscaler) {
        case NONE: return get<NoUpscaler>();
#ifdef ENABLE_DLSS
        case DLSS: return get<::DLSS>();
#endif
    }
    return get<NoUpscaler>();
}

Upscaler *Upscaler::get() {
    return upscalerInUse;
}

std::vector<Upscaler *> Upscaler::getAllUpscalers() {
    return {
      get<::NoUpscaler>(),
#ifdef ENABLE_DLSS
      get<::DLSS>(),
#endif
    };
}

std::vector<Upscaler *> Upscaler::getSupportedUpscalers() {
    std::vector<Upscaler *> upscalers;
    for (Upscaler *upscaler : getAllUpscalers())
        if (upscaler->isSupported()) upscalers.push_back(upscaler);
    return upscalers;
}

void Upscaler::set(Type upscaler) {
    set(get(upscaler));
}

void Upscaler::set(Upscaler *upscaler) {
    if (upscaler != nullptr && !upscaler->isSupported()) return;
    if (upscalerInUse != nullptr) upscalerInUse->setAvailable(false);
    upscalerInUse = upscaler;
    upscalerInUse->setAvailable(true);
}

void Upscaler::setGraphicsAPI(GraphicsAPI::Type graphicsAPI) {
    for (Upscaler *upscaler : getAllUpscalers()) upscaler->setFunctionPointers(graphicsAPI);
}

void Upscaler::disableAllUpscalers() {
    for (Upscaler *upscaler : Upscaler::getAllUpscalers()) upscaler->isSupportedAfter(false);
}

void Upscaler::setJitterInformation(float x, float y) {
    thisFrameJitterValues[0] = x;
    thisFrameJitterValues[1] = y;
}
