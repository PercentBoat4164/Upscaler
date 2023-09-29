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
        None,
        DLSS,
    }

    public enum Quality
    {
        Auto,
        UltraQuality,
        Quality,
        Balanced,
        Performance,
        UltraPerformance,
        DynamicAuto,
        DynamicManual,
    }

    private enum Event
    {
        Upscale,
    }

    // Camera
    private Camera _camera;

    // Dynamic Resolution state
    private bool UseDynamicResolution => quality == Quality.DynamicAuto | quality == Quality.DynamicManual;
    private bool _lastUseDynamicResolution;
    private static float _scaleWidth;
    private static float ScaleWidth
    {
        set => _scaleWidth = Math.Max(.5f, Math.Min(1f, value));
        get => _scaleWidth;
    }
    private static float _scaleHeight;
    private static float ScaleHeight
    {
        set => _scaleHeight = Math.Max(.5f, Math.Min(1f, value));
        get => _scaleHeight;
    }
    private uint CameraWidth => (uint)_camera.pixelWidth;
    private uint CameraHeight => (uint)_camera.pixelHeight;
    private Vector2 CameraResolution => new (CameraWidth, CameraHeight);
    private uint UpscalingWidth => _outputTarget != null ? (uint)_outputTarget.width : CameraWidth;
    private uint UpscalingHeight => _outputTarget != null ? (uint)_outputTarget.height : CameraHeight;
    private Vector2 UpscalingResolution => new (UpscalingWidth, UpscalingHeight);
    private Vector2 _lastUpscalingResolution;
    private uint _optimalRenderingWidth;
    private uint _optimalRenderingHeight;
    private uint RenderingWidth => (uint)(UseDynamicResolution ? UpscalingWidth * ScalableBufferManager.widthScaleFactor : _optimalRenderingWidth);
    private uint RenderingHeight => (uint)(UseDynamicResolution ? UpscalingHeight * ScalableBufferManager.heightScaleFactor : _optimalRenderingHeight);
    private Vector2 RenderingResolution => new(RenderingWidth, RenderingHeight);
    private float UpscalingFactor => (float)UpscalingWidth / RenderingWidth;
    private Vector2 _lastRenderingResolution;

    // HDR state
    private bool HDRActive => _camera.allowHDR;
    private bool _lastHDRActive;

    // RenderTextures
    private RenderTexture _outputTarget;
    private RenderTexture _inColorTarget;
    private RenderTexture _motionVectorTarget;

    // CommandBuffers
    private CommandBuffer _setRenderingResolution;
    private CommandBuffer _preUpscale;
    private CommandBuffer _upscale;
    private CommandBuffer _postUpscale;

    // Jitter
    private const uint SamplesPerPixel = 8;
    private uint SequenceLength => (uint)Math.Ceiling(SamplesPerPixel * UpscalingFactor * UpscalingFactor);
    private Vector2[] _jitterSequence;
    private uint _sequencePosition;

    // API
    public Type upscaler = Type.DLSS;
    public Quality quality = Quality.Auto;

    private Vector2 JitterCamera()
    {
        var ndcJitter = _jitterSequence[_sequencePosition++];
        _sequencePosition %= SequenceLength;
        var clipJitter = ndcJitter / new Vector2(RenderingWidth, RenderingHeight);
        _camera.ResetProjectionMatrix();
        var tempProj = _camera.projectionMatrix;
        tempProj.m02 += clipJitter.x;
        tempProj.m12 += clipJitter.y;
        _camera.projectionMatrix = tempProj;
        return ndcJitter;
    }

    private void GenerateJitterSequences()
    {
        _jitterSequence = new Vector2[SequenceLength];
        for (var i = 0; i < 2; i++)
        {
            var seqBase = i + 2;  // Bases 2 and 3 for x and y
            var n = 0;
            var d = 1;

            for (var index = 0; index < _jitterSequence.Length; index++)
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

                _jitterSequence[index][i] = (float)n / d - 0.5f;
            }
        }

        _sequencePosition = 0;
    }

    private static void BlitDepth(CommandBuffer cb, bool toTex, RenderTexture tex, Vector2? scale=null)
    {
        var quad = new Mesh();
        quad.SetVertices(new Vector3[]
        {
            new(-1, -1, 0),
            new(1, -1, 0),
            new(-1, 1, 0),
            new(1, 1, 0)
        });
        quad.SetUVs(0, new Vector2[]
        {
            new(0, 0),
            new(1, 0),
            new(0, 1),
            new(1, 1)
        });
        quad.SetIndices(new[]{2, 1, 0, 1, 2, 3}, MeshTopology.Triangles, 0);
        var material = new Material(Shader.Find(toTex ? "Upscaler/BlitCopyTo" : "Upscaler/BlitCopyFrom"));
        cb.SetProjectionMatrix(Matrix4x4.Ortho(-1, 1, -1, 1, 1, -1));
        cb.SetViewMatrix(Matrix4x4.LookAt(new Vector3(0, 0, -1), new Vector3(0, 0, 1), new Vector3(0, 1, 0)));
        cb.SetRenderTarget(toTex ? tex.depthBuffer : BuiltinRenderTextureType.CameraTarget);
        material.SetVector(Shader.PropertyToID("_ScaleFactor"), scale ?? Vector2.one);
        if (!toTex)
            material.SetTexture(Shader.PropertyToID("_Depth"), tex, RenderTextureSubElement.Depth);
        cb.DrawMesh(quad, Matrix4x4.identity, material);
    }

    private void RecordCommandBuffers()
    {
        var scale = UpscalingResolution / RenderingResolution;
        var inverseScale = RenderingResolution / UpscalingResolution;
        var offset = new Vector2(0, 0);
        var motionScale = UseDynamicResolution ? RenderingResolution : UpscalingResolution;

        _setRenderingResolution.Clear();
        _upscale.Clear();

        if (!UseDynamicResolution)
            _setRenderingResolution.SetViewport(new Rect(0, 0, RenderingWidth, RenderingHeight));

        _upscale.CopyTexture(BuiltinRenderTextureType.MotionVectors, 0, 0, 0, 0, (int)motionScale.x, (int)motionScale.y, _motionVectorTarget, 0, 0, 0, 0);
        _upscale.CopyTexture(BuiltinRenderTextureType.CameraTarget, 0, 0, 0, 0, (int)RenderingWidth, (int)RenderingHeight, _inColorTarget, 0, 0, 0, 0);
        BlitDepth(_upscale, true, _inColorTarget, inverseScale);
        _upscale.SetViewport(new Rect(0, 0, UpscalingWidth, UpscalingHeight));
        _upscale.IssuePluginEvent(Upscaler_GetRenderingEventCallback(), (int)Event.Upscale);
        _upscale.SetRenderTarget(_inColorTarget);
        _upscale.ClearRenderTarget(true, false, Color.black);
        BlitDepth(_upscale, false, _inColorTarget);
        _upscale.CopyTexture(_outputTarget, BuiltinRenderTextureType.CameraTarget);
    }

    private bool ManageTargets()
    {
        var dHDR = _lastHDRActive != HDRActive;
        var dUpscalingResolution = _lastUpscalingResolution != UpscalingResolution;
        var dRenderingResolution = _lastRenderingResolution != RenderingResolution;
        var dDynamicResolution = _lastUseDynamicResolution != UseDynamicResolution;

        var colorFormat = SystemInfo.GetGraphicsFormat(HDRActive ? DefaultFormat.HDR : DefaultFormat.LDR);
        var depthFormat = SystemInfo.GetGraphicsFormat(DefaultFormat.DepthStencil);
        const GraphicsFormat motionFormat = GraphicsFormat.R16G16_SFloat;

        if (dUpscalingResolution)
        {
            Upscaler_SetFramebufferSettings(UpscalingWidth, UpscalingHeight, HDRActive);
            var size = Upscaler_GetRecommendedInputResolution();
            Debug.Log("New output size: " + UpscalingWidth + "x" + UpscalingHeight);
            if (size == 0 && !UseDynamicResolution)
                return true;
            _optimalRenderingWidth = (uint)(size >> 32);
            _optimalRenderingHeight = (uint)(size & 0xFFFFFFFF);
        }

        Upscaler_SetCurrentInputResolution(RenderingWidth, RenderingHeight);

        if (dUpscalingResolution | dRenderingResolution | _jitterSequence == null)
            GenerateJitterSequences();

        if (_outputTarget == null | dHDR | dUpscalingResolution)
        {
            if (_outputTarget != null && _outputTarget.IsCreated())
                _outputTarget.Release();
            if (_outputTarget == null)
            {
                _outputTarget =
                    new RenderTexture((int)UpscalingWidth, (int)UpscalingHeight, 0, colorFormat)
                    {
                        enableRandomWrite = true
                    };
                _outputTarget.Create();
            }
            else if (!_outputTarget.enableRandomWrite)  /*@todo Move me to the validate settings area.*/
            {
                Debug.Log("Set the enableRandomWrite property to `true` before calling `Create` on the RenderTexture used as the `targetTexture` when using an upscaler.");
            }
        }

        // _inColorTarget is the only image affected by Dynamic Resolution. Note that when Dynamic Resolution is enabled
        // the image must be created with enough memory for the Dynamic Resolution Scale to be 1.0. In other words
        // create the image at the resolution of the output target.
        if (_inColorTarget == null | dHDR | dDynamicResolution | UseDynamicResolution ? dUpscalingResolution : dRenderingResolution)
        {
            if (_inColorTarget != null && _inColorTarget.IsCreated())
                _inColorTarget.Release();
            var scale = UseDynamicResolution ? UpscalingResolution : RenderingResolution;
            _inColorTarget =
                new RenderTexture((int)scale.x, (int)scale.y, colorFormat, depthFormat)
                {
                    filterMode = FilterMode.Point,
                    useDynamicScale = UseDynamicResolution
                };
            _inColorTarget.Create();
        }

        if (_motionVectorTarget == null | dUpscalingResolution)
        {
            if (_motionVectorTarget != null && _motionVectorTarget.IsCreated())
                _motionVectorTarget.Release();
            _motionVectorTarget = new RenderTexture((int)UpscalingWidth, (int)UpscalingHeight, 0, motionFormat);
            _motionVectorTarget.Create();
        }

        _lastHDRActive = HDRActive;
        _lastUpscalingResolution = UpscalingResolution;
        _lastRenderingResolution = RenderingResolution;
        _lastUseDynamicResolution = UseDynamicResolution;

        if (!dHDR && !dUpscalingResolution && !dRenderingResolution)
            return false;

        Upscaler_Prepare(_inColorTarget.GetNativeDepthBufferPtr(), _inColorTarget.depthStencilFormat,
            _motionVectorTarget.GetNativeTexturePtr(), _motionVectorTarget.graphicsFormat,
            _inColorTarget.GetNativeTexturePtr(), _inColorTarget.graphicsFormat,
            _outputTarget.GetNativeTexturePtr(), _outputTarget.graphicsFormat
        );
        return true;
    }

    private void OnEnable()
    {
        _camera = GetComponent<Camera>();
        _camera.depthTextureMode |= DepthTextureMode.MotionVectors | DepthTextureMode.Depth;
        Upscaler_InitializePlugin();

        _setRenderingResolution = new CommandBuffer();
        _setRenderingResolution.name = "Set Render Resolution";
        _upscale = new CommandBuffer();
        _upscale.name = "Upscale";

        _camera.AddCommandBuffer( CameraEvent.BeforeGBuffer, _setRenderingResolution);
        _camera.AddCommandBuffer(CameraEvent.BeforeSkybox, _setRenderingResolution);
        _camera.AddCommandBuffer(CameraEvent.BeforeDepthTexture, _setRenderingResolution);
        _camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque, _setRenderingResolution);
        _camera.AddCommandBuffer(CameraEvent.BeforeImageEffectsOpaque, _upscale);
    }

    private void Start()
    {
        Upscaler_Set(upscaler);
        if (Upscaler_Initialize())
        {
            Debug.Log("DLSS is not supported.");
            return;
        }

        Debug.Log("DLSS is supported.");
    }

    private void Update()
    {
        if (Input.GetKeyDown(KeyCode.UpArrow))
        {
            ScaleWidth += .1f;
            ScaleHeight += .1f;
            Debug.Log(ScaleWidth);
        }

        if (Input.GetKeyDown(KeyCode.DownArrow))
        {
            ScaleWidth -= .1f;
            ScaleHeight -= .1f;
            Debug.Log(ScaleWidth);
        }
    }

    private void OnPreCull()
    {
        if (Upscaler_GetError(upscaler) != 0 || upscaler == Type.None)
            return;

        if (UseDynamicResolution)
            ScalableBufferManager.ResizeBuffers(ScaleWidth, ScaleHeight);

        var historyShouldReset = ManageTargets();
        RecordCommandBuffers();

        var jitter = JitterCamera();
        Upscaler_SetJitterInformation(jitter.x, jitter.y, historyShouldReset);
    }

    private void OnPostRender()
    {
        if (!UseDynamicResolution || (ScaleWidth >= 1 && ScaleHeight >= 1) || _camera.targetTexture == null) return;
        ScalableBufferManager.ResizeBuffers(1, 1);
        Graphics.CopyTexture(_outputTarget, _camera.targetTexture);
    }

    private void OnDisable()
    {
        if (_setRenderingResolution != null)
        {
            _camera.RemoveCommandBuffer(CameraEvent.BeforeGBuffer, _setRenderingResolution);
            _camera.RemoveCommandBuffer(CameraEvent.BeforeDepthTexture, _setRenderingResolution);
            _camera.RemoveCommandBuffer(CameraEvent.BeforeForwardOpaque, _setRenderingResolution);
            _camera.RemoveCommandBuffer(CameraEvent.BeforeSkybox, _setRenderingResolution);
            _setRenderingResolution.Release();
        }
        if (_upscale != null)
        {
            _camera.RemoveCommandBuffer(CameraEvent.BeforeImageEffectsOpaque, _upscale);
            _upscale.Release();
        }
    }

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern void Upscaler_InitializePlugin();

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern bool Upscaler_Set(Type type);

    [DllImport("GfxPluginDLSSPlugin")]
    protected static extern int Upscaler_GetError(Type type);

    [DllImport("GfxPluginDLSSPlugin")]
    protected static extern IntPtr Upscaler_GetErrorMessage(Type type);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern int Upscaler_GetCurrentError();

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern IntPtr Upscaler_GetCurrentErrorMessage();

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern bool Upscaler_Initialize();

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern void Upscaler_SetFramebufferSettings(uint width, uint height, bool hdr);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern ulong Upscaler_GetRecommendedInputResolution();

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern int Upscaler_SetCurrentInputResolution(uint width, uint height);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern bool Upscaler_Prepare(
        IntPtr depthBuffer, GraphicsFormat depthFormat,
        IntPtr motionVectors, GraphicsFormat motionVectorFormat,
        IntPtr inColor, GraphicsFormat inColorFormat,
        IntPtr outColor, GraphicsFormat outColorFormat);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern bool Upscaler_SetJitterInformation(float x, float y, bool resetHistory);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern IntPtr Upscaler_GetRenderingEventCallback();
}