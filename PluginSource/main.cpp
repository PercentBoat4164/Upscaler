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
#    define WAIT_FOR_DEBUGGER               \
        {                                   \
            bool debuggerConnected = false; \
            while (!debuggerConnected)      \
                ;                           \
        }
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
    if (NVSDK_NGX_SUCCEED(t_result)) return log("DLSSPlugin: " + t_actionDescription + ": Succeeded");
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

namespace GraphicsAPI {
class GraphicsAPI {
public:
    virtual ~GraphicsAPI() = default;
};

class Vulkan : public GraphicsAPI {
public:
    struct DeviceFunctions {
    private:
        VkDevice device;

        PFN_vkCreateImageView        m_vkCreateImageView;
        PFN_vkCreateCommandPool      m_vkCreateCommandPool;
        PFN_vkAllocateCommandBuffers m_vkAllocateCommandBuffers;
        PFN_vkBeginCommandBuffer     m_vkBeginCommandBuffer;
        PFN_vkEndCommandBuffer       m_vkEndCommandBuffer;
        PFN_vkQueueSubmit            m_vkQueueSubmit;
        PFN_vkQueueWaitIdle          m_vkQueueWaitIdle;
        PFN_vkResetCommandBuffer     m_vkResetCommandBuffer;
        PFN_vkFreeCommandBuffers     m_vkFreeCommandBuffers;
        PFN_vkDestroyCommandPool     m_vkDestroyCommandPool;

    public:
        DeviceFunctions(VkDevice device, PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr) :
                device(device),
                // clang-format off
                m_vkCreateImageView(reinterpret_cast<PFN_vkCreateImageView>(vkGetDeviceProcAddr(device, "vkCreateImageView"))),
                m_vkCreateCommandPool(reinterpret_cast<PFN_vkCreateCommandPool>(vkGetDeviceProcAddr(device, "vkCreateCommandPool"))),
                m_vkAllocateCommandBuffers(reinterpret_cast<PFN_vkAllocateCommandBuffers>(vkGetDeviceProcAddr(device, "vkAllocateCommandBuffers"))),
                m_vkBeginCommandBuffer(reinterpret_cast<PFN_vkBeginCommandBuffer>(vkGetDeviceProcAddr(device, "vkBeginCommandBuffer"))),
                m_vkEndCommandBuffer(reinterpret_cast<PFN_vkEndCommandBuffer>(vkGetDeviceProcAddr(device, "vkEndCommandBuffer"))),
                m_vkQueueSubmit(reinterpret_cast<PFN_vkQueueSubmit>(vkGetDeviceProcAddr(device, "vkQueueSubmit"))),
                m_vkQueueWaitIdle(reinterpret_cast<PFN_vkQueueWaitIdle>(vkGetDeviceProcAddr(device, "vkQueueWaitIdle"))),
                m_vkResetCommandBuffer(reinterpret_cast<PFN_vkResetCommandBuffer>(vkGetDeviceProcAddr(device, "vkResetCommandBuffer"))),
                m_vkFreeCommandBuffers(reinterpret_cast<PFN_vkFreeCommandBuffers>(vkGetDeviceProcAddr(device, "vkFreeCommandBuffers"))),
                m_vkDestroyCommandPool(reinterpret_cast<PFN_vkDestroyCommandPool>(vkGetDeviceProcAddr(device, "vkDestroyCommandPool")))
        // clang-format on
        {
        }

        inline VkResult vkCreateCommandPool(
          const VkCommandPoolCreateInfo *pCreateInfo,
          const VkAllocationCallbacks   *pAllocator,
          VkCommandPool                 *pCommandPool
        ) {
            return m_vkCreateCommandPool(device, pCreateInfo, pAllocator, pCommandPool);
        }

        inline VkResult vkAllocateCommandBuffers(
          const VkCommandBufferAllocateInfo *pAllocateInfo,
          VkCommandBuffer                   *pCommandBuffers
        ) {
            return m_vkAllocateCommandBuffers(device, pAllocateInfo, pCommandBuffers);
        }

        inline VkResult vkBeginCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferBeginInfo *pBeginInfo) {
            return m_vkBeginCommandBuffer(commandBuffer, pBeginInfo);
        }

        inline VkResult vkEndCommandBuffer(VkCommandBuffer commandBuffer) {
            return m_vkEndCommandBuffer(commandBuffer);
        }

        inline VkResult
        vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo *pSubmits, VkFence fence) {
            return m_vkQueueSubmit(queue, submitCount, pSubmits, fence);
        }

        inline VkResult vkQueueWaitIdle(VkQueue queue) {
            return m_vkQueueWaitIdle(queue);
        }

        inline VkResult vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) {
            return m_vkResetCommandBuffer(commandBuffer, flags);
        }

        inline void vkFreeCommandBuffers(
          VkCommandPool    commandPool,
          uint32_t         commandBufferCount,
          VkCommandBuffer *pCommandBuffers
        ) {
            return m_vkFreeCommandBuffers(device, commandPool, commandBufferCount, pCommandBuffers);
        }

        inline void vkDestroyCommandPool(VkCommandPool commandPool, const VkAllocationCallbacks *pAllocator) {
            return m_vkDestroyCommandPool(device, commandPool, pAllocator);
        }

        inline VkResult vkCreateImageView(
          const VkImageViewCreateInfo *pCreateInfo,
          const VkAllocationCallbacks *pAllocator,
          VkImageView                 *pView
        ) {
            return m_vkCreateImageView(device, pCreateInfo, pAllocator, pView);
        }
    };

private:
    static PFN_vkGetInstanceProcAddr                  m_vkGetInstanceProcAddr;
    static PFN_vkGetDeviceProcAddr                    m_vkGetDeviceProcAddr;
    static PFN_vkCreateInstance                       m_vkCreateInstance;
    static PFN_vkEnumerateInstanceExtensionProperties m_vkEnumerateInstanceExtensionProperties;
    static PFN_vkCreateDevice                         m_vkCreateDevice;
    static PFN_vkEnumerateDeviceExtensionProperties   m_vkEnumerateDeviceExtensionProperties;

    static VkInstance                                    instance;
    static std::unordered_map<VkDevice, DeviceFunctions> deviceFunctions;
    static IUnityGraphicsVulkanV2                       *vulkanInterface;

    static bool loadEarlyFunctionPointers() {
        // clang-format off
        m_vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(m_vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance"));
        m_vkEnumerateInstanceExtensionProperties = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(m_vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties"));
        return (m_vkCreateInstance != nullptr) && (m_vkEnumerateInstanceExtensionProperties != nullptr);
        // clang-format on
    }

    static bool loadLateFunctionPointers() {
        // clang-format off
        m_vkEnumerateDeviceExtensionProperties = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(m_vkGetInstanceProcAddr(instance, "vkEnumerateDeviceExtensionProperties"));
        m_vkCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(m_vkGetInstanceProcAddr(instance, "vkCreateDevice"));
        m_vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(m_vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr"));
        return (m_vkEnumerateInstanceExtensionProperties != nullptr) && (m_vkCreateDevice != nullptr) && (m_vkGetDeviceProcAddr != nullptr);
        // clang-format on
    }

    static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateInstance(
      const VkInstanceCreateInfo  *pCreateInfo,
      const VkAllocationCallbacks *pAllocator,
      VkInstance                  *pInstance
    ) {
        VkInstanceCreateInfo createInfo = *pCreateInfo;
        std::stringstream    message;

        // Find out which requestedExtensions are supported
        uint32_t                           supportedExtensionCount{};
        std::vector<VkExtensionProperties> supportedExtensions;
        m_vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionCount, nullptr);
        supportedExtensions.resize(supportedExtensionCount);
        m_vkEnumerateInstanceExtensionProperties(nullptr, &supportedExtensionCount, supportedExtensions.data());

        // Find out which extensions need to be added.
        uint32_t                 requestedExtensionCount{};
        std::vector<std::string> requestedExtensions{};
        VkExtensionProperties   *extensions{};
        NVSDK_NGX_Result         queryResult = NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(
          &Application::featureDiscoveryInfo,
          &requestedExtensionCount,
          &extensions
        );

        requestedExtensions.reserve(requestedExtensionCount);
        for (uint32_t i{}; i < requestedExtensionCount; ++i)
            requestedExtensions.emplace_back(extensions[i].extensionName);


        // Ensure that each requested extension is supported
        uint32_t supportedRequestedExtensions{};
        for (const std::string &requestedExtension : requestedExtensions)
            for (VkExtensionProperties supportedExtension : supportedExtensions)
                if (requestedExtension == (const char *) supportedExtension.extensionName) {
                    ++supportedRequestedExtensions;
                    break;
                }


        // Add the extensions if they are supported and they are not already in the createInfo.
        std::vector<const char *> enabledExtensions;
        if (supportedRequestedExtensions == requestedExtensionCount) {
            Logger::log("All requested instance extensions are supported");
            if (NVSDK_NGX_SUCCEED(queryResult)) {
                // Add the extensions that have already been requested to the extensions that need to be added.
                enabledExtensions.reserve(pCreateInfo->enabledExtensionCount + requestedExtensionCount);
                for (uint32_t i{0}; i < pCreateInfo->enabledExtensionCount; ++i)
                    enabledExtensions.push_back(createInfo.ppEnabledExtensionNames[i]);
                for (const std::string &extension : requestedExtensions) {
                    bool extensionShouldBeAdded{true};
                    for (const auto *enabledExtension : enabledExtensions)
                        if (enabledExtension == extension) {
                            extensionShouldBeAdded = false;
                            break;
                        }
                    if (extensionShouldBeAdded) {
                        enabledExtensions.push_back(extension.c_str());
                        message << extension << ", ";
                    }
                }

                // Modify the createInfo.
                createInfo.enabledExtensionCount   = enabledExtensions.size();
                createInfo.ppEnabledExtensionNames = enabledExtensions.data();
            }
        }

        // Create the Instance.
        VkResult result = m_vkCreateInstance(&createInfo, pAllocator, pInstance);
        if (result == VK_SUCCESS) {
            instance = *pInstance;
            Logger::log("DLSS compatible instance creation", queryResult);
            if (NVSDK_NGX_SUCCEED(queryResult)) {
                std::string msg = message.str();
                if (msg.empty()) Logger::log("All requested instance extensions were already enabled.");
                else Logger::log("Added instance extensions: " + msg.substr(msg.length() - 2) + ".");
            }
        }
        return result;
    }

    static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateDevice(
      VkPhysicalDevice          physicalDevice,
      const VkDeviceCreateInfo *pCreateInfo,
      VkAllocationCallbacks    *pAllocator,
      VkDevice                 *pDevice
    ) {
        loadLateFunctionPointers();

        VkDeviceCreateInfo createInfo = *pCreateInfo;
        std::stringstream  message;

        // Find out which requestedExtensions are supported
        uint32_t                           supportedExtensionCount{};
        std::vector<VkExtensionProperties> supportedExtensions;
        m_vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &supportedExtensionCount, nullptr);
        supportedExtensions.resize(supportedExtensionCount);
        m_vkEnumerateDeviceExtensionProperties(
          physicalDevice,
          nullptr,
          &supportedExtensionCount,
          supportedExtensions.data()
        );

        // Find out which extensions need to be added for DLSS.
        uint32_t                 requestedExtensionCount{};
        std::vector<std::string> requestedExtensions{};
        VkExtensionProperties   *extensions{};
        NVSDK_NGX_Result         queryResult = NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(
          instance,
          physicalDevice,
          &Application::featureDiscoveryInfo,
          &requestedExtensionCount,
          &extensions
        );
        requestedExtensions.reserve(requestedExtensionCount);
        for (uint32_t i{}; i < requestedExtensionCount; ++i)
            requestedExtensions.emplace_back(extensions[i].extensionName);

        // Ensure that each requested extension is supported
        uint32_t extensionsSupported{};
        for (const std::string &requestedExtension : requestedExtensions)
            for (VkExtensionProperties supportedExtension : supportedExtensions)
                if (requestedExtension == (const char *) supportedExtension.extensionName) {
                    ++extensionsSupported;
                    break;
                }

        // Add the extensions if they are supported and they are not already in the createInfo.
        std::vector<const char *> enabledExtensions;
        if (extensionsSupported == requestedExtensionCount) {
            Logger::log("All requested device extensions are supported");
            if (NVSDK_NGX_SUCCEED(queryResult)) {
                // Add the extensions that have already been requested to the extensions that need to be added.
                enabledExtensions.reserve(pCreateInfo->enabledExtensionCount + requestedExtensionCount);
                for (uint32_t i{0}; i < pCreateInfo->enabledExtensionCount; ++i)
                    enabledExtensions.push_back(createInfo.ppEnabledExtensionNames[i]);
                for (const std::string &extension : requestedExtensions) {
                    bool extensionShouldBeAdded{true};
                    for (const auto *enabledExtension : enabledExtensions)
                        if (extension == enabledExtension) {
                            extensionShouldBeAdded = false;
                            break;
                        }
                    if (extensionShouldBeAdded) {
                        enabledExtensions.push_back(extension.c_str());
                        message << extension << ", ";
                    }
                }

                // Modify the createInfo.
                createInfo.enabledExtensionCount   = enabledExtensions.size();
                createInfo.ppEnabledExtensionNames = enabledExtensions.data();
            }
        }

        // Create the Device
        VkResult result = m_vkCreateDevice(physicalDevice, &createInfo, pAllocator, pDevice);
        if (result == VK_SUCCESS) {
            deviceFunctions.insert({*pDevice, DeviceFunctions(*pDevice, m_vkGetDeviceProcAddr)});
            Logger::log("DLSS compatible device creation", queryResult);
            if (NVSDK_NGX_SUCCEED(queryResult)) {
                std::string msg = message.str();
                if (msg.empty()) Logger::log("All requested device extensions were already enabled.");
                else Logger::log("Added device requestedExtensions: " + msg.substr(0, msg.length() - 2) + ".");
            }
        }

        return result;
    }

    static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
    Hook_vkGetInstanceProcAddr(VkInstance /*unused*/, const char *pName) {
        if (pName == nullptr) return nullptr;
        if (strcmp(pName, "vkCreateInstance") == 0)
            return reinterpret_cast<PFN_vkVoidFunction>(&Hook_vkCreateInstance);
        if (strcmp(pName, "vkCreateDevice") == 0)
            return reinterpret_cast<PFN_vkVoidFunction>(&Hook_vkCreateDevice);
        return m_vkGetInstanceProcAddr(instance, pName);
    }

    static PFN_vkGetInstanceProcAddr
    interceptInitialization(PFN_vkGetInstanceProcAddr t_getInstanceProcAddr, void * /*unused*/) {
        setVkGetInstanceProcAddr(t_getInstanceProcAddr);
        loadEarlyFunctionPointers();
        return Hook_vkGetInstanceProcAddr;
    }

public:
    inline static DeviceFunctions get(VkDevice device) {
        return deviceFunctions.at(device);
    }

    inline static void setVkGetInstanceProcAddr(PFN_vkGetInstanceProcAddr t_vkGetInstanceProcAddr) {
        m_vkGetInstanceProcAddr = t_vkGetInstanceProcAddr;
    }

    inline static PFN_vkGetInstanceProcAddr getVkGetInstanceProcAddr() {
        return m_vkGetInstanceProcAddr;
    }

    static bool interceptInitialization(IUnityGraphicsVulkanV2 *t_vulkanInterface) {
        vulkanInterface = t_vulkanInterface;
        return vulkanInterface->AddInterceptInitialization(interceptInitialization, nullptr, 0);
    }

    static bool RemoveInterceptInitialization() {
        bool result     = vulkanInterface->RemoveInterceptInitialization(interceptInitialization);
        vulkanInterface = nullptr;
        return result;
    }

    inline static IUnityGraphicsVulkanV2 *getVulkanInterface() {
        return vulkanInterface;
    }

    ~Vulkan() override = default;
};

PFN_vkGetInstanceProcAddr                  Vulkan::m_vkGetInstanceProcAddr{};
PFN_vkGetDeviceProcAddr                    Vulkan::m_vkGetDeviceProcAddr{};
PFN_vkCreateInstance                       Vulkan::m_vkCreateInstance{};
PFN_vkEnumerateInstanceExtensionProperties Vulkan::m_vkEnumerateInstanceExtensionProperties{};
PFN_vkCreateDevice                         Vulkan::m_vkCreateDevice{};
PFN_vkEnumerateDeviceExtensionProperties   Vulkan::m_vkEnumerateDeviceExtensionProperties{};
VkInstance                                    Vulkan::instance{};
std::unordered_map<VkDevice, Vulkan::DeviceFunctions> Vulkan::deviceFunctions{};
IUnityGraphicsVulkanV2                       *Vulkan::vulkanInterface{};
}  // namespace GraphicsAPI

namespace Upscaler {
class Upscaler {};

class DLSS : public Upscaler {};
}  // namespace Upscaler

namespace Unity {
IUnityInterfaces *interfaces;
IUnityGraphics   *graphicsInterface;
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

VkCommandBuffer beginOneTimeSubmitRecording() {
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

void endOneTimeSubmitRecording() {
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

    device.vkQueueSubmit(vulkanInstance.graphicsQueue, 1, &submitInfo, nullptr);
    device.vkQueueWaitIdle(vulkanInstance.graphicsQueue);
    device.vkResetCommandBuffer(_oneTimeSubmitCommandBuffer, 0x0);
    _oneTimeSubmitRecording = false;
}

void cancelOneTimeSubmitRecording() {
    if (!_oneTimeSubmitRecording) return;
    GraphicsAPI::Vulkan::DeviceFunctions device =
      GraphicsAPI::Vulkan::get(GraphicsAPI::Vulkan::getVulkanInterface()->Instance().device);
    device.vkResetCommandBuffer(_oneTimeSubmitCommandBuffer, 0x0);
    _oneTimeSubmitRecording = false;
}

void finishOneTimeSubmits() {
    GraphicsAPI::Vulkan::DeviceFunctions device =
      GraphicsAPI::Vulkan::get(GraphicsAPI::Vulkan::getVulkanInterface()->Instance().device);
    device.vkFreeCommandBuffers(_oneTimeSubmitCommandPool, 1, &_oneTimeSubmitCommandBuffer);
    device.vkDestroyCommandPool(_oneTimeSubmitCommandPool, nullptr);
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

extern "C" UNITY_INTERFACE_EXPORT void OnFramebufferResize(unsigned int t_width, unsigned int t_height) {
    Logger::log("Resizing DLSS targets: " + std::to_string(t_width) + "x" + std::to_string(t_height));

    if (!Plugin::DLSSSupported) return;

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

    GraphicsAPI::Vulkan::getVulkanInterface()->EnsureOutsideRenderPass();

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

    // clang-format off
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
    // clang-format on

    VkResult result = GraphicsAPI::Vulkan::get(GraphicsAPI::Vulkan::getVulkanInterface()->Instance().device)
                        .vkCreateImageView(&createInfo, nullptr, &imageView);
    if (result == VK_SUCCESS) Logger::log("Created depth resource for DLSS.");


    // clang-format off
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
    // clang-format on
}

extern "C" UNITY_INTERFACE_EXPORT void EvaluateDLSS() {
    UnityVulkanRecordingState state{};
    GraphicsAPI::Vulkan::getVulkanInterface()->EnsureInsideRenderPass();
    GraphicsAPI::Vulkan::getVulkanInterface()->CommandRecordingState(
      &state,
      kUnityVulkanGraphicsQueueAccess_DontCare
    );

    NVSDK_NGX_Result result =
      NGX_VULKAN_EVALUATE_DLSS_EXT(state.commandBuffer, Plugin::DLSS, Plugin::parameters, &Plugin::evalParameters);
    Logger::log("Evaluated DLSS feature", result);
}

extern "C" UNITY_INTERFACE_EXPORT bool initializeDLSS() {
    if (!Plugin::DLSSSupported) return false;

    UnityVulkanInstance vulkanInstance = GraphicsAPI::Vulkan::getVulkanInterface()->Instance();

    Plugin::prepareForOneTimeSubmits();

    // Initialize NGX SDK
    NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_Init(
      Application::id,
      Application::dataPath.c_str(),
      vulkanInstance.instance,
      vulkanInstance.physicalDevice,
      vulkanInstance.device,
      GraphicsAPI::Vulkan::getVkGetInstanceProcAddr(),
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
        stream << "DLSSPlugin: DLSS is not available on this hardware or platform. FeatureInitResult = 0x"
               << std::setfill('0') << std::setw(sizeof(FeatureInitResult) * 2) << std::hex << FeatureInitResult
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
    Unity::interfaces = t_unityInterfaces;
    if (!GraphicsAPI::Vulkan::interceptInitialization(t_unityInterfaces->Get<IUnityGraphicsVulkanV2>())) {
        Logger::log("DLSS Plugin failed to intercept initialization.");
        Plugin::DLSSSupported = false;
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
    GraphicsAPI::Vulkan::RemoveInterceptInitialization();
    // Remove the device event callback
    Unity::graphicsInterface->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
    // Perform shutdown graphics event
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventShutdown);
}
