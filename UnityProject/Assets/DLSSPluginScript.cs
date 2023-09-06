using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using AOT;
using UnityEngine;
using UnityEngine.Rendering;

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
        OnFramebufferResize((uint)Screen.width, (uint)Screen.height);
        _lastWidth = Screen.width;
        _lastHeight = Screen.height;
    }

    // OnEnable is called when the plugin is enabled
    private void OnEnable()
    {
        SetDebugCallback(LogDebugMessage);
    }

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern void SetDebugCallback(DebugCallback cb);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern bool IsDLSSSupported();

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern void OnFramebufferResize(uint width, uint height);

    [DllImport("GfxPluginDLSSPlugin")]
    public static extern void PrepareDLSS(IntPtr depthBuffer);

    [DllImport("GfxPluginDLSSPlugin")]
    public static extern void EvaluateDLSS();

    [MonoPInvokeCallback(typeof(DebugCallback))]
    private static void LogDebugMessage(IntPtr message)
    {
        Debug.Log(Marshal.PtrToStringAnsi(message));
    }
}