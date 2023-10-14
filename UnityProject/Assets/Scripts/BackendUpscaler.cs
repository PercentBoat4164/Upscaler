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
    private Vector2Int RenderingResolution => UseDynamicResolution ? new Vector2Int((int)(UpscalingResolution.x * ScalableBufferManager.widthScaleFactor), (int)(UpscalingResolution.y * ScalableBufferManager.heightScaleFactor)) : _optimalRenderingResolution;
    private Vector2Int _lastRenderingResolution;

    /*@todo Use a standardized form for the UpscalingFactor (>=1). */
    protected Vector2 MinScale;
    protected Vector2 MaxScale;
    private Vector2 _activeUpscalingFactor = new(0.9F, 0.9F);
    protected Vector2 ActiveUpscalingFactor
    {
        set => _activeUpscalingFactor = new Vector2(Math.Clamp(value.x, MinScale.x, MaxScale.x), Math.Clamp(value.y, MinScale.y, MaxScale.y));
        get => _activeUpscalingFactor;
    }

    // HDR state
    private bool HDRActive => Camera.allowHDR;
    private bool _lastHDRActive;

    // RenderTextures
    private RenderTexture _outputTarget;
    private RenderTexture _inColorTarget;
    private RenderTexture _motionVectorTarget;

    // CommandBuffers
    private CommandBuffer _setRenderingResolution;
    private CommandBuffer _upscale;

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

    private bool ManageTargets()
    {
        // Resize the buffers
        if ((ActiveQuality == Plugin.Quality.DynamicAuto) | Camera.allowDynamicResolution)
            /*@todo Enable automatic scaling based on GPU frame times if quality == DynamicAuto.*/
            ScalableBufferManager.ResizeBuffers(ActiveUpscalingFactor.x, ActiveUpscalingFactor.y);

        // Initialize any new upscaler
        if (DUpscaler)
            Plugin.SetUpscaler(ActiveMode);

        if (DUpscalingResolution | DHDR | DQuality | DUpscaler && ActiveMode != Plugin.Mode.None)
        {
            Plugin.SetFramebufferSettings((uint)UpscalingResolution.x, (uint)UpscalingResolution.y, ActiveQuality, HDRActive);
            var size = Plugin.GetRecommendedInputResolution();
            _optimalRenderingResolution = new Vector2Int((int)(size >> 32), (int)(size & 0xFFFFFFFF));
        }

        if (DRenderingResolution | DUpscalingResolution)
            Debug.Log(RenderingResolution.x + "x" + RenderingResolution.y + " -> " + UpscalingResolution.x + "x" + UpscalingResolution.y);

        Plugin.SetCurrentInputResolution((uint)RenderingResolution.x, (uint)RenderingResolution.y);
        Jitter.Generate(RenderingResolution, ActiveUpscalingFactor);

        var imagesChanged = false;

        if (DHDR | DDynamicResolution | DUpscalingResolution | (!UseDynamicResolution && DRenderingResolution) | DUpscaler)
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

        if (!imagesChanged)
            return false;

        Plugin.Prepare();

        return true;
    }

    bool ManageOutputTarget() {
        var dTarget = false;
        if (_outputTarget != null && _outputTarget.IsCreated())
        {
            _outputTarget.Release();
            _outputTarget = null;
            dTarget = true;
        }

        if (ActiveMode != Plugin.Mode.None)
        {
            _outputTarget =
                new RenderTexture(UpscalingResolution.x, UpscalingResolution.y, 0, ColorFormat)
                {
                    enableRandomWrite = true
                };
            _outputTarget.Create();

            Plugin.SetOutputColor(_outputTarget.GetNativeTexturePtr(), _outputTarget.graphicsFormat);
            dTarget = true;
        }

        return dTarget;
    }

    bool ManageMotionVectorTarget() {
        var dTarget = false;
        if (_motionVectorTarget != null && _motionVectorTarget.IsCreated())
        {
            _motionVectorTarget.Release();
            _motionVectorTarget = null;
            dTarget = true;
        }

        if (ActiveMode != Plugin.Mode.None)
        {
            _motionVectorTarget = new RenderTexture(UpscalingResolution.x, UpscalingResolution.y, 0, MotionFormat);
            _motionVectorTarget.Create();

            Plugin.SetMotionVectors(_motionVectorTarget.GetNativeTexturePtr(), _motionVectorTarget.graphicsFormat);
            dTarget = true;
        }
        return dTarget;
    }

    bool ManageInColorTarget()
    {
        var dTarget = false;

        if (_inColorTarget != null && _inColorTarget.IsCreated())
        {
            _inColorTarget.Release();
            _inColorTarget = null;
            dTarget = true;
        }

        if (ActiveMode != Plugin.Mode.None)
        {
            var scale = UseDynamicResolution ? UpscalingResolution : RenderingResolution;
            _inColorTarget =
                new RenderTexture(scale.x, scale.y, ColorFormat, DepthFormat)
                {
                    filterMode = FilterMode.Point
                };
            _inColorTarget.Create();

            Plugin.SetDepthBuffer(_inColorTarget.GetNativeDepthBufferPtr(), _inColorTarget.depthStencilFormat);
            Plugin.SetInputColor(_inColorTarget.GetNativeTexturePtr(), _inColorTarget.graphicsFormat);
            dTarget = true;
        }

        return dTarget;
    }

    protected void BeforeCameraCulling()
    {
        if (ManageTargets()) Plugin.ResetHistory();
        _renderPipeline.PrepareRendering(_setRenderingResolution, _upscale, RenderingResolution,
            UpscalingResolution, _motionVectorTarget, _inColorTarget, _outputTarget,
            ActiveMode);
        if (ActiveMode != Plugin.Mode.None) Jitter.Apply(Camera);
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
        _upscale = new CommandBuffer();
        _upscale.name = "Upscale";

        Camera.AddCommandBuffer(CameraEvent.BeforeGBuffer, _setRenderingResolution);
        Camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque, _setRenderingResolution);
        Camera.AddCommandBuffer(CameraEvent.BeforeSkybox, _setRenderingResolution);

        Camera.AddCommandBuffer(CameraEvent.BeforeImageEffectsOpaque, _upscale);

        Plugin.InitializePlugin();
    }

    private void Update()
    {
        if (Input.GetKeyDown(KeyCode.UpArrow))
            ActiveUpscalingFactor = new Vector2(ActiveUpscalingFactor.x + .1F, ActiveUpscalingFactor.y + .1F);

        if (Input.GetKeyDown(KeyCode.DownArrow))
            ActiveUpscalingFactor = new Vector2(ActiveUpscalingFactor.x - .1F, ActiveUpscalingFactor.y - .1F);
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

        if (_upscale != null)
        {
            Camera.RemoveCommandBuffer(CameraEvent.BeforeImageEffectsOpaque, _upscale);
            _upscale.Release();
        }
    }
}