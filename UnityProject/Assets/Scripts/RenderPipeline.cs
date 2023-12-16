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

    public abstract bool ManageOutputTarget(Plugin.UpscalerMode upscalerMode, Vector2Int resolution);

    public abstract bool ManageMotionVectorTarget(Plugin.UpscalerMode upscalerMode, Vector2Int resolution);

    public abstract bool ManageInColorTarget(Plugin.UpscalerMode upscalerMode, Vector2Int resolution);

    public abstract void Shutdown();
}