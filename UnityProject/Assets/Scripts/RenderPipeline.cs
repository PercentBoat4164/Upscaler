using UnityEngine;
using UnityEngine.Rendering;

public abstract class RenderPipeline
{
    public abstract void RecordCommandBuffers(
        CommandBuffer setRenderingResolution,
        CommandBuffer upscale,
        Vector2 renderingResolution,
        Vector2 upscalingResolution,
        RenderTexture motionVectors,
        RenderTexture inputTarget,
        RenderTexture outputTarget,
        Plugin.Upscaler upscaler
    );

    public abstract void BeforeCameraCulling();
}