/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

/**************************************************
 * Upscaler v1.0.0                                *
 * See the OfflineManual.pdf for more information *
 **************************************************/

using System;
using JetBrains.Annotations;
using UnityEngine;
using UnityEngine.Rendering;
#if UPSCALER_USE_URP
using UnityEngine.Rendering.Universal;
#endif

namespace Conifer.Upscaler
{
    /**
     * <summary>Holds all of the information that can be adjusted to tell Upscaler how to perform the upscaling.</summary>
     *
     * <remarks>Obtain an instance of this class using <see cref="Conifer.Upscaler.Upscaler.QuerySettings"/>.
     * Check the values, or make your changes then apply them using
     * <see cref="Conifer.Upscaler.Upscaler.ApplySettings"/>.</remarks>
     *
     * <example><code>
     * Settings settings = upscaler.QuerySettings();
     * settings.upscaler = Settings.Upscaler.DeepLearningSuperSampling;
     * upscaler.ApplySettings(settings);
     * </code></example>
     */
    [Serializable]
    public class Settings
    {
        /**
         * The <see cref="Settings.Upscaler"/>s that Upscaler supports.
         */
        [Serializable]
        public enum Upscaler
        {
            /// The <see cref="None"/> <see cref="Settings.Upscaler"/>. Use this to turn off upscaling.
            None,
            /// NVIDIA's Deep Learning Super Sampling upscaler.
            DeepLearningSuperSampling,
            /// AMD's FidelityFX Super Resolution upscaler.
            FidelityFXSuperResolution2,
            /// Intel's Xe Super Sampling upscaler.
            XeSuperSampling
        }

        /**
         * The <see cref="Settings.Quality"/> modes that the <see cref="Settings.Upscaler"/>s support. Query support for a particular mode using <see cref="Conifer.Upscaler.Upscaler.IsSupported(Conifer.Upscaler.Settings.Quality)"/> and <see cref="Conifer.Upscaler.Upscaler.IsSupported(Conifer.Upscaler.Settings.Upscaler, Conifer.Upscaler.Settings.Quality)"/>
         */
        [Serializable]
        public enum Quality
        {
            /// Automatically choose another <see cref="Settings.Quality"/> mode based on output resolution. Available to all <see cref="Upscaler"/>s.
            Auto,
            /// Render at native resolution and use the <see cref="Settings.Upscaler"/> for antialiasing.
            AntiAliasing,
            /// Recommended for extremely low resolution outputs. Only available when using the <see cref="Upscaler.XeSuperSampling"/> <see cref="Upscaler"/>.
            UltraQualityPlus,
            /// Recommended for very low resolution outputs. Only available when using the <see cref="Upscaler.XeSuperSampling"/> <see cref="Upscaler"/>.
            UltraQuality,
            /// Recommended for lower resolution outputs.
            Quality,
            /// Recommended in most scenarios.
            Balanced,
            /// Recommended for high resolution outputs.
            Performance,
            /// Recommended for very high (8K+) resolution outputs.
            UltraPerformance
        }

        /**
         * Passes special instructions to DLSS telling it how to deal with your application's idiosyncrasies.
         */
        [Serializable]
        public enum DLSSPreset
        {
            /// Default DLSS behavior. Recommended in most scenarios.
            Default,
            /// Prefers to preserve pixel history. Recommended for slowly changing content. (same as <see cref="Default"/>)
            Stable,
            /// Prefers to discard pixel history. Recommended for quickly changing content.
            FastPaced,
            /// Handles objects with missing or incorrect motion vectors best. Recommended for applications that cannot generate all required motion vectors.
            AntiGhosting,
        }

        internal readonly struct Resolution
        {
            private readonly int _x;
            private readonly int _y;

            public Vector2Int ToVector2Int() => new() { x = _x, y = _y };

            public Resolution(int x, int y)
            {
                _x = x;
                _y = y;
            }
        }

        internal readonly struct Jitter
        {
            private readonly float _x;
            private readonly float _y;

            public Vector2 ToVector2() => new() { x = _x, y = _y };
        }

        internal readonly struct CameraInfo
        {
            public CameraInfo(Camera camera)
            {
                _farPlane = camera.farClipPlane;
                _nearPlane = camera.nearClipPlane;
                _verticalFOV = camera.fieldOfView;
            }

            private readonly float _farPlane;
            private readonly float _nearPlane;
            private readonly float _verticalFOV;
        }

        /// The current <see cref="Settings.Quality"/> mode. Defaults to <see cref="Settings.Quality.Auto"/>.
        public Quality quality;
        /// The current <see cref="Settings.Upscaler"/>. Defaults to <see cref="Settings.Upscaler.None"/>.
        public Upscaler upscaler;
        /// The current <see cref="Settings.DLSSPreset"/>. Defaults to <see cref="Settings.DLSSPreset.Default"/>. Only used when <see cref="Settings.upscaler"/> is <see cref="Settings.Upscaler.DeepLearningSuperSampling"/>.
        public DLSSPreset DLSSpreset;
        /// The current sharpness value. This should always be in the range of <c>0.0</c> to <c>1.0</c>. Defaults to <c>0.0</c>. Only used when <see cref="Settings.upscaler"/> is <see cref="Settings.Upscaler.FidelityFXSuperResolution2"/>.
        public float sharpness;
        /// Instructs Upscaler to set <see cref="Settings.Upscaler.FidelityFXSuperResolution2"/> parameters for automatic reactive mask generation. Defaults to <c>true</c>. Only used when <see cref="Settings.upscaler"/> is <see cref="Settings.Upscaler.FidelityFXSuperResolution2"/>.
        public bool useReactiveMask;
        /// Setting this value too small will cause visual instability. Larger values can cause ghosting. Defaults to <c>0.05f</c>. Only used when <see cref="Settings.upscaler"/> is <see cref="Settings.Upscaler.FidelityFXSuperResolution2"/>.
        public float tcThreshold = 0.05f;
        /// Value used to scale transparency and composition mask after generation. Smaller values increase stability at hard edges of translucent objects. Defaults to <c>1.0f</c>. Only used when <see cref="Settings.upscaler"/> is <see cref="Settings.Upscaler.FidelityFXSuperResolution2"/>.
        public float tcScale = 1.0f;
        /// Value used to scale reactive mask after generation. Larger values result in more reactive pixels. Defaults to <c>5.0f</c>. Only used when <see cref="Settings.upscaler"/> is <see cref="Settings.Upscaler.FidelityFXSuperResolution2"/>.
        public float reactiveScale = 5.0f;
        /// Maximum value reactivity can reach. AMD recommends values of 0.9f and below. Defaults to <c>0.9f</c>. Only used when <see cref="Settings.upscaler"/> is <see cref="Settings.Upscaler.FidelityFXSuperResolution2"/>.
        public float reactiveMax = 0.9f;
        internal static float FrameTime => Time.deltaTime * 1000;

        /**
         * <summary>Copies all values into a new instance.</summary>
         * 
         * <returns>A deep copy of this <see cref="Settings"/> object.</returns>
         */
        public Settings Copy() => new() { quality = quality, upscaler = upscaler, DLSSpreset = DLSSpreset, sharpness = sharpness, useReactiveMask = useReactiveMask, tcThreshold = tcThreshold, tcScale = tcScale, reactiveScale = reactiveScale, reactiveMax = reactiveMax };

        public bool RequiresUpdateFrom(Settings other) => quality != other.quality || upscaler != other.upscaler || DLSSpreset != other.DLSSpreset;
    }

    /**
     * The unified interface used to interact with the different <see cref="Settings.Upscaler"/>s. It may only be put on a
     * <see cref="UnityEngine.Camera"/> object.
     */
    [RequireComponent(typeof(Camera))]
    public class Upscaler : MonoBehaviour
    {
        private const byte ErrorTypeOffset = 29;
        private const byte ErrorCodeOffset = 16;
        private const uint ErrorRecoverable = 1;

        /**
         * The possible <see cref="Status"/> values that can be reported by an <see cref="Settings.Upscaler"/>. See
         * <see cref="Upscaler.Success"/>, <see cref="Upscaler.Failure"/>, and <see cref="Upscaler.Recoverable"/> to
         * extract information from a particular <see cref="Status"/>.
         */
        public enum Status : uint
        {
            /// The success signal sent by non-<see cref="Settings.Upscaler.None"/> <see cref="Settings.Upscaler"/>s.
            Success                                            =                  0U,
            /// The success signal sent by the <see cref="Settings.Upscaler.None"/> <see cref="Settings.Upscaler"/>.
            NoUpscalerSet                                      =                  2U,
            /// A generic hardware level error. It should be treated as fatal for the <see cref="Settings.Upscaler"/> that reports it
            HardwareError                                      =                  1U << ErrorTypeOffset,
            /// The GPU does not support the required Vulkan device extensions.
            HardwareErrorDeviceExtensionsNotSupported          = HardwareError | (1U << ErrorCodeOffset),
            /// The GPU does not support the <see cref="Settings.Upscaler"/> that reports this.
            HardwareErrorDeviceNotSupported                    = HardwareError | (2U << ErrorCodeOffset),
            /// A generic system level error. It should be treated as fatal for the <see cref="Settings.Upscaler"/> that reports it.
            SoftwareError                                      =                  2U << ErrorTypeOffset,
            /// This system does not support the required Vulkan instance extensions.
            SoftwareErrorInstanceExtensionsNotSupported        = SoftwareError | (1U << ErrorCodeOffset),
            /// The GPU drivers are out of date. Tell the user to update them.
            SoftwareErrorDeviceDriversOutOfDate                = SoftwareError | (2U << ErrorCodeOffset),
            /// This operating system is not supported. This is a permanently fatal error for the <see cref="Settings.Upscaler"/> that reports this.
            SoftwareErrorOperatingSystemNotSupported           = SoftwareError | (3U << ErrorCodeOffset),
            /// The application is running in an environment that does not allow it to write to disk.
            SoftwareErrorInvalidWritePermissions               = SoftwareError | (4U << ErrorCodeOffset),
            /// NVIDIA has denied this application the <see cref="Settings.Upscaler.DeepLearningSuperSampling"/> feature.
            SoftwareErrorFeatureDenied                         = SoftwareError | (5U << ErrorCodeOffset),
            /// Not enough free VRAM exists to perform the requested operation. 
            SoftwareErrorOutOfGPUMemory                        = SoftwareError | (6U << ErrorCodeOffset)  | ErrorRecoverable,
            /// Not enough free RAM exists to perform the requested operation.
            SoftwareErrorOutOfSystemMemory                     = SoftwareError | (7U << ErrorCodeOffset)  | ErrorRecoverable,
            /// The safest solution to handling this error is to stop using the <see cref="Settings.Upscaler"/>. It may still work, but all guarantees are void.
            SoftwareErrorCriticalInternalWarning               = SoftwareError | (8U << ErrorCodeOffset),
            /// This is an internal error that may have been caused by the user forgetting to call some function. Typically one or more of the initialization functions.
            SoftwareErrorRecoverableInternalWarning            = SoftwareError | (9U << ErrorCodeOffset) | ErrorRecoverable,
            /// The current <see cref="Settings.Upscaler"/> does not support the current Graphics API. This error can be avoided by performing a call to <see cref="Upscaler.IsSupported(Conifer.Upscaler.Settings.Upscaler)"/> beforehand.
            SoftwareErrorUnsupportedGraphicsAPI                = SoftwareError | (10U << ErrorCodeOffset),
            /// A generic settings level error. This is a recoverable error for the <see cref="Settings.Upscaler"/> that reports it. Try changing the settings and trying again.
            SettingsError                                      =                 (3U << ErrorTypeOffset)  | ErrorRecoverable,
            /// The input resolution is unusable by the current <see cref="Settings.Upscaler"/>. The input resolution must fall within the bounds set by the <see cref="Settings.Upscaler"/>. Use <see cref="Upscaler.OutputResolution"/> along with <see cref="Upscaler.MinRenderScale"/> and <see cref="Upscaler.MaxRenderScale"/> to compute the lower and upper input resolution bounds respectively. See the accompanying status message for more information.
            SettingsErrorInvalidInputResolution                = SettingsError | (1U << ErrorCodeOffset),
            /// The output resolution is unusable by the current <see cref="Settings.Upscaler"/>. <see cref="Settings.Upscaler.DeepLearningSuperSampling"/> requires at least a 32x32 output resolution. See the accompanying status message for more information.
            SettingsErrorInvalidOutputResolution               = SettingsError | (2U << ErrorCodeOffset),
            /// The sharpness value is unusable by the current <see cref="Settings.Upscaler"/>. Sharpness values must be between <c>0.0f</c> and <c>1.0f</c>. See the accompanying status message for more information.
            SettingsErrorInvalidSharpnessValue                 = SettingsError | (3U << ErrorCodeOffset),
            /// The <see cref="Settings.Quality"/> mode is unusable by the current <see cref="Settings.Upscaler"/>. See the accompanying status message for more information.
            SettingsErrorQualityModeNotAvailable               = SettingsError | (4U << ErrorCodeOffset),
            /// The <see cref="Settings.DLSSPreset"/> is unusable by <see cref="Settings.Upscaler.DeepLearningSuperSampling"/>. See the accompanying status message for more information.
            SettingsErrorPresetNotAvailable                    = SettingsError | (5U << ErrorCodeOffset),
            /// An invalid <see cref="Settings.Upscaler"/> was provided.
            SettingsErrorUpscalerNotAvailable                  = SettingsError | (6U << ErrorCodeOffset),
            /// A GenericError* is thrown when a most likely cause has been found, but it is not certain. A plain <see cref="GenericError"/> is thrown when there are many possible known errors. Threat this as fatal for the <see cref="Settings.Upscaler"/> that reports it.
            GenericError                                       =                  4U << ErrorTypeOffset,
            /// Either the GPU does not support the required Vulkan device extensions or the system does not support the required Vulkan instance extensions. Treat this as fatal for the <see cref="Settings.Upscaler"/> that reports it.
            GenericErrorDeviceOrInstanceExtensionsNotSupported = GenericError  | (1U << ErrorCodeOffset),
            /// The cause of the error is unknown. Treat this as fatal for the <see cref="Settings.Upscaler"/> that reports it.
            UnknownError                                       =                  0xFFFFFFFF              & ~ErrorRecoverable
        }

        /**
         * <summary>Does a <see cref="Status"/> represent a success state?</summary>
         *
         * <param name="status">The <see cref="Status"/> in question.</param>
         *
         * <returns><c>true</c> if the <see cref="Status"/> is success-y and <c>false</c> if not.</returns>
         *
         * <remarks>This returns <c>true</c> only for <see cref="Status.Success"/> and <see cref="Status.NoUpscalerSet"/>.
         * Being a <see cref="Success"/> <see cref="Status"/> means that the <see cref="Settings.Upscaler"/> in question
         * is usable in its current state.</remarks>
         */
        public static bool Success(Status status) => status <= Status.NoUpscalerSet;

        /**
         * <summary>Does a <see cref="Status"/> represent a fail state?</summary>
         *
         * <param name="status">The <see cref="Status"/> in question.</param>
         *
         * <returns><c>true</c> if the <see cref="Status"/> is an error and <c>false</c> if not.</returns>
         *
         * <remarks>This returns <c>true</c> for all <see cref="Status"/>es that are not <see cref="Status.Success"/> or
         * <see cref="Status.NoUpscalerSet"/>. Being a <see cref="Failure"/> <see cref="Status"/> means that the
         * <see cref="Settings.Upscaler"/> in question is not usable in its current state. See <see cref="Recoverable"/>
         * to determine if fixing the problem is possible.</remarks>
         */
        public static bool Failure(Status status) => status > Status.NoUpscalerSet;

        /**
         * <summary>Is a <see cref="Status"/> recoverable from?</summary>
         *
         * <param name="status">The <see cref="Status"/> to determine the recoverability of.</param>
         *
         * <returns><c>true</c> if the <see cref="Status"/> is recoverable and <c>false</c> if not.</returns>
         *
         * <remarks><see cref="Success"/> <see cref="Status"/>es are not recoverable. Being <see cref="Recoverable"/>
         * means that it is possible to put the <see cref="Settings.Upscaler"/> back into a <see cref="Success"/> state.
         * Non-recoverable <see cref="Status"/>es are fatal for the <see cref="Settings.Upscaler"/> that reports them.
         * </remarks>
         */
        public static bool Recoverable(Status status) => ((uint)status & ErrorRecoverable) == ErrorRecoverable;

        /// Enables displaying the Rendering Area overlay. Defaults to <c>false</c>
        public bool showRenderingAreaOverlay;

        /// While this is true Upscaler will call <see cref="ResetHistory"/> every frame.
        public bool forceHistoryResetEveryFrame;

        private Camera _camera;
        private bool _hdr;
        private float _currentRenderScale;

        internal NativeInterface NativeInterface;
        [SerializeField] internal Settings settings;

        /// The current output resolution. This will be updated to match the <see cref="UnityEngine.Camera"/>'s
        /// <see cref="UnityEngine.Camera.pixelRect"/> whenever it resizes. Upscaler does not control the output
        /// resolution but rather adapts to whatever output resolution Unity requests.
        public Vector2Int OutputResolution { get; private set; }

        /// The current resolution at which the scene is being rendered. This is determined either by dynamic
        /// resolution, or by querying the <see cref="Settings.Upscaler"/>. It is never <c>(0, 0)</c>. When the
        /// <see cref="Settings.Upscaler"/> has a <see cref="Status"/> error or the <see cref="Settings.Upscaler"/> is
        /// <see cref="Conifer.Upscaler.Settings.Upscaler.None"/> it will be the same as <see cref="OutputResolution"/>.
        public Vector2Int RenderingResolution { get; private set; }

        /// The largest per-axis fraction of <see cref="OutputResolution"/> that dynamic resolution is allowed to use.
        /// It defaults to <c>1.0f</c> (1x upscaling).
        public float MaxRenderScale { get; private set; } = 1.0f;

        /// The smallest per-axis fraction of <see cref="OutputResolution"/> that dynamic resolution is allowed to use.
        /// It defaults to <c>0.5f</c> (2x upscaling).
        public float MinRenderScale { get; private set; } = 0.5f;

        /// The current scaling that the <see cref="Settings.Upscaler"/> is performing. See <see cref="MinRenderScale"/>
        /// and <see cref="MaxRenderScale"/> for the minimum and maximum allowed values respectively at any given time.
        /// Attempting to set this to anything outside of that range will result in the incoming value being clamped.
        public float CurrentRenderScale
        {
            get => _currentRenderScale;
            set => _currentRenderScale = Math.Clamp(value, MinRenderScale, MaxRenderScale);
        }

        /**
         * <summary>The callback used to handle any errors that the <see cref="Settings.Upscaler"/> throws.</summary>
         *
         * <param name="Status">The <see cref="Status"/> that the <see cref="Settings.Upscaler"/> errored with.</param>
         * <param name="string">A plain English message describing the nature of the issue.</param>
         *
         * <remarks>This callback is only ever called if an error occurs. When that happens it will be called from the
         * <see cref="Update"/> method during the next frame. If this callback fails to bring the
         * <see cref="Settings.Upscaler"/>'s <see cref="Status"/> back to a <see cref="Success"/> value, then the
         * default error handler will reset the current <see cref="Settings.Upscaler"/> to the default
         * <see cref="Conifer.Upscaler.Settings.Upscaler.None"/>.</remarks>
         *
         * <example><code>upscaler.ErrorCallback = (status, message) => { };</code></example>
         */
        [NonSerialized][CanBeNull] public Action<Status, string> ErrorCallback;

        /// The current <see cref="Status"/> for the managed <see cref="Settings.Upscaler"/>.
        public Status status;

        /**
         * <summary>Tells the <see cref="Settings.Upscaler"/> to reset the pixel history this frame.</summary>
         *
         * <remarks>This method is fast. It will set a flag that tells the <see cref="Settings.Upscaler"/> to reset the
         * pixel history this frame.This flag is automatically cleared at the end of each frame. This should be only
         * called everytime there is no correlation between what the camera saw last frame and what it sees this frame.
         * </remarks>
         *
         * <example><code>
         * CameraJumpCut(newLocation);
         * upscaler.ResetHistory();
         * </code></example>
         */
        public void ResetHistory() => NativeInterface.ResetHistory();
        
        /**
         * <summary>Check if an <see cref="Settings.Upscaler"/> is supported in the current environment.</summary>
         *
         * <param name="type">The <see cref="Settings.Upscaler"/> to query support for.</param>
         *
         * <returns><c>true</c> if the <see cref="Settings.Upscaler"/> is supported and <c>false</c> if it is not.
         * </returns>
         *
         * <remarks>This method is slow the first time it is used for each <see cref="Settings.Upscaler"/>, then fast
         * every time after that. Support for the <see cref="Settings.Upscaler"/> requested is computed then cached. Any
         * future calls with the same <see cref="Settings.Upscaler"/> will use the cached value.</remarks>
         *
         * <example><code>bool DLSSSupported = Upscaler.IsSupported(Settings.Upscaler.DeepLearningSuperSampling);</code>
         * </example>
         */
        public static bool IsSupported(Settings.Upscaler type) => NativeInterface.IsSupported(type);

        /**
         * <summary>Check if a <see cref="Settings.Quality"/> mode is supported by a given
         * <see cref="Settings.Upscaler"/>.</summary>
         *
         * <param name="type">The <see cref="Settings.Upscaler"/> to query.</param>
         * <param name="mode">The <see cref="Settings.Quality"/> mode to query support for.</param>
         *
         * <returns><c>true</c> if the <see cref="Settings.Upscaler"/> supports the requested
         * <see cref="Settings.Quality"/> mode and <c>false</c> if it does not.</returns>
         *
         * <remarks>This method is always fast. Every non-<see cref="Settings.Upscaler.None"/>
         * <see cref="Settings.Upscaler"/> will return <c>true</c> for the <see cref="Settings.Quality.Auto"/>
         * <see cref="Settings.Quality"/> mode.</remarks>
         *
         * <example><code>bool supportsAA = upscaler.IsSupported(Settings.Quality.AntiAliasing);</code></example>
         */
        public static bool IsSupported(Settings.Upscaler type, Settings.Quality mode) => NativeInterface.IsSupported(type, mode);

        /**
         * <summary>Check if a <see cref="Settings.Quality"/> mode is supported by the current
         * <see cref="Settings.Upscaler"/>.</summary>
         *
         * <param name="mode">The <see cref="Settings.Quality"/> mode to query the current
         * <see cref="Settings.Upscaler"/> for support of.</param>
         *
         * <returns><c>true</c> if the current <see cref="Settings.Upscaler"/> supports the requested
         * <see cref="Settings.Quality"/> mode and <c>false</c> if it does not.</returns>
         *
         * <remarks>This method is always fast. This is a convenience method for
         * <see cref="IsSupported(Conifer.Upscaler.Settings.Upscaler, Conifer.Upscaler.Settings.Quality)"/> that uses
         * this upscaler's <see cref="Settings.Upscaler"/> as the first argument. </remarks>
         *
         * <example><code>bool supportsAA = upscaler.IsSupported(Settings.Quality.AntiAliasing);</code></example>
         */
        public bool IsSupported(Settings.Quality mode) => IsSupported(settings.upscaler, mode);

        /**
         * <summary>Check if the <c>GfxPluginUpscaler</c> shared library has been loaded.</summary>
         *
         * <returns><c>true</c> if the <c>GfxPluginUpscaler</c> shared library has been loaded by Unity and <c>false</c> if it
         * has not been.</returns>
         *
         * <example><code>bool nativePluginLoaded = Upscaler.PluginLoaded();</code></example>
         */
        public static bool PluginLoaded() => NativeInterface.Loaded;

        /**
         * <summary>Query <see cref="Upscaler"/>'s current <see cref="Settings"/>.</summary>
         *
         * <returns>A copy of the <see cref="Settings"/> currently in use.</returns>
         *
         * <remarks>This method is fast. It can be used to check a specific setting, or used in conjunction with
         * <see cref="ApplySettings"/> to change a specific setting while leaving all others the same. The value
         * returned by this method <b>may</b> change every time <see cref="ApplySettings"/> is called.</remarks>
         *
         * <example><code>Upscaler.Settings currentSettings = upscaler.QuerySettings();</code></example>
         */
        public Settings QuerySettings() => settings is null ? new Settings() : settings.Copy();

        /**
         * <summary>Push the new <see cref="Settings"/> to the <see cref="Upscaler"/>.</summary>
         *
         * <param name="newSettings">The new <see cref="Settings"/> to apply.</param>
         * <param name="force">Should the <see cref="Settings"/> be applied even if they are the same as before?</param>
         *
         * <returns>The <see cref="Upscaler.Status"/> of the upscaler after attempting to apply settings.</returns>
         *
         * <remarks>This method is very slow when the <see cref="Settings"/> have changed, or if force is set to
         * <c>true</c>. When that is the case it will update the <see cref="OutputResolution"/>,
         * <see cref="RenderingResolution"/>, <see cref="MaxRenderScale"/>, and <see cref="MinRenderScale"/> based on
         * queries of the new <see cref="Settings.Upscaler"/>. It then resets the camera's projection matrix and sets
         * the internal <see cref="Settings"/> variable to a copy of <paramref name="newSettings"/>.</remarks>
         *
         * <example><code>upscaler.ApplySettings(settings);</code></example>
         */
        public Status ApplySettings(Settings newSettings, bool force = false)
        {
            if (Application.isPlaying && (force || settings.RequiresUpdateFrom(newSettings)))
            {
                OutputResolution = new Vector2Int(_camera.pixelWidth, _camera.pixelHeight);
                _hdr = _camera.allowHDR;

                status = NativeInterface.SetPerFeatureSettings(new Settings.Resolution(OutputResolution.x, OutputResolution.y), newSettings.upscaler, newSettings.DLSSpreset, newSettings.quality, newSettings.sharpness, _hdr);
                if (Failure(status)) return status;

                RenderingResolution = NativeInterface.GetRecommendedResolution().ToVector2Int();
                if (newSettings.upscaler != Settings.Upscaler.None)
                {
                    var resolution = NativeInterface.GetMaximumResolution().ToVector2Int();
                    MaxRenderScale = (float)resolution.x / OutputResolution.x;
                    resolution = NativeInterface.GetMinimumResolution().ToVector2Int();
                    MinRenderScale = (float)resolution.x / OutputResolution.x;
                } else {
                    MaxRenderScale = 1f;
                    MinRenderScale = 1f;
                }
                EnforceDynamicResolutionConstraints((float)RenderingResolution.x / OutputResolution.x);
                _camera.ResetProjectionMatrix();
            }

            settings = newSettings.Copy();
            return status;
        }

        private void EnforceDynamicResolutionConstraints(float? scale)
        {
            var asset = (UniversalRenderPipelineAsset)GraphicsSettings.currentRenderPipeline;
            if (scale is not null) CurrentRenderScale = (float)scale;
            else CurrentRenderScale = asset.renderScale;
            asset.renderScale = CurrentRenderScale;
            var resolution = (Vector2)OutputResolution * CurrentRenderScale;
            RenderingResolution = new Vector2Int((int)resolution.x, (int)resolution.y);
        }
        
        protected void OnEnable()
        {
            _camera = GetComponent<Camera>();
            _camera.depthTextureMode |= DepthTextureMode.MotionVectors | DepthTextureMode.Depth;

            settings ??= new Settings();
            NativeInterface = new NativeInterface();

            if (!IsSupported(settings.upscaler)) settings.upscaler = Settings.Upscaler.None;
            ApplySettings(settings, true);
        }

        protected void Update()
        {
            if (!Application.isPlaying) return;
            status = NativeInterface.GetStatus();
            if (Failure(status))
            {
                void HandleError(Status reason, string message)
                {
                    Debug.LogWarning(reason + " | " + message);
                    settings.upscaler = Settings.Upscaler.None;
                    ApplySettings(settings, true);
                }

                if (ErrorCallback is null) HandleError(status, NativeInterface.GetStatusMessage());
                else
                {
                    ErrorCallback(status, NativeInterface.GetStatusMessage());
                    status = NativeInterface.GetStatus();
                    if (Failure(status))
                    {
                        Debug.LogError("The registered error handler failed to rectify the following error.");
                        HandleError(status, NativeInterface.GetStatusMessage());
                    }
                }
            }

            if (OutputResolution.x != _camera.pixelWidth || OutputResolution.y != _camera.pixelHeight || _hdr != _camera.allowHDR)
                status = ApplySettings(settings, true);

            if (settings.upscaler == Settings.Upscaler.None) return;

            if (forceHistoryResetEveryFrame) ResetHistory();

            status = NativeInterface.SetPerFrameData(Settings.FrameTime, settings.sharpness, new Settings.CameraInfo(_camera), settings.useReactiveMask, settings.tcThreshold, settings.tcScale, settings.reactiveScale, settings.reactiveMax);
            if (Failure(status)) return;

            EnforceDynamicResolutionConstraints(null);

            if (FrameDebugger.enabled) return;
            _camera.ResetProjectionMatrix();
            _camera.nonJitteredProjectionMatrix = _camera.projectionMatrix;
            var clipSpaceJitter = NativeInterface.GetJitter(true).ToVector2() / RenderingResolution * 2;
            var projectionMatrix = _camera.projectionMatrix;
            if (_camera.orthographic)
            {
                projectionMatrix.m03 += clipSpaceJitter.x;
                projectionMatrix.m13 += clipSpaceJitter.y;
            }
            else
            {
                projectionMatrix.m02 += -clipSpaceJitter.x;
                projectionMatrix.m12 += -clipSpaceJitter.y;
            }
            _camera.projectionMatrix = projectionMatrix;
            _camera.useJitteredProjectionMatrixForTransparentRendering = true;
        }

        private void OnDisable() => ((UniversalRenderPipelineAsset)GraphicsSettings.currentRenderPipeline).renderScale = 1f;

        private void OnGUI()
        {
            if (settings.upscaler != Settings.Upscaler.None && showRenderingAreaOverlay)
            {
                var scale = ((UniversalRenderPipelineAsset)GraphicsSettings.currentRenderPipeline).renderScale;
                GUI.Box(new Rect(0, 0, RenderingResolution.x, RenderingResolution.y),
                    scale * 100 + "% of pixels rendered per-axis\n" + 100 / (1 / scale * (1 / scale))
                    +"% of total pixels rendered");
            }
        }
    }
}