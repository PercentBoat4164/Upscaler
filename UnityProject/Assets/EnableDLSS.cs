using System;
using System.Collections.Generic;
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
    private CameraJitter _cameraJitter;
    private CommandBuffer _beforeOpaque;
    private CommandBuffer _beforePostProcess;

    public Type upscaler = Type.DLSS;

    private void RecordCommandBuffers()
    {
        //_beforeOpaque.name = "Do not render a fullscreen image";
        //_beforeOpaque.SetViewport(new Rect(0, 0, _presentWidth, _presentHeight));

        //_beforePostProcess.name = "Evaluate upscaler and switch to external render targets";
        // Blit the color and depth to their respective targets from the camera's combined render target.
        //_beforePostProcess.Blit(null, _colorTarget);
        //_beforePostProcess.Blit(null, _depthTarget);
        // // Set the color render target for the upscaler. The depth buffer is already known.
        //_beforePostProcess.SetRenderTarget(_colorTarget);
        // // Upscale
        //_beforePostProcess.IssuePluginEvent(Upscaler_GetRenderingEventCallback(), (int)Event.BEFORE_POSTPROCESSING);
        //_beforePostProcess.SetViewport(new Rect(0, 0, _presentWidth, _presentHeight));
        // Blit the full size color image to the camera's combined render target.
        //_beforePostProcess.Blit(_colorTarget, (RenderTexture)null);
        // Blit and up size the depth buffer to the camera's combined render target.
        //_beforePostProcess.Blit(_depthTarget, (RenderTexture)null);
        //_beforePostProcess.Blit(BuiltinRenderTextureType.MotionVectors, BuiltinRenderTextureType.CameraTarget);
        
        // _beforeOpaque is added in two places to handle forward and deferred rendering
        //_camera.AddCommandBuffer(CameraEvent.BeforeGBuffer, _beforeOpaque);
        // _camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque, _beforeOpaque);
        //_camera.AddCommandBuffer(CameraEvent.BeforeHaloAndLensFlares, _beforePostProcess);
    }

    // Start is called before the first frame update
    private void Start()
    {
        _beforeOpaque = new CommandBuffer();
        _beforePostProcess = new CommandBuffer();
        if (!Upscaler_Initialize())
        {
            Debug.Log("DLSS is not supported.");
            return;
        }

        Debug.Log("DLSS is supported.");
        
        Upscaler_InitializePlugin(LogDebugMessage);
        Upscaler_Set(upscaler);
        _camera = GetComponent<Camera>();
        _cameraJitter = new CameraJitter();
        _camera.depthTextureMode = DepthTextureMode.MotionVectors | DepthTextureMode.Depth;
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

        var jitter = new Tuple<float, float>(0, 0);
        if (Input.GetKey("j"))
            jitter = _cameraJitter.JitterCamera(_camera, (int)(_presentWidth / _renderWidth));

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

    private class CameraJitter
    {
        private const int SamplesPerPixel = 8;
        private float[] _haltonBase2;
        private float[] _haltonBase3;
        private int _cyclePosition;
        private int _prevScaleFactor;
        private List<Tuple<float, float>> _jitterSamples;
        private Tuple<float, float> _lastJitter;

        public Tuple<float, float> JitterCamera(Camera cam, int newScaleFactor)
        {
            if (newScaleFactor != _prevScaleFactor)
            {
                _prevScaleFactor = newScaleFactor;
                var numSamples = SamplesPerPixel * _prevScaleFactor * _prevScaleFactor;
                _haltonBase2 = GenerateHaltonValues(2, numSamples);
                _haltonBase3 = GenerateHaltonValues(3, numSamples);
            }

            var nextJitter = NextJitter();
            cam.ResetProjectionMatrix();
            var tempProj = cam.nonJitteredProjectionMatrix;
            tempProj.m20 += nextJitter.Item1;
            tempProj.m21 += nextJitter.Item2;
            cam.projectionMatrix = tempProj;
            _lastJitter = nextJitter;
            return nextJitter;
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