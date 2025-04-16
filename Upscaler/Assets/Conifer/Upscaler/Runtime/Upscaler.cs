/**************************************************
 * Upscaler v2.0.1                                *
 * See the UserManual.pdf for more information    *
 **************************************************/

using System;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

namespace Conifer.Upscaler
{
    /**
     * The unified interface used to interact with the different <see cref="Technique"/>s. It may only be put on a
     * <see cref="UnityEngine.Camera"/> object.
     */
    [RequireComponent(typeof(Camera)), AddComponentMenu("Rendering/Upscaler v2.0.1"), ExecuteAlways]
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
            /// AMD's FidelityFX Super Resolution upscaler, designed for any modern desktop GPU with compute support.
            FidelityFXSuperResolution,
            /// Intel's X<sup>e</sup> Super Sampling upscaler, designed to excel on Intel GPUs, but works on any modern GPU with compute.
            XeSuperSampling,
            /// Snapdragon's Snapdragon Game Super Resolution v1, a spatial algorithm designed for mobile devices.
            SnapdragonGameSuperResolution1,
            /// Snapdragon's Snapdragon Game Super Resolution v2, a temporal algorithm with modes designed for anything from mobile to desktop class devices.
            SnapdragonGameSuperResolution2
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
            /// Render at native resolution and use the <see cref="Technique"/> for antialiasing. Not available for <see cref="Upscaler.IsSpatial(Conifer.Upscaler.Upscaler.Technique)"/> <see cref="Technique"/>s.
            AntiAliasing,
            /// Recommended for extremely low resolution outputs.
            UltraQualityPlus,
            /// Recommended for very low resolution outputs.
            UltraQuality,
            /// Recommended for low resolution outputs.
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
         * Different SGSR implementations which trade off quality and performance.
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
         * The possible <see cref="Status"/> values that can be reported by an <see cref="Technique"/>. See the
         * <see cref="Success"/>, <see cref="Failure"/>, and <see cref="Recoverable"/> functions to extract information
         * from a particular <see cref="Status"/>.
         */
        public enum Status : byte
        {
            /// The success signal. This indicates that nothing is going wrong, and it is safe to move forward with upscaling.
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
            /// The libraries required by this <see cref="Technique"/> were not loaded. Ensure that they have been installed.
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
        public static bool Failure(Status status) => !Success(status);

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

        /// Enables displaying frame generation input images. Will not be affected by postprocessing effects. Will display over <see cref="upscalingDebugView"/> if it is turned on at the same time. Only works when <see cref="frameGeneration"/> is enabled. Defaults to <c>false</c>.
        public bool frameGenerationDebugView;
        /// Displays tear lines to help debug frame generation. Only works when <see cref="frameGeneration"/> is enabled. Defaults to <c>false</c>.
        public bool showTearLines;
        /// Displays an indicator whenever a frame generation history reset occurs. Only works when <see cref="frameGeneration"/> is enabled. Defaults to <c>false</c>.
        public bool showResetIndicator;
        /// Displays debug pacing lines to help debug frame generation frame pacing issues. Only works when <see cref="frameGeneration"/> is enabled. Defaults to <c>false</c>.
        public bool showPacingIndicator;
        /// Only presents generated frames onto the screen. This allows easy visualization of generated frame quality. Only works when <see cref="frameGeneration"/> is enabled. Defaults to <c>false</c>.
        public bool onlyPresentGenerated;
        /// Enables displaying upscaling input images. Will be affected by postprocessing effects. Only works when <see cref="Technique.FidelityFXSuperResolution"/> is the active technique. Defaults to <c>false</c>.
        public bool upscalingDebugView;
        /// Enables displaying the Rendering Area overlay. Defaults to <c>false</c>.
        public bool showRenderingAreaOverlay;
        /// While this is true Upscaler will call <see cref="ResetHistory"/> every frame. Defaults to <c>false</c>.
        public bool forceHistoryResetEveryFrame;

        internal Camera Camera;
        internal Matrix4x4 LastViewToClip = Matrix4x4.identity;
        internal Matrix4x4 LastWorldToCamera = Matrix4x4.identity;
        internal Vector2 Jitter = Vector2.zero;
        internal int JitterIndex;

        /// Whether the upscaling is HDR aware or not. It will have a value of <c>true</c> if Upscaler is using HDR
        /// upscaling. It will have a value of <c>false</c> otherwise.
        public bool HDR { get; private set; }

        /// The current output resolution. Upscaler does not control the output resolution but rather adapts to whatever
        /// output resolution Unity requests.
        public Vector2Int OutputResolution { get; private set; } = Vector2Int.zero;
        public Vector2Int PreviousOutputResolution { get; private set; } = Vector2Int.zero;

        /// The current resolution at which the scene is being rendered. This may be set to any value within the bounds
        /// of <see cref="MaxInputResolution"/> and <see cref="MinInputResolution"/>. It is also set to the
        /// <see cref="RecommendedInputResolution"/> whenever settings are changed.
        public Vector2Int InputResolution
        {
            get => _inputResolution;
            set => _inputResolution = Vector2Int.Max(MinInputResolution, Vector2Int.Min(MaxInputResolution, value));
        }
        private Vector2Int _inputResolution;
        public Vector2Int PreviousInputResolution { get; private set; } = Vector2Int.zero;

        /// The recommended resolution for this <see cref="Quality"/> mode as given by the selected
        /// <see cref="Technique"/>. This value will only ever be <c>(0, 0)</c> when Upscaler has yet to be enabled.
        public Vector2Int RecommendedInputResolution { get; internal set; }

        /// The maximum dynamic resolution for this <see cref="Quality"/> mode as given by the selected
        /// <see cref="Technique"/>. This value will only ever be <c>(0, 0)</c> when Upscaler has yet to be enabled.
        public Vector2Int MaxInputResolution { get; internal set; }

        /// The minimum dynamic resolution for this <see cref="Quality"/> mode as given by the selected
        /// <see cref="Technique"/>. This value will only ever be <c>(0, 0)</c> when Upscaler has yet to be enabled.
        public Vector2Int MinInputResolution { get; internal set; }

        /**
         * <summary>The callback used to handle any errors that the <see cref="Technique"/> throws. Takes the current
         * <see cref="Status"/>, and the current status message.</summary>
         * <remarks>This callback is only ever called if an error occurs. When that happens it will be called before
         * rendering the next frame. If this callback fails to bring the <see cref="Technique"/>'s <see cref="Status"/>
         * back to a <see cref="Success"/> value, then the default error handler will reset the current
         * <see cref="Technique"/> to <see cref="Technique.None"/>.</remarks>
         * <example><code>upscaler.ErrorCallback = (status, message) => { };</code></example>
         */
        [NonSerialized] public Action<Status> ErrorCallback;

        /// The current <see cref="Status"/> for the current <see cref="Technique"/>.
        public Status CurrentStatus { get; internal set; } = Status.Success;
        /// The current <see cref="Quality"/> mode. Defaults to <see cref="Quality.Auto"/>.
        public Quality quality = Quality.Auto;
        public Quality PreviousQuality { get; private set; }
        /// The current <see cref="Technique"/>. Defaults to <see cref="Technique.None"/>.
        public Technique technique = Technique.None;
        public Technique PreviousTechnique { get; private set; }

        /// BETA FEATURE: Frame generation only works while using the Vulkan Graphics API. Set to true to enable frame generation.
        public bool frameGeneration;
        public bool PreviousFrameGeneration { get; private set; }
        /// The current <see cref="DlssPreset"/>. Defaults to <see cref="DlssPreset.Default"/>. Only used when <see cref="technique"/> is <see cref="Technique.DeepLearningSuperSampling"/>.
        public DlssPreset preset;
        public DlssPreset PreviousPreset { get; private set; }
        /// The current <see cref="SgsrMethod"/>. Defaults to Compute3Pass
        public SgsrMethod method;
        public SgsrMethod PreviousMethod { get; private set; }
        /// The current sharpness value. This should always be in the range of <c>0.0f</c> to <c>1.0f</c>. Defaults to <c>0.0f</c>. Only used when <see cref="technique"/> is <see cref="Technique.FidelityFXSuperResolution"/> or <see cref="Technique.SnapdragonGameSuperResolution1"/>.
        public float sharpness;
        public float PreviousSharpness { get; private set; }
        /// Instructs Upscaler to set <see cref="Technique.FidelityFXSuperResolution"/> parameters for automatic reactive mask generation. Defaults to <c>true</c>. Only used when <see cref="technique"/> is <see cref="Technique.FidelityFXSuperResolution"/>.
        public bool autoReactive = true;
        public bool PreviousAutoReactive { get; private set; }
        /// Maximum reactive value. More reactivity favors newer information. Conifer has found that <c>0.6f</c> works well in our Unity testing scene, but please test for your specific title. Defaults to <c>0.6f</c>. Only used when <see cref="technique"/> is <see cref="Technique.FidelityFXSuperResolution"/>.
        public float reactiveMax = 0.6f;
        public float PreviousReactiveMax { get; private set; }
        /// Value used to scale reactive mask after generation. Larger values result in more reactive pixels. Conifer has found that <c>0.9f</c> works well in our Unity testing scene, but please test for your specific title. Defaults to <c>0.9f</c>. Only used when <see cref="technique"/> is <see cref="Technique.FidelityFXSuperResolution"/>.
        public float reactiveScale = 0.9f;
        public float PreviousReactiveScale { get; private set; }
        /// Minimum reactive threshold. Increase to make more of the image reactive. Conifer has found that <c>0.3f</c> works well in our Unity testing scene, but please test for your specific title. Defaults to <c>0.3f</c>. Only used when <see cref="technique"/> is <see cref="Technique.FidelityFXSuperResolution"/>.
        public float reactiveThreshold = 0.3f;
        public float PreviousReactiveThreshold { get; private set; }
        /// BETA FEATURE: Enable computing <see cref="frameGeneration"/> on an asynchronous compute queue. This <em>may</em> increase performance on some systems. Only relevant when <see cref="frameGeneration"/> is enabled.
        public bool useAsyncCompute = true;
        public bool PreviousUseAsyncCompute { get; private set; }
        /// Enables the use of Edge Direction. Disabling this increases performance at the cost of visual quality. Defaults to <c>true</c>. Only used when <see cref="technique"/> is <see cref="Technique.SnapdragonGameSuperResolution1"/>.
        public bool useEdgeDirection = true;
        public bool PreviousUseEdgeDirection { get; private set; }

        public bool shouldHistoryResetThisFrame;

        private UpscalerBackend _backend;
        /**
         * <summary>Request the 'best' technique that is supported by this environment.</summary>
         *<returns>Returns the 'best' supported technique.</returns>
         * <remarks> Selects <see cref="Technique.SnapdragonGameSuperResolution2"/> if on a mobile platform and it is
         * supported. If it is not supported it selects <see cref="Technique.SnapdragonGameSuperResolution1"/>. If not on a mobile platform it selects the first supported technique as they appear in the following list:
         * <see cref="Technique.DeepLearningSuperSampling"/>, <see cref="Technique.XeSuperSampling"/>,
         * <see cref="Technique.FidelityFXSuperResolution"/>, <see cref="Technique.None"/></remarks>
         */
        public static Technique GetBestSupportedTechnique()
        {
            if (Application.isMobilePlatform) return IsSupported(Technique.SnapdragonGameSuperResolution2) ? Technique.SnapdragonGameSuperResolution2 : Technique.SnapdragonGameSuperResolution1;
            if (IsSupported(Technique.DeepLearningSuperSampling)) return Technique.DeepLearningSuperSampling;
            if (IsSupported(Technique.XeSuperSampling)) return Technique.XeSuperSampling;
            return IsSupported(Technique.FidelityFXSuperResolution) ? Technique.FidelityFXSuperResolution : Technique.None;
        }

        /**
         * <summary>Check if an <see cref="Technique"/> is supported in the current environment.</summary>
         * <param name="type">The <see cref="Technique"/> to query support for.</param>
         * <returns><c>true</c> if the <see cref="Technique"/> is supported and <c>false</c> if it is not.</returns>
         * <remarks>This method is slow the first time it is used for each <see cref="Technique"/>, then fast every time
         * after that. Support for the <see cref="Technique"/> requested is computed then cached. Any future calls with
         * the same <see cref="Technique"/> will use the cached value. Note that for
         * <see cref="Technique.SnapdragonGameSuperResolution2"/> the <see cref="SgsrMethod.Fragment2Pass"/> may
         * be able to be used even if this returns <c>false</c> for
         * <see cref="Technique.SnapdragonGameSuperResolution2"/>
         * so long as it returns <c>true</c> for <see cref="Technique.SnapdragonGameSuperResolution1"/>.</remarks>
         * <example><code>bool dlssSupported = Upscaler.IsSupported(Upscaler.Technique.DeepLearningSuperSampling);
         * </code></example>
         */
        public static bool IsSupported(Technique type) => type is Technique.None ||
                                                          (type is Technique.SnapdragonGameSuperResolution1 && SystemInfo.graphicsShaderLevel >= 50) ||
                                                          (type is Technique.SnapdragonGameSuperResolution2 && SystemInfo.graphicsShaderLevel >= 50 && SystemInfo.supportsComputeShaders) ||
                                                          (PluginLoaded() && NativeInterface.IsSupported(type));

        /**
         * <summary>Check if a <see cref="Quality"/> mode is supported by a given <see cref="Technique"/>.</summary>
         * <param name="type">The <see cref="Technique"/> to query.</param>
         * <param name="mode">The <see cref="Quality"/> mode to query support for.</param>
         * <returns><c>true</c> if the <see cref="Technique"/> supports the requested <see cref="Quality"/> mode and
         * <c>false</c> if it does not.</returns>
         * <remarks>This method is always fast. Every <see cref="Technique"/> will
         * return <c>true</c> for the <see cref="Quality.Auto"/> <see cref="Quality"/> mode. Note the caveats about SGSR
         * mentioned in the documentation for <see cref="IsSupported(Conifer.Upscaler.Upscaler.Technique)"/>.</remarks>
         * <example><code>bool dlssSupportsAA = upscaler.IsSupported(
         *   Upscaler.Technique.DeepLearningSuperSampling,
         *   Upscaler.Quality.AntiAliasing
         * );</code></example>
         */
        public static bool IsSupported(Technique type, Quality mode) => type is Technique.None ||
                                                                        (type is Technique.SnapdragonGameSuperResolution1 && SystemInfo.graphicsShaderLevel >= 50 && mode != Quality.AntiAliasing) ||
                                                                        (type is Technique.SnapdragonGameSuperResolution2 && SystemInfo.graphicsShaderLevel >= 50 && SystemInfo.supportsComputeShaders) ||
                                                                        (PluginLoaded() && NativeInterface.IsSupported(type, mode));

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
         * <summary>Is this a spatial <see cref="Technique"/>?</summary>
         * <param name="type">The <see cref="Technique"/> to query.</param>
         * <returns><c>true</c> if the <see cref="Technique"/> is spatial only, <c>false</c> otherwise.</returns>
         * <remarks>This is useful for determining whether motion vectors are required. <see cref="Technique"/>s for
         * which this returns <c>true</c> will usually perform faster and look worse than ones for which this returns
         * <c>false</c>.</remarks>
         * <example><code>bool xessIsSpatial = Upscaler.IsSpatial(Upscaler.Technique.XeSuperSampling);</code>
         * </example>
         */
        public static bool IsSpatial(Technique type) => type switch
            {
                Technique.None or Technique.SnapdragonGameSuperResolution1 => true,
                Technique.DeepLearningSuperSampling or Technique.FidelityFXSuperResolution or Technique.XeSuperSampling or Technique.SnapdragonGameSuperResolution2 => false,
                _ => throw new ArgumentOutOfRangeException(nameof(type), type, type + " is not a valid " + nameof(type) + " enum value.")
            };

        /**
         * <summary>Is <see cref="technique"/> spatial only?</summary>
         * <returns><c>true</c> if <see cref="technique"/> is spatial only, <c>false</c> otherwise.</returns>
         * <remarks>This is a convenience method for <see cref="IsSpatial(Conifer.Upscaler.Upscaler.Technique)"/>. This
         * is useful for determining whether motion vectors are required. <see cref="Technique"/>s for which this
         * returns <c>true</c> will usually perform faster and look worse than ones for which this returns <c>false</c>.
         * </remarks>
         * <example><code>bool isSpatial = upscaler.IsSpatial();</code></example>
         */
        public bool IsSpatial() => IsSpatial(technique);

        /**
         * <summary>Is <see cref="technique"/> spatio-temporal?</summary>
         * <param name="type">The <see cref="Technique"/> to query.</param>
         * <returns><c>true</c> if <see cref="technique"/> is spatio-temporal, <c>false</c> otherwise.</returns>
         * <remarks>This is useful for determining whether motion vectors are required. <see cref="Technique"/>s for
         * which this returns <c>false</c> will usually perform faster and look worse than ones for which this returns
         * <c>true</c>.</remarks>
         * <example><code>bool xessUsesMotionVectors = Upscaler.IsTemporal(Upscaler.Technique.XeSuperSampling);</code>
         * </example>
         */
        public static bool IsTemporal(Technique type) => !IsSpatial(type);

        /**
         * <summary>Is <see cref="technique"/> spatio-temporal?</summary>
         * <returns><c>true</c> if <see cref="technique"/> is spatio-temporal, <c>false</c> otherwise.</returns>
         * <remarks>This is a convenience method for <see cref="IsTemporal(Conifer.Upscaler.Upscaler.Technique)"/>. This
         * is useful for determining whether motion vectors are required. <see cref="Technique"/>s for which this
         * returns <c>false</c> will usually perform faster and look worse than ones for which this returns <c>true</c>.
         * </remarks>
         * <example><code>bool usesMotionVectors = upscaler.IsTemporal();</code></example>
         */
        public bool IsTemporal() => !IsSpatial(technique);

        /**
         * <summary>Does this <see cref="Technique"/> require the <c>GfxPluginUpscaler</c> library to be loaded?
         * </summary>
         * <param name="type">The <see cref="Technique"/> to query.</param>
         * <returns><c>true</c> if this <see cref="Technique"/> cannot be used without loading the
         * <c>GfxPluginUpscaler</c> shared library, <c>false</c> otherwise.</returns>
         * <remarks>This is method is fast.</remarks>
         * <example><code>
         * bool requiresPlugin = Upscaler.RequiresNativePlugin(Upscaler.Technique.FidelityFXSuperResolution);
         * </code></example>
         */
        public static bool RequiresNativePlugin(Technique type) => type switch
            {
                Technique.SnapdragonGameSuperResolution1 or Technique.SnapdragonGameSuperResolution2 or Technique.None => false,
                Technique.DeepLearningSuperSampling or Technique.FidelityFXSuperResolution or Technique.XeSuperSampling => true,
                _ => throw new ArgumentOutOfRangeException(nameof(type), type, type + " is not a valid " + nameof(type) + " enum value.")
            };

        /**
         * <summary>Does <see cref="technique"/> require the <c>GfxPluginUpscaler</c> library to be loaded?</summary>
         * <returns><c>true</c> if <see cref="technique"/> cannot be used without loading the <c>GfxPluginUpscaler</c>
         * shared library, <c>false</c> otherwise.</returns>
         * <remarks>This is method is fast. It is a convenience method for
         * <see cref="RequiresNativePlugin(Conifer.Upscaler.Upscaler.Technique)"/>.</remarks>
         * <example><code>
         * bool requiresPlugin = upscaler.RequiresNativePlugin();
         * </code></example>
         */
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
         * time this method is called all previously recorded messages (usually messages from Upscaler's startup
         * process) will be filtered, then pushed. This method is probably first called by Upscaler's Inspector GUI when
         * in the Unity Editor.</remarks>
         */
        public static void SetLogLevel(LogType level) => NativeInterface.SetLogLevel(level);

        private bool InternalApplySettings(bool force)
        {
            var needsUpdate = force;
            if (force || technique != PreviousTechnique || (technique == Technique.SnapdragonGameSuperResolution2 && method != PreviousMethod))
            {
                needsUpdate = true;
                _backend?.Dispose();
                if (technique == Technique.SnapdragonGameSuperResolution1) _backend = new SnapdragonGameSuperResolutionV1Backend();
                else if (technique == Technique.SnapdragonGameSuperResolution2)
                    _backend = method switch
                    {
                        SgsrMethod.Fragment2Pass => new SnapdragonGameSuperResolutionV2Fragment2PassBackend(),
                        SgsrMethod.Compute2Pass => new SnapdragonGameSuperResolutionV2Compute2PassBackend(),
                        SgsrMethod.Compute3Pass => new SnapdragonGameSuperResolutionV2Compute3PassBackend(),
                        _ => null
                    };
                else _backend = null;
            }
            var outputResolution = Vector2Int.RoundToInt(Camera.pixelRect.size);
            if (outputResolution != OutputResolution) shouldHistoryResetThisFrame = true;
            OutputResolution = outputResolution;
            _backend?.ComputeInputResolutionConstraints(this);
            needsUpdate |= quality != PreviousQuality || OutputResolution != PreviousOutputResolution;
            if (needsUpdate) InputResolution = RecommendedInputResolution;
            return needsUpdate ||
                   InputResolution != PreviousInputResolution ||
                   (technique == Technique.DeepLearningSuperSampling && preset != PreviousPreset) ||
                   (technique == Technique.SnapdragonGameSuperResolution1 && useEdgeDirection != PreviousUseEdgeDirection) ||
                   (technique == Technique.FidelityFXSuperResolution && (
                       autoReactive != PreviousAutoReactive ||
                       Math.Abs(reactiveMax - PreviousReactiveMax) > 0 ||
                       Math.Abs(reactiveScale - PreviousReactiveScale) > 0 ||
                       Math.Abs(reactiveThreshold - PreviousReactiveThreshold) > 0)) ||
                   (technique is Technique.FidelityFXSuperResolution or Technique.SnapdragonGameSuperResolution1 && Math.Abs(sharpness - PreviousSharpness) > 0);
        }

        public bool automaticLODBias = true;
        public float LODBias { get; private set; } = 1;

        internal bool ApplySettings(bool force)
        {
            var needsUpdate = InternalApplySettings(force);
            if (Failure(CurrentStatus))
            {
                if (ErrorCallback is not null)
                {
                    ErrorCallback(CurrentStatus);
                    needsUpdate |= InternalApplySettings(false);
                    if (Failure(CurrentStatus)) Debug.LogError("The registered error handler failed to rectify the following error.");
                }
                Debug.LogWarning("Conifer | Upscaler | Error: " + CurrentStatus, this);
                technique = Technique.None;
                quality = Quality.Auto;
                needsUpdate |= InternalApplySettings(true);
            }

            PreviousInputResolution = InputResolution;
            PreviousOutputResolution = OutputResolution;
            PreviousQuality = quality;
            PreviousTechnique = technique;
            PreviousPreset = preset;
            PreviousMethod = method;
            PreviousUseEdgeDirection = useEdgeDirection;
            PreviousSharpness = sharpness;
            PreviousAutoReactive = autoReactive;
            PreviousReactiveMax = reactiveMax;
            PreviousReactiveScale = reactiveScale;
            PreviousReactiveThreshold = reactiveThreshold;
            PreviousFrameGeneration = frameGeneration;
            PreviousUseAsyncCompute = useAsyncCompute;

            var bias = (float)OutputResolution.x / InputResolution.x;
            if (automaticLODBias) QualitySettings.lodBias = QualitySettings.lodBias / LODBias * bias;
            LODBias = bias;
            return needsUpdate;
        }

        protected void OnEnable()
        {
            Camera = GetComponent<Camera>();
            if (SystemInfo.supportsMotionVectors) Camera.depthTextureMode |= DepthTextureMode.MotionVectors | DepthTextureMode.Depth;
        }

        private void OnDisable()
        {
            _backend?.Dispose();
            _backend = null;
            if (_source != null && _source.IsCreated()) _source.Release();
            if (_destination != null && _destination.IsCreated()) _destination.Release();
        }

        private void OnGUI()
        {
            if (technique == Technique.None || !showRenderingAreaOverlay) return;
            var scale = (float)InputResolution.x / OutputResolution.x;
            GUI.Box(new Rect(0, OutputResolution.y - InputResolution.y, InputResolution.x, InputResolution.y),
                Math.Ceiling(scale * 100) + "% per-axis\n" + Math.Ceiling(scale * scale * 100) + "% total");
        }

        #region BRP Integration

        private static readonly int OpaqueID = Shader.PropertyToID("_CameraOpaqueTexture");
        private CommandBuffer _generateOpaque;
        private CommandBuffer _cleanupOpaque;
        private RenderTexture _source;
        private RenderTexture _destination;

        private static float HaltonSequence(int n, int b)
        {
            var result = 0f;
            var f = 1f / b;
            while (n > 0)
            {
                result += n % b * f;
                n /= b;
                f /= b;
            }
            return result;
        }

        private void OnPreCull()
        {
            var previousTechnique = PreviousTechnique;
            var previousMethod = PreviousMethod;
            var previousAutoReactive = PreviousAutoReactive;
            var needsUpdate = ApplySettings(false);

            if (technique == Technique.None || _backend == null)
            {
                if (_source != null && _source.IsCreated()) _source.Release();
                if (_destination != null && _destination.IsCreated()) _destination.Release();
                return;
            }

            if (needsUpdate)
            {
                if (_destination != null && _destination.IsCreated()) _destination.Release();
                _destination = new RenderTexture(OutputResolution.x, OutputResolution.y, 0, RenderTextureFormat.Default)
                {
                    enableRandomWrite = true  //@todo: Only enable this if needed.
                };
                _destination.Create();
                if (_source != null && _source.IsCreated()) _source.Release();
                _source = new RenderTexture(InputResolution.x, InputResolution.y, 0, RenderTextureFormat.Default);
                _source.Create();
                if (!_backend?.Update(this, _source, _destination) ?? true) return;
                if ((previousTechnique == Technique.SnapdragonGameSuperResolution2 && previousMethod == SgsrMethod.Compute3Pass) ||
                    (previousTechnique == Technique.FidelityFXSuperResolution && previousAutoReactive))
                {
                    Camera.RemoveCommandBuffer(CameraEvent.AfterSkybox, _generateOpaque);
                    Camera.RemoveCommandBuffer(CameraEvent.AfterEverything, _cleanupOpaque);
                    _generateOpaque = null;
                    _cleanupOpaque = null;
                }
                if ((technique == Technique.SnapdragonGameSuperResolution2 && method == SgsrMethod.Compute3Pass) ||
                    (technique == Technique.FidelityFXSuperResolution && autoReactive))
                {
                    _generateOpaque = new CommandBuffer();
                    _generateOpaque.name = "Conifer | Upscaler | Copy Opaque";
                    _generateOpaque.GetTemporaryRT(OpaqueID, InputResolution.x, InputResolution.y, 0, FilterMode.Point, RenderTextureFormat.Default);
                    _generateOpaque.CopyTexture(BuiltinRenderTextureType.CurrentActive, OpaqueID);
                    Camera.AddCommandBuffer(CameraEvent.AfterSkybox, _generateOpaque);
                    _cleanupOpaque = new CommandBuffer();
                    _cleanupOpaque.name = "Conifer | Upscaler | Clean Up Opaque";
                    _cleanupOpaque.ReleaseTemporaryRT(OpaqueID);
                    Camera.AddCommandBuffer(CameraEvent.AfterEverything, _cleanupOpaque);
                }
            }
            Camera.rect = new Rect(0, 0, (float)InputResolution.x / OutputResolution.x, (float)InputResolution.y / OutputResolution.y);

            if (IsSpatial()) return;
            Jitter = new Vector2(HaltonSequence(JitterIndex, 2), HaltonSequence(JitterIndex, 3));
            JitterIndex = (JitterIndex + 1) % (int)Math.Ceiling(8 * Math.Pow((float)OutputResolution.x / InputResolution.x, 2));
            var clipSpaceJitter = Jitter / InputResolution * 2;
            Camera.ResetProjectionMatrix();
            Camera.nonJitteredProjectionMatrix = Camera.projectionMatrix;
            var projectionMatrix = Camera.projectionMatrix;
            if (Camera.orthographic)
            {
                projectionMatrix.m03 += clipSpaceJitter.x;
                projectionMatrix.m13 += clipSpaceJitter.y;
            }
            else
            {
                projectionMatrix.m02 -= clipSpaceJitter.x;
                projectionMatrix.m12 -= clipSpaceJitter.y;
            }
            Camera.projectionMatrix = projectionMatrix;
            Camera.useJitteredProjectionMatrixForTransparentRendering = true;
        }

        private void OnPostRender()
        {
            if (_backend == null || technique == Technique.None) return;
            Camera.rect = new Rect(0, 0, 1, 1);

            // @todo: Move this to a CommandBuffer that uses the BeforeImageEffects CameraEvent.
            Graphics.CopyTexture(Camera.activeTexture, _source);
            _backend.Upscale(this);
        }

        private void OnRenderImage(RenderTexture source, RenderTexture destination)
        {
            if (destination == null)
                if (_backend == null || technique == Technique.None) Graphics.Blit(source, destination);
                else Graphics.Blit(_destination, destination);
            else
                if (_backend == null || technique == Technique.None) Graphics.CopyTexture(source, destination);
                else Graphics.CopyTexture(_destination, destination);
            shouldHistoryResetThisFrame = forceHistoryResetEveryFrame;
        }
        #endregion
    }
}