#pragma once

#include "Plugin.hpp"

namespace Upscaler {
class Upscaler {
public:
    Upscaler() = default;
    Upscaler(const Upscaler &) = delete;
    Upscaler(Upscaler &&) = default;
    Upscaler &operator =(const Upscaler &) = delete;
    Upscaler &operator =(Upscaler &&) = default;

    template<typename T>
    requires std::derived_from<T, Upscaler>
    static Upscaler *get() {
        return T::get();
    }

    virtual bool setIsSupported(bool) = 0;

    virtual ~Upscaler() = default;
};
}  // namespace Upscaler
