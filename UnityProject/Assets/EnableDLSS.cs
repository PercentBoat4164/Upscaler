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

        var jitter = _cameraJitter.JitterCamera();
        
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
        private int _prevScaleFactor;
        private const int SamplesPerPixel = 8;
        private int _cycleLimit;
        private int _cyclePosition;
        private int _n2, _n3;
        private int _d2, _d3;
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
            _cycleLimit = SamplesPerPixel * newScaleFactorDLSS * newScaleFactorDLSS;
            _cyclePosition = 1;
            _n2 = 0;
            _n3 = 0;
            _d2 = 1;
            _d3 = 1;
            Debug.Log("New DLSS Scaling Factor. Unique Jitter Samples: " + _cycleLimit);
        }

        public Tuple<float, float> JitterCamera()
        {
            var nextJitter = NextJitter();
            var tempProj = _cam.projectionMatrix;
            tempProj.m20 += nextJitter.Item1 - _lastJitter.Item1;
            tempProj.m21 += nextJitter.Item2 - _lastJitter.Item2;
            _cam.projectionMatrix = tempProj;
            _lastJitter = nextJitter;
            return nextJitter;
        }
        
        private Tuple<float, float> NextJitter()
        {
            if (_cyclePosition > _cycleLimit)
            {
                _cyclePosition = 1;
                _n2 = 0;
                _n3 = 0;
                _d2 = 1;
                _d3 = 1;
            }
            _cyclePosition += 1;
            return new Tuple<float, float>(
                NextHaltonValue(3, ref _n2, ref _d2) - 0.5f, 
                NextHaltonValue(5, ref _n3, ref _d3) - 0.5f);
        }

        private static float NextHaltonValue(int seqBase, ref int nState, ref int dState)
        {
            var x = dState - nState;
            if (x == 1) 
            {
                nState = 1;
                dState *= seqBase;
            }
            else
            {
                var y = dState / seqBase;
                while (x <= y)
                {
                    y /= seqBase;
                }
                nState = (seqBase + 1) * y - x;
            }
            
            return (float)nState / dState;
        }
    }
}