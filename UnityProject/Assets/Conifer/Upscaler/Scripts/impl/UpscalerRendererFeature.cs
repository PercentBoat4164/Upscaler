#if UPSCALER_USE_URP
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

namespace Conifer.Upscaler.Scripts
{
    public class UpscalerRendererFeature : ScriptableRendererFeature
    {
        private static readonly int MotionID = Shader.PropertyToID("_MotionVectorTexture");
        private static readonly int OutputID = Shader.PropertyToID("Upscaler_OutputTexture");
        
        private class UpscalerRenderPass : ScriptableRenderPass
        {
            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                if (!Application.isPlaying || !_registered) return;
                var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
                if (upscaler is null || upscaler.settings.FeatureSettings.upscaler == Settings.Upscaler.None) return; 

                var commandBuffer = CommandBufferPool.Get("Upscale");
                commandBuffer.GetTemporaryRT(OutputID, upscaler.OutputResolution.x, upscaler.OutputResolution.y, 0,
                    FilterMode.Point, upscaler.Plugin.ColorFormat(), 1, true, RenderTextureMemoryless.None, false);
                var motion = Shader.GetGlobalTexture(MotionID);
                var output = Shader.GetGlobalTexture(OutputID);
                if (motion is not null && output is not null)
                    upscaler.Plugin.Upscale(commandBuffer, renderingData.cameraData.targetTexture, motion, output);
                context.ExecuteCommandBuffer(commandBuffer);

                upscaler.Plugin.Camera.targetTexture = upscaler.UpscalingData.CameraTarget;
                
                commandBuffer.Clear();
                commandBuffer.Blit(output, upscaler.UpscalingData.CameraTarget);
                commandBuffer.ReleaseTemporaryRT(OutputID);
                context.ExecuteCommandBuffer(commandBuffer);
            }
        }

        // Render passes
        private UpscalerRenderPass _upscalerRenderPass;
        private static bool _registered;

        public override void Create()
        {
            _upscalerRenderPass = new UpscalerRenderPass();
            name = "Upscaler";

            if (!_registered) RenderPipelineManager.beginCameraRendering += PreUpscale;
            _registered = true;
        }

        public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
        {
            _upscalerRenderPass.ConfigureInput(ScriptableRenderPassInput.Motion | ScriptableRenderPassInput.Depth);
            _upscalerRenderPass.renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing + 25;
            renderer.EnqueuePass(_upscalerRenderPass);
        }

        private static void PreUpscale(ScriptableRenderContext context, Camera camera)
        {
            if (!Application.isPlaying || !camera.enabled) return;
            var upscaler = camera.GetComponent<Upscaler>();
            if (upscaler is null)
            {
                Debug.LogError(
                    "All cameras using the Upscaler Renderer Feature must have the Upscaler script attached to them as well.", camera);
                return;
            }
            upscaler.OnPreCull();

            if (upscaler.settings.FeatureSettings.upscaler == Settings.Upscaler.None) return;
            upscaler.UpscalingData.CameraTarget = camera.targetTexture;
            camera.targetTexture = upscaler.UpscalingData.ColorTarget;
        }

        protected override void Dispose(bool dispose)
        {
            if (!dispose) return;
            if (_registered) RenderPipelineManager.beginCameraRendering -= PreUpscale;
            _registered = false;
        }
    }
}
#endif