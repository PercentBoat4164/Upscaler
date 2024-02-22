#if UPSCALER_USE_URP
using Conifer.Upscaler.Scripts.impl;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

namespace Conifer.Upscaler.Scripts
{
    public class UpscalerRendererFeature : ScriptableRendererFeature
    {
        private class UpscalerRenderPass : ScriptableRenderPass
        {
            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                if (!Application.isPlaying || !_registered) return;
                var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
                if (upscaler == null || upscaler.upscalerMode == Upscaler.UpscalerMode.None) return;

                // Execute the upscale
                var commandBuffer = CommandBufferPool.Get("Upscale");
                commandBuffer.SetRenderTarget(upscaler.UpscalingData.InColorTarget.colorBuffer, upscaler.UpscalingData.InColorTarget.depthBuffer);
                BlitLib.CopyCameraDepth(commandBuffer);
                BlitLib.BlitToMotionTexture(commandBuffer, upscaler.UpscalingData.MotionVectorTarget);
                upscaler.Plugin.Upscale(commandBuffer);
                commandBuffer.Blit(upscaler.UpscalingData.OutputTarget, upscaler.UpscalingData.CameraTarget);
                context.ExecuteCommandBuffer(commandBuffer);

                upscaler.Plugin.Camera.targetTexture = upscaler.UpscalingData.CameraTarget;
                RenderTexture.active = upscaler.UpscalingData.CameraTarget;
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
            _upscalerRenderPass.ConfigureInput(ScriptableRenderPassInput.Motion);
            _upscalerRenderPass.renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing + 1;
            renderer.EnqueuePass(_upscalerRenderPass);
        }

        private static void PreUpscale(ScriptableRenderContext context, Camera camera)
        {
            if (!Application.isPlaying) return;
            var upscaler = camera.GetComponent<Upscaler>();
            if (upscaler == null)
            {
                Debug.LogError(
                    "All cameras using the Upscaler Renderer Feature must have the Upscaler script attached to them as well.", camera);
                return;
            }
            upscaler.OnPreCull();

            if (upscaler.upscalerMode == Upscaler.UpscalerMode.None) return;
            upscaler.UpscalingData.CameraTarget = camera.targetTexture;
            camera.targetTexture = upscaler.UpscalingData.InColorTarget;
            RenderTexture.active = upscaler.UpscalingData.InColorTarget;
        }

        protected override void Dispose(bool dispose)
        {
            if (dispose)
            {
                if (_registered) RenderPipelineManager.beginCameraRendering -= PreUpscale;
                _registered = false;
            }
        }
    }
}
#endif