#pragma once

#include <vulkan/vulkan.h>

#include <nvsdk_ngx_defs.h>
#include <nvsdk_ngx_helpers_vk.h>
#include <nvsdk_ngx_params.h>

#include <string>
#include <vector>

using ExtensionGroup = std::vector<std::string>;

class Plugin {
public:
    static bool                          DLSSSupported;
    static NVSDK_NGX_Handle             *DLSS;
    static NVSDK_NGX_Parameter          *parameters;
    static NVSDK_NGX_VK_DLSS_Eval_Params evalParameters;
    static NVSDK_NGX_Resource_VK         depthBufferResource;
    static VkCommandPool                 _oneTimeSubmitCommandPool;
    static VkCommandBuffer               _oneTimeSubmitCommandBuffer;
    static bool                          _oneTimeSubmitRecording;

    static void prepareForOneTimeSubmits();

    static VkCommandBuffer beginOneTimeSubmitRecording();

    static void endOneTimeSubmitRecording();

    static void cancelOneTimeSubmitRecording();

    static void finishOneTimeSubmits();

    class Settings {
    public:
    struct Resolution {
        unsigned int width;
        unsigned int height;
    };

    static Resolution                  renderResolution;
    static Resolution                  dynamicMaximumRenderResolution;
    static Resolution                  dynamicMinimumRenderResolution;
    static Resolution                  presentResolution;
    static float                       sharpness;
    static bool                        lowResolutionMotionVectors;
    static bool                        HDR;
    static bool                        invertedDepth;
    static bool                        DLSSSharpening;
    static bool                        DLSSAutoExposure;
    static NVSDK_NGX_PerfQuality_Value DLSSQuality;

    static void setPresentResolution(Resolution t_presentResolution);

    static NVSDK_NGX_DLSS_Create_Params getDLSSCreateParams();

    static void useOptimalSettings();
    };
};