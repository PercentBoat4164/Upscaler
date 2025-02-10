/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

/**************************************************
 * Upscaler v2.0.0                                *
 * See the UserManual.pdf for more information    *
 **************************************************/

using System;
using System.Linq;
using System.Reflection;
using System.IO;
using Conifer.Upscaler.URP;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

namespace Conifer.Upscaler.Editor
{
    [CustomEditor(typeof(Upscaler))]
    public class UpscalerEditor : UnityEditor.Editor
    {
        private static readonly FieldInfo FRenderDataList = typeof(UniversalRenderPipelineAsset).GetField("m_RendererDataList", BindingFlags.NonPublic | BindingFlags.Instance)!;
        private static readonly FieldInfo FRenderers = typeof(UniversalRenderPipelineAsset).GetField("m_Renderers", BindingFlags.NonPublic | BindingFlags.Instance)!;
        private static readonly FieldInfo FOpaqueDownsampling = typeof(UniversalRenderPipelineAsset).GetField("m_OpaqueDownsampling", BindingFlags.NonPublic | BindingFlags.Instance)!;

        [SerializeField] private bool advancedSettingsFoldout;
        [SerializeField] private bool debugSettingsFoldout;
        [SerializeField] private bool betaSettingsFoldout;

        private SerializedProperty _technique;
        private SerializedProperty _quality;
        private SerializedProperty _frameGeneration;

        private SerializedProperty _dlssPreset;
        private SerializedProperty _sgsrMethod;
        private SerializedProperty _sharpness;
        private SerializedProperty _autoReactive;
        private SerializedProperty _reactiveMax;
        private SerializedProperty _reactiveScale;
        private SerializedProperty _reactiveThreshold;
        private SerializedProperty _useEdgeDirection;

        private SerializedProperty _useAsyncCompute;

        private SerializedProperty _upscalingDebugView;
        private SerializedProperty _showRenderingAreaOverlay;

        private SerializedProperty _frameGenerationDebugView;
        private SerializedProperty _showTearLines;
        private SerializedProperty _showResetIndicator;
        private SerializedProperty _showPacingIndicator;
        private SerializedProperty _onlyPresentGenerated;

        private SerializedProperty _forceHistoryResetEveryFrame;

        private void OnEnable()
        {
            advancedSettingsFoldout = EditorPrefs.GetBool("Conifer:Upscaler:advancedSettingsFoldout", false);
            debugSettingsFoldout = EditorPrefs.GetBool("Conifer:Upscaler:debugSettingsFoldout", false);
            betaSettingsFoldout = EditorPrefs.GetBool("Conifer:Upscaler:betaSettingsFoldout", false);

            _technique = serializedObject.FindProperty("technique");
            _quality = serializedObject.FindProperty("quality");
            _frameGeneration = serializedObject.FindProperty("frameGeneration");

            _dlssPreset = serializedObject.FindProperty("dlssPreset");
            _sgsrMethod = serializedObject.FindProperty("sgsrMethod");
            _sharpness = serializedObject.FindProperty("sharpness");
            _autoReactive = serializedObject.FindProperty("autoReactive");
            _reactiveMax = serializedObject.FindProperty("reactiveMax");
            _reactiveScale = serializedObject.FindProperty("reactiveScale");
            _reactiveThreshold = serializedObject.FindProperty("reactiveThreshold");
            _useEdgeDirection = serializedObject.FindProperty("useEdgeDirection");

            _useAsyncCompute = serializedObject.FindProperty("useAsyncCompute");

            _upscalingDebugView = serializedObject.FindProperty("upscalingDebugView");
            _showRenderingAreaOverlay = serializedObject.FindProperty("showRenderingAreaOverlay");

            _frameGenerationDebugView = serializedObject.FindProperty("frameGenerationDebugView");
            _showTearLines = serializedObject.FindProperty("showTearLines");
            _showResetIndicator = serializedObject.FindProperty("showResetIndicator");
            _showPacingIndicator = serializedObject.FindProperty("showPacingIndicator");
            _onlyPresentGenerated = serializedObject.FindProperty("onlyPresentGenerated");

            _forceHistoryResetEveryFrame = serializedObject.FindProperty("forceHistoryResetEveryFrame");

            var icon = new Texture2D(2, 2);
            icon.LoadImage(File.ReadAllBytes(Application.dataPath + "/Conifer/Upscaler/Editor/ConiferLogo.png"));
            EditorGUIUtility.SetIconForObject(serializedObject.targetObject, icon);
        }

        private void OnDisable()
        {
            EditorPrefs.SetBool("Conifer:Upscaler:advancedSettingsFoldout", advancedSettingsFoldout);
            EditorPrefs.SetBool("Conifer:Upscaler:debugSettingsFoldout", debugSettingsFoldout);
            EditorPrefs.SetBool("Conifer:Upscaler:betaSettingsFoldout", betaSettingsFoldout);
        }

        public override void OnInspectorGUI()
        {
            var upscaler = (Upscaler)serializedObject.targetObject;
            var camera = upscaler.GetComponent<Camera>();
            var cameraData = camera.GetUniversalAdditionalCameraData();
            var activeRenderer = ((ScriptableRendererData[])FRenderDataList.GetValue(UniversalRenderPipeline.asset))
                [(FRenderers.GetValue(UniversalRenderPipeline.asset) as ScriptableRenderer[])!
                    .Select((renderer, index) => new { renderer, index })
                    .First(i => i.renderer == cameraData.scriptableRenderer).index];
            if (activeRenderer.useNativeRenderPass && SystemInfo.graphicsDeviceType == UnityEngine.Rendering.GraphicsDeviceType.Vulkan)
            {
                EditorGUILayout.HelpBox("When using Vulkan, 'Native RenderPass' must be disabled in the active Renderer Data.", MessageType.Error);
                if (GUILayout.Button("Disable 'Native RenderPass'.")) activeRenderer.useNativeRenderPass = false;
            }
            if (activeRenderer.rendererFeatures.Where(feature => feature is UpscalerRendererFeature).ToArray().Length == 0)
                EditorGUILayout.HelpBox("There must be a single UpscalerRendererFeature in this camera's 'Renderer'.", MessageType.Error);
            if (camera.GetComponents<Upscaler>().Length > 1)
            {
                EditorGUILayout.HelpBox("There must be only a single 'Upscaler' component on this camera.",
                    MessageType.Error);
                if (GUILayout.Button("Remove extra Upscaler components."))
                    foreach (var component in camera.GetComponents<Upscaler>().Skip(1))
                        DestroyImmediate(component);
            }
            if (!cameraData.renderPostProcessing)
            {
                EditorGUILayout.HelpBox("'Post Processing' must be enabled.", MessageType.Error);
                if (GUILayout.Button("Enable 'Post Processing'")) cameraData.renderPostProcessing = true;
            }

            if (!Upscaler.IsSupported((Upscaler.Technique)_technique.intValue)) _technique.intValue = (int)Upscaler.GetBestSupportedTechnique();
            _technique.intValue = (int)(Upscaler.Technique)EditorGUILayout.EnumPopup(new GUIContent("Upscaler"), (Upscaler.Technique)_technique.intValue, x => Upscaler.IsSupported((Upscaler.Technique)x), false);

            if ((Upscaler.Technique)_technique.intValue != Upscaler.Technique.None)
            {
                if (upscaler.IsTemporal() && cameraData.antialiasing != AntialiasingMode.None)
                {
                    EditorGUILayout.HelpBox("Set 'Anti-aliasing' to 'No Anti-aliasing' for best results.", MessageType.Warning);
                    if (GUILayout.Button("Set to 'No Anti-aliasing'")) cameraData.antialiasing = AntialiasingMode.None;
                }
                if (camera.allowMSAA)
                {
                    EditorGUILayout.HelpBox("Disallow 'MSAA' for best results.", MessageType.Warning);
                    if (GUILayout.Button("Disallow 'MSAA'")) camera.allowMSAA = false;
                }
            }

            if (upscaler.autoReactive && upscaler.technique == Upscaler.Technique.FidelityFXSuperResolution)
            {
                if (UniversalRenderPipeline.asset?.supportsCameraOpaqueTexture is false)
                {
                    EditorGUILayout.HelpBox("FSR's autoReactive mode requires access to the 'Opaque Texture'.", MessageType.Warning);
                    if (GUILayout.Button("Enable 'Opaque Texture'"))
                        UniversalRenderPipeline.asset.supportsCameraOpaqueTexture = true;
                }

                if (UniversalRenderPipeline.asset?.opaqueDownsampling is not Downsampling.None)
                {
                    EditorGUILayout.HelpBox("Set 'Opaque Downsampling' to 'None' for best results with FSR's autoReactive mode.", MessageType.Warning);
                    if (GUILayout.Button("set 'Opaque Downsampling' to 'None'"))
                        FOpaqueDownsampling.SetValue(UniversalRenderPipeline.asset, Downsampling.None);
                }
            }

            _quality.intValue = (int)(Upscaler.Quality)EditorGUILayout.EnumPopup(new GUIContent("Quality",
                    "Choose a Quality mode for the upscaler. Use Auto to automatically select a Quality mode " +
                    "based on output resolution. The Auto quality mode is guaranteed to be supported for all " +
                    "non-None upscalers. Greyed out options are not available for this upscaler."),
                (Upscaler.Quality)_quality.intValue, x => (Upscaler.Technique)_technique.intValue == Upscaler.Technique.None || upscaler.IsSupported((Upscaler.Quality)x), false);

            var dynamicResolutionSupported = !Equals(upscaler.MaxInputResolution, upscaler.MinInputResolution) || !Application.isPlaying;
            if ((Upscaler.Technique)_technique.intValue != Upscaler.Technique.None && !dynamicResolutionSupported)
                EditorGUILayout.HelpBox("This quality mode does not support Dynamic Resolution.", MessageType.None);

            if ((Upscaler.Technique)_technique.intValue != Upscaler.Technique.XeSuperSampling && (Upscaler.Technique)_technique.intValue != Upscaler.Technique.None)
            {
                EditorGUILayout.Separator();
                EditorGUI.indentLevel += 1;
                advancedSettingsFoldout = EditorGUILayout.Foldout(advancedSettingsFoldout, "Advanced Settings");
                if (advancedSettingsFoldout)
                {
                    switch ((Upscaler.Technique)_technique.intValue)
                    {
                        case Upscaler.Technique.FidelityFXSuperResolution:
                            _sharpness.floatValue = EditorGUILayout.Slider(new GUIContent("Sharpness",
                                    "Controls the amount of RCAS sharpening to apply after upscaling. Too much will produce a dirty, crunchy image. Too little will produce a smooth, blurry image. A good balance will produce a clear, clean image\n\nConifer's default: 0.3f"),
                                _sharpness.floatValue, 0f, 1f);
                            _autoReactive.boolValue = EditorGUILayout.Toggle(new GUIContent("Use Reactive Mask",
                                "Enable the use of an automatically generated reactive mask. This can greatly improve quality if the parameters are refined well for your application."),
                                _autoReactive.boolValue);
                            if (_autoReactive.boolValue)
                            {
                                EditorGUI.indentLevel += 1;
                                _reactiveMax.floatValue = EditorGUILayout.Slider(new GUIContent("Reactivity Max",
                                        "Maximum reactive value. More reactivity favors newer information.\n\nConifer's default: 0.6f"),
                                    _reactiveMax.floatValue, 0, 1.0f);
                                _reactiveScale.floatValue = EditorGUILayout.Slider(new GUIContent("Reactivity Scale",
                                        "Value used to scale reactive mask after generation. Larger values result in more reactive pixels.\n\nConifer's default: 0.9f"),
                                    _reactiveScale.floatValue, 0, 1.0f);
                                _reactiveThreshold.floatValue = EditorGUILayout.Slider(new GUIContent("Reactivity Threshold",
                                        "Minimum reactive threshold. Increase to make more of the image reactive.\n\nConifer's default: 0.3f"),
                                    _reactiveThreshold.floatValue, 0, 1.0f);
                                EditorGUI.indentLevel -= 1;
                            }
                            break;
                        case Upscaler.Technique.DeepLearningSuperSampling:
                            _dlssPreset.intValue = (int)(Upscaler.DlssPreset)EditorGUILayout.EnumPopup(
                                new GUIContent("DLSS Preset",
                                    "Allows choosing a group of DLSS models preselected for optimal quality in various circumstances.\n\n" +
                                    "'Default': The most commonly applicable option. Leaving this option here will probably be fine.\n\n" +
                                    "'Stable': Similar to default. Prioritizes older information for better anti-aliasing quality.\n\n" +
                                    "'Fast Paced': Opposite of 'Stable'. Prioritizes newer information for reduced ghosting.\n\n" +
                                    "'Anti Ghosting': Similar to 'Fast Paced'. Attempts to compensate for objects with missing motion vectors."),
                                (Upscaler.DlssPreset)_dlssPreset.intValue);
                            break;
                        case Upscaler.Technique.SnapdragonGameSuperResolution1:
                                _sharpness.floatValue = EditorGUILayout.Slider(new GUIContent("Sharpness"), _sharpness.floatValue, 0, 1);
                                _useEdgeDirection.boolValue = EditorGUILayout.Toggle(new GUIContent("Use Edge Direction"), _useEdgeDirection.boolValue);
                                break;
                        case Upscaler.Technique.SnapdragonGameSuperResolution2:
                                _sgsrMethod.intValue = (int)(Upscaler.SgsrMethod)EditorGUILayout.EnumPopup(new GUIContent("SGSR Method"), (Upscaler.SgsrMethod)_sgsrMethod.intValue);
                                break;
                        case Upscaler.Technique.None:
                        case Upscaler.Technique.XeSuperSampling:
                        default: break;
                    }
                    if ((Upscaler.Technique)_technique.intValue == Upscaler.Technique.XeSuperSampling || (Upscaler.Technique)_technique.intValue == Upscaler.Technique.None)
                        EditorGUILayout.Separator();
                }
            }

            EditorGUILayout.Separator();
            debugSettingsFoldout = EditorGUILayout.Foldout(debugSettingsFoldout, "Debug Settings");
            if (debugSettingsFoldout)
            {
                if ((Upscaler.Technique)_technique.intValue == Upscaler.Technique.FidelityFXSuperResolution) {
                    _upscalingDebugView.boolValue = EditorGUILayout.Toggle(
                        new GUIContent("View Upscaling Debug Images",
                            "Draws extra views over the scene to help understand the inputs to the upscaling process. (FSR only)"),
                        _upscalingDebugView.boolValue);
                }
                _showRenderingAreaOverlay.boolValue = EditorGUILayout.Toggle(
                    new GUIContent("Overlay Rendering Area",
                        "Overlays a box onto the screen in the OnGUI pass. The box is the same size on-screen as the image that the camera renders into before upscaling."),
                    _showRenderingAreaOverlay.boolValue);
                if (dynamicResolutionSupported)
                {
                    var resolution = EditorGUILayout.Slider(
                        new GUIContent("Dynamic Resolution",
                            "Sets the width of the rendering (input) resolution."),
                        upscaler.InputResolution.x, upscaler.MinInputResolution.x,
                        upscaler.MaxInputResolution.x);
                    upscaler.InputResolution = new Vector2Int((int)Math.Ceiling(resolution),
                        (int)Math.Ceiling(resolution / upscaler.OutputResolution.x * upscaler.OutputResolution.y));
                }
                EditorGUILayout.Separator();
                _forceHistoryResetEveryFrame.boolValue = EditorGUILayout.Toggle(
                    new GUIContent("Force History Reset",
                        "Forces the active upscaler to ignore it's internal history buffer."),
                    _forceHistoryResetEveryFrame.boolValue);
                EditorGUILayout.Separator();
                var logLevel = (LogType)EditorGUILayout.EnumPopup(new GUIContent("Global Log Level", "Sets the log level for all Upscaler instances at once."), (LogType)EditorPrefs.GetInt("Conifer:Upscaler:logLevel", (int)LogType.Warning));
                EditorPrefs.SetInt("Conifer:Upscaler:logLevel", (int)logLevel);
                Upscaler.SetLogLevel(logLevel);
            }

            EditorGUILayout.Separator();
            betaSettingsFoldout = EditorGUILayout.Foldout(betaSettingsFoldout, "Beta Settings");
            if (betaSettingsFoldout)
            {
                if (SystemInfo.graphicsDeviceType != GraphicsDeviceType.Vulkan)
                {
                    EditorGUILayout.HelpBox("Frame generation is currently only available when using the Vulkan Graphics API.", MessageType.Error);
                    _frameGeneration.boolValue = false;
                    GUI.enabled = false;
                }
                _frameGeneration.boolValue = EditorGUILayout.Toggle("FSR Frame Generation", _frameGeneration.boolValue);
                GUI.enabled = _frameGeneration.boolValue;
                _useAsyncCompute.boolValue = EditorGUILayout.Toggle("Use Async Compute", _useAsyncCompute.boolValue);
                EditorGUILayout.Separator();
                _frameGenerationDebugView.boolValue = EditorGUILayout.Toggle(
                    new GUIContent("View Frame Generation Debug Images",
                        "Draws extra views over the scene to help understand the inputs to the frame generation process. (Frame Generation only)"),
                    _frameGenerationDebugView.boolValue);
                _showTearLines.boolValue = EditorGUILayout.Toggle(
                    new GUIContent("Show Tear Lines",
                        "Draws flashing lines on the sides of the screen to make screen tears visually apparent. (Frame Generation only)"),
                    _showTearLines.boolValue);
                _showResetIndicator.boolValue = EditorGUILayout.Toggle(
                    new GUIContent("Show Reset Indicator",
                        "Draws a blue bar across the screen when the frame generation history has been reset. (Frame Generation only)"),
                    _showResetIndicator.boolValue);
                _showPacingIndicator.boolValue = EditorGUILayout.Toggle(
                    new GUIContent("Show Pacing Indicator",
                        "Draws a blue bar across the screen when the frame generation history has been reset. (Frame Generation only)"),
                    _showPacingIndicator.boolValue);
                _onlyPresentGenerated.boolValue = EditorGUILayout.Toggle(
                    new GUIContent("Only Present Generated Frames",
                        "Presents only the generated frames."),
                    _onlyPresentGenerated.boolValue);
                EditorGUILayout.Separator();
                GUI.enabled = true;
            }
            
            EditorGUI.indentLevel -= 1;

            serializedObject.ApplyModifiedProperties();
        }
    }
}