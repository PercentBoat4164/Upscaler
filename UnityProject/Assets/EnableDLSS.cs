using System;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;

public class EnableDLSS : MonoBehaviour
{
    private const byte ErrorTypeOffset = 29;
    private const byte ErrorCodeOffset = 16;
    private const byte ErrorRecoverable = 1;

    public enum ErrorReason : uint
    {
        ErrorNone = 0U,
        ErrorDummyUpscaler = 2U,
        HardwareError = 1U << ErrorTypeOffset,
        HardwareErrorDeviceExtensionsNotSupported = HardwareError | (1U << ErrorCodeOffset),
        HardwareErrorDeviceNotSupported = HardwareError | (2U << ErrorCodeOffset),
        SoftwareError = 2U << ErrorTypeOffset,
        SoftwareErrorInstanceExtensionsNotSupported = SoftwareError | (1U << ErrorCodeOffset),
        SoftwareErrorDeviceDriversOutOfDate = SoftwareError | (2U << ErrorCodeOffset),
        SoftwareErrorOperatingSystemNotSupported = SoftwareError | (3U << ErrorCodeOffset),

        SoftwareErrorInvalidWritePermissions =
            SoftwareError | (4U << ErrorCodeOffset), // Should be marked as recoverable?
        SoftwareErrorFeatureDenied = SoftwareError | (5U << ErrorCodeOffset),
        SoftwareErrorOutOfGPUMemory = SoftwareError | (6U << ErrorCodeOffset) | ErrorRecoverable,

        /// This likely indicates that a segfault has happened or is about to happen. Abort and avoid the crash if at all possible.
        SoftwareErrorCriticalInternalError = SoftwareError | (7U << ErrorCodeOffset),

        /// The safest solution to handling this error is to stop using the upscaler. It may still work, but all guarantees are void.
        SoftwareErrorCriticalInternalWarning = SoftwareError | (8U << ErrorCodeOffset),

        /// This is an internal error that may have been caused by the user forgetting to call some function. Typically one or more of the initialization functions.
        SoftwareErrorRecoverableInternalWarning = SoftwareError | (9U << ErrorCodeOffset) | ErrorRecoverable,
        SettingsError = (3U << ErrorTypeOffset) | ErrorRecoverable,
        SettingsErrorInputResolutionTooSmall = SettingsError | (1U << ErrorCodeOffset),
        SettingsErrorInputResolutionTooBig = SettingsError | (2U << ErrorCodeOffset),
        SettingsErrorUpscalerNotAvailable = SettingsError | (3U << ErrorCodeOffset),
        SettingsErrorQualityModeNotAvailable = SettingsError | (4U << ErrorCodeOffset),

        /// A GENERIC_ERROR_* is thrown when a most likely cause has been found but it is not certain. A plain GENERIC_ERROR is thrown when there are many possible known errors.
        GenericError = 4U << ErrorTypeOffset,
        GenericErrorDeviceOrInstanceExtensionsNotSupported = GenericError | (1U << ErrorCodeOffset),
        UnknownError = 0xFFFFFFFE
    };

    public enum Upscaler
    {
        None,
        DLSS
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
        DynamicManual
    }

    private enum Event
    {
        Upscale
    }

    // Camera
    private Camera _camera;

    // Dynamic Resolution state
    private bool UseDynamicResolution => (quality == Quality.DynamicAuto) | (quality == Quality.DynamicManual);
    private bool _lastUseDynamicResolution;
    private static float _upscalingFactorWidth = .9f;

    private static float UpscalingFactorWidth
    {
        set => _upscalingFactorWidth = Math.Max(.5f, Math.Min(1f, value));
        get => _upscalingFactorWidth;
    }

    private static float _upscalingFactorHeight = .9f;

    private static float UpscalingFactorHeight
    {
        set => _upscalingFactorHeight = Math.Max(.5f, Math.Min(1f, value));
        get => _upscalingFactorHeight;
    }

    private uint UpscalingWidth =>
        (uint)(_camera.targetTexture != null ? _camera.targetTexture.width : _camera.pixelWidth);

    private uint UpscalingHeight =>
        (uint)(_camera.targetTexture != null ? _camera.targetTexture.height : _camera.pixelHeight);

    private Vector2 UpscalingResolution => new(UpscalingWidth, UpscalingHeight);
    private Vector2 _lastUpscalingResolution;
    private uint _optimalRenderingWidth;
    private uint _optimalRenderingHeight;

    private uint RenderingWidth => (uint)(UseDynamicResolution
        ? UpscalingWidth * ScalableBufferManager.widthScaleFactor
        : _optimalRenderingWidth);

    private uint RenderingHeight => (uint)(UseDynamicResolution
        ? UpscalingHeight * ScalableBufferManager.heightScaleFactor
        : _optimalRenderingHeight);

    private Vector2 RenderingResolution => new(RenderingWidth, RenderingHeight);
    private Vector2 UpscalingFactor => UpscalingResolution / RenderingResolution;
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
    private uint SequenceLength => (uint)Math.Ceiling(SamplesPerPixel * UpscalingFactor.x * UpscalingFactor.y);
    private Vector2[] _jitterSequence;
    private uint _sequencePosition;

    // API
    public Upscaler upscaler = Upscaler.DLSS;
    private Upscaler _lastUpscaler = Upscaler.None;
    public Quality quality = Quality.DynamicAuto;
    private Quality _lastQuality = Quality.Auto;

    private Vector2 JitterCamera()
    {
        var ndcJitter = _jitterSequence[_sequencePosition++];
        _sequencePosition %= SequenceLength;
        var clipJitter = ndcJitter / UpscalingResolution;
        _camera.ResetProjectionMatrix();
        var tempProj = _camera.projectionMatrix;
        tempProj.m02 += clipJitter.x;
        tempProj.m12 += clipJitter.y;
        _camera.projectionMatrix = tempProj;
        Upscaler_SetJitterInformation(ndcJitter.x, ndcJitter.y);
        return ndcJitter;
    }

    private void GenerateJitterSequences()
    {
        _jitterSequence = new Vector2[SequenceLength];
        for (var i = 0; i < 2; i++)
        {
            var seqBase = i + 2; // Bases 2 and 3 for x and y
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
                    while (x <= y) y /= seqBase;

                    n = (seqBase + 1) * y - x;
                }

                _jitterSequence[index][i] = (float)n / d - 0.5f;
            }
        }

        _sequencePosition = 0;
    }

    private static void BlitDepth(CommandBuffer cb, bool toTex, RenderTexture tex, Vector2? scale = null)
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
        quad.SetIndices(new[] { 2, 1, 0, 1, 2, 3 }, MeshTopology.Triangles, 0);
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
        _setRenderingResolution.Clear();
        _upscale.Clear();

        if (upscaler == Upscaler.None) return;

        _setRenderingResolution.SetViewport(new Rect(0, 0, RenderingWidth, RenderingHeight));

        _upscale.CopyTexture(BuiltinRenderTextureType.MotionVectors, 0, 0, 0, 0, (int)UpscalingWidth,
            (int)UpscalingHeight, _motionVectorTarget, 0, 0, 0, 0);
        _upscale.CopyTexture(BuiltinRenderTextureType.CameraTarget, 0, 0, 0, 0, (int)RenderingWidth,
            (int)RenderingHeight, _inColorTarget, 0, 0, 0, 0);
        BlitDepth(_upscale, true, _inColorTarget, RenderingResolution / UpscalingResolution);
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
        var dUpscaler = _lastUpscaler != upscaler;
        var dQuality = _lastQuality != quality;

        var colorFormat = SystemInfo.GetGraphicsFormat(HDRActive ? DefaultFormat.HDR : DefaultFormat.LDR);
        var depthFormat = SystemInfo.GetGraphicsFormat(DefaultFormat.DepthStencil);
        const GraphicsFormat motionFormat = GraphicsFormat.R16G16_SFloat;

        var imagesChanged = false;

        var dDynamicResolution = _lastUseDynamicResolution != UseDynamicResolution;

        if (upscaler != Upscaler.None)
            _camera.allowDynamicResolution =
                false; /*@todo Throw some error if Dynamic Resolution is checked in Unity while some upscaler is active.*/
        else if ((quality == Quality.DynamicAuto) | (quality == Quality.DynamicManual))
            _camera.allowDynamicResolution = true;

        // Resize the buffers.
        if ((quality == Quality.DynamicAuto) |
            _camera.allowDynamicResolution) /*@todo Enable automatic scaling based on GPU frame times if quality == AutoDynamic.*/
            ScalableBufferManager.ResizeBuffers(UpscalingFactorWidth, UpscalingFactorHeight);

        // Initialize any new upscaler
        if (dUpscaler)
            Upscaler_Set(upscaler);

        if (dUpscalingResolution | dHDR | dQuality | dUpscaler && upscaler != Upscaler.None)
        {
            Upscaler_SetFramebufferSettings(UpscalingWidth, UpscalingHeight, quality, HDRActive);
            var size = Upscaler_GetRecommendedInputResolution();
            if (size == 0)
                return true;
            _optimalRenderingWidth = (uint)(size >> 32);
            _optimalRenderingHeight = (uint)(size & 0xFFFFFFFF);
        }

        // This must come after the new recommended rendering resolution has been fetched from the upscaler.
        var dRenderingResolution = _lastRenderingResolution != RenderingResolution;

        if (dRenderingResolution | dUpscalingResolution)
            Debug.Log(RenderingWidth + "x" + RenderingHeight + " -> " + UpscalingWidth + "x" + UpscalingHeight);

        Upscaler_SetCurrentInputResolution(RenderingWidth, RenderingHeight);

        if (dUpscalingResolution | dRenderingResolution | (_jitterSequence == null))
            GenerateJitterSequences();

        if (dHDR | dUpscalingResolution | dUpscaler)
        {
            if (_outputTarget != null && _outputTarget.IsCreated())
            {
                _outputTarget.Release();
                _outputTarget = null;
            }

            if (upscaler != Upscaler.None)
            {
                if (_outputTarget == null)
                {
                    _outputTarget =
                        new RenderTexture((int)UpscalingWidth, (int)UpscalingHeight, 0, colorFormat)
                        {
                            enableRandomWrite = true
                        };
                    _outputTarget.Create();
                }
                else if (!_outputTarget.enableRandomWrite) /*@todo Move me to the validate settings area.*/
                {
                    Debug.Log(
                        "Set the enableRandomWrite property to `true` before calling `Create` on the RenderTexture used as the `targetTexture` when using an upscaler.");
                }

                imagesChanged = true;
            }
        }

        if (dHDR | dDynamicResolution | dUpscalingResolution | (!UseDynamicResolution && dRenderingResolution) |
            dUpscaler)
        {
            if (_inColorTarget != null && _inColorTarget.IsCreated())
            {
                _inColorTarget.Release();
                _inColorTarget = null;
            }

            if (upscaler != Upscaler.None)
            {
                var scale = UseDynamicResolution ? UpscalingResolution : RenderingResolution;
                _inColorTarget =
                    new RenderTexture((int)scale.x, (int)scale.y, colorFormat, depthFormat)
                    {
                        filterMode = FilterMode.Point
                    };
                _inColorTarget.Create();
                imagesChanged = true;
            }
        }

        if (dUpscalingResolution | dUpscaler)
        {
            if (_motionVectorTarget != null && _motionVectorTarget.IsCreated())
            {
                _motionVectorTarget.Release();
                _motionVectorTarget = null;
            }

            if (upscaler != Upscaler.None)
            {
                _motionVectorTarget = new RenderTexture((int)UpscalingWidth, (int)UpscalingHeight, 0, motionFormat);
                _motionVectorTarget.Create();
                imagesChanged = true;
            }
        }

        _lastHDRActive = HDRActive;
        _lastUpscalingResolution = UpscalingResolution;
        _lastRenderingResolution = RenderingResolution;
        _lastUseDynamicResolution = UseDynamicResolution;
        _lastUpscaler = upscaler;
        _lastQuality = quality;

        if (!imagesChanged | (_outputTarget == null) | (_inColorTarget == null) | (_motionVectorTarget == null))
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

        _camera.AddCommandBuffer(CameraEvent.BeforeGBuffer, _setRenderingResolution);
        _camera.AddCommandBuffer(CameraEvent.BeforeSkybox, _setRenderingResolution);
        _camera.AddCommandBuffer(CameraEvent.BeforeDepthTexture, _setRenderingResolution);
        _camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque, _setRenderingResolution);
        _camera.AddCommandBuffer(CameraEvent.BeforeImageEffectsOpaque, _upscale);
    }

    private void Update()
    {
        if (Input.GetKeyDown(KeyCode.UpArrow))
        {
            UpscalingFactorWidth += .1f;
            UpscalingFactorHeight += .1f;
        }

        if (Input.GetKeyDown(KeyCode.DownArrow))
        {
            UpscalingFactorWidth -= .1f;
            UpscalingFactorHeight -= .1f;
        }
    }

    private void OnPreCull()
    {
        if (Upscaler_GetCurrentError() > ErrorReason.ErrorDummyUpscaler)
        {
            Debug.Log(Upscaler_GetCurrentError());
            upscaler = _lastUpscaler == upscaler ? Upscaler.None : _lastUpscaler;
        }

        if (ManageTargets())
            Upscaler_ResetHistory();

        RecordCommandBuffers();

        if (upscaler != Upscaler.None)
            JitterCamera();
    }

    private void OnDisable()
    {
        Upscaler_ShutdownPlugin();

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
    private static extern bool Upscaler_Set(Upscaler upscaler);

    [DllImport("GfxPluginDLSSPlugin")]
    protected static extern ErrorReason Upscaler_GetError(Upscaler upscaler);

    [DllImport("GfxPluginDLSSPlugin")]
    protected static extern IntPtr Upscaler_GetErrorMessage(Upscaler upscaler);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern ErrorReason Upscaler_GetCurrentError();

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern IntPtr Upscaler_GetCurrentErrorMessage();

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern void Upscaler_SetFramebufferSettings(uint width, uint height, Quality quality, bool hdr);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern ulong Upscaler_GetRecommendedInputResolution();

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern ErrorReason Upscaler_SetCurrentInputResolution(uint width, uint height);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern void Upscaler_SetJitterInformation(float x, float y);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern void Upscaler_ResetHistory();

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern bool Upscaler_Prepare(
        IntPtr depthBuffer, GraphicsFormat depthFormat,
        IntPtr motionVectors, GraphicsFormat motionVectorFormat,
        IntPtr inColor, GraphicsFormat inColorFormat,
        IntPtr outColor, GraphicsFormat outColorFormat);

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern void Upscaler_Shutdown();

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern void Upscaler_ShutdownPlugin();

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern IntPtr Upscaler_GetRenderingEventCallback();
}