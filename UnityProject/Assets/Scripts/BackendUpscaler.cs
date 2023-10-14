using System;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;

public class BackendUpscaler : MonoBehaviour
{
    // Camera
    private Camera _camera;

    // Dynamic Resolution state
    private bool UseDynamicResolution =>
        (ActiveQuality == Plugin.Quality.DynamicAuto) | (ActiveQuality == Plugin.Quality.DynamicManual);

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
    private CommandBuffer _upscale;

    // Internal Render Pipeline abstraction
    private RenderPipeline _renderPipeline;

    // API
    protected Plugin.Upscaler ActiveUpscaler = Plugin.Upscaler.DLSS;
    private Plugin.Upscaler _lastUpscaler;
    protected Plugin.Quality ActiveQuality = Plugin.Quality.DynamicAuto;
    private Plugin.Quality _lastQuality;

    private bool DHDR => _lastHDRActive != HDRActive;
    private bool DUpscalingResolution => _lastUpscalingResolution != UpscalingResolution;
    private bool DUpscaler => _lastUpscaler != ActiveUpscaler;
    private bool DQuality => _lastQuality != ActiveQuality;
    private bool DDynamicResolution => _lastUseDynamicResolution != UseDynamicResolution;
    private bool DRenderingResolution => _lastRenderingResolution != RenderingResolution;

    private const GraphicsFormat motionFormat = GraphicsFormat.R16G16_SFloat;
    private GraphicsFormat colorFormat => SystemInfo.GetGraphicsFormat(HDRActive ? DefaultFormat.HDR : DefaultFormat.LDR);
    private GraphicsFormat depthFormat => SystemInfo.GetGraphicsFormat(DefaultFormat.DepthStencil);

    private bool ManageTargets()
    {
        // Resize the buffers
        if ((ActiveQuality == Plugin.Quality.DynamicAuto) | _camera.allowDynamicResolution)
            /*@todo Enable automatic scaling based on GPU frame times if quality == DynamicAuto.*/
            ScalableBufferManager.ResizeBuffers(UpscalingFactorWidth, UpscalingFactorHeight);

        // Initialize any new upscaler
        if (DUpscaler)
            Plugin.Set(ActiveUpscaler);

        if (DUpscalingResolution | DHDR | DQuality | DUpscaler && ActiveUpscaler != Plugin.Upscaler.None)
        {
            Plugin.SetFramebufferSettings(UpscalingWidth, UpscalingHeight, ActiveQuality, HDRActive);
            var size = Plugin.GetRecommendedInputResolution();
            _optimalRenderingWidth = (uint)(size >> 32);
            _optimalRenderingHeight = (uint)(size & 0xFFFFFFFF);
        }

        if (DRenderingResolution | DUpscalingResolution)
            Debug.Log(RenderingWidth + "x" + RenderingHeight + " -> " + UpscalingWidth + "x" + UpscalingHeight);

        Plugin.SetCurrentInputResolution(RenderingWidth, RenderingHeight);
        Jitter.Generate(RenderingResolution, UpscalingFactor);

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
        _lastUpscaler = ActiveUpscaler;
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

        if (ActiveUpscaler != Plugin.Upscaler.None)
        {
            _outputTarget =
                new RenderTexture((int)UpscalingWidth, (int)UpscalingHeight, 0, colorFormat)
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

        if (ActiveUpscaler != Plugin.Upscaler.None)
        {
            _motionVectorTarget = new RenderTexture((int)UpscalingWidth, (int)UpscalingHeight, 0, motionFormat);
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

        if (ActiveUpscaler != Plugin.Upscaler.None)
        {
            var scale = UseDynamicResolution ? UpscalingResolution : RenderingResolution;
            _inColorTarget =
                new RenderTexture((int)scale.x, (int)scale.y, colorFormat, depthFormat)
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

    protected void BeforeCameraCulling() => _renderPipeline.BeforeCameraCulling();

    protected void OnEnable()
    {
        // Set up camera
        _camera = GetComponent<Camera>();
        _camera.depthTextureMode |= DepthTextureMode.MotionVectors | DepthTextureMode.Depth;

        // Set up the TexMan
        TexMan.Setup();

        _renderPipeline = new Builtin();

        // Set up command buffers
        _setRenderingResolution = new CommandBuffer();
        _setRenderingResolution.name = "Set Render Resolution";
        _upscale = new CommandBuffer();
        _upscale.name = "Upscale";

        _camera.AddCommandBuffer(CameraEvent.BeforeGBuffer, _setRenderingResolution);
        _camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque, _setRenderingResolution);
        _camera.AddCommandBuffer(CameraEvent.BeforeSkybox, _setRenderingResolution);

        _camera.AddCommandBuffer(CameraEvent.BeforeImageEffectsOpaque, _upscale);

        Plugin.InitializePlugin();
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

    protected void OnDisable()
    {
        Plugin.ShutdownPlugin();

        if (_setRenderingResolution != null)
        {
            _camera.RemoveCommandBuffer(CameraEvent.BeforeGBuffer, _setRenderingResolution);
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
}