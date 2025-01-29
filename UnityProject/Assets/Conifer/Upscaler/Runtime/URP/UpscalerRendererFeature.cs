/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

/**************************************************
 * Upscaler v1.2.0                                *
 * See the UserManual.pdf for more information    *
 **************************************************/

using System;
using System.Reflection;
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

        private static readonly MethodInfo UseScaling = typeof(RTHandle).GetProperty("useScaling", BindingFlags.Instance | BindingFlags.Public)?.SetMethod;
        private static readonly MethodInfo ScaleFactor = typeof(RTHandle).GetProperty("scaleFactor", BindingFlags.Instance | BindingFlags.Public)?.SetMethod;
        
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

        private static void ManageComputeShader(ref ComputeShader computeShader, string path, bool shouldBeLoaded) 
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
#if ENABLE_VR && ENABLE_XR_MODULE
            private static readonly FieldInfo JitterMat = typeof(CameraData).GetField("m_JitterMatrix", BindingFlags.Instance | BindingFlags.NonPublic);
#endif
            
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
                var clipSpaceJitter = upscaler.Jitter / upscaler.InputResolution * 2;
#if ENABLE_VR && ENABLE_XR_MODULE
                if (renderingData.cameraData.xrRendering)
                {
                    object cameraData = renderingData.cameraData;
                    JitterMat.SetValue(cameraData, Matrix4x4.Translate(new Vector3(clipSpaceJitter.x, clipSpaceJitter.y, 0.0f)));
                    renderingData.cameraData = (CameraData)cameraData;
                    ScriptableRenderer.SetCameraMatrices(cb, ref renderingData.cameraData, true);
                }
                else
#endif
                {
                    var projectionMatrix = renderingData.cameraData.GetProjectionMatrix();
                    if (renderingData.cameraData.camera.orthographic)
                    {
                        projectionMatrix.m03 += clipSpaceJitter.x;
                        projectionMatrix.m13 += clipSpaceJitter.y;
                    }
                    else
                    {
                        projectionMatrix.m02 -= clipSpaceJitter.x;
                        projectionMatrix.m12 -= clipSpaceJitter.y;
                    }
                    cb.SetViewProjectionMatrices(renderingData.cameraData.GetViewMatrix(), projectionMatrix);
                }
                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);
            }
        }

        private class Upscale : ScriptableRenderPass
        {
            private static Matrix4x4 _previousProjectionMatrix;

            internal RTHandle Output;
            internal RTHandle Reactive;
            internal RTHandle LumaHistory;
            internal RTHandle History;
            internal RTHandle Motion;
            internal RTHandle Opaque;
            internal RTHandle FsrDepth;

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

            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
                var cb = CommandBufferPool.Get("Upscale");
                switch (upscaler.technique)
                {
                    case Upscaler.Technique.SnapdragonGameSuperResolution1:
                        cb.SetRenderTarget(Output);
                        cb.SetGlobalTexture(BlitID, renderingData.cameraData.renderer.cameraColorTargetHandle);
                        cb.SetProjectionMatrix(Ortho);
                        cb.SetViewMatrix(LookAt);
                        cb.SetGlobalVector(ViewportInfoID, new Vector4(1.0f / upscaler.OutputResolution.x, 1.0f / upscaler.OutputResolution.y, upscaler.InputResolution.x, upscaler.InputResolution.y));
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
                        var descriptor = History.rt.descriptor;
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
                                descriptor = LumaHistory.rt.descriptor;
                                cb.GetTemporaryRT(LumaID, descriptor);
                                descriptor.enableRandomWrite = true;
                                cb.GetTemporaryRT(LumaSinkID, descriptor);
                                cb.SetGlobalTexture(ColorID, renderingData.cameraData.renderer.cameraColorTargetHandle);
                                cb.SetGlobalTexture(LumaHistoryID, LumaHistory);
                                cb.SetGlobalTexture(HistoryID, History);
                                cb.SetGlobalTexture(OutputSinkID, Output);
                                cb.DispatchCompute(Sgsr3PassConvert, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                cb.CopyTexture(LumaSinkID, LumaID);
                                cb.CopyTexture(MotionDepthAlphaBufferSinkID, MotionDepthAlphaBufferID);
                                cb.DispatchCompute(Sgsr3PassActivate, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                cb.CopyTexture(LumaSinkID, LumaHistory);
                                cb.CopyTexture(MotionDepthAlphaBufferSinkID, MotionDepthAlphaBufferID);
                                threadGroups = Vector3Int.CeilToInt(new Vector3(upscaler.OutputResolution.x, upscaler.OutputResolution.y, 1) / 8);
                                cb.DispatchCompute(Sgsr3PassUpscale, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                cb.CopyTexture(LumaID, LumaHistory);
                                cb.CopyTexture(NextHistoryID, History);
                                cb.ReleaseTemporaryRT(MotionDepthAlphaBufferSinkID);
                                cb.ReleaseTemporaryRT(LumaID);
                                cb.ReleaseTemporaryRT(LumaSinkID);
                                break;
                            }
                            case Upscaler.SgsrMethod.Compute2Pass:
                            {
                                descriptor.enableRandomWrite = true;
                                cb.GetTemporaryRT(MotionDepthAlphaBufferSinkID, descriptor);
                                descriptor = LumaHistory.rt.descriptor;
                                cb.GetTemporaryRT(LumaID, descriptor);
                                descriptor.enableRandomWrite = true;
                                cb.GetTemporaryRT(LumaSinkID, descriptor);
                                cb.SetGlobalTexture(ColorID, renderingData.cameraData.renderer.cameraColorTargetHandle);
                                cb.SetGlobalTexture(HistoryID, History);
                                cb.SetGlobalTexture(OutputSinkID, Output);
                                cb.DispatchCompute(Sgsr2PassConvert, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                threadGroups = Vector3Int.CeilToInt(new Vector3(upscaler.OutputResolution.x, upscaler.OutputResolution.y, 1) / 8);
                                cb.CopyTexture(LumaSinkID, LumaID);
                                cb.CopyTexture(MotionDepthAlphaBufferSinkID, MotionDepthAlphaBufferID);
                                cb.DispatchCompute(Sgsr2PassUpscale, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                cb.CopyTexture(NextHistoryID, History);
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
                                cb.SetGlobalTexture(HistoryID, History);
                                cb.SetGlobalTexture(BlitID, renderingData.cameraData.renderer.cameraColorTargetHandle);
                                cb.SetRenderTarget(Output);
                                cb.DrawMesh(_triangle, Matrix4x4.identity, SgsrUpscaleMaterial);
                                cb.CopyTexture(Output, History);
                                break;
                            }
                            default: throw new NotImplementedException();
                        }
                        cb.ReleaseTemporaryRT(NextHistoryID);
                        cb.ReleaseTemporaryRT(MotionDepthAlphaBufferID);
                        break;
                    case Upscaler.Technique.FidelityFXSuperResolution:
                        if (upscaler.autoReactive) cb.Blit(Shader.GetGlobalTexture(OpaqueID) ?? Texture2D.blackTexture, Opaque);
                        BlitDepth(cb, renderingData.cameraData.renderer.cameraDepthTargetHandle, FsrDepth, (Vector2)upscaler.InputResolution / upscaler.OutputResolution);
                        cb.Blit(Shader.GetGlobalTexture(MotionID) ?? Texture2D.blackTexture, Motion);
                        upscaler.NativeInterface.Upscale(cb, upscaler, renderingData.cameraData.renderer.cameraColorTargetHandle.GetScaledSize());
                        break;
                    case Upscaler.Technique.DeepLearningSuperSampling:
                    case Upscaler.Technique.XeSuperSampling:
                        cb.Blit(Shader.GetGlobalTexture(MotionID) ?? Texture2D.blackTexture, Motion);
                        upscaler.NativeInterface.Upscale(cb, upscaler, renderingData.cameraData.renderer.cameraColorTargetHandle.GetScaledSize());
                        break;
                    case Upscaler.Technique.None:
                    default: throw new NotImplementedException();
                }
                cb.CopyTexture(Output, renderingData.cameraData.renderer.cameraColorTargetHandle);
                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);
                var args = new object[] { Vector2.one };
                ScaleFactor.Invoke(renderingData.cameraData.renderer.cameraColorTargetHandle, args);
                ScaleFactor.Invoke(renderingData.cameraData.renderer.cameraDepthTargetHandle, args);
            }
            
            public void Dispose()
            {
                Output?.Release();
                Output = null;
                Reactive?.Release();
                Reactive = null;
                LumaHistory?.Release();
                LumaHistory = null;
                History?.Release();
                History = null;
                Motion?.Release();
                Motion = null;
                Opaque?.Release();
                Opaque = null;
                FsrDepth?.Release();
                FsrDepth = null;
            }
        }

        private class Generate : ScriptableRenderPass
        {
            private readonly RTHandle[] _hudless = new RTHandle[2];
            private uint _hudlessBufferIndex;
            private int _temp = Shader.PropertyToID("Conifer_Upscaler_TempColor");
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
#if UNITY_EDITOR
                var srcRes = new Vector2(renderingData.cameraData.cameraTargetDescriptor.width, renderingData.cameraData.cameraTargetDescriptor.height);
#endif
                cb.GetTemporaryRT(_temp, _hudless[_hudlessBufferIndex].rt.descriptor);
                cb.Blit(null, _temp);
                cb.Blit(_temp, _hudless[_hudlessBufferIndex]
#if UNITY_EDITOR
                    , upscaler.NativeInterface.EditorResolution / srcRes, -upscaler.NativeInterface.EditorOffset / srcRes
#endif
                );
                cb.Blit(Shader.GetGlobalTexture(MotionID), _flippedMotion, new Vector2(1.0f, -1.0f), new Vector2(0.0f, 1.0f));
                BlitDepth(cb, Shader.GetGlobalTexture(DepthID), _flippedDepth, new Vector2(1.0f, -1.0f), new Vector2(0.0f, 1.0f));
                upscaler.NativeInterface.FrameGenerate(cb, upscaler, _hudlessBufferIndex);
                cb.SetRenderTarget(k_CameraTarget);
                cb.ReleaseTemporaryRT(_temp);
                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);
                _hudlessBufferIndex = (_hudlessBufferIndex + 1U) % (uint)_hudless.Length;
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

        private readonly SetMipBias _setMipBias = new();
        private readonly Upscale _upscale = new();
        private readonly Generate _generate = new();

        private const string Sgsr1Upscale = "SnapdragonGameSuperResolution/v1/Upscale";
        private const string Sgsr2C3Convert = "SnapdragonGameSuperResolution/v2/C3/Convert";
        private const string Sgsr2C3Activate = "SnapdragonGameSuperResolution/v2/C3/Activate";
        private const string Sgsr2C3Upscale = "SnapdragonGameSuperResolution/v2/C3/Upscale";
        private const string Sgsr2C2Convert = "SnapdragonGameSuperResolution/v2/C2/Convert";
        private const string Sgsr2C2Upscale = "SnapdragonGameSuperResolution/v2/C2/Upscale";
        private const string Sgsr2F2Convert = "SnapdragonGameSuperResolution/v2/F2/Convert";
        private const string Sgsr2F2Upscale = "SnapdragonGameSuperResolution/v2/F2/Upscale";
        
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
        private bool _lastStopHistoryReset;

        public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
        {
            var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
            if (!Application.isPlaying || renderingData.cameraData.cameraType != CameraType.Game || upscaler is null || !upscaler.isActiveAndEnabled)
            {
                _upscale.Output?.Release();
                _upscale.Output = null;
                return;
            }

            upscaler.ShouldResetHistory(upscaler.forceHistoryResetEveryFrame || renderingData.cameraData.camera.GetUniversalAdditionalCameraData().resetHistory);
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
            }

            if (isResizingThisFrame || !upscaler.frameGeneration || !previousFrameGeneration || NativeInterface.GetBackBufferFormat() == GraphicsFormat.None) return;
            _generate.ConfigureInput(ScriptableRenderPassInput.None);
            renderer.EnqueuePass(_generate);
        }

        public override void SetupRenderPasses(ScriptableRenderer renderer, in RenderingData renderingData)
        {
            var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
            if (!Application.isPlaying || renderingData.cameraData.cameraType != CameraType.Game || upscaler is null || !upscaler.isActiveAndEnabled) return;
            
            var shouldBeLoaded = upscaler.technique == Upscaler.Technique.SnapdragonGameSuperResolution1;
            ManageMaterial(ref Upscale.SgsrMaterial, Sgsr1Upscale, shouldBeLoaded);
            shouldBeLoaded = upscaler.technique == Upscaler.Technique.SnapdragonGameSuperResolution2;
            ManageComputeShader(ref Upscale.Sgsr3PassConvert, Sgsr2C3Convert, shouldBeLoaded && upscaler.sgsrMethod == Upscaler.SgsrMethod.Compute3Pass);
            ManageComputeShader(ref Upscale.Sgsr3PassActivate, Sgsr2C3Activate, shouldBeLoaded && upscaler.sgsrMethod == Upscaler.SgsrMethod.Compute3Pass);
            ManageComputeShader(ref Upscale.Sgsr3PassUpscale, Sgsr2C3Upscale, shouldBeLoaded && upscaler.sgsrMethod == Upscaler.SgsrMethod.Compute3Pass);
            ManageComputeShader(ref Upscale.Sgsr2PassConvert, Sgsr2C2Convert, shouldBeLoaded && upscaler.sgsrMethod == Upscaler.SgsrMethod.Compute2Pass);
            ManageComputeShader(ref Upscale.Sgsr2PassUpscale, Sgsr2C2Upscale, shouldBeLoaded && upscaler.sgsrMethod == Upscaler.SgsrMethod.Compute2Pass);
            ManageMaterial(ref Upscale.SgsrConvertMaterial, Sgsr2F2Convert, shouldBeLoaded && upscaler.sgsrMethod == Upscaler.SgsrMethod.Fragment2Pass);
            ManageMaterial(ref Upscale.SgsrUpscaleMaterial, Sgsr2F2Upscale, shouldBeLoaded && upscaler.sgsrMethod == Upscaler.SgsrMethod.Fragment2Pass);
            
            var renderTargetDescriptor = renderingData.cameraData.cameraTargetDescriptor;
            renderTargetDescriptor.width = upscaler.InputResolution.x;
            renderTargetDescriptor.height = upscaler.InputResolution.y;
            var displayTargetDescriptor = renderingData.cameraData.cameraTargetDescriptor;
            displayTargetDescriptor.useDynamicScale = false;

            var descriptor = displayTargetDescriptor;
            descriptor.depthStencilFormat = GraphicsFormat.None;
            descriptor.enableRandomWrite = true;
            var renderTargetsUpdated = RenderingUtils.ReAllocateIfNeeded(ref _upscale.Output, descriptor, name: "Conifer_Upscaler_Output", filterMode: FilterMode.Point);

            if (upscaler.IsTemporal())
            {
                descriptor = displayTargetDescriptor;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                descriptor.graphicsFormat = GraphicsFormat.R16G16_SFloat;
                renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _upscale.Motion, descriptor, name: "Conifer_Upscaler_Motion", filterMode: FilterMode.Point);
            }
            else
            {
                _upscale.Motion?.Release();
                _upscale.Motion = null;
            }

            if (upscaler.technique == Upscaler.Technique.SnapdragonGameSuperResolution2 && upscaler.sgsrMethod is Upscaler.SgsrMethod.Compute3Pass or Upscaler.SgsrMethod.Compute2Pass)
            {
                descriptor = renderTargetDescriptor;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                descriptor.graphicsFormat = GraphicsFormat.R32_UInt;
                RenderingUtils.ReAllocateIfNeeded(ref _upscale.LumaHistory, descriptor, name: "Conifer_Upscaler_LumaHistory", filterMode: FilterMode.Point);
            }
            else
            {
                _upscale.LumaHistory?.Release();
                _upscale.LumaHistory = null;
            }

            if (upscaler.technique == Upscaler.Technique.SnapdragonGameSuperResolution2) {
                descriptor = displayTargetDescriptor;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                descriptor.graphicsFormat = GraphicsFormat.R16G16B16A16_SFloat;
                RenderingUtils.ReAllocateIfNeeded(ref _upscale.History, descriptor, name: "Conifer_Upscaler_History", filterMode: FilterMode.Point);
            }
            else
            {
                _upscale.History?.Release();
                _upscale.History = null;
            }

            if (upscaler.technique == Upscaler.Technique.FidelityFXSuperResolution)
            {
                descriptor = renderTargetDescriptor;
                descriptor.colorFormat = RenderTextureFormat.Depth;
                renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _upscale.FsrDepth, descriptor, isShadowMap: true, name: "Conifer_Upscaler_FsrDepth", filterMode: FilterMode.Point);
            }

            if (upscaler.technique == Upscaler.Technique.FidelityFXSuperResolution && upscaler.autoReactive)
            {
                descriptor = renderTargetDescriptor;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _upscale.Opaque, descriptor, name: "Conifer_Upscaler_Opaque", filterMode: FilterMode.Point);
                descriptor.graphicsFormat = GraphicsFormat.R8_UNorm;
                descriptor.enableRandomWrite = true;
                renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _upscale.Reactive, descriptor, name: "Conifer_Upscaler_ReactiveMask", filterMode: FilterMode.Point);
            }
            else
            {
                _upscale.Reactive?.Release();
                _upscale.Reactive = null;
                _upscale.Opaque?.Release();
                _upscale.Opaque = null;
            }

            if (upscaler.technique != Upscaler.Technique.None)
            {
                var args = new object[] { true };
                UseScaling.Invoke(renderer.cameraColorTargetHandle, args);
                UseScaling.Invoke(renderer.cameraDepthTargetHandle, args);
                args = new object[] { (Vector2)upscaler.InputResolution / upscaler.OutputResolution };
                ScaleFactor.Invoke(renderer.cameraColorTargetHandle, args);
                ScaleFactor.Invoke(renderer.cameraDepthTargetHandle, args);

                if ((renderTargetsUpdated || upscaler.technique != _lastTechnique || upscaler.autoReactive != _lastAutoReactive) && upscaler.RequiresNativePlugin())
                    upscaler.NativeInterface.SetUpscalingImages(renderer.cameraColorTargetHandle, upscaler.technique == Upscaler.Technique.FidelityFXSuperResolution ? _upscale.FsrDepth : renderer.cameraDepthTargetHandle, _upscale.Motion, _upscale.Output, _upscale.Reactive, _upscale.Opaque, upscaler.autoReactive);
            }
            _lastTechnique = upscaler.technique;
            _lastAutoReactive = upscaler.autoReactive;
        }

        protected override void Dispose(bool disposing)
        {
            if (!disposing) return;
            ManageMaterial(ref Upscale.SgsrMaterial, Sgsr1Upscale, false);
            ManageComputeShader(ref Upscale.Sgsr3PassConvert, Sgsr2C3Convert, false);
            ManageComputeShader(ref Upscale.Sgsr3PassActivate, Sgsr2C3Activate, false);
            ManageComputeShader(ref Upscale.Sgsr3PassUpscale, Sgsr2C3Upscale, false);
            ManageComputeShader(ref Upscale.Sgsr2PassConvert, Sgsr2C2Convert, false);
            ManageComputeShader(ref Upscale.Sgsr2PassUpscale, Sgsr2C2Upscale, false);
            ManageMaterial(ref Upscale.SgsrConvertMaterial, Sgsr2F2Convert, false);
            ManageMaterial(ref Upscale.SgsrUpscaleMaterial, Sgsr2F2Upscale, false);
            _upscale.Dispose();
            _generate.Dispose();
        }
    }
}