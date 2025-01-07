/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

/**************************************************
 * Upscaler v1.2.0                                *
 * See the UserManual.pdf for more information    *
 **************************************************/

using System;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

namespace Conifer.Upscaler.URP
{
    [DisallowMultipleRendererFeature("Upscaler Renderer Feature")]
    public class UpscalerRendererFeature : ScriptableRendererFeature
    {
        private static Mesh _triangle;
        private static Mesh _upsideDownTriangle;
        private static Material _depthBlitMaterial;
        private static Material _blitMaterial;
        private static readonly int BlitID = Shader.PropertyToID("_MainTex");
        private static readonly int MotionID = Shader.PropertyToID("_MotionVectorTexture");
        private static readonly int OffsetID = Shader.PropertyToID("Conifer_Upscaler_Offset");
        private static readonly int ScaleID = Shader.PropertyToID("Conifer_Upscaler_Scale");
        private static readonly Matrix4x4 Ortho = Matrix4x4.Ortho(-1, 1, 1, -1, 1, -1);
        private static readonly Matrix4x4 LookAt = Matrix4x4.LookAt(Vector3.back, Vector3.forward, Vector3.up);

        private static RTHandle _cameraRenderResolutionColorTarget;
        private static RTHandle _cameraRenderResolutionDepthTarget;
        private static RTHandle _cameraOutputResolutionColorTarget;
        private static RTHandle _cameraOutputResolutionDepthTarget;
        private static bool _renderTargetsUpdated;

        private static Upscaler _upscaler;

        private static void MultipurposeBlit(CommandBuffer cb, RenderTargetIdentifier src, RenderTargetIdentifier dst, bool depth, bool flip, Vector2? offset = null, Vector2? srcRes = null, Vector2? dstRes = null)
        {
            cb.SetProjectionMatrix(Ortho);
            cb.SetViewMatrix(LookAt);
            cb.SetRenderTarget(dst);
            cb.SetGlobalTexture(BlitID, src);
            cb.SetGlobalVector(OffsetID, (offset ?? Vector2.zero) / (dstRes ?? Vector2.one));
            cb.SetGlobalVector(ScaleID, (srcRes ?? Vector2.one) / (dstRes ?? Vector2.one));
            cb.DrawMesh(flip ? _upsideDownTriangle : _triangle, Matrix4x4.identity, depth ? _depthBlitMaterial : _blitMaterial);
        }

        private static void MultipurposeBlit(CommandBuffer cb, RenderTargetIdentifier src, RenderTargetIdentifier dst, bool depth, Vector2? offset = null, Vector2? scale = null)
        {
            cb.SetProjectionMatrix(Ortho);
            cb.SetViewMatrix(LookAt);
            cb.SetRenderTarget(dst);
            cb.SetGlobalTexture(BlitID, src);
            cb.SetGlobalVector(OffsetID, offset ?? Vector2.zero);
            cb.SetGlobalVector(ScaleID, scale ?? Vector2.one);
            cb.DrawMesh(_triangle, Matrix4x4.identity, depth ? _depthBlitMaterial : _blitMaterial);
        }

        private class SetMipBias : ScriptableRenderPass
        {
            private static readonly int GlobalMipBias = Shader.PropertyToID("_GlobalMipBias");
            private static int _jitterIndex;

            public SetMipBias() => renderPassEvent = RenderPassEvent.BeforeRenderingPrePasses;

            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var cb = CommandBufferPool.Get("SetMipBias");
                var mipBias = (float)Math.Log((float)_upscaler.InputResolution.x / _upscaler.OutputResolution.x, 2f) - 1f;
                cb.SetGlobalVector(GlobalMipBias, new Vector4(mipBias, mipBias * mipBias));
                _upscaler.Jitter = new Vector2(HaltonSequence.Get(_jitterIndex, 2), HaltonSequence.Get(_jitterIndex, 3));
                _jitterIndex = (_jitterIndex + 1) % (int)Math.Ceiling(7 * Math.Pow((float)_upscaler.OutputResolution.x / _upscaler.InputResolution.x, 2));
                var clipSpaceJitter = -_upscaler.Jitter / _upscaler.InputResolution * 2;
                var projectionMatrix = renderingData.cameraData.GetProjectionMatrix();
                if (renderingData.cameraData.camera.orthographic)
                {
                    projectionMatrix.m03 += clipSpaceJitter.x;
                    projectionMatrix.m13 += clipSpaceJitter.y;
                }
                else
                {
                    projectionMatrix.m02 += clipSpaceJitter.x;
                    projectionMatrix.m12 += clipSpaceJitter.y;
                }
                cb.SetViewProjectionMatrices(renderingData.cameraData.GetViewMatrix(), projectionMatrix);
                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);
            }
        }

        private class Upscale : ScriptableRenderPass
        {
            private static Texture _lastMotion;
            private static readonly int OpaqueID = Shader.PropertyToID("_CameraOpaqueTexture");
            private static Texture _lastOpaque;
            private static Upscaler.Technique _lastTechnique;
            private static bool _lastReactive;
            private static RTHandle _output;
            private static RTHandle _reactive;
            private static Matrix4x4 _previousProjectionMatrix;

            internal static Material SgsrMaterial;
            internal static Material SgsrConvertMaterial;
            internal static Material SgsrUpscaleMaterial;
            internal static ComputeShader Sgsr2PassConvert;
            internal static ComputeShader Sgsr2PassUpscale;
            internal static ComputeShader Sgsr3PassConvert;
            internal static ComputeShader Sgsr3PassActivate;
            internal static ComputeShader Sgsr3PassUpscale;

            private static RTHandle _motionDepthClipAlphaBuffer;
            private static RTHandle _motionDepthClipAlphaBufferSink;
            private static RTHandle _previousOutput;
            private static RTHandle _lumaHistory;
            private static RTHandle _lumaHistorySink;
            private static RTHandle _history;

            private static readonly int ViewportInfoID = Shader.PropertyToID("Conifer_Upscaler_ViewportInfo");
            private static readonly int EdgeSharpnessID = Shader.PropertyToID("Conifer_Upscaler_EdgeSharpness");
            private static readonly int MotionDepthAlphaBufferID = Shader.PropertyToID("Conifer_Upscaler_MotionDepthAlphaBuffer");
            private static readonly int MotionDepthAlphaBufferSinkID = Shader.PropertyToID("Conifer_Upscaler_MotionDepthAlphaBufferSink");
            private static readonly int LumaID = Shader.PropertyToID("Conifer_Upscaler_Luma");
            private static readonly int LumaSinkID = Shader.PropertyToID("Conifer_Upscaler_LumaSink");
            private static readonly int LumaHistoryID = Shader.PropertyToID("Conifer_Upscaler_LumaHistory");
            private static readonly int LumaHistorySinkID = Shader.PropertyToID("Conifer_Upscaler_LumaHistorySink");
            private static readonly int HistoryID = Shader.PropertyToID("Conifer_Upscaler_History");
            private static readonly int NextHistoryID = Shader.PropertyToID("Conifer_Upscaler_NextHistory");
            private static readonly int PreviousOutputID = Shader.PropertyToID("Conifer_Upscaler_PreviousOutput");
            private static readonly int CameraColorTextureID = Shader.PropertyToID("_CameraColorTexture");
            private static readonly int OutputSinkID = Shader.PropertyToID("Conifer_Upscaler_OutputSink");
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

            public Upscale() => renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing;

            public override void OnCameraSetup(CommandBuffer cmd, ref RenderingData renderingData)
            {
                var cameraTextureDescriptor = renderingData.cameraData.cameraTargetDescriptor;
                cameraTextureDescriptor.width = _upscaler.InputResolution.x;
                cameraTextureDescriptor.height = _upscaler.InputResolution.y;
                var descriptor = cameraTextureDescriptor;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                _renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _cameraRenderResolutionColorTarget, descriptor, name: "Conifer_CameraColorTarget");
                descriptor = cameraTextureDescriptor;
                descriptor.colorFormat = RenderTextureFormat.Depth;
                if (_upscaler.frameGeneration)
                {
                    descriptor.graphicsFormat = GraphicsFormat.D32_SFloat;
                    _renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _cameraRenderResolutionDepthTarget, descriptor, isShadowMap: true, name: "Conifer_CameraDepthTarget");
                }
                else
                {
                    _renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _cameraRenderResolutionDepthTarget, descriptor, name: "Conifer_CameraDepthTarget");
                }
                MultipurposeBlit(cmd, Texture2D.blackTexture, _cameraRenderResolutionDepthTarget, true);

                _cameraOutputResolutionColorTarget = renderingData.cameraData.renderer.cameraColorTargetHandle;
                _cameraOutputResolutionDepthTarget = renderingData.cameraData.renderer.cameraDepthTargetHandle;
                cmd.Blit(Texture2D.blackTexture, _cameraOutputResolutionColorTarget);
                renderingData.cameraData.renderer.ConfigureCameraTarget(_cameraRenderResolutionColorTarget, _cameraRenderResolutionDepthTarget);

                if (_upscaler.technique != Upscaler.Technique.SnapdragonGameSuperResolutionTemporal) return;
                descriptor = renderingData.cameraData.cameraTargetDescriptor;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                RenderingUtils.ReAllocateIfNeeded(ref _previousOutput, descriptor, name: "Conifer_Upscaler_HistoryBuffer");
                descriptor = cameraTextureDescriptor;
                descriptor.graphicsFormat = GraphicsFormat.R16G16B16A16_SFloat;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                RenderingUtils.ReAllocateIfNeeded(ref _motionDepthClipAlphaBuffer, descriptor, name: "Conifer_Upscaler_MotionDepthAlphaBuffer");
                if (_upscaler.sgsrMethod is not (Upscaler.SgsrMethod.Compute3Pass or Upscaler.SgsrMethod.Compute2Pass)) return;
                descriptor.enableRandomWrite = true;
                RenderingUtils.ReAllocateIfNeeded(ref _motionDepthClipAlphaBufferSink, descriptor, name: "Conifer_Upscaler_MotionDepthAlphaBufferSink");
                descriptor.graphicsFormat = GraphicsFormat.R32_UInt;
                RenderingUtils.ReAllocateIfNeeded(ref _lumaHistorySink, descriptor, name: "Conifer_Upscaler_LumaHistorySink");
                descriptor.enableRandomWrite = false;
                RenderingUtils.ReAllocateIfNeeded(ref _lumaHistory, descriptor, name: "Conifer_Upscaler_LumaHistory");
                descriptor = renderingData.cameraData.cameraTargetDescriptor;
                descriptor.graphicsFormat = GraphicsFormat.R16G16B16A16_SFloat;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                RenderingUtils.ReAllocateIfNeeded(ref _history, descriptor, name: "Conifer_Upscaler_History");
            }

            public override void Configure(CommandBuffer cmd, RenderTextureDescriptor descriptor)
            {
                if (_upscaler.IsSpatial()) return;
                descriptor.enableRandomWrite = true;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                var needsUpdate = _renderTargetsUpdated;
                needsUpdate |= RenderingUtils.ReAllocateIfNeeded(ref _output, descriptor, name: "Conifer_UpscalerOutput");

                if (_upscaler.technique == Upscaler.Technique.FidelityFXSuperResolution && _upscaler.useReactiveMask)
                {
                    descriptor.width = _upscaler.InputResolution.x;
                    descriptor.height = _upscaler.InputResolution.y;
                    descriptor.graphicsFormat = GraphicsFormat.R8_UNorm;
                    needsUpdate |= RenderingUtils.ReAllocateIfNeeded(ref _reactive, descriptor, name: "Conifer_UpscalerReactiveMask");
                }
                else
                {
                    _reactive?.Release();
                    _reactive = null;
                }

                if (Time.frameCount == 1) return;
                var thisMotion = Shader.GetGlobalTexture(MotionID);
                var thisOpaque = Shader.GetGlobalTexture(OpaqueID);
                if (needsUpdate || thisMotion != _lastMotion || thisOpaque != _lastOpaque || _upscaler.technique != _lastTechnique || _upscaler.useReactiveMask != _lastReactive)
                    _upscaler.NativeInterface.SetUpscalingImages(_cameraRenderResolutionColorTarget.rt.GetNativeTexturePtr(), _cameraRenderResolutionDepthTarget.rt.GetNativeTexturePtr(), thisMotion?.GetNativeTexturePtr() ?? IntPtr.Zero, _output.rt.GetNativeTexturePtr(), _reactive?.rt.GetNativeTexturePtr() ?? IntPtr.Zero, thisOpaque?.GetNativeTexturePtr() ?? IntPtr.Zero, _upscaler.useReactiveMask);
                _renderTargetsUpdated = false;
                _lastMotion = thisMotion;
                _lastOpaque = thisOpaque;
                _lastTechnique = _upscaler.technique;
                _lastReactive = _upscaler.useReactiveMask;
            }

            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var cb = CommandBufferPool.Get("Upscale");
                if (!_upscaler.DisableUpscaling && Time.frameCount != 1)
                {
                    switch (_upscaler.technique)
                    {
                        case Upscaler.Technique.SnapdragonGameSuperResolutionSpatial:
                            cb.SetRenderTarget(_cameraOutputResolutionColorTarget);
                            cb.SetGlobalTexture(BlitID, _cameraRenderResolutionColorTarget);
                            cb.SetProjectionMatrix(Ortho);
                            cb.SetViewMatrix(LookAt);
                            cb.SetGlobalVector(ViewportInfoID, new Vector4(1.0f / _upscaler.InputResolution.x, 1.0f / _upscaler.InputResolution.y, _upscaler.InputResolution.x, _upscaler.InputResolution.y));
                            cb.SetGlobalFloat(EdgeSharpnessID, _upscaler.sharpness);
                            cb.DrawMesh(_triangle, Matrix4x4.identity, SgsrMaterial, 0, 0);
                            break;
                        case Upscaler.Technique.SnapdragonGameSuperResolutionTemporal:
                            var current = renderingData.cameraData.camera.cameraToWorldMatrix * renderingData.cameraData.camera.projectionMatrix;
                            float vpDiff = 0;
                            for(var i = 0; i < 4; i++) for(var j = 0; j < 4; j++)
                                vpDiff += Math.Abs(current[i, j] - _previousProjectionMatrix[i, j]);
                            _previousProjectionMatrix = current;
                            var cameraIsSame = vpDiff < 1e-5;
                            cb.SetGlobalVector(RenderSizeID, (Vector2)_upscaler.InputResolution);
                            cb.SetGlobalVector(OutputSizeID, (Vector2)_upscaler.OutputResolution);
                            cb.SetGlobalVector(RenderSizeRcpID, Vector2.one / _upscaler.InputResolution);
                            cb.SetGlobalVector(OutputSizeRcpID, Vector2.one / _upscaler.OutputResolution);
                            cb.SetGlobalVector(JitterOffsetID, _upscaler.Jitter);
                            cb.SetGlobalVector(ScaleRatioID, new Vector4((float)_upscaler.OutputResolution.x / _upscaler.InputResolution.x, Mathf.Min(20.0f, Mathf.Pow((float)_upscaler.OutputResolution.x * _upscaler.OutputResolution.y / (_upscaler.InputResolution.x * _upscaler.InputResolution.y), 3.0f))));
                            cb.SetGlobalFloat(PreExposureID, 1.0f);
                            cb.SetGlobalFloat(CameraFovAngleHorID, Mathf.Tan(Mathf.Deg2Rad * (renderingData.cameraData.camera.fieldOfView / 2)) * _upscaler.InputResolution.x / _upscaler.InputResolution.y);
                            cb.SetGlobalFloat(MinLerpContributionID, cameraIsSame ? 0.3f : 0.0f);
                            cb.SetGlobalFloat(ResetID, Convert.ToSingle(_upscaler.NativeInterface.ShouldResetHistory));
                            cb.SetGlobalInt(SameCameraID, cameraIsSame ? 1 : 0);
                            cb.SetGlobalVector(RenderSizeID, (Vector2)_upscaler.InputResolution);
                            switch (_upscaler.sgsrMethod)
                            {
                                case Upscaler.SgsrMethod.Compute3Pass:
                                {
                                    cb.SetGlobalTexture(MotionDepthAlphaBufferID, _motionDepthClipAlphaBuffer);
                                    cb.SetGlobalTexture(MotionDepthAlphaBufferSinkID, _motionDepthClipAlphaBufferSink);
                                    cb.SetGlobalTexture(OpaqueID, _cameraRenderResolutionColorTarget);
                                    cb.SetGlobalTexture(CameraColorTextureID, _cameraRenderResolutionColorTarget);
                                    cb.GetTemporaryRT(LumaSinkID, _lumaHistorySink.rt.descriptor);
                                    cb.SetGlobalTexture(LumaHistoryID, _lumaHistory);
                                    cb.SetGlobalTexture(LumaHistorySinkID, _lumaHistorySink);
                                    var nextHistoryDescriptor = _history.rt.descriptor;
                                    nextHistoryDescriptor.enableRandomWrite = true;
                                    cb.GetTemporaryRT(NextHistoryID, nextHistoryDescriptor);
                                    cb.SetGlobalTexture(HistoryID, _history);
                                    cb.SetGlobalTexture(OutputSinkID, _output);
                                    var threadGroups = Vector3Int.CeilToInt(new Vector3(_upscaler.InputResolution.x, _upscaler.InputResolution.y, 1) / 8);
                                    cb.DispatchCompute(Sgsr3PassConvert, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                    cb.GetTemporaryRT(LumaID, _lumaHistory.rt.descriptor);
                                    cb.CopyTexture(LumaSinkID, LumaID);
                                    cb.ReleaseTemporaryRT(LumaSinkID);
                                    cb.CopyTexture(_motionDepthClipAlphaBufferSink, _motionDepthClipAlphaBuffer);
                                    cb.DispatchCompute(Sgsr3PassActivate, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                    cb.CopyTexture(_lumaHistorySink, _lumaHistory);
                                    cb.CopyTexture(_motionDepthClipAlphaBufferSink, _motionDepthClipAlphaBuffer);
                                    threadGroups = Vector3Int.CeilToInt(new Vector3(_upscaler.OutputResolution.x, _upscaler.OutputResolution.y, 1) / 8);
                                    cb.DispatchCompute(Sgsr3PassUpscale, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                    cb.CopyTexture(LumaID, _lumaHistory);
                                    cb.ReleaseTemporaryRT(LumaID);
                                    cb.CopyTexture(_output, _cameraOutputResolutionColorTarget);
                                    cb.CopyTexture(NextHistoryID, _history);
                                    break;
                                }
                                case Upscaler.SgsrMethod.Compute2Pass:
                                {
                                    cb.SetGlobalTexture(MotionDepthAlphaBufferID, _motionDepthClipAlphaBuffer);
                                    cb.SetGlobalTexture(MotionDepthAlphaBufferSinkID, _motionDepthClipAlphaBufferSink);
                                    cb.SetGlobalTexture(OpaqueID, _cameraRenderResolutionColorTarget);
                                    cb.SetGlobalTexture(CameraColorTextureID, _cameraRenderResolutionColorTarget);
                                    cb.GetTemporaryRT(LumaSinkID, _lumaHistorySink.rt.descriptor);
                                    var nextHistoryDescriptor = _history.rt.descriptor;
                                    nextHistoryDescriptor.enableRandomWrite = true;
                                    cb.GetTemporaryRT(NextHistoryID, nextHistoryDescriptor);
                                    cb.SetGlobalTexture(HistoryID, _history);
                                    cb.SetGlobalTexture(OutputSinkID, _output);
                                    var threadGroups = Vector3Int.CeilToInt(new Vector3(_upscaler.InputResolution.x, _upscaler.InputResolution.y, 1) / 8);
                                    cb.DispatchCompute(Sgsr2PassConvert, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                    threadGroups = Vector3Int.CeilToInt(new Vector3(_upscaler.OutputResolution.x, _upscaler.OutputResolution.y, 1) / 8);
                                    cb.GetTemporaryRT(LumaID, _lumaHistory.rt.descriptor);
                                    cb.CopyTexture(LumaSinkID, LumaID);
                                    cb.ReleaseTemporaryRT(LumaSinkID);
                                    cb.CopyTexture(_motionDepthClipAlphaBufferSink, _motionDepthClipAlphaBuffer);
                                    cb.DispatchCompute(Sgsr2PassUpscale, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                    cb.ReleaseTemporaryRT(LumaID);
                                    cb.CopyTexture(_output, _cameraOutputResolutionColorTarget);
                                    cb.CopyTexture(NextHistoryID, _history);
                                    break;
                                }
                                case Upscaler.SgsrMethod.Fragment2Pass:
                                {
                                    cb.SetProjectionMatrix(Ortho);
                                    cb.SetViewMatrix(LookAt);
                                    cb.SetRenderTarget(_motionDepthClipAlphaBuffer);
                                    cb.DrawMesh(_triangle, Matrix4x4.identity, SgsrConvertMaterial);
                                    cb.SetGlobalTexture(MotionDepthAlphaBufferID, _motionDepthClipAlphaBuffer);
                                    cb.SetGlobalTexture(PreviousOutputID, _previousOutput);
                                    cb.SetGlobalTexture(BlitID, _cameraRenderResolutionColorTarget);
                                    cb.SetRenderTarget(_cameraOutputResolutionColorTarget);
                                    cb.DrawMesh(_triangle, Matrix4x4.identity, SgsrUpscaleMaterial);
                                    cb.CopyTexture(_cameraOutputResolutionColorTarget, _previousOutput);
                                    break;
                                }
                                default:
                                    cb.Blit(_cameraRenderResolutionColorTarget, _cameraOutputResolutionColorTarget);
                                    break;
                            }
                            break;
                        case Upscaler.Technique.DeepLearningSuperSampling:
                        case Upscaler.Technique.FidelityFXSuperResolution:
                        case Upscaler.Technique.XeSuperSampling:
                            _upscaler.NativeInterface.Upscale(cb, _upscaler);
                            cb.CopyTexture(_output, _cameraOutputResolutionColorTarget);
                            break;
                        case Upscaler.Technique.None:
                        default:
                            cb.Blit(_cameraRenderResolutionColorTarget, _cameraOutputResolutionColorTarget);
                            break;
                    }
                }
                else
                {
                    cb.Blit(_cameraRenderResolutionColorTarget, _cameraOutputResolutionColorTarget);
                }
                MultipurposeBlit(cb, _cameraRenderResolutionDepthTarget, _cameraOutputResolutionDepthTarget, true);
                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);
                renderingData.cameraData.renderer.ConfigureCameraTarget(_cameraOutputResolutionColorTarget, _cameraOutputResolutionDepthTarget);
                _upscaler.ShouldResetHistory(false);
            }

            public static void FreeMemory()
            {
                _output?.Release();
                _output = null;
                _reactive?.Release();
                _reactive = null;
                _motionDepthClipAlphaBuffer?.Release();
                _motionDepthClipAlphaBuffer = null;
                _motionDepthClipAlphaBufferSink?.Release();
                _motionDepthClipAlphaBufferSink = null;
                _previousOutput?.Release();
                _previousOutput = null;
                _lumaHistory?.Release();
                _lumaHistory = null;
                _lumaHistorySink?.Release();
                _lumaHistorySink = null;
                _history?.Release();
                _history = null;
            }
        }

        private class FrameGenerate : ScriptableRenderPass
        {
            private static readonly RTHandle[] Hudless = new RTHandle[2];
            private static uint _hudlessBufferIndex;
            private static RTHandle _flippedDepth;
            private static RTHandle _flippedMotion;
            private static readonly Vector2Int Offset = new(1, 1);
            private static readonly Vector2Int ExtraResolution = new(2, 42);

            public FrameGenerate() => renderPassEvent = (RenderPassEvent)int.MaxValue;

            public override void OnCameraSetup(CommandBuffer cmd, ref RenderingData renderingData)
            {
                var descriptor = renderingData.cameraData.cameraTargetDescriptor;
                descriptor.colorFormat = GraphicsFormatUtility.GetRenderTextureFormat(NativeInterface.GetBackBufferFormat());
                descriptor.depthStencilFormat = GraphicsFormat.None;
                descriptor.width += ExtraResolution.x;
                descriptor.height += ExtraResolution.y;
                var needsUpdate = _renderTargetsUpdated;
                needsUpdate |= RenderingUtils.ReAllocateIfNeeded(ref Hudless[0], descriptor, name: "Conifer_FrameGenHUDLessTarget0");
                needsUpdate |= RenderingUtils.ReAllocateIfNeeded(ref Hudless[1], descriptor, name: "Conifer_FrameGenHUDLessTarget1");
                descriptor = _cameraOutputResolutionColorTarget.rt.descriptor;
                descriptor.width += ExtraResolution.x;
                descriptor.height += ExtraResolution.y;
                descriptor.graphicsFormat = GraphicsFormat.R16G16_SFloat;
                needsUpdate |= RenderingUtils.ReAllocateIfNeeded(ref _flippedMotion, descriptor, name: "Conifer_FrameGenFlippedMotionVectorTarget");
                descriptor = _cameraOutputResolutionDepthTarget.rt.descriptor;
                descriptor.width += ExtraResolution.x;
                descriptor.height += ExtraResolution.y;
                descriptor.graphicsFormat = GraphicsFormat.D32_SFloat;
                needsUpdate |= RenderingUtils.ReAllocateIfNeeded(ref _flippedDepth, descriptor, isShadowMap: true, name: "Conifer_FrameGenFlippedDepthTarget");

                if (!needsUpdate) return;
                NativeInterface.SetFrameGenerationImages(Hudless[0].rt.GetNativeTexturePtr(), Hudless[1].rt.GetNativeTexturePtr(), _flippedDepth.rt.GetNativeTexturePtr(), _flippedMotion.rt.GetNativeTexturePtr());
            }

            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var cb = CommandBufferPool.Get("Frame Generate");
                var srcRes = new Vector2(renderingData.cameraData.cameraTargetDescriptor.width, renderingData.cameraData.cameraTargetDescriptor.height);
                var dstRes = srcRes + ExtraResolution;
                cb.Blit(null, _cameraOutputResolutionColorTarget);
                cb.CopyTexture(_cameraOutputResolutionColorTarget, 0, 0, 0, 0, renderingData.cameraData.cameraTargetDescriptor.width, renderingData.cameraData.cameraTargetDescriptor.height, Hudless[_hudlessBufferIndex], 0, 0, 1, 40);
                MultipurposeBlit(cb, Shader.GetGlobalTexture(MotionID), _flippedMotion, false, true, Offset, srcRes, dstRes);
                MultipurposeBlit(cb, _cameraOutputResolutionDepthTarget, _flippedDepth, true, true, Offset, srcRes, dstRes);
                _upscaler.NativeInterface.FrameGenerate(cb, _upscaler, _hudlessBufferIndex);
                cb.SetRenderTarget(k_CameraTarget);cb.SetRenderTarget(k_CameraTarget);
                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);
                _hudlessBufferIndex = (_hudlessBufferIndex + 1U) % (uint)Hudless.Length;
            }

            public static void FreeMemory()
            {
                Hudless[0]?.Release();
                Hudless[0] = null;
                Hudless[1]?.Release();
                Hudless[1] = null;
                _flippedDepth?.Release();
                _flippedDepth = null;
                _flippedMotion?.Release();
                _flippedMotion = null;
            }
        }

        private readonly SetMipBias _setMipBias = new();
        private readonly Upscale _upscale = new();
        private readonly FrameGenerate _frameGenerate = new();

        public override void Create()
        {
            name = "Upscaler";
            _triangle = new Mesh
            {
                vertices = new Vector3[] { new(-1, -1, 0), new(3, -1, 0), new(-1, 3, 0) },
                uv = new Vector2[] { new(0, 1), new(2, 1), new(0, -1) },
                triangles = new[] { 0, 1, 2 }
            };
            _upsideDownTriangle = new Mesh {
                vertices = _triangle.vertices,
                uv = new Vector2[] { new(0, 0), new(2, 0), new(0, 2) },
                triangles = _triangle.triangles
            };

            _depthBlitMaterial = new Material(Shader.Find("Hidden/BlitDepth"));
            _blitMaterial = new Material(Shader.Find("Hidden/BlitColor"));
            Upscale.SgsrMaterial = new Material(Resources.Load<Shader>("SnapdragonGameSuperResolution/v1/Upscale"));
            Upscale.SgsrConvertMaterial = new Material(Resources.Load<Shader>("SnapdragonGameSuperResolution/v2/F2/Convert"));
            Upscale.SgsrUpscaleMaterial = new Material(Resources.Load<Shader>("SnapdragonGameSuperResolution/v2/F2/Upscale"));
            Upscale.Sgsr2PassConvert = Resources.Load<ComputeShader>("SnapdragonGameSuperResolution/v2/C2/Convert");
            Upscale.Sgsr2PassUpscale = Resources.Load<ComputeShader>("SnapdragonGameSuperResolution/v2/C2/Upscale");
            Upscale.Sgsr3PassConvert = Resources.Load<ComputeShader>("SnapdragonGameSuperResolution/v2/C3/Convert");
            Upscale.Sgsr3PassActivate = Resources.Load<ComputeShader>("SnapdragonGameSuperResolution/v2/C3/Activate");
            Upscale.Sgsr3PassUpscale = Resources.Load<ComputeShader>("SnapdragonGameSuperResolution/v2/C3/Upscale");
        }

        public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
        {
            _upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
            if (!Application.isPlaying || renderingData.cameraData.cameraType != CameraType.Game || _upscaler is null ||
                !_upscaler.isActiveAndEnabled || _upscaler.technique == Upscaler.Technique.None)
            {
                _cameraRenderResolutionColorTarget?.Release();
                _cameraRenderResolutionColorTarget = null;
                _cameraRenderResolutionDepthTarget?.Release();
                _cameraRenderResolutionDepthTarget = null;
                Upscale.FreeMemory();
                FrameGenerate.FreeMemory();
                return;
            }

            if (renderingData.cameraData.camera.GetUniversalAdditionalCameraData().resetHistory) _upscaler.ResetHistory();

            _setMipBias.ConfigureInput(ScriptableRenderPassInput.None);
            _upscale.ConfigureInput(ScriptableRenderPassInput.Motion);
            if (_upscaler.IsTemporal()) renderer.EnqueuePass(_setMipBias);
            renderer.EnqueuePass(_upscale);
            if (NativeInterface.GetBackBufferFormat() != GraphicsFormat.None)
                renderer.EnqueuePass(_frameGenerate);
            _renderTargetsUpdated = false;
        }
    }
}