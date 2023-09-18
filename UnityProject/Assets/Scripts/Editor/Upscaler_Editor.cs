using UnityEditor;
using UnityEngine;

[CustomEditor(typeof(EnableDLSS))]
public class Upscaler_Editor : Editor
{
    private SerializedProperty _upscalerType;
    private bool _basicSettingsFoldout = true;
    private bool _advancedSettingsFoldout = false;
    
    void OnEnable() {
        _upscalerType = serializedObject.FindProperty("upscaler");
    }
 
    public override void OnInspectorGUI() {
        serializedObject.Update();
        
        EditorGUILayout.LabelField("Upscaler Settings");

        _basicSettingsFoldout = EditorGUILayout.Foldout(_basicSettingsFoldout, "Basic Upscaler Settings");
        if(_basicSettingsFoldout)
        {
            EditorGUI.indentLevel += 1;
            EditorGUILayout.PropertyField(_upscalerType, new GUIContent("Upscaler to Use"));
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