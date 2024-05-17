/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

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
     * settings.upscaler = Settings.Upscaler.DLSS;
     * upscaler.ApplySettings(settings);
     * </code></example>
     */
    [Serializable]
    public class Settings
    {
        /**
         * The upscalers that Upscaler supports.
         */
        [Serializable]
        public enum Upscaler
        {
            /// The dummy upscaler.
            None,
            /// NVIDIA's Deep Learning Super Sampling upscaler.
            DeepLearningSuperSampling,
        }

        /**
         * The quality modes that the upscalers support.
         */
        [Serializable]
        public enum Quality
        {
            /// Automatically choose based on output resolution.
            Auto,
            /// Render at native resolution. Only use DLSS for antialiasing.
            AntiAliasing,
            /// Render at 44.4% resolution. Recommended for lower resolution outputs.
            Quality,
            /// Render at 31% resolution. Recommended for most scenarios.
            Balanced,
            /// Render at 25% resolution. Recommended for high resolution outputs.
            Performance,
            /// Render at 11.1% resolution. Recommended for very high (8K+) resolution outputs.
            UltraPerformance
        }

        /**
         * Passes special instructions to DLSS telling how to deal with your application's idiosyncrasies.
         */
        [Serializable]
        public enum DLSSPreset
        {
            /// Default DLSS behavior. Recommended in most scenarios.
            Default,
            /// Prefers to preserve pixel history. Recommended for slowly changing content.
            Stable,
            /// Prefers to discard pixel history. Recommended for quickly changing content.
            FastPaced,
            /// Handles objects with missing or incorrect motion vectors best. Recommended for applications
            /// that cannot generate all the required motion vectors.
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

        /// The current quality mode. Defaults to <see cref="Settings.Quality.Auto"/>.
        public Quality quality;
        /// The current upscaler. Defaults to <see cref="Settings.Upscaler.None"/>.
        public Upscaler upscaler;
        /// The current DLSS preset. Defaults to <see cref="Settings.DLSSPreset.Default"/>.
        public DLSSPreset DLSSpreset;
        /// The current sharpness value. This should always be in the range of 0 to 1. Defaults to 0.
        public float sharpness;
        /// Enables displaying the Rendering Area overlay.
        public bool showRenderingAreaOverlay;
        internal static float FrameTime => Time.deltaTime * 1000;

        /**
         * <summary>Copies all values into a new instance.</summary>
         * 
         * <returns>A deep copy of this <see cref="Settings"/> object.</returns>
         */
        public Settings Copy() => new() { quality = quality, upscaler = upscaler, DLSSpreset = DLSSpreset, sharpness = sharpness, showRenderingAreaOverlay = showRenderingAreaOverlay };

        public static bool operator ==(Settings self, Settings other)
        {
            if (self is not null && other is not null)
                return self.quality == other.quality && self.upscaler == other.upscaler && self.DLSSpreset == other.DLSSpreset && Equals(self.sharpness, other.sharpness) && self.showRenderingAreaOverlay == other.showRenderingAreaOverlay;
            return false;
        }

        public static bool operator !=(Settings self, Settings other) => !(self == other);

        public override bool Equals(object obj) => (Settings)obj == this;

        public override int GetHashCode() => HashCode.Combine(quality, upscaler, DLSSpreset, sharpness, showRenderingAreaOverlay);
    }

    /**
     * The unified interface used to interact with the different upscalers. It may only be put on a
     * <see cref="UnityEngine.Camera"/> object.
     */
    [RequireComponent(typeof(Camera))]
    public class Upscaler : MonoBehaviour
    {
        private const byte ErrorTypeOffset = 29;
        private const byte ErrorCodeOffset = 16;
        private const uint ErrorRecoverable = 1;

        /**
         * The possible <see cref="Status"/> values that can be reported by an upscaler. See
         * <see cref="Upscaler.Success"/>, <see cref="Upscaler.Failure"/>, and <see cref="Upscaler.Recoverable"/> to
         * extract information from a particular <see cref="Status"/>.
         */
        public enum Status : uint
        {
            /// The success signal sent by DLSS.
            Success                                            =                  0U,
            /// The success signal sent by the dummy upscaler.
            NoUpscalerSet                                      =                  2U,
            /// A generic hardware level error. It should be treated as fatal for the upscaler that reports it
            HardwareError                                      =                  1U << ErrorTypeOffset,
            /// The GPU does not support the required Vulkan device extensions.
            HardwareErrorDeviceExtensionsNotSupported          = HardwareError | (1U << ErrorCodeOffset),
            /// The GPU does not support the upscaler that reports this.
            HardwareErrorDeviceNotSupported                    = HardwareError | (2U << ErrorCodeOffset),
            /// A generic system level error. It should be treated as fatal for the upscaler that reports it.
            SoftwareError                                      =                  2U << ErrorTypeOffset,
            /// This system does not support the required Vulkan instance extensions.
            SoftwareErrorInstanceExtensionsNotSupported        = SoftwareError | (1U << ErrorCodeOffset),
            /// The GPU drivers are out of date. Tell the user to update them.
            SoftwareErrorDeviceDriversOutOfDate                = SoftwareError | (2U << ErrorCodeOffset),
            /// This operating system is not supported. This is a permanently fatal error for the upscaler that reports this.
            SoftwareErrorOperatingSystemNotSupported           = SoftwareError | (3U << ErrorCodeOffset),
            /// The application is running in an environment that does not allow it to write to disk.
            SoftwareErrorInvalidWritePermissions               = SoftwareError | (4U << ErrorCodeOffset),
            /// NVIDIA has denied this application the DLSS feature.
            SoftwareErrorFeatureDenied                         = SoftwareError | (5U << ErrorCodeOffset),
            /// Not enough free VRAM exists to perform the requested operation. 
            SoftwareErrorOutOfGPUMemory                        = SoftwareError | (6U << ErrorCodeOffset)  | ErrorRecoverable,
            /// Not enough free RAM exists to perform the requested operation.
            SoftwareErrorOutOfSystemMemory                     = SoftwareError | (7U << ErrorCodeOffset)  | ErrorRecoverable,
            /// This likely indicates that a segfault has happened or is about to happen. Abort and avoid the crash if at all possible.
            SoftwareErrorCriticalInternalError                 = SoftwareError | (8U << ErrorCodeOffset),
            /// The safest solution to handling this error is to stop using the upscaler. It may still work, but all guarantees are void.
            SoftwareErrorCriticalInternalWarning               = SoftwareError | (9U << ErrorCodeOffset),
            /// This is an internal error that may have been caused by the user forgetting to call some function. Typically one or more of the initialization functions.
            SoftwareErrorRecoverableInternalWarning            = SoftwareError | (10U << ErrorCodeOffset) | ErrorRecoverable,
            /// A generic settings level error. This is a recoverable error for the upscaler that reports it. Change the settings and try again.
            SettingsError                                      =                 (3U << ErrorTypeOffset)  | ErrorRecoverable,
            /// The input resolution is unusable by the current upscaler. See the accompanying status message for more information.
            SettingsErrorInvalidInputResolution                = SettingsError | (1U << ErrorCodeOffset),
            /// The output resolution is unusable by the current upscaler. See the accompanying status message for more information.
            SettingsErrorInvalidOutputResolution               = SettingsError | (2U << ErrorCodeOffset),
            /// The sharpness value is unusable by the current upscaler. See the accompanying status message for more information.
            SettingsErrorInvalidSharpnessValue                 = SettingsError | (3U << ErrorCodeOffset),
            /// The quality mode is unusable by the current upscaler. See the accompanying status message for more information.
            SettingsErrorQualityModeNotAvailable               = SettingsError | (4U << ErrorCodeOffset),
            /// An invalid <see cref="Settings.Upscaler"/> was provided.
            SettingsErrorUpscalerNotAvailable                  = SettingsError | (5U << ErrorCodeOffset),
            /// The preset is unusable by DLSS. See the accompanying status message for more information.
            SettingsErrorPresetNotAvailable                    = SettingsError | (6U << ErrorCodeOffset),
            /// A GenericError* is thrown when a most likely cause has been found but it is not certain. A plain GenericError is thrown when there are many possible known errors.
            GenericError                                       =                  4U << ErrorTypeOffset,
            /// Either the GPU does not support the required Vulkan device extensions or the system does not support the required Vulkan instance extensions.
            GenericErrorDeviceOrInstanceExtensionsNotSupported = GenericError  | (1U << ErrorCodeOffset),
            /// The cause of the error is unknown. Treat this as fatal for the upscaler that reports it.
            UnknownError                                       =                  0xFFFFFFFF              & ~ErrorRecoverable
        }

        /**
         * <summary>Does a <see cref="Status"/> represent a success state?</summary>
         *
         * <param name="status">The status in question.</param>
         *
         * <returns>true if the <see cref="Status"/> is success-y and false if not.</returns>
         *
         * <remarks>This returns true only for <see cref="Status.Success"/> and <see cref="Status.NoUpscalerSet"/>.
         * Being a <see cref="Success"/> <see cref="Status"/> means that the upscaler in question is usable in its
         * current state.</remarks>
         */
        public static bool Success(Status status) => status <= Status.NoUpscalerSet;

        /**
         * <summary>Does a <see cref="Status"/> represent a fail state?</summary>
         *
         * <param name="status">The status in question.</param>
         *
         * <returns>true if the <see cref="Status"/> is an error and false if not.</returns>
         *
         * <remarks>This returns true for all <see cref="Status"/>es that are not <see cref="Status.Success"/> or
         * <see cref="Status.NoUpscalerSet"/>. Being a <see cref="Failure"/> <see cref="Status"/> means that the
         * upscaler in question is not usable in its current state. See <see cref="Recoverable"/> to determine if fixing
         * the problem is possible.</remarks>
         */
        public static bool Failure(Status status) => status > Status.NoUpscalerSet;

        /**
         * <summary>Is a <see cref="Status"/> recoverable from?</summary>
         *
         * <param name="status">The status to determine the recoverability of.</param>
         *
         * <returns>true if the <see cref="Status"/> is recoverable and false if not.</returns>
         *
         * <remarks><see cref="Success"/> <see cref="Status"/>es are not recoverable. Being <see cref="Recoverable"/>
         * means that it is possible to put the upscaler back into a <see cref="Success"/> state. Non-recoverable
         * <see cref="Status"/>es are fatal for the upscaler that reports them.</remarks>
         */
        public static bool Recoverable(Status status) => ((uint)status & ErrorRecoverable) == ErrorRecoverable;

        private Camera _camera;
        private float _currentRenderScale;
        private CommandBuffer _upscalerPrepare;

        internal NativeInterface NativeInterface;
        [SerializeField] internal Settings settings;

        /// The current output resolution. This will be updated to match the camera's
        /// <see cref="UnityEngine.Camera.pixelRect"/> whenever it resizes. Upscaler does not control the output
        /// resolution but rather adapts to whatever output resolution Unity requests.
        public Vector2Int OutputResolution { get; private set; }

        /// The current resolution at which the scene is being rendered. This is determined either by dynamic
        /// resolution, or by querying the upscaler. It is never (0, 0). When the upscaler has a <see cref="Status"/>
        /// error or the upscaler is <see cref="Conifer.Upscaler.Settings.Upscaler.None"/> it will be the same as
        /// <see cref="OutputResolution"/>.
        public Vector2Int RenderingResolution { get; private set; }

        /// The largest per-axis fraction of <see cref="OutputResolution"/> that dynamic resolution is allowed to use.
        /// It defaults to 1.
        public float MaxRenderScale { get; private set; } = 1f;

        /// The smallest per-axis fraction of <see cref="OutputResolution"/> that dynamic resolution is allowed to use.
        /// It defaults to 0.5.
        public float MinRenderScale { get; private set; } = .5f;

        /// The current scaling that the upscaler is performing. See <see cref="MinRenderScale"/>, and
        /// <see cref="MaxRenderScale"/> for the minimum and maximum allowed values respectively at any given time.
        /// Attempting to set this to anything outside of this range will result in the incoming value being clamped.
        public float CurrentRenderScale
        {
            get => _currentRenderScale;
            set => _currentRenderScale = Math.Clamp(value, MinRenderScale, MaxRenderScale);
        }

        /**
         * <summary>The callback used to handle any errors that the upscaler throws.</summary>
         *
         * <param name="Status">The <see cref="Status"/> that the upscaler errored with.</param>
         * <param name="string">A plain English message describing the nature of the issue.</param>
         *
         * <remarks>This callback is only ever called if an error occurs. When that happens it will be called from the
         * <see cref="Update"/> method during the next frame. If this callback fails to bring the upscaler's
         * <see cref="Status"/> back to a <see cref="Success"/> value, then the default error handler will reset the
         * upscaler to the default <see cref="Conifer.Upscaler.Settings.Upscaler.None"/>.</remarks>
         *
         * <example><code>upscaler.ErrorCallback = (status, message) => { };</code></example>
         */
        [NonSerialized][CanBeNull] public Action<Status, string> ErrorCallback;

        /// The current <see cref="Status"/>.
        public Status status;

        /**
         * <summary>Tells the upscaler to reset the pixel history this frame.</summary>
         *
         * <remarks>This method is fast. It will set a flag that tells the upscaler to reset the pixel history this frame.
         * This flag is automatically cleared at the end of each frame. This should be only called everytime there is no
         * correlation between what the camera saw last frame and what it sees this frame.</remarks>
         *
         * <example><code>
         * CameraJumpCut();
         * upscaler.ResetHistory();
         * </code></example>
         */
        public void ResetHistory() => NativeInterface.ResetHistory();
        
        /**
         * <summary>Check if an upscaler is supported in the current environment.</summary>
         *
         * <param name="mode">The upscaler to query support for.</param>
         *
         * <returns>`true` if the upscaler is supported and `false` if it is not.</returns>
         *
         * <remarks>This method is slow the first time it is used for each upscaler, then fast every time after that.
         * Support for the upscaler requested is computed then cached. Any future calls with the same upscaler will use
         * the cached value.</remarks>
         *
         * <example><code>bool DLSSSupported = Upscaler.IsSupported(Settings.Upscaler.DeepLearningSuperSampling);</code>
         * </example>
         */
        public static bool IsSupported(Settings.Upscaler mode) => NativeInterface.IsSupported(mode);

        /**
         * <summary>Check if the GfxPluginUpscaler shared library has been loaded.</summary>
         *
         * <returns>`true` if the GfxPluginUpscaler shared library has been loaded by Unity and false if it has not
         * been.</returns>
         *
         * <example><code>bool nativePluginLoaded = Upscaler.PluginLoaded();</code></example>
         */
        public static bool PluginLoaded() => NativeInterface.Loaded;

        /**
         * <summary>Query Upscaler's current settings.</summary>
         *
         * <returns>A copy of the settings currently in use.</returns>
         *
         * <remarks>This method is fast. It can be used to check a specific setting, or used in conjunction with
         * <see cref="ApplySettings"/> to change a specific setting while leaving all others the same. The value
         * returned by this method may change every time <see cref="ApplySettings"/> is called.</remarks>
         *
         * <example><code>Upscaler.Settings currentSettings = upscaler.QuerySettings();</code></example>
         */
        public Settings QuerySettings() => settings is null ? new Settings() : settings.Copy();

        /**
         * <summary>Push the new settings to the upscaler.</summary>
         *
         * <param name="newSettings">The new settings to apply.</param>
         * <param name="force">Should the settings be applied even if they are the same as before?</param>
         *
         * <returns>The <see cref="Upscaler.Status"/> of the upscaler after attempting to apply settings.</returns>
         *
         * <remarks>This method is very slow when the settings have changed, or if force is set to true. When that is
         * the case it will update the <see cref="OutputResolution"/>, <see cref="RenderingResolution"/>,
         * <see cref="MaxRenderScale"/>, and <see cref="MinRenderScale"/> based on queries of the new upscaler. It then
         * resets the camera's projection matrix if jittering should be disabled, and  updates the
         * <see cref="Texture.mipMapBias"/> value for all textures in materials that are in renderers before setting the
         * internal settings variable to a copy of new one.</remarks>
         *
         * <example><code>upscaler.ApplySettings(settings);</code></example>
         */
        public Status ApplySettings(Settings newSettings, bool force = false)
        {
            if (!force && settings == newSettings) return status;
            if (Application.isPlaying)
            {
                _camera.ResetProjectionMatrix();
                OutputResolution = new Vector2Int(_camera.pixelWidth, _camera.pixelHeight);
                status = NativeInterface.SetPerFeatureSettings(new Settings.Resolution(OutputResolution.x, OutputResolution.y), newSettings.upscaler, newSettings.DLSSpreset, newSettings.quality, false);
                if (Failure(status)) return status;

                Graphics.ExecuteCommandBuffer(_upscalerPrepare);
                status = NativeInterface.GetStatus();
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

        private void PreUpscale(ScriptableRenderContext context, Camera _)
        {
            if (!Application.isPlaying) return;
            if (settings.upscaler == Settings.Upscaler.None) return;

            status = NativeInterface.SetPerFrameData(Settings.FrameTime, settings.sharpness, new Settings.CameraInfo(_camera));
            if (Failure(status)) return;

            EnforceDynamicResolutionConstraints(null);

            _camera.ResetProjectionMatrix();
            if (FrameDebugger.enabled) return;
            var pixelSpaceJitter = NativeInterface.GetJitter(true).ToVector2();
            var clipSpaceJitter = -pixelSpaceJitter / RenderingResolution * 2;
            var temporaryProjectionMatrix = _camera.projectionMatrix;
            temporaryProjectionMatrix.m02 += clipSpaceJitter.x;
            temporaryProjectionMatrix.m12 += clipSpaceJitter.y;
            var projectionMatrix = _camera.projectionMatrix;
            _camera.nonJitteredProjectionMatrix = projectionMatrix;
            _camera.projectionMatrix = temporaryProjectionMatrix;
            _camera.useJitteredProjectionMatrixForTransparentRendering = true;
        }
        
        protected void OnEnable()
        {
            _camera = GetComponent<Camera>();
            _camera.depthTextureMode |= DepthTextureMode.MotionVectors | DepthTextureMode.Depth;

            settings ??= new Settings();
            NativeInterface = new NativeInterface();
            _upscalerPrepare = new CommandBuffer();
            _upscalerPrepare.name = "Prepare Upscaler";
            NativeInterface.Prepare(_upscalerPrepare);

            if (!IsSupported(settings.upscaler)) settings.upscaler = Settings.Upscaler.None;
            ApplySettings(settings, true);
            RenderPipelineManager.beginCameraRendering += PreUpscale;
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

            if (OutputResolution.x != _camera.pixelWidth || OutputResolution.y != _camera.pixelHeight)
                status = ApplySettings(settings, true);
        }

        private void OnDisable()
        {
            EnforceDynamicResolutionConstraints(1.0f);
            RenderPipelineManager.beginCameraRendering -= PreUpscale;
        }

        private void OnGUI()
        {
            if (settings.showRenderingAreaOverlay)
                GUI.Box(new Rect(0, 0, RenderingResolution.x, RenderingResolution.y), "Rendering Area");
        }
    }
}