using UnityEngine;

namespace Upscaler.impl
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

        public abstract void UpdatePostUpscaleCommandBuffer();

        public abstract bool ManageOutputTarget(Upscaler.UpscalerMode upscalerMode, Vector2Int upscalingResolution);

        public abstract bool ManageMotionVectorTarget(Upscaler.UpscalerMode upscalerMode,
            Upscaler.QualityMode qualityMode,
            Vector2Int upscalingResolution);

        public abstract bool ManageInColorTarget(Upscaler.UpscalerMode upscalerMode, Upscaler.QualityMode qualityMode,
            Vector2Int maximumDynamicRenderingResolution);

        public abstract void Shutdown();
    }
}