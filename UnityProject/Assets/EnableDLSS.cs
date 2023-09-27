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

    private delegate void DebugCallback(IntPtr message);
    
    // Camera
    private Camera _camera;

    // Dynamic Resolution state
    private bool UseDynamicResolution => quality == Quality.DynamicAuto | quality == Quality.DynamicManual;
    private uint CameraWidth => (uint)_camera.pixelWidth;
    private uint CameraHeight => (uint)_camera.pixelHeight;
    private Vector2 CameraResolution => new (CameraWidth, CameraHeight);
    private uint UpscalingWidth => _outputTarget != null ? (uint)_outputTarget.width : CameraWidth;
    private uint UpscalingHeight => _outputTarget != null ? (uint)_outputTarget.height : CameraHeight;
    private Vector2 UpscalingResolution => new (UpscalingWidth, UpscalingHeight);
    private uint _optimalRenderingWidth;
    private uint _optimalRenderingHeight;
    private uint RenderingWidth => (uint)(UseDynamicResolution ? _camera.pixelWidth * ScalableBufferManager.widthScaleFactor : _optimalRenderingWidth);
    private uint RenderingHeight => (uint)(UseDynamicResolution ? _camera.pixelHeight * ScalableBufferManager.heightScaleFactor : _optimalRenderingHeight);
    private Vector2 RenderingResolution => new(RenderingWidth, RenderingHeight);
    private float ScalingFactor => (float)UpscalingWidth / RenderingWidth;
    private Vector2 _lastUpscaledResolution;
    private Vector2 _lastRenderingResolution;

    // HDR state
    private bool HDRActive => _camera.allowHDR;
    private bool _lastHDRActive;

    // RenderTextures
    private RenderTexture _outputTarget;
    private RenderTexture _inColorTarget;
    private RenderTexture _motionVectorTarget;

    // CommandBuffers
    private CommandBuffer _beforeOpaque;
    private CommandBuffer _preUpscale;
    private CommandBuffer _upscale;
    private CommandBuffer _postUpscale;

    // Jitter
    private const uint SamplesPerPixel = 8;
    private uint SequenceLength => (uint)Math.Ceiling(SamplesPerPixel * ScalingFactor * ScalingFactor);
    private Vector2[] _jitterSequence;
    private uint _sequencePosition;

    // API
    public Type upscaler = Type.DLSS;
    public Quality quality = Quality.Auto;

    private Vector2 JitterCamera()
    {
        var ndcJitter = _jitterSequence[_sequencePosition++];
        _sequencePosition %= SequenceLength;
        var clipJitter = ndcJitter / new Vector2(RenderingWidth, RenderingHeight) * 2;
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

    private static void BlitDepth(CommandBuffer cb, bool to, RenderTexture tex, Vector2? scale=null)
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
        var material = new Material(Shader.Find(to ? "Upscaler/BlitCopyTo" : "Upscaler/BlitCopyFrom"));
        cb.SetProjectionMatrix(Matrix4x4.Ortho(-1, 1, -1, 1, 1, -1));
        cb.SetViewMatrix(Matrix4x4.LookAt(new Vector3(0, 0, -1), new Vector3(0, 0, 1), new Vector3(0, 1, 0)));
        cb.SetRenderTarget(to ? tex.depthBuffer : BuiltinRenderTextureType.CameraTarget);
        material.SetVector(Shader.PropertyToID("_ScaleFactor"), scale ?? Vector2.one);
        if (!to)
            material.SetTexture(Shader.PropertyToID("_Depth"), tex, RenderTextureSubElement.Depth);
        cb.DrawMesh(quad, Matrix4x4.identity, material);
    }

    private void RecordCommandBuffers()
    {
        var scale = UpscalingResolution / RenderingResolution;
        var inverseScale = RenderingResolution / UpscalingResolution;
        var offset = new Vector2(0, 0);

        if (_beforeOpaque != null)
        {
            _camera.RemoveCommandBuffer(CameraEvent.BeforeGBuffer, _beforeOpaque);
            _camera.RemoveCommandBuffer(CameraEvent.BeforeDepthTexture, _beforeOpaque);
            _camera.RemoveCommandBuffer(CameraEvent.BeforeForwardOpaque, _beforeOpaque);
            _beforeOpaque.Release();
        }
        if (_upscale != null)
        {
            _camera.RemoveCommandBuffer(CameraEvent.BeforeImageEffectsOpaque, _upscale);
            _upscale.Release();
        }

        _upscale = new CommandBuffer();
        _beforeOpaque = new CommandBuffer();

        _beforeOpaque.name = "Set Render Resolution";
        _beforeOpaque.SetViewport(new Rect(0, 0, RenderingWidth, RenderingHeight));

        _upscale.name = "Upscale";
        _upscale.SetViewport(new Rect(0, 0, UpscalingWidth, UpscalingHeight));
        _upscale.Blit(BuiltinRenderTextureType.MotionVectors, _motionVectorTarget);
        _upscale.CopyTexture(BuiltinRenderTextureType.CameraTarget, 0, 0, 0, 0, (int)RenderingWidth, (int)RenderingHeight, _inColorTarget, 0, 0, 0, 0);
        // _upscale.Blit(BuiltinRenderTextureType.CameraTarget, _inColorTarget, inverseScale, offset);
        BlitDepth(_upscale, true, _inColorTarget, inverseScale);
        _upscale.SetRenderTarget(BuiltinRenderTextureType.CameraTarget);
        _upscale.ClearRenderTarget(true, false, Color.black);
        _upscale.IssuePluginEvent(Upscaler_GetRenderingEventCallback(), (int)Event.Upscale);
        BlitDepth(_upscale, false, _inColorTarget);
        _upscale.CopyTexture(_outputTarget, BuiltinRenderTextureType.CameraTarget);
        // _upscale.Blit(_outputTarget, BuiltinRenderTextureType.CameraTarget);

        // _beforeOpaque is added in multiple places to handle forward and deferred rendering
        _camera.AddCommandBuffer(CameraEvent.BeforeGBuffer, _beforeOpaque);
        _camera.AddCommandBuffer(CameraEvent.BeforeDepthTexture, _beforeOpaque);
        _camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque, _beforeOpaque);

        _camera.AddCommandBuffer(CameraEvent.BeforeImageEffectsOpaque, _upscale);
    }

    private bool ManageTargets()
    {
        var dHDR = _lastHDRActive != HDRActive;
        var dUpscaledResolution = _lastUpscaledResolution != UpscalingResolution;
        var dRenderingResolution = _lastRenderingResolution != RenderingResolution;

        var colorFormat = SystemInfo.GetGraphicsFormat(HDRActive ? DefaultFormat.HDR : DefaultFormat.LDR);
        var depthFormat = SystemInfo.GetGraphicsFormat(DefaultFormat.DepthStencil);
        const GraphicsFormat motionFormat = GraphicsFormat.R16G16_SFloat;

        if (dUpscaledResolution && !UseDynamicResolution)
        {
            var size = Upscaler_ResizeTargets(UpscalingWidth, UpscalingHeight, HDRActive);
            if (size == 0)
                return true;
            _optimalRenderingWidth = (uint)(size >> 32);
            _optimalRenderingHeight = (uint)(size & 0xFFFFFFFF);
        }

        if (dUpscaledResolution | dRenderingResolution)
            GenerateJitterSequences();

        if (_outputTarget == null | dHDR | dUpscaledResolution)
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

        if (_inColorTarget == null | dHDR | dRenderingResolution)
        {
            if (_inColorTarget != null && _inColorTarget.IsCreated())
                _inColorTarget.Release();
            _inColorTarget =
                new RenderTexture((int)RenderingWidth, (int)RenderingHeight, colorFormat, depthFormat)
                {
                    filterMode = FilterMode.Point
                };
            _inColorTarget.Create();
        }

        if (_motionVectorTarget == null | dRenderingResolution)
        {
            if (_motionVectorTarget != null && _motionVectorTarget.IsCreated())
                _motionVectorTarget.Release();
            _motionVectorTarget = new RenderTexture((int)RenderingWidth, (int)RenderingHeight, 0, motionFormat);
            _motionVectorTarget.Create();
        }

        _lastRenderingResolution = RenderingResolution;
        _lastUpscaledResolution = UpscalingResolution;
        _lastHDRActive = HDRActive;

        if (!dHDR && !dUpscaledResolution && !dRenderingResolution)
            return false;

        Upscaler_Prepare(_inColorTarget.GetNativeDepthBufferPtr(), _inColorTarget.depthStencilFormat,
            _motionVectorTarget.GetNativeTexturePtr(), _motionVectorTarget.graphicsFormat,
            _inColorTarget.GetNativeTexturePtr(), _inColorTarget.graphicsFormat,
            _outputTarget.GetNativeTexturePtr(), _outputTarget.graphicsFormat
        );
        return true;
    }

    private void Start()
    {
        _camera = GetComponent<Camera>();
        _camera.depthTextureMode |= DepthTextureMode.MotionVectors | DepthTextureMode.Depth;
        GenerateJitterSequences();

        Upscaler_InitializePlugin(LogDebugMessage);
        Upscaler_Set(upscaler);
        if (!Upscaler_Initialize())
        {
            Debug.Log("DLSS is not supported.");
            return;
        }

        Debug.Log("DLSS is supported.");
    }

    private void OnPreCull()
    {
        if (!Upscaler_IsSupported(upscaler) || upscaler == Type.None)
            return;

        var historyShouldReset = ManageTargets();
        RecordCommandBuffers();

        var jitter = JitterCamera();
        Upscaler_SetJitterInformation(jitter.x, jitter.y, historyShouldReset);
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
    private static extern ulong Upscaler_ResizeTargets(uint width, uint height, bool HDR);

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

    [MonoPInvokeCallback(typeof(DebugCallback))]
    private static void LogDebugMessage(IntPtr message)
    {
        Debug.Log(Marshal.PtrToStringAnsi(message));
    }
}