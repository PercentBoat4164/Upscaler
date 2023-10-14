using UnityEngine;
using UnityEngine.Rendering;

public class Builtin : RenderPipeline
{
    public override void PrepareRendering(
        CommandBuffer setRenderingResolution,
        CommandBuffer upscale,
        Vector2 renderingResolution,
        Vector2 upscalingResolution,
        RenderTexture motionVectors,
        RenderTexture inputTarget,
        RenderTexture outputTarget,
        Plugin.Mode mode
    ) {
        setRenderingResolution.Clear();
        upscale.Clear();

        if (mode == Plugin.Mode.None) return;

        setRenderingResolution.SetViewport(new Rect(0, 0, renderingResolution.x, renderingResolution.y));

        /*@todo Fix shadows when using the deferred rendering path. */
        /*@todo Use the full resolution depth buffers when using the forward rendering path. */
        upscale.CopyTexture(BuiltinRenderTextureType.MotionVectors, 0, 0, 0, 0, (int)upscalingResolution.x,
            (int)upscalingResolution.y, motionVectors, 0, 0, 0, 0);
        upscale.CopyTexture(BuiltinRenderTextureType.CameraTarget, 0, 0, 0, 0, (int)renderingResolution.x,
            (int)renderingResolution.y, inputTarget, 0, 0, 0, 0);
        TexMan.BlitToDepthTexture(upscale, inputTarget, renderingResolution / upscalingResolution);
        upscale.SetViewport(new Rect(0, 0, upscalingResolution.x, upscalingResolution.y));
        upscale.IssuePluginEvent(Plugin.GetRenderingEventCallback(), (int)Plugin.Event.Upscale);
        TexMan.BlitToCameraDepth(upscale, inputTarget);
        upscale.CopyTexture(outputTarget, BuiltinRenderTextureType.CameraTarget);
    }
}