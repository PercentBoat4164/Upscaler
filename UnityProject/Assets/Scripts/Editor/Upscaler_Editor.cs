using System;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(Upscaler))]
public class Upscaler_Editor : Editor
{
    private SerializedProperty _upscalerType, _qualityMode;
    private bool _basicSettingsFoldout = true;
    private bool _advancedSettingsFoldout = false;
    private GUIStyle style;
    private BackendUpscaler.UpscalerStatus status;
    private String message;
    
    private void OnEnable()
    {
        _upscalerType = serializedObject.FindProperty("upscaler");
        _qualityMode = serializedObject.FindProperty("quality");
        style = new GUIStyle();
    }

    public override void OnInspectorGUI()
    {
        serializedObject.Update();

        EditorGUILayout.LabelField("Upscaler Settings");

        status = ((Upscaler)serializedObject.targetObject).Status;
        
        if (status <= BackendUpscaler.UpscalerStatus.NoUpscalerSet)
        {
            style.normal.textColor = Color.green;
            message = "Upscaling Mode Successfully Running";
        }
        else
        {
            style.normal.textColor = Color.red;
            message = "Upscaler Had an Error. Fell back to No Upscaling.";
        }
        
        EditorGUILayout.LabelField(new GUIContent("Upscaler Status"), new GUIContent(message));
        
        EditorGUILayout.LabelField(new GUIContent("Status Code"),
            new GUIContent(status.ToString()), style);

        _basicSettingsFoldout = EditorGUILayout.Foldout(_basicSettingsFoldout, "Basic Upscaler Settings");
        if (_basicSettingsFoldout)
        {
            EditorGUI.indentLevel += 1;
            EditorGUILayout.PropertyField(_upscalerType, new GUIContent("Upscaler to Use"));
            EditorGUILayout.PropertyField(_qualityMode, new GUIContent("Quality Performance Mode"));
            EditorGUI.indentLevel -= 1;
        }

        _advancedSettingsFoldout = EditorGUILayout.Foldout(_advancedSettingsFoldout, "Advanced Upscaler Settings");
        if (_advancedSettingsFoldout)
        {
            EditorGUI.indentLevel += 1;
            EditorGUI.indentLevel -= 1;
        }

        serializedObject.ApplyModifiedProperties();
    }
}