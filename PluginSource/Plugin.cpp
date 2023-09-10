#include "Plugin.hpp"

#include "GraphicsAPI/Vulkan.hpp"
#include "Upscaler/DLSS.hpp"

NVSDK_NGX_Handle             *Plugin::DLSS{nullptr};
NVSDK_NGX_Parameter          *Plugin::parameters{nullptr};
NVSDK_NGX_VK_DLSS_Eval_Params Plugin::evalParameters{};
NVSDK_NGX_Resource_VK         Plugin::depthBufferResource{};
VkCommandPool                 Plugin::_oneTimeSubmitCommandPool{};
VkCommandBuffer               Plugin::_oneTimeSubmitCommandBuffer{};
bool                          Plugin::_oneTimeSubmitRecording{false};

void Plugin::prepareForOneTimeSubmits() {
    if (_oneTimeSubmitRecording) return;

    UnityVulkanInstance                  vulkanInstance = GraphicsAPI::Vulkan::getVulkanInterface()->Instance();
    GraphicsAPI::Vulkan::DeviceFunctions device         = GraphicsAPI::Vulkan::get(vulkanInstance.device);

    VkCommandPoolCreateInfo createInfo{
      .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext            = nullptr,
      .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
      .queueFamilyIndex = vulkanInstance.queueFamilyIndex,
    };

    device.vkCreateCommandPool(&createInfo, nullptr, &_oneTimeSubmitCommandPool);

    VkCommandBufferAllocateInfo allocateInfo{
      .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext              = nullptr,
      .commandPool        = _oneTimeSubmitCommandPool,
      .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 0x1,
    };

    device.vkAllocateCommandBuffers(&allocateInfo, &_oneTimeSubmitCommandBuffer);
    _oneTimeSubmitRecording = true;
}

VkCommandBuffer Plugin::beginOneTimeSubmitRecording() {
    GraphicsAPI::Vulkan::DeviceFunctions device =
      GraphicsAPI::Vulkan::get(GraphicsAPI::Vulkan::getVulkanInterface()->Instance().device);

    VkCommandBufferBeginInfo beginInfo{
      .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext            = nullptr,
      .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = nullptr,
    };

    device.vkBeginCommandBuffer(_oneTimeSubmitCommandBuffer, &beginInfo);

    return _oneTimeSubmitCommandBuffer;
}

void Plugin::endOneTimeSubmitRecording() {
    if (!_oneTimeSubmitRecording) return;
    UnityVulkanInstance                  vulkanInstance = GraphicsAPI::Vulkan::getVulkanInterface()->Instance();
    GraphicsAPI::Vulkan::DeviceFunctions device         = GraphicsAPI::Vulkan::get(vulkanInstance.device);

    device.vkEndCommandBuffer(_oneTimeSubmitCommandBuffer);

    VkSubmitInfo submitInfo{
      .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext                = nullptr,
      .waitSemaphoreCount   = 0x0,
      .pWaitSemaphores      = nullptr,
      .pWaitDstStageMask    = nullptr,
      .commandBufferCount   = 0x1,
      .pCommandBuffers      = &_oneTimeSubmitCommandBuffer,
      .signalSemaphoreCount = 0x0,
      .pSignalSemaphores    = nullptr,
    };

    device.vkQueueSubmit(vulkanInstance.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    device.vkQueueWaitIdle(vulkanInstance.graphicsQueue);
    device.vkResetCommandBuffer(_oneTimeSubmitCommandBuffer, 0x0);
    _oneTimeSubmitRecording = false;
}

void Plugin::cancelOneTimeSubmitRecording() {
    if (!_oneTimeSubmitRecording) return;
    GraphicsAPI::Vulkan::DeviceFunctions device =
      GraphicsAPI::Vulkan::get(GraphicsAPI::Vulkan::getVulkanInterface()->Instance().device);
    device.vkResetCommandBuffer(_oneTimeSubmitCommandBuffer, 0x0);
    _oneTimeSubmitRecording = false;
}

void Plugin::finishOneTimeSubmits() {
    GraphicsAPI::Vulkan::DeviceFunctions device =
      GraphicsAPI::Vulkan::get(GraphicsAPI::Vulkan::getVulkanInterface()->Instance().device);
    device.vkFreeCommandBuffers(_oneTimeSubmitCommandPool, 1, &_oneTimeSubmitCommandBuffer);
    device.vkDestroyCommandPool(_oneTimeSubmitCommandPool, nullptr);
}

Plugin::Settings::Resolution                  Plugin::Settings::renderResolution{};
Plugin::Settings::Resolution                  Plugin::Settings::dynamicMaximumRenderResolution{};
Plugin::Settings::Resolution                  Plugin::Settings::dynamicMinimumRenderResolution{};
Plugin::Settings::Resolution                  Plugin::Settings::presentResolution{};
float                       Plugin::Settings::sharpness{};
bool                        Plugin::Settings::lowResolutionMotionVectors{true};
bool                        Plugin::Settings::HDR{};
bool                        Plugin::Settings::invertedDepth{};
bool                        Plugin::Settings::DLSSSharpening{};
bool                        Plugin::Settings::DLSSAutoExposure{};
NVSDK_NGX_PerfQuality_Value Plugin::Settings::DLSSQuality{NVSDK_NGX_PerfQuality_Value_Balanced};

void Plugin::Settings::setPresentResolution(Plugin::Settings::Resolution t_presentResolution) {
    presentResolution = t_presentResolution;
    renderResolution  = {presentResolution.width / 2, presentResolution.height / 2};
}

NVSDK_NGX_DLSS_Create_Params Plugin::Settings::getDLSSCreateParams() {
    // clang-format off
    return {
      .Feature = {
        .InWidth            = renderResolution.width,
        .InHeight           = renderResolution.height,
        .InTargetWidth      = presentResolution.width,
        .InTargetHeight     = presentResolution.height,
        .InPerfQualityValue = DLSSQuality,
      },
      .InFeatureCreateFlags = static_cast<int>(
        (lowResolutionMotionVectors ? NVSDK_NGX_DLSS_Feature_Flags_MVLowRes : 0U) |
        (HDR ? NVSDK_NGX_DLSS_Feature_Flags_IsHDR : 0U) |
        (invertedDepth ? NVSDK_NGX_DLSS_Feature_Flags_DepthInverted : 0U) |
        (DLSSSharpening ? NVSDK_NGX_DLSS_Feature_Flags_DoSharpening : 0U) |
        (DLSSAutoExposure ? NVSDK_NGX_DLSS_Feature_Flags_AutoExposure : 0U)
      ),
      .InEnableOutputSubrects = false,
    };
    // clang-format on
}

void Plugin::Settings::useOptimalSettings() {
    NVSDK_NGX_Result result = NGX_DLSS_GET_OPTIMAL_SETTINGS(
      parameters,
      presentResolution.width,
      presentResolution.height,
      DLSSQuality,
      &renderResolution.width,
      &renderResolution.height,
      &dynamicMaximumRenderResolution.width,
      &dynamicMaximumRenderResolution.height,
      &dynamicMinimumRenderResolution.width,
      &dynamicMinimumRenderResolution.height,
      &sharpness
    );
    if (NVSDK_NGX_FAILED(result)) {
        renderResolution               = presentResolution;
        dynamicMaximumRenderResolution = presentResolution;
        dynamicMinimumRenderResolution = presentResolution;
        sharpness                      = 0.F;
        Logger::log("Get optimal DLSS settings: ", result);
    }
}
