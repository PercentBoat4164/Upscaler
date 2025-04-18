/**************************************************
 * Upscaler v2.0.1                                *
 * See the UserManual.pdf for more information    *
 **************************************************/

using System;
using JetBrains.Annotations;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;

namespace Conifer.Upscaler
{
    public class SnapdragonGameSuperResolutionV2Fragment2PassBackend : SnapdragonGameSuperResolutionAbstractBackend
    {
        private static SupportState _supported = SupportState.Untested;

        public static bool Supported()
        {
            if (_supported != SupportState.Untested) return _supported == SupportState.Supported;
            if (SystemInfo.graphicsShaderLevel < 50 || !SystemInfo.supportsMotionVectors) _supported = SupportState.Unsupported;
            else
            {
                try
                {
                    _ = new SnapdragonGameSuperResolutionV2Fragment2PassBackend();
                    _supported = SupportState.Supported;
                }
                catch
                {
                    _supported = SupportState.Unsupported;
                }
            }
            return _supported == SupportState.Supported;
        }

        private readonly Material _material = new(Resources.Load<Shader>("SnapdragonGameSuperResolution/V2Fragment2Pass"));
        private Matrix4x4 _previousMatrix;
        private RenderTexture _history;
        private RenderTexture _motionDepthAlpha;
        private Texture _input;
        private Texture _output;

        public SnapdragonGameSuperResolutionV2Fragment2PassBackend()
        {
#if CONIFER_UPSCALER_USE_URP
            _material.SetVector(BlitScaleBiasID, new Vector4(1, 1, 0, 0));
#endif
            _material.SetFloat(PreExposureID, 1.0f);
        }

        public override bool Update(in Upscaler upscaler, in Texture input, in Texture output)
        {
            var inputsMatch = _input == input;
            var outputsMatch = _output == output;

            if (!outputsMatch || _history == null)
            {
                _history?.Release();
                _history = new RenderTexture(upscaler.OutputResolution.x, upscaler.OutputResolution.y, 0, output.graphicsFormat);
                _history.Create();
                _material.SetTexture(HistoryID, _history);
                _material.SetVector(OutputSizeID, (Vector2)upscaler.OutputResolution);
                _material.SetVector(OutputSizeRcpID, Vector2.one / upscaler.OutputResolution);
            }
            if (!inputsMatch || _motionDepthAlpha == null)
            {
                _motionDepthAlpha?.Release();
                _motionDepthAlpha = new RenderTexture(input.width, input.height, 0, GraphicsFormat.R16G16B16A16_SFloat);
                _motionDepthAlpha.Create();
                _material.SetTexture(MotionDepthAlphaBufferID, _motionDepthAlpha);
                _material.SetVector(RenderSizeID, (Vector2)upscaler.InputResolution);
                _material.SetVector(RenderSizeRcpID, Vector2.one / upscaler.InputResolution);
            }
            if (!inputsMatch || !outputsMatch) _material.SetVector(ScaleRatioID, new Vector4((float)upscaler.OutputResolution.x / upscaler.InputResolution.x, Mathf.Min(20.0f, Mathf.Pow((float)upscaler.OutputResolution.x * upscaler.OutputResolution.y / (upscaler.InputResolution.x * upscaler.InputResolution.y), 3.0f))));

            _output = output;
            _input = input;
            return true;
        }

        public override void Upscale(in Upscaler upscaler, in CommandBuffer commandBuffer = null)
        {
            var cameraIsSame = IsSameCamera(upscaler.Camera);
            if (!cameraIsSame)
                _material.SetFloat(CameraFovAngleHorID, Mathf.Tan(Mathf.Deg2Rad * (upscaler.Camera.fieldOfView / 2)) * _input.width / _input.height);
            _material.SetFloat(MinLerpContributionID, cameraIsSame ? 0.3f : 0.0f);
            _material.SetInt(SameCameraID, cameraIsSame ? 1 : 0);
            _material.SetFloat(ResetID, Convert.ToSingle(upscaler.shouldHistoryResetThisFrame));
            _material.SetVector(JitterOffsetID, upscaler.Jitter);

            if (commandBuffer == null)
            {
                Graphics.Blit(_input, _motionDepthAlpha, _material, 0);
                Graphics.Blit(_input, _output as RenderTexture, _material, 1);
                Graphics.CopyTexture(_output, _history);
            }
            else
            {
                commandBuffer.Blit(_input, _motionDepthAlpha, _material, 0);
                commandBuffer.Blit(_input, _output, _material, 1);
                commandBuffer.CopyTexture(_output, _history);
            }
        }

        public new void Dispose()
        {
            base.Dispose();
            if (_history != null && _history.IsCreated()) _history?.Release();
            if (_motionDepthAlpha != null && _motionDepthAlpha.IsCreated()) _motionDepthAlpha?.Release();
        }
    }
}