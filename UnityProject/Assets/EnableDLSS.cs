using System;
using System.Collections.Generic;
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
    private CameraJitter _cameraJitter;
    
    // Start is called before the first frame update
    private void Start()
    {
        if (!IsDLSSSupported())
        {
            Debug.Log("DLSS is not supported.");
            return;
        }

        Debug.Log("DLSS is supported.");
        _camera = GetComponent<Camera>();
        _cameraJitter = new CameraJitter(_camera, 1);
    }
    
    private void OnPreRender()
    {
        if (!IsDLSSSupported())
            return;

        if (Screen.width != _lastWidth || Screen.height != _lastHeight)
        {
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
            
            _cameraJitter.Reset((int) _lastWidth / width);
        }

        var jitter = new Tuple<float, float>(0, 0);
        if (Input.GetKey("j"))
            jitter = _cameraJitter.JitterCamera();
        
        //_tempTarget = _camera.targetTexture;
        //_camera.targetTexture = _renderTarget;
    }
    
    private void OnPostRender()
    {
        if (!IsDLSSSupported())
            return;
        //_camera.targetTexture = _tempTarget;
        //Graphics.Blit(_renderTarget, _camera.targetTexture);
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

    private class CameraJitter
    {
        private const int SamplesPerPixel = 8;
        private float[] _haltonBase2;
        private float[] _haltonBase3;
        private int _cyclePosition;
        private int _prevScaleFactor;
        private List<Tuple<float, float>> _jitterSamples;
        private Tuple<float, float> _lastJitter;
        private Camera _cam;
        
        public CameraJitter(Camera cam, int scaleFactorDLSS)
        {
            _cam = cam;
            _lastJitter = new Tuple<float, float>(0, 0);
            Reset(scaleFactorDLSS);
        }

        public void Reset(int newScaleFactorDLSS)
        {
            if (newScaleFactorDLSS == _prevScaleFactor)
                return;
            _prevScaleFactor = newScaleFactorDLSS;
            var numSamples = SamplesPerPixel * newScaleFactorDLSS * newScaleFactorDLSS;
            _haltonBase2 = GenerateHaltonValues(2, numSamples);
            _haltonBase3 = GenerateHaltonValues(3, numSamples);
        }

        public Tuple<float, float> JitterCamera()
        {
            var nextJitter = NextJitter();
            _cam.ResetProjectionMatrix();
            var tempProj = _cam.nonJitteredProjectionMatrix;
            tempProj.m30 += nextJitter.Item1;
            tempProj.m31 += nextJitter.Item2;
            _cam.projectionMatrix = tempProj;
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