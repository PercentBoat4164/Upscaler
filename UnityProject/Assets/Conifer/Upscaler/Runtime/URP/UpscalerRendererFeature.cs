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
#if UNITY_EDITOR
using UnityEditor;
#endif

namespace Conifer.Upscaler.URP
{
    [DisallowMultipleRendererFeature("Upscaler Renderer Feature")]
    public class UpscalerRendererFeature : ScriptableRendererFeature
    {
        private static Mesh _triangle;
        private static Material _depthBlitMaterial;
        private static readonly int OffsetID = Shader.PropertyToID("Conifer_Upscaler_Offset");
        private static readonly int ScaleID = Shader.PropertyToID("Conifer_Upscaler_Scale");
        
        private static readonly int BlitID = Shader.PropertyToID("_MainTex");
        private static readonly int MotionID = Shader.PropertyToID("_MotionVectorTexture");
        private static readonly int OpaqueID = Shader.PropertyToID("_CameraOpaqueTexture");
        private static readonly int ColorID = Shader.PropertyToID("_CameraColorTexture");
        private static readonly int DepthID = Shader.PropertyToID("_CameraDepthTexture");

        private static readonly Matrix4x4 Ortho = Matrix4x4.Ortho(-1, 1, 1, -1, 1, -1);
        private static readonly Matrix4x4 LookAt = Matrix4x4.LookAt(Vector3.back, Vector3.forward, Vector3.up);

        /**@todo: Remove `static` keywords for all variables below this comment.*/
        
        private static RTHandle _cameraRenderResolutionColorTarget;
        private static RTHandle _cameraRenderResolutionDepthTarget;
        private static RTHandle _cameraOutputResolutionColorTarget;
        private static RTHandle _cameraOutputResolutionDepthTarget;
        
        private static void BlitDepth(CommandBuffer cb, RenderTargetIdentifier src, RenderTargetIdentifier dst, Vector2? scale = null, Vector2? offset = null)
        {
            cb.SetProjectionMatrix(Ortho);
            cb.SetViewMatrix(LookAt);
            cb.SetRenderTarget(dst);
            cb.SetGlobalTexture(BlitID, src);
            cb.SetGlobalVector(OffsetID, offset ?? Vector2.zero);
            cb.SetGlobalVector(ScaleID, scale ?? Vector2.one);
            cb.DrawMesh(_triangle, Matrix4x4.identity, _depthBlitMaterial);
        }
        
        private static void Manage(ref ComputeShader computeShader, string path, bool shouldBeLoaded) 
        {
            if (computeShader is null && shouldBeLoaded) computeShader = Resources.Load<ComputeShader>(path);
            else if (computeShader is not null && !shouldBeLoaded)
            {
                Resources.UnloadAsset(computeShader);
                computeShader = null;
            }
        }
            
        private static void ManageMaterial(ref Material material, string path, bool shouldBeLoaded)
        {
            if (material is null && shouldBeLoaded) material = new Material(Resources.Load<Shader>(path));
            else if (material is not null && !shouldBeLoaded)
            {
                Resources.UnloadAsset(material.shader);
#if UNITY_EDITOR
                if (EditorApplication.isPlaying) Destroy(material);
                else DestroyImmediate(material, true);
#else
                Destroy(material);
#endif
                material = null;
            }
        }

        private class SetMipBias : ScriptableRenderPass
        {
            private static readonly int GlobalMipBias = Shader.PropertyToID("_GlobalMipBias");
            private int _jitterIndex;

            public SetMipBias() => renderPassEvent = RenderPassEvent.BeforeRenderingPrePasses;

            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
                var cb = CommandBufferPool.Get("SetMipBias");
                var mipBias = (float)Math.Log((float)upscaler.InputResolution.x / upscaler.OutputResolution.x, 2f) - 1f;
                cb.SetGlobalVector(GlobalMipBias, new Vector4(mipBias, mipBias * mipBias));
                upscaler.Jitter = new Vector2(HaltonSequence.Get(_jitterIndex, 2), HaltonSequence.Get(_jitterIndex, 3));
                _jitterIndex = (_jitterIndex + 1) % (int)Math.Ceiling(7 * Math.Pow((float)upscaler.OutputResolution.x / upscaler.InputResolution.x, 2));
                var clipSpaceJitter = -upscaler.Jitter / upscaler.InputResolution * 2;
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
            private static Matrix4x4 _previousProjectionMatrix;

            internal readonly RTHandle[] Inputs = new RTHandle[6];
            internal static readonly Index Output = 0;
            internal static readonly Index Reactive = 1;
            internal static readonly Index LumaHistory = 2;
            internal static readonly Index History = 3;
            internal static readonly Index Motion = 4;
            internal static readonly Index Opaque = 5;

            internal static Material SgsrMaterial;
            internal static Material SgsrConvertMaterial;
            internal static Material SgsrUpscaleMaterial;
            internal static ComputeShader Sgsr2PassConvert;
            internal static ComputeShader Sgsr2PassUpscale;
            internal static ComputeShader Sgsr3PassConvert;
            internal static ComputeShader Sgsr3PassActivate;
            internal static ComputeShader Sgsr3PassUpscale;

            private static readonly int ViewportInfoID = Shader.PropertyToID("Conifer_Upscaler_ViewportInfo");
            private static readonly int EdgeSharpnessID = Shader.PropertyToID("Conifer_Upscaler_EdgeSharpness");
            private static readonly int MotionDepthAlphaBufferID = Shader.PropertyToID("Conifer_Upscaler_MotionDepthAlphaBuffer");
            private static readonly int MotionDepthAlphaBufferSinkID = Shader.PropertyToID("Conifer_Upscaler_MotionDepthAlphaBufferSink");
            private static readonly int LumaID = Shader.PropertyToID("Conifer_Upscaler_Luma");
            private static readonly int LumaSinkID = Shader.PropertyToID("Conifer_Upscaler_LumaSink");
            private static readonly int LumaHistoryID = Shader.PropertyToID("Conifer_Upscaler_LumaHistory");
            private static readonly int HistoryID = Shader.PropertyToID("Conifer_Upscaler_History");
            private static readonly int NextHistoryID = Shader.PropertyToID("Conifer_Upscaler_NextHistory");
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
                _cameraOutputResolutionColorTarget = renderingData.cameraData.renderer.cameraColorTargetHandle;
                _cameraOutputResolutionDepthTarget = renderingData.cameraData.renderer.cameraDepthTargetHandle;
                cmd.SetRenderTarget(_cameraRenderResolutionColorTarget, _cameraRenderResolutionDepthTarget);
                cmd.ClearRenderTarget(RTClearFlags.All, Color.black, 0.0f, 0U);
                renderingData.cameraData.renderer.ConfigureCameraTarget(_cameraRenderResolutionColorTarget, _cameraRenderResolutionDepthTarget);
            }

            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
                var cb = CommandBufferPool.Get("Upscale");
                switch (upscaler.technique)
                {
                    case Upscaler.Technique.SnapdragonGameSuperResolution1:
                        cb.SetRenderTarget(_cameraOutputResolutionColorTarget);
                        cb.SetGlobalTexture(BlitID, _cameraRenderResolutionColorTarget);
                        cb.SetProjectionMatrix(Ortho);
                        cb.SetViewMatrix(LookAt);
                        cb.SetGlobalVector(ViewportInfoID, new Vector4(1.0f / upscaler.InputResolution.x, 1.0f / upscaler.InputResolution.y, upscaler.InputResolution.x, upscaler.InputResolution.y));
                        cb.SetGlobalFloat(EdgeSharpnessID, upscaler.sharpness + 1);
                        cb.DrawMesh(_triangle, Matrix4x4.identity, SgsrMaterial, 0, 0);
                        break;
                    case Upscaler.Technique.SnapdragonGameSuperResolution2:
                        var current = renderingData.cameraData.camera.cameraToWorldMatrix * renderingData.cameraData.camera.projectionMatrix;
                        float vpDiff = 0;
                        for (var i = 0; i < 4; i++) for (var j = 0; j < 4; j++) vpDiff += Math.Abs(current[i, j] - _previousProjectionMatrix[i, j]);
                        _previousProjectionMatrix = current;
                        var cameraIsSame = vpDiff < 1e-5;
                        cb.SetGlobalVector(RenderSizeID, (Vector2)upscaler.InputResolution);
                        cb.SetGlobalVector(OutputSizeID, (Vector2)upscaler.OutputResolution);
                        cb.SetGlobalVector(RenderSizeRcpID, Vector2.one / upscaler.InputResolution);
                        cb.SetGlobalVector(OutputSizeRcpID, Vector2.one / upscaler.OutputResolution);
                        cb.SetGlobalVector(JitterOffsetID, upscaler.Jitter);
                        cb.SetGlobalVector(ScaleRatioID, new Vector4((float)upscaler.OutputResolution.x / upscaler.InputResolution.x, Mathf.Min(20.0f, Mathf.Pow((float)upscaler.OutputResolution.x * upscaler.OutputResolution.y / (upscaler.InputResolution.x * upscaler.InputResolution.y), 3.0f))));
                        cb.SetGlobalFloat(PreExposureID, 1.0f);
                        cb.SetGlobalFloat(CameraFovAngleHorID, Mathf.Tan(Mathf.Deg2Rad * (renderingData.cameraData.camera.fieldOfView / 2)) * upscaler.InputResolution.x / upscaler.InputResolution.y);
                        cb.SetGlobalFloat(MinLerpContributionID, cameraIsSame ? 0.3f : 0.0f);
                        cb.SetGlobalFloat(ResetID, Convert.ToSingle(upscaler.NativeInterface.ShouldResetHistory));
                        cb.SetGlobalInt(SameCameraID, cameraIsSame ? 1 : 0);
                        cb.SetGlobalVector(RenderSizeID, (Vector2)upscaler.InputResolution);
                        var threadGroups = Vector3Int.CeilToInt(new Vector3(upscaler.InputResolution.x, upscaler.InputResolution.y, 1) / 8);
                        var cameraTextureDescriptor = renderingData.cameraData.cameraTargetDescriptor;
                        cameraTextureDescriptor.width = upscaler.InputResolution.x;
                        cameraTextureDescriptor.height = upscaler.InputResolution.y;
                        var descriptor = Inputs[History].rt.descriptor;
                        descriptor.enableRandomWrite = true;
                        cb.GetTemporaryRT(NextHistoryID, descriptor);
                        descriptor = cameraTextureDescriptor;
                        descriptor.graphicsFormat = GraphicsFormat.R16G16B16A16_SFloat;
                        descriptor.depthStencilFormat = GraphicsFormat.None;
                        cb.GetTemporaryRT(MotionDepthAlphaBufferID, descriptor);
                        switch (upscaler.sgsrMethod)
                        {
                            case Upscaler.SgsrMethod.Compute3Pass:
                            {
                                descriptor.enableRandomWrite = true;
                                cb.GetTemporaryRT(MotionDepthAlphaBufferSinkID, descriptor);
                                descriptor = Inputs[LumaHistory].rt.descriptor;
                                cb.GetTemporaryRT(LumaID, descriptor);
                                descriptor.enableRandomWrite = true;
                                cb.GetTemporaryRT(LumaSinkID, descriptor);
                                cb.SetGlobalTexture(ColorID, _cameraRenderResolutionColorTarget);
                                cb.SetGlobalTexture(LumaHistoryID, Inputs[LumaHistory]);
                                cb.SetGlobalTexture(HistoryID, Inputs[History]);
                                cb.SetGlobalTexture(OutputSinkID, Inputs[Output]);
                                cb.DispatchCompute(Sgsr3PassConvert, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                cb.CopyTexture(LumaSinkID, LumaID);
                                cb.CopyTexture(MotionDepthAlphaBufferSinkID, MotionDepthAlphaBufferID);
                                cb.DispatchCompute(Sgsr3PassActivate, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                cb.CopyTexture(LumaSinkID, Inputs[LumaHistory]);
                                cb.CopyTexture(MotionDepthAlphaBufferSinkID, MotionDepthAlphaBufferID);
                                threadGroups = Vector3Int.CeilToInt(new Vector3(upscaler.OutputResolution.x, upscaler.OutputResolution.y, 1) / 8);
                                cb.DispatchCompute(Sgsr3PassUpscale, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                cb.CopyTexture(LumaID, Inputs[LumaHistory]);
                                cb.CopyTexture(Inputs[Output], _cameraOutputResolutionColorTarget);
                                cb.CopyTexture(NextHistoryID, Inputs[History]);
                                cb.ReleaseTemporaryRT(MotionDepthAlphaBufferSinkID);
                                cb.ReleaseTemporaryRT(LumaID);
                                cb.ReleaseTemporaryRT(LumaSinkID);
                                break;
                            }
                            case Upscaler.SgsrMethod.Compute2Pass:
                            {
                                descriptor.enableRandomWrite = true;
                                cb.GetTemporaryRT(MotionDepthAlphaBufferSinkID, descriptor);
                                descriptor = Inputs[LumaHistory].rt.descriptor;
                                cb.GetTemporaryRT(LumaID, descriptor);
                                descriptor.enableRandomWrite = true;
                                cb.GetTemporaryRT(LumaSinkID, descriptor);
                                cb.SetGlobalTexture(ColorID, _cameraRenderResolutionColorTarget);
                                cb.SetGlobalTexture(HistoryID, Inputs[History]);
                                cb.SetGlobalTexture(OutputSinkID, Inputs[Output]);
                                cb.DispatchCompute(Sgsr2PassConvert, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                threadGroups = Vector3Int.CeilToInt(new Vector3(upscaler.OutputResolution.x, upscaler.OutputResolution.y, 1) / 8);
                                cb.CopyTexture(LumaSinkID, LumaID);
                                cb.CopyTexture(MotionDepthAlphaBufferSinkID, MotionDepthAlphaBufferID);
                                cb.DispatchCompute(Sgsr2PassUpscale, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                cb.CopyTexture(Inputs[Output], _cameraOutputResolutionColorTarget);
                                cb.CopyTexture(NextHistoryID, Inputs[History]);
                                cb.ReleaseTemporaryRT(MotionDepthAlphaBufferSinkID);
                                cb.ReleaseTemporaryRT(LumaID);
                                cb.ReleaseTemporaryRT(LumaSinkID);
                                break;
                            }
                            case Upscaler.SgsrMethod.Fragment2Pass:
                            {
                                cb.SetProjectionMatrix(Ortho);
                                cb.SetViewMatrix(LookAt);
                                cb.SetRenderTarget(MotionDepthAlphaBufferID);
                                cb.DrawMesh(_triangle, Matrix4x4.identity, SgsrConvertMaterial);
                                cb.SetGlobalTexture(HistoryID, Inputs[History]);
                                cb.SetGlobalTexture(BlitID, _cameraRenderResolutionColorTarget);
                                cb.SetRenderTarget(Inputs[History]);
                                cb.DrawMesh(_triangle, Matrix4x4.identity, SgsrUpscaleMaterial);
                                cb.CopyTexture(Inputs[History], _cameraOutputResolutionColorTarget);
                                break;
                            }
                            default: throw new NotImplementedException();
                        }

                        cb.ReleaseTemporaryRT(NextHistoryID);
                        cb.ReleaseTemporaryRT(MotionDepthAlphaBufferID);
                        break;
                    case Upscaler.Technique.FidelityFXSuperResolution:
                        if (upscaler.autoReactive) cb.Blit(Shader.GetGlobalTexture(OpaqueID) ?? Texture2D.blackTexture, Inputs[Opaque]);
                        cb.Blit(Shader.GetGlobalTexture(MotionID) ?? Texture2D.blackTexture, Inputs[Motion]);
                        upscaler.NativeInterface.Upscale(cb, upscaler);
                        cb.CopyTexture(Inputs[Output], _cameraOutputResolutionColorTarget);
                        break;
                    case Upscaler.Technique.DeepLearningSuperSampling:
                    case Upscaler.Technique.XeSuperSampling:
                        cb.Blit(Shader.GetGlobalTexture(MotionID) ?? Texture2D.blackTexture, Inputs[Motion]);
                        upscaler.NativeInterface.Upscale(cb, upscaler);
                        cb.CopyTexture(Inputs[Output], _cameraOutputResolutionColorTarget);
                        break;
                    case Upscaler.Technique.None:
                    default: throw new NotImplementedException();
                }
                BlitDepth(cb, _cameraRenderResolutionDepthTarget, _cameraOutputResolutionDepthTarget);
                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);
                renderingData.cameraData.renderer.ConfigureCameraTarget(_cameraOutputResolutionColorTarget, _cameraOutputResolutionDepthTarget);
            }
            
            public void Dispose()
            {
                for (var i = 0; i < Inputs.Length; ++i)
                {
                    Inputs[i]?.Release();
                    Inputs[i] = null;
                }
            }
        }

        private class Generate : ScriptableRenderPass
        {
            private readonly RTHandle[] _hudless = new RTHandle[2];
            private uint _hudlessBufferIndex;
            private RTHandle _flippedDepth;
            private RTHandle _flippedMotion;

            public Generate() => renderPassEvent = (RenderPassEvent)int.MaxValue;

            public override void OnCameraSetup(CommandBuffer cmd, ref RenderingData renderingData)
            {
                var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
                var descriptor = renderingData.cameraData.cameraTargetDescriptor;
                descriptor.colorFormat = GraphicsFormatUtility.GetRenderTextureFormat(NativeInterface.GetBackBufferFormat());
                descriptor.depthStencilFormat = GraphicsFormat.None;
#if UNITY_EDITOR
                descriptor.width = (int)upscaler.NativeInterface.EditorResolution.x;
                descriptor.height = (int)upscaler.NativeInterface.EditorResolution.y;
#endif
                var needsUpdate = RenderingUtils.ReAllocateIfNeeded(ref _hudless[0], descriptor, name: "Conifer_Upscaler_HUDLess0");
                needsUpdate |= RenderingUtils.ReAllocateIfNeeded(ref _hudless[1], descriptor, name: "Conifer_Upscaler_HUDLess1");
                descriptor = renderingData.cameraData.cameraTargetDescriptor;
                descriptor.graphicsFormat = GraphicsFormat.R16G16_SFloat;
                needsUpdate |= RenderingUtils.ReAllocateIfNeeded(ref _flippedMotion, descriptor, name: "Conifer_Upscaler_FlippedMotion");
                descriptor = renderingData.cameraData.cameraTargetDescriptor;
                descriptor.width = upscaler.InputResolution.x;
                descriptor.height = upscaler.InputResolution.y;
                descriptor.colorFormat = RenderTextureFormat.Depth;
                needsUpdate |= RenderingUtils.ReAllocateIfNeeded(ref _flippedDepth, descriptor, isShadowMap: true, name: "Conifer_Upscaler_FlippedDepth");

                if (!needsUpdate) return;
                NativeInterface.SetFrameGenerationImages(_hudless[0], _hudless[1], _flippedDepth, _flippedMotion);
            }

            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
                var cb = CommandBufferPool.Get("Generate");
                var srcRes = new Vector2(renderingData.cameraData.cameraTargetDescriptor.width, renderingData.cameraData.cameraTargetDescriptor.height);
                cb.Blit(null, _cameraOutputResolutionColorTarget);
                cb.Blit(_cameraOutputResolutionColorTarget, _hudless[_hudlessBufferIndex]
#if UNITY_EDITOR
                    , upscaler.NativeInterface.EditorResolution / srcRes, -upscaler.NativeInterface.EditorOffset / srcRes
#endif
                );
                cb.Blit(Shader.GetGlobalTexture(MotionID), _flippedMotion, new Vector2(1.0f, -1.0f), new Vector2(0.0f, 1.0f));
                BlitDepth(cb, Shader.GetGlobalTexture(DepthID), _flippedDepth, new Vector2(1.0f, -1.0f), new Vector2(0.0f, 1.0f));
                upscaler.NativeInterface.FrameGenerate(cb, upscaler, _hudlessBufferIndex);
                cb.SetRenderTarget(k_CameraTarget);
                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);
                _hudlessBufferIndex = (_hudlessBufferIndex + 1U) % (uint)_hudless.Length;
                upscaler.ShouldResetHistory(false);
            }

            public void Dispose()
            {
                for (var i = 0; i < _hudless.Length; ++i)
                {
                    _hudless[i]?.Release();
                    _hudless[i] = null;
                }
                _flippedDepth?.Release();
                _flippedDepth = null;
                _flippedMotion?.Release();
                _flippedMotion = null;
            }
        }

        private class ResetHistory : ScriptableRenderPass
        {
            public ResetHistory() => renderPassEvent = (RenderPassEvent)int.MaxValue;
            
            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData) => renderingData.cameraData.camera.GetComponent<Upscaler>().ShouldResetHistory(false);
        }

        private readonly SetMipBias _setMipBias = new();
        private readonly Upscale _upscale = new();
        private readonly Generate _generate = new();
        private static readonly ResetHistory _resetHistory = new();
        
        public override void Create()
        {
            name = "Upscaler";
            _triangle = new Mesh
            {
                vertices = new Vector3[] { new(-1, -1, 0), new(3, -1, 0), new(-1, 3, 0) },
                uv = new Vector2[] { new(0, 1), new(2, 1), new(0, -1) },
                triangles = new[] { 0, 1, 2 }
            };

            _depthBlitMaterial = new Material(Shader.Find("Hidden/BlitDepth"));
        }

        private Upscaler.Technique _lastTechnique;
        private bool _lastAutoReactive;

        public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
        {
            var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
            if (!Application.isPlaying || renderingData.cameraData.cameraType != CameraType.Game || upscaler is null || !upscaler.isActiveAndEnabled)
            {
                _cameraRenderResolutionColorTarget?.Release();
                _cameraRenderResolutionColorTarget = null;
                _cameraRenderResolutionDepthTarget?.Release();
                _cameraRenderResolutionDepthTarget = null;
                return;
            }

            if (upscaler.forceHistoryResetEveryFrame || renderingData.cameraData.camera.GetUniversalAdditionalCameraData().resetHistory) upscaler.ResetHistory();
            var isResizingThisFrame = upscaler.OutputResolution != upscaler.LastOutputResolution;
            var previousFrameGeneration = upscaler.PreviousFrameGeneration;
            upscaler.CurrentStatus = upscaler.ApplySettings();
            if (Upscaler.Failure(upscaler.CurrentStatus))
            {
                if (upscaler.ErrorCallback is not null)
                {
                    upscaler.ErrorCallback(upscaler.CurrentStatus, upscaler.NativeInterface.GetStatusMessage());
                    upscaler.CurrentStatus = upscaler.NativeInterface.GetStatus();
                    if (!Upscaler.Failure(upscaler.CurrentStatus)) return;
                    Debug.LogError("The registered error handler failed to rectify the following error.");
                }

                Debug.LogWarning(upscaler.NativeInterface.GetStatus() + " | " + upscaler.NativeInterface.GetStatusMessage());
                upscaler.technique = Upscaler.Technique.None;
                upscaler.quality = Upscaler.Quality.Auto;
                upscaler.ApplySettings(true);
            }

            var stopHistoryReset = false;
            if (!isResizingThisFrame && upscaler.technique != Upscaler.Technique.None)
            {
                var isTemporal = upscaler.IsTemporal();
                if (isTemporal)
                {
                    _setMipBias.ConfigureInput(ScriptableRenderPassInput.None);
                    renderer.EnqueuePass(_setMipBias);
                }
                _upscale.ConfigureInput(isTemporal ? ScriptableRenderPassInput.Motion : ScriptableRenderPassInput.None);
                renderer.EnqueuePass(_upscale);
                stopHistoryReset |= isTemporal;
            }

            if (!isResizingThisFrame && upscaler.frameGeneration && previousFrameGeneration && NativeInterface.GetBackBufferFormat() != GraphicsFormat.None)
            {
                _generate.ConfigureInput(ScriptableRenderPassInput.Motion);
                renderer.EnqueuePass(_generate);
                stopHistoryReset = true;
            }

            if (!stopHistoryReset) return;
            _resetHistory.ConfigureInput(ScriptableRenderPassInput.None);
            renderer.EnqueuePass(_resetHistory);
        }

        public override void SetupRenderPasses(ScriptableRenderer renderer, in RenderingData renderingData)
        {
            var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
            if (!Application.isPlaying || renderingData.cameraData.cameraType != CameraType.Game || upscaler is null || !upscaler.isActiveAndEnabled) return;
            
            var shouldBeLoaded = upscaler.technique == Upscaler.Technique.SnapdragonGameSuperResolution1;
            ManageMaterial(ref Upscale.SgsrMaterial, "SnapdragonGameSuperResolution/v1/Upscale", shouldBeLoaded);
            shouldBeLoaded = upscaler.technique == Upscaler.Technique.SnapdragonGameSuperResolution2;
            Manage(ref Upscale.Sgsr3PassConvert, "SnapdragonGameSuperResolution/v2/C3/Convert", shouldBeLoaded && upscaler.sgsrMethod == Upscaler.SgsrMethod.Compute3Pass);
            Manage(ref Upscale.Sgsr3PassActivate, "SnapdragonGameSuperResolution/v2/C3/Activate", shouldBeLoaded && upscaler.sgsrMethod == Upscaler.SgsrMethod.Compute3Pass);
            Manage(ref Upscale.Sgsr3PassUpscale, "SnapdragonGameSuperResolution/v2/C3/Upscale", shouldBeLoaded && upscaler.sgsrMethod == Upscaler.SgsrMethod.Compute3Pass);
            Manage(ref Upscale.Sgsr2PassConvert, "SnapdragonGameSuperResolution/v2/C2/Convert", shouldBeLoaded && upscaler.sgsrMethod == Upscaler.SgsrMethod.Compute2Pass);
            Manage(ref Upscale.Sgsr2PassUpscale, "SnapdragonGameSuperResolution/v2/C2/Upscale", shouldBeLoaded && upscaler.sgsrMethod == Upscaler.SgsrMethod.Compute2Pass);
            ManageMaterial(ref Upscale.SgsrConvertMaterial, "SnapdragonGameSuperResolution/v2/F2/Convert", shouldBeLoaded && upscaler.sgsrMethod == Upscaler.SgsrMethod.Fragment2Pass);
            ManageMaterial(ref Upscale.SgsrUpscaleMaterial, "SnapdragonGameSuperResolution/v2/F2/Upscale", shouldBeLoaded && upscaler.sgsrMethod == Upscaler.SgsrMethod.Fragment2Pass);
            
            var renderTargetDescriptor = renderingData.cameraData.cameraTargetDescriptor;
            renderTargetDescriptor.width = upscaler.InputResolution.x;
            renderTargetDescriptor.height = upscaler.InputResolution.y;
            /**@todo: Remove Upscaler.InputResolution in favor of Unity's dynamic resolution system.*/
            // renderTargetDescriptor.useDynamicScale = true;
            var displayTargetDescriptor = renderingData.cameraData.cameraTargetDescriptor;
            displayTargetDescriptor.useDynamicScale = false;
            
            var descriptor = renderTargetDescriptor;
            descriptor.depthStencilFormat = GraphicsFormat.None;
            var renderTargetsUpdated = RenderingUtils.ReAllocateIfNeeded(ref _cameraRenderResolutionColorTarget, descriptor, name: "Conifer_CameraColorTarget");
            descriptor = renderTargetDescriptor;
            descriptor.colorFormat = RenderTextureFormat.Depth;
            renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _cameraRenderResolutionDepthTarget, descriptor, isShadowMap: upscaler.technique == Upscaler.Technique.FidelityFXSuperResolution, name: "Conifer_CameraDepthTarget");

            if (upscaler.IsTemporal())
            {
                descriptor = displayTargetDescriptor;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                descriptor.graphicsFormat = GraphicsFormat.R16G16_SFloat;
                renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _upscale.Inputs[Upscale.Motion], descriptor, name: "Conifer_Upscaler_Motion");
            }
            else
            {
                _upscale.Inputs[Upscale.Motion]?.Release();
                _upscale.Inputs[Upscale.Motion] = null;
            }
            
            if (upscaler.technique != Upscaler.Technique.SnapdragonGameSuperResolution1)
            {
                descriptor = displayTargetDescriptor;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                descriptor.enableRandomWrite = true;
                renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _upscale.Inputs[Upscale.Output], descriptor, name: "Conifer_Upscaler_Output");
            }
            else
            {
                _upscale.Inputs[Upscale.Output]?.Release();
                _upscale.Inputs[Upscale.Output] = null;
            }
            
            if (upscaler.technique == Upscaler.Technique.SnapdragonGameSuperResolution2 && upscaler.sgsrMethod is Upscaler.SgsrMethod.Compute3Pass or Upscaler.SgsrMethod.Compute2Pass)
            {
                descriptor = renderTargetDescriptor;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                descriptor.graphicsFormat = GraphicsFormat.R32_UInt;
                RenderingUtils.ReAllocateIfNeeded(ref _upscale.Inputs[Upscale.LumaHistory], descriptor, name: "Conifer_Upscaler_LumaHistory");
            }
            else
            {
                _upscale.Inputs[Upscale.LumaHistory]?.Release();
                _upscale.Inputs[Upscale.LumaHistory] = null;
            }
            
            if (upscaler.technique == Upscaler.Technique.SnapdragonGameSuperResolution2) {
                descriptor = displayTargetDescriptor;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                descriptor.graphicsFormat = GraphicsFormat.R16G16B16A16_SFloat;
                RenderingUtils.ReAllocateIfNeeded(ref _upscale.Inputs[Upscale.History], descriptor, name: "Conifer_Upscaler_History");
            }
            else
            {
                _upscale.Inputs[Upscale.History]?.Release();
                _upscale.Inputs[Upscale.History] = null;
            }

            if (upscaler.technique == Upscaler.Technique.FidelityFXSuperResolution && upscaler.autoReactive)
            {
                descriptor = renderTargetDescriptor;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _upscale.Inputs[Upscale.Opaque], descriptor, name: "Conifer_Upscaler_Opaque");
                descriptor.graphicsFormat = GraphicsFormat.R8_UNorm;
                descriptor.enableRandomWrite = true;
                renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _upscale.Inputs[Upscale.Reactive], descriptor, name: "Conifer_Upscaler_ReactiveMask");
            }
            else
            {
                _upscale.Inputs[Upscale.Reactive]?.Release();
                _upscale.Inputs[Upscale.Reactive] = null;
                _upscale.Inputs[Upscale.Opaque]?.Release();
                _upscale.Inputs[Upscale.Opaque] = null;
            }
            
            if ((renderTargetsUpdated || upscaler.technique != _lastTechnique || upscaler.autoReactive != _lastAutoReactive) && upscaler.RequiresNativePlugin())
                upscaler.NativeInterface.SetUpscalingImages(_cameraRenderResolutionColorTarget, _cameraRenderResolutionDepthTarget, _upscale.Inputs[Upscale.Motion], _upscale.Inputs[Upscale.Output], _upscale.Inputs[Upscale.Reactive], _upscale.Inputs[Upscale.Opaque], upscaler.autoReactive);
            _lastTechnique = upscaler.technique;
            _lastAutoReactive = upscaler.autoReactive;
        }
        
        protected override void Dispose(bool disposing)
        {
            if (!disposing) return;
            ManageMaterial(ref Upscale.SgsrMaterial, "SnapdragonGameSuperResolution/v1/Upscale", false);
            Manage(ref Upscale.Sgsr3PassConvert, "SnapdragonGameSuperResolution/v2/C3/Convert", false);
            Manage(ref Upscale.Sgsr3PassActivate, "SnapdragonGameSuperResolution/v2/C3/Activate", false);
            Manage(ref Upscale.Sgsr3PassUpscale, "SnapdragonGameSuperResolution/v2/C3/Upscale", false);
            Manage(ref Upscale.Sgsr2PassConvert, "SnapdragonGameSuperResolution/v2/C2/Convert", false);
            Manage(ref Upscale.Sgsr2PassUpscale, "SnapdragonGameSuperResolution/v2/C2/Upscale", false);
            ManageMaterial(ref Upscale.SgsrConvertMaterial, "SnapdragonGameSuperResolution/v2/F2/Convert", false);
            ManageMaterial(ref Upscale.SgsrUpscaleMaterial, "SnapdragonGameSuperResolution/v2/F2/Upscale", false);
            _upscale.Dispose();
            _generate.Dispose();
        }
    }
}