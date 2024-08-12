/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

/**************************************************
 * Upscaler v1.1.0                                *
 * See the OfflineManual.pdf for more information *
 **************************************************/

using System;
using System.Linq;
using System.Net;
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
        private static bool _installationFoldout = !Upscaler.PluginLoaded();
        private bool _advancedSettingsFoldout;
        private bool _debugSettingsFoldout;
        private static readonly FieldInfo FRenderDataList = typeof(UniversalRenderPipelineAsset).GetField("m_RendererDataList", BindingFlags.NonPublic | BindingFlags.Instance)!;
        private static readonly FieldInfo FRenderers = typeof(UniversalRenderPipelineAsset).GetField("m_Renderers", BindingFlags.NonPublic | BindingFlags.Instance)!;
        private static readonly FieldInfo FOpaqueDownsampling = typeof(UniversalRenderPipelineAsset).GetField("m_OpaqueDownsampling", BindingFlags.NonPublic | BindingFlags.Instance)!;
        private static bool _needsRestart;
        private static readonly string DlssLibraryPath = Application.dataPath + "/Plugins/nvngx_dlss.dll";
        [SerializeField] private static uint _dlssUpdateNumber;
        private const uint ThisDlssUpdateNumber = 1;
        private static readonly string XessLibraryPath = Application.dataPath + "/Plugins/libxess.dll";
        [SerializeField] private static uint _xessUpdateNumber;
        private const uint ThisXessUpdateNumber = 1;

        public override void OnInspectorGUI()
        {
            EditorGUI.indentLevel += 1;
            _installationFoldout = EditorGUILayout.Foldout(_installationFoldout, "Third-party Library Installation and Licenses");
            if (_installationFoldout)
            {
                EditorGUILayout.HelpBox("AMD FSR is included with Upscaler and is provided under the MIT license.", MessageType.Info);
                EditorGUILayout.Separator();
                if (EditorGUILayout.LinkButton("See the NVIDIA RTX license.")) Application.OpenURL("https://github.com/NVIDIA/DLSS/blob/main/LICENSE.txt");
                var dlssInstalled = File.Exists(DlssLibraryPath);
                EditorGUILayout.HelpBox(dlssInstalled ? "You have agreed to the NVIDIA RTX license." : "By clicking the below buttons you agree to the above NVIDIA RTX license.", MessageType.Info);
                if (_dlssUpdateNumber != ThisDlssUpdateNumber && GUILayout.Button((dlssInstalled ? "Update" : "Install") + " DLSS Windows library"))
                {
                    new WebClient().DownloadFile("https://github.com/NVIDIA/DLSS/raw/ec405c6443583977a50d5842b244d3e498728f86/lib/Windows_x86_64/rel/nvngx_dlss.dll", DlssLibraryPath);
                    _dlssUpdateNumber = ThisDlssUpdateNumber;
                    _needsRestart = true;
                }
                EditorGUILayout.Separator();
                if (EditorGUILayout.LinkButton("See the Intel Simplified Software License.")) Application.OpenURL("https://github.com/intel/xess/blob/main/licenses/LICENSE.pdf");
                var xessInstalled = File.Exists(XessLibraryPath);
                EditorGUILayout.HelpBox(xessInstalled ? "You have agreed to the Intel Simplified Software License." : "By clicking the below button you agree to the above Intel Simplified Software License.", MessageType.Info);
                if (_xessUpdateNumber != ThisXessUpdateNumber && GUILayout.Button((xessInstalled ? "Update" : "Install") + " XeSS Windows library"))
                {
                    new WebClient().DownloadFile("https://github.com/intel/xess/raw/1d593fd8a2634a06d0d812bd574aa3031313ded0/bin/libxess.dll", XessLibraryPath);
                    _xessUpdateNumber = ThisXessUpdateNumber;
                }
            }
            EditorGUI.indentLevel -= 1;
            if (_needsRestart) EditorGUILayout.HelpBox("You must restart Unity to load the DLSS library.", MessageType.Warning);

            if (!Upscaler.PluginLoaded())
            {
                EditorGUILayout.HelpBox("You may need to restart Unity to load the Upscaler Native Plugin.", MessageType.Error);
                return;
            }

            var upscaler = (Upscaler)serializedObject.targetObject;
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

            if (!Upscaler.IsSupported(upscaler.technique)) upscaler.technique = Upscaler.GetBestSupportedTechnique();
            upscaler.technique = (Upscaler.Technique)EditorGUILayout.EnumPopup(new GUIContent("Upscaler"), upscaler.technique, x => Upscaler.IsSupported((Upscaler.Technique)x), false);

            if (upscaler.technique != Upscaler.Technique.None)
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

            upscaler.quality = (Upscaler.Quality)EditorGUILayout.EnumPopup(new GUIContent("Quality",
                    "Choose a Quality mode for the upscaler. Use Auto to automatically select a Quality mode " +
                    "based on output resolution. The Auto quality mode is guaranteed to be supported for all " +
                    "non-None upscalers. Greyed out options are not available for this upscaler."),
                upscaler.quality, x => upscaler.technique == Upscaler.Technique.None || upscaler.IsSupported((Upscaler.Quality)x), false);

            var dynamicResolutionSupported = !Equals(upscaler.MaxInputResolution, upscaler.MinInputResolution) || !Application.isPlaying;
            if (upscaler.technique != Upscaler.Technique.None && !dynamicResolutionSupported)
                EditorGUILayout.HelpBox("This quality mode does not support Dynamic Resolution.", MessageType.None);
            if (upscaler.technique != Upscaler.Technique.None)
            {
                EditorGUILayout.Separator();
                EditorGUI.indentLevel += 1;
                if (upscaler.technique != Upscaler.Technique.XeSuperSampling)
                {
                    _advancedSettingsFoldout = EditorGUILayout.Foldout(_advancedSettingsFoldout, "Advanced Settings");
                    if (_advancedSettingsFoldout)
                    {
                        switch (upscaler.technique)
                        {
                            case Upscaler.Technique.FidelityFXSuperResolution:
                                upscaler.sharpness = EditorGUILayout.Slider(
                                    new GUIContent("Sharpness"), upscaler.sharpness, 0f, 1f);
                                upscaler.useReactiveMask =
                                    EditorGUILayout.Toggle("Use Reactive Mask", upscaler.useReactiveMask);
                                if (upscaler.useReactiveMask)
                                {
                                    upscaler.reactiveValue = EditorGUILayout.Slider("Reactivity Value", upscaler.reactiveValue, 0, 1.0f);
                                    upscaler.reactiveScale = EditorGUILayout.Slider("Reactivity Scale", upscaler.reactiveScale, 0, 1.0f);
                                    upscaler.reactiveThreshold = EditorGUILayout.Slider("Reactivity Threshold", upscaler.reactiveThreshold, 0, 1.0f);
                                }
                                break;
                            case Upscaler.Technique.DeepLearningSuperSampling:
                                upscaler.dlssPreset = (Upscaler.DlssPreset)EditorGUILayout.EnumPopup(
                                    new GUIContent("DLSS Preset"), upscaler.dlssPreset);
                                break;
                            case Upscaler.Technique.None: break;
                            case Upscaler.Technique.XeSuperSampling: break;
                            default: break;
                        }
                    }
                }
                EditorGUILayout.Separator();
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
                            upscaler.InputResolution.x, upscaler.MinInputResolution.x,
                            upscaler.MaxInputResolution.x);
                        upscaler.InputResolution = new Vector2Int((int)Math.Ceiling(resolution),
                            (int)Math.Ceiling(resolution / upscaler.OutputResolution.x * upscaler.OutputResolution.y));
                    }
                }

                EditorGUI.indentLevel -= 1;
            }

            upscaler.ApplySettings();
        }
    }
}