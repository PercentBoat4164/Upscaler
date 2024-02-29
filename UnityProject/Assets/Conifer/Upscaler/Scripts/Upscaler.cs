using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using Conifer.Upscaler.Scripts.impl;
using JetBrains.Annotations;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
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
            DLAA,
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

        [NonSerialized] public Quality quality = Quality.Auto;
        [NonSerialized] public Upscaler upscaler = Upscaler.DLSS;
        [NonSerialized] public float Sharpness;
        internal static float FrameTime => Time.deltaTime * 1000;

        public Settings Copy() => new() { quality = quality, upscaler = upscaler, Sharpness = Sharpness };

        public static bool operator ==(Settings self, Settings other) => self.quality == other.quality && self.upscaler == other.upscaler && self.Sharpness == other.Sharpness;

        public static bool operator !=(Settings self, Settings other) => !(self == other);

        public override bool Equals(object obj) => (Settings)obj == this;

        public override int GetHashCode() => HashCode.Combine(quality, upscaler, Sharpness);
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
        
        private static readonly int MotionID = Shader.PropertyToID("_CameraMotionVectorsTexture");
        private static readonly int OutputID = Shader.PropertyToID("Upscaler_OutputTexture");

        // Camera
        internal Camera Camera;
        private bool _hdr;

        // Resolutions
        public Vector2Int OutputResolution { get; private set; }
        public Vector2Int RenderingResolution { get; private set; }

        internal UpscalingData UpscalingData;
        internal Plugin Plugin;

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
        
        public Settings QuerySettings() => settings.Copy();

        public Status ApplySettings(Settings newSettings, bool force = false)
        {
            if (!force && settings == newSettings) return status;
            if (Application.isPlaying)
            {
                _hdr = Camera.allowHDR;
                OutputResolution = new Vector2Int(Camera.pixelWidth, Camera.pixelHeight);
                status = Plugin.SetPerFeatureSettings(new Settings.Resolution(OutputResolution.x, OutputResolution.y),
                    newSettings.upscaler, newSettings.quality, _hdr);
                if (Failure(status)) return status;

                RenderingResolution = Plugin.GetRecommendedResolution().ToVector2Int();
                if (newSettings.upscaler != Settings.Upscaler.None && RenderingResolution == new Vector2Int(0, 0)) return status;

                UpscalingData.ManageOutputColorTarget(SystemInfo.GetGraphicsFormat(Camera.allowHDR ? DefaultFormat.HDR : DefaultFormat.LDR), newSettings.upscaler, OutputResolution);
                UpscalingData.ManageSourceDepthTarget(newSettings.upscaler, RenderingResolution);

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

            settings = newSettings;

            return status;
        }
        
        private void HandleError(Status reason, string message)
        {
            Debug.LogWarning(reason + " | " + message);
            Debug.Log("The default Upscaler error handler reset the current Upscaler to None.");
            settings.upscaler = Settings.Upscaler.None;
            ApplySettings(settings, true);
        }
        
        public void OnEnable()
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


            Camera = GetComponent<Camera>();
            Camera.depthTextureMode |= DepthTextureMode.MotionVectors | DepthTextureMode.Depth;

            Plugin = new Plugin(Camera);

            _upscalerPrepare = new CommandBuffer();
            _upscalerPrepare.name = "Prepare upscaler";
            Plugin.Prepare(_upscalerPrepare);
            
            UpscalingData = new UpscalingData();

            SupportedUpscalerModes = Enum.GetValues(typeof(Settings.Upscaler)).Cast<Settings.Upscaler>()
                .Where(Plugin.IsSupported).ToList().AsReadOnly();
            if (!DeviceSupportsUpscalerMode(settings.upscaler))
                settings.upscaler = Settings.Upscaler.None;
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

            if (OutputResolution.x == Camera.pixelWidth && OutputResolution.y == Camera.pixelHeight &&
                Camera.allowHDR == _hdr) return;
            
            status = ApplySettings(settings, true);
        }

        protected internal void OnPreCull()
        {
            if (!Application.isPlaying) return;
            
            status = Plugin.SetPerFrameData(Settings.FrameTime, settings.Sharpness, new Settings.CameraInfo(Camera));
            if (Failure(status)) return;

            if (settings.upscaler == Settings.Upscaler.None) return;

            Camera.rect = new Rect(0, 0, (float)RenderingResolution.x / OutputResolution.x,
                (float)RenderingResolution.y / OutputResolution.y);
            // Camera.rect = new Rect(0, 0, OutputResolution.x * ScalableBufferManager.widthScaleFactor,
            //     OutputResolution.y * ScalableBufferManager.heightScaleFactor);
            
            var pixelSpaceJitter = Plugin.GetJitter(true).ToVector2();
            var clipSpaceJitter = -pixelSpaceJitter / RenderingResolution * 2;
            Camera.ResetProjectionMatrix();
            var tempProj = Camera.projectionMatrix;
            tempProj.m02 += clipSpaceJitter.x;
            tempProj.m12 += clipSpaceJitter.y;
            Camera.projectionMatrix = tempProj;
        }

        /**@todo Find out more about*/
        /// [ImageEffectAfterScale]
        protected void OnRenderImage(RenderTexture source, RenderTexture destination)
        {
            if (!Application.isPlaying || settings.upscaler == Settings.Upscaler.None)
            {
                Graphics.Blit(source, destination);
                return;
            }

            Camera.rect = new Rect(0, 0, OutputResolution.x, OutputResolution.y);
            var outputColor = destination ? destination : UpscalingData.OutputColorTarget;
            var sourceDepth = UpscalingData.SourceDepthTarget;
            var upscale = new CommandBuffer();
            upscale.name = "Upscale";
            var motion = Shader.GetGlobalTexture(MotionID);
            upscale.Blit(source.depthBuffer, sourceDepth.rt.depthBuffer);  // Using CopyTexture here freezes the Editor UI.
            Plugin.Upscale(upscale, source, sourceDepth, motion, outputColor);
            if (!destination)
                upscale.Blit(outputColor.colorBuffer, destination);
            
            Graphics.ExecuteCommandBuffer(upscale);
            Graphics.SetRenderTarget(destination);
        }

        protected void OnDisable() => UpscalingData?.Release();
    }
}