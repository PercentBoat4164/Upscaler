using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

public class UpscalerRendererFeature : ScriptableRendererFeature
{
    private class RenderResolutionPass : ScriptableRenderPass
    {
        public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
        {
            var cb = CommandBufferPool.Get("Set Render Resolution");
            cb.SetViewport(new Rect(0, 0, 100, 100));
            context.ExecuteCommandBuffer(cb);
            CommandBufferPool.Release(cb);
        }
    }

    private class UpscalerPass : ScriptableRenderPass
    {
        public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
        {
            var cb = CommandBufferPool.Get("Upscale");
            context.ExecuteCommandBuffer(cb);
            CommandBufferPool.Release(cb);
        }
    }

    private RenderResolutionPass rrp;
    private UpscalerPass up;

    public override void Create()
    {
        rrp = new RenderResolutionPass();
        up = new UpscalerPass()
        {
            renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing
        };
    }

    public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
    {
        rrp.renderPassEvent = RenderPassEvent.BeforeRenderingPrePasses;
        renderer.EnqueuePass(rrp);
        rrp.renderPassEvent = RenderPassEvent.BeforeRenderingPrePasses;
        renderer.EnqueuePass(rrp);
        rrp.renderPassEvent = RenderPassEvent.BeforeRenderingGbuffer;
        renderer.EnqueuePass(rrp);
        rrp.renderPassEvent = RenderPassEvent.BeforeRenderingDeferredLights;
        renderer.EnqueuePass(rrp);
        rrp.renderPassEvent = RenderPassEvent.BeforeRenderingOpaques;
        renderer.EnqueuePass(rrp);
        rrp.renderPassEvent = RenderPassEvent.BeforeRenderingSkybox;
        renderer.EnqueuePass(rrp);
        rrp.renderPassEvent = RenderPassEvent.BeforeRenderingTransparents;
        renderer.EnqueuePass(rrp);

        up.ConfigureInput(ScriptableRenderPassInput.Motion | ScriptableRenderPassInput.Depth | ScriptableRenderPassInput.Color);
        renderer.EnqueuePass(up);
    }
}