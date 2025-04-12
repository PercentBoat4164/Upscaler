/**************************************************
 * Upscaler v2.0.1                                *
 * See the UserManual.pdf for more information    *
 **************************************************/

using System;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;

namespace Conifer.Upscaler
{
    public class SGSR2F2Backend : SGSRAbstractBackend
    {
            private static readonly int MotionDepthAlphaBufferID = Shader.PropertyToID("Conifer_Upscaler_MotionDepthAlphaBuffer");
            private static readonly int HistoryID = Shader.PropertyToID("Conifer_Upscaler_History");
            private static readonly int RenderSizeID = Shader.PropertyToID("Conifer_Upscaler_RenderSize");
            private static readonly int OutputSizeID = Shader.PropertyToID("Conifer_Upscaler_OutputSize");
            private static readonly int RenderSizeRcpID = Shader.PropertyToID("Conifer_Upscaler_RenderSizeRcp");
            private static readonly int OutputSizeRcpID = Shader.PropertyToID("Conifer_Upscaler_OutputSizeRcp");
            private static readonly int JitterOffsetID = Shader.PropertyToID("Conifer_Upscaler_JitterOffset");
            private static readonly int ScaleRatioID = Shader.PropertyToID("Conifer_Upscaler_ScaleRatio");
            private static readonly int PreExposureID = Shader.PropertyToID("Conifer_Upscaler_PreExposure");
            private static readonly int CameraFovAngleHorID = Shader.PropertyToID("Conifer_Upscaler_CameraFovAngleHor");
            private static readonly int MinLerpContributionID = Shader.PropertyToID("Conifer_Upscaler_MinLerpContribution");
            private static readonly int ResetID = Shader.PropertyToID("Conifer_Upscaler_Reset");
            private static readonly int SameCameraID = Shader.PropertyToID("Conifer_Upscaler_SameCamera");

        private static SupportState _supported = SupportState.Untested;

        public static bool Supported()
        {
            if (_supported != SupportState.Untested) return _supported == SupportState.Supported;
            if (SystemInfo.graphicsShaderLevel < 50 || !SystemInfo.supportsMotionVectors) _supported = SupportState.Unsupported;
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

        private readonly Material _material = new(Resources.Load<Shader>("SnapdragonGameSuperResolution/v2F2"));
        private Matrix4x4 _previousMatrix;
        private RenderTexture _history;
        private RenderTexture _motionDepthAlpha;
        private Texture _input;
        private Texture _output;

        public SGSR2F2Backend()
        {
#if CONIFER_UPSCALER_USE_URP
            _material.SetVector(BlitScaleBiasID, new Vector4(1, 1, 0, 0));
#endif
            _material.SetFloat(PreExposureID, 1.0f);
        }

        public override bool ApplyRefresh(in Upscaler upscaler, in Texture input, in Texture output, in Texture depth = null, in Texture motion = null, bool autoReactive = false, in Texture opaque = null, in Texture reactive = null)
        {
            var inputsMatch = _input == input;
            var outputsMatch = _output == output;

            var outputResolution = upscaler.OutputResolution;
            if (!outputsMatch || _history == null)
            {
                _history?.Release();
                _history = new RenderTexture(outputResolution.x, outputResolution.y, 0, output?.graphicsFormat ?? SystemInfo.GetGraphicsFormat(upscaler.HDR ? DefaultFormat.HDR : DefaultFormat.LDR));
                _material.SetTexture(HistoryID, _history);
                _material.SetVector(OutputSizeID, (Vector2)outputResolution);
                _material.SetVector(OutputSizeRcpID, Vector2.one / outputResolution);
            }
            var inputResolution = upscaler.InputResolution;
            if (!inputsMatch || _motionDepthAlpha == null)
            {
                _material.SetTexture(BlitTextureID, input);
                _motionDepthAlpha?.Release();
                _motionDepthAlpha = new RenderTexture(input.width, input.height, 0, GraphicsFormat.R16G16B16A16_SFloat);
                _material.SetTexture(MotionDepthAlphaBufferID, _motionDepthAlpha);
                _material.SetVector(RenderSizeID, (Vector2)inputResolution);
                _material.SetVector(RenderSizeRcpID, Vector2.one / inputResolution);
            }
            if (!inputsMatch || !outputsMatch) _material.SetVector(ScaleRatioID, new Vector4((float)outputResolution.x / inputResolution.x, Mathf.Min(20.0f, Mathf.Pow((float)outputResolution.x * outputResolution.y / (inputResolution.x * inputResolution.y), 3.0f))));

            var current = upscaler.Camera.GetStereoViewMatrix(UnityEngine.Camera.StereoscopicEye.Left) * upscaler.Camera.GetStereoNonJitteredProjectionMatrix(UnityEngine.Camera.StereoscopicEye.Left);
            float vpDiff = 0;
            for (var i = 0; i < 4; i++) for (var j = 0; j < 4; j++) vpDiff += Math.Abs(current[i, j] - _previousMatrix[i, j]);
            _previousMatrix = current;
            var cameraIsSame = vpDiff < 1e-5;
            if (!cameraIsSame || !inputsMatch) _material.SetFloat(CameraFovAngleHorID, Mathf.Tan(Mathf.Deg2Rad * (upscaler.Camera.fieldOfView / 2)) * inputResolution.x / inputResolution.y);
            _material.SetFloat(MinLerpContributionID, cameraIsSame ? 0.3f : 0.0f);
            _material.SetInt(SameCameraID, cameraIsSame ? 1 : 0);
            _material.SetFloat(ResetID, Convert.ToSingle(upscaler.NativeInterface.ShouldResetHistory));
            _material.SetVector(JitterOffsetID, upscaler.Jitter);

            _output = output;
            _input = input;
            return true;
        }

        public override void Upscale(in CommandBuffer commandBuffer = null)
        {
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
            _history?.Release();
            _motionDepthAlpha?.Release();
        }
    }
}