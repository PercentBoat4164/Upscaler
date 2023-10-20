using UnityEngine;
using UnityEngine.Experimental.Rendering;

public abstract class RenderPipeline
{
    protected const GraphicsFormat MotionFormat = GraphicsFormat.R16G16_SFloat;
    protected static GraphicsFormat ColorFormat(bool HDRActive) => SystemInfo.GetGraphicsFormat(HDRActive ? DefaultFormat.HDR : DefaultFormat.LDR);
    protected static GraphicsFormat DepthFormat => SystemInfo.GetGraphicsFormat(DefaultFormat.DepthStencil);

    public abstract void PrepareRendering(
        Vector2 renderingResolution,
        Vector2 upscalingResolution,
        Plugin.Mode mode
    );

    public abstract void Shutdown();

    public abstract bool ManageOutputTarget(Plugin.Mode mode, Vector2Int upscalingResolution);

    public abstract bool ManageMotionVectorTarget(Plugin.Mode mode, Vector2Int upscalingResolution);

    public abstract bool ManageInColorTarget(Plugin.Mode mode, Vector2Int upscalingResolution);
}