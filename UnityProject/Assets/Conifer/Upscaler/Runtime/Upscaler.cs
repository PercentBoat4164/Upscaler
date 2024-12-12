/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

/**************************************************
 * Upscaler v1.1.3                                *
 * See the UserManual.pdf for more information    *
 **************************************************/

using System;
using UnityEngine;
using UnityEngine.Rendering;

namespace Conifer.Upscaler
{
    /**
     * The unified interface used to interact with the different <see cref="Technique"/>s. It may only be put on a
     * <see cref="UnityEngine.Camera"/> object.
     */
    [RequireComponent(typeof(Camera)), AddComponentMenu("Rendering/Upscaler v1.1.3")]
    public class Upscaler : MonoBehaviour
    {
        /**
         * The upscaling solutions from various vendors that Upscaler supports.
         */
        [Serializable]
        public enum Technique
        {
            /// The <see cref="None"/> <see cref="Technique"/>. Use this to turn off upscaling.
            None,
            /// NVIDIA's Deep Learning Super Sampling upscaler, designed for modern NVIDIA GPUs.
            DeepLearningSuperSampling,
            /// AMD's FidelityFX Super Resolution upscaler, designed for any modern GPU with compute support.
            FidelityFXSuperResolution,
            /// Intel's X<sup>e</sup> Super Sampling upscaler, designed to excel on Intel GPUs, but works on any modern GPU with compute.
            XeSuperSampling,
            /// Snapdragon's Snapdragon Game Super Resolution v1, a spatial algorithm designed for mobile devices.
            SnapdragonGameSuperResolutionSpatial,
            /// Snapdragon's Snapdragon Game Super Resolution v1, a temporal algorithm with modes designed for anything from mobile to desktop class devices.
            SnapdragonGameSuperResolutionTemporal
        }

        /**
         * The <see cref="Quality"/> modes that the <see cref="Technique"/>s support. Query support for a particular
         * mode using <see cref="IsSupported(Quality)"/> and <see cref="IsSupported(Technique, Quality)"/>
         */
        [Serializable]
        public enum Quality
        {
            /// Automatically choose another <see cref="Quality"/> mode based on output resolution. Available to all <see cref="Technique"/>s.
            Auto,
            /// Render at native resolution and use the <see cref="Technique"/> for antialiasing.
            AntiAliasing,
            /// Recommended for extremely low resolution outputs. Only available when using the <see cref="Technique.XeSuperSampling"/> <see cref="Technique"/>.
            UltraQualityPlus,
            /// Recommended for very low resolution outputs. Only available when using the <see cref="Technique.XeSuperSampling"/> <see cref="Technique"/>.
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
         * Different DLSS models in an effort to help it to deal with your application's idiosyncrasies.
         */
        [Serializable]
        public enum DlssPreset
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

        /**
         * Different SGSR implementations which trade off quality and performance
         */
        [Serializable]
        public enum SgsrMethod
        {
            /// Default SGSR behavior. Recommended for most desktop and high-end mobile devices. Slowest method, higher quality than <see cref="Compute2Pass"/>.
            Compute3Pass,
            /// Faster than <see cref="Compute3Pass"/>, higher quality than <see cref="Fragment2Pass"/>
            Compute2Pass,
            /// Recommended for low-end mobile devices. Faster than <see cref="Compute2Pass"/>, lowest quality.
            Fragment2Pass,
        }

        private const byte ErrorRecoverable = 1 << 7;

        /**
         * The possible <see cref="Status"/> values that can be reported by an <see cref="Technique"/>. See
         * <see cref="Success"/>, <see cref="Failure"/>, and <see cref="Recoverable"/> to extract information from a
         * particular <see cref="Status"/>.
         */
        public enum Status : byte
        {
            /// The success signal sent by non-<see cref="Technique.None"/> <see cref="Technique"/>s.
            Success                     = 0 | ErrorRecoverable,
            /// The GPU does not support the <see cref="Technique"/> that reports this. This is a permanently fatal error for the <see cref="Technique"/> that reports this.
            DeviceNotSupported          = 1,
            /// The GPU drivers are out of date. Tell the user to update them. This is a permanently fatal error for the <see cref="Technique"/> that reports this.
            DriversOutOfDate            = 2,
            /// The current <see cref="Technique"/> does not support the current Graphics API. This is a permanently fatal error for the <see cref="Technique"/> that reports this.
            UnsupportedGraphicsApi      = 3,
            /// This operating system is not supported. This is a permanently fatal error for the <see cref="Technique"/> that reports this.
            OperatingSystemNotSupported = 4,
            /// NVIDIA has denied this application the <see cref="Technique.DeepLearningSuperSampling"/> feature. This is a permanently fatal error for the <see cref="Technique"/> that reports this.
            FeatureDenied               = 5,
            /// Upscaler attempted an allocation that would overflow either the RAM or VRAM. Free up memory then try again.
            OutOfMemory                 = 6 | ErrorRecoverable,
            /// The library required by this <see cref="Technique"/> were not loaded. Ensure that they have been installed.
            LibraryNotLoaded            = 7,
            /// This is an error that may have been caused by an invalid configuration (e.g. output resolution too small). Restore a valid configuration then try again.
            RecoverableRuntimeError     = 8 | ErrorRecoverable,
            /// This error should result only from a bug in Upscaler or in Unity itself.
            FatalRuntimeError           = 9,
        }

        /**
         * <summary>Does a <see cref="Status"/> represent a success state?</summary>
         * <param name="status">The <see cref="Status"/> in question.</param>
         * <returns><c>true</c> if the <see cref="Status"/> is success-y and <c>false</c> if not.</returns>
         * <remarks>This returns <c>true</c> only for <see cref="Status.Success"/>. Being a <see cref="Success"/>
         * <see cref="Status"/> means that the <see cref="Technique"/> in question is usable in its current state.
         * </remarks>
         */
        public static bool Success(Status status) => status == Status.Success;

        /**
         * <summary>Does a <see cref="Status"/> represent a fail state?</summary>
         * <param name="status">The <see cref="Status"/> in question.</param>
         * <returns><c>true</c> if the <see cref="Status"/> is an error and <c>false</c> if not.</returns>
         * <remarks>This returns <c>true</c> for all <see cref="Status"/>es that are not <see cref="Status.Success"/>.
         * Being a <see cref="Failure"/> <see cref="Status"/> means that the <see cref="Technique"/> in question
         * is not usable in its current state. See <see cref="Recoverable"/> to determine if fixing the problem is
         * possible.</remarks>
         */
        public static bool Failure(Status status) => status != Status.Success;

        /**
         * <summary>Is a <see cref="Status"/> recoverable from?</summary>
         * <param name="status">The <see cref="Status"/> to determine the recoverability of.</param>
         * <returns><c>true</c> if the <see cref="Status"/> is recoverable and <c>false</c> if not.</returns>
         * <remarks><see cref="Success"/> <see cref="Status"/>es are not recoverable. Being <see cref="Recoverable"/>
         * means that it is possible to put the <see cref="Technique"/> back into a <see cref="Success"/> state.
         * Non-recoverable <see cref="Status"/>es are fatal for the <see cref="Technique"/> that reports them.
         * </remarks>
         */
        public static bool Recoverable(Status status) => ((uint)status & ErrorRecoverable) == ErrorRecoverable;

        /// Enables displaying the Rendering Area overlay. Defaults to <c>false</c>
        public bool debugView;
        /// Enables displaying the Rendering Area overlay. Defaults to <c>false</c>
        public bool showRenderingAreaOverlay;
        /// While this is true Upscaler will call <see cref="ResetHistory"/> every frame.
        public bool forceHistoryResetEveryFrame;

        private Camera _camera;
        internal Matrix4x4 LastViewToClip = Matrix4x4.identity;
        internal Matrix4x4 LastWorldToCamera = Matrix4x4.identity;
        internal Vector2 Jitter = Vector2.zero;

        internal NativeInterface NativeInterface;

        /// Whether the upscaling is HDR aware or not. It will have a value of <c>true</c> if Upscaler is using HDR
        /// upscaling. It will have a value of <c>false</c> otherwise.
        public bool HDR { get; private set; }

        /// The current output resolution. Upscaler does not control the output resolution but rather adapts to whatever
        /// output resolution Unity requests. This value is a rounded version of <c>Camera.pixelRect.size</c>
        public Vector2Int OutputResolution => _camera ? Vector2Int.RoundToInt(_camera.pixelRect.size) : Vector2Int.one;
        private Vector2Int _outputResolution = Vector2Int.zero;

        /// The current resolution at which the scene is being rendered. This may be set to any value within the bounds
        /// of <see cref="MaxInputResolution"/> and <see cref="MinInputResolution"/>. It is also set to the
        /// <see cref="RecommendedInputResolution"/> whenever the <see cref="technique"/> or <see cref="quality"/> are
        /// changed using <see cref="ApplySettings"/>. It is never <c>(0, 0)</c>. When the <see cref="Technique"/> has a
        /// <see cref="Status"/> error or the <see cref="Technique"/> is <see cref="Technique.None"/> it will be the
        /// same as <see cref="OutputResolution"/>.
        public Vector2Int InputResolution
        {
            get => _inputResolution;
            set => _inputResolution = Vector2Int.Max(MinInputResolution, Vector2Int.Min(MaxInputResolution, value));
        }
        private Vector2Int _inputResolution;

        /// The recommended resolution for this quality mode as given by the selected <see cref="Technique"/>. This
        /// value will only ever be <c>(0, 0)</c> when Upscaler has yet to be enabled.
        public Vector2Int RecommendedInputResolution { get; private set; }

        /// The maximum dynamic resolution for this quality mode as given by the selected <see cref="Technique"/>. This
        /// value will only ever be <c>(0, 0)</c> when Upscaler has yet to be enabled.
        public Vector2Int MaxInputResolution { get; private set; }

        /// The minimum dynamic resolution for this quality mode as given by the selected <see cref="Technique"/>. This
        /// value will only ever be <c>(0, 0)</c> when Upscaler has yet to be enabled.
        public Vector2Int MinInputResolution { get; private set; }

        /**
         * <summary>The callback used to handle any errors that the <see cref="Technique"/> throws.</summary>
         * <param name="Status">The <see cref="Status"/> that the <see cref="Technique"/> errored with.</param>
         * <param name="string">A plain English message describing the nature of the issue.</param>
         * <remarks>This callback is only ever called if an error occurs. When that happens it will be called from the
         * <see cref="Update"/> method during the next frame. If this callback fails to bring the
         * <see cref="Technique"/>'s <see cref="Status"/> back to a <see cref="Success"/> value, then the default error
         * handler will reset the current <see cref="Technique"/> to the default <see cref="Technique.None"/>.</remarks>
         * <example><code>upscaler.ErrorCallback = (status, message) => { };</code></example>
         */
        [NonSerialized] public Action<Status, string> ErrorCallback;

        /// The current <see cref="Status"/> for the managed <see cref="Technique"/>.
        public Status CurrentStatus { get; private set; }
        /// The current <see cref="Quality"/> mode. Defaults to <see cref="Quality.Auto"/>.
        public Quality quality;
        private Quality _quality;
        /// The current <see cref="Technique"/>. Defaults to <see cref="Technique.None"/>.
        public Technique technique = GetBestSupportedTechnique();
        private Technique _technique;
        /// The current <see cref="DlssPreset"/>. Defaults to <see cref="DlssPreset.Default"/>. Only used when <see cref="technique"/> is <see cref="Technique.DeepLearningSuperSampling"/>.
        public DlssPreset dlssPreset;
        private DlssPreset _dlssPreset;
        /// The current <see cref="SgsrMethod"/>. Defaults to Compute3Pass
        public SgsrMethod sgsrMethod;
        /// The current sharpness value. This should always be in the range of <c>0.0f</c> to <c>1.0f</c>. Defaults to <c>0.0f</c>. Only used when <see cref="technique"/> is <see cref="Technique.FidelityFXSuperResolution"/> or <see cref="Technique.SnapdragonGameSuperResolutionSpatial"/>.
        public float sharpness;
        /// Instructs Upscaler to set <see cref="Technique.FidelityFXSuperResolution"/> parameters for automatic reactive mask generation. Defaults to <c>true</c>. Only used when <see cref="technique"/> is <see cref="Technique.FidelityFXSuperResolution"/>.
        public bool useReactiveMask = true;
        /// Maximum reactive value. More reactivity favors newer information. Conifer has found that <c>0.6f</c> works well in our Unity testing scene, but please test for your specific title. Defaults to <c>0.6f</c>. Only used when <see cref="technique"/> is <see cref="Technique.FidelityFXSuperResolution"/>.
        public float reactiveMax = 0.6f;
        /// Value used to scale reactive mask after generation. Larger values result in more reactive pixels. Conifer has found that <c>0.9f</c> works well in our Unity testing scene, but please test for your specific title. Defaults to <c>0.9f</c>. Only used when <see cref="technique"/> is <see cref="Technique.FidelityFXSuperResolution"/>.
        public float reactiveScale = 0.9f;
        /// Minimum reactive threshold. Increase to make more of the image reactive. Conifer has found that <c>0.3f</c> works well in our Unity testing scene, but please test for your specific title. Defaults to <c>0.3f</c>. Only used when <see cref="technique"/> is <see cref="Technique.FidelityFXSuperResolution"/>.
        public float reactiveThreshold = 0.3f;
        /// Enables the use of Edge Direction. Disabling this increases performance at the cost of visual quality. Defaults to <c>true</c>. Only used when <see cref="technique"/> is <see cref="Technique.SnapdragonGameSuperResolutionSpatial"/>.
        public bool useEdgeDirection = true;
        private bool _useEdgeDirection;

        /**
         * <summary>Request the 'best' technique that is supported by this environment.</summary>
         *<returns>Returns the 'best' supported technique.</returns>
         * <remarks> Selects <see cref="Technique.SnapdragonGameSuperResolutionSpatial"/> if on a mobile platform and it is
         * supported. It otherwise selects the first supported technique as they appear in the following list:
         * <see cref="Technique.DeepLearningSuperSampling"/>, <see cref="Technique.XeSuperSampling"/>,
         * <see cref="Technique.FidelityFXSuperResolution"/>, <see cref="Technique.None"/></remarks>
         */
        public static Technique GetBestSupportedTechnique()
        {
            if (Application.isMobilePlatform && IsSupported(Technique.SnapdragonGameSuperResolutionSpatial)) return Technique.SnapdragonGameSuperResolutionSpatial;
            if (IsSupported(Technique.DeepLearningSuperSampling)) return Technique.DeepLearningSuperSampling;
            if (IsSupported(Technique.XeSuperSampling)) return Technique.XeSuperSampling;
            return IsSupported(Technique.FidelityFXSuperResolution) ? Technique.FidelityFXSuperResolution : Technique.None;
        }

        /**
         * <summary>Tells the <see cref="Technique"/> whether or not to reset the pixel history next upscale.</summary>
         * <remarks>This method is fast. It just sets a flag that tells the <see cref="Technique"/> to reset the pixel
         * history during the next upscale. This flag is automatically cleared after each upscale. This should be called
         * with <c>true</c> everytime there is little or no correlation between what the camera saw last frame and what
         * it sees this frame. This will be set to <c>true</c> if the <c>resetHistory</c> flag is on in the
         * <c>UniversalAdditionalCameraData</c>.
         * </remarks>
         * <example><code>
         * SnapCameraToLastFramePosition();
         * upscaler.ShouldResetHistory(false);
         * // Or
         * TurnCameraAround();
         * upscaler.ShouldResetHistory(true);
         * </code></example>
         */
        public void ShouldResetHistory(bool reset) => NativeInterface.ShouldResetHistory = reset;

        /**
         * <summary>Tells the <see cref="Technique"/> to reset the pixel history next upscale.</summary>
         * <remarks>Internally calls the <see cref="ShouldResetHistory"/> function, passing it <c>true</c>.</remarks>
         * <example><code>
         * CameraJumpCut(newLocation);
         * upscaler.ResetHistory();
         * </code></example>
         */
        public void ResetHistory() => NativeInterface.ShouldResetHistory = true;

        /**
         * <summary>Check if an <see cref="Technique"/> is supported in the current environment.</summary>
         * <param name="type">The <see cref="Technique"/> to query support for.</param>
         * <returns><c>true</c> if the <see cref="Technique"/> is supported and <c>false</c> if it is not.</returns>
         * <remarks>This method is slow the first time it is used for each <see cref="Technique"/>, then fast every time
         * after that. Support for the <see cref="Technique"/> requested is computed then cached. Any future calls with
         * the same <see cref="Technique"/> will use the cached value.</remarks>
         * <example><code>bool DLSSSupported = Upscaler.IsSupported(Upscaler.Technique.DeepLearningSuperSampling);
         * </code></example>
         */
        public static bool IsSupported(Technique type) => type is Technique.None ||
                                                          (type is Technique.SnapdragonGameSuperResolutionSpatial or Technique.SnapdragonGameSuperResolutionTemporal && SystemInfo.graphicsShaderLevel >= 50) ||
                                                          (PluginLoaded() && NativeInterface.IsSupported(type));

        /**
         * <summary>Check if a <see cref="Quality"/> mode is supported by a given <see cref="Technique"/>.</summary>
         * <param name="type">The <see cref="Technique"/> to query.</param>
         * <param name="mode">The <see cref="Quality"/> mode to query support for.</param>
         * <returns><c>true</c> if the <see cref="Technique"/> supports the requested <see cref="Quality"/> mode and
         * <c>false</c> if it does not.</returns>
         * <remarks>This method is always fast. Every non-<see cref="Technique.None"/> <see cref="Technique"/> will
         * return <c>true</c> for the <see cref="Quality.Auto"/> <see cref="Quality"/> mode.</remarks>
         * <example><code>bool dlssSupportsAA = upscaler.IsSupported(
         *   Upscaler.Technique.DeepLearningSuperSampling,
         *   Upscaler.Quality.AntiAliasing
         * );</code></example>
         */
        public static bool IsSupported(Technique type, Quality mode) => type is Technique.None ||
                                                                        (type is Technique.SnapdragonGameSuperResolutionSpatial && SystemInfo.graphicsShaderLevel >= 50 && mode != Quality.AntiAliasing) ||
                                                                        (type is Technique.SnapdragonGameSuperResolutionTemporal && SystemInfo.graphicsShaderLevel >= 50) ||
                                                                        NativeInterface.IsSupported(type, mode);

        /**
         * <summary>Check if a <see cref="Quality"/> mode is supported by the current <see cref="Technique"/>.</summary>
         * <param name="mode">The <see cref="Quality"/> mode to query the current <see cref="Technique"/> for support of
         * .</param>
         * <returns><c>true</c> if the current <see cref="Technique"/> supports the requested <see cref="Quality"/> mode
         * and <c>false</c> if it does not.</returns>
         * <remarks>This method is always fast. This is a convenience method for
         * <see cref="IsSupported(Technique, Quality)"/> that uses this <see cref="Upscaler"/>'s <see cref="Technique"/> as the first
         * argument. </remarks>
         * <example><code>bool supportsAA = upscaler.IsSupported(Upscaler.Quality.AntiAliasing);</code></example>
         */
        public bool IsSupported(Quality mode) => IsSupported(technique, mode);

        /**
         *
         */
        public static bool IsSpatial(Technique type) => type switch
            {
                Technique.None or Technique.SnapdragonGameSuperResolutionSpatial => true,
                Technique.DeepLearningSuperSampling or Technique.FidelityFXSuperResolution or Technique.XeSuperSampling or Technique.SnapdragonGameSuperResolutionTemporal => false,
                _ => throw new ArgumentOutOfRangeException(nameof(type), type, type + " is not a valid " + nameof(type) + " enum value.")
            };

        /**
         *
         */
        public bool IsSpatial() => IsSpatial(technique);

        /**
         *
         */
        public static bool IsTemporal(Technique type) => !IsSpatial(type);

        /**
         *
         */
        public bool IsTemporal() => !IsSpatial(technique);

        public static bool RequiresNativePlugin(Technique type) => type switch
            {
                Technique.SnapdragonGameSuperResolutionSpatial or Technique.SnapdragonGameSuperResolutionTemporal or Technique.None => false,
                Technique.DeepLearningSuperSampling or Technique.FidelityFXSuperResolution or Technique.XeSuperSampling => true,
                _ => throw new ArgumentOutOfRangeException(nameof(type), type, type + " is not a valid " + nameof(type) + " enum value.")
            };

        public bool RequiresNativePlugin() => RequiresNativePlugin(technique);

        /**
         * <summary>Check if the <c>GfxPluginUpscaler</c> shared library has been loaded.</summary>
         * <returns><c>true</c> if the <c>GfxPluginUpscaler</c> shared library has been loaded by Unity and <c>false</c>
         * if it has not been.</returns>
         * <remarks>When this function returns false <see cref="Technique.DeepLearningSuperSampling"/>,
         * <see cref="Technique.FidelityFXSuperResolution"/>, and <see cref="Technique.XeSuperSampling"/> will all be
         * unavailable.</remarks>
         * <example><code>bool nativePluginLoaded = Upscaler.PluginLoaded();</code></example>
         */
        public static bool PluginLoaded() => NativeInterface.Loaded;

        /**
         * <summary>Sets the log level filter for the C++ backend.</summary>
         * <param name="level">The minimum log level that messages must be before they are logged.</param>
         * <remarks>This applies to all instances of the Upscaler script globally. This method is very fast. The first
         * time this method is called all previously recorded messages will be filtered, then pushed. This method is
         * probably first called by Upscaler's Inspector GUI.</remarks>
         */
        public static void SetLogLevel(LogType level) => NativeInterface.SetLogLevel(level);

        /**
         * <summary>Use the new settings. This does not need to be called when adjusting dynamic resolution. Simply
         * change <see cref="InputResolution"/>.</summary>
         * <param name="force">Should the settings be applied even if they are the same as before? Defaults to
         * <c>false</c>. <b>USE WITH DISCRETION.</b></param>
         * <returns>The <see cref="Status"/> of the upscaler after attempting to apply settings.</returns>
         * <remarks>This method is very slow when the settings have changed, or if <see cref="force"/> is set to
         * <c>true</c>. When that is the case it will update the <see cref="InputResolution"/> based on a query of the
         * new <see cref="Technique"/>. An important thing to note is that when <see cref="force"/> is set to
         * <c>true</c> all settings validation is disabled. This is done to ensure that the settings are pushed to the
         * upscaler, but does mean that using this option <em>can</em> crash Unity.</remarks>
         * <example><code>upscaler.ApplySettings();</code></example>
         */
        public Status ApplySettings(bool force = false)
        {
            if (!Application.isPlaying || NativeInterface is null) return Status.Success;
            if (!force && quality == _quality && useEdgeDirection == _useEdgeDirection && dlssPreset == _dlssPreset && technique == _technique && OutputResolution == _outputResolution && HDR == _camera.allowHDR) return NativeInterface.GetStatus();

            var newOutputResolution = Vector2Int.RoundToInt(_camera.pixelRect.size);
            HDR = _camera.allowHDR;

            if (!force)
            {
                if (!Enum.IsDefined(typeof(Quality), quality)) return NativeInterface.SetStatus(Status.RecoverableRuntimeError, "`quality`(" + quality + ") is not a valid Quality.");
                if (!Enum.IsDefined(typeof(DlssPreset), dlssPreset)) return NativeInterface.SetStatus(Status.RecoverableRuntimeError, "`dlssPreset`(" + dlssPreset + ") is not a valid DlssPreset.");
                if (!Enum.IsDefined(typeof(Technique), technique)) return NativeInterface.SetStatus(Status.RecoverableRuntimeError, "`technique`(" + technique + ") is not a valid Technique.");
                if (!IsSupported(technique)) return NativeInterface.SetStatus(Status.RecoverableRuntimeError, "`technique`(" + technique + ") is not supported.");
                if (technique == Technique.DeepLearningSuperSampling && Vector2Int.Max(newOutputResolution, new Vector2Int(64, 32)) != newOutputResolution) return NativeInterface.SetStatus(Status.RecoverableRuntimeError, "OutputResolution is less than (64, 32).");
                if (technique != Technique.None && !IsSupported(technique, quality)) return NativeInterface.SetStatus(Status.RecoverableRuntimeError, "`quality`(" + quality + ") is not supported by the `technique`(" + technique + ").");
            }

            if (RequiresNativePlugin(technique))
            {
                CurrentStatus = NativeInterface.SetPerFeatureSettings(OutputResolution, technique, dlssPreset, quality, sharpness, HDR);
                RecommendedInputResolution = Vector2Int.Max(NativeInterface.GetRecommendedResolution(), Vector2Int.one);
                MaxInputResolution = Vector2Int.Max(NativeInterface.GetMaximumResolution(), Vector2Int.one);
                MinInputResolution = Vector2Int.Max(NativeInterface.GetMinimumResolution(), Vector2Int.one);
            }
            else
            {
                NativeInterface.SetPerFeatureSettings(OutputResolution, Technique.None, dlssPreset, quality, sharpness, HDR);
                if (useEdgeDirection) Shader.EnableKeyword("CONIFER_UPSCALER_USE_EDGE_DIRECTION");
                else Shader.DisableKeyword("CONIFER_UPSCALER_USE_EDGE_DIRECTION");
                if (SystemInfo.graphicsShaderLevel < 50) return CurrentStatus = Status.DeviceNotSupported;
                var scale = 0.769;
                switch (quality)
                {
                    case Quality.Auto:
                        scale = ((uint)OutputResolution.x * (uint)OutputResolution.y) switch
                        {
                            <= 2560 * 1440 => 0.769,
                            <= 3840 * 2160 => 0.667,
                            _ => 0.5
                        }; break;
                    case Quality.AntiAliasing:
                        if (technique == Technique.SnapdragonGameSuperResolutionSpatial) CurrentStatus = Status.RecoverableRuntimeError;
                        else scale = 1.0;
                        break;
                    case Quality.UltraQualityPlus: scale = 0.769; break;
                    case Quality.UltraQuality: scale = 0.667; break;
                    case Quality.Quality: scale = 0.588; break;
                    case Quality.Balanced: scale = 0.5; break;
                    case Quality.Performance: scale = 0.435; break;
                    case Quality.UltraPerformance: scale = 0.333; break;
                    default: CurrentStatus = Status.RecoverableRuntimeError; break;
                }

                RecommendedInputResolution = new Vector2Int((int)Math.Ceiling(OutputResolution.x * scale), (int)Math.Ceiling(OutputResolution.y * scale));
                MaxInputResolution = OutputResolution;
                MinInputResolution = Vector2Int.one;
            }

            if (OutputResolution != _outputResolution || technique != _technique || quality != _quality || force)
                InputResolution = RecommendedInputResolution;
            _outputResolution = OutputResolution;
            if (Failure(CurrentStatus))
                return CurrentStatus;
            if (IsTemporal(_technique))
                _camera.ResetProjectionMatrix();
            _quality = quality;
            _useEdgeDirection = useEdgeDirection;
            _dlssPreset = dlssPreset;
            _technique = technique;
            ResetHistory();
            return CurrentStatus;
        }

        protected void OnEnable()
        {
            _camera = GetComponent<Camera>();
            _camera.depthTextureMode |= DepthTextureMode.MotionVectors | DepthTextureMode.Depth;

            NativeInterface ??= new NativeInterface();

            if (!IsSupported(technique)) technique = GetBestSupportedTechnique();
            ApplySettings();
        }

        private int _screenWidth = Screen.width;
        private int _screenHeight = Screen.height;
        internal bool DisableUpscaling;

        protected void Update()
        {
            DisableUpscaling = Screen.width != _screenWidth || Screen.height != _screenHeight || Time.frameCount == 1;
            if (DisableUpscaling) {
                ResetHistory();
                _screenWidth = Screen.width;
                _screenHeight = Screen.height;
            }

            if (forceHistoryResetEveryFrame) ResetHistory();
            if (!Application.isPlaying) return;
            CurrentStatus = ApplySettings();
            if (!Failure(CurrentStatus)) return;
            if (ErrorCallback is not null)
            {
                ErrorCallback(CurrentStatus, NativeInterface.GetStatusMessage());
                CurrentStatus = NativeInterface.GetStatus();
                if (!Failure(CurrentStatus)) return;
                Debug.LogError("The registered error handler failed to rectify the following error.");
            }

            Debug.LogWarning(NativeInterface.GetStatus() + " | " + NativeInterface.GetStatusMessage());
            technique = Technique.None;
            quality = Quality.Auto;
            ApplySettings(true);
        }

        private void OnDisable() => _camera.ResetProjectionMatrix();

        private void OnGUI()
        {
            if (technique == Technique.None || !showRenderingAreaOverlay) return;
            var scale = (float)InputResolution.x / OutputResolution.x;
            GUI.Box(new Rect(0, OutputResolution.y - InputResolution.y, InputResolution.x, InputResolution.y),
                Math.Ceiling(scale * 100) + "% per-axis\n" + Math.Ceiling(scale * scale * 100) + "% total");
        }
    }
}