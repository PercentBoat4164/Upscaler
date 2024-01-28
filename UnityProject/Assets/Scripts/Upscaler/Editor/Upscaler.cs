using UnityEditor;
using UnityEngine;
using Upscaler.impl;

namespace Editor
{
    [CustomEditor(typeof(global::Upscaler.Upscaler))]
    public class Upscaler : UnityEditor.Editor
    {
        private bool _basicSettingsFoldout = true;
        private bool _advancedSettingsFoldout;

        public override void OnInspectorGUI()
        {
            var upscalerObject = (global::Upscaler.Upscaler)serializedObject.targetObject;

            EditorGUILayout.LabelField("Upscaler Settings");
            var style = new GUIStyle();
            var status = upscalerObject.Status;
            string message;
            if (Plugin.Success(status))
            {
                style.normal.textColor = Color.green;
                message = "Upscaling successfully running.";
            }
            else
            {
                style.normal.textColor = Color.red;
                message = "Upscaling encountered an Error. Fell back to the 'None' Upscaler.";
            }

            EditorGUILayout.LabelField(new GUIContent("Upscaler Status",
                "Provides a description of the current status of the upscaler in plain English."
            ), new GUIContent(message));
            EditorGUILayout.LabelField(new GUIContent("Status Code",
                    "Indicates the current 'UpscalerStatus' enum value.\n" +
                    "\nThis can also be accessed via the API using the 'Upscaler.Status' property.\n" +
                    "\nNote: Both 'NoUpscalerSet' and 'Success' indicate that the plugin is working as expected."
                ), new GUIContent(status.ToString()), style);


            _basicSettingsFoldout = EditorGUILayout.Foldout(_basicSettingsFoldout, "Basic Upscaler Settings");
            if (_basicSettingsFoldout)
            {
                EditorGUI.indentLevel += 1;
                upscalerObject.upscalerMode = (Plugin.UpscalerMode)EditorGUILayout.EnumPopup(
                    new GUIContent("Upscaler",
                        "Choose an Upscaler to use.\n" +
                        "\nUse None to completely disable upscaling.\n" +
                        "\nUse DLSS to enable DLSS upscaling."
                    ), upscalerObject.upscalerMode);
                upscalerObject.qualityMode = (Plugin.QualityMode)EditorGUILayout.EnumPopup(
                    new GUIContent("Quality",
                        "Choose a Quality Mode for the upscaler.\n" +
                        "\nUse Auto to automatically select a Quality Mode based on output resolution:\n" +
                        "< 1920 x 1080 -> Disabled\n" +
                        "<= 2560 x 1440 -> Quality\n" +
                        "<= 3840 x 2160 -> Performance\n" +
                        "> 3840 x 2160 -> Ultra Performance\n" +
                        "\nUse Quality to upscale by 33.3% on each axis.\n" +
                        "\nUse Balanced to upscale by 42% on each axis.\n" +
                        "\nUse Performance to upscale by 50% on each axis.\n" +
                        "\nUse Ultra Performance to upscale by 66.6% on each axis.\n"
                    ), upscalerObject.qualityMode);
                EditorGUI.indentLevel -= 1;
            }

            _advancedSettingsFoldout = EditorGUILayout.Foldout(_advancedSettingsFoldout, "Advanced Upscaler Settings");
            if (_advancedSettingsFoldout)
            {
                EditorGUI.indentLevel += 1;
                upscalerObject.sharpness = EditorGUILayout.Slider(
                    new GUIContent("Sharpness (Deprecated)",
                        "The amount of sharpening that DLSS should apply to the image.\n" +
                        "\nNote: This only works if DLSS is the the active Upscaler.\n" +
                        "\nNote: This feature is deprecated. NVIDIA suggests shipping your own sharpening solution."
                    ), upscalerObject.sharpness, 0f, 1f);
                EditorGUI.indentLevel -= 1;
            }
        }
    }
}