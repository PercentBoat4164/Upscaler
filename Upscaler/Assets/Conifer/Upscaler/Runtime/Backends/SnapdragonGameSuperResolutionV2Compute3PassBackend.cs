using System;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;

namespace Conifer.Upscaler
{
    public class SnapdragonGameSuperResolutionV2Compute3PassBackend : SnapdragonGameSuperResolutionAbstractBackend
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
                    _ = new SnapdragonGameSuperResolutionV2Compute2PassBackend();
                    _supported = SupportState.Supported;
                }
                catch
                {
                    _supported = SupportState.Unsupported;
                }
            }
            return _supported == SupportState.Supported;
        }

        private readonly ComputeShader _computeShader = Resources.Load<ComputeShader>("SnapdragonGameSuperResolution/V2Compute3Pass");
        private RenderTexture _history;
        private RenderTexture _lumaHistory;
        private RenderTexture _motionDepthAlpha;
        private Texture _input;
        private Texture _output;

        public SnapdragonGameSuperResolutionV2Compute3PassBackend() => _computeShader.SetFloat(PreExposureID, 1.0f);

        public override bool Update(in Upscaler upscaler, in Texture input, in Texture output)
        {
            var inputsMatch = _input == input;
            var outputsMatch = _output == output;

            if (!outputsMatch || _history == null)
            {
                _computeShader.SetTexture(2, OutputSinkID, output);
                _history?.Release();
                _history = new RenderTexture(upscaler.OutputResolution.x, upscaler.OutputResolution.y, 0, GraphicsFormat.R16G16B16A16_SFloat)
                {
                    enableRandomWrite = true
                };
                _history.Create();
                _computeShader.SetTexture(2, NextHistoryID, _history);
                _computeShader.SetVector(OutputSizeID, (Vector2)upscaler.OutputResolution);
                _computeShader.SetVector(OutputSizeRcpID, Vector2.one / upscaler.OutputResolution);
            }
            if (!inputsMatch || _motionDepthAlpha == null)
            {
                _computeShader.SetTexture(0, MainTexID, input);
                _motionDepthAlpha?.Release();
                _motionDepthAlpha = new RenderTexture(input.width, input.height, 0, GraphicsFormat.R16G16B16A16_SFloat)
                {
                    enableRandomWrite = true
                };
                _motionDepthAlpha.Create();
                _computeShader.SetTexture(1, MotionDepthAlphaBufferID, _motionDepthAlpha);
                _computeShader.SetTexture(0, MotionDepthAlphaBufferSinkID, _motionDepthAlpha);
                _computeShader.SetTexture(2, MotionDepthAlphaBufferID, _motionDepthAlpha);
                _computeShader.SetTexture(1, MotionDepthAlphaBufferSinkID, _motionDepthAlpha);
                _lumaHistory?.Release();
                _lumaHistory = new RenderTexture(input.width, input.height, 0, GraphicsFormat.R32_UInt)
                {
                    enableRandomWrite = true
                };
                _lumaHistory.Create();
                _computeShader.SetTexture(1, LumaNextHistoryID, _lumaHistory);
                _computeShader.SetVector(RenderSizeID, (Vector2)upscaler.InputResolution);
                _computeShader.SetVector(RenderSizeRcpID, Vector2.one / upscaler.InputResolution);
            }
            if (!inputsMatch || !outputsMatch) _computeShader.SetVector(ScaleRatioID, new Vector4((float)upscaler.OutputResolution.x / upscaler.InputResolution.x, Mathf.Min(20.0f, Mathf.Pow((float)upscaler.OutputResolution.x * upscaler.OutputResolution.y / (upscaler.InputResolution.x * upscaler.InputResolution.y), 3.0f))));

            _output = output;
            _input = input;
            return true;
        }

        public override void Upscale(in Upscaler upscaler, in CommandBuffer commandBuffer = null)
        {
            var cameraIsSame = IsSameCamera(upscaler.Camera);
            if (!cameraIsSame) _computeShader.SetFloat(CameraFovAngleHorID, Mathf.Tan(Mathf.Deg2Rad * (upscaler.Camera.fieldOfView / 2)) * _input.width / _input.height);
            _computeShader.SetFloat(MinLerpContributionID, cameraIsSame ? 0.3f : 0.0f);
            _computeShader.SetInt(SameCameraID, cameraIsSame ? 1 : 0);
            _computeShader.SetFloat(ResetID, Convert.ToSingle(upscaler.shouldHistoryResetThisFrame));
            _computeShader.SetVector(JitterOffsetID, upscaler.Jitter);

            if (commandBuffer == null)
            {
                var history = RenderTexture.GetTemporary(_history.descriptor);
                var luma = RenderTexture.GetTemporary(_lumaHistory.descriptor);
                var lumaHistory = RenderTexture.GetTemporary(_lumaHistory.descriptor);
                Graphics.CopyTexture(_history, history);
                Graphics.CopyTexture(_lumaHistory, lumaHistory);
                _computeShader.SetTexture(2, HistoryID, history);
                _computeShader.SetTexture(0, LumaSinkID, luma);
                _computeShader.SetTexture(1, LumaID, luma);
                _computeShader.SetTexture(2, LumaID, luma);
                _computeShader.SetTexture(1, LumaHistoryID, lumaHistory);
                _computeShader.Dispatch(0, Mathf.CeilToInt(_input.width / 8.0f), Mathf.CeilToInt(_input.height / 8.0f), 1);
                _computeShader.Dispatch(1, Mathf.CeilToInt(_input.width / 8.0f), Mathf.CeilToInt(_input.height / 8.0f), 1);
                _computeShader.Dispatch(2, Mathf.CeilToInt(_output.width / 8.0f), Mathf.CeilToInt(_output.height / 8.0f), 1);
                RenderTexture.ReleaseTemporary(history);
                RenderTexture.ReleaseTemporary(luma);
                RenderTexture.ReleaseTemporary(lumaHistory);
            }
            else
            {
                commandBuffer.GetTemporaryRT(HistoryID, _history.descriptor);
                commandBuffer.GetTemporaryRT(LumaID, _lumaHistory.descriptor);
                commandBuffer.SetGlobalTexture(LumaSinkID, LumaID);
                commandBuffer.GetTemporaryRT(LumaHistoryID, _lumaHistory.descriptor);
                commandBuffer.CopyTexture(_history, HistoryID);
                commandBuffer.CopyTexture(_lumaHistory, LumaHistoryID);
                commandBuffer.DispatchCompute(_computeShader, 0, Mathf.CeilToInt(_input.width / 8.0f), Mathf.CeilToInt(_input.height / 8.0f), 1);
                commandBuffer.DispatchCompute(_computeShader, 1, Mathf.CeilToInt(_input.width / 8.0f), Mathf.CeilToInt(_input.height / 8.0f), 1);
                commandBuffer.DispatchCompute(_computeShader, 2, Mathf.CeilToInt(_output.width / 8.0f), Mathf.CeilToInt(_output.height / 8.0f), 1);
                commandBuffer.ReleaseTemporaryRT(HistoryID);
                commandBuffer.ReleaseTemporaryRT(LumaID);
                commandBuffer.ReleaseTemporaryRT(LumaHistoryID);
            }
        }
    }
}