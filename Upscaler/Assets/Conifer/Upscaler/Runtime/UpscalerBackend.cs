using System;
using JetBrains.Annotations;
using UnityEngine;
using UnityEngine.Rendering;

namespace Conifer.Upscaler
{
    public abstract class UpscalerBackend : IDisposable
    {
        protected static readonly int MainTexID = Shader.PropertyToID("_MainTex");
#if CONIFER_UPSCALER_USE_URP
        protected static readonly int BlitScaleBiasID = Shader.PropertyToID("_BlitScaleBias");
#endif

        protected Texture Input;
        protected Texture Output;

        public abstract Upscaler.Status ComputeInputResolutionConstraints([NotNull] in Upscaler upscaler);
        public abstract Upscaler.Status Update([NotNull] in Upscaler upscaler, [NotNull] in Texture input, [NotNull] in Texture output);
        public abstract void Upscale([NotNull] in Upscaler upscaler, in CommandBuffer commandBuffer = null);
        public abstract void Dispose();
    }
}