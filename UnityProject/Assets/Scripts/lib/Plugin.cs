using System;
using System.Runtime.InteropServices;
using UnityEngine.Device;
using UnityEngine.Experimental.Rendering;

public static class Plugin
{
    private const byte ErrorTypeOffset = 29;
    private const byte ErrorCodeOffset = 16;
    private const uint ErrorRecoverable = 1;

    public enum UpscalerStatus : uint
    {
        Success = 0U,
        NoUpscalerSet = 2U,
        HardwareError = 1U << ErrorTypeOffset,
        HardwareErrorDeviceExtensionsNotSupported = HardwareError | (1U << ErrorCodeOffset),
        HardwareErrorDeviceNotSupported = HardwareError | (2U << ErrorCodeOffset),
        SoftwareError = 2U << ErrorTypeOffset,
        SoftwareErrorInstanceExtensionsNotSupported = SoftwareError | (1U << ErrorCodeOffset),
        SoftwareErrorDeviceDriversOutOfDate = SoftwareError | (2U << ErrorCodeOffset),
        SoftwareErrorOperatingSystemNotSupported = SoftwareError | (3U << ErrorCodeOffset),

        SoftwareErrorInvalidWritePermissions =
            SoftwareError | (4U << ErrorCodeOffset), // Should be marked as recoverable?
        SoftwareErrorFeatureDenied = SoftwareError | (5U << ErrorCodeOffset),
        SoftwareErrorOutOfGPUMemory = SoftwareError | (6U << ErrorCodeOffset) | ErrorRecoverable,

        /// This likely indicates that a segfault has happened or is about to happen. Abort and avoid the crash if at all possible.
        SoftwareErrorCriticalInternalError = SoftwareError | (7U << ErrorCodeOffset),

        /// The safest solution to handling this error is to stop using the upscaler. It may still work, but all guarantees are void.
        SoftwareErrorCriticalInternalWarning = SoftwareError | (8U << ErrorCodeOffset),

        /// This is an internal error that may have been caused by the user forgetting to call some function. Typically one or more of the initialization functions.
        SoftwareErrorRecoverableInternalWarning = SoftwareError | (9U << ErrorCodeOffset) | ErrorRecoverable,
        SettingsError = (3U << ErrorTypeOffset) | ErrorRecoverable,
        SettingsErrorInvalidInputResolution = SettingsError | (1U << ErrorCodeOffset),
        SettingsErrorInvalidSharpnessValue = SettingsError | (2U << ErrorCodeOffset),
        SettingsErrorUpscalerNotAvailable = SettingsError | (3U << ErrorCodeOffset),
        SettingsErrorQualityModeNotAvailable = SettingsError | (4U << ErrorCodeOffset),

        /// A GENERIC_ERROR_* is thrown when a most likely cause has been found but it is not certain. A plain GENERIC_ERROR is thrown when there are many possible known errors.
        GenericError = 4U << ErrorTypeOffset,
        GenericErrorDeviceOrInstanceExtensionsNotSupported = GenericError | (1U << ErrorCodeOffset),
        UnknownError = 0xFFFFFFFE
    }

    public static bool Success(UpscalerStatus status) => status <= UpscalerStatus.NoUpscalerSet;

    public static bool Failure(UpscalerStatus status) => status > UpscalerStatus.NoUpscalerSet;

    public static bool Recoverable(UpscalerStatus status) => ((uint)status & ErrorRecoverable) == ErrorRecoverable;

    public static bool NonRecoverable(UpscalerStatus status) => ((uint)status & ErrorRecoverable) != ErrorRecoverable;

    public enum Mode
    {
        None,
        DLSS
    }

    public enum Quality
    {
        Auto,
        UltraQuality,
        Quality,
        Balanced,
        Performance,
        UltraPerformance,
        DynamicAuto,
        DynamicManual
    }

    public enum Event
    {
        Upscale
    }

    public static GraphicsFormat MotionFormat() => GraphicsFormat.R16G16_SFloat;
    public static GraphicsFormat ColorFormat(bool HDRActive) => SystemInfo.GetGraphicsFormat(HDRActive ? DefaultFormat.HDR : DefaultFormat.LDR);
    public static GraphicsFormat DepthFormat() => SystemInfo.GetGraphicsFormat(DefaultFormat.DepthStencil);

    public delegate void InternalErrorCallback(IntPtr data, UpscalerStatus er, IntPtr p);

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetError")]
    public static extern UpscalerStatus GetError(Mode mode);

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetErrorMessage")]
    public static extern IntPtr GetErrorMessage(Mode mode);

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetCurrentError")]
    public static extern UpscalerStatus GetCurrentError();

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetCurrentErrorMessage")]
    public static extern IntPtr GetCurrentErrorMessage();

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_ResetHistory")]
    public static extern void ResetHistory();

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetDepthBuffer")]
    public static extern void SetDepthBuffer(IntPtr handle, GraphicsFormat format);

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetInputColor")]
    public static extern void SetInputColor(IntPtr handle, GraphicsFormat format);

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetMotionVectors")]
    public static extern void SetMotionVectors(IntPtr handle, GraphicsFormat format);

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetOutputColor")]
    public static extern void SetOutputColor(IntPtr handle, GraphicsFormat format);

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_InitializePlugin")]
    public static extern void Initialize();

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetUpscaler")]
    public static extern UpscalerStatus SetUpscaler(Mode mode);

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetFramebufferSettings")]
    public static extern UpscalerStatus SetFramebufferSettings(uint width, uint height, Quality quality, bool hdr);

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetRecommendedInputResolution")]
    public static extern ulong GetRecommendedInputResolution();

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetMinimumInputResolution")]
    public static extern ulong GetMinimumInputResolution();

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetMaximumInputResolution")]
    public static extern ulong GetMaximumInputResolution();

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetSharpnessValue")]
    public static extern UpscalerStatus SetSharpnessValue(float sharpness);

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCurrentInputResolution")]
    public static extern UpscalerStatus SetCurrentInputResolution(uint width, uint height);

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_Prepare")]
    public static extern UpscalerStatus Prepare();

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_Shutdown")]
    public static extern void Shutdown();

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_ShutdownPlugin")]
    public static extern void ShutdownPlugin();

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetRenderingEventCallback")]
    public static extern IntPtr GetRenderingEventCallback();

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetJitterInformation")]
    public static extern void SetJitterInformation(float x, float y);

    [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_InitializePlugin")]
    public static extern void Initialize(IntPtr upscalerObject, InternalErrorCallback errorCallback);
}