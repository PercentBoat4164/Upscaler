/**************************************************
 * Upscaler v2.0.0                                *
 * See the UserManual.pdf for more information    *
 **************************************************/

using System;
using System.Reflection;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
#if UNITY_6000_0_OR_NEWER
using UnityEngine.Rendering.RenderGraphModule;
using UnityEngine.Rendering.RenderGraphModule.Util;
#endif
using UnityEngine.Rendering.Universal;

namespace Conifer.Upscaler.URP
{
    [DisallowMultipleRendererFeature("Upscaler Renderer Feature")]
    public class UpscalerRendererFeature : ScriptableRendererFeature
    {
        private static Material _depthBlitMaterial;

        private static readonly int BlitID = Shader.PropertyToID("_BlitTexture");
        private static readonly int BlitScaleBiasID = Shader.PropertyToID("_BlitScaleBias");
        private static readonly int MotionID = Shader.PropertyToID("_MotionVectorTexture");
        private static readonly int OpaqueID = Shader.PropertyToID("_CameraOpaqueTexture");
        private static readonly int ColorID = Shader.PropertyToID("_CameraColorTexture");
#if !UNITY_6000_0_OR_NEWER
        private static readonly int DepthID = Shader.PropertyToID("_CameraDepthTexture");
#endif

        private static readonly Matrix4x4 Ortho = Matrix4x4.Ortho(-1, 1, 1, -1, 1, -1);
        private static readonly Matrix4x4 LookAt = Matrix4x4.LookAt(Vector3.back, Vector3.forward, Vector3.up);

        private static readonly MethodInfo UseScaling = typeof(RTHandle).GetProperty("useScaling", BindingFlags.Instance | BindingFlags.Public)?.SetMethod!;
        private static readonly MethodInfo ScaleFactor = typeof(RTHandle).GetProperty("scaleFactor", BindingFlags.Instance | BindingFlags.Public)?.SetMethod!;

        private static void BlitDepth(CommandBuffer cb, RenderTargetIdentifier src, RenderTargetIdentifier dst, Vector2 scale = default, Vector2 offset = default)
        {
            if (scale == default) scale = Vector2.one;
            if (offset == default) offset = Vector2.zero;
            cb.SetViewProjectionMatrices(LookAt, Ortho);
            cb.SetRenderTarget(dst);
            cb.SetGlobalTexture(BlitID, src);
            cb.SetGlobalVector(BlitScaleBiasID, new Vector4(scale.x, scale.y, offset.x, offset.y));
            cb.DrawProcedural(Matrix4x4.identity, _depthBlitMaterial, 0, MeshTopology.Triangles, 3);
        }

        private class SetupUpscale : ScriptableRenderPass
        {
#if ENABLE_VR && ENABLE_XR_MODULE
            private static readonly FieldInfo JitterMat = typeof(CameraData).GetField("m_JitterMatrix", BindingFlags.Instance | BindingFlags.NonPublic);
#endif

            private static readonly int GlobalMipBias = Shader.PropertyToID("_GlobalMipBias");

            public SetupUpscale() => renderPassEvent = RenderPassEvent.BeforeRenderingPrePasses;

#if UNITY_6000_0_OR_NEWER
            private class PassData
            {
                internal Vector2Int Output;
                internal Vector2Int Input;
                internal Vector2 Jitter;
                internal UniversalCameraData CameraData;
            }

            public override void RecordRenderGraph(RenderGraph renderGraph, ContextContainer frameData)
            {
                var cameraData = frameData.Get<UniversalCameraData>();
                var upscaler = cameraData.camera.GetComponent<Upscaler>();
                if (upscaler.IsTemporal())
                {
                    upscaler.Jitter = new Vector2(HaltonSequence.Get(upscaler.JitterIndex, 2), HaltonSequence.Get(upscaler.JitterIndex, 3));
                    upscaler.JitterIndex = (upscaler.JitterIndex + 1) % (int)Math.Ceiling(7 * Math.Pow((float)upscaler.OutputResolution.x / upscaler.InputResolution.x, 2));
                    using var builder = renderGraph.AddComputePass<PassData>("Conifer | Upscaler | Setup Upscaling", out var data);
                    data.CameraData = cameraData;
                    data.Output = upscaler.OutputResolution;
                    data.Input = upscaler.InputResolution;
                    data.Jitter = upscaler.Jitter / upscaler.InputResolution * 2;
                    builder.SetRenderFunc((PassData passData, ComputeGraphContext context) => ExecutePass(passData, context));
                    builder.AllowPassCulling(false);
                    builder.AllowGlobalStateModification(true);
                }
                var resources = frameData.Get<UniversalResourceData>();
                var descriptor = renderGraph.GetTextureDesc(resources.cameraColor);
                descriptor.width = upscaler.InputResolution.x;
                descriptor.height = upscaler.InputResolution.y;
                descriptor.name = "Conifer | Upscaler | Input - Image";
                resources.cameraColor = renderGraph.CreateTexture(descriptor);
                descriptor = renderGraph.GetTextureDesc(resources.cameraDepth);
                descriptor.width = upscaler.InputResolution.x;
                descriptor.height = upscaler.InputResolution.y;
                descriptor.name = "Conifer | Upscaler | Input Depth - Image";
                resources.cameraDepth = renderGraph.CreateTexture(descriptor);
            }

            private static void ExecutePass(PassData data, ComputeGraphContext context)
            {
                var mipBias = (float)Math.Log((float)data.Input.x / data.Output.x, 2f) - 1f;
                context.cmd.SetGlobalVector(GlobalMipBias, new Vector4(mipBias, mipBias * mipBias));
#if ENABLE_VR && ENABLE_XR_MODULE
                if (data.CameraData.xrRendering)
                {
                    JitterMat.SetValue(data.CameraData, Matrix4x4.Translate(new Vector3(data.Jitter.x, data.Jitter.y, 0.0f)));
                    ScriptableRenderer.SetCameraMatrices(context.cmd, data.CameraData, true);
                }
                else
#endif
                {
                    var projectionMatrix = data.CameraData.GetProjectionMatrix();
                    if (data.CameraData.camera.orthographic)
                    {
                        projectionMatrix.m03 += data.Jitter.x;
                        projectionMatrix.m13 += data.Jitter.y;
                    }
                    else
                    {
                        projectionMatrix.m02 -= data.Jitter.x;
                        projectionMatrix.m12 -= data.Jitter.y;
                    }
                    context.cmd.SetViewProjectionMatrices(data.CameraData.GetViewMatrix(), projectionMatrix);
                }
            }

            [Obsolete("This rendering path is for compatibility mode only (when Render Graph is disabled). Use Render Graph API instead.", false)]
#endif
            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
                if (upscaler.IsSpatial()) return;
                var cb = CommandBufferPool.Get("SetMipBias");
                var mipBias = (float)Math.Log((float)upscaler.InputResolution.x / upscaler.OutputResolution.x, 2f) - 1f;
                cb.SetGlobalVector(GlobalMipBias, new Vector4(mipBias, mipBias * mipBias));
                upscaler.Jitter = new Vector2(HaltonSequence.Get(upscaler.JitterIndex, 2), HaltonSequence.Get(upscaler.JitterIndex, 3));
                upscaler.JitterIndex = (upscaler.JitterIndex + 1) % (int)Math.Ceiling(7 * Math.Pow((float)upscaler.OutputResolution.x / upscaler.InputResolution.x, 2));
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
            internal bool Disposed = true;
            private Matrix4x4 _previousMatrix;

            internal RTHandle _color;
            private RTHandle _output;
            private RTHandle _reactive;
            private RTHandle _lumaHistory;
            private RTHandle _history;
            private RTHandle _motion;
            private RTHandle _opaque;
            internal RTHandle _depth;

            private Material _sgsr1Upscale;
            private Material _sgsr2F2Convert;
            private Material _sgsr2F2Upscale;
            private ComputeShader _sgsr2C2Convert;
            private ComputeShader _sgsr2C2Upscale;
            private ComputeShader _sgsr2C3Convert;
            private ComputeShader _sgsr2C3Activate;
            private ComputeShader _sgsr2C3Upscale;

            private static readonly int ViewportInfoID = Shader.PropertyToID("Conifer_Upscaler_ViewportInfo");
            private static readonly int EdgeSharpnessID = Shader.PropertyToID("Conifer_Upscaler_EdgeSharpness");
            private static readonly int MotionDepthAlphaBufferID = Shader.PropertyToID("Conifer_Upscaler_MotionDepthAlphaBuffer");
            private static readonly int MotionDepthAlphaBufferSinkID = Shader.PropertyToID("Conifer_Upscaler_MotionDepthAlphaBufferSink");
            private static readonly int TempMotionDepthAlphaBuffer0ID = Shader.PropertyToID("Conifer_Upscaler_TempMotionDepthAlphaBuffer0");
            private static readonly int TempMotionDepthAlphaBuffer1ID = Shader.PropertyToID("Conifer_Upscaler_TempMotionDepthAlphaBuffer1");
            private static readonly int LumaID = Shader.PropertyToID("Conifer_Upscaler_Luma");
            private static readonly int LumaSinkID = Shader.PropertyToID("Conifer_Upscaler_LumaSink");
            private static readonly int TempLuma0ID = Shader.PropertyToID("Conifer_Upscaler_TempLuma0");
            private static readonly int TempLuma1ID = Shader.PropertyToID("Conifer_Upscaler_TempLuma1");
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
                _depth?.Release();
                _depth = null;
                _color?.Release();
                _color = null;
                switch (upscaler.PreviousTechnique)
                {
                    case Upscaler.Technique.None: return;
                    case Upscaler.Technique.DeepLearningSuperSampling or Upscaler.Technique.XeSuperSampling or Upscaler.Technique.FidelityFXSuperResolution:
                        if (!preserve)
                        {
                            _motion?.Release();
                            _motion = null;
                        }
                        if (upscaler.PreviousAutoReactive && upscaler.PreviousTechnique == Upscaler.Technique.FidelityFXSuperResolution)
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
                                if (_sgsr2C3Convert is not null) {
                                    Resources.UnloadAsset(_sgsr2C3Convert);
                                    _sgsr2C3Convert = null;
                                }
                                if (_sgsr2C3Activate is not null) {
                                    Resources.UnloadAsset(_sgsr2C3Activate);
                                    _sgsr2C3Activate = null;
                                }
                                if (_sgsr2C3Upscale is not null) {
                                    Resources.UnloadAsset(_sgsr2C3Upscale);
                                    _sgsr2C3Upscale = null;
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
                Disposed = false;
                var renderTargetDescriptor = displayTargetDescriptor;
                renderTargetDescriptor.width = upscaler.InputResolution.x;
                renderTargetDescriptor.height = upscaler.InputResolution.y;
                displayTargetDescriptor.useDynamicScale = false;

                var descriptor = displayTargetDescriptor;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                descriptor.enableRandomWrite = true;
#if UNITY_6000_0_OR_NEWER
                var renderTargetsUpdated = RenderingUtils.ReAllocateHandleIfNeeded(ref _output, descriptor, name: "Conifer_Upscaler_Output");
#else
                var renderTargetsUpdated = RenderingUtils.ReAllocateIfNeeded(ref _output, descriptor, name: "Conifer_Upscaler_Output");
#endif
#if UNITY_6000_0_OR_NEWER
                var compatibilityMode = GraphicsSettings.GetRenderPipelineSettings<RenderGraphSettings>().enableRenderCompatibilityMode;
                descriptor = compatibilityMode ? displayTargetDescriptor : renderTargetDescriptor;
#else
                descriptor = displayTargetDescriptor;
#endif
                descriptor.colorFormat = RenderTextureFormat.Depth;
                descriptor.stencilFormat = GraphicsFormat.None;
#if UNITY_6000_0_OR_NEWER
                renderTargetsUpdated |= RenderingUtils.ReAllocateHandleIfNeeded(ref _depth, descriptor, name: "Conifer_Upscaler_Depth");
#else
                renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _depth, descriptor, isShadowMap: true, name: "Conifer_Upscaler_Depth");
#endif
#if UNITY_6000_0_OR_NEWER
                descriptor = compatibilityMode ? displayTargetDescriptor : renderTargetDescriptor;
#else
                descriptor = displayTargetDescriptor;
#endif
                descriptor.depthStencilFormat = GraphicsFormat.None;
#if UNITY_6000_0_OR_NEWER
                renderTargetsUpdated |= RenderingUtils.ReAllocateHandleIfNeeded(ref _color, descriptor, name: "Conifer_Upscaler_Color");
#else
                renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _color, descriptor, name: "Conifer_Upscaler_Color");
#endif
                switch (upscaler.technique)
                {
                    case Upscaler.Technique.None: return false;
                    case Upscaler.Technique.DeepLearningSuperSampling or Upscaler.Technique.XeSuperSampling or Upscaler.Technique.FidelityFXSuperResolution:
                        descriptor = displayTargetDescriptor;
                        descriptor.depthStencilFormat = GraphicsFormat.None;
                        descriptor.graphicsFormat = GraphicsFormat.R16G16_SFloat;
#if UNITY_6000_0_OR_NEWER
                        renderTargetsUpdated |= RenderingUtils.ReAllocateHandleIfNeeded(ref _motion, descriptor, name: "Conifer_Upscaler_Motion");
#else
                        renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _motion, descriptor, name: "Conifer_Upscaler_Motion");
#endif
                        if (upscaler.autoReactive && upscaler.technique == Upscaler.Technique.FidelityFXSuperResolution)
                        {
                            descriptor = renderTargetDescriptor;
                            descriptor.depthStencilFormat = GraphicsFormat.None;
#if UNITY_6000_0_OR_NEWER
                            renderTargetsUpdated |= RenderingUtils.ReAllocateHandleIfNeeded(ref _opaque, descriptor, name: "Conifer_Upscaler_Opaque");
#else
                            renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _opaque, descriptor, name: "Conifer_Upscaler_Opaque");
#endif
                            descriptor.graphicsFormat = GraphicsFormat.R8_UNorm;
                            descriptor.enableRandomWrite = true;
#if UNITY_6000_0_OR_NEWER
                            renderTargetsUpdated |= RenderingUtils.ReAllocateHandleIfNeeded(ref _reactive, descriptor, name: "Conifer_Upscaler_ReactiveMask");
#else
                            renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _reactive, descriptor, name: "Conifer_Upscaler_ReactiveMask");
#endif
                        }
                        break;
                    case Upscaler.Technique.SnapdragonGameSuperResolution1:
                        _sgsr1Upscale = new Material(Resources.Load<Shader>("SnapdragonGameSuperResolution/v1/Upscale"));
                        break;
                    case Upscaler.Technique.SnapdragonGameSuperResolution2:
                        switch (upscaler.sgsrMethod)
                        {
                            case Upscaler.SgsrMethod.Compute3Pass:
                                _sgsr2C3Convert = Resources.Load<ComputeShader>("SnapdragonGameSuperResolution/v2/C3/Convert");
                                _sgsr2C3Activate = Resources.Load<ComputeShader>("SnapdragonGameSuperResolution/v2/C3/Activate");
                                _sgsr2C3Upscale = Resources.Load<ComputeShader>("SnapdragonGameSuperResolution/v2/C3/Upscale");
                                descriptor = renderTargetDescriptor;
                                descriptor.depthStencilFormat = GraphicsFormat.None;
                                descriptor.graphicsFormat = GraphicsFormat.R32_UInt;
#if UNITY_6000_0_OR_NEWER
                                descriptor.enableRandomWrite = true;
                                RenderingUtils.ReAllocateHandleIfNeeded(ref _lumaHistory, descriptor, name: "Conifer_Upscaler_LumaHistory");
#else
                                RenderingUtils.ReAllocateIfNeeded(ref _lumaHistory, descriptor, name: "Conifer_Upscaler_LumaHistory");
#endif
                                break;
                            case Upscaler.SgsrMethod.Compute2Pass:
                                _sgsr2C2Convert = Resources.Load<ComputeShader>("SnapdragonGameSuperResolution/v2/C2/Convert");
                                _sgsr2C2Upscale = Resources.Load<ComputeShader>("SnapdragonGameSuperResolution/v2/C2/Upscale");
                                descriptor = renderTargetDescriptor;
                                descriptor.depthStencilFormat = GraphicsFormat.None;
                                descriptor.graphicsFormat = GraphicsFormat.R32_UInt;
#if UNITY_6000_0_OR_NEWER
                                descriptor.enableRandomWrite = true;
                                RenderingUtils.ReAllocateHandleIfNeeded(ref _lumaHistory, descriptor, name: "Conifer_Upscaler_LumaHistory");
#else
                                RenderingUtils.ReAllocateIfNeeded(ref _lumaHistory, descriptor, name: "Conifer_Upscaler_LumaHistory");
#endif
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
#if UNITY_6000_0_OR_NEWER
                        RenderingUtils.ReAllocateHandleIfNeeded(ref _history, descriptor, name: "Conifer_Upscaler_History");
#else
                        RenderingUtils.ReAllocateIfNeeded(ref _history, descriptor, name: "Conifer_Upscaler_History");
#endif
                        break;
                    default:
                        throw new ArgumentOutOfRangeException(nameof(upscaler.technique), upscaler.technique, null);
                }
                return renderTargetsUpdated;
            }

            public void SetImages(Upscaler upscaler) => upscaler.NativeInterface.SetUpscalingImages(_color, _depth, _motion, _output, _reactive, _opaque, upscaler.autoReactive);
#if UNITY_6000_0_OR_NEWER
            private class Sgsr2F2PassData
            {
                internal Vector2Int Output;
                internal Vector2Int Input;
                internal Vector2 Jitter;
                internal bool CameraIsSame;
                internal float FOV;
                internal bool ShouldReset;
                internal Material ConvertMaterial;
                internal Material UpscaleMaterial;
                internal TextureHandle OutputTexture;
                internal TextureHandle InputTexture;
                internal TextureHandle HistoryTexture;
                internal TextureHandle MotionDepthAlphaTexture;
            }

            private class Sgsr2C2PassData
            {
                internal Vector2Int Output;
                internal Vector2Int Input;
                internal Vector2 Jitter;
                internal bool CameraIsSame;
                internal float FOV;
                internal bool ShouldReset;
                internal Vector3Int OutputThreadGroups;
                internal Vector3Int InputThreadGroups;
                internal ComputeShader ConvertShader;
                internal ComputeShader UpscaleShader;
                internal TextureHandle OutputTexture;
                internal TextureHandle InputTexture;
                internal TextureHandle HistoryTexture;
                internal TextureHandle TempHistoryTexture;
                internal TextureHandle TempMotionDepthAlphaTexture;
                internal TextureHandle TempLumaTexture;
            }

            private class Sgsr2C3PassData
            {
                internal Vector2Int Output;
                internal Vector2Int Input;
                internal Vector2 Jitter;
                internal bool CameraIsSame;
                internal float FOV;
                internal bool ShouldReset;
                internal Vector3Int OutputThreadGroups;
                internal Vector3Int InputThreadGroups;
                internal ComputeShader ConvertShader;
                internal ComputeShader ActivateShader;
                internal ComputeShader UpscaleShader;
                internal TextureHandle OutputTexture;
                internal TextureHandle InputTexture;
                internal TextureHandle HistoryTexture;
                internal TextureHandle TempHistoryTexture;
                internal TextureHandle TempMotionDepthAlphaTexture0;
                internal TextureHandle TempMotionDepthAlphaTexture1;
                internal TextureHandle LumaTexture;
                internal TextureHandle TempLumaTexture0;
                internal TextureHandle TempLumaTexture1;
            }

            private class NativeUpscalerData
            {
                internal Upscaler Upscaler;
            }

            public override void RecordRenderGraph(RenderGraph renderGraph, ContextContainer frameData)
            {
                var cameraData = frameData.Get<UniversalCameraData>();
                var upscaler = cameraData.camera.GetComponent<Upscaler>();
                var resources = frameData.Get<UniversalResourceData>();
                var renderResolutionDescriptor = renderGraph.GetTextureDesc(resources.cameraColor);
                var outputResolutionDescriptor = renderResolutionDescriptor;
                outputResolutionDescriptor.width = upscaler.OutputResolution.x;
                outputResolutionDescriptor.height = upscaler.OutputResolution.y;
                switch (upscaler.technique) {
                    case Upscaler.Technique.SnapdragonGameSuperResolution1:
                        var descriptor = outputResolutionDescriptor;
                        descriptor.clearBuffer = false;
                        descriptor.discardBuffer = false;
                        descriptor.name = "Conifer | Upscaler | Output - Image";
                        var output = renderGraph.CreateTexture(descriptor);
                        var bmp = new RenderGraphUtils.BlitMaterialParameters(resources.cameraColor, output, _sgsr1Upscale, 0);
                        _sgsr1Upscale.SetVector(ViewportInfoID, new Vector4(1.0f / upscaler.InputResolution.x, 1.0f / upscaler.InputResolution.y, upscaler.InputResolution.x, upscaler.InputResolution.y));
                        _sgsr1Upscale.SetFloat(EdgeSharpnessID, upscaler.sharpness + 1);
                        renderGraph.AddBlitPass(bmp, passName: "Conifer | Upscaler | Snapdragon Game Super Resolution 1");
                        resources.cameraColor = output;
                        break;
                    case Upscaler.Technique.SnapdragonGameSuperResolution2:
                        var importResourceParams = new ImportResourceParams { clearOnFirstUse = false, discardOnLastUse = false };
                        var historyTexture = renderGraph.ImportTexture(_history, importResourceParams);
                        descriptor = outputResolutionDescriptor;
                        descriptor.clearBuffer = false;
                        descriptor.discardBuffer = false;
                        descriptor.name = "Conifer | Upscaler | Output - Image";
                        if (upscaler.sgsrMethod != Upscaler.SgsrMethod.Fragment2Pass)
                            descriptor.enableRandomWrite = true;
                        output = renderGraph.CreateTexture(descriptor);
                        descriptor = renderResolutionDescriptor;
                        descriptor.format = GraphicsFormat.R16G16B16A16_SFloat;
                        descriptor.clearBuffer = false;
                        descriptor.discardBuffer = false;
                        descriptor.name = "Conifer | Upscaler | Motion Depth Alpha - Image";
                        if (upscaler.sgsrMethod != Upscaler.SgsrMethod.Fragment2Pass)
                            descriptor.enableRandomWrite = true;
                        var tempMotionDepthAlpha0 = renderGraph.CreateTexture(descriptor);
                        var current = cameraData.GetViewMatrix() * cameraData.GetProjectionMatrix();
                        float vpDiff = 0;
                        for (var i = 0; i < 4; i++) for (var j = 0; j < 4; j++) vpDiff += Math.Abs(current[i, j] - _previousMatrix[i, j]);
                        _previousMatrix = current;
                        switch (upscaler.sgsrMethod) {
                            case Upscaler.SgsrMethod.Compute3Pass:
                                var tempMotionDepthAlpha1 = renderGraph.CreateTexture(tempMotionDepthAlpha0);
                                var lumaHistory = renderGraph.ImportTexture(_lumaHistory, importResourceParams);
                                var tempLuma0 = renderGraph.CreateTexture(lumaHistory);
                                var tempLuma1 = renderGraph.CreateTexture(lumaHistory);
                                descriptor = renderGraph.GetTextureDesc(historyTexture);
                                descriptor.enableRandomWrite = true;
                                descriptor.name = "Conifer | Upscaler | Next History - Image";
                                var tempHistory0 = renderGraph.CreateTexture(descriptor);
                                using (var builder = renderGraph.AddComputePass<Sgsr2C3PassData>("Conifer | Upscaler | Snapdragon Game Super Resolution 2 - Compute 3 Pass", out var data))
                                {
                                    data.Output = upscaler.OutputResolution;
                                    data.Input = upscaler.InputResolution;
                                    data.Jitter = upscaler.Jitter;
                                    data.CameraIsSame = vpDiff < 1e-5;
                                    data.FOV = cameraData.camera.fieldOfView;
                                    data.ShouldReset = upscaler.NativeInterface.ShouldResetHistory;
                                    data.OutputThreadGroups = Vector3Int.CeilToInt(new Vector3(upscaler.OutputResolution.x, upscaler.OutputResolution.y, 1) / 8);
                                    data.InputThreadGroups = Vector3Int.CeilToInt(new Vector3(upscaler.InputResolution.x, upscaler.InputResolution.y, 1) / 8);
                                    data.ConvertShader = _sgsr2C3Convert;
                                    data.ActivateShader = _sgsr2C3Activate;
                                    data.UpscaleShader = _sgsr2C3Upscale;
                                    data.OutputTexture = output;
                                    data.InputTexture = resources.cameraColor;
                                    data.HistoryTexture = historyTexture;
                                    data.TempHistoryTexture = tempHistory0;
                                    data.TempMotionDepthAlphaTexture0 = tempMotionDepthAlpha0;
                                    data.TempMotionDepthAlphaTexture1 = tempMotionDepthAlpha1;
                                    data.LumaTexture = lumaHistory;
                                    data.TempLumaTexture0 = tempLuma0;
                                    data.TempLumaTexture1 = tempLuma1;
                                    builder.UseTexture(output, AccessFlags.Write);
                                    builder.UseTexture(resources.cameraColor);
                                    builder.UseTexture(historyTexture);
                                    builder.UseTexture(tempHistory0, AccessFlags.Write);
                                    builder.UseTexture(tempMotionDepthAlpha0, AccessFlags.ReadWrite | AccessFlags.Discard);
                                    builder.UseTexture(tempMotionDepthAlpha1, AccessFlags.ReadWrite | AccessFlags.Discard);
                                    builder.UseTexture(lumaHistory, AccessFlags.ReadWrite);
                                    builder.UseTexture(tempLuma0, AccessFlags.ReadWrite | AccessFlags.Discard);
                                    builder.UseTexture(tempLuma1, AccessFlags.ReadWrite);
                                    builder.UseTexture(resources.motionVectorColor);
                                    builder.UseTexture(resources.cameraDepth);
                                    builder.SetRenderFunc((Sgsr2C3PassData passData, ComputeGraphContext context) => ExecuteSgsr2C3(passData, context));
                                    builder.AllowGlobalStateModification(true);
                                }
                                renderGraph.AddCopyPass(tempHistory0, historyTexture, passName: "Conifer | Upscaler | Snapdragon Game Super Resolution 2 - Copy Next History To Current History");
                                renderGraph.AddCopyPass(tempLuma1, lumaHistory, passName: "Conifer | Upscaler | Snapdragon Game Super Resolution 2 - Copy Next LumaHistory To Current LumaHistory");
                                break;
                            case Upscaler.SgsrMethod.Compute2Pass:
                                lumaHistory = renderGraph.ImportTexture(_lumaHistory, importResourceParams);
                                descriptor = renderGraph.GetTextureDesc(historyTexture);
                                descriptor.enableRandomWrite = true;
                                descriptor.name = "Conifer | Upscaler | Next History - Image";
                                tempHistory0 = renderGraph.CreateTexture(descriptor);
                                using (var builder = renderGraph.AddComputePass<Sgsr2C2PassData>("Conifer | Upscaler | Snapdragon Game Super Resolution 2 - Compute 2 Pass", out var data))
                                {
                                    data.Output = upscaler.OutputResolution;
                                    data.Input = upscaler.InputResolution;
                                    data.Jitter = upscaler.Jitter;
                                    data.CameraIsSame = vpDiff < 1e-5;
                                    data.FOV = cameraData.camera.fieldOfView;
                                    data.ShouldReset = upscaler.NativeInterface.ShouldResetHistory;
                                    data.OutputThreadGroups = Vector3Int.CeilToInt(new Vector3(upscaler.OutputResolution.x, upscaler.OutputResolution.y, 1) / 8);
                                    data.InputThreadGroups = Vector3Int.CeilToInt(new Vector3(upscaler.InputResolution.x, upscaler.InputResolution.y, 1) / 8);
                                    data.ConvertShader = _sgsr2C2Convert;
                                    data.UpscaleShader = _sgsr2C2Upscale;
                                    data.OutputTexture = output;
                                    data.InputTexture = resources.cameraColor;
                                    data.HistoryTexture = historyTexture;
                                    data.TempHistoryTexture = tempHistory0;
                                    data.TempMotionDepthAlphaTexture = tempMotionDepthAlpha0;
                                    data.TempLumaTexture = lumaHistory;
                                    builder.UseTexture(output, AccessFlags.Write);
                                    builder.UseTexture(resources.cameraColor);
                                    builder.UseTexture(historyTexture);
                                    builder.UseTexture(tempHistory0, AccessFlags.Write);
                                    builder.UseTexture(tempMotionDepthAlpha0, AccessFlags.ReadWrite | AccessFlags.Discard);
                                    builder.UseTexture(lumaHistory, AccessFlags.ReadWrite);
                                    builder.UseTexture(resources.motionVectorColor);
                                    builder.UseTexture(resources.cameraDepth);
                                    builder.SetRenderFunc((Sgsr2C2PassData passData, ComputeGraphContext context) => ExecuteSgsr2C2(passData, context));
                                    builder.AllowGlobalStateModification(true);
                                }
                                renderGraph.AddCopyPass(tempHistory0, historyTexture, passName: "Conifer | Upscaler | Snapdragon Game Super Resolution 2 - Copy Next History To Current History");
                                break;
                            case Upscaler.SgsrMethod.Fragment2Pass:
                                using (var builder = renderGraph.AddUnsafePass<Sgsr2F2PassData>("Conifer | Upscaler | Snapdragon Game Super Resolution 2 - Fragment 2 Pass", out var data))
                                {
                                    data.Output = upscaler.OutputResolution;
                                    data.Input = new Vector2Int(renderResolutionDescriptor.width, renderResolutionDescriptor.height);
                                    data.Jitter = upscaler.Jitter;
                                    data.CameraIsSame = vpDiff < 1e-5;
                                    data.FOV = cameraData.camera.fieldOfView;
                                    data.ShouldReset = upscaler.NativeInterface.ShouldResetHistory;
                                    data.ConvertMaterial = _sgsr2F2Convert;
                                    data.UpscaleMaterial = _sgsr2F2Upscale;
                                    data.OutputTexture = output;
                                    data.InputTexture = resources.cameraColor;
                                    data.HistoryTexture = historyTexture;
                                    data.MotionDepthAlphaTexture = tempMotionDepthAlpha0;
                                    builder.UseTexture(output, AccessFlags.Write);
                                    builder.UseTexture(resources.cameraColor);
                                    builder.UseTexture(historyTexture, AccessFlags.ReadWrite);
                                    builder.UseTexture(tempMotionDepthAlpha0, AccessFlags.ReadWrite | AccessFlags.Discard);
                                    builder.UseTexture(resources.motionVectorColor);
                                    builder.UseTexture(resources.cameraDepth);
                                    builder.SetRenderFunc((Sgsr2F2PassData passData, UnsafeGraphContext context) => ExecuteSgsr2F2(passData, context));
                                }
                                renderGraph.AddCopyPass(output, historyTexture, passName: "Conifer | Upscaler | Snapdragon Game Super Resolution 2 - Copy Upscaled Output To History");
                                break;
                            default: throw new ArgumentOutOfRangeException(nameof(upscaler.sgsrMethod), upscaler.sgsrMethod, null);
                        }
                        resources.cameraColor = output;
                        break;
                    case Upscaler.Technique.DeepLearningSuperSampling or Upscaler.Technique.FidelityFXSuperResolution or Upscaler.Technique.XeSuperSampling:
                        importResourceParams = new ImportResourceParams{ clearOnFirstUse = false, discardOnLastUse = false };
                        var depth = renderGraph.ImportTexture(_depth, importResourceParams);
                        var motion = renderGraph.ImportTexture(_motion, importResourceParams);
                        var color = renderGraph.ImportTexture(_color, importResourceParams);
                        var opaque = TextureHandle.nullHandle;
                        var reactive = TextureHandle.nullHandle;
                        output = renderGraph.ImportTexture(_output, importResourceParams);
                        renderGraph.AddBlitPass(resources.cameraDepth, depth, Vector2.one, Vector2.zero, passName: "Conifer | Upscaler | Blit Depth Input");
                        renderGraph.AddCopyPass(resources.motionVectorColor, motion, passName: "Conifer | Upscaler | Copy Motion Vectors");
                        renderGraph.AddCopyPass(resources.cameraColor, color, passName: "Conifer | Upscaler | Copy Camera Color");
                        if (upscaler.autoReactive && upscaler.technique == Upscaler.Technique.FidelityFXSuperResolution)
                        {
                            opaque = renderGraph.ImportTexture(_opaque, importResourceParams);
                            reactive = renderGraph.ImportTexture(_reactive, importResourceParams);
                            renderGraph.AddCopyPass(resources.cameraOpaqueTexture, opaque, passName: "Conifer | Upscaler | Copy Opaque Color");
                        }

                        var name = upscaler.technique switch
                        {
                            Upscaler.Technique.FidelityFXSuperResolution => "FidelityFX Super Resolution",
                            Upscaler.Technique.XeSuperSampling => "Xe Super Sampling",
                            Upscaler.Technique.DeepLearningSuperSampling => "Deep Learning Super Sampling",
                            Upscaler.Technique.None or Upscaler.Technique.SnapdragonGameSuperResolution1 or Upscaler.Technique.SnapdragonGameSuperResolution2 => throw new ArgumentOutOfRangeException(nameof(upscaler.technique), upscaler.technique, null),
                            _ => throw new ArgumentOutOfRangeException(nameof(upscaler.technique), upscaler.technique, null)
                        };
                        using (var builder = renderGraph.AddComputePass<NativeUpscalerData>("Conifer | Upscaler | " + name, out var data))
                        {
                            data.Upscaler = upscaler;
                            builder.UseTexture(depth, AccessFlags.Read | AccessFlags.Discard);
                            builder.UseTexture(motion, AccessFlags.Read | AccessFlags.Discard);
                            builder.UseTexture(color, AccessFlags.Read | AccessFlags.Discard);
                            builder.UseTexture(output, AccessFlags.Write);
                            if (upscaler.autoReactive && upscaler.technique == Upscaler.Technique.FidelityFXSuperResolution)
                            {
                                builder.UseTexture(opaque, AccessFlags.Read | AccessFlags.Discard);
                                builder.UseTexture(reactive, AccessFlags.ReadWrite);
                            }
                            builder.SetRenderFunc((NativeUpscalerData passData, ComputeGraphContext context) => passData.Upscaler.NativeInterface.Upscale(context.cmd, passData.Upscaler, upscaler.InputResolution));
                        }
                        resources.cameraColor = output;
                        break;
                }
            }

            private static void ExecuteSgsr2C3(Sgsr2C3PassData data, ComputeGraphContext context)
            {
                context.cmd.SetGlobalVector(RenderSizeID, (Vector2)data.Input);
                context.cmd.SetGlobalVector(OutputSizeID, (Vector2)data.Output);
                context.cmd.SetGlobalVector(RenderSizeRcpID, Vector2.one / data.Input);
                context.cmd.SetGlobalVector(OutputSizeRcpID, Vector2.one / data.Output);
                context.cmd.SetGlobalVector(JitterOffsetID, data.Jitter);
                context.cmd.SetGlobalVector(ScaleRatioID, new Vector4((float)data.Output.x / data.Input.x, Mathf.Min(20.0f, Mathf.Pow((float)data.Output.x * data.Output.y / (data.Input.x * data.Input.y), 3.0f))));
                context.cmd.SetGlobalFloat(PreExposureID, 1.0f);
                context.cmd.SetGlobalFloat(CameraFovAngleHorID, Mathf.Tan(Mathf.Deg2Rad * (data.FOV / 2)) * data.Input.x / data.Input.y);
                context.cmd.SetGlobalFloat(MinLerpContributionID, data.CameraIsSame ? 0.3f : 0.0f);
                context.cmd.SetGlobalFloat(ResetID, Convert.ToSingle(data.ShouldReset));
                context.cmd.SetGlobalInt(SameCameraID, data.CameraIsSame ? 1 : 0);
                context.cmd.SetGlobalTexture(ColorID, data.InputTexture);
                context.cmd.SetGlobalTexture(MotionDepthAlphaBufferSinkID, data.TempMotionDepthAlphaTexture0);
                context.cmd.SetGlobalTexture(LumaSinkID, data.TempLumaTexture0);
                context.cmd.DispatchCompute(data.ConvertShader, 0, data.InputThreadGroups.x, data.InputThreadGroups.y, data.InputThreadGroups.z);
                context.cmd.SetGlobalTexture(MotionDepthAlphaBufferSinkID, data.TempMotionDepthAlphaTexture1);
                context.cmd.SetGlobalTexture(MotionDepthAlphaBufferID, data.TempMotionDepthAlphaTexture0);
                context.cmd.SetGlobalTexture(LumaID, data.TempLumaTexture0);
                context.cmd.SetGlobalTexture(LumaSinkID, data.TempLumaTexture1);
                context.cmd.SetGlobalTexture(LumaHistoryID, data.LumaTexture);
                context.cmd.DispatchCompute(data.ActivateShader, 0, data.InputThreadGroups.x, data.InputThreadGroups.y, data.InputThreadGroups.z);
                context.cmd.SetGlobalTexture(MotionDepthAlphaBufferID, data.TempMotionDepthAlphaTexture1);
                context.cmd.SetGlobalTexture(NextHistoryID, data.TempHistoryTexture);
                context.cmd.SetGlobalTexture(HistoryID, data.HistoryTexture);
                context.cmd.SetGlobalTexture(OutputSinkID, data.OutputTexture);
                context.cmd.DispatchCompute(data.UpscaleShader, 0, data.OutputThreadGroups.x, data.OutputThreadGroups.y, data.OutputThreadGroups.z);
            }

            private static void ExecuteSgsr2C2(Sgsr2C2PassData data, ComputeGraphContext context)
            {
                context.cmd.SetGlobalVector(RenderSizeID, (Vector2)data.Input);
                context.cmd.SetGlobalVector(OutputSizeID, (Vector2)data.Output);
                context.cmd.SetGlobalVector(RenderSizeRcpID, Vector2.one / data.Input);
                context.cmd.SetGlobalVector(OutputSizeRcpID, Vector2.one / data.Output);
                context.cmd.SetGlobalVector(JitterOffsetID, data.Jitter);
                context.cmd.SetGlobalVector(ScaleRatioID, new Vector4((float)data.Output.x / data.Input.x, Mathf.Min(20.0f, Mathf.Pow((float)data.Output.x * data.Output.y / (data.Input.x * data.Input.y), 3.0f))));
                context.cmd.SetGlobalFloat(PreExposureID, 1.0f);
                context.cmd.SetGlobalFloat(CameraFovAngleHorID, Mathf.Tan(Mathf.Deg2Rad * (data.FOV / 2)) * data.Input.x / data.Input.y);
                context.cmd.SetGlobalFloat(MinLerpContributionID, data.CameraIsSame ? 0.3f : 0.0f);
                context.cmd.SetGlobalFloat(ResetID, Convert.ToSingle(data.ShouldReset));
                context.cmd.SetGlobalInt(SameCameraID, data.CameraIsSame ? 1 : 0);
                context.cmd.SetGlobalTexture(ColorID, data.InputTexture);
                context.cmd.SetGlobalTexture(MotionDepthAlphaBufferSinkID, data.TempMotionDepthAlphaTexture);
                context.cmd.SetGlobalTexture(LumaSinkID, data.TempLumaTexture);
                context.cmd.DispatchCompute(data.ConvertShader, 0, data.InputThreadGroups.x, data.InputThreadGroups.y, data.InputThreadGroups.z);
                context.cmd.SetGlobalTexture(MotionDepthAlphaBufferID, data.TempMotionDepthAlphaTexture);
                context.cmd.SetGlobalTexture(LumaID, data.TempLumaTexture);
                context.cmd.SetGlobalTexture(NextHistoryID, data.TempHistoryTexture);
                context.cmd.SetGlobalTexture(HistoryID, data.HistoryTexture);
                context.cmd.SetGlobalTexture(OutputSinkID, data.OutputTexture);
                context.cmd.DispatchCompute(data.UpscaleShader, 0, data.OutputThreadGroups.x, data.OutputThreadGroups.y, data.OutputThreadGroups.z);
            }

            private static void ExecuteSgsr2F2(Sgsr2F2PassData data, UnsafeGraphContext context)
            {
                context.cmd.SetGlobalVector(RenderSizeID, (Vector2)data.Input);
                context.cmd.SetGlobalVector(OutputSizeID, (Vector2)data.Output);
                context.cmd.SetGlobalVector(RenderSizeRcpID, Vector2.one / data.Input);
                context.cmd.SetGlobalVector(OutputSizeRcpID, Vector2.one / data.Output);
                context.cmd.SetGlobalVector(JitterOffsetID, data.Jitter);
                context.cmd.SetGlobalVector(ScaleRatioID, new Vector4((float)data.Output.x / data.Input.x, Mathf.Min(20.0f, Mathf.Pow((float)data.Output.x * data.Output.y / (data.Input.x * data.Input.y), 3.0f))));
                context.cmd.SetGlobalFloat(PreExposureID, 1.0f);
                context.cmd.SetGlobalFloat(CameraFovAngleHorID, Mathf.Tan(Mathf.Deg2Rad * (data.FOV / 2)) * data.Input.x / data.Input.y);
                context.cmd.SetGlobalFloat(MinLerpContributionID, data.CameraIsSame ? 0.3f : 0.0f);
                context.cmd.SetGlobalFloat(ResetID, Convert.ToSingle(data.ShouldReset));
                context.cmd.SetGlobalInt(SameCameraID, data.CameraIsSame ? 1 : 0);
                context.cmd.SetGlobalVector("_BlitScaleBias", new Vector4(1, 1, 0, 0));
                context.cmd.SetRenderTarget(data.MotionDepthAlphaTexture);
                context.cmd.DrawProcedural(Matrix4x4.identity, data.ConvertMaterial, 0, MeshTopology.Triangles, 3);
                context.cmd.SetGlobalTexture(MotionDepthAlphaBufferID, data.MotionDepthAlphaTexture);
                context.cmd.SetGlobalTexture(HistoryID, data.HistoryTexture);
                context.cmd.SetGlobalTexture(BlitID, data.InputTexture);
                context.cmd.SetRenderTarget(data.OutputTexture);
                context.cmd.DrawProcedural(Matrix4x4.identity, data.UpscaleMaterial, 0, MeshTopology.Triangles, 3);
            }

            [Obsolete("This rendering path is for compatibility mode only (when Render Graph is disabled). Use Render Graph API instead.", false)]
#endif
            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
                var cb = CommandBufferPool.Get("Upscale");
                switch (upscaler.technique)
                {
                    case Upscaler.Technique.SnapdragonGameSuperResolution1:
                        cb.SetRenderTarget(_output);
                        cb.SetGlobalTexture(BlitID, renderingData.cameraData.renderer.cameraColorTargetHandle);
                        cb.SetGlobalVector(BlitScaleBiasID, new Vector4(1, 1, 0, 0));
                        cb.SetViewProjectionMatrices(LookAt, Ortho);
                        cb.SetGlobalVector(ViewportInfoID, new Vector4(1.0f / upscaler.OutputResolution.x, 1.0f / upscaler.OutputResolution.y, upscaler.InputResolution.x, upscaler.InputResolution.y));
                        cb.SetGlobalFloat(EdgeSharpnessID, upscaler.sharpness + 1);
                        cb.DrawProcedural(Matrix4x4.identity, _sgsr1Upscale, 0, MeshTopology.Triangles, 3);
                        break;
                    case Upscaler.Technique.SnapdragonGameSuperResolution2:
                        var current = renderingData.cameraData.camera.cameraToWorldMatrix * renderingData.cameraData.camera.projectionMatrix;
                        float vpDiff = 0;
                        for (var i = 0; i < 4; i++) for (var j = 0; j < 4; j++) vpDiff += Math.Abs(current[i, j] - _previousMatrix[i, j]);
                        _previousMatrix = current;
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
                        descriptor.enableRandomWrite = true;
                        cb.GetTemporaryRT(TempMotionDepthAlphaBuffer0ID, descriptor);
                        switch (upscaler.sgsrMethod)
                        {
                            case Upscaler.SgsrMethod.Compute3Pass:
                            {
                                descriptor.enableRandomWrite = true;
                                cb.GetTemporaryRT(TempMotionDepthAlphaBuffer1ID, descriptor);
                                descriptor = _lumaHistory.rt.descriptor;
                                descriptor.enableRandomWrite = true;
                                cb.GetTemporaryRT(TempLuma0ID, descriptor);
                                cb.GetTemporaryRT(TempLuma1ID, descriptor);
                                cb.SetGlobalTexture(ColorID, renderingData.cameraData.renderer.cameraColorTargetHandle);
                                cb.SetGlobalTexture(MotionDepthAlphaBufferSinkID, TempMotionDepthAlphaBuffer0ID);
                                cb.SetGlobalTexture(LumaSinkID, TempLuma0ID);
                                cb.DispatchCompute(_sgsr2C3Convert, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                cb.SetGlobalTexture(MotionDepthAlphaBufferSinkID, TempMotionDepthAlphaBuffer1ID);
                                cb.SetGlobalTexture(MotionDepthAlphaBufferID, TempMotionDepthAlphaBuffer0ID);
                                cb.SetGlobalTexture(LumaID, TempLuma0ID);
                                cb.SetGlobalTexture(LumaSinkID, TempLuma1ID);
                                cb.SetGlobalTexture(LumaHistoryID, _lumaHistory);
                                cb.DispatchCompute(_sgsr2C3Activate, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                cb.SetGlobalTexture(MotionDepthAlphaBufferID, TempMotionDepthAlphaBuffer1ID);
                                cb.SetGlobalTexture(HistoryID, _history);
                                cb.SetGlobalTexture(OutputSinkID, _output);
                                threadGroups = Vector3Int.CeilToInt(new Vector3(upscaler.OutputResolution.x, upscaler.OutputResolution.y, 1) / 8);
                                cb.DispatchCompute(_sgsr2C3Upscale, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                cb.CopyTexture(NextHistoryID, _history);
                                cb.CopyTexture(TempLuma1ID, _lumaHistory);
                                cb.ReleaseTemporaryRT(TempMotionDepthAlphaBuffer1ID);
                                cb.ReleaseTemporaryRT(TempLuma0ID);
                                cb.ReleaseTemporaryRT(TempLuma1ID);
                                break;
                            }
                            case Upscaler.SgsrMethod.Compute2Pass:
                            {
                                descriptor = _lumaHistory.rt.descriptor;
                                descriptor.enableRandomWrite = true;
                                cb.GetTemporaryRT(TempLuma0ID, descriptor);
                                cb.SetGlobalTexture(ColorID, renderingData.cameraData.renderer.cameraColorTargetHandle);
                                cb.SetGlobalTexture(MotionDepthAlphaBufferSinkID, TempMotionDepthAlphaBuffer0ID);
                                cb.SetGlobalTexture(LumaSinkID, TempLuma0ID);
                                cb.DispatchCompute(_sgsr2C2Convert, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                threadGroups = Vector3Int.CeilToInt(new Vector3(upscaler.OutputResolution.x, upscaler.OutputResolution.y, 1) / 8);
                                cb.SetGlobalTexture(MotionDepthAlphaBufferID, TempMotionDepthAlphaBuffer0ID);
                                cb.SetGlobalTexture(LumaID, TempLuma0ID);
                                cb.SetGlobalTexture(HistoryID, _history);
                                cb.SetGlobalTexture(OutputSinkID, _output);
                                cb.DispatchCompute(_sgsr2C2Upscale, 0, threadGroups.x, threadGroups.y, threadGroups.z);
                                cb.CopyTexture(NextHistoryID, _history);
                                cb.ReleaseTemporaryRT(TempLuma0ID);
                                break;
                            }
                            case Upscaler.SgsrMethod.Fragment2Pass:
                            {
                                cb.SetGlobalVector(BlitScaleBiasID, new Vector4(1, 1, 0, 0));
                                cb.SetViewProjectionMatrices(LookAt, Ortho);
                                cb.SetRenderTarget(TempMotionDepthAlphaBuffer0ID);
                                cb.DrawProcedural(Matrix4x4.identity, _sgsr2F2Convert, 0, MeshTopology.Triangles, 3);
                                cb.SetGlobalTexture(MotionDepthAlphaBufferID, TempMotionDepthAlphaBuffer0ID);
                                cb.SetGlobalTexture(HistoryID, _history);
                                cb.SetGlobalTexture(BlitID, renderingData.cameraData.renderer.cameraColorTargetHandle);
                                cb.SetRenderTarget(_output);
                                cb.DrawProcedural(Matrix4x4.identity, _sgsr2F2Upscale, 0, MeshTopology.Triangles, 3);
                                cb.Blit(_output, _history);
                                break;
                            }
                            default: throw new ArgumentOutOfRangeException(nameof(upscaler.sgsrMethod), upscaler.sgsrMethod, null);
                        }
                        cb.ReleaseTemporaryRT(NextHistoryID);
                        cb.ReleaseTemporaryRT(TempMotionDepthAlphaBuffer0ID);
                        break;
                    case Upscaler.Technique.FidelityFXSuperResolution:
                        if (upscaler.autoReactive) cb.Blit(Shader.GetGlobalTexture(OpaqueID) ?? Texture2D.blackTexture, _opaque);
                        BlitDepth(cb, renderingData.cameraData.renderer.cameraDepthTargetHandle, _depth, (Vector2)upscaler.InputResolution / upscaler.OutputResolution);
                        cb.Blit(Shader.GetGlobalTexture(MotionID) ?? Texture2D.blackTexture, _motion);
                        upscaler.NativeInterface.Upscale(cb, upscaler, renderingData.cameraData.renderer.cameraColorTargetHandle.GetScaledSize());
                        break;
                    case Upscaler.Technique.DeepLearningSuperSampling:
                    case Upscaler.Technique.XeSuperSampling:
                        cb.Blit(Shader.GetGlobalTexture(MotionID) ?? Texture2D.blackTexture, _motion);
                        upscaler.NativeInterface.Upscale(cb, upscaler, renderingData.cameraData.renderer.cameraColorTargetHandle.GetScaledSize());
                        break;
                    case Upscaler.Technique.None:
                    default:  throw new ArgumentOutOfRangeException(nameof(upscaler.technique), upscaler.technique, null);
                }
                cb.CopyTexture(_output, renderingData.cameraData.renderer.cameraColorTargetHandle);
                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);
                var args = new object[] { false };
                UseScaling.Invoke(renderingData.cameraData.renderer.cameraColorTargetHandle, args);
                UseScaling.Invoke(renderingData.cameraData.renderer.cameraDepthTargetHandle, args);
            }

            public void Dispose()
            {
                Disposed = true;
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
                if (_sgsr2C3Convert is not null) {
                    Resources.UnloadAsset(_sgsr2C3Convert);
                    _sgsr2C3Convert = null;
                }
                if (_sgsr2C3Activate is not null) {
                    Resources.UnloadAsset(_sgsr2C3Activate);
                    _sgsr2C3Activate = null;
                }
                if (_sgsr2C3Upscale is not null) {
                    Resources.UnloadAsset(_sgsr2C3Upscale);
                    _sgsr2C3Upscale = null;
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
                _depth?.Release();
                _depth = null;
                _color?.Release();
                _color = null;
            }
        }

#if !UNITY_6000_0_OR_NEWER
        private class SetupGenerate : ScriptableRenderPass
        {
            public SetupGenerate() => renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing;

#if UNITY_6000_0_OR_NEWER
            [Obsolete("This rendering path is for compatibility mode only (when Render Graph is disabled). Use Render Graph API instead.", false)]
#endif
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

#if UNITY_6000_0_OR_NEWER
            [Obsolete("This rendering path is for compatibility mode only (when Render Graph is disabled). Use Render Graph API instead.", false)]
#endif
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

#if UNITY_6000_0_OR_NEWER
            [Obsolete("This rendering path is for compatibility mode only (when Render Graph is disabled). Use Render Graph API instead.", false)]
#endif
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
                    , Vector2.one, -upscaler.NativeInterface.EditorOffset / srcRes
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
#endif

        private class RemoveHistoryReset : ScriptableRenderPass
        {
            public RemoveHistoryReset() => renderPassEvent = RenderPassEvent.AfterRendering;

#if UNITY_6000_0_OR_NEWER
            public override void RecordRenderGraph(RenderGraph renderGraph, ContextContainer frameData) => frameData.Get<UniversalCameraData>().camera.GetComponent<Upscaler>().ShouldResetHistory(false);

            [Obsolete("This rendering path is for compatibility mode only (when Render Graph is disabled). Use Render Graph API instead.", false)]
#endif
            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData) => renderingData.cameraData.camera.GetComponent<Upscaler>().ShouldResetHistory(false);
        }

        private bool _renderTargetsUpdated;
        private bool _isResizingThisFrame;
        private readonly SetupUpscale _setupUpscale = new();
        private readonly Upscale _upscale = new();
#if !UNITY_6000_0_OR_NEWER
        private readonly SetupGenerate _setupGenerate = new ();
        private readonly Generate _generate = new();
#endif
        private readonly RemoveHistoryReset _removeHistoryReset = new();

        public override void Create()
        {
            name = "Upscaler";
            _depthBlitMaterial = new Material(Shader.Find("Hidden/BlitDepth"));
        }

        public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
        {
            var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
            if (!Application.isPlaying || renderingData.cameraData.cameraType != CameraType.Game || upscaler is null || !upscaler.isActiveAndEnabled) return;

            if (upscaler.forceHistoryResetEveryFrame || renderingData.cameraData.camera.GetUniversalAdditionalCameraData().resetHistory) upscaler.ResetHistory();
            _isResizingThisFrame = upscaler.OutputResolution != upscaler.PreviousOutputResolution;
#if !UNITY_6000_0_OR_NEWER
            var previousFrameGeneration = upscaler.PreviousFrameGeneration;
#endif
            _renderTargetsUpdated = upscaler.technique != upscaler.PreviousTechnique;
            var requiresFullReset = upscaler.PreviousTechnique != upscaler.technique || upscaler.PreviousSgsrMethod != upscaler.sgsrMethod || upscaler.PreviousAutoReactive != upscaler.autoReactive || upscaler.PreviousQuality != upscaler.quality || (!_isResizingThisFrame && upscaler.PreviousPreviousOutputResolution != upscaler.PreviousOutputResolution);
            var inputResolutionChanged = upscaler.InputResolution != upscaler.PreviousInputResolution;
            upscaler.CurrentStatus = upscaler.ApplySettings();
            if (Upscaler.Failure(upscaler.CurrentStatus))
            {
                if (upscaler.ErrorCallback is not null)
                {
                    upscaler.ErrorCallback(upscaler.CurrentStatus, upscaler.NativeInterface.GetStatusMessage());
                    upscaler.CurrentStatus = upscaler.ApplySettings();
                    if (Upscaler.Failure(upscaler.CurrentStatus)) Debug.LogError("The registered error handler failed to rectify the following error.");
                }
                Debug.LogWarning(upscaler.NativeInterface.GetStatus() + " | " + upscaler.NativeInterface.GetStatusMessage());
                upscaler.technique = Upscaler.Technique.None;
                upscaler.quality = Upscaler.Quality.Auto;
                upscaler.ApplySettings(true);
            }
            if (requiresFullReset || inputResolutionChanged || _upscale.Disposed)
            {
                _upscale.Cleanup(upscaler, !requiresFullReset);
                _renderTargetsUpdated |= _upscale.Initialize(upscaler, renderingData.cameraData.cameraTargetDescriptor);
            }
#if UNITY_6000_0_OR_NEWER
            if (_renderTargetsUpdated && upscaler.RequiresNativePlugin()) _upscale.SetImages(upscaler);
#endif
            if (!_isResizingThisFrame && upscaler.technique != Upscaler.Technique.None)
            {
                _setupUpscale.ConfigureInput(ScriptableRenderPassInput.None);
                renderer.EnqueuePass(_setupUpscale);
                _upscale.ConfigureInput((upscaler.IsTemporal() ? ScriptableRenderPassInput.Motion | ScriptableRenderPassInput.Depth : ScriptableRenderPassInput.None) | ScriptableRenderPassInput.Color);
                renderer.EnqueuePass(_upscale);
            }
#if !UNITY_6000_0_OR_NEWER
            if (!_isResizingThisFrame && upscaler.frameGeneration && previousFrameGeneration && NativeInterface.GetBackBufferFormat() != GraphicsFormat.None)
            {
                _setupGenerate.ConfigureInput(ScriptableRenderPassInput.Motion);
                renderer.EnqueuePass(_setupGenerate);
                _generate.ConfigureInput(ScriptableRenderPassInput.None);
                renderer.EnqueuePass(_generate);
                VolumeManager.instance.stack.GetComponent<MotionBlur>().intensity.value /= 2.0f;
            }
#endif
            _removeHistoryReset.ConfigureInput(ScriptableRenderPassInput.None);
            renderer.EnqueuePass(_removeHistoryReset);
        }

#if UNITY_6000_0_OR_NEWER
        [Obsolete("This rendering path is for compatibility mode only (when Render Graph is disabled). Use Render Graph API instead.", false)]
#endif
        public override void SetupRenderPasses(ScriptableRenderer renderer, in RenderingData renderingData)
        {
            var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
            if (!Application.isPlaying || renderingData.cameraData.cameraType != CameraType.Game || upscaler is null || !upscaler.isActiveAndEnabled || upscaler.technique == Upscaler.Technique.None) return;
            if (!_isResizingThisFrame)
            {
                var args = new object[] { true };
                UseScaling.Invoke(_upscale._color, args);
                UseScaling.Invoke(_upscale._depth, args);
                args = new object[] { (Vector2)upscaler.InputResolution / upscaler.OutputResolution };
                ScaleFactor.Invoke(_upscale._color, args);
                ScaleFactor.Invoke(_upscale._depth, args);
            }
            renderer.ConfigureCameraTarget(_upscale._color, _upscale._depth);
            if (_renderTargetsUpdated && upscaler.RequiresNativePlugin()) _upscale.SetImages(upscaler);
        }

        protected override void Dispose(bool disposing)
        {
            if (!disposing) return;
            _upscale.Dispose();
#if !UNITY_6000_0_OR_NEWER
            _generate.Dispose();
#endif
        }
    }
}