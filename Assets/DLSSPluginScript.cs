using AOT;
using System;
using System.Runtime.InteropServices;
using UnityEngine;

public class NewBehaviourScript : MonoBehaviour {
    public delegate void debugCallback(IntPtr message);
    [DllImport("DLSSPlugin")] public static extern void SetDebugCallback(debugCallback cb);
    [DllImport("DLSSPlugin")] public static extern void InitializeNGX(String appDataPath);

    [MonoPInvokeCallback(typeof(debugCallback))]
    void LogDebugMessage(IntPtr message) {
    	Debug.Log(Marshal.PtrToStringAnsi(message));
    }

    void OnLoad() {
    }

    // OnEnable is called when the plugin is enabled
    void OnEnable() {
    	SetDebugCallback(LogDebugMessage);
    	InitializeNGX(Application.dataPath);
    }
	
    // Start is called before the first frame update
    void Start() {
    }

    // Update is called once per frame
    void Update() {

    }
}
