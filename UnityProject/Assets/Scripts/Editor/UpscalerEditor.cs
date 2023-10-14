using System;
using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(Upscaler))]
public class UpscalerEditor : Editor
{
    private SerializedProperty _upscalerType, _qualityMode, _sharpnessVal, _widthScale, _heightScale;
    private bool _basicSettingsFoldout = true;
    private bool _advancedSettingsFoldout;
    private GUIStyle _style;
    private Upscaler _upscalerObject; 
    private BackendUpscaler.UpscalerStatus _status;
    private String _message;
    
    private void OnEnable()
    {
        _upscalerType = serializedObject.FindProperty("upscaler");
        _qualityMode = serializedObject.FindProperty("quality");
        _sharpnessVal = serializedObject.FindProperty("sharpness");
        _heightScale = serializedObject.FindProperty("heightScaleFactor");
        _widthScale = serializedObject.FindProperty("widthScaleFactor");
        _style = new GUIStyle();
    }

    public override void OnInspectorGUI()
    {
        serializedObject.Update();

        EditorGUILayout.LabelField("Upscaler Settings");

        _upscalerObject = (Upscaler)serializedObject.targetObject;
        var status = _upscalerObject.Status;
        
        if (status <= BackendUpscaler.UpscalerStatus.NoUpscalerSet)
        {
            _style.normal.textColor = Color.green;
            _message = "Upscaling Mode Successfully Running";
        }
        else
        {
            _style.normal.textColor = Color.red;
            _message = "Upscaler Had an Error. Fell back to No Upscaling.";
        }
        
        EditorGUILayout.LabelField(new GUIContent("Upscaler Status"), new GUIContent(_message));
        
        EditorGUILayout.LabelField(new GUIContent("Status Code"),
            new GUIContent(status.ToString()), _style);

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
            EditorGUILayout.Slider(_sharpnessVal, 0, 1, new GUIContent("Sharpness (Deprecated)"));
            if (_upscalerObject.quality == BackendUpscaler.Quality.DynamicManual)
            {
                EditorGUILayout.Slider(_widthScale, _upscalerObject.MinWidthScaleFactor,
                    _upscalerObject.MaxWidthScaleFactor, new GUIContent("Width Scale Factor (Render Width / Output Width)"));
                EditorGUILayout.Slider(_heightScale, _upscalerObject.MinHeightScaleFactor,
                    _upscalerObject.MaxHeightScaleFactor,
                    new GUIContent("Height Scale Factor (Render height / Output Height)"));
            }
            EditorGUI.indentLevel -= 1;
        }

        serializedObject.ApplyModifiedProperties();
    }
}