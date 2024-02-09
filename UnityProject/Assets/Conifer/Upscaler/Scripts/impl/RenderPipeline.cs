using UnityEngine;

namespace Conifer.Upscaler.Scripts.impl
{
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

        public virtual void UpdatePostUpscaleCommandBuffer()
        {
        }

        public abstract bool ManageOutputTarget(Upscaler.UpscalerMode upscalerMode, Vector2Int resolution);

        public abstract bool ManageMotionVectorTarget(Upscaler.UpscalerMode upscalerMode,
            Vector2Int resolution);

        public abstract bool ManageInColorTarget(Upscaler.UpscalerMode upscalerMode,
            Vector2Int resolution);

        public abstract void Shutdown();
    }
}