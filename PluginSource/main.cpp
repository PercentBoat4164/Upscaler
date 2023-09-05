#include "IUnityGraphics.h"
#include "IUnityGraphicsVulkan.h"
#include "IUnityInterface.h"

// DLSS requires the vulkan imports from Unity
#include "nvsdk_ngx.h"
#include "nvsdk_ngx_defs.h"
#include "nvsdk_ngx_helpers.h"
#include "nvsdk_ngx_helpers_vk.h"
#include "nvsdk_ngx_params.h"
#include "nvsdk_ngx_vk.h"

#include <cstring>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>

#ifndef NDEBUG
// Usage: Insert this where the debugger should connect. Execute. Connect the debugger. Set 'debuggerConnected' to
// true. Step.
// Use 'handle SIGXCPU SIGPWR nostop noprint' to prevent Unity's signals.
#    define WAIT_FOR_DEBUGGER           \
        bool debuggerConnected = false; \
        while (!debuggerConnected)      \
            ;
#else
#    define WAIT_FOR_DEBUGGER
#endif

namespace Logger {
void (*Info)(const char *) = nullptr;
std::vector<std::string> messages;

std::string to_string(const wchar_t *wcstr) {
    auto       s                 = std::mbstate_t();
    const auto target_char_count = std::wcsrtombs(nullptr, &wcstr, 0, &s);
    if (target_char_count == static_cast<std::size_t>(-1)) throw std::logic_error("Illegal byte sequence");

    auto str = std::string(target_char_count, '\0');
    std::wcsrtombs(str.data(), &wcstr, str.size() + 1, &s);
    return str;
}

void flush() {
    for (const std::string &message : messages) Info(message.c_str());
    messages.clear();
}

void log(std::string t_message) {
    if (t_message.back() == '\n') t_message.erase(t_message.length() - 1, 1);
    if (Info == nullptr) messages.push_back(t_message);
    else Info(t_message.c_str());
}

void log(const std::string &t_actionDescription, NVSDK_NGX_Result t_result) {
    if (NVSDK_NGX_SUCCEED(t_result))
        return log("DLSSPlugin: " + t_actionDescription + ": Succeeded");
    log("DLSSPlugin: " + t_actionDescription + ": Failed | " + to_string(GetNGXResultAsString(t_result)) + ".");
}

void log(const char *t_message, NVSDK_NGX_Logging_Level t_loggingLevel, NVSDK_NGX_Feature t_sourceComponent) {
    log(t_message);
}
}  // namespace Logger

namespace Application {
uint64_t                         id{231313132};
std::wstring                     dataPath{L"./Logs"};
NVSDK_NGX_Application_Identifier ngxIdentifier{
  .IdentifierType = NVSDK_NGX_Application_Identifier_Type_Application_Id,
  .v              = {
                     .ApplicationId = Application::id,
                     }
};
NVSDK_NGX_FeatureCommonInfo featureCommonInfo{
  .PathListInfo =
    {
                   .Path   = new const wchar_t *{L"./Assets/Plugins"},
                   .Length = 1,
                   },
  .InternalData = nullptr,
  .LoggingInfo  = {
                   .LoggingCallback          = Logger::log,
                   .MinimumLoggingLevel      = NVSDK_NGX_LOGGING_LEVEL_VERBOSE,
                   .DisableOtherLoggingSinks = false,
                   }
};
NVSDK_NGX_FeatureDiscoveryInfo featureDiscoveryInfo{
  .SDKVersion          = NVSDK_NGX_Version_API,
  .FeatureID           = NVSDK_NGX_Feature_SuperSampling,
  .Identifier          = Application::ngxIdentifier,
  .ApplicationDataPath = Application::dataPath.c_str(),
  .FeatureInfo         = &Application::featureCommonInfo,
};
}  // namespace Application

namespace Unity {
IUnityInterfaces       *interfaces;
IUnityGraphics         *graphicsInterface;
IUnityGraphicsVulkanV2 *vulkanGraphicsInterface;
VkInstance              vulkanInstance;

namespace Hooks {
// Loaded before initialization
PFN_vkGetInstanceProcAddr    vkGetInstanceProcAddr;
PFN_vkCreateInstance         vkCreateInstance;
PFN_vkCreateDevice           vkCreateDevice;
// Loaded after initialization
PFN_vkCreateImageView        vkCreateImageView;
PFN_vkCreateCommandPool      vkCreateCommandPool;
PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
PFN_vkBeginCommandBuffer     vkBeginCommandBuffer;
PFN_vkEndCommandBuffer       vkEndCommandBuffer;
PFN_vkQueueSubmit            vkQueueSubmit;
PFN_vkQueueWaitIdle          vkQueueWaitIdle;
PFN_vkResetCommandBuffer     vkResetCommandBuffer;
PFN_vkFreeCommandBuffers     vkFreeCommandBuffers;
PFN_vkDestroyCommandPool     vkDestroyCommandPool;

void loadVulkanFunctionHooks(VkInstance instance, VkDevice device) {
    // @todo Get device functions using the device rather than the instance to avoid indirection on every call.
    vkCreateImageView =
      reinterpret_cast<PFN_vkCreateImageView>(vkGetInstanceProcAddr(instance, "vkCreateImageView"));
    vkCreateCommandPool =
      reinterpret_cast<PFN_vkCreateCommandPool>(vkGetInstanceProcAddr(instance, "vkCreateCommandPool"));
    vkAllocateCommandBuffers =
      reinterpret_cast<PFN_vkAllocateCommandBuffers>(vkGetInstanceProcAddr(instance, "vkAllocateCommandBuffers"));
    vkBeginCommandBuffer =
      reinterpret_cast<PFN_vkBeginCommandBuffer>(vkGetInstanceProcAddr(instance, "vkBeginCommandBuffer"));
    vkEndCommandBuffer =
      reinterpret_cast<PFN_vkEndCommandBuffer>(vkGetInstanceProcAddr(instance, "vkEndCommandBuffer"));
    vkQueueSubmit   = reinterpret_cast<PFN_vkQueueSubmit>(vkGetInstanceProcAddr(instance, "vkQueueSubmit"));
    vkQueueWaitIdle = reinterpret_cast<PFN_vkQueueWaitIdle>(vkGetInstanceProcAddr(instance, "vkQueueWaitIdle"));
    vkResetCommandBuffer =
      reinterpret_cast<PFN_vkResetCommandBuffer>(vkGetInstanceProcAddr(instance, "vkResetCommandBuffer"));
    vkFreeCommandBuffers =
      reinterpret_cast<PFN_vkFreeCommandBuffers>(vkGetInstanceProcAddr(instance, "vkFreeCommandBuffers"));
    vkDestroyCommandPool =
      reinterpret_cast<PFN_vkDestroyCommandPool>(vkGetInstanceProcAddr(instance, "vkDestroyCommandPool"));
}
}  // namespace Hooks
}  // namespace Unity

namespace Plugin {
bool                          DLSSSupported{true};
NVSDK_NGX_Handle             *DLSS{nullptr};
NVSDK_NGX_Parameter          *parameters{nullptr};
NVSDK_NGX_VK_DLSS_Eval_Params evalParameters;
NVSDK_NGX_Resource_VK         depthBufferResource;
VkCommandPool                 _oneTimeSubmitCommandPool;
VkCommandBuffer               _oneTimeSubmitCommandBuffer;
bool                          _oneTimeSubmitRecording{false};

void prepareForOneTimeSubmits() {
    if (_oneTimeSubmitRecording) return;
    UnityVulkanInstance vulkan = Unity::vulkanGraphicsInterface->Instance();

    VkCommandPoolCreateInfo createInfo{
      .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext            = nullptr,
      .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
      .queueFamilyIndex = vulkan.queueFamilyIndex,
    };

    Unity::Hooks::vkCreateCommandPool(vulkan.device, &createInfo, nullptr, &_oneTimeSubmitCommandPool);

    VkCommandBufferAllocateInfo allocateInfo{
      .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext              = nullptr,
      .commandPool        = _oneTimeSubmitCommandPool,
      .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 0x1,
    };

    Unity::Hooks::vkAllocateCommandBuffers(vulkan.device, &allocateInfo, &_oneTimeSubmitCommandBuffer);
    _oneTimeSubmitRecording = true;
}

VkCommandBuffer beginOneTimeSubmitRecording() {
    VkCommandBufferBeginInfo beginInfo{
      .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext            = nullptr,
      .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = nullptr,
    };

    Unity::Hooks::vkBeginCommandBuffer(_oneTimeSubmitCommandBuffer, &beginInfo);

    return _oneTimeSubmitCommandBuffer;
}

void endOneTimeSubmitRecording() {
    if (!_oneTimeSubmitRecording) return;

    UnityVulkanInstance vulkan = Unity::vulkanGraphicsInterface->Instance();

    Unity::Hooks::vkEndCommandBuffer(_oneTimeSubmitCommandBuffer);

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

    Unity::Hooks::vkQueueSubmit(vulkan.graphicsQueue, 1, &submitInfo, nullptr);
    Unity::Hooks::vkQueueWaitIdle(vulkan.graphicsQueue);
    Unity::Hooks::vkResetCommandBuffer(_oneTimeSubmitCommandBuffer, 0x0);
    _oneTimeSubmitRecording = false;
}

void cancelOneTimeSubmitRecording() {
    if (!_oneTimeSubmitRecording) return;
    Unity::Hooks::vkResetCommandBuffer(_oneTimeSubmitCommandBuffer, 0x0);
    _oneTimeSubmitRecording = false;
}

void finishOneTimeSubmits() {
    UnityVulkanInstance vulkan = Unity::vulkanGraphicsInterface->Instance();

    Unity::Hooks::vkFreeCommandBuffers(vulkan.device, _oneTimeSubmitCommandPool, 1, &_oneTimeSubmitCommandBuffer);
    Unity::Hooks::vkDestroyCommandPool(vulkan.device, _oneTimeSubmitCommandPool, nullptr);
}

namespace Settings {
struct Resolution {
    unsigned int width;
    unsigned int height;
};

Resolution                  renderResolution;
Resolution                  dynamicMaximumRenderResolution;
Resolution                  dynamicMinimumRenderResolution;
Resolution                  presentResolution;
float                       sharpness;
bool                        lowResolutionMotionVectors{true};
bool                        HDR;
bool                        invertedDepth;
bool                        DLSSSharpening;
bool                        DLSSAutoExposure;
NVSDK_NGX_PerfQuality_Value DLSSQuality{NVSDK_NGX_PerfQuality_Value_Balanced};

void setPresentResolution(Resolution t_presentResolution) {
    presentResolution = t_presentResolution;
    renderResolution  = {presentResolution.width / 2, presentResolution.height / 2};
}

NVSDK_NGX_DLSS_Create_Params getDLSSCreateParams() {
    return {
      .Feature =
        {
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
}

void useOptimalSettings() {
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
}  // namespace Settings
}  // namespace Plugin

extern "C" UNITY_INTERFACE_EXPORT void __cdecl OnFramebufferResize(unsigned int t_width, unsigned int t_height) {
    Logger::log("Resizing DLSS targets: " + std::to_string(t_width) + "x" + std::to_string(t_height));

    // Release any previously existing feature
    if (Plugin::DLSS != nullptr) {
        NVSDK_NGX_VULKAN_ReleaseFeature(Plugin::DLSS);
        Plugin::DLSS = nullptr;
    }

    Plugin::Settings::setPresentResolution({t_width, t_height});
    Plugin::Settings::useOptimalSettings();
    NVSDK_NGX_DLSS_Create_Params DLSSCreateParams = Plugin::Settings::getDLSSCreateParams();

    NVSDK_NGX_Parameter_SetUI(Plugin::parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_DLAA, 1);
    NVSDK_NGX_Parameter_SetUI(Plugin::parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Quality, 1);
    NVSDK_NGX_Parameter_SetUI(Plugin::parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Balanced, 1);
    NVSDK_NGX_Parameter_SetUI(Plugin::parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_Performance, 1);
    NVSDK_NGX_Parameter_SetUI(Plugin::parameters, NVSDK_NGX_Parameter_DLSS_Hint_Render_Preset_UltraPerformance, 1);

    Unity::vulkanGraphicsInterface->EnsureOutsideRenderPass();

    VkCommandBuffer  commandBuffer = Plugin::beginOneTimeSubmitRecording();
    NVSDK_NGX_Result result =
      NGX_VULKAN_CREATE_DLSS_EXT(commandBuffer, 1, 1, &Plugin::DLSS, Plugin::parameters, &DLSSCreateParams);
    Logger::log("Create DLSS feature", result);
    if (NVSDK_NGX_FAILED(result)) {
        Plugin::cancelOneTimeSubmitRecording();
        return;
    }
    Plugin::endOneTimeSubmitRecording();
}

extern "C" UNITY_INTERFACE_EXPORT void PrepareDLSS(VkImage t_depthBuffer) {
    VkImageView imageView{nullptr};

    VkImageViewCreateInfo createInfo{
      .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext    = nullptr,
      .flags    = 0,
      .image    = t_depthBuffer,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format   = VK_FORMAT_D24_UNORM_S8_UINT,
      .components = {
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
      },
      .subresourceRange = {
        .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
      },
    };

    VkResult result = Unity::Hooks::vkCreateImageView(
      Unity::vulkanGraphicsInterface->Instance().device,
      &createInfo,
      nullptr,
      &imageView
    );
    if (result == VK_SUCCESS)
        Logger::log("Created depth resource for DLSS.");


    Plugin::depthBufferResource = {
      .Resource = {
        .ImageViewInfo = {
          .ImageView        = imageView,
          .Image            = t_depthBuffer,
          .SubresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
          },
          .Format = VK_FORMAT_D24_UNORM_S8_UINT,
          .Width  = Plugin::Settings::renderResolution.width,
          .Height = Plugin::Settings::renderResolution.height,
        },
      },
      .Type      = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW,
      .ReadWrite = true,
    };

    Plugin::evalParameters = {
      .pInDepth                  = &Plugin::depthBufferResource,
      .pInMotionVectors          = nullptr,
      .InJitterOffsetX           = 0.F,
      .InJitterOffsetY           = 0.F,
      .InRenderSubrectDimensions = {
        .Width  = Plugin::Settings::renderResolution.width,
        .Height = Plugin::Settings::renderResolution.height,
      },
    };
}

extern "C" UNITY_INTERFACE_EXPORT void EvaluateDLSS() {
    UnityVulkanRecordingState state{};
    Unity::vulkanGraphicsInterface->EnsureInsideRenderPass;
    Unity::vulkanGraphicsInterface->CommandRecordingState(&state, kUnityVulkanGraphicsQueueAccess_DontCare);

    NVSDK_NGX_Result result =
      NGX_VULKAN_EVALUATE_DLSS_EXT(state.commandBuffer, Plugin::DLSS, Plugin::parameters, &Plugin::evalParameters);
    Logger::log("Evaluated DLSS feature", result);
}

extern "C" UNITY_INTERFACE_EXPORT bool initializeDLSS() {
    if (!Plugin::DLSSSupported) return false;

    UnityVulkanInstance vulkan = Unity::vulkanGraphicsInterface->Instance();

    Unity::Hooks::loadVulkanFunctionHooks(vulkan.instance, vulkan.device);
    Plugin::prepareForOneTimeSubmits();

    // Initialize NGX SDK
    NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_Init(
      Application::id,
      Application::dataPath.c_str(),
      vulkan.instance,
      vulkan.physicalDevice,
      vulkan.device,
      Unity::Hooks::vkGetInstanceProcAddr,
      nullptr,
      &Application::featureCommonInfo,
      NVSDK_NGX_Version_API
    );
    Logger::log("Initialize NGX SDK", result);

    // Ensure that the device that Unity selected supports DLSS
    // Set and obtain parameters
    result = NVSDK_NGX_VULKAN_GetCapabilityParameters(&Plugin::parameters);
    Logger::log("Get NGX Vulkan capability parameters", result);
    if (NVSDK_NGX_FAILED(result)) return false;
    // Check for DLSS support
    // Is driver up-to-date
    int              needsUpdatedDriver{};
    int              requiredMajorDriverVersion{};
    int              requiredMinorDriverVersion{};
    NVSDK_NGX_Result updateDriverResult =
      Plugin::parameters->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsUpdatedDriver);
    Logger::log("Query DLSS graphics driver requirements", updateDriverResult);
    NVSDK_NGX_Result minMajorDriverVersionResult = Plugin::parameters->Get(
      NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor,
      &requiredMajorDriverVersion
    );
    Logger::log("Query DLSS minimum graphics driver major version", minMajorDriverVersionResult);
    NVSDK_NGX_Result minMinorDriverVersionResult = Plugin::parameters->Get(
      NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor,
      &requiredMinorDriverVersion
    );
    Logger::log("Query DLSS minimum graphics driver minor version", minMinorDriverVersionResult);
    if (NVSDK_NGX_FAILED(updateDriverResult) || NVSDK_NGX_FAILED(minMajorDriverVersionResult) || NVSDK_NGX_FAILED(minMinorDriverVersionResult))
        return false;
    if (needsUpdatedDriver != 0) {
        Logger::log(
          "DLSS initialization failed. Minimum driver requirement not met. Update to at least: " +
          std::to_string(requiredMajorDriverVersion) + "." + std::to_string(requiredMinorDriverVersion)
        );
        return false;
    }
    Logger::log(
      "Graphics driver version is greater than DLSS' required minimum version (" +
      std::to_string(requiredMajorDriverVersion) + "." + std::to_string(requiredMinorDriverVersion) + ")."
    );
    // Is DLSS available on this hardware and platform
    int DLSSSupported{};
    result = Plugin::parameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &DLSSSupported);
    Logger::log("Query DLSS feature availability", result);
    if (NVSDK_NGX_FAILED(result)) return false;
    if (DLSSSupported == 0) {
        NVSDK_NGX_Result FeatureInitResult = NVSDK_NGX_Result_Fail;
        NVSDK_NGX_Parameter_GetI(
          Plugin::parameters,
          NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult,
          reinterpret_cast<int *>(&FeatureInitResult)
        );
        std::stringstream stream;
        stream << "DLSSPlugin: DLSS is not available on this hardware or platform. FeatureInitResult = 0x" << std::setfill('0')
               << std::setw(sizeof(FeatureInitResult) * 2) << std::hex << FeatureInitResult
               << ", info: " << Logger::to_string(GetNGXResultAsString(FeatureInitResult));
        Logger::log(stream.str());
        return false;
    }
    // Is DLSS denied for this application
    result = Plugin::parameters->Get(NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, &DLSSSupported);
    Logger::log("Query DLSS feature initialization", result);
    if (NVSDK_NGX_FAILED(result)) return false;
    // clean up
    if (DLSSSupported == 0) {
        Logger::log("DLSS is denied for this application.");
        return false;
    }

    // DLSS is available.
    Plugin::DLSSSupported = DLSSSupported != 0;
    return Plugin::DLSSSupported;
}

/// Hijacks the vkCreateDevice function that Unity uses to create its Vulkan Device.
VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateDevice(
  VkPhysicalDevice             physicalDevice,
  const VkDeviceCreateInfo    *pCreateInfo,
  const VkAllocationCallbacks *pAllocator,
  VkDevice                    *pDevice
) {
// @todo This is commented out because it is not necessary for success and it fails on Windows.
    // Does this physical device support DLSS?
//#ifdef __linux__
//    NVSDK_NGX_FeatureRequirement featureRequirement;
//    NVSDK_NGX_Result             featureRequirementResult = NVSDK_NGX_VULKAN_GetFeatureRequirements(
//      Unity::vulkanInstance,
//      physicalDevice,
//      &Application::featureDiscoveryInfo,
//      &featureRequirement
//    );
//    Logger::log("Get DLSS feature requirements", featureRequirementResult);
//    if (NVSDK_NGX_FAILED(featureRequirementResult) || featureRequirement.FeatureSupported != NVSDK_NGX_FeatureSupportResult_Supported)
//        Plugin::DLSSSupported = false;
//#endif

    // Continue either way
    VkDeviceCreateInfo createInfo = *pCreateInfo;
    std::string        message;

    // @todo Detect if device extensions requested are supported before creating device.
    // Find out which extensions need to be added.
    uint32_t               extensionCount{};
    VkExtensionProperties *extensions{};
    NVSDK_NGX_Result       queryResult = NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(
      Unity::vulkanInstance,
      physicalDevice,
      &Application::featureDiscoveryInfo,
      &extensionCount,
      &extensions
    );

    std::vector<const char *> enabledExtensions;
    if (NVSDK_NGX_SUCCEED(queryResult)) {
        // Add the extensions that have already been requested to the extensions that need to be added.
        enabledExtensions.reserve(pCreateInfo->enabledExtensionCount + extensionCount);
        for (uint32_t i{0}; i < pCreateInfo->enabledExtensionCount; ++i)
            enabledExtensions.push_back(createInfo.ppEnabledExtensionNames[i]);
        for (uint32_t i{0}; i < extensionCount;) {
            enabledExtensions.push_back(extensions[i].extensionName);
            message += extensions[i].extensionName;
            if (++i < extensionCount) message += ", ";
        }

        // Modify the createInfo.
        createInfo.enabledExtensionCount   = enabledExtensions.size();
        createInfo.ppEnabledExtensionNames = enabledExtensions.data();
    }

    // Create the Device
    VkResult result = Unity::Hooks::vkCreateDevice(physicalDevice, &createInfo, pAllocator, pDevice);
    if (result == VK_SUCCESS) {
        Logger::log("DLSS compatible device creation", queryResult);
        if (NVSDK_NGX_SUCCEED(queryResult))
            Logger::log("Added " + std::to_string(extensionCount) + " device extensions: " + message + ".");
    }
    return result;
}

/// Hijacks the vkCreateInstance function that Unity uses to create its Vulkan Instance.
VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateInstance(
  const VkInstanceCreateInfo  *pCreateInfo,
  const VkAllocationCallbacks *pAllocator,
  VkInstance                  *pInstance
) {
    VkInstanceCreateInfo createInfo = *pCreateInfo;
    std::string          message;

    // @todo Detect if instance extensions requested are supported before creating instance.
    // Find out which extensions need to be added.
    uint32_t               extensionCount{};
    VkExtensionProperties *extensions{};
    NVSDK_NGX_Result       queryResult = NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(
      &Application::featureDiscoveryInfo,
      &extensionCount,
      &extensions
    );

    std::vector<const char *> enabledExtensions;
    if (NVSDK_NGX_SUCCEED(queryResult)) {
        // Add the extensions that have already been requested to the extensions that need to be added.
        enabledExtensions.reserve(pCreateInfo->enabledExtensionCount + extensionCount);
        for (uint32_t i{0}; i < pCreateInfo->enabledExtensionCount; ++i)
            enabledExtensions.push_back(createInfo.ppEnabledExtensionNames[i]);
        for (uint32_t i{0}; i < extensionCount;) {
            enabledExtensions.push_back(extensions[i].extensionName);
            message += extensions[i].extensionName;
            if (++i < extensionCount) message += ", ";
        }

        // Modify the createInfo.
        createInfo.enabledExtensionCount   = enabledExtensions.size();
        createInfo.ppEnabledExtensionNames = enabledExtensions.data();
    }

    // Create the Instance.
    VkResult result       = Unity::Hooks::vkCreateInstance(&createInfo, pAllocator, pInstance);
    Unity::vulkanInstance = *pInstance;
    if (result == VK_SUCCESS) {
        Logger::log("DLSS compatible instance creation", queryResult);
        if (NVSDK_NGX_SUCCEED(queryResult))
            Logger::log("Added " + std::to_string(extensionCount) + " instance extensions: " + message + ".");
    }
    return result;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Hook_vkGetInstanceProcAddr(VkInstance instance, const char *pName) {
    if (pName == nullptr) return nullptr;
    if (strcmp(pName, "vkCreateInstance") == 0) {
        Unity::Hooks::vkCreateInstance =
          reinterpret_cast<PFN_vkCreateInstance>(Unity::Hooks::vkGetInstanceProcAddr(instance, pName));
        return reinterpret_cast<PFN_vkVoidFunction>(&Hook_vkCreateInstance);
    }
    if (strcmp(pName, "vkCreateDevice") == 0) {
        Unity::Hooks::vkCreateDevice =
          reinterpret_cast<PFN_vkCreateDevice>(Unity::Hooks::vkGetInstanceProcAddr(instance, pName));
        return reinterpret_cast<PFN_vkVoidFunction>(&Hook_vkCreateDevice);
    }
    return Unity::Hooks::vkGetInstanceProcAddr(instance, pName);
}

PFN_vkGetInstanceProcAddr
interceptInitialization(PFN_vkGetInstanceProcAddr t_getInstanceProcAddr, void * /*unused*/) {
    Unity::Hooks::vkGetInstanceProcAddr = t_getInstanceProcAddr;
    return Hook_vkGetInstanceProcAddr;
}

extern "C" UNITY_INTERFACE_EXPORT void SetDebugCallback(void (*t_debugFunction)(const char *)) {
    Logger::Info = t_debugFunction;
    Logger::flush();
}

extern "C" UNITY_INTERFACE_EXPORT void OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType) {
    switch (eventType) {
        case kUnityGfxDeviceEventInitialize: {
            UnityGfxRenderer renderer = Unity::graphicsInterface->GetRenderer();
            if (renderer == kUnityGfxRendererNull) break;
            if (renderer == kUnityGfxRendererVulkan) Plugin::DLSSSupported = initializeDLSS();
            break;
        }
        case kUnityGfxDeviceEventShutdown: {
            Unity::graphicsInterface->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
        }
        default: break;
    }
}

extern "C" UNITY_INTERFACE_EXPORT bool IsDLSSSupported() {
    return Plugin::DLSSSupported;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces *t_unityInterfaces) {
    Unity::interfaces              = t_unityInterfaces;
    // @todo Add a fallback if IUnityGraphicsVulkanV2 is not available.
    Unity::vulkanGraphicsInterface = t_unityInterfaces->Get<IUnityGraphicsVulkanV2>();
    if (!Unity::vulkanGraphicsInterface->AddInterceptInitialization(interceptInitialization, nullptr, 0)) {
        Logger::log("DLSS Plugin failed to intercept initialization.");
        return;
    }
    Unity::graphicsInterface = t_unityInterfaces->Get<IUnityGraphics>();
    Unity::graphicsInterface->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API UnityPluginUnload() {
    // Finish all one time submits
    Plugin::finishOneTimeSubmits();
    // Clean up
    if (Plugin::parameters != nullptr) NVSDK_NGX_VULKAN_DestroyParameters(Plugin::parameters);
    // Release features
    if (Plugin::DLSS != nullptr) NVSDK_NGX_VULKAN_ReleaseFeature(Plugin::DLSS);
    Plugin::DLSS = nullptr;
    // Shutdown NGX
    NVSDK_NGX_VULKAN_Shutdown1(nullptr);
    // Remove vulkan initialization interception
    Unity::vulkanGraphicsInterface->RemoveInterceptInitialization(interceptInitialization);
    // Remove the device event callback
    Unity::graphicsInterface->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
    // Perform shutdown graphics event
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventShutdown);
}
