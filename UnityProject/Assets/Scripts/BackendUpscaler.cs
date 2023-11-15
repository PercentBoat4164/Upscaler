using System;
using UnityEngine;
using UnityEngine.Rendering;

public class BackendUpscaler : MonoBehaviour
{
    // Camera
    protected Camera Camera;

    // Dynamic Resolution state
    private bool UseDynamicResolution =>
        (ActiveQuality == Plugin.Quality.DynamicAuto) | (ActiveQuality == Plugin.Quality.DynamicManual);
    private bool _lastUseDynamicResolution;
    private double _lastFrameStartTime;

    // Upscaling Resolution
    protected Vector2Int UpscalingResolution => Camera.targetTexture != null ? new Vector2Int(Camera.targetTexture.width, Camera.targetTexture.height) : new Vector2Int(Camera.pixelWidth, Camera.pixelHeight);
    private Vector2Int _lastUpscalingResolution;

    // Rendering Resolution
    private Vector2Int _optimalRenderingResolution;
    protected Vector2Int MinimumDynamicRenderingResolution;
    protected Vector2Int MaximumDynamicRenderingResolution;
    private Vector2Int RenderingResolution => UseDynamicResolution ? new Vector2Int((int)Math.Ceiling(UpscalingResolution.x * ScalableBufferManager.widthScaleFactor), (int)Math.Ceiling(UpscalingResolution.y * ScalableBufferManager.heightScaleFactor)) : _optimalRenderingResolution;
    private Vector2Int _lastRenderingResolution;

    // Resolution scale
    // Resolution scale attributes are only used when Dynamic Resolution Scaling is enabled.
    public Vector2 MinScaleFactor => (Vector2)MinimumDynamicRenderingResolution / UpscalingResolution;
    public Vector2 MaxScaleFactor => (Vector2)MaximumDynamicRenderingResolution / UpscalingResolution;

    // Automatic dynamic resolution scaling.
    // private static RefreshRate TargetFrameRate => new DisplayInfo().refreshRate;
    private static float TargetFrameRate => 240f;

    // HDR state
    protected bool ActiveHDR => Camera.allowHDR;
    private bool _lastHDRActive;

    // Internal Render Pipeline abstraction
    private RenderPipeline _renderPipeline;

    // API
    protected Plugin.Mode ActiveMode = Plugin.Mode.DLSS;
    private Plugin.Mode _lastMode;
    protected Plugin.Quality ActiveQuality = Plugin.Quality.DynamicAuto;
    private Plugin.Quality _lastQuality;

    private bool DHDR => _lastHDRActive != ActiveHDR;
    protected bool DUpscalingResolution => _lastUpscalingResolution != UpscalingResolution;
    private bool DUpscaler => _lastMode != ActiveMode;
    private bool DQuality => _lastQuality != ActiveQuality;
    private bool DDynamicResolution => _lastUseDynamicResolution != UseDynamicResolution;
    private bool DRenderingResolution => _lastRenderingResolution != RenderingResolution;

    private void ManageTargets()
    {
        if (DUpscaler) Plugin.SetUpscaler(ActiveMode);
        if (DUpscalingResolution | DHDR | DQuality | DUpscaler && ActiveMode != Plugin.Mode.None)
        {
            Plugin.SetFramebufferSettings((uint)UpscalingResolution.x, (uint)UpscalingResolution.y, ActiveQuality, ActiveHDR);
            var size = Plugin.GetRecommendedInputResolution();
            _optimalRenderingResolution = new Vector2Int((int)(size >> 32), (int)(size & 0xFFFFFFFF));
        }

        if (ActiveQuality == Plugin.Quality.DynamicAuto)
        {
            /*@todo Fix the timings here. */
            var multiplier = (float)Math.Clamp((Time.realtimeSinceStartupAsDouble - _lastFrameStartTime) / (1 / 60.0), 0.01, 100.0);
            ScalableBufferManager.ResizeBuffers(
                Math.Clamp(ScalableBufferManager.widthScaleFactor * multiplier, MinScaleFactor.x, MaxScaleFactor.x),
                Math.Clamp(ScalableBufferManager.heightScaleFactor * multiplier, MinScaleFactor.y, MaxScaleFactor.y)
            );
            _lastFrameStartTime = Time.realtimeSinceStartupAsDouble;
        }

        if (DRenderingResolution)
            Plugin.SetCurrentInputResolution((uint)RenderingResolution.x, (uint)RenderingResolution.y);
        if (DRenderingResolution | DUpscalingResolution)
            Jitter.Generate((Vector2)UpscalingResolution / RenderingResolution);

        if (DRenderingResolution | DUpscalingResolution)
            Debug.Log(RenderingResolution.x + "x" + RenderingResolution.y + " -> " + UpscalingResolution.x + "x" + UpscalingResolution.y);

        var imagesChanged = false;

        if (DHDR | DDynamicResolution | DUpscalingResolution | (!UseDynamicResolution && DRenderingResolution) | DUpscaler | DQuality)
            imagesChanged |= _renderPipeline.ManageInColorTarget(ActiveMode, UseDynamicResolution ? UpscalingResolution : RenderingResolution);

        if (DHDR | DUpscalingResolution | DUpscaler | DQuality)
            imagesChanged |= _renderPipeline.ManageOutputTarget(ActiveMode, UpscalingResolution);

        if (imagesChanged)
            _renderPipeline.UpdatePostUpscaleCommandBuffer();

        if (DDynamicResolution | DUpscalingResolution | (!UseDynamicResolution && DRenderingResolution) | DUpscaler | DQuality)
            imagesChanged |= _renderPipeline.ManageMotionVectorTarget(ActiveMode, UseDynamicResolution ? UpscalingResolution : RenderingResolution);

        // Do not look at this. It is very pretty, I assure you.
        _lastHDRActive = ActiveHDR;
        _lastUpscalingResolution = UpscalingResolution;
        _lastRenderingResolution = RenderingResolution;
        _lastUseDynamicResolution = UseDynamicResolution;
        _lastMode = ActiveMode;
        _lastQuality = ActiveQuality;

        if (imagesChanged)
            Plugin.Prepare();
    }

    protected void OnEnable()
    {
        // Set up camera
        Camera = GetComponent<Camera>();
        Camera.depthTextureMode |= DepthTextureMode.MotionVectors | DepthTextureMode.Depth;

        // Set up the TexMan
        TexMan.Setup();

        // Initialize the plugin.
        /*@todo Handle the case that different quality modes use different render pipelines. RenderPipelineManager.activeRenderPipelineTypeChanged? */
        if (GraphicsSettings.currentRenderPipeline == null)
            _renderPipeline = new Builtin(Camera);
        #if UPSCALER_USE_URP
        else
            _renderPipeline = new Universal(Camera, ((Upscaler)this).OnPreCull);
        #endif

        // Initialize the plugin
        Plugin.Initialize();
        Plugin.SetUpscaler(ActiveMode);
        if (ActiveMode == Plugin.Mode.None) return;
        Plugin.SetFramebufferSettings((uint)UpscalingResolution.x, (uint)UpscalingResolution.y, ActiveQuality, ActiveHDR);
        Plugin.SetCurrentInputResolution((uint)RenderingResolution.x, (uint)RenderingResolution.y);
    }

    protected virtual void OnPreCull()
    {
        if (!Application.isPlaying) return;
        ManageTargets();
        if (ActiveMode != Plugin.Mode.None) Jitter.Apply(Camera, RenderingResolution);
    }

    protected void OnPreRender()
    {
        if (!Application.isPlaying) return;
        if (ActiveMode != Plugin.Mode.None) ((Builtin)_renderPipeline).PrepareRendering();
    }

    protected void OnPostRender()
    {
        if (!Application.isPlaying) return;
        if (ActiveMode != Plugin.Mode.None) ((Builtin)_renderPipeline).Upscale();
    }

    protected void OnDisable()
    {
        // Shutdown internal plugin
        Plugin.ShutdownPlugin();

        // Shutdown the active render pipeline
        _renderPipeline.Shutdown();
    }
}