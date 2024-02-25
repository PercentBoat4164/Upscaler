using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using Conifer.Upscaler.Scripts.impl;
using JetBrains.Annotations;
using UnityEngine;
using UnityEngine.Rendering;
#if UPSCALER_USE_URP
using UnityEngine.Rendering.Universal;
#endif

namespace Conifer.Upscaler.Scripts
{
    public class Settings
    {
        public enum Upscaler
        {
            None,
            DLSS,
        }

        public enum Quality
        {
            Auto,
            Quality,
            Balanced,
            Performance,
            UltraPerformance
        }

        public struct Resolution
        {
            [NonSerialized] private int X;
            [NonSerialized] private int Y;

            public Vector2Int ToVector2Int() => new() { x = X, y = Y };

            public Resolution(int x, int y)
            {
                X = x;
                Y = y;
            }
        }

        public struct Jitter
        {
            [NonSerialized] private float X;
            [NonSerialized] private float Y;

            public Vector2 ToVector2() => new() { x = X, y = Y };
        }
        
        public struct CameraInfo
        {
            public CameraInfo(Camera camera)
            {
                _farPlane = camera.farClipPlane;
                _nearPlane = camera.nearClipPlane;
                _verticalFOV = camera.fieldOfView;
            }

            private float _farPlane;
            private float _nearPlane;
            private float _verticalFOV;
        }
        
        public class PerFeatureSettings
        {
            // Require new feature
            [NonSerialized] public Quality quality = Quality.Auto;
            [NonSerialized] public Upscaler upscaler = Upscaler.DLSS;

            public PerFeatureSettings Copy() => new() { quality = quality, upscaler = upscaler };
            
            public static bool operator ==(PerFeatureSettings self, PerFeatureSettings other)
            {
                return self.quality == other.quality && self.upscaler == other.upscaler;
            }

            public static bool operator !=(PerFeatureSettings self, PerFeatureSettings other) => !(self == other);

            public override bool Equals(object obj) => (PerFeatureSettings)obj == this;

            public override int GetHashCode()
            {
                return HashCode.Combine(quality, upscaler);
            }
        }

        [NonSerialized] public PerFeatureSettings FeatureSettings = new();
        [NonSerialized] public float Sharpness;
        internal static float FrameTime => Time.deltaTime * 1000;
        
        public Settings Copy() => new() { FeatureSettings = FeatureSettings.Copy(), Sharpness = Sharpness };

        public static bool operator ==(Settings self, Settings other)
        {
            return self.Sharpness == other.Sharpness && self.FeatureSettings == other.FeatureSettings;
        }

        public static bool operator !=(Settings self, Settings other) => !(self == other);

        public override bool Equals(object obj) => (Settings)obj == this;
        
        public override int GetHashCode()
        {
            return HashCode.Combine(FeatureSettings, Sharpness);
        }
    }

    [RequireComponent(typeof(Camera))]
    public class Upscaler : MonoBehaviour
    {
        private const byte ErrorTypeOffset = 29;
        private const byte ErrorCodeOffset = 16;
        private const uint ErrorRecoverable = 1;

        public enum Status : uint
        {
            Success                                            =                  0U,
            NoUpscalerSet                                      =                  2U,
            HardwareError                                      =                  1U << ErrorTypeOffset,
            HardwareErrorDeviceExtensionsNotSupported          = HardwareError | (1U << ErrorCodeOffset),
            HardwareErrorDeviceNotSupported                    = HardwareError | (2U << ErrorCodeOffset),
            SoftwareError                                      =                  2U << ErrorTypeOffset,
            SoftwareErrorInstanceExtensionsNotSupported        = SoftwareError | (1U << ErrorCodeOffset),
            SoftwareErrorDeviceDriversOutOfDate                = SoftwareError | (2U << ErrorCodeOffset),
            SoftwareErrorOperatingSystemNotSupported           = SoftwareError | (3U << ErrorCodeOffset),
            SoftwareErrorInvalidWritePermissions               = SoftwareError | (4U << ErrorCodeOffset),
            SoftwareErrorFeatureDenied                         = SoftwareError | (5U << ErrorCodeOffset),
            SoftwareErrorOutOfGPUMemory                        = SoftwareError | (6U << ErrorCodeOffset)  | ErrorRecoverable,
            SoftwareErrorOutOfSystemMemory                     = SoftwareError | (7U << ErrorCodeOffset)  | ErrorRecoverable,
            /// This likely indicates that a segfault has happened or is about to happen. Abort and avoid the crash if at all possible.
            SoftwareErrorCriticalInternalError                 = SoftwareError | (8U << ErrorCodeOffset),
            /// The safest solution to handling this error is to stop using the upscaler. It may still work, but all guarantees are void.
            SoftwareErrorCriticalInternalWarning               = SoftwareError | (9U << ErrorCodeOffset),
            /// This is an internal error that may have been caused by the user forgetting to call some function. Typically one or more of the initialization functions.
            SoftwareErrorRecoverableInternalWarning            = SoftwareError | (10U << ErrorCodeOffset) | ErrorRecoverable,
            SettingsError                                      =                 (3U << ErrorTypeOffset)  | ErrorRecoverable,
            SettingsErrorInvalidInputResolution                = SettingsError | (1U << ErrorCodeOffset),
            SettingsErrorInvalidOutputResolution               = SettingsError | (2U << ErrorCodeOffset),
            SettingsErrorInvalidSharpnessValue                 = SettingsError | (3U << ErrorCodeOffset),
            SettingsErrorUpscalerNotAvailable                  = SettingsError | (4U << ErrorCodeOffset),
            SettingsErrorQualityModeNotAvailable               = SettingsError | (5U << ErrorCodeOffset),
            /// A GENERIC_ERROR_* is thrown when a most likely cause has been found but it is not certain. A plain GENERIC_ERROR is thrown when there are many possible known errors.
            GenericError                                       =                  4U << ErrorTypeOffset,
            GenericErrorDeviceOrInstanceExtensionsNotSupported = GenericError  | (1U << ErrorCodeOffset),
            UnknownError                                       =                  0xFFFFFFFF              & ~ErrorRecoverable
        }

        public static bool Success(Status status) => status <= Status.NoUpscalerSet;
        public static bool Failure(Status status) => status > Status.NoUpscalerSet;
        public static bool Recoverable(Status status) => ((uint)status & ErrorRecoverable) == ErrorRecoverable;
        public static bool NonRecoverable(Status status) => ((uint)status & ErrorRecoverable) != ErrorRecoverable;

        // Camera
        private Camera _camera;
        private bool _hdr;

        // Resolutions
        public Vector2Int OutputResolution { get; private set; }
        public Vector2Int RenderingResolution { get; private set; }

        internal UpscalingData UpscalingData;
        internal Plugin Plugin;
        private Builtin _brp;

        // Upscaler preparer command buffer
        private CommandBuffer _upscalerPrepare;

        internal Settings settings = new();

        // Strictly code accessible endpoints

        /// Callback function that will run when an error is encountered by the upscaler.
        /// It will be passed status (which contains error information) and a more detailed error message string.
        /// This function allows developers to determine what should happen when upscaler encounters an error
        /// This function is called the frame after an error, and it's changes take effect that frame
        /// If the same error is encountered during multiple frames, the function is only called for the first frame
        [NonSerialized][CanBeNull] public Action<Status, string> ErrorCallback;

        /// The current UpscalerStatus
        /// Contains Error Information for Settings Errors or Internal Problems
        public Status status;

        /// Removes history so that artifacts from previous frames are not left over in DLSS
        /// Should be called every time the scene sees a complete scene change
        public void ResetHistory() => Plugin.ResetHistory();

        /// Readonly list of device/OS supported Upscalers
        private IList<Settings.Upscaler> SupportedUpscalerModes { get; set; }
        
        /// Returns true if the device and operating system support the given upscaling mode
        /// Returns false if device and OS do not support Upscaling Mode
        public bool DeviceSupportsUpscalerMode(Settings.Upscaler mode) => SupportedUpscalerModes.Contains(mode);

        private Vector2Int GetResolution()
        {
            return new Vector2Int(_camera.pixelWidth, _camera.pixelHeight);
        }
        
        public Settings QuerySettings() => settings.Copy();

        public Status ApplySettings(Settings newSettings, bool force = false)
        {
            if (!force && settings == newSettings) return status;
            if (Application.isPlaying)
            {
                _hdr = _camera.allowHDR;
                OutputResolution = GetResolution();
                status = Plugin.SetPerFeatureSettings(new Settings.Resolution(OutputResolution.x, OutputResolution.y),
                    newSettings.FeatureSettings.upscaler, newSettings.FeatureSettings.quality, _hdr);
                if (Failure(status)) return status;

                RenderingResolution = Plugin.GetRecommendedResolution().ToVector2Int();
                if (newSettings.FeatureSettings.upscaler != Settings.Upscaler.None && RenderingResolution == new Vector2Int(0, 0)) return status;
                
                UpscalingData.ManageColorTarget(Plugin, newSettings.FeatureSettings.upscaler, RenderingResolution);

                Graphics.ExecuteCommandBuffer(_upscalerPrepare);
                status = Plugin.GetStatus();
                if (Failure(status)) return status;

                var mipBias = (float)Math.Log((float)RenderingResolution.x / OutputResolution.x, 2f) - 1f;
                if (newSettings.FeatureSettings.upscaler == Settings.Upscaler.None)
                {
                    mipBias = 0;
                    _camera.ResetProjectionMatrix();
                }
                
                foreach (var renderer in FindObjectsByType<Renderer>(FindObjectsInactive.Include, FindObjectsSortMode.None))
                foreach (var mat in renderer.materials)
                foreach (var texID in mat.GetTexturePropertyNameIDs())
                {
                    var tex = mat.GetTexture(texID);
                    if (tex is not null) tex.mipMapBias = mipBias;
                }
            }

            settings = newSettings;

            return status;
        }
        
        private void HandleError(Status reason, string message)
        {
            Debug.LogError(reason + " | " + message);
            Debug.Log("The default Upscaler error handler reset the current Upscaler to None.");
            settings.FeatureSettings.upscaler = Settings.Upscaler.None;
            ApplySettings(settings, true);
        }
        
        public void OnEnable()
        {
            // Check for badness causing mistakes in configuration
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
                    case > 1:
                        Debug.LogError("There can only be one UpscalerRendererFeature per Renderer.",
                            GetComponent<Camera>());
                        break;
                    case 0:
                        Debug.LogError("There must be at least one UpscalerRendererFeature in this camera's Renderer.",
                            GetComponent<Camera>());
                        break;
                }
            }
#endif

            if (GetComponents<Upscaler>().Length > 1)
                Debug.LogError("Only one Upscaler script may be attached to a camera at a time.",
                    GetComponent<Camera>());


            // Set up camera
            _camera = GetComponent<Camera>();
            _camera.depthTextureMode |= DepthTextureMode.MotionVectors | DepthTextureMode.Depth;

            Plugin = new Plugin(_camera);

            _upscalerPrepare = new CommandBuffer();
            _upscalerPrepare.name = "Prepare upscaler";
            Plugin.Prepare(_upscalerPrepare);

            _brp = new Builtin();

            // Prepare the Upscaling data
            UpscalingData = new UpscalingData();

            SupportedUpscalerModes = Enum.GetValues(typeof(Settings.Upscaler)).Cast<Settings.Upscaler>()
                .Where(Plugin.IsSupported).ToList().AsReadOnly();
            if (!DeviceSupportsUpscalerMode(settings.FeatureSettings.upscaler))
                settings.FeatureSettings.upscaler = Settings.Upscaler.None;
        }

        protected internal void OnPreCull()
        {
            if (!Application.isPlaying) return;
            // Handle Errors
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

            // Ensure images are up-to-date
            if (OutputResolution != GetResolution() || _camera.allowHDR != _hdr)
            {
                status = ApplySettings(settings, true);
                if (Failure(status)) return;
            }
            
            // Render
            status = Plugin.SetPerFrameData(Settings.FrameTime, settings.Sharpness, new Settings.CameraInfo(_camera));
            if (Failure(status)) return;

            if (settings.FeatureSettings.upscaler == Settings.Upscaler.None) return;
            
            var pixelSpaceJitter = Plugin.GetJitter().ToVector2();
            var clipSpaceJitter = -pixelSpaceJitter / RenderingResolution * 2;
            _camera.ResetProjectionMatrix();
            var tempProj = _camera.projectionMatrix;
            tempProj.m02 += clipSpaceJitter.x;
            tempProj.m12 += clipSpaceJitter.y;
            _camera.projectionMatrix = tempProj;
        }

        protected void OnPreRender()
        {
            if (Application.isPlaying && settings.FeatureSettings.upscaler != Settings.Upscaler.None)
                _brp.PrepareRendering(this);
        }

        protected void OnPostRender()
        {
            if (Application.isPlaying && settings.FeatureSettings.upscaler != Settings.Upscaler.None)
                _brp?.Upscale(this);
        }

        protected void OnDisable()
        {
            _brp.Shutdown();
            UpscalingData?.Release();
        }
    }
}