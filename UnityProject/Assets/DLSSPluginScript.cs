using System;
using System.Runtime.InteropServices;
using AOT;
using UnityEngine;

public class EnableDLSS : MonoBehaviour
{
    private delegate void DebugCallback(IntPtr message);

    private int _lastHeight;

    private int _lastWidth;

    // Start is called before the first frame update
    private void Start()
    {
        if (!IsDLSSSupported())
        {
            Debug.Log("DLSS is not supported.");
            return;
        }

        Debug.Log("DLSS is supported.");
        // Graphics.activeDepthBuffer.GetNativeRenderBufferPtr();
//         var ptr = RenderTexture.active.GetNativeDepthBufferPtr();
//         PrepareDLSS(ptr);
    }

    // Update is called once per frame
    private void Update()
    {
        if (Screen.width == _lastWidth && Screen.height == _lastHeight) return;
        var width = (uint)Screen.width;
        var height = (uint)Screen.height;
        OnFramebufferResize(LogDebugMessage, width, height);
        _lastWidth = Screen.width;
        _lastHeight = Screen.height;
    }

    // OnEnable is called when the plugin is enabled
    private void OnEnable()
    {
        SetDebugCallback(LogDebugMessage);
    }

    private void OnDisable()
    {
        SetDebugCallback(null);
    }

    [DllImport("DLSSPlugin")]
    private static extern void SetDebugCallback(DebugCallback cb);

    [DllImport("DLSSPlugin")]
    private static extern bool IsDLSSSupported();

    [DllImport("DLSSPlugin")]
    private static extern void OnFramebufferResize(DebugCallback cb, uint width, uint height);

    [DllImport("DLSSPlugin")]
    public static extern void PrepareDLSS(IntPtr depthBuffer);

    [DllImport("DLSSPlugin")]
    public static extern void EvaluateDLSS();

    [MonoPInvokeCallback(typeof(DebugCallback))]
    private void LogDebugMessage(IntPtr message)
    {
        Debug.Log(Marshal.PtrToStringAnsi(message));
    }
}