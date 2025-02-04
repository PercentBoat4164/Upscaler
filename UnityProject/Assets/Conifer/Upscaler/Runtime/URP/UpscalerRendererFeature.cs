/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

/**************************************************
 * Upscaler v2.0.0b                                *
 * See the UserManual.pdf for more information    *
 **************************************************/

using System;
using System.Reflection;
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

        private static readonly MethodInfo UseScaling = typeof(RTHandle).GetProperty("useScaling", BindingFlags.Instance | BindingFlags.Public)?.SetMethod!;
        private static readonly MethodInfo ScaleFactor = typeof(RTHandle).GetProperty("scaleFactor", BindingFlags.Instance | BindingFlags.Public)?.SetMethod!;
        private static readonly MethodInfo ReferenceSize = typeof(RTHandle).GetProperty("referenceSize", BindingFlags.Instance | BindingFlags.Public)?.SetMethod!;
        
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

        private class SetupUpscale : ScriptableRenderPass
        {
#if ENABLE_VR && ENABLE_XR_MODULE
            private static readonly FieldInfo JitterMat = typeof(CameraData).GetField("m_JitterMatrix", BindingFlags.Instance | BindingFlags.NonPublic);
#endif
            
            private static readonly int GlobalMipBias = Shader.PropertyToID("_GlobalMipBias");
            private int _jitterIndex;

            public SetupUpscale() => renderPassEvent = RenderPassEvent.BeforeRenderingPrePasses;

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
            private Matrix4x4 _previousProjectionMatrix;

            private RTHandle _output;
            private RTHandle _reactive;
            private RTHandle _lumaHistory;
            private RTHandle _history;
            private RTHandle _motion;
            private RTHandle _opaque;
            private RTHandle _fsrDepth;

            private Material _sgsr1Upscale;
            private Material _sgsr2F2Convert;
            private Material _sgsr2F2Upscale;
            private ComputeShader _sgsr2C2Convert;
            private ComputeShader _sgsr2C2Upscale;
            private ComputeShader _sgsr2C3PassConvert;
            private ComputeShader _sgsr2C3PassActivate;
            private ComputeShader _sgsr2C3PassUpscale;

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
            
            internal void Cleanup(in Upscaler upscaler, bool preserve = false)
            {
                switch (upscaler.PreviousTechnique)
                {
                    case Upscaler.Technique.None: return;
                    case Upscaler.Technique.DeepLearningSuperSampling:
                    case Upscaler.Technique.XeSuperSampling:
                        if (!preserve)
                        {
                            _motion?.Release();
                            _motion = null;
                        }
                        break;
                    case Upscaler.Technique.FidelityFXSuperResolution:
                        if (!preserve)
                        {
                            _motion?.Release();
                            _motion = null;
                        }
                        _fsrDepth?.Release();
                        _fsrDepth = null;
                        if (upscaler.PreviousAutoReactive)
                        {
                            _reactive?.Release();
                            _reactive = null;
                            _opaque?.Release();
                            _opaque = null;
                        }
                        break;
                    case Upscaler.Technique.SnapdragonGameSuperResolution1:
                        if (_sgsr1Upscale is not null)
                        {
                            Resources.UnloadAsset(_sgsr1Upscale.shader);
                            _sgsr1Upscale = null;
                        }
                        break;
                    case Upscaler.Technique.SnapdragonGameSuperResolution2:
                        switch (upscaler.PreviousSgsrMethod)
                        {
                            case Upscaler.SgsrMethod.Compute3Pass:
                                if (_sgsr2C3PassConvert is not null) {
                                    Resources.UnloadAsset(_sgsr2C3PassConvert);
                                    _sgsr2C3PassConvert = null;
                                }
                                if (_sgsr2C3PassActivate is not null) {
                                    Resources.UnloadAsset(_sgsr2C3PassActivate);
                                    _sgsr2C3PassActivate = null;
                                }
                                if (_sgsr2C3PassUpscale is not null) {
                                    Resources.UnloadAsset(_sgsr2C3PassUpscale);
                                    _sgsr2C3PassUpscale = null;
                                }
                                _lumaHistory?.Release();
                                _lumaHistory = null;
                                break;
                            case Upscaler.SgsrMethod.Compute2Pass:
                                if (_sgsr2C2Convert is not null) {
                                    Resources.UnloadAsset(_sgsr2C2Convert);
                                    _sgsr2C2Convert = null;
                                }
                                if (_sgsr2C2Upscale is not null) {
                                    Resources.UnloadAsset(_sgsr2C2Upscale);
                                    _sgsr2C2Upscale = null;
                                }
                                _lumaHistory?.Release();
                                _lumaHistory = null;
                                break;
                            case Upscaler.SgsrMethod.Fragment2Pass:
                                if (_sgsr2F2Convert is not null) {
                                    Resources.UnloadAsset(_sgsr2F2Convert.shader);
                                    _sgsr2F2Convert = null;
                                }
                                if (_sgsr2F2Upscale is not null) {
                                    Resources.UnloadAsset(_sgsr2F2Upscale.shader);
                                    _sgsr2F2Upscale = null;
                                }
                                break;
                            default: throw new ArgumentOutOfRangeException(nameof(upscaler.sgsrMethod), upscaler.sgsrMethod, null);
                        }
                        if (!preserve)
                        {
                            _history?.Release();
                            _history = null;
                        }
                        break;
                    default: throw new ArgumentOutOfRangeException(nameof(upscaler.technique), upscaler.technique, null);
                }
                if (preserve) return;
                _output?.Release();
                _output = null;
            }

            internal bool Initialize(in Upscaler upscaler, RenderTextureDescriptor displayTargetDescriptor)
            {
                var renderTargetDescriptor = displayTargetDescriptor;
                renderTargetDescriptor.width = upscaler.InputResolution.x;
                renderTargetDescriptor.height = upscaler.InputResolution.y;
                displayTargetDescriptor.useDynamicScale = false;

                var descriptor = displayTargetDescriptor;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                descriptor.enableRandomWrite = true;
                var renderTargetsUpdated = RenderingUtils.ReAllocateIfNeeded(ref _output, descriptor, name: "Conifer_Upscaler_Output", filterMode: FilterMode.Point);
                switch (upscaler.technique)
                {
                    case Upscaler.Technique.None: return false;
                    case Upscaler.Technique.DeepLearningSuperSampling:
                    case Upscaler.Technique.XeSuperSampling:
                        descriptor = displayTargetDescriptor;
                        descriptor.depthStencilFormat = GraphicsFormat.None;
                        descriptor.graphicsFormat = GraphicsFormat.R16G16_SFloat;
                        renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _motion, descriptor, name: "Conifer_Upscaler_Motion", filterMode: FilterMode.Point);
                        break;
                    case Upscaler.Technique.FidelityFXSuperResolution:
                        descriptor = displayTargetDescriptor;
                        descriptor.depthStencilFormat = GraphicsFormat.None;
                        descriptor.graphicsFormat = GraphicsFormat.R16G16_SFloat;
                        renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _motion, descriptor, name: "Conifer_Upscaler_Motion", filterMode: FilterMode.Point);
                        descriptor = renderTargetDescriptor;
                        descriptor.colorFormat = RenderTextureFormat.Depth;
                        renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _fsrDepth, descriptor, isShadowMap: true, name: "Conifer_Upscaler_FsrDepth", filterMode: FilterMode.Point);
                        if (upscaler.autoReactive)
                        {
                            descriptor = renderTargetDescriptor;
                            descriptor.depthStencilFormat = GraphicsFormat.None;
                            renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _opaque, descriptor, name: "Conifer_Upscaler_Opaque", filterMode: FilterMode.Point);
                            descriptor.graphicsFormat = GraphicsFormat.R8_UNorm;
                            descriptor.enableRandomWrite = true;
                            renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _reactive, descriptor, name: "Conifer_Upscaler_ReactiveMask", filterMode: FilterMode.Point);
                        }
                        break;
                    case Upscaler.Technique.SnapdragonGameSuperResolution1:
                        _sgsr1Upscale = new Material(Resources.Load<Shader>("SnapdragonGameSuperResolution/v1/Upscale"));
                        break;
                    case Upscaler.Technique.SnapdragonGameSuperResolution2:
                        switch (upscaler.sgsrMethod)
                        {
                            case Upscaler.SgsrMethod.Compute3Pass:
                                _sgsr2C3PassConvert = Resources.Load<ComputeShader>("SnapdragonGameSuperResolution/v2/C3/Convert");
                                _sgsr2C3PassActivate = Resources.Load<ComputeShader>("SnapdragonGameSuperResolution/v2/C3/Activate");
                                _sgsr2C3PassUpscale = Resources.Load<ComputeShader>("SnapdragonGameSuperResolution/v2/C3/Upscale");
                                descriptor = renderTargetDescriptor;
                                descriptor.depthStencilFormat = GraphicsFormat.None;
                                descriptor.graphicsFormat = GraphicsFormat.R32_UInt;
                                RenderingUtils.ReAllocateIfNeeded(ref _lumaHistory, descriptor, name: "Conifer_Upscaler_LumaHistory", filterMode: FilterMode.Point);
                                break;
                            case Upscaler.SgsrMethod.Compute2Pass:
                                _sgsr2C2Convert = Resources.Load<ComputeShader>("SnapdragonGameSuperResolution/v2/C2/Convert");
                                _sgsr2C2Upscale = Resources.Load<ComputeShader>("SnapdragonGameSuperResolution/v2/C2/Upscale");
                                descriptor = renderTargetDescriptor;
                                descriptor.depthStencilFormat = GraphicsFormat.None;
                                descriptor.graphicsFormat = GraphicsFormat.R32_UInt;
                                RenderingUtils.ReAllocateIfNeeded(ref _lumaHistory, descriptor, name: "Conifer_Upscaler_LumaHistory", filterMode: FilterMode.Point);
                                break;
                            case Upscaler.SgsrMethod.Fragment2Pass:
                                _sgsr2F2Convert = new Material(Resources.Load<Shader>("SnapdragonGameSuperResolution/v2/F2/Convert"));
                                _sgsr2F2Upscale = new Material(Resources.Load<Shader>("SnapdragonGameSuperResolution/v2/F2/Upscale"));
                                break;
                            default: throw new ArgumentOutOfRangeException(nameof(upscaler.sgsrMethod), upscaler.sgsrMethod, null);
                        }
                        descriptor = displayTargetDescriptor;
                        descriptor.depthStencilFormat = GraphicsFormat.None;
                        descriptor.graphicsFormat = GraphicsFormat.R16G16B16A16_SFloat;
                        RenderingUtils.ReAllocateIfNeeded(ref _history, descriptor, name: "Conifer_Upscaler_History", filterMode: FilterMode.Point);
                        break;
                    default:
                        throw new ArgumentOutOfRangeException(nameof(upscaler.technique), upscaler.technique, null);
                }
                return renderTargetsUpdated;
            }
            
            public void SetImages(Upscaler upscaler, ScriptableRenderer renderer) => upscaler.NativeInterface.SetUpscalingImages(renderer.cameraColorTargetHandle, upscaler.technique == Upscaler.Technique.FidelityFXSuperResolution ? _fsrDepth : renderer.cameraDepthTargetHandle, _motion, _output, _reactive, _opaque, upscaler.autoReactive);
            
            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
                var cb = CommandBufferPool.Get("Upscale");
                switch (upscaler.technique)
                {
                    case Upscaler.Technique.SnapdragonGameSuperResolution1:
                        cb.SetRenderTarget(_output);
                        cb.SetGlobalTexture(BlitID, renderingData.cameraData.renderer.cameraColorTargetHandle);
                        cb.SetProjectionMatrix(Ortho);
                        cb.SetViewMatrix(LookAt);
                        cb.SetGlobalVector(ViewportInfoID, new Vector4(1.0f / upscaler.OutputResolution.x, 1.0f / upscaler.OutputResolution.y, upscaler.InputResolution.x, upscaler.InputResolution.y));
                        cb.SetGlobalFloat(EdgeSharpnessID, upscaler.sharpness + 1);
                        cb.DrawMesh(_triangle, Matrix4x4.identity, _sgsr1Upscale, 0, 0);
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
                        var descriptor = _history.rt.descriptor;
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
                                descriptor = _lumaHistory.rt.descriptor;
                                cb.GetTemporaryRT(LumaID, descriptor);
                                descriptor.enableRandomWrite = true;
                                cb.GetTemporaryRT(LumaSinkID, descriptor);
                                cb.SetGlobalTexture(ColorID, renderingData.cameraData.renderer.cameraColorTargetHandle);
                                cb.SetGlobalTexture(LumaHistoryID, _lumaHistory);
                                cb.SetGlobalTexture(HistoryID, _history);
                                cb.SetGlobalTexture(OutputSinkID, _output);
                                cb.DispatchCompute(_sgsr2C3PassConvert, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                cb.CopyTexture(LumaSinkID, LumaID);
                                cb.CopyTexture(MotionDepthAlphaBufferSinkID, MotionDepthAlphaBufferID);
                                cb.DispatchCompute(_sgsr2C3PassActivate, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                cb.CopyTexture(LumaSinkID, _lumaHistory);
                                cb.CopyTexture(MotionDepthAlphaBufferSinkID, MotionDepthAlphaBufferID);
                                threadGroups = Vector3Int.CeilToInt(new Vector3(upscaler.OutputResolution.x, upscaler.OutputResolution.y, 1) / 8);
                                cb.DispatchCompute(_sgsr2C3PassUpscale, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                cb.CopyTexture(LumaID, _lumaHistory);
                                cb.CopyTexture(NextHistoryID, _history);
                                cb.ReleaseTemporaryRT(MotionDepthAlphaBufferSinkID);
                                cb.ReleaseTemporaryRT(LumaID);
                                cb.ReleaseTemporaryRT(LumaSinkID);
                                break;
                            }
                            case Upscaler.SgsrMethod.Compute2Pass:
                            {
                                descriptor.enableRandomWrite = true;
                                cb.GetTemporaryRT(MotionDepthAlphaBufferSinkID, descriptor);
                                descriptor = _lumaHistory.rt.descriptor;
                                cb.GetTemporaryRT(LumaID, descriptor);
                                descriptor.enableRandomWrite = true;
                                cb.GetTemporaryRT(LumaSinkID, descriptor);
                                cb.SetGlobalTexture(ColorID, renderingData.cameraData.renderer.cameraColorTargetHandle);
                                cb.SetGlobalTexture(HistoryID, _history);
                                cb.SetGlobalTexture(OutputSinkID, _output);
                                cb.DispatchCompute(_sgsr2C2Convert, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                threadGroups = Vector3Int.CeilToInt(new Vector3(upscaler.OutputResolution.x, upscaler.OutputResolution.y, 1) / 8);
                                cb.CopyTexture(LumaSinkID, LumaID);
                                cb.CopyTexture(MotionDepthAlphaBufferSinkID, MotionDepthAlphaBufferID);
                                cb.DispatchCompute(_sgsr2C2Upscale, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                cb.CopyTexture(NextHistoryID, _history);
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
                                cb.DrawMesh(_triangle, Matrix4x4.identity, _sgsr2F2Convert);
                                cb.SetGlobalTexture(HistoryID, _history);
                                cb.SetGlobalTexture(BlitID, renderingData.cameraData.renderer.cameraColorTargetHandle);
                                cb.SetRenderTarget(_output);
                                cb.DrawMesh(_triangle, Matrix4x4.identity, _sgsr2F2Upscale);
                                cb.Blit(_output, _history);
                                break;
                            }
                            default: throw new NotImplementedException();
                        }
                        cb.ReleaseTemporaryRT(NextHistoryID);
                        cb.ReleaseTemporaryRT(MotionDepthAlphaBufferID);
                        break;
                    case Upscaler.Technique.FidelityFXSuperResolution:
                        if (upscaler.autoReactive) cb.Blit(Shader.GetGlobalTexture(OpaqueID) ?? Texture2D.blackTexture, _opaque);
                        BlitDepth(cb, renderingData.cameraData.renderer.cameraDepthTargetHandle, _fsrDepth, (Vector2)upscaler.InputResolution / upscaler.OutputResolution);
                        cb.Blit(Shader.GetGlobalTexture(MotionID) ?? Texture2D.blackTexture, _motion);
                        upscaler.NativeInterface.Upscale(cb, upscaler, renderingData.cameraData.renderer.cameraColorTargetHandle.GetScaledSize());
                        break;
                    case Upscaler.Technique.DeepLearningSuperSampling:
                    case Upscaler.Technique.XeSuperSampling:
                        cb.Blit(Shader.GetGlobalTexture(MotionID) ?? Texture2D.blackTexture, _motion);
                        upscaler.NativeInterface.Upscale(cb, upscaler, renderingData.cameraData.renderer.cameraColorTargetHandle.GetScaledSize());
                        break;
                    case Upscaler.Technique.None:
                    default: throw new NotImplementedException();
                }
                cb.CopyTexture(_output, renderingData.cameraData.renderer.cameraColorTargetHandle);
                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);
                var args = new object[] { Vector2.one };
                ScaleFactor.Invoke(renderingData.cameraData.renderer.cameraColorTargetHandle, args);
                ScaleFactor.Invoke(renderingData.cameraData.renderer.cameraDepthTargetHandle, args);
            }
            
            public void Dispose()
            {
                if (_sgsr1Upscale is not null)
                {
                    Resources.UnloadAsset(_sgsr1Upscale.shader);
                    _sgsr1Upscale = null;
                }
                if (_sgsr2F2Convert is not null)
                {
                    Resources.UnloadAsset(_sgsr2F2Convert.shader);
                    _sgsr2F2Convert = null;
                }
                if (_sgsr2F2Upscale is not null) {
                    Resources.UnloadAsset(_sgsr2F2Upscale.shader);
                    _sgsr2F2Upscale = null;
                }
                if (_sgsr2C2Convert is not null) {
                    Resources.UnloadAsset(_sgsr2C2Convert);
                    _sgsr2C2Convert = null;
                }
                if (_sgsr2C2Upscale is not null) {
                    Resources.UnloadAsset(_sgsr2C2Upscale);
                    _sgsr2C2Upscale = null;
                }
                if (_sgsr2C3PassConvert is not null) {
                    Resources.UnloadAsset(_sgsr2C3PassConvert);
                    _sgsr2C3PassConvert = null;
                }
                if (_sgsr2C3PassActivate is not null) {
                    Resources.UnloadAsset(_sgsr2C3PassActivate);
                    _sgsr2C3PassActivate = null;
                }
                if (_sgsr2C3PassUpscale is not null) {
                    Resources.UnloadAsset(_sgsr2C3PassUpscale);
                    _sgsr2C3PassUpscale = null;
                }
                _output?.Release();
                _output = null;
                _reactive?.Release();
                _reactive = null;
                _lumaHistory?.Release();
                _lumaHistory = null;
                _history?.Release();
                _history = null;
                _motion?.Release();
                _motion = null;
                _opaque?.Release();
                _opaque = null;
                _fsrDepth?.Release();
                _fsrDepth = null;
            }
        }

        private class SetupGenerate : ScriptableRenderPass
        {
            public SetupGenerate() => renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing;
            
            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var descriptor = renderingData.cameraData.cameraTargetDescriptor;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                descriptor.graphicsFormat = GraphicsFormat.R16G16_SFloat;
                var cb = CommandBufferPool.Get("Generate");
                cb.GetTemporaryRT(Generate.TempMotion, descriptor);
                cb.Blit(Shader.GetGlobalTexture(MotionID) ?? Texture2D.blackTexture, Generate.TempMotion);
                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);
            }
        }
        
        private class Generate : ScriptableRenderPass
        {
            private readonly RTHandle[] _hudless = new RTHandle[2];
            private uint _hudlessBufferIndex;
            private static readonly int TempColor = Shader.PropertyToID("Conifer_Upscaler_TempColor");
            internal static readonly int TempMotion = Shader.PropertyToID("Conifer_Upscaler_TempMotion");
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
#if UNITY_EDITOR
                descriptor.width = (int)upscaler.NativeInterface.EditorResolution.x;
                descriptor.height = (int)upscaler.NativeInterface.EditorResolution.y;
#endif
                descriptor.depthStencilFormat = GraphicsFormat.None;
                descriptor.graphicsFormat = GraphicsFormat.R16G16_SFloat;
                needsUpdate |= RenderingUtils.ReAllocateIfNeeded(ref _flippedMotion, descriptor, name: "Conifer_Upscaler_FlippedMotion");
                descriptor = renderingData.cameraData.cameraTargetDescriptor;
#if UNITY_EDITOR
                descriptor.width = (int)upscaler.NativeInterface.EditorResolution.x;
                descriptor.height = (int)upscaler.NativeInterface.EditorResolution.y;
#endif
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
                cb.GetTemporaryRT(TempColor, _hudless[_hudlessBufferIndex].rt.descriptor);
                cb.Blit(null, TempColor);
                cb.Blit(TempColor, _hudless[_hudlessBufferIndex]
#if UNITY_EDITOR
                    // , upscaler.NativeInterface.EditorResolution / srcRes
                    , Vector2.one
                    , -upscaler.NativeInterface.EditorOffset / srcRes
#endif
                );
                cb.Blit(TempMotion, _flippedMotion,
#if UNITY_EDITOR
                    upscaler.NativeInterface.EditorResolution / srcRes *
#endif
                    new Vector2(1.0f, -1.0f),
#if UNITY_EDITOR
                    upscaler.NativeInterface.EditorOffset / srcRes +
#endif
                    new Vector2(0.0f, 1.0f));
                BlitDepth(cb, Shader.GetGlobalTexture(DepthID), _flippedDepth,
#if UNITY_EDITOR
                    upscaler.NativeInterface.EditorResolution / srcRes *
#endif
                    new Vector2(1.0f, -1.0f),
#if UNITY_EDITOR
                    upscaler.NativeInterface.EditorOffset / srcRes +
#endif
                    new Vector2(0.0f, 1.0f));
                upscaler.NativeInterface.FrameGenerate(cb, upscaler, _hudlessBufferIndex);
                cb.SetRenderTarget(k_CameraTarget);
                cb.ReleaseTemporaryRT(TempColor);
                cb.ReleaseTemporaryRT(TempMotion);
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

        private class RemoveHistoryReset : ScriptableRenderPass
        {
            public RemoveHistoryReset() => renderPassEvent = (RenderPassEvent)int.MaxValue;
            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData) => renderingData.cameraData.camera.GetComponent<Upscaler>().ShouldResetHistory(false);
        }

        private bool _renderTargetsUpdated;
        private bool _isResizingThisFrame;
        private readonly SetupUpscale _setupUpscale = new();
        private readonly Upscale _upscale = new();
        private readonly SetupGenerate _setupGenerate = new ();
        private readonly Generate _generate = new();
        private readonly RemoveHistoryReset _removeHistoryReset = new();
        private static RTHandle _colorHandle;
        private static RTHandle _depthHandle;
        
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
        
        public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
        {
            var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
            if (!Application.isPlaying || renderingData.cameraData.cameraType != CameraType.Game || upscaler is null || !upscaler.isActiveAndEnabled) return;

            if (upscaler.forceHistoryResetEveryFrame || renderingData.cameraData.camera.GetUniversalAdditionalCameraData().resetHistory) upscaler.ResetHistory();
            _isResizingThisFrame = upscaler.OutputResolution != upscaler.PreviousOutputResolution;
            var previousFrameGeneration = upscaler.PreviousFrameGeneration;
            _renderTargetsUpdated = false;
            var requiresFullReset = upscaler.PreviousTechnique != upscaler.technique || upscaler.PreviousSgsrMethod != upscaler.sgsrMethod || upscaler.PreviousAutoReactive != upscaler.autoReactive || upscaler.PreviousQuality != upscaler.quality || (!_isResizingThisFrame && upscaler.PreviousPreviousOutputResolution != upscaler.PreviousOutputResolution);
            if (!_isResizingThisFrame && (requiresFullReset || upscaler.InputResolution != upscaler.PreviousInputResolution))
            {
                _upscale.Cleanup(upscaler, !requiresFullReset);
                _renderTargetsUpdated |= _upscale.Initialize(upscaler, renderingData.cameraData.cameraTargetDescriptor);
            }
            upscaler.CurrentStatus = upscaler.ApplySettings();
            if (Upscaler.Failure(upscaler.CurrentStatus)) HandleErrors(renderingData.cameraData.cameraTargetDescriptor);
            
            if (!_isResizingThisFrame && upscaler.technique != Upscaler.Technique.None)
            {
                if (upscaler.IsTemporal())
                {
                    _setupUpscale.ConfigureInput(ScriptableRenderPassInput.None);
                    renderer.EnqueuePass(_setupUpscale);
                }
                _upscale.ConfigureInput(upscaler.IsTemporal() ? ScriptableRenderPassInput.Motion : ScriptableRenderPassInput.None);
                renderer.EnqueuePass(_upscale);
            }

            if (!_isResizingThisFrame && upscaler.frameGeneration && previousFrameGeneration && NativeInterface.GetBackBufferFormat() != GraphicsFormat.None)
            {
                _setupGenerate.ConfigureInput(ScriptableRenderPassInput.Motion);
                renderer.EnqueuePass(_setupGenerate);
                _generate.ConfigureInput(ScriptableRenderPassInput.None);
                renderer.EnqueuePass(_generate);
                VolumeManager.instance.stack.GetComponent<MotionBlur>().intensity.value /= 2.0f;
            }

            _removeHistoryReset.ConfigureInput(ScriptableRenderPassInput.None);
            renderer.EnqueuePass(_removeHistoryReset);
            return;

            void HandleErrors(RenderTextureDescriptor descriptor)
            {
                if (upscaler.ErrorCallback is not null)
                {
                    upscaler.ErrorCallback(upscaler.CurrentStatus, upscaler.NativeInterface.GetStatusMessage());
                    if (upscaler.PreviousTechnique != upscaler.technique || upscaler.PreviousSgsrMethod != upscaler.sgsrMethod || upscaler.PreviousAutoReactive != upscaler.autoReactive || upscaler.PreviousQuality != upscaler.quality)
                    {
                        _upscale.Cleanup(upscaler);
                        _renderTargetsUpdated |= _upscale.Initialize(upscaler, descriptor);
                    }
                    upscaler.CurrentStatus = upscaler.ApplySettings();
                    if (!Upscaler.Failure(upscaler.CurrentStatus)) return;
                    Debug.LogError("The registered error handler failed to rectify the following error.");
                }

                Debug.LogWarning(upscaler.NativeInterface.GetStatus() + " | " + upscaler.NativeInterface.GetStatusMessage());
                upscaler.technique = Upscaler.Technique.None;
                upscaler.quality = Upscaler.Quality.Auto;
                _upscale.Cleanup(upscaler);
                _renderTargetsUpdated |= _upscale.Initialize(upscaler, descriptor);
                upscaler.ApplySettings(true);
            }
        }

        public override void SetupRenderPasses(ScriptableRenderer renderer, in RenderingData renderingData)
        {
            var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
            if (!Application.isPlaying || renderingData.cameraData.cameraType != CameraType.Game || upscaler is null || !upscaler.isActiveAndEnabled || upscaler.technique == Upscaler.Technique.None) return;
            if (!_isResizingThisFrame && _colorHandle is not null && _depthHandle is not null)
            {
                var args = new object[] { (Vector2)upscaler.InputResolution / upscaler.OutputResolution };
                ScaleFactor.Invoke(_colorHandle, args);
                ScaleFactor.Invoke(_depthHandle, args);
                renderer.ConfigureCameraTarget(_colorHandle, _depthHandle);
            }
            else
            {
                _colorHandle?.Release();
                _depthHandle?.Release();
                _colorHandle = RTHandles.Alloc(renderer.cameraColorTargetHandle.rt);
                _depthHandle = RTHandles.Alloc(renderer.cameraDepthTargetHandle.rt);
                var args = new object[] { true };
                UseScaling.Invoke(_colorHandle, args);
                UseScaling.Invoke(_depthHandle, args);
                args = new object[] { upscaler.OutputResolution };
                ReferenceSize.Invoke(_colorHandle, args);
                ReferenceSize.Invoke(_depthHandle, args);
            }
            if (_renderTargetsUpdated && upscaler.RequiresNativePlugin()) _upscale.SetImages(renderingData.cameraData.camera.GetComponent<Upscaler>(), renderer);
        }

        protected override void Dispose(bool disposing)
        {
            if (!disposing) return;
            _upscale.Dispose();
            _generate.Dispose();
        }
    }
}