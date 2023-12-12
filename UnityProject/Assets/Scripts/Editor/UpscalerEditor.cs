using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(Upscaler))]
public class UpscalerEditor : Editor
{
    private SerializedProperty _upscalerType, _qualityMode, _sharpnessVal, _renderScale;
    private bool _basicSettingsFoldout = true;
    private bool _advancedSettingsFoldout;
    private GUIStyle _style;
    private Upscaler _upscalerObject; 
    private Plugin.UpscalerStatus _status;
    private string _message;
    
    private void OnEnable()
    {
        _upscalerType = serializedObject.FindProperty("upscalerMode");
        _qualityMode = serializedObject.FindProperty("qualityMode");
        _sharpnessVal = serializedObject.FindProperty("sharpness");
        _renderScale = serializedObject.FindProperty("renderScale");
        _style = new GUIStyle();
    }

    public override void OnInspectorGUI()
    {
        EditorGUILayout.LabelField("Upscaler Settings");

        _upscalerObject = (Upscaler)serializedObject.targetObject;
        var status = _upscalerObject.Status;
        
        if (status <= Plugin.UpscalerStatus.NoUpscalerSet)
        {
            _style.normal.textColor = Color.green;
            _message = "Upscaling successfully running";
        }
        else
        {
            _style.normal.textColor = Color.red;
            _message = "Upscaler had an Error. Fell back to None.";
        }
        
        EditorGUILayout.LabelField(new GUIContent("Upscaler Status"), new GUIContent(_message));
        
        EditorGUILayout.LabelField(new GUIContent("Status Code"),
            new GUIContent(status.ToString()), _style);

        _basicSettingsFoldout = EditorGUILayout.Foldout(_basicSettingsFoldout, "Basic Upscaler Settings");
        if (_basicSettingsFoldout)
        {
            EditorGUI.indentLevel += 1;
            EditorGUILayout.PropertyField(_upscalerType, new GUIContent("Upscaler mode"));
            EditorGUILayout.PropertyField(_qualityMode, new GUIContent("Quality mode"));
            EditorGUI.indentLevel -= 1;
        }

        _advancedSettingsFoldout = EditorGUILayout.Foldout(_advancedSettingsFoldout, "Advanced Upscaler Settings");
        if (_advancedSettingsFoldout)
        {
            EditorGUI.indentLevel += 1;
            EditorGUILayout.Slider(_sharpnessVal, 0, 1, new GUIContent("Sharpness (Deprecated)"));
            EditorGUI.indentLevel -= 1;
        }

        serializedObject.ApplyModifiedProperties();
    }
}