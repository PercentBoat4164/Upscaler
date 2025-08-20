#if !UNITY_6000_0_OR_NEWER && UPSCALER_USE_URP
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

namespace Upscaler.Runtime.URP
{
    public partial class UpscalerRendererFeature
    {
        private class GenerateRenderPass : ScriptableRenderPass
        {
            public GenerateRenderPass() => renderPassEvent = (RenderPassEvent)int.MaxValue;

#if UNITY_6000_0_OR_NEWER
            [Obsolete("This rendering path is for compatibility mode only (when Render Graph is disabled). Use Render Graph API instead.", false)]
#endif
            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
                var cb = CommandBufferPool.Get("Generate");
                upscaler.FgBackend.Generate(upscaler, cb, Shader.GetGlobalTexture(DepthID), Shader.GetGlobalTexture(MotionID));
                cb.SetRenderTarget(k_CameraTarget);
                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);
            }
        }
    }
}
#endif