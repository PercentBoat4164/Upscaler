using System;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;

public class BackendUpscaler : MonoBehaviour
{
    // Camera
    protected Camera Camera;

    // Dynamic Resolution state
    private bool UseDynamicResolution =>
        (ActiveQuality == Plugin.Quality.DynamicAuto) | (ActiveQuality == Plugin.Quality.DynamicManual);
    private bool _lastUseDynamicResolution;

    // Upscaling Resolution
    protected Vector2Int UpscalingResolution => new(Camera.targetTexture != null ? Camera.targetTexture.width : Camera.pixelWidth, Camera.targetTexture != null ? Camera.targetTexture.height : Camera.pixelHeight);
    private Vector2Int _lastUpscalingResolution;

    // Rendering Resolution
    private Vector2Int _optimalRenderingResolution;
    protected Vector2Int MinimumDynamicRenderingResolution;
    protected Vector2Int MaximumDynamicRenderingResolution;
    private Vector2Int RenderingResolution => UseDynamicResolution ? new Vector2Int((int)(UpscalingResolution.x * ScalableBufferManager.widthScaleFactor), (int)(UpscalingResolution.y * ScalableBufferManager.heightScaleFactor)) : _optimalRenderingResolution;
    private Vector2Int _lastRenderingResolution;

    // Resolution scale
    // Resolution scale attributes are only used when Dynamic Resolution Scaling is enabled.
    /*@todo Use a standardized form for the UpscalingFactor (<=1). */
    public Vector2 MinScaleFactor => (Vector2)MinimumDynamicRenderingResolution / UpscalingResolution;
    public Vector2 MaxScaleFactor => (Vector2)MaximumDynamicRenderingResolution / UpscalingResolution;

    // Automatic dynamic resolution scaling.
    private static RefreshRate TargetFrameRate => new DisplayInfo().refreshRate;

    // HDR state
    protected bool HDRActive => Camera.allowHDR;
    private bool _lastHDRActive;

    // RenderTextures
    private RenderTexture _outputTarget;
    private RenderTexture _inColorTarget;
    private RenderTexture _motionVectorTarget;

    // CommandBuffers
    private CommandBuffer _setRenderingResolution;
    private CommandBuffer _upscale;
    private CommandBuffer _setDepthSize;

    // Internal Render Pipeline abstraction
    private RenderPipeline _renderPipeline;

    // API
    protected Plugin.Mode ActiveMode = Plugin.Mode.DLSS;
    private Plugin.Mode _lastMode;
    protected Plugin.Quality ActiveQuality = Plugin.Quality.DynamicAuto;
    private Plugin.Quality _lastQuality;

    private bool DHDR => _lastHDRActive != HDRActive;
    protected bool DUpscalingResolution => _lastUpscalingResolution != UpscalingResolution;
    private bool DUpscaler => _lastMode != ActiveMode;
    private bool DQuality => _lastQuality != ActiveQuality;
    private bool DDynamicResolution => _lastUseDynamicResolution != UseDynamicResolution;
    private bool DRenderingResolution => _lastRenderingResolution != RenderingResolution;

    private const GraphicsFormat MotionFormat = GraphicsFormat.R16G16_SFloat;
    private GraphicsFormat ColorFormat => SystemInfo.GetGraphicsFormat(HDRActive ? DefaultFormat.HDR : DefaultFormat.LDR);
    private static GraphicsFormat DepthFormat => SystemInfo.GetGraphicsFormat(DefaultFormat.DepthStencil);

    private void ComputeUpscalingFactorFromGPUFrameTimes()
    {
        /*@todo Implement a PID to control the ActiveUpscalingFactor? Pro: Smoother resolution transitions - harder to notice change in resolution. Con: Slower to react - easier to notice change in framerate. */
        var multiplier = Math.Clamp(0.01F, 100F, (float)TargetFrameRate.value / (float)new FrameTiming().gpuFrameTime);
        ScalableBufferManager.ResizeBuffers(ScalableBufferManager.widthScaleFactor * multiplier, ScalableBufferManager.heightScaleFactor * multiplier);
    }

    private bool ManageTargets()
    {
        if (DUpscaler) Plugin.SetUpscaler(ActiveMode);

        if (ActiveQuality == Plugin.Quality.DynamicAuto | ActiveQuality == Plugin.Quality.DynamicManual)
        {
            if (ActiveQuality == Plugin.Quality.DynamicAuto) ComputeUpscalingFactorFromGPUFrameTimes();
        }
        else
        {
            if (DUpscalingResolution | DHDR | DQuality | DUpscaler && ActiveMode != Plugin.Mode.None)
            {
                Plugin.SetFramebufferSettings((uint)UpscalingResolution.x, (uint)UpscalingResolution.y, ActiveQuality, HDRActive);
                var size = Plugin.GetRecommendedInputResolution();
                _optimalRenderingResolution = new Vector2Int((int)(size >> 32), (int)(size & 0xFFFFFFFF));
            }
        }

        Plugin.SetCurrentInputResolution((uint)RenderingResolution.x, (uint)RenderingResolution.y);

        Jitter.Generate((Vector2)UpscalingResolution / RenderingResolution);

        if (DRenderingResolution | DUpscalingResolution)
            Debug.Log(RenderingResolution.x + "x" + RenderingResolution.y + " -> " + UpscalingResolution.x + "x" + UpscalingResolution.y);

        var imagesChanged = false;

        if (DHDR | DDynamicResolution | DUpscalingResolution | (!UseDynamicResolution && DRenderingResolution) | DUpscaler | DQuality)
            imagesChanged |= ManageInColorTarget();

        if (DHDR | DUpscalingResolution | DUpscaler)
            imagesChanged |= ManageOutputTarget();

        if (DUpscalingResolution | DUpscaler)
            imagesChanged |= ManageMotionVectorTarget();

        // Do not look at this. It is very pretty, I assure you.
        _lastHDRActive = HDRActive;
        _lastUpscalingResolution = UpscalingResolution;
        _lastRenderingResolution = RenderingResolution;
        _lastUseDynamicResolution = UseDynamicResolution;
        _lastMode = ActiveMode;
        _lastQuality = ActiveQuality;

        if (imagesChanged)
            Plugin.Prepare();

        return imagesChanged;
    }

    private bool ManageOutputTarget() {
        var dTarget = false;
        if (_outputTarget != null && _outputTarget.IsCreated())
        {
            _outputTarget.Release();
            _outputTarget = null;
            dTarget = true;
        }

        /*@todo If the camera's target is null, use Display.displays[Camera.targetDisplay].colorBuffer */

        if (ActiveMode == Plugin.Mode.None) return dTarget;

        _outputTarget =
            new RenderTexture(UpscalingResolution.x, UpscalingResolution.y, 0, ColorFormat)
            {
                enableRandomWrite = true
            };
        _outputTarget.Create();

        Plugin.SetOutputColor(_outputTarget.GetNativeTexturePtr(), _outputTarget.graphicsFormat);
        return true;
    }

    private bool ManageMotionVectorTarget() {
        var dTarget = false;
        if (_motionVectorTarget != null && _motionVectorTarget.IsCreated())
        {
            _motionVectorTarget.Release();
            _motionVectorTarget = null;
            dTarget = true;
        }

        if (ActiveMode == Plugin.Mode.None) return dTarget;

        _motionVectorTarget = new RenderTexture(UpscalingResolution.x, UpscalingResolution.y, 0, MotionFormat);
        _motionVectorTarget.Create();

        Plugin.SetMotionVectors(_motionVectorTarget.GetNativeTexturePtr(), _motionVectorTarget.graphicsFormat);
        return true;
    }

    private bool ManageInColorTarget()
    {
        var dTarget = false;

        if (_inColorTarget != null && _inColorTarget.IsCreated())
        {
            _inColorTarget.Release();
            _inColorTarget = null;
            dTarget = true;
        }

        if (ActiveMode == Plugin.Mode.None) return dTarget;

        var scale = UseDynamicResolution ? UpscalingResolution : RenderingResolution;
        _inColorTarget =
            new RenderTexture(scale.x, scale.y, ColorFormat, DepthFormat)
            {
                filterMode = FilterMode.Point
            };
        _inColorTarget.Create();

        Plugin.SetDepthBuffer(_inColorTarget.GetNativeDepthBufferPtr(), _inColorTarget.depthStencilFormat);
        Plugin.SetInputColor(_inColorTarget.GetNativeTexturePtr(), _inColorTarget.graphicsFormat);
        return true;
    }

    protected void BeforeCameraCulling()
    {
        if (ManageTargets()) Plugin.ResetHistory();
        _renderPipeline.PrepareRendering(_setRenderingResolution, _setDepthSize, _upscale, RenderingResolution,
            UpscalingResolution, _motionVectorTarget, _inColorTarget, _outputTarget,
            ActiveMode);
        if (ActiveMode != Plugin.Mode.None) Jitter.Apply(Camera, RenderingResolution);
    }

    protected void OnEnable()
    {
        // Set up camera
        Camera = GetComponent<Camera>();
        Camera.depthTextureMode |= DepthTextureMode.MotionVectors | DepthTextureMode.Depth;

        // Set up the TexMan
        TexMan.Setup();

        _renderPipeline = new Builtin();

        // Set up command buffers
        _setRenderingResolution = new CommandBuffer();
        _setRenderingResolution.name = "Set Render Resolution";
        _setDepthSize = new CommandBuffer();
        _setDepthSize.name = "Set Depth Size";
        _upscale = new CommandBuffer();
        _upscale.name = "Upscale";

        Camera.AddCommandBuffer(CameraEvent.BeforeGBuffer, _setRenderingResolution);
        Camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque, _setRenderingResolution);
        Camera.AddCommandBuffer(CameraEvent.BeforeSkybox, _setRenderingResolution);

        // Camera.AddCommandBuffer(CameraEvent.BeforeLighting, _setDepthSize);

        Camera.AddCommandBuffer(CameraEvent.BeforeImageEffectsOpaque, _upscale);

        // Initialize the plugin
        Plugin.InitializePlugin();
    }

    protected void OnDisable()
    {
        Plugin.ShutdownPlugin();

        if (_setRenderingResolution != null)
        {
            Camera.RemoveCommandBuffer(CameraEvent.BeforeGBuffer, _setRenderingResolution);
            Camera.RemoveCommandBuffer(CameraEvent.BeforeForwardOpaque, _setRenderingResolution);
            Camera.RemoveCommandBuffer(CameraEvent.BeforeSkybox, _setRenderingResolution);
            _setRenderingResolution.Release();
        }

        // if (_setDepthSize != null)
        // {
        //     Camera.RemoveCommandBuffer(CameraEvent.BeforeLighting, _setDepthSize);
        // }

        // ReSharper disable once InvertIf
        if (_upscale != null)
        {
            Camera.RemoveCommandBuffer(CameraEvent.BeforeImageEffectsOpaque, _upscale);
            _upscale.Release();
        }
    }
}