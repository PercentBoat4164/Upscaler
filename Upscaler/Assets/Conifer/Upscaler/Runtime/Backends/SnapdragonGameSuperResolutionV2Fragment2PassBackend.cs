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
        public static bool Supported { get; }
        private readonly Material _material = new(Resources.Load<Shader>("SnapdragonGameSuperResolution/V2Fragment2Pass"));
        private RenderTexture _history;
        private RenderTexture _motionDepthAlpha;

        static SnapdragonGameSuperResolutionV2Fragment2PassBackend()
        {
            Supported = true;
            try
            {
                if (!SystemInfo.supportsMotionVectors)
                {
                    Supported = false;
                    return;
                }
                var backend = new SnapdragonGameSuperResolutionV2Fragment2PassBackend();
                Supported = backend._material.shader.isSupported;
                backend.Dispose();
            }
            catch
            {
                Supported = false;
            }
        }

        public SnapdragonGameSuperResolutionV2Fragment2PassBackend()
        {
            if (!Supported) return;
            _material.SetFloat(PreExposureID, 1.0f);
        }

        public override Upscaler.Status Update(in Upscaler upscaler, in Texture input, in Texture output)
        {
            if (!Supported) return Upscaler.Status.FatalRuntimeError;
            var inputsMatch = Input == input;
            var outputsMatch = Output == output;

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

            Output = output;
            Input = input;
            return Upscaler.Status.Success;
        }

        public override void Upscale(in Upscaler upscaler, in CommandBuffer commandBuffer = null)
        {
            if (!Supported) return;
            var cameraIsSame = IsSameCamera(upscaler.Camera);
            if (!cameraIsSame)
                _material.SetFloat(CameraFovAngleHorID, Mathf.Tan(Mathf.Deg2Rad * (upscaler.Camera.fieldOfView / 2)) * Input.width / Input.height);
            _material.SetFloat(MinLerpContributionID, cameraIsSame ? 0.3f : 0.0f);
            _material.SetInt(SameCameraID, cameraIsSame ? 1 : 0);
            _material.SetFloat(ResetID, Convert.ToSingle(upscaler.shouldHistoryResetThisFrame));
            _material.SetVector(JitterOffsetID, upscaler.Jitter);

            if (commandBuffer == null)
            {
                Graphics.Blit(Input, _motionDepthAlpha, _material, 0);
                Graphics.Blit(Input, Output as RenderTexture, _material, 1);
                Graphics.CopyTexture(Output, _history);
            }
            else
            {
                commandBuffer.Blit(Input, _motionDepthAlpha, _material, 0);
                commandBuffer.Blit(Input, Output, _material, 1);
                commandBuffer.CopyTexture(Output, _history);
            }
        }

        public override void Dispose()
        {
            if (_history != null && _history.IsCreated()) _history?.Release();
            if (_motionDepthAlpha != null && _motionDepthAlpha.IsCreated()) _motionDepthAlpha?.Release();
        }
    }
}