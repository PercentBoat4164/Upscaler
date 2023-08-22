using AOT;
using System;
using System.Runtime.InteropServices;
using UnityEngine;

public class NewBehaviourScript : MonoBehaviour {
    public delegate void debugCallback(IntPtr message);
    [DllImport("DLSSPlugin")] public static extern void SetDebugCallback(debugCallback cb);
    [DllImport("DLSSPlugin")] public static extern bool InitializeDLSS();
    [DllImport("DLSSPlugin")] public static extern void IdentifyApplication(String appDataPath, UInt64 unityVersion);

    [MonoPInvokeCallback(typeof(debugCallback))]
    void LogDebugMessage(IntPtr message) {
    	Debug.Log(Marshal.PtrToStringAnsi(message));
    }

    // OnEnable is called when the plugin is enabled
    void OnEnable() {
    	SetDebugCallback(LogDebugMessage);
    	IdentifyApplication(Application.dataPath, 1);
    	if (InitializeDLSS()) Debug.Log("DLSS is supported.");
    	else Debug.Log("DLSS is not supported.");
    }
	
    // Start is called before the first frame update
    void Start() {

    }

    // Update is called once per frame
    void Update() {

    }
}
