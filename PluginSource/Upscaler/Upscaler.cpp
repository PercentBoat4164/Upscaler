#include "Upscaler.hpp"

#include "DLSS.hpp"

Upscaler *Upscaler::upscalerInUse{};

template<typename T>
    requires std::derived_from<T, Upscaler>
Upscaler *Upscaler::get() {
    return T::get();
}

Upscaler *Upscaler::get(Type upscaler) {
    switch (upscaler) {
        case NONE:
            return nullptr;
        case DLSS:
            return get<::DLSS>();
    }
    return nullptr;
}

Upscaler *Upscaler::get() {
    return upscalerInUse;
}

std::vector<Upscaler *> Upscaler::getAllUpscalers() {
    return {
      get<::DLSS>(),
    };
}

std::vector<Upscaler *> Upscaler::getSupportedUpscalers() {
    std::vector<Upscaler *> upscalers;
    for (Upscaler *upscaler : getAllUpscalers())
        if (upscaler->isSupported()) upscalers.push_back(upscaler);
    return upscalers;
}

template<typename T>
    requires std::derived_from<T, Upscaler>
void Upscaler::set() {
    upscalerInUse = T::get();
}

void Upscaler::set(Type upscaler) {
    upscalerInUse = get(upscaler);
}

void Upscaler::set(Upscaler *upscaler) {
    upscalerInUse = upscaler;
}

void Upscaler::disableAllUpscalers() {
    for (Upscaler *upscaler : Upscaler::getAllUpscalers())
        upscaler->setIsSupported(false);
}
