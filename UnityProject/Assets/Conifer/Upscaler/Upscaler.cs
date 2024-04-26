/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

using System;
using System.Linq;
using System.Reflection;
using Conifer.Upscaler.impl;
using JetBrains.Annotations;
using UnityEditor;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
#if UPSCALER_USE_URP
using Conifer.Upscaler.URP;
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
            DLSS,
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
            DLAA,
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
            /// that cannot generate all of the required motion vectors.
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
        internal static float FrameTime => Time.deltaTime * 1000;

        /**
         * <summary>Copies all values into a new instance.</summary>
         * 
         * <returns>A deep copy of this <see cref="Settings"/> object.</returns>
         */
        public Settings Copy() => new() { quality = quality, upscaler = upscaler, DLSSpreset = DLSSpreset, sharpness = sharpness };

        public static bool operator ==(Settings self, Settings other)
        {
            if (self is not null && other is not null)
                return self.quality == other.quality && self.upscaler == other.upscaler && self.DLSSpreset == other.DLSSpreset && Equals(self.sharpness, other.sharpness);
            return false;
        }

        public static bool operator !=(Settings self, Settings other) => !(self == other);

        public override bool Equals(object obj) => (Settings)obj == this;

        public override int GetHashCode() => HashCode.Combine(quality, upscaler, DLSSpreset, sharpness);
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

        internal static readonly int MotionID = Shader.PropertyToID("_CameraMotionVectorsTexture");
        internal static readonly int SourceDepthID = Shader.PropertyToID("_CameraDepthTexture");

        internal Camera Camera;

        internal UpscalingData UpscalingData;
        internal Plugin Plugin;
        [SerializeField] internal Settings settings;

        private CommandBuffer _upscalerPrepare;

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
        /// This only applies when the upscaler is not <see cref="Conifer.Upscaler.Settings.Upscaler.None"/>.
        /// This has a private setter. It defaults to 1.
        public float MaxRenderScale { get; private set; } = 1f;

        /// The smallest per-axis fraction of <see cref="OutputResolution"/> that dynamic resolution is allowed to use.
        /// This only applies when the upscaler is not <see cref="Conifer.Upscaler.Settings.Upscaler.None"/>.
        /// This has a private setter. It defaults to 0.5.
        public float MinRenderScale { get; private set; } = .5f;

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
        public void ResetHistory() => Plugin.ResetHistory();
        
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
         * <example><code>bool DLSSSupported = Upscaler.Supported(Settings.Upscaler.DLSS);</code></example>
         */
        public static bool Supported(Settings.Upscaler mode) => Plugin.IsSupported(mode);

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
         * <remarks>This method is very slow when it the settings have changed, or force is set to true. When that is
         * the case it will update the <see cref="OutputResolution"/>, <see cref="RenderingResolution"/>,
         * <see cref="MaxRenderScale"/>, and <see cref="MinRenderScale"/> based on queries of the new upscaler. It then
         * updates internally used images, resets the camera's projection matrix if jittering should be disabled, and
         * finally it updates the <see cref="Texture.mipMapBias"/> value for all textures in materials that are in
         * renderers before setting the internal settings variable to a copy of new one.</remarks>
         *
         * <example><code>upscaler.ApplySettings(settings);</code></example>
         */
        public Status ApplySettings(Settings newSettings, bool force = false)
        {
            if (!force && settings == newSettings) return status;
            if (Application.isPlaying)
            {
                OutputResolution = new Vector2Int(Camera.pixelWidth, Camera.pixelHeight);
                status = Plugin.SetPerFeatureSettings(new Settings.Resolution(OutputResolution.x, OutputResolution.y), newSettings.upscaler, newSettings.DLSSpreset, newSettings.quality, false);
                if (Failure(status)) return status;

                RenderingResolution = Plugin.GetRecommendedResolution().ToVector2Int();
                if (newSettings.upscaler != Settings.Upscaler.None)
                {
                    var resolution = Plugin.GetMaximumResolution().ToVector2Int();
                    MaxRenderScale = (float)Math.Ceiling(Math.Min((float)resolution.x / OutputResolution.x, (float)resolution.y / OutputResolution.y) * 20) / 20;
                    resolution = Plugin.GetMinimumResolution().ToVector2Int();
                    MinRenderScale = (float)Math.Ceiling(Math.Max((float)resolution.x / OutputResolution.x, (float)resolution.y / OutputResolution.y) * 20) / 20;
                } else {
                    MaxRenderScale = 1f;
                    MinRenderScale = .5f;
                }

                if (newSettings.upscaler != Settings.Upscaler.None && RenderingResolution == new Vector2Int(0, 0))
                {
                    RenderingResolution = OutputResolution;
                    return status;
                }

                UpscalingData.ManageColorTargets(SystemInfo.GetGraphicsFormat(DefaultFormat.LDR), newSettings.upscaler,
                    RenderingResolution, OutputResolution);

                Graphics.ExecuteCommandBuffer(_upscalerPrepare);
                status = Plugin.GetStatus();
                if (Failure(status)) return status;

                var mipBias = (float)Math.Log((float)RenderingResolution.x / OutputResolution.x, 2f) - 1f;
                if (newSettings.upscaler == Settings.Upscaler.None)
                {
                    mipBias = -.5f;
                    Camera.ResetProjectionMatrix();
                }
                
                foreach (var renderer in FindObjectsByType<Renderer>(FindObjectsInactive.Include, FindObjectsSortMode.None))
                foreach (var mat in renderer.materials)
                foreach (var texID in mat.GetTexturePropertyNameIDs())
                {
                    var tex = mat.GetTexture(texID);
                    if (tex is not null) tex.mipMapBias = mipBias;
                }
            }

            settings = newSettings.Copy();
            return status;
        }
        
        private void HandleError(Status reason, string message)
        {
            Debug.LogWarning(reason + " | " + message);
            settings.upscaler = Settings.Upscaler.None;
            ApplySettings(settings, true);
        }
        
        protected void OnEnable()
        {
#if UPSCALER_USE_URP
            if (GraphicsSettings.renderPipelineAsset is not null)
            {
                var features = ((ScriptableRendererData[])typeof(UniversalRenderPipelineAsset)
                    .GetField("m_RendererDataList", BindingFlags.NonPublic | BindingFlags.Instance)!
                    .GetValue(UniversalRenderPipeline.asset))[(int)typeof(UniversalRenderPipelineAsset)
                    .GetField("m_DefaultRendererIndex", BindingFlags.NonPublic | BindingFlags.Instance)!
                    .GetValue(UniversalRenderPipeline.asset)].rendererFeatures;
                var upscalerFeatures = features.Where(feature => feature is UpscalerRendererFeature).ToArray();
                switch (upscalerFeatures.Length)
                {
                    case > 1: Debug.LogError("There can only be one UpscalerRendererFeature per Renderer.", GetComponent<Camera>());
                        break;
                    case 0: Debug.LogError("There must be at least one UpscalerRendererFeature in this camera's Renderer.", GetComponent<Camera>());
                        break;
                }
            }
#endif

            if (GetComponents<Upscaler>().Length > 1) Debug.LogError("Only one Upscaler script may be attached to a camera at a time.", GetComponent<Camera>());

            settings ??= new Settings();

            Camera = GetComponent<Camera>();
            Camera.depthTextureMode |= DepthTextureMode.MotionVectors | DepthTextureMode.Depth;

            Plugin = new Plugin();

            _upscalerPrepare = new CommandBuffer();
            _upscalerPrepare.name = "Prepare upscaler";
            Plugin.Prepare(_upscalerPrepare);
            
            UpscalingData = new UpscalingData();
            
            if (!Supported(settings.upscaler)) settings.upscaler = Settings.Upscaler.None;
        }

        protected void Update()
        {
            if (!Application.isPlaying) return;
            status = Plugin.GetStatus();
            if (Failure(status))
            {
                if (ErrorCallback is null) HandleError(status, Plugin.GetStatusMessage());
                else
                {
                    ErrorCallback(status, Plugin.GetStatusMessage());
                    status = Plugin.GetStatus();
                    if (Failure(status))
                    {
                        Debug.LogError("The registered error handler failed to rectify the following error.");
                        HandleError(status, Plugin.GetStatusMessage());
                    }
                }
            }

            if (OutputResolution.x == Camera.pixelWidth && OutputResolution.y == Camera.pixelHeight)
                return;
            
            status = ApplySettings(settings, true);
        }

        protected internal void OnPreCull()
        {
            if (!EditorApplication.isPlaying || settings.upscaler == Settings.Upscaler.None) return;

            // if (_dynamicResolution)
            // {
            //     var scale = 0f;
            //     if (Equals(ScalableBufferManager.widthScaleFactor, ScalableBufferManager.heightScaleFactor))
            //         scale = Math.Min(ScalableBufferManager.widthScaleFactor, ScalableBufferManager.heightScaleFactor);
            //     if (ScalableBufferManager.widthScaleFactor > MaxRenderScale || ScalableBufferManager.heightScaleFactor > MaxRenderScale)
            //         scale = MaxRenderScale;
            //     if (ScalableBufferManager.widthScaleFactor < MinRenderScale || ScalableBufferManager.heightScaleFactor < MinRenderScale)
            //         scale = MinRenderScale;
            //     if (scale != 0f)
            //         ScalableBufferManager.ResizeBuffers(scale, scale);
            //     RenderingResolution = new Vector2Int((int)(OutputResolution.x * ScalableBufferManager.widthScaleFactor),
            //         (int)(OutputResolution.y * ScalableBufferManager.heightScaleFactor));
            // }

            ((UniversalRenderPipelineAsset)GraphicsSettings.currentRenderPipeline).renderScale = Math.Clamp(
                (float)RenderingResolution.x / OutputResolution.x, MinRenderScale, MaxRenderScale);

            status = Plugin.SetPerFrameData(Settings.FrameTime, settings.sharpness, new Settings.CameraInfo(Camera));
            if (Failure(status)) return;

            // if (!_dynamicResolution)
            //     Camera.rect = new Rect(0, 0, (float)RenderingResolution.x / OutputResolution.x,
            //         (float)RenderingResolution.y / OutputResolution.y);

            Camera.ResetProjectionMatrix();
            if (FrameDebugger.enabled) return;
            var pixelSpaceJitter = Plugin.GetJitter(true).ToVector2();
            var clipSpaceJitter = -pixelSpaceJitter / RenderingResolution * 2;
            var tempProj = Camera.projectionMatrix;
            tempProj.m02 += clipSpaceJitter.x;
            tempProj.m12 += clipSpaceJitter.y;
            Camera.projectionMatrix = tempProj;
        }

        protected void OnRenderImage(RenderTexture source, RenderTexture destination)
        {
            if (!Application.isPlaying || settings.upscaler == Settings.Upscaler.None)
            {
                Graphics.Blit(source, destination);
                return;
            }

            Camera.rect = new Rect(0, 0, OutputResolution.x, OutputResolution.y);
            var outputColor = destination && destination.enableRandomWrite? destination : UpscalingData.OutputColorTarget;
            var upscale = new CommandBuffer();
            upscale.name = "Upscale";
            Plugin.Upscale(upscale, source, Shader.GetGlobalTexture(SourceDepthID), Shader.GetGlobalTexture(MotionID), outputColor);
            if (outputColor != destination)
                upscale.Blit(outputColor.colorBuffer, destination);

            Graphics.ExecuteCommandBuffer(upscale);
            Graphics.SetRenderTarget(destination);
        }

        protected void OnDisable() => UpscalingData?.Release();
    }
}