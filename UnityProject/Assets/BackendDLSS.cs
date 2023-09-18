using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using AOT;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;

public class BackendDLSS : MonoBehaviour
{
    public enum ErrorReason
    {
        DEVICE_NOT_SUPPORTED
    }
    
    public enum Type
    {
        NONE,
        DLSS,
    }
    
    private enum Event
    {
        BEFORE_POSTPROCESSING,
    }
    
    protected Type Upscaler = Type.DLSS; 
    
    private delegate void DebugCallback(IntPtr message);

    private uint _presentHeight;
    private uint _presentWidth;
    private uint _renderWidth;
    private uint _renderHeight;
    private RenderTexture _motionVectorTarget;
    private RenderTexture _inColorTarget;
    private RenderTexture _outColorTarget;
    private RenderTexture _depthTarget;
    private Camera _camera;
    private HaltonJitterer _haltonJitterer;
    private CommandBuffer _beforeOpaque;
    private CommandBuffer _preUpscale;
    private CommandBuffer _upscale;
    private CommandBuffer _postUpscale;
    
    private void SetUpCommandBuffers()
    {
        var scale = new Vector2((float)_presentWidth / _renderWidth, (float)_presentHeight / _renderHeight);
        var inverseScale = new Vector2((float)_renderWidth / _presentWidth, (float)_renderHeight / _presentHeight);
        var offset = new Vector2(0, 0);
        var renderResolution = new Vector2(_renderWidth, _renderHeight);
        var presentResolution = new Vector2(_presentWidth, _presentHeight);

        if (_beforeOpaque != null)
        {
            _camera.RemoveCommandBuffer(CameraEvent.BeforeGBuffer, _beforeOpaque);
            _camera.RemoveCommandBuffer(CameraEvent.BeforeForwardOpaque, _beforeOpaque);
            _camera.RemoveCommandBuffer(CameraEvent.BeforeImageEffectsOpaque, _preUpscale);
            _camera.RemoveCommandBuffer(CameraEvent.BeforeImageEffectsOpaque, _postUpscale);
        }

        _camera.depthTextureMode = DepthTextureMode.MotionVectors | DepthTextureMode.Depth;

        _beforeOpaque = new CommandBuffer();
        _preUpscale = new CommandBuffer();
        _postUpscale = new CommandBuffer();

        _beforeOpaque.name = "Set Render Resolution";
        _beforeOpaque.SetViewport(new Rect(0, 0, _renderWidth, _renderHeight));

        _preUpscale.name = "Copy To Upscaler";
        _preUpscale.Blit(BuiltinRenderTextureType.CameraTarget, _inColorTarget, inverseScale, offset);
        _preUpscale.Blit(BuiltinRenderTextureType.MotionVectors, _motionVectorTarget, inverseScale, offset);
        _preUpscale.Blit(BuiltinRenderTextureType.Depth, _depthTarget, inverseScale, offset);
        _preUpscale.SetViewport(new Rect(0, 0, _presentWidth, _presentHeight));

        _postUpscale.name = "Copy From Upscaler";
        _postUpscale.Blit(_outColorTarget, BuiltinRenderTextureType.CameraTarget);
        _postUpscale.Blit(_depthTarget, BuiltinRenderTextureType.Depth);

        // _beforeOpaque is added in two places to handle forward and deferred rendering
        _camera.AddCommandBuffer(CameraEvent.BeforeGBuffer, _beforeOpaque);
        _camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque, _beforeOpaque);
    }

    private void RecordCommandBuffers()
    {
        if (_upscale != null)
        {
            _camera.RemoveCommandBuffer(CameraEvent.BeforeImageEffectsOpaque, _preUpscale);
            _camera.RemoveCommandBuffer(CameraEvent.BeforeImageEffectsOpaque, _upscale);
            _camera.RemoveCommandBuffer(CameraEvent.BeforeImageEffectsOpaque, _postUpscale);
        }

        _upscale = new CommandBuffer();

        _upscale.name = "Upscale";
        _upscale.IssuePluginEvent(Upscaler_GetRenderingEventCallback(), (int)Event.BEFORE_POSTPROCESSING);

        _camera.AddCommandBuffer(CameraEvent.BeforeImageEffectsOpaque, _preUpscale);
        _camera.AddCommandBuffer(CameraEvent.BeforeImageEffectsOpaque, _upscale);
        _camera.AddCommandBuffer(CameraEvent.BeforeImageEffectsOpaque, _postUpscale);
    }

    // Start is called before the first frame update
    private void Start()
    {
        _camera = GetComponent<Camera>();

        Upscaler_InitializePlugin(LogDebugMessage);
        Upscaler_Set(Upscaler);
        if (!Upscaler_Initialize())
        {
            Debug.Log("DLSS is not supported.");
            return;
        }

        Debug.Log("DLSS is supported.");
        _haltonJitterer = new HaltonJitterer();
    }

    private void OnPreRender()
    {
        if (!Upscaler_IsSupported(Type.DLSS))
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

            _inColorTarget = new RenderTexture((int)_renderWidth, (int)_renderHeight, 0, GraphicsFormat.R8G8B8A8_UNorm);
            _inColorTarget.Create();
            _outColorTarget =
                new RenderTexture((int)_presentWidth, (int)_presentHeight, 0, GraphicsFormat.R8G8B8A8_UNorm)
                {
                    enableRandomWrite = true
                };
            _outColorTarget.Create();
            _motionVectorTarget = new RenderTexture((int)_renderWidth, (int)_renderHeight, 0, GraphicsFormat.R32G32_SFloat);
            _motionVectorTarget.Create();
            _depthTarget = new RenderTexture((int)_renderWidth, (int)_renderHeight, 0, DefaultFormat.DepthStencil)
            {
                filterMode = FilterMode.Point
            };
            _depthTarget.Create();
            Upscaler_Prepare(_depthTarget.GetNativeDepthBufferPtr(), _depthTarget.depthStencilFormat,
                _motionVectorTarget.GetNativeTexturePtr(), _motionVectorTarget.graphicsFormat,
                _inColorTarget.GetNativeTexturePtr(), _inColorTarget.graphicsFormat,
                _outColorTarget.GetNativeTexturePtr(), _outColorTarget.graphicsFormat
            );
            SetUpCommandBuffers();
        }

        RecordCommandBuffers();

        var jitter = _haltonJitterer.JitterCamera(_camera, (int)(_presentWidth / _renderWidth), _renderWidth, _renderHeight);
        Upscaler_SetJitterInformation(jitter.Item1, jitter.Item2);
    }

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern void Upscaler_InitializePlugin(DebugCallback cb);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern bool Upscaler_Set(Type type);

    [DllImport("GfxPluginDLSSPlugin")]
    protected static extern bool Upscaler_IsSupported(Type type);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern bool Upscaler_IsCurrentlyAvailable();

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern bool Upscaler_IsAvailable(Type type);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern bool Upscaler_Initialize();

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern ulong Upscaler_ResizeTargets(uint width, uint height);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern bool Upscaler_Prepare(
        IntPtr depthBuffer, GraphicsFormat depthFormat,
        IntPtr motionVectors, GraphicsFormat motionVectorFormat,
        IntPtr inColor, GraphicsFormat inColorFormat,
        IntPtr outColor, GraphicsFormat outColorFormat);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern bool Upscaler_SetJitterInformation(float x, float y);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern IntPtr Upscaler_GetRenderingEventCallback();

    [MonoPInvokeCallback(typeof(DebugCallback))]
    private static void LogDebugMessage(IntPtr message)
    {
        Debug.Log(Marshal.PtrToStringAnsi(message));
    }

    protected class UpscalerException : Exception
    {
        private readonly ErrorReason _reason;

        public UpscalerException(ErrorReason reason)
        {
            _reason = reason; 
        }

        public override string ToString()
        {
            return base.ToString() + "Upscaling failed. Reason: " + _reason.ToString();
        }
    }
    
    private class HaltonJitterer
    {
        private const int SamplesPerPixel = 8;
        private float[] _haltonBase2;
        private float[] _haltonBase3;
        private int _cyclePosition;
        private int _prevScaleFactor;
        private List<Tuple<float, float>> _jitterSamples;
        private Tuple<float, float> _lastJitter = new(0,0);

        public Tuple<float, float> JitterCamera(Camera cam, int scaleFactor, uint renderWidth, uint renderHeight)
        {
            if (scaleFactor != _prevScaleFactor)
            {
                _prevScaleFactor = scaleFactor;
                var numSamples = SamplesPerPixel * _prevScaleFactor * _prevScaleFactor;
                _haltonBase2 = GenerateHaltonValues(2, numSamples);
                _haltonBase3 = GenerateHaltonValues(3, numSamples);
            }

            var camJitter = NextJitter();
            camJitter = new Tuple<float, float>(camJitter.Item1 / renderWidth, camJitter.Item2 /renderHeight);
            cam.ResetProjectionMatrix();
            var tempProj = cam.projectionMatrix;
            tempProj.m03 += camJitter.Item1 - _lastJitter.Item1;
            tempProj.m13 += camJitter.Item2 - _lastJitter.Item2;
            cam.projectionMatrix = tempProj;
            _lastJitter = camJitter;
            return camJitter;
        }

        private Tuple<float, float> NextJitter()
        {
            if (_cyclePosition >= _haltonBase2.Length)
            {
                _cyclePosition = 0;
            }
            var jitter = new Tuple<float, float>(
                _haltonBase2[_cyclePosition] - 0.5f,
                _haltonBase3[_cyclePosition] - 0.5f);
            _cyclePosition++;
            return jitter;
        }

        private static float[] GenerateHaltonValues(int seqBase, int seqLength)
        {
            var n = 0;
            var d = 1;

            float[] haltonSeq = new float[seqLength];

            for (var index = 0; index < seqLength; index++)
            {
                var x = d - n;
                if (x == 1)
                {
                    n = 1;
                    d *= seqBase;
                }
                else
                {
                    var y = d / seqBase;
                    while (x <= y)
                    {
                        y /= seqBase;
                    }
                    n = (seqBase + 1) * y - x;
                }

                haltonSeq[index] = (float)n / d;
            }

            return haltonSeq;
        }
    }
}