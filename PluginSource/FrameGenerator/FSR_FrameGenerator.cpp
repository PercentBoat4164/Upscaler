#include "FSR_FrameGenerator.hpp"

HMODULE FSR_FrameGenerator::library{nullptr};

ffxContext FSR_FrameGenerator::swapchainContext {nullptr};
ffxContext FSR_FrameGenerator::context {nullptr};
std::array<FfxApiResource, 2> FSR_FrameGenerator::hudlessColorResource {};
FfxApiResource FSR_FrameGenerator::depthResource {};
FfxApiResource FSR_FrameGenerator::motionResource {};

FSR_FrameGenerator::QueueData FSR_FrameGenerator::asyncCompute{}, FSR_FrameGenerator::present{}, FSR_FrameGenerator::imageAcquire{};
bool FSR_FrameGenerator::supported{false};
bool FSR_FrameGenerator::asyncComputeSupported{false};
FSR_FrameGenerator::CallbackContext FSR_FrameGenerator::callbackContext{&FSR_FrameGenerator::context, false};

PfnFfxCreateContext FSR_FrameGenerator::ffxCreateContext{nullptr};
PfnFfxDestroyContext FSR_FrameGenerator::ffxDestroyContext{nullptr};
PfnFfxConfigure FSR_FrameGenerator::ffxConfigure{nullptr};
PfnFfxQuery FSR_FrameGenerator::ffxQuery{nullptr};
PfnFfxDispatch FSR_FrameGenerator::ffxDispatch{nullptr};