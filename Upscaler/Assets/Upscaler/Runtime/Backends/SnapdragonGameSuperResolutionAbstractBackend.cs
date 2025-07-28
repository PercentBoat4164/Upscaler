using System;
using UnityEngine;

namespace Upscaler.Runtime.Backends
{
    public abstract class SnapdragonGameSuperResolutionAbstractBackend : UpscalerBackend
    {
        protected const string UseEdgeDirectionKeyword = "_UPSCALER__USE_EDGE_DIRECTION";
        protected const string UseOutputResolutionMotionVectorsKeyword = "_UPSCALER__ENABLE_OUTPUT_RESOLUTION_MOTION_VECTORS";

        protected static readonly int MainTexID = Shader.PropertyToID("_MainTex");
        protected static readonly int DepthID = Shader.PropertyToID("Upscaler_Depth");
        protected static readonly int MotionVectorID = Shader.PropertyToID("Upscaler_MotionVectors");
        protected static readonly int OpaqueID = Shader.PropertyToID("Upscaler_Opaque");

        protected static readonly int InputScaleID = Shader.PropertyToID("Upscaler_InputScale");
        protected static readonly int ViewportInfoID = Shader.PropertyToID("Upscaler_ViewportInfo");
        protected static readonly int SharpnessID = Shader.PropertyToID("Upscaler_Sharpness");
        protected static readonly int MotionDepthAlphaBufferID = Shader.PropertyToID("Upscaler_MotionDepthAlphaBuffer");
        protected static readonly int MotionDepthAlphaBufferSinkID = Shader.PropertyToID("Upscaler_MotionDepthAlphaBufferSink");
        protected static readonly int LumaID = Shader.PropertyToID("Upscaler_Luma");
        protected static readonly int LumaSinkID = Shader.PropertyToID("Upscaler_LumaSink");
        protected static readonly int LumaHistoryID = Shader.PropertyToID("Upscaler_LumaHistory");
        protected static readonly int LumaNextHistoryID = Shader.PropertyToID("Upscaler_LumaNextHistory");
        protected static readonly int HistoryID = Shader.PropertyToID("Upscaler_History");
        protected static readonly int NextHistoryID = Shader.PropertyToID("Upscaler_NextHistory");
        protected static readonly int OutputSinkID = Shader.PropertyToID("Upscaler_OutputSink");
        protected static readonly int RenderSizeID = Shader.PropertyToID("Upscaler_RenderSize");
        protected static readonly int OutputSizeID = Shader.PropertyToID("Upscaler_OutputSize");
        protected static readonly int RenderSizeRcpID = Shader.PropertyToID("Upscaler_RenderSizeRcp");
        protected static readonly int OutputSizeRcpID = Shader.PropertyToID("Upscaler_OutputSizeRcp");
        protected static readonly int JitterOffsetID = Shader.PropertyToID("Upscaler_JitterOffset");
        protected static readonly int ScaleRatioID = Shader.PropertyToID("Upscaler_ScaleRatio");
        protected static readonly int PreExposureID = Shader.PropertyToID("Upscaler_PreExposure");
        protected static readonly int CameraFovAngleHorID = Shader.PropertyToID("Upscaler_CameraFovAngleHor");
        protected static readonly int MinLerpContributionID = Shader.PropertyToID("Upscaler_MinLerpContribution");
        protected static readonly int ResetID = Shader.PropertyToID("Upscaler_Reset");
        protected static readonly int SameCameraID = Shader.PropertyToID("Upscaler_SameCamera");

        private Matrix4x4 _previousMatrix;

        protected bool IsSameCamera(in Camera camera)
        {
            var current = camera.GetStereoViewMatrix(Camera.StereoscopicEye.Left) * camera.GetStereoNonJitteredProjectionMatrix(Camera.StereoscopicEye.Left);
            float vpDiff = 0;
            for (var i = 0; i < 4; i++) for (var j = 0; j < 4; j++) vpDiff += Math.Abs(current[i, j] - _previousMatrix[i, j]);
            _previousMatrix = current;
            return vpDiff < 1e-5;
        }

        public override Upscaler.Status ComputeInputResolutionConstraints(in Upscaler upscaler, Flags flags)
        {
            double scale;
            switch (upscaler.quality)
            {
                case Upscaler.Quality.Auto:
                    scale = ((uint)upscaler.OutputResolution.x * (uint)upscaler.OutputResolution.y) switch
                    {
                        <= 2560 * 1440 => 0.769,
                        <= 3840 * 2160 => 0.667,
                        _ => 0.5
                    }; break;
                case Upscaler.Quality.AntiAliasing: scale = 1; break;
                case Upscaler.Quality.UltraQualityPlus: scale = 0.769; break;
                case Upscaler.Quality.UltraQuality: scale = 0.667; break;
                case Upscaler.Quality.Quality: scale = 0.588; break;
                case Upscaler.Quality.Balanced: scale = 0.5; break;
                case Upscaler.Quality.Performance: scale = 0.435; break;
                case Upscaler.Quality.UltraPerformance: scale = 0.333; break;
                default: return Upscaler.Status.RecoverableRuntimeError;
            }
            upscaler.RecommendedInputResolution = new Vector2Int((int)Math.Round(upscaler.OutputResolution.x * scale), (int)Math.Round(upscaler.OutputResolution.y * scale));
            upscaler.MinInputResolution = Vector2Int.one;
            upscaler.MaxInputResolution = upscaler.OutputResolution;

            return Upscaler.Status.Success;
        }
    }
}