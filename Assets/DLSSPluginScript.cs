using System;
using System.Runtime.InteropServices;
using AOT;
using UnityEngine;

public class EnableDLSS : MonoBehaviour {
    public delegate void DebugCallback(IntPtr message);
    [DllImport("DLSSPlugin")] public static extern void SetDebugCallback(DebugCallback cb);
    [DllImport("DLSSPlugin")] public static extern bool IsDLSSSupported();
    [DllImport("DLSSPlugin")] public static extern void OnFramebufferResize(long width, long height);
    [DllImport("DLSSPlugin")] public static extern void PrepareDLSS(IntPtr depthBuffer);
    [DllImport("DLSSPlugin")] public static extern void EvaluateDLSS();

    private int _lastWidth;
    private int _lastHeight;

    [MonoPInvokeCallback(typeof(DebugCallback))]
    void LogDebugMessage(IntPtr message) => Debug.Log(Marshal.PtrToStringAnsi(message));

    // OnEnable is called when the plugin is enabled
    void OnEnable() {
    	SetDebugCallback(LogDebugMessage);
    }
	
    // Start is called before the first frame update
    void Start() {
        if (!IsDLSSSupported()) {
            Debug.Log("DLSS is not supported.");
            return;
        }
        Debug.Log("DLSS is supported.");
        // Graphics.activeDepthBuffer.GetNativeRenderBufferPtr();
        var ptr = RenderTexture.active.GetNativeDepthBufferPtr();
        PrepareDLSS(ptr);
    }

    // Update is called once per frame
    void Update() {
        if (Screen.width == _lastWidth && Screen.height == _lastHeight) return;
        OnFramebufferResize(Screen.width, Screen.height);
        _lastWidth = Screen.width;
        _lastHeight = Screen.height;
    }
}
