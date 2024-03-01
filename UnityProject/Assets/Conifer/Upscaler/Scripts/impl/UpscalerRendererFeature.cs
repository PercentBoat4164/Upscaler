#if UPSCALER_USE_URP
using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

namespace Conifer.Upscaler.Scripts
{
    public class UpscalerRendererFeature : ScriptableRendererFeature
    {
        private static readonly int MotionID = Shader.PropertyToID("_MotionVectorTexture");
        private static readonly int SourceColorID = Shader.PropertyToID("_CameraColorTexture");
        private static readonly int SourceDepthID = Shader.PropertyToID("_CameraDepthTexture");
        private static Upscaler _upscaler;
        
        private class UpscalePhaseOne : ScriptableRenderPass
        {
            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var sourceDepth = Shader.GetGlobalTexture(SourceDepthID);
                if (sourceDepth is null) return;
                var upscale = CommandBufferPool.Get("Upscale");
                var outputColor = _upscaler.Camera.targetTexture ? _upscaler.Camera.targetTexture : _upscaler.UpscalingData.OutputColorTarget;
                upscale.Blit(sourceDepth, _upscaler.UpscalingData.SourceDepthTarget);
                _upscaler.Plugin.Upscale(upscale, Shader.GetGlobalTexture(SourceColorID), _upscaler.UpscalingData.SourceDepthTarget, Shader.GetGlobalTexture(MotionID), outputColor);
                if (_upscaler.Camera.targetTexture is null)
                {
                    upscale.Blit(outputColor.colorBuffer, _upscaler.Camera.targetTexture);
                } else {
                    upscale.Blit(_upscaler.UpscalingData.SourceDepthTarget, _upscaler.Camera.targetTexture.depthBuffer);
                }

                context.ExecuteCommandBuffer(upscale);
                upscale.Release();
            }
        }

        // Render passes
        private readonly UpscalePhaseOne _upscalePhaseOne = new();
        private static bool _registered;

        public override void Create()
        {
            name = "Upscaler";
            if (!_registered) RenderPipelineManager.beginContextRendering += PreUpscale;
            _registered = true;
        }

        public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
        {
            if (!Application.isPlaying || !_registered || _upscaler is null || _upscaler.settings.upscaler == Settings.Upscaler.None) return;
            _upscaler.Camera.rect = new Rect(0, 0, _upscaler.OutputResolution.x, _upscaler.OutputResolution.y);
            _upscalePhaseOne.ConfigureInput(ScriptableRenderPassInput.Motion);
            _upscalePhaseOne.ConfigureTarget(_upscaler.UpscalingData.OutputColorTarget);
            _upscalePhaseOne.renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing + 25;
            renderer.EnqueuePass(_upscalePhaseOne);
        }

        private static void PreUpscale(ScriptableRenderContext context, List<Camera> cameras)
        {
            if (!Application.isPlaying) return;
            foreach (var camera in cameras.Where(camera => camera.enabled))
            {
                _upscaler = camera.GetComponent<Upscaler>();
                if (_upscaler is null) Debug.LogError("All cameras using the Upscaler Renderer Feature must have the Upscaler script attached to them as well.", camera);
                else _upscaler.OnPreCull();
            }
        }

        protected override void Dispose(bool dispose)
        {
            if (!dispose) return;
            if (_registered) RenderPipelineManager.beginContextRendering -= PreUpscale;
            _registered = false;
        }
    }
}
#endif