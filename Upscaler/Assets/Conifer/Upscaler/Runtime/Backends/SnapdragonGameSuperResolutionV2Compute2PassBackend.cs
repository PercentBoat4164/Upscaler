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
    public class SnapdragonGameSuperResolutionV2Compute2PassBackend : SnapdragonGameSuperResolutionAbstractBackend
    {
        public static bool Supported { get; }
        private readonly ComputeShader _computeShader = Resources.Load<ComputeShader>("SnapdragonGameSuperResolution/V2Compute2Pass");
        private RenderTexture _history;
        //@todo: Use temporary RenderTextures for anything that does not require cross-frame storage.
        private RenderTexture _luma;
        private RenderTexture _motionDepthAlpha;

        static SnapdragonGameSuperResolutionV2Compute2PassBackend()
        {
            Supported = true;
            try
            {
                if (!SystemInfo.supportsMotionVectors || !SystemInfo.supportsComputeShaders)
                {
                    Supported = false;
                    return;
                }
                var backend = new SnapdragonGameSuperResolutionV2Compute2PassBackend();
                Supported = backend._computeShader.IsSupported(0) && backend._computeShader.IsSupported(1);
                backend.Dispose();
            }
            catch
            {
                Supported = false;
            }
        }

        public SnapdragonGameSuperResolutionV2Compute2PassBackend()
        {
            if (!Supported) return;
            _computeShader.SetFloat(PreExposureID, 1.0f);
        }

        public override Upscaler.Status Update(in Upscaler upscaler, in Texture input, in Texture output, Flags flags)
        {
            if (!Supported) return Upscaler.Status.FatalRuntimeError;
            var inputsMatch = Input == input;
            var outputsMatch = Output == output;

            if (!outputsMatch || _history == null)
            {
                _computeShader.SetTexture(1, OutputSinkID, output);
                _history?.Release();
                _history = new RenderTexture(upscaler.OutputResolution.x, upscaler.OutputResolution.y, 0, GraphicsFormat.R16G16B16A16_SFloat)
                {
                    enableRandomWrite = true
                };
                _history.Create();
                _computeShader.SetTexture(1, NextHistoryID, _history);
                _computeShader.SetVector(OutputSizeID, (Vector2)upscaler.OutputResolution);
                _computeShader.SetVector(OutputSizeRcpID, Vector2.one / upscaler.OutputResolution);
            }
            if (!inputsMatch || upscaler.InputResolution.x != input.width || upscaler.InputResolution.y != input.height || _motionDepthAlpha == null)
            {
                _computeShader.SetTexture(0, MainTexID, input);
                _motionDepthAlpha?.Release();
                _motionDepthAlpha = new RenderTexture(upscaler.InputResolution.x, upscaler.InputResolution.y, 0, GraphicsFormat.R16G16B16A16_SFloat)
                {
                    enableRandomWrite = true
                };
                _motionDepthAlpha.Create();
                _computeShader.SetTexture(1, MotionDepthAlphaBufferID, _motionDepthAlpha);
                _computeShader.SetTexture(0, MotionDepthAlphaBufferSinkID, _motionDepthAlpha);
                _luma?.Release();
                _luma = new RenderTexture(upscaler.InputResolution.x, upscaler.InputResolution.y, 0, GraphicsFormat.R32_UInt)
                {
                    enableRandomWrite = true
                };
                _luma.Create();
                _computeShader.SetTexture(1, LumaID, _luma);
                _computeShader.SetTexture(0, LumaSinkID, _luma);
                _computeShader.SetVector(RenderSizeID, (Vector2)upscaler.InputResolution);
                _computeShader.SetVector(RenderSizeRcpID, Vector2.one / upscaler.InputResolution);
            }
            if (!inputsMatch || !outputsMatch) _computeShader.SetVector(ScaleRatioID, new Vector4((float)upscaler.OutputResolution.x / upscaler.InputResolution.x, Mathf.Min(20.0f, Mathf.Pow((float)upscaler.OutputResolution.x * upscaler.OutputResolution.y / (upscaler.InputResolution.x * upscaler.InputResolution.y), 3.0f))));
            if ((flags & Flags.OutputResolutionMotionVectors) == Flags.OutputResolutionMotionVectors) _computeShader.EnableKeyword(UseOutputResolutionMotionVectorsKeyword);
            else _computeShader.DisableKeyword(UseOutputResolutionMotionVectorsKeyword);

            Output = output;
            Input = input;
            return Upscaler.Status.Success;
        }

        public override void Upscale(in Upscaler upscaler, in CommandBuffer commandBuffer, in Texture depth, in Texture motion, in Texture opaque = null)
        {
            if (!Supported) return;
            var cameraIsSame = IsSameCamera(upscaler.Camera);
            if (!cameraIsSame) _computeShader.SetFloat(CameraFovAngleHorID, Mathf.Tan(Mathf.Deg2Rad * (upscaler.Camera.fieldOfView / 2)) * Input.width / Input.height);
            _computeShader.SetFloat(MinLerpContributionID, cameraIsSame ? 0.3f : 0.0f);
            _computeShader.SetInt(SameCameraID, cameraIsSame ? 1 : 0);
            _computeShader.SetFloat(ResetID, Convert.ToSingle(upscaler.shouldHistoryResetThisFrame));
            _computeShader.SetVector(JitterOffsetID, upscaler.Jitter);
            _computeShader.SetTexture(0, DepthID, depth ?? Texture2D.whiteTexture);
            _computeShader.SetTexture(0, MotionVectorID, motion ?? Texture2D.blackTexture);

            commandBuffer.GetTemporaryRT(HistoryID, _history.descriptor);
            commandBuffer.CopyTexture(_history, HistoryID);
            commandBuffer.DispatchCompute(_computeShader, 0, Mathf.CeilToInt(Input.width / 8.0f), Mathf.CeilToInt(Input.height / 8.0f), 1);
            commandBuffer.DispatchCompute(_computeShader, 1, Mathf.CeilToInt(Output.width / 8.0f), Mathf.CeilToInt(Output.height / 8.0f), 1);
            commandBuffer.ReleaseTemporaryRT(HistoryID);
        }

        public override void Dispose()
        {
            if (_luma != null && _luma.IsCreated()) _luma.Release();
            if (_history != null && _history.IsCreated()) _history.Release();
            if (_motionDepthAlpha != null && _motionDepthAlpha.IsCreated()) _motionDepthAlpha.Release();
        }
    }
}