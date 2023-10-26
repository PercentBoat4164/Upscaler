using UnityEngine;
using UnityEngine.Experimental.Rendering;

public abstract class RenderPipeline
{
    public enum PipelineType
    {
        Builtin,
        Universal
    }

    protected const GraphicsFormat MotionFormat = GraphicsFormat.R16G16_SFloat;
    protected static GraphicsFormat ColorFormat(bool HDRActive) => SystemInfo.GetGraphicsFormat(HDRActive ? DefaultFormat.HDR : DefaultFormat.LDR);
    protected static GraphicsFormat DepthFormat => SystemInfo.GetGraphicsFormat(DefaultFormat.DepthStencil);
    public static PipelineType Type;

    protected RenderPipeline(PipelineType type)
    {
        Type = type;
    }

    public abstract void PrepareRendering(
        Vector2Int renderingResolution,
        Vector2Int upscalingResolution,
        Plugin.Mode mode
    );

    public abstract void Shutdown();

    public abstract bool ManageOutputTarget(Plugin.Mode mode, Vector2Int upscalingResolution);

    public abstract bool ManageMotionVectorTarget(Plugin.Mode mode, Vector2Int upscalingResolution);

    public abstract bool ManageInColorTarget(Plugin.Mode mode, Vector2Int maximumDynamicRenderingResolution);
}