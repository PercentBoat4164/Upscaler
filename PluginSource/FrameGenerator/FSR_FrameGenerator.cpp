#include "FSR_FrameGenerator.hpp"

ffx::Context FSR_FrameGenerator::swapchainContext {nullptr};
ffx::Context FSR_FrameGenerator::context {nullptr};
std::array<FfxApiResource, 2> FSR_FrameGenerator::hudlessColorResource {};
FfxApiResource FSR_FrameGenerator::depthResource {};
FfxApiResource FSR_FrameGenerator::motionResource {};

FSR_FrameGenerator::QueueData FSR_FrameGenerator::asyncCompute{}, FSR_FrameGenerator::present{}, FSR_FrameGenerator::imageAcquire{};
bool FSR_FrameGenerator::supported{false};
bool FSR_FrameGenerator::asyncComputeSupported{false};
