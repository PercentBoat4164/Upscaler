using System;
using System.Runtime.InteropServices;
using AOT;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;

public class EnableDLSS : MonoBehaviour
{
    public enum Type
    {
        NONE,
        DLSS,
    }

    private enum Event
    {
        BEFORE_POSTPROCESSING,
    }

    private delegate void DebugCallback(IntPtr message);

    private uint _presentHeight;
    private uint _presentWidth;
    private uint _renderWidth;
    private uint _renderHeight;

    private RenderTexture _colorTarget;
    private RenderTexture _depthTarget;
    private Camera _camera;
    private CommandBuffer _beforeOpaque;
    private CommandBuffer _beforePostProcess;

    public Type upscaler = Type.DLSS;

    private void RecordCommandBuffers()
    {
        _beforeOpaque.name = "Do not render a fullscreen image";
        _beforeOpaque.SetViewport(new Rect(0, 0, _presentWidth, _presentHeight));
        
        _beforePostProcess.name = "Evaluate upscaler and switch to external render targets";
        // Blit the color and depth to their respective targets from the camera's combined render target.
        _beforePostProcess.Blit(null, _colorTarget);
        _beforePostProcess.Blit(null, _depthTarget);
        // // Set the color render target for the upscaler. The depth buffer is already known.
        _beforePostProcess.SetRenderTarget(_colorTarget);
        // // Upscale
        _beforePostProcess.IssuePluginEvent(Upscaler_GetRenderingEventCallback(), (int)Event.BEFORE_POSTPROCESSING);
        _beforePostProcess.SetViewport(new Rect(0, 0, _presentWidth, _presentHeight));
        // Blit the full size color image to the camera's combined render target.
        _beforePostProcess.Blit(_colorTarget, (RenderTexture)null);
        // Blit and up size the depth buffer to the camera's combined render target.
        _beforePostProcess.Blit(_depthTarget, (RenderTexture)null);

        // _beforeOpaque is added in two places to handle forward and deferred rendering
        // _camera.AddCommandBuffer(CameraEvent.BeforeGBuffer, _beforeOpaque);
        // _camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque, _beforeOpaque);
        // _camera.AddCommandBuffer(CameraEvent.BeforeHaloAndLensFlares, _beforePostProcess);
    }

    // Start is called before the first frame update
    private void Start()
    {
        _beforeOpaque = new CommandBuffer();
        _beforePostProcess = new CommandBuffer();
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
        if (!Upscaler_IsCurrentlyAvailable())
            return;

        if (Screen.width != _presentWidth || Screen.height != _presentHeight)
        {
            _presentWidth = (uint)Screen.width;
            _presentHeight = (uint)Screen.height;
            var size = Upscaler_ResizeTargets(_presentWidth, _presentHeight);
            if (size == 0)
                return;
            _renderWidth = (uint)(size >> 32);
            _renderHeight = (uint)(size & 0xFFFFFFFF);

            _colorTarget = new RenderTexture(new RenderTextureDescriptor((int)_renderWidth, (int)_renderHeight));
            _colorTarget.Create();
            _depthTarget = new RenderTexture(new RenderTextureDescriptor((int)_renderWidth, (int)_renderHeight));
            _depthTarget.Create();
            Upscaler_Prepare(_depthTarget.GetNativeDepthBufferPtr(), _depthTarget.depthStencilFormat);
        }

        RecordCommandBuffers();
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

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern IntPtr Upscaler_GetRenderingEventCallback();

    [MonoPInvokeCallback(typeof(DebugCallback))]
    private static void LogDebugMessage(IntPtr message)
    {
        Debug.Log(Marshal.PtrToStringAnsi(message));
    }
}