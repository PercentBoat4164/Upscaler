using UnityEngine;
using UnityEngine.Rendering;

namespace Upscaler.Runtime.Backends
{
    public class SnapdragonGameSuperResolutionV1Backend : SnapdragonGameSuperResolutionAbstractBackend
    {
        public static bool Supported { get; }
        private readonly Material _material = new(Resources.Load<Shader>("SnapdragonGameSuperResolution/V1"));

        static SnapdragonGameSuperResolutionV1Backend()
        {
            Supported = true;
            try
            {
                var backend = new SnapdragonGameSuperResolutionV1Backend();
                Supported = backend._material.shader.isSupported;
                backend.Dispose();
            }
            catch
            {
                Supported = false;
            }
        }

        public override Upscaler.Status Update(in Upscaler upscaler, in Texture input, in Texture output, Flags flags)
        {
            if (!Supported) return Upscaler.Status.FatalRuntimeError;
            _material.SetVector(InputScaleID, new Vector2((float)upscaler.InputResolution.x / input.width, (float)upscaler.InputResolution.y / input.height));
            _material.SetVector(ViewportInfoID, new Vector4(1.0f / input.width, 1.0f / input.height, input.width, input.height));
            _material.SetFloat(SharpnessID, upscaler.sharpness + 1);
            if (upscaler.useEdgeDirection) _material.EnableKeyword(UseEdgeDirectionKeyword);
            else _material.DisableKeyword(UseEdgeDirectionKeyword);

            Input = input;
            Output = output;
            return Upscaler.Status.Success;
        }

        public override void Upscale(in Upscaler upscaler, in CommandBuffer commandBuffer, in Texture depth, in Texture motion, in Texture opaque = null)
        {
            if (!Supported) return;
            commandBuffer.Blit(Input, Output, _material, 0);
        }

        public override void Dispose() { }
    }
}