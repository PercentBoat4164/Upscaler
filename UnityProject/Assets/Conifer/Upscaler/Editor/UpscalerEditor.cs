/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

using System.Diagnostics;
using System.Linq;
using System.Reflection;
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
        private static readonly bool NativePluginLoaded = Process.GetCurrentProcess().Modules.Cast<ProcessModule>().Select(module => module.ModuleName).Contains("GfxPluginUpscaler.dll");

        public override void OnInspectorGUI()
        {
            if (!NativePluginLoaded)
            {
                EditorGUILayout.HelpBox("You must restart Unity to load the Upscaler Native Plugin.",
                    MessageType.Error);
                return;
            }

            var upscalerObject = (Upscaler)serializedObject.targetObject;

            var style = new GUIStyle();
            var status = upscalerObject.status;
            style.normal.textColor = Upscaler.Success(status) ? Color.green : Color.red;

            EditorGUILayout.LabelField(new GUIContent("Status Code",
                "Indicates the current 'UpscalerStatus' enum value.\n" +
                "\nThis can also be accessed via the API using the 'Upscaler.Status' property.\n" +
                "\nNote: Both 'NoUpscalerSet' and 'Success' indicate that the plugin is working as expected."
            ), new GUIContent(status.ToString()), style);

            var newSettings = upscalerObject.QuerySettings();

            var camera = upscalerObject.GetComponent<Camera>();
            var cameraData = camera.GetUniversalAdditionalCameraData();
            var features = ((ScriptableRendererData[])typeof(UniversalRenderPipelineAsset)
                .GetField("m_RendererDataList", BindingFlags.NonPublic | BindingFlags.Instance)!
                .GetValue(UniversalRenderPipeline.asset))[(typeof(UniversalRenderPipelineAsset)
                .GetField("m_Renderers", BindingFlags.NonPublic | BindingFlags.Instance)!
                .GetValue(UniversalRenderPipeline.asset) as ScriptableRenderer[])!
                .Select((renderer, index) => new { renderer, index })
                .First(i => i.renderer == cameraData.scriptableRenderer).index].rendererFeatures;
            if (features.Where(feature => feature is UpscalerRendererFeature).ToArray().Length == 0)
                EditorGUILayout.HelpBox("There must be an UpscalerRendererFeature in this camera's 'Renderer'.", MessageType.Error);
            if (camera.GetComponents<Upscaler>().Length > 1)
            {
                EditorGUILayout.HelpBox("There must be only a single 'Upscaler' component on this camera.",
                    MessageType.Error);
                if (GUILayout.Button("Remove extra Upscaler components."))
                    foreach (var upscaler in camera.GetComponents<Upscaler>().Skip(1))
                        DestroyImmediate(upscaler);
            }
            if (!cameraData.renderPostProcessing)
            {
                EditorGUILayout.HelpBox("'Post Processing' must be enabled.", MessageType.Error);
                if (GUILayout.Button("Enable 'Post Processing'")) cameraData.renderPostProcessing = true;
            }

            newSettings.upscaler = (Settings.Upscaler)EditorGUILayout.EnumPopup(
                new GUIContent("Upscaler",
                    "Choose an Upscaler to use.\n" +
                    "\nUse None to completely disable upscaling.\n" +
                    "\nUse DLSS to enable NVIDIA's Deep Learning Super Sampling upscaling."
                ), newSettings.upscaler);

            if (newSettings.upscaler != Settings.Upscaler.None)
            {
                if ((GraphicsSettings.renderPipelineAsset! as UniversalRenderPipelineAsset)?.upscalingFilter == UpscalingFilterSelection.FSR)
                {
                    EditorGUILayout.HelpBox("The URP Asset's 'Upscaling Filter' must not be set to 'FidelityFX Super Resolution 1.0'.",
                        MessageType.Error);
                    if (GUILayout.Button("Set to 'Automatic'"))
                        (GraphicsSettings.renderPipelineAsset! as UniversalRenderPipelineAsset)!.upscalingFilter =
                            UpscalingFilterSelection.Auto;
                }
                if (cameraData.antialiasing != AntialiasingMode.None)
                {
                    EditorGUILayout.HelpBox("Set 'Anti-aliasing' to 'No Anti-aliasing' for best results.",
                        MessageType.Warning);
                    if (GUILayout.Button("Set to 'No Anti-aliasing'")) cameraData.antialiasing = AntialiasingMode.None;
                }
                if (camera.allowMSAA)
                {
                    EditorGUILayout.HelpBox("Disallow 'MSAA' for best results.", MessageType.Warning);
                    if (GUILayout.Button("Disallow 'MSAA'")) camera.allowMSAA = false;
                }
            }

            EditorGUI.indentLevel += 1;
            newSettings.quality = (Settings.Quality)EditorGUILayout.EnumPopup(
                    new GUIContent("Quality",
                        "Choose a Quality Mode for the upscaler.\n" +
                        "\nUse Auto to automatically select a Quality Mode based on output resolution:\n" +
                        "<= 2560 x 1440 -> Quality\n" +
                        "<= 3840 x 2160 -> Performance\n" +
                        "> 3840 x 2160 -> Ultra Performance\n" +
                        "\nUse Quality to upscale by 33.3% on each axis.\n" +
                        "\nUse Balanced to upscale by 42% on each axis.\n" +
                        "\nUse Performance to upscale by 50% on each axis.\n" +
                        "\nUse Ultra Performance to upscale by 66.6% on each axis.\n"
                    ), newSettings.quality);

            if (newSettings.upscaler != Settings.Upscaler.None && Equals(upscalerObject.MaxRenderScale, upscalerObject.MinRenderScale))
                EditorGUILayout.HelpBox("This quality mode does not support Dynamic Resolution.", MessageType.None);

            if (newSettings.upscaler != Settings.Upscaler.None)
            {
                _advancedSettingsFoldout = EditorGUILayout.Foldout(_advancedSettingsFoldout, "Advanced Upscaler Settings");
                if (_advancedSettingsFoldout)
                {
                    newSettings.sharpness = EditorGUILayout.Slider(
                        new GUIContent("Sharpness (Deprecated)",
                            "The amount of sharpening that DLSS should apply to the image.\n" +
                            "\nNote: This only works if DLSS is the the active Upscaler.\n" +
                            "\nNote: This feature is deprecated. NVIDIA suggests shipping your own sharpening solution."
                        ), newSettings.sharpness, 0f, 1f);
                    if (newSettings.upscaler == Settings.Upscaler.DeepLearningSuperSampling)
                        newSettings.DLSSpreset = (Settings.DLSSPreset)EditorGUILayout.EnumPopup(
                            new GUIContent("DLSS Preset",
                                "For most applications this can be left at Default.\n" +
                                "Use presets when DLSS does not work well with your application by default.\n" +
                                "\nUse Default for the default behaviour.\n" +
                                "\nUse Stable if your application tends to move the contents of the screen slowly. It prefers to keep information from previous frames.\n" +
                                "\nUse Fast Paced if your application tends to move the contents of the screen quickly. It prefers to use information from the current frame.\n" +
                                "\nUse Anti Ghosting if your application fails to provide all of the necessary motion vectors to DLSS."
                            ), newSettings.DLSSpreset);
                }

                _debugSettingsFoldout = EditorGUILayout.Foldout(_debugSettingsFoldout, "Debug Settings");
                if (_debugSettingsFoldout)
                    newSettings.showRenderingAreaOverlay = EditorGUILayout.Toggle("Overlay Rendering Area", newSettings.showRenderingAreaOverlay);
            }
            EditorGUI.indentLevel -= 1;

            upscalerObject.ApplySettings(newSettings);
        }
    }
}