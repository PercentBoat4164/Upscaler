using System;
using JetBrains.Annotations;
using UnityEngine;
using UnityEngine.Rendering;

namespace Upscaler.Runtime.Backends
{
    public abstract class UpscalerBackend : IDisposable
    {
        [Flags]
        public enum Flags : byte
        {
            None = 0,
            OutputResolutionMotionVectors = 1 << 0,
            EnableHDR = 1 << 1
        }

        protected Texture Input;
        protected Texture Output;

        public abstract Upscaler.Status ComputeInputResolutionConstraints([NotNull] in Upscaler upscaler, Flags flags);
        public abstract Upscaler.Status Update([NotNull] in Upscaler upscaler, [NotNull] in Texture input, [NotNull] in Texture output, Flags flags);
        public abstract void Upscale([NotNull] in Upscaler upscaler, [NotNull] in CommandBuffer commandBuffer, in Texture depth, in Texture motion, in Texture opaque = null);
        public abstract void Dispose();
    }
}