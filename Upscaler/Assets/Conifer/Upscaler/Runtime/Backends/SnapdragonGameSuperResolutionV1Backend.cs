/**************************************************
 * Upscaler v2.0.1                                *
 * See the UserManual.pdf for more information    *
 **************************************************/

using JetBrains.Annotations;
using UnityEngine;
using UnityEngine.Rendering;

namespace Conifer.Upscaler
{
    public class SnapdragonGameSuperResolutionV1Backend : SnapdragonGameSuperResolutionAbstractBackend
    {
        private static SupportState _supported = SupportState.Untested;

        public static bool Supported()
        {
            if (_supported != SupportState.Untested) return _supported == SupportState.Supported;
            if (SystemInfo.graphicsShaderLevel < 50) _supported = SupportState.Unsupported;
            else
            {
                try
                {
                    _ = new SnapdragonGameSuperResolutionV1Backend();
                    _supported = SupportState.Supported;
                }
                catch
                {
                    _supported = SupportState.Unsupported;
                }
            }
            return _supported == SupportState.Supported;
        }

        private readonly Material _material = new(Resources.Load<Shader>("SnapdragonGameSuperResolution/V1"));
        private Texture _input;
        private Texture _output;

#if CONIFER_UPSCALER_USE_URP
        public SGSR1Backend() => _material.SetVector(BlitScaleBiasID, new Vector4(1, 1, 0, 0));
#endif

        public override bool Update(in Upscaler upscaler, in Texture input, in Texture output)
        {
            _material.SetVector(ViewportInfoID, new Vector4(1.0f / input.width, 1.0f / input.height, input.width, input.height));
            _material.SetFloat(SharpnessID, upscaler.sharpness + 1);
            if (upscaler.useEdgeDirection) _material.EnableKeyword("CONIFER__UPSCALER__USE_EDGE_DIRECTION");
            else _material.DisableKeyword("CONIFER__UPSCALER__USE_EDGE_DIRECTION");

            _input = input;
            _output = output;
            return true;
        }

        public override void Upscale(in Upscaler upscaler, in CommandBuffer commandBuffer = null)
        {
            if (commandBuffer == null) Graphics.Blit(_input, _output as RenderTexture, _material, 0);
            else commandBuffer.Blit(_input, _output, _material, 0);
        }
    }
}