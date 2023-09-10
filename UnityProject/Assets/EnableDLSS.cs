using System;
using System.Runtime.InteropServices;
using AOT;
using UnityEngine;
using UnityEngine.Experimental.Rendering;

public class EnableDLSS : MonoBehaviour
{
    private delegate void DebugCallback(IntPtr message);

    private uint _lastHeight;
    private uint _lastWidth;

    private RenderTexture _renderTarget;
    private RenderTexture _tempTarget;
    private Camera _camera;

    // Start is called before the first frame update
    private void Start()
    {
        if (!IsDLSSSupported())
        {
            Debug.Log("DLSS is not supported.");
            return;
        }

        _camera = GetComponent<Camera>();
        Debug.Log("DLSS is supported.");
    }

    private void OnPreRender()
    {
        if (Screen.width == _lastWidth && Screen.height == _lastHeight || !IsDLSSSupported())
            return;
        _lastWidth = (uint)Screen.width;
        _lastHeight = (uint)Screen.height;
        var size = OnFramebufferResize(_lastWidth, _lastHeight);
        if (size == 0)
            return;
        var width = (int)(size >> 32);
        var height = (int)(size & 0xFFFFFFFF);

        _renderTarget = new RenderTexture(width, height, 24, DefaultFormat.DepthStencil);
        _renderTarget.Create();
        setDepthBuffer(_renderTarget.GetNativeDepthBufferPtr(), (uint)_renderTarget.depthStencilFormat);

        _tempTarget = _camera.targetTexture;
        _camera.targetTexture = _renderTarget;
    }

    private void OnPostRender()
    {
        if (!IsDLSSSupported())
            return;
        _camera.targetTexture = _tempTarget;
        Graphics.Blit(_renderTarget, _camera.targetTexture);
    }

    private void OnEnable()
    {
        SetDebugCallback(LogDebugMessage);
    }

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern void setDepthBuffer(IntPtr buffer, uint format);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern void SetDebugCallback(DebugCallback cb);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern bool IsDLSSSupported();

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern ulong OnFramebufferResize(uint width, uint height);

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