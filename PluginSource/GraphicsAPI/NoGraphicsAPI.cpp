#include "NoGraphicsAPI.hpp"

NoGraphicsAPI *NoGraphicsAPI::get() {
    static NoGraphicsAPI *noGraphicsAPI{new NoGraphicsAPI};
    return noGraphicsAPI;
}

GraphicsAPI::Type NoGraphicsAPI::getType() {
    return NONE;
}

bool NoGraphicsAPI::useUnityInterfaces(IUnityInterfaces */* unused */) {
    return false;
}
