using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using AOT;
using Conifer.Upscaler.Scripts.impl;
using JetBrains.Annotations;
using UnityEngine;
using UnityEngine.Rendering;
#if UPSCALER_USE_URP
using UnityEngine.Rendering.Universal;
#endif

namespace Conifer.Upscaler.Scripts
{
    [RequireComponent(typeof(Camera))]
    public class Upscaler : MonoBehaviour
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
            SoftwareErrorOutOfSystemMemory = SoftwareError | (7U << ErrorCodeOffset) | ErrorRecoverable,

            /// This likely indicates that a segfault has happened or is about to happen. Abort and avoid the crash if at all possible.
            SoftwareErrorCriticalInternalError = SoftwareError | (8U << ErrorCodeOffset),

            /// The safest solution to handling this error is to stop using the upscaler. It may still work, but all guarantees are void.
            SoftwareErrorCriticalInternalWarning = SoftwareError | (9U << ErrorCodeOffset),

            /// This is an internal error that may have been caused by the user forgetting to call some function. Typically one or more of the initialization functions.
            SoftwareErrorRecoverableInternalWarning = SoftwareError | (10U << ErrorCodeOffset) | ErrorRecoverable,
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

        public static bool Success(UpscalerStatus status)
        {
            return status <= UpscalerStatus.NoUpscalerSet;
        }

        public static bool Failure(UpscalerStatus status)
        {
            return status > UpscalerStatus.NoUpscalerSet;
        }

        public static bool Recoverable(UpscalerStatus status)
        {
            return ((uint)status & ErrorRecoverable) == ErrorRecoverable;
        }

        public static bool NonRecoverable(UpscalerStatus status)
        {
            return ((uint)status & ErrorRecoverable) != ErrorRecoverable;
        }

        public enum UpscalerMode
        {
            None,
            DLSS,
        }

        public enum QualityMode
        {
            Auto,

            // UltraQuality,
            Quality,
            Balanced,
            Performance,
            UltraPerformance
        }

        // Camera
        private Camera _camera;

        // Upscaling Resolution
        private Vector2Int UpscalingResolution => _camera
            ? _camera.targetTexture != null
                ? new Vector2Int(_camera.targetTexture.width, _camera.targetTexture.height)
                : new Vector2Int(_camera.pixelWidth, _camera.pixelHeight)
            : new Vector2Int(Display.displays[Display.activeEditorGameViewTarget].renderingWidth,
                Display.displays[Display.activeEditorGameViewTarget].renderingWidth);

        private Vector2Int _lastUpscalingResolution;

        // Rendering Resolution
        private Vector2Int RenderingResolution { get; set; }
        private Vector2Int _lastRenderingResolution;

        // HDR state
        private bool ActiveHDR => _camera.allowHDR;
        private bool _lastHDRActive;

        // Sharpness
        private float _sharpness;
        private float _lastSharpness;

        // Internal Render Pipeline abstraction
        public UpscalingData UpscalingData;
        internal Plugin Plugin;
        private Builtin _brp;

        // Upscaler preparer command buffer
        private CommandBuffer _upscalerPrepare;

        // API
        private UpscalerMode _activeUpscalerMode;
        private UpscalerMode _lastUpscalerMode;
        private QualityMode _activeQualityMode;
        private QualityMode _lastQualityMode;

        private bool DHDR => _lastHDRActive != ActiveHDR;
        private bool DSharpness => _lastSharpness.Equals(_sharpness);
        private bool DUpscalingResolution => _lastUpscalingResolution != UpscalingResolution;
        private bool DUpscaler => _lastUpscalerMode != _activeUpscalerMode;
        private bool DQuality => _lastQualityMode != _activeQualityMode;
        private bool DRenderingResolution => _lastRenderingResolution != RenderingResolution;

        // EXPOSED API FEATURES

        // BASIC UPSCALER OPTIONS
        // Can be set in Editor or in code.

        /// Current Upscaling Mode to Use
        public UpscalerMode upscalerMode = UpscalerMode.DLSS;

        /// Quality / Performance Mode for the Upscaler
        public QualityMode qualityMode = QualityMode.Auto;

        // ADVANCED UPSCALER OPTIONS
        // Can be set in Editor or in code.

        /// Sharpness (Technically DLSS Deprecated); Ranges from 0 to 1
        public float sharpness;

        // Strictly code accessible endpoints

        /// Callback function that will run when an error is encountered by the upscaler.
        /// It will be passed status (which contains error information) and a more detailed error message string.
        /// This function allows developers to determine what should happen when upscaler encounters an error
        /// This function is called the frame after an error, and it's changes take effect that frame
        /// If the same error is encountered during multiple frames, the function is only called for the first frame
        [CanBeNull] public Action<UpscalerStatus, string> ErrorCallback = null;

        /// The current UpscalerStatus
        /// Contains Error Information for Settings Errors or Internal Problems
        public UpscalerStatus Status { get; private set; }

        /// Readonly list of device/OS supported Upscalers
        public IList<UpscalerMode> SupportedUpscalerModes { get; private set; }

        /// Removes history so that artifacts from previous frames are not left over in DLSS
        /// Should be called every time the scene sees a complete scene change
        public void ResetHistoryBuffer() => Plugin.ResetHistory();

        /// Returns true if the device and operating system support the given upscaling mode
        /// Returns false if device and OS do not support Upscaling Mode
        public bool DeviceSupportsUpscalerMode(UpscalerMode mode)
        {
            return SupportedUpscalerModes.Contains(mode);
        }

        // INTERNAL API IMPLEMENTATION

        // Calls base class method to initialize Upscaler and then uses Plugin functions to gather information about device supported options
        public void OnEnable()
        {
            // Check for badness causing mistakes in configuration
#if UPSCALER_USE_URP
            var features = ((ScriptableRendererData[])typeof(UniversalRenderPipelineAsset)
                .GetField("m_RendererDataList", BindingFlags.NonPublic | BindingFlags.Instance)!
                .GetValue(UniversalRenderPipeline.asset))[(int)typeof(UniversalRenderPipelineAsset)
                .GetField("m_DefaultRendererIndex", BindingFlags.NonPublic | BindingFlags.Instance)!
                .GetValue(UniversalRenderPipeline.asset)].rendererFeatures;
            var upscalerFeatures = features.Where(feature => feature is UpscalerRendererFeature).ToArray();
            switch (upscalerFeatures.Count())
            {
                case > 1:
                    Debug.LogError("There can only be one UpscalerRendererFeature per Renderer.", GetComponent<Camera>());
                    break;
                case 0:
                    Debug.LogError("There must be at least one UpscalerRendererFeature in this camera's Renderer.",
                        GetComponent<Camera>());
                    break;
            }
#endif

            if (GetComponents<Upscaler>().Length > 1)
                Debug.LogError("Only one Upscaler script may be attached to a camera at a time.", GetComponent<Camera>());


            // Set up camera
            _camera = GetComponent<Camera>();
            _camera.depthTextureMode |= DepthTextureMode.MotionVectors | DepthTextureMode.Depth;

            Plugin = new Plugin(_camera);
            Plugin.RegisterErrorCallback(this, InternalErrorCallbackWrapper);

            _upscalerPrepare = new CommandBuffer();
            _upscalerPrepare.name = "Prepare upscaler";
            Plugin.Prepare(_upscalerPrepare);

            _brp = new Builtin(Plugin);

            // Set up the BlitLib
            BlitLib.Setup();

            // Prepare the Upscaling data
            UpscalingData?.Release();
            UpscalingData = new UpscalingData(Plugin);

            // Initialize the plugin
            SupportedUpscalerModes = Enum.GetValues(typeof(UpscalerMode)).Cast<UpscalerMode>().Where(Plugin.IsSupported).ToList().AsReadOnly();
            if (!DeviceSupportsUpscalerMode(upscalerMode))
                upscalerMode = UpscalerMode.None;

            _lastUpscalerMode = _activeUpscalerMode = upscalerMode;
            _lastQualityMode = _activeQualityMode = qualityMode;
            Plugin.SetUpscaler(_activeUpscalerMode);
            if (upscalerMode == UpscalerMode.None)
            {
                return;
            }

            Plugin.SetFramebufferSettings((uint)UpscalingResolution.x, (uint)UpscalingResolution.y, _activeQualityMode,
                ActiveHDR);
        }

        /// Runs Before Culling; Validates Settings and Checks for Errors from Previous Frame Before Calling
        /// Real OnPreCull Actions from Backend
        protected internal void OnPreCull()
        {
            if (!Application.isPlaying)
            {
                return;
            }

            if (ChangeInSettings())
            {
                // If there are settings changes, Validate and Push them (this includes a potential call to Error Callback)
                var settingsChange = ValidateAndPushSettings();

                // If Settings Change Caused Error, pass to Internal Error Handler
                if (Failure(settingsChange.Item1))
                {
                    InternalErrorHandler(settingsChange.Item1, settingsChange.Item2);
                }
            }

            Plugin.SetFrameInformation(Time.deltaTime * 1000, new CameraInfo(_camera));

            if (DUpscaler)
            {
                Plugin.SetUpscaler(_activeUpscalerMode);
            }

            if (DUpscalingResolution | DHDR | DQuality | DUpscaler && _activeUpscalerMode != UpscalerMode.None)
            {
                Plugin.SetFramebufferSettings((uint)UpscalingResolution.x, (uint)UpscalingResolution.y,
                    _activeQualityMode,
                    ActiveHDR);
                var size = Plugin.GetRecommendedResolution();
                RenderingResolution = new Vector2Int((int)(size >> 32), (int)(size & 0xFFFFFFFF));
            }

            if (DRenderingResolution | DUpscalingResolution)
            {
                Jitter.Generate((Vector2)UpscalingResolution / RenderingResolution);
            }

            var upscalerOutdated = false;

            if (DSharpness)
            {
                Plugin.SetSharpnessValue(_sharpness);
                upscalerOutdated = (_lastSharpness == 0) ^ (_sharpness == 0);
            }

            if (DHDR | DUpscalingResolution | DRenderingResolution |
                DUpscaler | DQuality)
            {
                upscalerOutdated |=
                    UpscalingData.ManageInColorTarget(_activeUpscalerMode, RenderingResolution);
            }

            if (DHDR | DUpscalingResolution | DUpscaler | DQuality)
            {
                upscalerOutdated |= UpscalingData.ManageOutputTarget(_activeUpscalerMode, UpscalingResolution);
            }

            if (DUpscalingResolution | DRenderingResolution | DUpscaler | DQuality)
            {
                upscalerOutdated |=
                    UpscalingData.ManageMotionVectorTarget(_activeUpscalerMode, RenderingResolution);
            }

            if (upscalerOutdated)
            {
                Graphics.ExecuteCommandBuffer(_upscalerPrepare);
            }

            if (DUpscaler && _activeUpscalerMode == UpscalerMode.None)
            {
                Jitter.Reset(Plugin);
            }

            _lastHDRActive = ActiveHDR;
            _lastUpscalingResolution = UpscalingResolution;
            _lastRenderingResolution = RenderingResolution;
            _lastUpscalerMode = _activeUpscalerMode;
            _lastQualityMode = _activeQualityMode;
            _lastSharpness = _sharpness;

            if (_activeUpscalerMode != UpscalerMode.None)
            {
                Jitter.Apply(Plugin, RenderingResolution);
            }
        }

        protected void OnPreRender()
        {
            if (Application.isPlaying && _activeUpscalerMode != UpscalerMode.None) _brp.PrepareRendering(UpscalingData);
        }

        protected void OnPostRender()
        {
            if (Application.isPlaying && _activeUpscalerMode != UpscalerMode.None) _brp.Upscale(UpscalingData);
        }

        /// Shows if settings have changed since last checked
        /// No internal change should ever reflect a settings 'change'
        /// This will only return true when a user change causes settings to fall out of sync between Upscaler and BackendUpscaler
        private bool ChangeInSettings()
        {
            return upscalerMode != _activeUpscalerMode || qualityMode != _activeQualityMode ||
                   !sharpness.Equals(_lastSharpness);
        }

        /// Validates settings changes and pushes them to BackendUpscaler so they take effect if no issue met.
        /// Status with new settings will be returned.
        /// Returns a Tuple containing new internal UpscalerStatus as well as a message about settings change.
        private Tuple<UpscalerStatus, string> ValidateAndPushSettings()
        {
            // Check for lack of support for currently selected upscaler.
            var settingsError = Plugin.GetStatus();
            if (Failure(settingsError))
            {
                var errorMsg = "There was an Error in Changing Upscaler Settings. Details:\n";
                errorMsg += "Invalid Upscaler: " + Plugin.GetStatusMessage() + "\n";
                Status = settingsError;
                return new Tuple<UpscalerStatus, string>(settingsError, errorMsg);
            }

            // Propagate changes to backend if no errors when changing settings.
            _activeUpscalerMode = upscalerMode;
            _lastSharpness = _sharpness;
            _sharpness = sharpness;
            _activeQualityMode = qualityMode;

            // Get proper success status and set internal status to match.
            Status = _activeUpscalerMode == UpscalerMode.None
                ? UpscalerStatus.NoUpscalerSet
                : UpscalerStatus.Success;

            // Return some success status if no settings errors were encountered.
            return new Tuple<UpscalerStatus, string>(Status, "Upscaler Settings Updated Successfully");
        }

        // If the program ever encounters some kind of error, this will be called.
        // Rendering errors call this through InternalErrorCallbackWrapper.
        // Settings errors call this from ValidateAndPushSettings.

        // If there's an error at all, revert to previous successful upscaler (if it exists, otherwise none).
        // Unrecoverable error: remove upscaler that was set from list of supported upscalers.
        // In general, call the callback once during the frame (settings will update next frame).
        private void InternalErrorHandler(UpscalerStatus reason, string message)
        {
            if (ErrorCallback == null)
            {
                Debug.LogError(message + "\nNo error callback. Reverting upscaling to None.");
                upscalerMode = UpscalerMode.None;
            }
            else
            {
                ErrorCallback(reason, message);

                if (!ChangeInSettings())
                {
                    Debug.LogError(message +
                                   "\nError callback did not make any settings changes. Reverting Upscaling to None.");
                    upscalerMode = UpscalerMode.None;
                }

                Plugin.ResetStatus();
                var settingsChangeEffect = ValidateAndPushSettings();
                if (Failure(settingsChangeEffect.Item1))
                {
                    Debug.LogError("Original Error:\n" + message + "\nCallback attempted to fix settings " +
                                   "but failed. New Error:\n" + settingsChangeEffect.Item2);
                    upscalerMode = UpscalerMode.None;
                }
                else
                {
                    Debug.LogError("Upscaler encountered an Error, but it was fixed by callback. Original Error:\n" +
                                   message);
                }
            }
        }

        [MonoPInvokeCallback(typeof(InternalErrorCallback))]
        internal static void InternalErrorCallbackWrapper(IntPtr upscaler, UpscalerStatus reason, IntPtr message)
        {
            var handle = (GCHandle)upscaler;
            (handle.Target as Upscaler)!.InternalErrorHandler(reason,
                "Error was encountered while upscaling. Details: " + Marshal.PtrToStringAnsi(message) + "\n");
        }

        protected void OnDisable()
        {
            // Shutdown the active render pipeline
            _brp.Shutdown();
            UpscalingData?.Release();
        }
    }
}