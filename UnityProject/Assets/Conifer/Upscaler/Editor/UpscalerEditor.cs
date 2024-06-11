/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

using System;
using System.Linq;
using System.Reflection;
#if UPSCALER_USE_URP
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
        private bool _advancedSettingsFoldout;
        private bool _debugSettingsFoldout;
        private static readonly FieldInfo FRenderDataList = typeof(UniversalRenderPipelineAsset).GetField("m_RendererDataList", BindingFlags.NonPublic | BindingFlags.Instance)!;
        private static readonly FieldInfo FRenderers = typeof(UniversalRenderPipelineAsset).GetField("m_Renderers", BindingFlags.NonPublic | BindingFlags.Instance)!;
        private static readonly FieldInfo FOpaqueDownsampling = typeof(UniversalRenderPipelineAsset).GetField("m_OpaqueDownsampling", BindingFlags.NonPublic | BindingFlags.Instance)!;

        public override void OnInspectorGUI()
        {
            if (!Upscaler.PluginLoaded())
            {
                EditorGUILayout.HelpBox("You must restart Unity to load the Upscaler Native Plugin.",
                    MessageType.Error);
                return;
            }

            var upscaler = (Upscaler)serializedObject.targetObject;
            var newSettings = upscaler.QuerySettings();
            var camera = upscaler.GetComponent<Camera>();
            var cameraData = camera.GetUniversalAdditionalCameraData();
            var features = ((ScriptableRendererData[])FRenderDataList.GetValue(UniversalRenderPipeline.asset))
                [(FRenderers.GetValue(GraphicsSettings.renderPipelineAsset) as ScriptableRenderer[])!
                    .Select((renderer, index) => new { renderer, index })
                    .First(i => i.renderer == cameraData.scriptableRenderer).index
                ].rendererFeatures;
            if (features.Where(feature => feature is UpscalerRendererFeature).ToArray().Length == 0)
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

            newSettings.upscaler = (Settings.Upscaler)EditorGUILayout.EnumPopup(new GUIContent("Upscaler"), newSettings.upscaler, x => Upscaler.IsSupported((Settings.Upscaler)x), false);

            if (newSettings.upscaler != Settings.Upscaler.None)
            {
                if ((GraphicsSettings.renderPipelineAsset as UniversalRenderPipelineAsset)?.upscalingFilter is UpscalingFilterSelection.FSR)
                {
                    EditorGUILayout.HelpBox("The URP Asset's 'Upscaling Filter' must not be set to 'FidelityFX Super Resolution 1.0'.",
                        MessageType.Error);
                    if (GUILayout.Button("Set to 'Automatic'"))
                        (GraphicsSettings.renderPipelineAsset as UniversalRenderPipelineAsset)!.upscalingFilter =
                            UpscalingFilterSelection.Auto;
                }
                if (cameraData.antialiasing != AntialiasingMode.None)
                {
                    EditorGUILayout.HelpBox("Set 'Anti-aliasing' to 'No Anti-aliasing' for best results.", MessageType.Error);
                    if (GUILayout.Button("Set to 'No Anti-aliasing'")) cameraData.antialiasing = AntialiasingMode.None;
                }
                if (camera.allowMSAA)
                {
                    EditorGUILayout.HelpBox("Disallow 'MSAA' for best results.", MessageType.Warning);
                    if (GUILayout.Button("Disallow 'MSAA'")) camera.allowMSAA = false;
                }
            }
            /* @todo Make our own opaque texture when we need to then remove this warning and the one below it?*/
            if ((GraphicsSettings.renderPipelineAsset as UniversalRenderPipelineAsset)?.supportsCameraOpaqueTexture is false)
            {
                EditorGUILayout.HelpBox("Upscaler requires access the 'Opaque Texture'.", MessageType.Warning);
                if (GUILayout.Button("Enable 'Opaque Texture'"))
                    (GraphicsSettings.renderPipelineAsset as UniversalRenderPipelineAsset)!
                        .supportsCameraOpaqueTexture = true;
            }
            if ((GraphicsSettings.renderPipelineAsset as UniversalRenderPipelineAsset)?.opaqueDownsampling is not Downsampling.None)
            {
                EditorGUILayout.HelpBox("set 'Opaque Downsampling' to 'None' for best results.", MessageType.Warning);
                if (GUILayout.Button("set 'Opaque Downsampling' to 'None'"))
                    FOpaqueDownsampling.SetValue(GraphicsSettings.renderPipelineAsset, Downsampling.None);
            }
            newSettings.quality = (Settings.Quality)EditorGUILayout.EnumPopup(new GUIContent("Quality",
                    "Choose a Quality mode for the upscaler. Use Auto to automatically select a Quality mode " +
                    "based on output resolution. The Auto quality mode is guaranteed to be supported for all " +
                    "non-None upscalers. Greyed out options are not available for this upscaler."),
                newSettings.quality, x => newSettings.upscaler == Settings.Upscaler.None || upscaler.IsSupported((Settings.Quality)x), false);

            var dynamicResolutionSupported = !Equals(upscaler.MaxRenderResolution, upscaler.MinRenderResolution);
            if (newSettings.upscaler != Settings.Upscaler.None && !dynamicResolutionSupported)
                EditorGUILayout.HelpBox("This quality mode does not support Dynamic Resolution.", MessageType.None);

            if (newSettings.upscaler != Settings.Upscaler.None)
            {
                EditorGUI.indentLevel += 1;
                if (newSettings.upscaler != Settings.Upscaler.XeSuperSampling)
                {
                    _advancedSettingsFoldout = EditorGUILayout.Foldout(_advancedSettingsFoldout, "Advanced Settings");
                    if (_advancedSettingsFoldout)
                    {
                        switch (newSettings.upscaler)
                        {
                            case Settings.Upscaler.FidelityFXSuperResolution2:
                            {
                                newSettings.sharpness = EditorGUILayout.Slider(
                                    new GUIContent("Sharpness"), newSettings.sharpness, 0f, 1f);
                                newSettings.useReactiveMask =
                                    EditorGUILayout.Toggle("Use Reactive Mask", newSettings.useReactiveMask);
                                if (newSettings.useReactiveMask)
                                {
                                    newSettings.tcThreshold = EditorGUILayout.Slider("T/C Threshold",
                                        newSettings.tcThreshold, 0, 1.0f);
                                    newSettings.tcScale =
                                        EditorGUILayout.Slider("T/C Scale", newSettings.tcScale, 0, 5.0f);
                                    newSettings.reactiveScale = EditorGUILayout.Slider("Reactivity Scale",
                                        newSettings.reactiveScale, 0, 10.0f);
                                    newSettings.reactiveMax = EditorGUILayout.Slider("Reactivity Max",
                                        newSettings.reactiveMax, 0, 1.0f);
                                }
                                break;
                            }
                            case Settings.Upscaler.DeepLearningSuperSampling:
                                newSettings.DLSSpreset = (Settings.DLSSPreset)EditorGUILayout.EnumPopup(
                                    new GUIContent("DLSS Preset"), newSettings.DLSSpreset);
                                break;
                            case Settings.Upscaler.None: break;
                            case Settings.Upscaler.XeSuperSampling: break;
                            default: break;
                        }
                    }
                }

                _debugSettingsFoldout = EditorGUILayout.Foldout(_debugSettingsFoldout, "Debug Settings");
                if (_debugSettingsFoldout)
                {
                    upscaler.showRenderingAreaOverlay = EditorGUILayout.Toggle(
                        new GUIContent("Overlay Rendering Area",
                            "Overlays a box onto the screen in the OnGUI pass. The box is the same size on-screen as the image that the camera renders into before upscaling."),
                        upscaler.showRenderingAreaOverlay);
                    upscaler.forceHistoryResetEveryFrame = EditorGUILayout.Toggle(
                        new GUIContent("Force History Reset",
                            "Forces the active upscaler to ignore it's internal history buffer."),
                        upscaler.forceHistoryResetEveryFrame);
                    if (dynamicResolutionSupported)
                    {
                        var resolution = EditorGUILayout.Slider(
                            new GUIContent("Dynamic Resolution", "Sets the dynamic resolution values."),
                            upscaler.RenderResolution.x, upscaler.MinRenderResolution.x,
                            upscaler.MaxRenderResolution.x);
                        upscaler.RenderResolution = new Vector2Int((int)Math.Ceiling(resolution),
                            (int)Math.Ceiling(resolution / upscaler.OutputResolution.x * upscaler.OutputResolution.y));
                    }
                }

                EditorGUI.indentLevel -= 1;
            }

            upscaler.ApplySettings(newSettings);
        }
    }
}
#endif