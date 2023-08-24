#include "Unity/IUnityGraphics.h"
#include "Unity/IUnityGraphicsVulkan.h"
#include "Unity/IUnityInterface.h"

#include "DLSS/nvsdk_ngx.h"
#include "DLSS/nvsdk_ngx_defs.h"
#include "DLSS/nvsdk_ngx_helpers.h"
#include "DLSS/nvsdk_ngx_helpers_vk.h"
#include "DLSS/nvsdk_ngx_params.h"
#include "DLSS/nvsdk_ngx_vk.h"

#include <cstring>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>

#ifndef NDEBUG
// Usage: Insert this where the debugger should connect. Execute. Connect the debugger. Set 'debuggerConnected' to true. Step.
#define WAIT_FOR_DEBUGGER \
  bool debuggerConnected = false; \
  while (!debuggerConnected);
#else
#define WAIT_FOR_DEBUGGER
#endif

namespace Logger {
void (*Info)(const char *) = nullptr;
std::vector<std::string> messages;

void flush() {
  for (const std::string &message : messages)
    Info(message.c_str());
}

void log(std::string t_message) {
  if (t_message.back() == '\n') t_message.erase(t_message.length() - 1, 1);
  if (!Info) messages.push_back(t_message);
  else Info(t_message.c_str());
}

void log(const std::string& t_actionDescription, NVSDK_NGX_Result t_result) {
  switch (t_result) {
  case NVSDK_NGX_Result_Success:
    log(t_actionDescription + ": Succeeded");
    break;
  case NVSDK_NGX_Result_FAIL_FeatureNotSupported:
    log(t_actionDescription + ": Failed | Feature not supported.");
    break;
  case NVSDK_NGX_Result_FAIL_PlatformError:
    log(t_actionDescription + ": Failed | Platform error.");
    break;
  case NVSDK_NGX_Result_FAIL_FeatureAlreadyExists:
    log(t_actionDescription + ": Failed | Feature already exists");
    break;
  case NVSDK_NGX_Result_FAIL_FeatureNotFound:
    log(t_actionDescription + ": Failed | Feature not found.");
    break;
  case NVSDK_NGX_Result_FAIL_InvalidParameter:
    log(t_actionDescription + ": Failed | Invalid parameter.");
    break;
  case NVSDK_NGX_Result_FAIL_ScratchBufferTooSmall:
    log(t_actionDescription + ": Failed | Scratch buffer too small.");
    break;
  case NVSDK_NGX_Result_FAIL_NotInitialized:
    log(t_actionDescription + ": Failed | Not initialized.");
    break;
  case NVSDK_NGX_Result_FAIL_UnsupportedInputFormat:
    log(t_actionDescription + ": Failed | Unsupported input format.");
    break;
  case NVSDK_NGX_Result_FAIL_RWFlagMissing:
    log(t_actionDescription + ": Failed | RW flag missing.");
    break;
  case NVSDK_NGX_Result_FAIL_MissingInput:
    log(t_actionDescription + ": Failed | Missing input.");
    break;
  case NVSDK_NGX_Result_FAIL_UnableToInitializeFeature:
    log(t_actionDescription + ": Failed | Unable to initialize feature.");
    break;
  case NVSDK_NGX_Result_FAIL_OutOfDate:
    log(t_actionDescription + ": Failed | Out of date.");
    break;
  case NVSDK_NGX_Result_FAIL_OutOfGPUMemory:
    log(t_actionDescription + ": Failed | Out of GPU memory.");
    break;
  case NVSDK_NGX_Result_FAIL_UnsupportedFormat:
    log(t_actionDescription + ": Failed | Unsupported format.");
    break;
  case NVSDK_NGX_Result_FAIL_UnableToWriteToAppDataPath:
    log(t_actionDescription + ": Failed | Unable to write to app data path.");
    break;
  case NVSDK_NGX_Result_FAIL_UnsupportedParameter:
    log(t_actionDescription + ": Failed | Unsupported parameter.");
    break;
  case NVSDK_NGX_Result_FAIL_Denied:
    log(t_actionDescription + ": Failed | Denied.");
    break;
  case NVSDK_NGX_Result_FAIL_NotImplemented:
    log(t_actionDescription + ": Failed | Not implemented.");
    break;

  default:
    log(t_actionDescription + ": Failed | Unknown Error");
  }
}

void log(const char *t_message, NVSDK_NGX_Logging_Level t_loggingLevel, NVSDK_NGX_Feature t_sourceComponent) {
  log(t_message);
}
} // namespace Logger

namespace Application {
uint64_t id{0x0023};
std::wstring dataPath{L"."};
NVSDK_NGX_Application_Identifier ngxIdentifier {
      .IdentifierType = NVSDK_NGX_Application_Identifier_Type_Application_Id,
      .v = {
          .ApplicationId = Application::id,
      }
  };
NVSDK_NGX_FeatureCommonInfo featureCommonInfo {
      .PathListInfo = {
          .Path = nullptr,
          .Length = 0,
      },
      .InternalData = nullptr,
      .LoggingInfo = {
          .LoggingCallback = Logger::log,
          .MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_VERBOSE,
          .DisableOtherLoggingSinks = false,
      }
  };
NVSDK_NGX_FeatureDiscoveryInfo featureDiscoveryInfo {
      .SDKVersion = NVSDK_NGX_Version_API,
      .FeatureID = NVSDK_NGX_Feature_SuperSampling,
      .Identifier = Application::ngxIdentifier,
      .ApplicationDataPath = Application::dataPath.c_str(),
      .FeatureInfo = &Application::featureCommonInfo,
  };
} // namespace Application

namespace Unity {
IUnityInterfaces *interfaces;
IUnityGraphics *graphicsInterface;
IUnityGraphicsVulkanV2 *vulkanGraphicsInterface;
VkInstance vulkanInstance;
namespace Hooks {
// Loaded before initialization
PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
decltype(&vkCreateInstance) vkCreateInstance;
decltype(&vkCreateDevice) vkCreateDevice;
// Loaded after initialization
decltype(&vkGetDeviceProcAddr) vkGetDeviceProcAddr;
decltype(&vkEnumerateInstanceExtensionProperties) vkEnumerateInstanceExtensionProperties;
decltype(&vkEnumerateDeviceExtensionProperties) vkEnumerateDeviceExtensionProperties;
decltype(&vkCreateCommandPool) vkCreateCommandPool;
decltype(&vkAllocateCommandBuffers) vkAllocateCommandBuffers;
decltype(&vkBeginCommandBuffer) vkBeginCommandBuffer;
decltype(&vkEndCommandBuffer) vkEndCommandBuffer;
decltype(&vkQueueSubmit) vkQueueSubmit;
decltype(&vkQueueWaitIdle) vkQueueWaitIdle;
decltype(&vkResetCommandBuffer) vkResetCommandBuffer;
decltype(&vkFreeCommandBuffers) vkFreeCommandBuffers;
decltype(&vkDestroyCommandPool) vkDestroyCommandPool;

void loadVulkanFunctionHooks(VkInstance instance) {
  vkGetDeviceProcAddr = (decltype(&::vkGetDeviceProcAddr))vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr");
  vkEnumerateInstanceExtensionProperties = (decltype(&::vkEnumerateInstanceExtensionProperties))vkGetInstanceProcAddr(instance, "vkEnumerateInstanceExtensionProperties");
  vkEnumerateDeviceExtensionProperties = (decltype(&::vkEnumerateDeviceExtensionProperties))vkGetInstanceProcAddr(instance, "vkEnumerateInstanceExtensionProperties");
  vkCreateCommandPool = (decltype(&::vkCreateCommandPool))vkGetInstanceProcAddr(instance, "vkCreateCommandPool");
  vkAllocateCommandBuffers = (decltype(&::vkAllocateCommandBuffers))vkGetInstanceProcAddr(instance, "vkAllocateCommandBuffers");
  vkBeginCommandBuffer = (decltype(&::vkBeginCommandBuffer))vkGetInstanceProcAddr(instance, "vkBeginCommandBuffer");
  vkEndCommandBuffer = (decltype(&::vkEndCommandBuffer))vkGetInstanceProcAddr(instance, "vkEndCommandBuffer");
  vkQueueSubmit = (decltype(&::vkQueueSubmit))vkGetInstanceProcAddr(instance, "vkQueueSubmit");
  vkQueueWaitIdle = (decltype(&::vkQueueWaitIdle))vkGetInstanceProcAddr(instance, "vkQueueWaitIdle");
  vkResetCommandBuffer = (decltype(&::vkResetCommandBuffer))vkGetInstanceProcAddr(instance, "vkResetCommandBuffer");
  vkFreeCommandBuffers = (decltype(&::vkFreeCommandBuffers))vkGetInstanceProcAddr(instance, "vkFreeCommandBuffers");
  vkDestroyCommandPool = (decltype(&::vkDestroyCommandPool))vkGetInstanceProcAddr(instance, "vkDestroyCommandPool");
}
} // namespace Hooks
} // namespace Unity

namespace Plugin {
namespace Settings {
struct Resolution {
  unsigned int width;
  unsigned int height;
};
Resolution renderResolution;
Resolution presentResolution;
NVSDK_NGX_PerfQuality_Value DLSSQuality{NVSDK_NGX_PerfQuality_Value_Balanced};
}
bool DLSSSupported;
NVSDK_NGX_Handle *DLSS;
VkCommandPool _oneTimeSubmitCommandPool;
VkCommandBuffer _oneTimeSubmitCommandBuffer;

void prepareForOneTimeSubmits() {
    UnityVulkanInstance vulkan = Unity::vulkanGraphicsInterface->Instance();

    VkCommandPoolCreateInfo createInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = vulkan.queueFamilyIndex,
    };

    Unity::Hooks::vkCreateCommandPool(vulkan.device, &createInfo, nullptr, &_oneTimeSubmitCommandPool);

    VkCommandBufferAllocateInfo allocateInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = _oneTimeSubmitCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 0x1,
    };

    Unity::Hooks::vkAllocateCommandBuffers(vulkan.device, &allocateInfo, &_oneTimeSubmitCommandBuffer);
}

VkCommandBuffer beginOneTimeSubmitRecording() {
    VkCommandBufferBeginInfo beginInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };

    Unity::Hooks::vkBeginCommandBuffer(_oneTimeSubmitCommandBuffer, &beginInfo);

    return _oneTimeSubmitCommandBuffer;
}

void endOneTimeSubmitRecording() {
    UnityVulkanInstance vulkan = Unity::vulkanGraphicsInterface->Instance();

    Unity::Hooks::vkEndCommandBuffer(_oneTimeSubmitCommandBuffer);

    VkSubmitInfo submitInfo {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0x0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 0x1,
        .pCommandBuffers = &_oneTimeSubmitCommandBuffer,
        .signalSemaphoreCount = 0x0,
        .pSignalSemaphores = nullptr,
    };

    Unity::Hooks::vkQueueSubmit(vulkan.graphicsQueue, 1, &submitInfo, nullptr);
    Unity::Hooks::vkQueueWaitIdle(vulkan.graphicsQueue);
    Unity::Hooks::vkResetCommandBuffer(_oneTimeSubmitCommandBuffer, 0x0);
}

void cancelOneTimeSubmitRecording() {
    Unity::Hooks::vkResetCommandBuffer(_oneTimeSubmitCommandBuffer, 0x0);
}

void finishOneTimeSubmits() {
    UnityVulkanInstance vulkan = Unity::vulkanGraphicsInterface->Instance();

    Unity::Hooks::vkFreeCommandBuffers(vulkan.device, _oneTimeSubmitCommandPool, 1, &_oneTimeSubmitCommandBuffer);
    Unity::Hooks::vkDestroyCommandPool(vulkan.device, _oneTimeSubmitCommandPool, nullptr);
}

std::string to_string(wchar_t const* wcstr){
    auto s = std::mbstate_t();
    auto const target_char_count = std::wcsrtombs(nullptr, &wcstr, 0, &s);
    if(target_char_count == static_cast<std::size_t>(-1))
        throw std::logic_error("Illegal byte sequence");

    auto str = std::string(target_char_count, '\0');
    std::wcsrtombs(str.data(), &wcstr, str.size() + 1, &s);
    return str;
}
}

extern "C" UNITY_INTERFACE_API void OnFramebufferResize(unsigned int width, unsigned int height) {
  UnityVulkanInstance vulkan = Unity::vulkanGraphicsInterface->Instance();
  Logger::log("Resizing DLSS targets: " + std::to_string(width) + "x" + std::to_string((height)));

  // Get optimal settings
  NVSDK_NGX_Parameter *parameters;
  NVSDK_NGX_VULKAN_AllocateParameters(&parameters);
  unsigned int renderOptimalWidth, renderOptimalHeight, renderMaxWidth, renderMaxHeight, renderMinWidth, renderMinHeight;
  float sharpness;
  NGX_DLSS_GET_OPTIMAL_SETTINGS(parameters, width, height, Plugin::Settings::DLSSQuality, &renderOptimalWidth, &renderOptimalHeight, &renderMaxWidth, &renderMaxHeight, &renderMinWidth, &renderMinHeight, &sharpness);

  NVSDK_NGX_DLSS_Create_Params DLSSCreateParams {
    .Feature = {
        .InWidth = renderOptimalWidth,
        .InHeight = renderOptimalHeight,
        .InTargetWidth = width,
        .InTargetHeight = height,
        .InPerfQualityValue = Plugin::Settings::DLSSQuality,
    },
  };

  Unity::vulkanGraphicsInterface->EnsureOutsideRenderPass();

  VkCommandBuffer commandBuffer = Plugin::beginOneTimeSubmitRecording();
  NVSDK_NGX_Result result = NGX_VULKAN_CREATE_DLSS_EXT1(vulkan.device, commandBuffer, 1, 1, &Plugin::DLSS, parameters, &DLSSCreateParams);
  Logger::log("Create DLSS feature", result);
  if (!NVSDK_NGX_SUCCEED(result)) {
    Plugin::cancelOneTimeSubmitRecording();
    return;
  }
  Plugin::endOneTimeSubmitRecording();
}

extern "C" UNITY_INTERFACE_EXPORT bool initializeDLSS() {
  UnityVulkanInstance vulkan = Unity::vulkanGraphicsInterface->Instance();

  Unity::Hooks::loadVulkanFunctionHooks(vulkan.instance);

  Plugin::prepareForOneTimeSubmits();

  // Initialize NGX SDK
  NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_Init(
      Application::id, Application::dataPath.c_str(), vulkan.instance, vulkan.physicalDevice, vulkan.device, Unity::Hooks::vkGetInstanceProcAddr, nullptr, &Application::featureCommonInfo, NVSDK_NGX_Version_API);
  Logger::log("Initialize NGX SDK", result);

  // Set up the Super Sampling feature
  NVSDK_NGX_FeatureRequirement featureRequirement;
  result = NVSDK_NGX_VULKAN_GetFeatureRequirements(
      vulkan.instance, vulkan.physicalDevice, &Application::featureDiscoveryInfo,
      &featureRequirement);
  Logger::log("Get DLSS feature requirements", result);

  if (!NVSDK_NGX_SUCCEED(result) || featureRequirement.FeatureSupported != NVSDK_NGX_FeatureSupportResult_Supported) return false;

  // Set and obtain parameters
  NVSDK_NGX_Parameter *parameters{nullptr};
  result = NVSDK_NGX_VULKAN_GetCapabilityParameters(&parameters);
  Logger::log("Get NGX Vulkan capability parameters", result);
  if (!NVSDK_NGX_SUCCEED(result)) {
    NVSDK_NGX_VULKAN_DestroyParameters(parameters);
    return false;
  }
  // Check for DLSS support
  // Is driver up-to-date
  int needsUpdatedDriver;
  int requiredMajorDriverVersion;
  int requiredMinorDriverVersion;
  NVSDK_NGX_Result updateDriverResult = parameters->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsUpdatedDriver);
  Logger::log("Query DLSS graphics driver requirements", updateDriverResult);
  NVSDK_NGX_Result minMajorDriverVersionResult = parameters->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, &requiredMajorDriverVersion);
  Logger::log("Query DLSS minimum graphics driver major version", minMajorDriverVersionResult);
  NVSDK_NGX_Result minMinorDriverVersionResult = parameters->Get(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, &requiredMinorDriverVersion);
  Logger::log("Query DLSS minimum graphics driver minor version", minMinorDriverVersionResult);
  if (!NVSDK_NGX_SUCCEED(updateDriverResult) || !NVSDK_NGX_SUCCEED(minMajorDriverVersionResult) || !NVSDK_NGX_SUCCEED(minMinorDriverVersionResult)) {
    NVSDK_NGX_VULKAN_DestroyParameters(parameters);
    return false;
  }
  if (needsUpdatedDriver) {
    Logger::log("DLSS initialization failed. Minimum driver requirement not met. Update to at least: " + std::to_string(requiredMajorDriverVersion) + "." + std::to_string(requiredMinorDriverVersion));
    return false;
  }
  Logger::log("Graphics driver version is greater than DLSS' required minimum version (" + std::to_string(requiredMajorDriverVersion) + "." + std::to_string(requiredMinorDriverVersion) + ").");
  // Is DLSS available on this hardware and platform
  int DLSSSupported;
  result = parameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &DLSSSupported);
  Logger::log("Query DLSS feature availability", result);
  if (!NVSDK_NGX_SUCCEED(result)) return false;
  if (!DLSSSupported) {
    NVSDK_NGX_Result FeatureInitResult = NVSDK_NGX_Result_Fail;
    NVSDK_NGX_Parameter_GetI(parameters, NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, (int*)&FeatureInitResult);
    std::stringstream stream;
    stream << "DLSS is not available on this hardware or platform. FeatureInitResult = 0x" << std::setfill('0') << std::setw(sizeof(FeatureInitResult) * 2) << std::hex << FeatureInitResult << ", info: " << Plugin::to_string(GetNGXResultAsString(FeatureInitResult));
    Logger::log(stream.str());
    Logger::log("Setting DLSSSupport and continuing anyway.");
    DLSSSupported = 1;
  }
  // Is DLSS denied for this application
  result = parameters->Get(NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, &DLSSSupported);
  Logger::log("Query DLSS feature initialization", result);
  if (!NVSDK_NGX_SUCCEED(result)) {
    NVSDK_NGX_VULKAN_DestroyParameters(parameters);
    return false;
  }
  // clean up
  NVSDK_NGX_VULKAN_DestroyParameters(parameters);
  if (!DLSSSupported) {
    Logger::log("DLSS is denied for this application.");
    return false;
  }

  // Set up scratch buffers
  NVSDK_NGX_VULKAN_AllocateParameters(&parameters);
  size_t scratchBufferSize;
  result = NVSDK_NGX_VULKAN_GetScratchBufferSize(NVSDK_NGX_Feature_SuperSampling, parameters, &scratchBufferSize);
  Logger::log("Query DLSS' required scratch buffer size", result);
  if (!NVSDK_NGX_SUCCEED(result)) {
    NVSDK_NGX_VULKAN_DestroyParameters(parameters);
    return false;
  }
  Logger::log("DLSS requires a " + std::to_string(scratchBufferSize) + " B scratch buffer.");
//  vkCreateBuffer();
//  NVSDK_NGX_Create_Buffer_Resource_VK();

  // DLSS is available.
  return DLSSSupported;
}

/// Hijacks the vkCreateDevice function that Unity uses to create its Vulkan Device.
VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) {
  VkDeviceCreateInfo createInfo = *pCreateInfo;
  std::string message;

/// Enable all extensions and hope that DLSS is compatible
//  uint32_t extensionCount;
//  Unity::Hooks::vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
//  std::vector<VkExtensionProperties> extensions;
//  extensions.resize(extensionCount);
//  Unity::Hooks::vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount,extensions.data());
//
//  std::vector<const char *> extensionNames;
//  extensionNames.reserve(extensionCount);
//  for (VkExtensionProperties &extension : extensions) {
//    extensionNames.push_back(extension.extensionName);
//    message += extension.extensionName + std::string(", ");
//  }
//  message = message.substr(0, message.length() - 2);
//
//  createInfo.enabledExtensionCount = extensionCount;
//  createInfo.ppEnabledExtensionNames = extensionNames.data();

  // @todo Detect if device extensions requested are supported before creating device.
  // Find out which extensions need to be added.
  uint32_t extensionCount;
  VkExtensionProperties *extensions;
  NVSDK_NGX_Result queryResult = NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(Unity::vulkanInstance, physicalDevice, &Application::featureDiscoveryInfo, &extensionCount, &extensions);

  if (NVSDK_NGX_SUCCEED(queryResult)) {
    // Add the extensions that have already been requested to the extensions that need to be added.
    // @todo Consider doing this entirely on the stack for speed.
    std::vector<const char *> enabledExtensions;
    enabledExtensions.reserve(pCreateInfo->enabledExtensionCount +
                              extensionCount);
    for (uint32_t i{0}; i < pCreateInfo->enabledExtensionCount; ++i)
      enabledExtensions.push_back(createInfo.ppEnabledExtensionNames[i]);
    for (uint32_t i{0}; i < extensionCount;) {
      enabledExtensions.push_back(extensions[i].extensionName);
      message += extensions[i].extensionName;
      if (++i < extensionCount) message += ", ";
    }

    // Modify the createInfo.
    createInfo.enabledExtensionCount = enabledExtensions.size();
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();
  }

  // Create the Device
  VkResult result = Unity::Hooks::vkCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
  if (result == VK_SUCCESS) {
    Logger::log("DLSS compatible device creation", queryResult);
    if(NVSDK_NGX_SUCCEED(queryResult)) Logger::log("Added " + std::to_string(extensionCount) + " device extensions: " + message + ".");
  }
  return result;
}

/// Hijacks the vkCreateInstance function that Unity uses to create its Vulkan Instance.
VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateInstance(const VkInstanceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkInstance* pInstance) {
  VkInstanceCreateInfo createInfo = *pCreateInfo;
  std::string message;

/// Enable all extensions and hope that DLSS is compatible
//  uint32_t extensionCount;
//  Unity::Hooks::vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
//  std::vector<VkExtensionProperties> extensions;
//  extensions.resize(extensionCount);
//  Unity::Hooks::vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount,extensions.data());
//
//  std::vector<const char *> extensionNames;
//  extensionNames.reserve(extensionCount);
//  for (VkExtensionProperties &extension : extensions) {
//    extensionNames.push_back(extension.extensionName);
//    message += extension.extensionName + std::string(", ");
//  }
//  message = message.substr(0, message.length() - 2);
//
//  createInfo.enabledExtensionCount = extensionCount;
//  createInfo.ppEnabledExtensionNames = extensionNames.data();

  // @todo Detect if instance extensions requested are supported before creating instance.
  // Find out which extensions need to be added.
  uint32_t extensionCount;
  VkExtensionProperties *extensions;
  NVSDK_NGX_Result queryResult = NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(&Application::featureDiscoveryInfo, &extensionCount, &extensions);

  if (NVSDK_NGX_SUCCEED(queryResult)) {
    // Add the extensions that have already been requested to the extensions that need to be added.
    // @todo Consider doing this entirely on the stack for speed.
    std::vector<const char*> enabledExtensions;
    enabledExtensions.reserve(pCreateInfo->enabledExtensionCount + extensionCount);
    for (uint32_t i{0}; i < pCreateInfo->enabledExtensionCount; ++i)
      enabledExtensions.push_back(createInfo.ppEnabledExtensionNames[i]);
    for (uint32_t i{0}; i < extensionCount;) {
      enabledExtensions.push_back(extensions[i].extensionName);
      message += extensions[i].extensionName;
      if (++i < extensionCount) message += ", ";
    }

    // Modify the createInfo.
    createInfo.enabledExtensionCount = enabledExtensions.size();
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();
  }

  // Create the Instance.
  VkResult result = Unity::Hooks::vkCreateInstance(pCreateInfo, pAllocator, pInstance);
  Unity::vulkanInstance = *pInstance;
  if (result == VK_SUCCESS) {
    Logger::log("DLSS compatible instance creation", queryResult);
    if(NVSDK_NGX_SUCCEED(queryResult)) Logger::log("Added " + std::to_string(extensionCount) + " instance extensions: " + message + ".");
  }
  return result;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
Hook_vkGetInstanceProcAddr(VkInstance instance, const char *pName) {
  if (!pName) return nullptr;
  if (strcmp(pName, "vkCreateInstance") == 0) {
    Unity::Hooks::vkCreateInstance = (decltype(&vkCreateInstance))Unity::Hooks::vkGetInstanceProcAddr(instance, pName);
    return reinterpret_cast<PFN_vkVoidFunction>(&Hook_vkCreateInstance);
  }
  if (strcmp(pName, "vkCreateDevice") == 0) {
    Unity::Hooks::vkCreateDevice = (decltype(&vkCreateDevice))Unity::Hooks::vkGetInstanceProcAddr(instance, pName);
    return reinterpret_cast<PFN_vkVoidFunction>(&Hook_vkCreateDevice);
  }
  return Unity::Hooks::vkGetInstanceProcAddr(instance, pName);
}

PFN_vkGetInstanceProcAddr interceptInitialization(PFN_vkGetInstanceProcAddr t_getInstanceProcAddr, void *t_userData) {
  Unity::Hooks::vkGetInstanceProcAddr = t_getInstanceProcAddr;
  return Hook_vkGetInstanceProcAddr;
}

extern "C" UNITY_INTERFACE_EXPORT void
SetDebugCallback(void (*t_debugFunction)(const char *)) {
  Logger::Info = t_debugFunction;
  Logger::flush();
}

extern "C" void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType) {
  switch (eventType) {
    case kUnityGfxDeviceEventInitialize: {
      UnityGfxRenderer renderer = Unity::graphicsInterface->GetRenderer();
      if (renderer == kUnityGfxRendererNull)
        break;
      if (renderer == kUnityGfxRendererVulkan) {
        Plugin::DLSSSupported = initializeDLSS();
      }
      break;
    }
    case kUnityGfxDeviceEventShutdown: {
      Unity::graphicsInterface->UnregisterDeviceEventCallback( OnGraphicsDeviceEvent);
    }
    case kUnityGfxDeviceEventBeforeReset:
      break;
    case kUnityGfxDeviceEventAfterReset:
      break;
    default:
      break;
  }
}

extern "C" bool UNITY_INTERFACE_EXPORT IsDLSSSupported() {
  return Plugin::DLSSSupported;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
UnityPluginLoad(IUnityInterfaces *t_unityInterfaces) {
  Unity::interfaces = t_unityInterfaces;
  Unity::vulkanGraphicsInterface = t_unityInterfaces->Get<IUnityGraphicsVulkanV2>();
  if (!Unity::vulkanGraphicsInterface->AddInterceptInitialization(interceptInitialization, nullptr, 0)) {
    Logger::log("DLSS Plugin failed to intercept initialization.");
    return;
  }
  Unity::graphicsInterface = t_unityInterfaces->Get<IUnityGraphics>();
  Unity::graphicsInterface->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
  OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload() {
  // Release features
//  NVSDK_NGX_VULKAN_ReleaseFeature();

  // Shutdown NGX
  NVSDK_NGX_VULKAN_Shutdown1(Unity::vulkanGraphicsInterface->Instance().device);

  // Finish all one time submits
  Plugin::finishOneTimeSubmits();

  // Perform shutdown graphics event
  OnGraphicsDeviceEvent(kUnityGfxDeviceEventShutdown);
}
