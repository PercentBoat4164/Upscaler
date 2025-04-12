/**************************************************
 * Upscaler v2.0.1                                *
 * See the UserManual.pdf for more information    *
 **************************************************/

using UnityEngine;
using UnityEngine.Rendering;

namespace Conifer.Upscaler
{
    public class SGSR1Backend : SGSRAbstractBackend
    {
        private static readonly int ViewportInfoID = Shader.PropertyToID("Conifer_Upscaler_ViewportInfo");
        private static readonly int EdgeSharpnessID = Shader.PropertyToID("Conifer_Upscaler_Sharpness");

        private static SupportState _supported = SupportState.Untested;

        public static bool Supported()
        {
            if (_supported != SupportState.Untested) return _supported == SupportState.Supported;
            if (SystemInfo.graphicsShaderLevel < 50) _supported = SupportState.Unsupported;
            else
            {
                try
                {
                    _ = new SGSR1Backend();
                    _supported = SupportState.Supported;
                }
                catch
                {
                    _supported = SupportState.Unsupported;
                }
            }
            return _supported == SupportState.Supported;
        }

        private readonly Material _material = new(Resources.Load<Shader>("SnapdragonGameSuperResolution/v1"));
        private Texture _input;
        private Texture _output;

#if CONIFER_UPSCALER_USE_URP
        public SGSR1Backend() => _material.SetVector(BlitScaleBiasID, new Vector4(1, 1, 0, 0));
#endif

        public override bool ApplyRefresh(in Upscaler upscaler, in Texture input, in Texture output, in Texture depth = null, in Texture motion = null, bool autoReactive = false, in Texture opaque = null, in Texture reactive = null)
        {
            var inputResolution = upscaler.InputResolution;
            _material.SetVector(ViewportInfoID, new Vector4(1.0f / inputResolution.x, 1.0f / inputResolution.y, inputResolution.x, inputResolution.y));
            _material.SetFloat(EdgeSharpnessID, upscaler.sharpness + 1);
            if (upscaler.useEdgeDirection) _material.EnableKeyword("CONIFER__UPSCALER__USE_EDGE_DIRECTION");
            else _material.DisableKeyword("CONIFER__UPSCALER__USE_EDGE_DIRECTION");

            _input = input;
            _output = output;
            return true;
        }

        public override void Upscale(in CommandBuffer commandBuffer = null)
        {
            if (commandBuffer == null) Graphics.Blit(_input, _output as RenderTexture, _material, 0);
            else commandBuffer.Blit(_input, _output, _material, 0);
        }
    }
}