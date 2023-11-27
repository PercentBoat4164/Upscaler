using UnityEngine;

public abstract class RenderPipeline
{
    public enum PipelineType
    {
        Builtin,
        Universal
    }

    public static PipelineType Type;
    protected readonly Camera Camera;

    protected RenderPipeline(Camera camera, PipelineType type)
    {
        Type = type;
        Camera = camera;
    }

    public abstract void UpdatePostUpscaleCommandBuffer();

    public abstract bool ManageOutputTarget(Plugin.Mode mode, Vector2Int upscalingResolution);

    public abstract bool ManageMotionVectorTarget(Plugin.Mode mode, Plugin.Quality quality, Vector2Int upscalingResolution);

    public abstract bool ManageInColorTarget(Plugin.Mode mode, Plugin.Quality quality, Vector2Int maximumDynamicRenderingResolution);

    public abstract void Shutdown();
}