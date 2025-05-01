using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

namespace Conifer.Upscaler.URP
{
    public partial class UpscalerRendererFeature
    {
        private class SetupGenerateRenderPass : ScriptableRenderPass
        {
            public SetupGenerateRenderPass() => renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing;

#if UNITY_6000_0_OR_NEWER
            [Obsolete("This rendering path is for compatibility mode only (when Render Graph is disabled). Use Render Graph API instead.", false)]
#endif
            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var descriptor = renderingData.cameraData.cameraTargetDescriptor;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                descriptor.graphicsFormat = GraphicsFormat.R16G16_SFloat;
                var cb = CommandBufferPool.Get("Generate");
                cb.GetTemporaryRT(GenerateRenderPass.TempMotion, descriptor);
                cb.Blit(MotionID, GenerateRenderPass.TempMotion);
                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);
            }
        }
    }
}