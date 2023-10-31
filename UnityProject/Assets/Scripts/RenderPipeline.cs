using UnityEngine;

public abstract class RenderPipeline
{
    public enum PipelineType
    {
        Builtin,
        Universal
    }

    public static PipelineType Type;
    protected readonly Camera _camera;

    protected RenderPipeline(Camera camera, PipelineType type)
    {
        Type = type;
        _camera = camera;
    }

    public abstract bool ManageOutputTarget(Plugin.Mode mode, Vector2Int upscalingResolution);

    public abstract bool ManageMotionVectorTarget(Plugin.Mode mode, Vector2Int upscalingResolution);

    public abstract bool ManageInColorTarget(Plugin.Mode mode, Vector2Int maximumDynamicRenderingResolution);

    public abstract void Shutdown();
}