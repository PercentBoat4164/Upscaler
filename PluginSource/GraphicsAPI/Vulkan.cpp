#ifdef ENABLE_VULKAN
#    include "Vulkan.hpp"

#    include <Upscaler/Upscaler.hpp>

#    include <IUnityGraphicsVulkan.h>
#    include <IUnityRenderingExtensions.h>

#    include <algorithm>
#    include <cstring>

VkInstance Vulkan::temporaryInstance{VK_NULL_HANDLE};

PFN_vkGetInstanceProcAddr                  Vulkan::m_vkGetInstanceProcAddr{VK_NULL_HANDLE};
PFN_vkGetDeviceProcAddr                    Vulkan::m_vkGetDeviceProcAddr{VK_NULL_HANDLE};
PFN_vkCreateInstance                       Vulkan::m_vkCreateInstance{VK_NULL_HANDLE};
PFN_vkEnumerateInstanceExtensionProperties Vulkan::m_vkEnumerateInstanceExtensionProperties{VK_NULL_HANDLE};
PFN_vkCreateDevice                         Vulkan::m_vkCreateDevice{VK_NULL_HANDLE};
PFN_vkEnumerateDeviceExtensionProperties   Vulkan::m_vkEnumerateDeviceExtensionProperties{VK_NULL_HANDLE};

PFN_vkCreateImageView  Vulkan::m_vkCreateImageView{VK_NULL_HANDLE};
PFN_vkDestroyImageView Vulkan::m_vkDestroyImageView{VK_NULL_HANDLE};

IUnityGraphicsVulkanV2* Vulkan::graphicsInterface{nullptr};

void Vulkan::loadEarlyFunctionPointers() {
    m_vkCreateInstance                       = reinterpret_cast<PFN_vkCreateInstance>(m_vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance"));
    m_vkEnumerateInstanceExtensionProperties = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(m_vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties"));
}

void Vulkan::loadInstanceFunctionPointers(VkInstance instance) {
    m_vkEnumerateDeviceExtensionProperties = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(m_vkGetInstanceProcAddr(instance, "vkEnumerateDeviceExtensionProperties"));
    m_vkCreateDevice                       = reinterpret_cast<PFN_vkCreateDevice>(m_vkGetInstanceProcAddr(instance, "vkCreateDevice"));
    m_vkGetDeviceProcAddr                  = reinterpret_cast<PFN_vkGetDeviceProcAddr>(m_vkGetInstanceProcAddr(instance, "vkGetDeviceProcAddr"));
}

void Vulkan::loadDeviceFunctionPointers(VkDevice device) {
    m_vkCreateImageView  = reinterpret_cast<PFN_vkCreateImageView>(m_vkGetDeviceProcAddr(device, "vkCreateImageView"));
    m_vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(m_vkGetDeviceProcAddr(device, "vkDestroyImageView"));
}

std::vector<std::string> Vulkan::getSupportedInstanceExtensions() {
    uint32_t                           extensionCount{};
    std::vector<std::string>           extensions;
    std::vector<VkExtensionProperties> extensionProperties{};
    m_vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    extensionProperties.resize(extensionCount);
    m_vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProperties.data());
    extensions.reserve(extensionCount);
    for (auto [extensionName, specVersion] : extensionProperties) extensions.emplace_back(extensionName);
    return extensions;
}

VkResult Vulkan::Hook_vkCreateInstance(
  const VkInstanceCreateInfo*  pCreateInfo,
  const VkAllocationCallbacks* pAllocator,
  VkInstance*                  pInstance
) {
    VkInstanceCreateInfo createInfo = *pCreateInfo;

    // Find out which extensions are supported
    const std::vector<std::string>  supportedExtensions = getSupportedInstanceExtensions();
    const std::vector<std::string>& requestedExtensions = Upscaler::requestVulkanInstanceExtensions(supportedExtensions);

    // Vectorize the enabled extensions
    std::vector<const char*> enabledExtensions{};
    enabledExtensions.reserve(pCreateInfo->enabledExtensionCount + requestedExtensions.size());
    for (uint32_t i{}; i < pCreateInfo->enabledExtensionCount; ++i)
        enabledExtensions.emplace_back(pCreateInfo->ppEnabledExtensionNames[i]);
    for (const std::string& extension : requestedExtensions)
        if (std::find_if(enabledExtensions.begin(), enabledExtensions.end(), [&extension](const char* str) { return strcmp(str, extension.c_str()) == 0; }) == enabledExtensions.end())
            enabledExtensions.push_back(extension.c_str());

    // Modify the createInfo.
    createInfo.enabledExtensionCount   = enabledExtensions.size();
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();

    // Create the Instance.
    VkResult result = m_vkCreateInstance(&createInfo, pAllocator, pInstance);
    if (result != VK_SUCCESS)
        result = m_vkCreateInstance(pCreateInfo, pAllocator, pInstance);
    temporaryInstance = *pInstance;
    return result;
}

std::vector<std::string> Vulkan::getSupportedDeviceExtensions(VkPhysicalDevice physicalDevice) {
    uint32_t                           extensionCount{};
    std::vector<std::string>           extensions;
    std::vector<VkExtensionProperties> extensionProperties{};
    m_vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    extensionProperties.resize(extensionCount);
    m_vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensionProperties.data());
    extensions.reserve(extensionCount);
    for (auto [extensionName, specVersion] : extensionProperties) extensions.emplace_back(extensionName);
    return extensions;
}

VkResult Vulkan::Hook_vkCreateDevice(
  VkPhysicalDevice             physicalDevice,
  const VkDeviceCreateInfo*    pCreateInfo,
  const VkAllocationCallbacks* pAllocator,
  VkDevice*                    pDevice
) {
    loadInstanceFunctionPointers(temporaryInstance);

    VkDeviceCreateInfo createInfo = *pCreateInfo;
    std::stringstream  message;

    // Find out which extensions are supported
    const std::vector<std::string>  supportedExtensions = getSupportedDeviceExtensions(physicalDevice);
    const std::vector<std::string>& requestedExtensions = Upscaler::requestVulkanDeviceExtensions(temporaryInstance, physicalDevice, supportedExtensions);
    temporaryInstance                                   = VK_NULL_HANDLE;

    // Vectorize the enabled extensions
    std::vector<const char*> enabledExtensions{};
    enabledExtensions.reserve(pCreateInfo->enabledExtensionCount + requestedExtensions.size());
    for (uint32_t i{}; i < pCreateInfo->enabledExtensionCount; ++i)
        enabledExtensions.emplace_back(pCreateInfo->ppEnabledExtensionNames[i]);
    for (const std::string& extension : requestedExtensions)
        if (std::find_if(enabledExtensions.begin(), enabledExtensions.end(), [&extension](const char* str) { return strcmp(str, extension.c_str()) == 0; }) == enabledExtensions.end())
            enabledExtensions.push_back(extension.c_str());

    // Modify the createInfo.
    createInfo.enabledExtensionCount   = enabledExtensions.size();
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();

    // Create the Device
    VkResult result = m_vkCreateDevice(physicalDevice, &createInfo, pAllocator, pDevice);
    if (result != VK_SUCCESS)
        result = m_vkCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
    if (result == VK_SUCCESS)
        loadDeviceFunctionPointers(*pDevice);
    return result;
}

PFN_vkVoidFunction Vulkan::Hook_vkGetInstanceProcAddr(VkInstance t_instance, const char* pName) {
    if (pName == nullptr) return nullptr;
    if (strcmp(pName, "vkCreateInstance") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(&Hook_vkCreateInstance);
    if (strcmp(pName, "vkCreateDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Hook_vkCreateDevice);
    return m_vkGetInstanceProcAddr(t_instance, pName);
}

PFN_vkGetInstanceProcAddr
Vulkan::interceptInitialization(PFN_vkGetInstanceProcAddr t_getInstanceProcAddr, void* /*unused*/) {
    m_vkGetInstanceProcAddr = t_getInstanceProcAddr;
    loadEarlyFunctionPointers();
    return Hook_vkGetInstanceProcAddr;
}

bool Vulkan::registerUnityInterfaces(IUnityInterfaces* t_unityInterfaces) {
    graphicsInterface = t_unityInterfaces->Get<IUnityGraphicsVulkanV2>();
    return graphicsInterface->AddInterceptInitialization(interceptInitialization, nullptr, 0);
}

IUnityGraphicsVulkanV2* Vulkan::getGraphicsInterface() {
    return graphicsInterface;
}

bool Vulkan::unregisterUnityInterfaces() {
    const bool result = graphicsInterface->RemoveInterceptInitialization(interceptInitialization);
    graphicsInterface = nullptr;
    return result;
}

VkFormat Vulkan::getFormat(const UnityRenderingExtTextureFormat format) {
    switch (format) {
        case kUnityRenderingExtFormatNone: return VK_FORMAT_UNDEFINED;
        case kUnityRenderingExtFormatR8_SRGB: return VK_FORMAT_R8_SRGB;
        case kUnityRenderingExtFormatR8G8_SRGB: return VK_FORMAT_R8G8_SRGB;
        case kUnityRenderingExtFormatR8G8B8_SRGB: return VK_FORMAT_R8G8B8_SRGB;
        case kUnityRenderingExtFormatR8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
        case kUnityRenderingExtFormatR8_UNorm: return VK_FORMAT_R8_UNORM;
        case kUnityRenderingExtFormatR8G8_UNorm: return VK_FORMAT_R8G8_UNORM;
        case kUnityRenderingExtFormatR8G8B8_UNorm: return VK_FORMAT_R8G8B8_UNORM;
        case kUnityRenderingExtFormatR8G8B8A8_UNorm: return VK_FORMAT_R8G8B8A8_UNORM;
        case kUnityRenderingExtFormatR8_SNorm: return VK_FORMAT_R8_SNORM;
        case kUnityRenderingExtFormatR8G8_SNorm: return VK_FORMAT_R8G8_SNORM;
        case kUnityRenderingExtFormatR8G8B8_SNorm: return VK_FORMAT_R8G8B8_SNORM;
        case kUnityRenderingExtFormatR8G8B8A8_SNorm: return VK_FORMAT_R8G8B8A8_SNORM;
        case kUnityRenderingExtFormatR8_UInt: return VK_FORMAT_R8_UINT;
        case kUnityRenderingExtFormatR8G8_UInt: return VK_FORMAT_R8G8_UINT;
        case kUnityRenderingExtFormatR8G8B8_UInt: return VK_FORMAT_R8G8B8_UINT;
        case kUnityRenderingExtFormatR8G8B8A8_UInt: return VK_FORMAT_R8G8B8A8_UINT;
        case kUnityRenderingExtFormatR8_SInt: return VK_FORMAT_R8_SINT;
        case kUnityRenderingExtFormatR8G8_SInt: return VK_FORMAT_R8G8_SINT;
        case kUnityRenderingExtFormatR8G8B8_SInt: return VK_FORMAT_R8G8B8_SINT;
        case kUnityRenderingExtFormatR8G8B8A8_SInt: return VK_FORMAT_R8G8B8A8_SINT;
        case kUnityRenderingExtFormatR16_UNorm: return VK_FORMAT_R16_UNORM;
        case kUnityRenderingExtFormatR16G16_UNorm: return VK_FORMAT_R16G16_UNORM;
        case kUnityRenderingExtFormatR16G16B16_UNorm: return VK_FORMAT_R16G16B16_UNORM;
        case kUnityRenderingExtFormatR16G16B16A16_UNorm: return VK_FORMAT_R16G16B16A16_UNORM;
        case kUnityRenderingExtFormatR16_SNorm: return VK_FORMAT_R16_SNORM;
        case kUnityRenderingExtFormatR16G16_SNorm: return VK_FORMAT_R16G16_SNORM;
        case kUnityRenderingExtFormatR16G16B16_SNorm: return VK_FORMAT_R16G16B16_SNORM;
        case kUnityRenderingExtFormatR16G16B16A16_SNorm: return VK_FORMAT_R16G16B16A16_SNORM;
        case kUnityRenderingExtFormatR16_UInt: return VK_FORMAT_R16_UINT;
        case kUnityRenderingExtFormatR16G16_UInt: return VK_FORMAT_R16G16_UINT;
        case kUnityRenderingExtFormatR16G16B16_UInt: return VK_FORMAT_R16G16B16_UINT;
        case kUnityRenderingExtFormatR16G16B16A16_UInt: return VK_FORMAT_R16G16B16A16_UINT;
        case kUnityRenderingExtFormatR16_SInt: return VK_FORMAT_R16_SINT;
        case kUnityRenderingExtFormatR16G16_SInt: return VK_FORMAT_R16G16_SINT;
        case kUnityRenderingExtFormatR16G16B16_SInt: return VK_FORMAT_R16G16B16_SINT;
        case kUnityRenderingExtFormatR16G16B16A16_SInt: return VK_FORMAT_R16G16B16A16_SINT;
        case kUnityRenderingExtFormatR32_UInt: return VK_FORMAT_R32_UINT;
        case kUnityRenderingExtFormatR32G32_UInt: return VK_FORMAT_R32G32_UINT;
        case kUnityRenderingExtFormatR32G32B32_UInt: return VK_FORMAT_R32G32B32_UINT;
        case kUnityRenderingExtFormatR32G32B32A32_UInt: return VK_FORMAT_R32G32B32A32_UINT;
        case kUnityRenderingExtFormatR32_SInt: return VK_FORMAT_R32_SINT;
        case kUnityRenderingExtFormatR32G32_SInt: return VK_FORMAT_R32G32_SINT;
        case kUnityRenderingExtFormatR32G32B32_SInt: return VK_FORMAT_R32G32B32_SINT;
        case kUnityRenderingExtFormatR32G32B32A32_SInt: return VK_FORMAT_R32G32B32A32_SINT;
        case kUnityRenderingExtFormatR16_SFloat: return VK_FORMAT_R16_SFLOAT;
        case kUnityRenderingExtFormatR16G16_SFloat: return VK_FORMAT_R16G16_SFLOAT;
        case kUnityRenderingExtFormatR16G16B16_SFloat: return VK_FORMAT_R16G16B16_SFLOAT;
        case kUnityRenderingExtFormatR16G16B16A16_SFloat: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case kUnityRenderingExtFormatR32_SFloat: return VK_FORMAT_R32_SFLOAT;
        case kUnityRenderingExtFormatR32G32_SFloat: return VK_FORMAT_R32G32_SFLOAT;
        case kUnityRenderingExtFormatR32G32B32_SFloat: return VK_FORMAT_R32G32B32_SFLOAT;
        case kUnityRenderingExtFormatR32G32B32A32_SFloat: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case kUnityRenderingExtFormatL8_UNorm:                               // INVALID
        case kUnityRenderingExtFormatA8_UNorm:                               // INVALID
        case kUnityRenderingExtFormatA16_UNorm: return VK_FORMAT_UNDEFINED;  // INVALID
        case kUnityRenderingExtFormatB8G8R8_SRGB: return VK_FORMAT_B8G8R8_SRGB;
        case kUnityRenderingExtFormatB8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
        case kUnityRenderingExtFormatB8G8R8_UNorm: return VK_FORMAT_B8G8R8_UNORM;
        case kUnityRenderingExtFormatB8G8R8A8_UNorm: return VK_FORMAT_B8G8R8A8_UNORM;
        case kUnityRenderingExtFormatB8G8R8_SNorm: return VK_FORMAT_B8G8R8_SNORM;
        case kUnityRenderingExtFormatB8G8R8A8_SNorm: return VK_FORMAT_B8G8R8A8_SNORM;
        case kUnityRenderingExtFormatB8G8R8_UInt: return VK_FORMAT_B8G8R8_UINT;
        case kUnityRenderingExtFormatB8G8R8A8_UInt: return VK_FORMAT_B8G8R8A8_UINT;
        case kUnityRenderingExtFormatB8G8R8_SInt: return VK_FORMAT_B8G8R8_SINT;
        case kUnityRenderingExtFormatB8G8R8A8_SInt: return VK_FORMAT_B8G8R8A8_SINT;
        case kUnityRenderingExtFormatR4G4B4A4_UNormPack16: return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
        case kUnityRenderingExtFormatB4G4R4A4_UNormPack16: return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
        case kUnityRenderingExtFormatR5G6B5_UNormPack16: return VK_FORMAT_R5G6B5_UNORM_PACK16;
        case kUnityRenderingExtFormatB5G6R5_UNormPack16: return VK_FORMAT_B5G6R5_UNORM_PACK16;
        case kUnityRenderingExtFormatR5G5B5A1_UNormPack16: return VK_FORMAT_R5G5B5A1_UNORM_PACK16;
        case kUnityRenderingExtFormatB5G5R5A1_UNormPack16: return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
        case kUnityRenderingExtFormatA1R5G5B5_UNormPack16: return VK_FORMAT_A1R5G5B5_UNORM_PACK16;
        case kUnityRenderingExtFormatE5B9G9R9_UFloatPack32: return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
        case kUnityRenderingExtFormatB10G11R11_UFloatPack32: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        case kUnityRenderingExtFormatA2B10G10R10_UNormPack32: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case kUnityRenderingExtFormatA2B10G10R10_UIntPack32: return VK_FORMAT_A2B10G10R10_UINT_PACK32;
        case kUnityRenderingExtFormatA2B10G10R10_SIntPack32: return VK_FORMAT_A2B10G10R10_SINT_PACK32;
        case kUnityRenderingExtFormatA2R10G10B10_UNormPack32: return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
        case kUnityRenderingExtFormatA2R10G10B10_UIntPack32: return VK_FORMAT_A2R10G10B10_UINT_PACK32;
        case kUnityRenderingExtFormatA2R10G10B10_SIntPack32: return VK_FORMAT_A2R10G10B10_SINT_PACK32;
        case kUnityRenderingExtFormatA2R10G10B10_XRSRGBPack32:                         // INVALID
        case kUnityRenderingExtFormatA2R10G10B10_XRUNormPack32:                        // INVALID
        case kUnityRenderingExtFormatR10G10B10_XRSRGBPack32:                           // INVALID
        case kUnityRenderingExtFormatR10G10B10_XRUNormPack32:                          // INVALID
        case kUnityRenderingExtFormatA10R10G10B10_XRSRGBPack32:                        // INVALID
        case kUnityRenderingExtFormatA10R10G10B10_XRUNormPack32:                       // INVALID
        case kUnityRenderingExtFormatA8R8G8B8_SRGB:                                    // INVALID
        case kUnityRenderingExtFormatA8R8G8B8_UNorm:                                   // INVALID
        case kUnityRenderingExtFormatA32R32G32B32_SFloat: return VK_FORMAT_UNDEFINED;  // INVALID
        case kUnityRenderingExtFormatD16_UNorm: return VK_FORMAT_D16_UNORM;
        case kUnityRenderingExtFormatD24_UNorm: return VK_FORMAT_UNDEFINED;  // INVALID
        case kUnityRenderingExtFormatD24_UNorm_S8_UInt: return VK_FORMAT_D24_UNORM_S8_UINT;
        case kUnityRenderingExtFormatD32_SFloat: return VK_FORMAT_D32_SFLOAT;
        case kUnityRenderingExtFormatD32_SFloat_S8_UInt: return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case kUnityRenderingExtFormatS8_UInt: return VK_FORMAT_S8_UINT;
        case kUnityRenderingExtFormatRGBA_DXT1_SRGB:                               // case kUnityRenderingExtFormatRGBA_DXT1_UNorm:  // INVALID
        case kUnityRenderingExtFormatRGBA_DXT3_SRGB:                               // INVALID
        case kUnityRenderingExtFormatRGBA_DXT3_UNorm:                              // INVALID
        case kUnityRenderingExtFormatRGBA_DXT5_SRGB:                               // INVALID
        case kUnityRenderingExtFormatRGBA_DXT5_UNorm: return VK_FORMAT_UNDEFINED;  // INVALID
        case kUnityRenderingExtFormatR_BC4_UNorm: return VK_FORMAT_BC4_UNORM_BLOCK;
        case kUnityRenderingExtFormatR_BC4_SNorm: return VK_FORMAT_BC4_SNORM_BLOCK;
        case kUnityRenderingExtFormatRG_BC5_UNorm: return VK_FORMAT_BC5_UNORM_BLOCK;
        case kUnityRenderingExtFormatRG_BC5_SNorm: return VK_FORMAT_BC5_SNORM_BLOCK;
        case kUnityRenderingExtFormatRGB_BC6H_UFloat: return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case kUnityRenderingExtFormatRGB_BC6H_SFloat: return VK_FORMAT_BC6H_SFLOAT_BLOCK;
        case kUnityRenderingExtFormatRGBA_BC7_SRGB: return VK_FORMAT_BC7_SRGB_BLOCK;
        case kUnityRenderingExtFormatRGBA_BC7_UNorm: return VK_FORMAT_BC7_UNORM_BLOCK;
        case kUnityRenderingExtFormatRGB_PVRTC_2Bpp_SRGB: return VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG;
        case kUnityRenderingExtFormatRGB_PVRTC_2Bpp_UNorm: return VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG;
        case kUnityRenderingExtFormatRGB_PVRTC_4Bpp_SRGB: return VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG;
        case kUnityRenderingExtFormatRGB_PVRTC_4Bpp_UNorm: return VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG;
        case kUnityRenderingExtFormatRGBA_PVRTC_2Bpp_SRGB: return VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG;
        case kUnityRenderingExtFormatRGBA_PVRTC_2Bpp_UNorm: return VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG;
        case kUnityRenderingExtFormatRGBA_PVRTC_4Bpp_SRGB: return VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG;
        case kUnityRenderingExtFormatRGBA_PVRTC_4Bpp_UNorm: return VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG;
        case kUnityRenderingExtFormatRGB_ETC_UNorm: return VK_FORMAT_UNDEFINED;  // INVALID
        case kUnityRenderingExtFormatRGB_ETC2_SRGB: return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
        case kUnityRenderingExtFormatRGB_ETC2_UNorm: return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
        case kUnityRenderingExtFormatRGB_A1_ETC2_SRGB: return VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK;
        case kUnityRenderingExtFormatRGB_A1_ETC2_UNorm: return VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
        case kUnityRenderingExtFormatRGBA_ETC2_SRGB: return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
        case kUnityRenderingExtFormatRGBA_ETC2_UNorm: return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
        case kUnityRenderingExtFormatR_EAC_UNorm: return VK_FORMAT_EAC_R11_UNORM_BLOCK;
        case kUnityRenderingExtFormatR_EAC_SNorm: return VK_FORMAT_EAC_R11_SNORM_BLOCK;
        case kUnityRenderingExtFormatRG_EAC_UNorm: return VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
        case kUnityRenderingExtFormatRG_EAC_SNorm: return VK_FORMAT_EAC_R11G11_SNORM_BLOCK;
        case kUnityRenderingExtFormatRGBA_ASTC4X4_SRGB: return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
        case kUnityRenderingExtFormatRGBA_ASTC4X4_UNorm: return VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK;
        case kUnityRenderingExtFormatRGBA_ASTC5X5_SRGB: return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;
        case kUnityRenderingExtFormatRGBA_ASTC5X5_UNorm: return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
        case kUnityRenderingExtFormatRGBA_ASTC6X6_SRGB: return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
        case kUnityRenderingExtFormatRGBA_ASTC6X6_UNorm: return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
        case kUnityRenderingExtFormatRGBA_ASTC8X8_SRGB: return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
        case kUnityRenderingExtFormatRGBA_ASTC8X8_UNorm: return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
        case kUnityRenderingExtFormatRGBA_ASTC10X10_SRGB: return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;
        case kUnityRenderingExtFormatRGBA_ASTC10X10_UNorm: return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
        case kUnityRenderingExtFormatRGBA_ASTC12X12_SRGB: return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;
        case kUnityRenderingExtFormatRGBA_ASTC12X12_UNorm: return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;
        case kUnityRenderingExtFormatYUV2:
        case kUnityRenderingExtFormatRGBA_ASTC4X4_UFloat:
        case kUnityRenderingExtFormatRGBA_ASTC5X5_UFloat:
        case kUnityRenderingExtFormatRGBA_ASTC6X6_UFloat:
        case kUnityRenderingExtFormatRGBA_ASTC8X8_UFloat:
        case kUnityRenderingExtFormatRGBA_ASTC10X10_UFloat:
        case kUnityRenderingExtFormatRGBA_ASTC12X12_UFloat:
        default: return VK_FORMAT_UNDEFINED;
    }
}

VkImageView Vulkan::createImageView(VkImage image, const VkFormat format, const VkImageAspectFlags flags) {
    // clang-format off
    const VkImageViewCreateInfo createInfo {
      .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext    = nullptr,
      .flags    = 0x0,
      .image    = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format   = format,
      .components = {
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
      },
      .subresourceRange = {
        .aspectMask     = flags,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
      },
    };
    // clang-format on

    VkImageView view{VK_NULL_HANDLE};
    m_vkCreateImageView(graphicsInterface->Instance().device, &createInfo, nullptr, &view);
    return view;
}

void Vulkan::destroyImageView(VkImageView viewToDestroy) {
    if (viewToDestroy != VK_NULL_HANDLE) {
        m_vkDestroyImageView(graphicsInterface->Instance().device, viewToDestroy, nullptr);
        viewToDestroy = VK_NULL_HANDLE;
    }
}
#endif