using System;
using System.Runtime.InteropServices;
using AOT;
using UnityEngine;
using UnityEngine.Experimental.Rendering;

public class EnableDLSS : MonoBehaviour
{
    public enum Type
    {
        NONE,
        DLSS,
    }

    private delegate void DebugCallback(IntPtr message);

    private uint _lastHeight;
    private uint _lastWidth;

    private RenderTexture _renderTarget;
    private RenderTexture _tempTarget;
    private Camera _camera;

    public Type upscaler = Type.DLSS;

    // Start is called before the first frame update
    private void Start()
    {
        Upscaler_InitializePlugin(LogDebugMessage);
        Upscaler_Set(upscaler);
        if (!Upscaler_Initialize())
        {
            Debug.Log("DLSS is not supported.");
            return;
        }

        _camera = GetComponent<Camera>();
        Debug.Log("DLSS is supported.");
    }

    private void OnPreRender()
    {
        if (Screen.width == _lastWidth && Screen.height == _lastHeight || !Upscaler_IsCurrentlyAvailable())
            return;
        _lastWidth = (uint)Screen.width;
        _lastHeight = (uint)Screen.height;
        var size = Upscaler_ResizeTargets(_lastWidth, _lastHeight);
        if (size == 0)
            return;
        var width = (int)(size >> 32);
        var height = (int)(size & 0xFFFFFFFF);

        _renderTarget = new RenderTexture(width, height, 24, DefaultFormat.DepthStencil);
        _renderTarget.Create();
        Upscaler_Prepare(_renderTarget.GetNativeDepthBufferPtr(), _renderTarget.depthStencilFormat);

        _tempTarget = _camera.targetTexture;
        _camera.targetTexture = _renderTarget;
    }

    private void OnPostRender()
    {
        if (!Upscaler_IsCurrentlyAvailable())
            return;
        _camera.targetTexture = _tempTarget;
        Graphics.Blit(_renderTarget, _camera.targetTexture);
    }

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern void Upscaler_InitializePlugin(DebugCallback cb);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern bool Upscaler_Set(Type type);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern bool Upscaler_IsSupported(Type type);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern bool Upscaler_IsCurrentlyAvailable();

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern bool Upscaler_IsAvailable(Type type);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern bool Upscaler_Initialize();

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern ulong Upscaler_ResizeTargets(uint width, uint height);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern bool Upscaler_Prepare(IntPtr buffer, GraphicsFormat format);

    [MonoPInvokeCallback(typeof(DebugCallback))]
    private static void LogDebugMessage(IntPtr message)
    {
        Debug.Log(Marshal.PtrToStringAnsi(message));
    }
}