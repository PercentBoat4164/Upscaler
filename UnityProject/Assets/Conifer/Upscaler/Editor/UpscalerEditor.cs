/**********************************************************************************************************************
 * Conifer Limited License                                                                                            *
 * This software is provided under a custom limited license, subject to the following terms and conditions:           *
 * Individuals or entities who have purchased this software are granted the right to modify the source code for their *
 *  personal or internal business use only.                                                                           *
 * Redistribution or copying of the source code, in whole or in part, including modified source code, is strictly     *
 *  prohibited without prior written consent from the original author.                                                *
 * Any usage of the source code, including custom modifications, must be accompanied by this license as well as a     *
 *  prominent credit in the final product attributing the original author of the software.                            *
 * The original author reserves all rights not expressly granted herein.                                              *
 * Copyright © 2024 Conifer Computing Company. All rights reserved.                                                   *
 **********************************************************************************************************************/

/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

namespace Conifer.Upscaler.Editor
{
    [CustomEditor(typeof(Upscaler))]
    public class UpscalerEditor : UnityEditor.Editor
    {
        private bool _basicSettingsFoldout = true;
        private bool _advancedSettingsFoldout;

        public override void OnInspectorGUI()
        {
            var upscalerObject = (Upscaler)serializedObject.targetObject;

            EditorGUILayout.LabelField("Upscaler Settings");
            var style = new GUIStyle();
            var status = upscalerObject.status;
            style.normal.textColor = Upscaler.Success(status) ? Color.green : Color.red;

            EditorGUILayout.LabelField(new GUIContent("Status Code",
                "Indicates the current 'UpscalerStatus' enum value.\n" +
                "\nThis can also be accessed via the API using the 'Upscaler.Status' property.\n" +
                "\nNote: Both 'NoUpscalerSet' and 'Success' indicate that the plugin is working as expected."
            ), new GUIContent(status.ToString()), style);

            var newSettings = upscalerObject.QuerySettings();

            if (newSettings.upscaler != Settings.Upscaler.None)
            {
                var camera = upscalerObject.GetComponent<Camera>();
                var cameraData = camera.GetUniversalAdditionalCameraData();
                if ((GraphicsSettings.renderPipelineAsset as UniversalRenderPipelineAsset)?.upscalingFilter != UpscalingFilterSelection.Linear)
                    EditorGUILayout.HelpBox("Set the URP Asset's 'Upscaling Filter' to 'Bilinear'.", MessageType.Error);
                if (cameraData.antialiasing != AntialiasingMode.None)
                    EditorGUILayout.HelpBox("Set 'Anti-aliasing' to 'No Anti-aliasing' for best results.",
                        MessageType.Warning);
                if (camera.allowMSAA)
                    EditorGUILayout.HelpBox("Disallow 'MSAA' for best results.", MessageType.Warning);
                if (!cameraData.renderPostProcessing)
                    EditorGUILayout.HelpBox("'Post Processing' must be enabled.", MessageType.Error);
            }

            _basicSettingsFoldout = EditorGUILayout.Foldout(_basicSettingsFoldout, "Basic Upscaler Settings");
            if (_basicSettingsFoldout)
            {
                EditorGUI.indentLevel += 1;
                newSettings.upscaler = (Settings.Upscaler)EditorGUILayout.EnumPopup(
                    new GUIContent("Upscaler",
                        "Choose an Upscaler to use.\n" +
                        "\nUse None to completely disable upscaling.\n" +
                        "\nUse DLSS to enable NVIDIA's Deep Learning Super Sampling upscaling."
                    ), newSettings.upscaler);
                if (newSettings.upscaler != Settings.Upscaler.None)
                {
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
                }

                if (Equals(upscalerObject.MaxRenderScale, upscalerObject.MinRenderScale))
                    EditorGUILayout.HelpBox("This quality mode does not support Dynamic Resolution.", MessageType.None);
                EditorGUI.indentLevel -= 1;
            }

            if (newSettings.upscaler != Settings.Upscaler.None)
            {
                _advancedSettingsFoldout = EditorGUILayout.Foldout(_advancedSettingsFoldout, "Advanced Upscaler Settings");
                if (_advancedSettingsFoldout)
                {
                    EditorGUI.indentLevel += 1;
                    newSettings.sharpness = EditorGUILayout.Slider(
                        new GUIContent("Sharpness (Deprecated)",
                            "The amount of sharpening that DLSS should apply to the image.\n" +
                            "\nNote: This only works if DLSS is the the active Upscaler.\n" +
                            "\nNote: This feature is deprecated. NVIDIA suggests shipping your own sharpening solution."
                        ), newSettings.sharpness, 0f, 1f);
                    if (newSettings.upscaler == Settings.Upscaler.DLSS)
                    {
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
                    EditorGUI.indentLevel -= 1;
                }
            }

            upscalerObject.ApplySettings(newSettings);
        }
    }
}