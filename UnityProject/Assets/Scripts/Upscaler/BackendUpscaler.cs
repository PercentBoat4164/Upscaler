using System;
using UnityEngine;
using UnityEngine.Rendering;
using Upscaler.impl;
using RenderPipeline = Upscaler.impl.RenderPipeline;

namespace Upscaler
{
    public class BackendUpscaler : MonoBehaviour
    {
        // Camera
        protected Camera Camera;

        // Upscaling Resolution
        private Vector2Int UpscalingResolution => Camera
            ? Camera.targetTexture != null
                ? new Vector2Int(Camera.targetTexture.width, Camera.targetTexture.height)
                : new Vector2Int(Camera.pixelWidth, Camera.pixelHeight)
            : new Vector2Int(Display.displays[Display.activeEditorGameViewTarget].renderingWidth,
                Display.displays[Display.activeEditorGameViewTarget].renderingWidth);

        private Vector2Int _lastUpscalingResolution;

        // Rendering Resolution
        private Vector2Int RenderingResolution { get; set; }
        private Vector2Int _lastRenderingResolution;

        // HDR state
        private bool ActiveHDR => Camera.allowHDR;
        private bool _lastHDRActive;

        // Sharpness
        protected float Sharpness = 0;
        protected float LastSharpness;

        // Internal Render Pipeline abstraction
        private RenderPipeline _renderPipeline;

        // Upscaler preparer command buffer
        private CommandBuffer _upscalerPrepare;

        // API
        protected Plugin.UpscalerMode ActiveUpscalerMode = Plugin.UpscalerMode.DLSS;
        private Plugin.UpscalerMode _lastUpscalerMode;
        protected Plugin.QualityMode ActiveQualityMode = Plugin.QualityMode.Auto;
        private Plugin.QualityMode _lastQualityMode;

        private bool DHDR => _lastHDRActive != ActiveHDR;
        private bool DSharpness => LastSharpness.Equals(Sharpness);
        private bool DUpscalingResolution => _lastUpscalingResolution != UpscalingResolution;
        private bool DUpscaler => _lastUpscalerMode != ActiveUpscalerMode;
        private bool DQuality => _lastQualityMode != ActiveQualityMode;
        private bool DRenderingResolution => _lastRenderingResolution != RenderingResolution;

        private void ManageTargets()
        {
            if (DUpscaler)
                Plugin.SetUpscaler(ActiveUpscalerMode);

            if (DUpscalingResolution | DHDR | DQuality | DUpscaler && ActiveUpscalerMode != Plugin.UpscalerMode.None)
            {
                Plugin.SetFramebufferSettings((uint)UpscalingResolution.x, (uint)UpscalingResolution.y, ActiveQualityMode,
                    ActiveHDR);
                var size = Plugin.GetRecommendedInputResolution();
                RenderingResolution = new Vector2Int((int)(size >> 32), (int)(size & 0xFFFFFFFF));
            }

            if (DRenderingResolution | DUpscalingResolution)
                Jitter.Generate((Vector2)UpscalingResolution / RenderingResolution);

            var upscalerOutdated = false;

            if (DSharpness)
            {
                Plugin.SetSharpnessValue(Sharpness);
                upscalerOutdated = (LastSharpness == 0) ^ (Sharpness == 0);
            }

            if (DHDR | DUpscalingResolution | DRenderingResolution |
                DUpscaler | DQuality)
                upscalerOutdated |=
                    _renderPipeline.ManageInColorTarget(ActiveUpscalerMode, ActiveQualityMode, RenderingResolution);

            if (DHDR | DUpscalingResolution | DUpscaler | DQuality)
                upscalerOutdated |= _renderPipeline.ManageOutputTarget(ActiveUpscalerMode, UpscalingResolution);

            if (upscalerOutdated)
                _renderPipeline.UpdatePostUpscaleCommandBuffer();

            if (DUpscalingResolution | DRenderingResolution | DUpscaler | DQuality)
                upscalerOutdated |=
                    _renderPipeline.ManageMotionVectorTarget(ActiveUpscalerMode, ActiveQualityMode, RenderingResolution);

            if (upscalerOutdated)
                Graphics.ExecuteCommandBuffer(_upscalerPrepare);

            if (DUpscaler && ActiveUpscalerMode == Plugin.UpscalerMode.None) Jitter.Reset(Camera);

            // Do not look at this. It is very pretty, I assure you.
            _lastHDRActive = ActiveHDR;
            _lastUpscalingResolution = UpscalingResolution;
            _lastRenderingResolution = RenderingResolution;
            _lastUpscalerMode = ActiveUpscalerMode;
            _lastQualityMode = ActiveQualityMode;
            LastSharpness = Sharpness;
        }

        private void SetPipeline()
        {
            if (GraphicsSettings.currentRenderPipeline == null)
                _renderPipeline = new Builtin(Camera);
#if UPSCALER_USE_URP
        else
            _renderPipeline = new Universal(Camera, ((Upscaler)this).OnPreCull);
#endif
        }

        protected void Setup(IntPtr handle, Plugin.InternalErrorCallback callback)
        {
            // Set up camera
            Camera = GetComponent<Camera>();
            Camera.depthTextureMode |= DepthTextureMode.MotionVectors | DepthTextureMode.Depth;

            _upscalerPrepare = new CommandBuffer();
            _upscalerPrepare.name = "Prepare upscaler";
            _upscalerPrepare.IssuePluginEvent(Plugin.GetRenderingEventCallback(), (int)Plugin.Event.Prepare);

            // Set up the BlitLib
            BlitLib.Setup();

            // Initialize the plugin.
            RenderPipelineManager.activeRenderPipelineAssetChanged += (_, _) => SetPipeline();
            SetPipeline();

            // Initialize the plugin
            Plugin.Initialize(handle, callback);
            _lastUpscalerMode = ActiveUpscalerMode;
            Plugin.SetUpscaler(ActiveUpscalerMode);
            if (ActiveUpscalerMode == Plugin.UpscalerMode.None) return;
            Plugin.SetFramebufferSettings((uint)UpscalingResolution.x, (uint)UpscalingResolution.y, ActiveQualityMode,
                ActiveHDR);
        }

        protected virtual void OnPreCull()
        {
            if (!Application.isPlaying) return;
            ManageTargets();
            if (ActiveUpscalerMode != Plugin.UpscalerMode.None) Jitter.Apply(Camera, RenderingResolution);
        }

        protected void OnPreRender()
        {
            if (!Application.isPlaying) return;
            if (ActiveUpscalerMode != Plugin.UpscalerMode.None) ((Builtin)_renderPipeline).PrepareRendering();
        }

        protected void OnPostRender()
        {
            if (!Application.isPlaying) return;
            if (ActiveUpscalerMode != Plugin.UpscalerMode.None) ((Builtin)_renderPipeline).Upscale();
        }

        protected void OnDisable()
        {
            // Shutdown internal plugin
            Plugin.ShutdownPlugin();

            // Shutdown the active render pipeline
            _renderPipeline.Shutdown();
        }
    }
}