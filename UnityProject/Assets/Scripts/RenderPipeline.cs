using UnityEngine;
using UnityEngine.Rendering;

public abstract class RenderPipeline
{
    public abstract void PrepareRendering(
        CommandBuffer setRenderingResolution,
        CommandBuffer setDepthSize,
        CommandBuffer upscale,
        Vector2 renderingResolution,
        Vector2 upscalingResolution,
        RenderTexture motionVectors,
        RenderTexture inputTarget,
        RenderTexture outputTarget,
        Plugin.Mode mode
    );
}