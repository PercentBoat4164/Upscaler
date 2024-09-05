#include "FSR_FrameGenerator.hpp"

ffx::Context FSR_FrameGenerator::swapchainContext {nullptr};
ffx::Context FSR_FrameGenerator::context {nullptr};
FfxApiResource FSR_FrameGenerator::hudlessColorResource {};
FfxApiResource FSR_FrameGenerator::depthResource {};
FfxApiResource FSR_FrameGenerator::motionResource {};
VkFormat FSR_FrameGenerator::backBufferFormat {};
