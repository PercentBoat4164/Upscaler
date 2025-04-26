/**************************************************
 * Upscaler v2.0.1                                *
 * See the UserManual.pdf for more information    *
 **************************************************/

using UnityEngine;
using UnityEngine.Rendering;

namespace Conifer.Upscaler
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

        public override Upscaler.Status Update(in Upscaler upscaler, in Texture input, in Texture output)
        {
            if (!Supported) return Upscaler.Status.FatalRuntimeError;
            _material.SetVector(ViewportInfoID, new Vector4(1.0f / input.width, 1.0f / input.height, input.width, input.height));
            _material.SetFloat(SharpnessID, upscaler.sharpness + 1);
            if (upscaler.useEdgeDirection) _material.EnableKeyword("CONIFER__UPSCALER__USE_EDGE_DIRECTION");
            else _material.DisableKeyword("CONIFER__UPSCALER__USE_EDGE_DIRECTION");

            Input = input;
            Output = output;
            return Upscaler.Status.Success;
        }

        public override void Upscale(in Upscaler upscaler, in CommandBuffer commandBuffer = null)
        {
            if (!Supported) return;
            if (commandBuffer == null) Graphics.Blit(Input, Output as RenderTexture, _material, 0);
            else commandBuffer.Blit(Input, Output, _material, 0);
        }

        public override void Dispose() { }
    }
}