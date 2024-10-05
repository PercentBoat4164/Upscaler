#include "FSR_FrameGenerator.hpp"

ffx::Context FSR_FrameGenerator::swapchainContext {nullptr};
ffx::Context FSR_FrameGenerator::context {nullptr};
FfxApiResource FSR_FrameGenerator::hudlessColorResource {};
FfxApiResource FSR_FrameGenerator::depthResource {};
FfxApiResource FSR_FrameGenerator::motionResource {};
VkFormat FSR_FrameGenerator::backBufferFormat {};

FSR_FrameGenerator::QueueData FSR_FrameGenerator::asyncCompute{}, FSR_FrameGenerator::present{}, FSR_FrameGenerator::imageAcquire{};
bool FSR_FrameGenerator::supported;
bool FSR_FrameGenerator::asyncComputeSupported;
