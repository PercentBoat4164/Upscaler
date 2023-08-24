using AOT;
using System;
using System.Runtime.InteropServices;
using UnityEngine;

public class EnableDLSS : MonoBehaviour {
    public delegate void debugCallback(IntPtr message);
    [DllImport("DLSSPlugin")] public static extern void SetDebugCallback(debugCallback cb);
    [DllImport("DLSSPlugin")] public static extern bool IsDLSSSupported();
    [DllImport("DLSSPlugin")] public static extern void OnFramebufferResize(long width, long height);

    private int lastWidth = 0;
    private int lastHeight = 0;

    [MonoPInvokeCallback(typeof(debugCallback))]
    void LogDebugMessage(IntPtr message) {
    	Debug.Log(Marshal.PtrToStringAnsi(message));
    }

    // OnEnable is called when the plugin is enabled
    void OnEnable() {
    	SetDebugCallback(LogDebugMessage);
    }
	
    // Start is called before the first frame update
    void Start() {
        if (IsDLSSSupported()) Debug.Log("DLSS is supported.");
        else Debug.Log("DLSS is not supported.");
    }

    // Update is called once per frame
    void Update() {
        if (Screen.width != lastWidth || Screen.height != lastHeight) {
            OnFramebufferResize(Screen.width, Screen.height);
            lastWidth = Screen.width;
            lastHeight = Screen.height;
        }
    }
}
