/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

/**************************************************
 * Upscaler v1.1.3                                *
 * See the UserManual.pdf for more information    *
 **************************************************/

using System;
using System.Reflection;
using System.Runtime.InteropServices;
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
        private static Material _blitMaterial;
        private static readonly int BlitID = Shader.PropertyToID("_MainTex");
        private static readonly Matrix4x4 Ortho = Matrix4x4.Ortho(-1, 1, 1, -1, 1, -1);
        private static readonly Matrix4x4 LookAt = Matrix4x4.LookAt(Vector3.back, Vector3.forward, Vector3.up);

        private static void BlitDepth(CommandBuffer cb, RenderTargetIdentifier src, RenderTargetIdentifier dst)
        {
            cb.SetRenderTarget(dst);
            cb.SetGlobalTexture(BlitID, src);
            cb.SetProjectionMatrix(Ortho);
            cb.SetViewMatrix(LookAt);
            cb.DrawMesh(_triangle, Matrix4x4.identity, _blitMaterial);
        }

        private static RTHandle _cameraRenderResolutionColorTarget;
        private static RTHandle _cameraRenderResolutionDepthTarget;
        private static RTHandle _cameraOutputResolutionColorTarget;
        private static RTHandle _cameraOutputResolutionDepthTarget;

        private static Upscaler _upscaler;

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
            private static readonly int MotionID = Shader.PropertyToID("_MotionVectorTexture");
            private static Texture _lastMotion;
            private static readonly int OpaqueID = Shader.PropertyToID("_CameraOpaqueTexture");
            private static Texture _lastOpaque;
            private static Upscaler.Technique _lastTechnique;
            private static bool _lastReactive;
            private static IntPtr _depthPtr;
            private static IntPtr _colorPtr;
            private static RTHandle _output;
            private static IntPtr _outputPtr;
            private static RTHandle _reactive;
            private static IntPtr _reactivePtr;
            private static bool _needsUpdate;

            internal static Material SgsrMaterial;
            internal static Material SgsrConvertMaterial;
            internal static Material SgsrUpscaleMaterial;
            private static readonly int ViewportInfoID = Shader.PropertyToID("Conifer_Upscaler_ViewportInfo");
            private static readonly int EdgeSharpnessID = Shader.PropertyToID("Conifer_Upscaler_EdgeSharpness");

            private static RTHandle _motionDepthClipAlphaBuffer;
            private static RTHandle _previousOutput;
            private static readonly int MotionDepthClipAlphaBufferID = Shader.PropertyToID("Conifer_Upscaler_MotionDepthClipAlphaBuffer");
            private static readonly int PreviousOutputID = Shader.PropertyToID("Conifer_Upscaler_PrevOutput");
            private static readonly int ConiferDepthID = Shader.PropertyToID("Conifer_Upscaler_Depth");
            private static readonly int ConiferMotionID = Shader.PropertyToID("Conifer_Upscaler_Motion");

            public Upscale() => renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing;

            public override void OnCameraSetup(CommandBuffer cmd, ref RenderingData renderingData)
            {
                var cameraTextureDescriptor = renderingData.cameraData.cameraTargetDescriptor;
                cameraTextureDescriptor.width = _upscaler.InputResolution.x;
                cameraTextureDescriptor.height = _upscaler.InputResolution.y;
                var descriptor = cameraTextureDescriptor;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                if (RenderingUtils.ReAllocateIfNeeded(ref _cameraRenderResolutionColorTarget, descriptor, name: "Conifer_CameraColorTarget"))
                {
                    _needsUpdate = true;
                    _colorPtr = _cameraRenderResolutionColorTarget.rt.GetNativeTexturePtr();
                }
                descriptor = cameraTextureDescriptor;
                descriptor.colorFormat = RenderTextureFormat.Depth;
                if (RenderingUtils.ReAllocateIfNeeded(ref _cameraRenderResolutionDepthTarget, descriptor, name: "Conifer_CameraDepthTarget"))
                {
                    _needsUpdate = true;
                    _depthPtr = _cameraRenderResolutionDepthTarget.rt.GetNativeTexturePtr();
                }
                BlitDepth(cmd, Texture2D.blackTexture, _cameraRenderResolutionDepthTarget);

                _cameraOutputResolutionColorTarget = renderingData.cameraData.renderer.cameraColorTargetHandle;
                _cameraOutputResolutionDepthTarget = renderingData.cameraData.renderer.cameraDepthTargetHandle;
                renderingData.cameraData.renderer.ConfigureCameraTarget(_cameraRenderResolutionColorTarget, _cameraRenderResolutionDepthTarget);

                if (_upscaler.technique != Upscaler.Technique.SnapdragonGameSuperResolutionTemporal) return;
                descriptor = renderingData.cameraData.cameraTargetDescriptor;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                RenderingUtils.ReAllocateIfNeeded(ref _previousOutput, descriptor, name: "Conifer_Upscaler_SGSR2HistoryBuffer");
                descriptor = cameraTextureDescriptor;
                descriptor.graphicsFormat = GraphicsFormat.R16G16B16A16_SFloat;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                RenderingUtils.ReAllocateIfNeeded(ref _motionDepthClipAlphaBuffer, descriptor, name: "Conifer_Upscaler_SGSR2MotionDepthClipAlphaBuffer");
                descriptor = cameraTextureDescriptor;
                descriptor.graphicsFormat = GraphicsFormat.None;
                cmd.GetTemporaryRT(ConiferDepthID, descriptor);
                descriptor.graphicsFormat = GraphicsFormat.R16G16_SFloat;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                cmd.GetTemporaryRT(ConiferMotionID, descriptor);
            }

            public override void Configure(CommandBuffer cmd, RenderTextureDescriptor descriptor)
            {
                if (_upscaler.IsSpatial()) return;
                descriptor.enableRandomWrite = true;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                if (RenderingUtils.ReAllocateIfNeeded(ref _output, descriptor, name: "Conifer_UpscalerOutput"))
                {
                    _needsUpdate = true;
                    _outputPtr = _output.rt.GetNativeTexturePtr();
                }

                if (_upscaler.technique == Upscaler.Technique.FidelityFXSuperResolution && _upscaler.useReactiveMask)
                {
                    descriptor.width = _upscaler.InputResolution.x;
                    descriptor.height = _upscaler.InputResolution.y;
                    descriptor.graphicsFormat = GraphicsFormat.R8_UNorm;
                    if (RenderingUtils.ReAllocateIfNeeded(ref _reactive, descriptor, name: "Conifer_UpscalerReactiveMask"))
                    {
                        _needsUpdate = true;
                        _reactivePtr = _reactive.rt.GetNativeTexturePtr();
                    }
                }
                else
                {
                    _reactive?.Release();
                    _reactive = null;
                    _reactivePtr = IntPtr.Zero;
                }

                if (Time.frameCount == 1) return;
                var thisMotion = Shader.GetGlobalTexture(MotionID);
                var thisOpaque = Shader.GetGlobalTexture(OpaqueID);
                if (_needsUpdate || thisMotion != _lastMotion || thisOpaque != _lastOpaque || _upscaler.technique != _lastTechnique || _upscaler.useReactiveMask != _lastReactive)
                    _upscaler.NativeInterface.SetImages(_colorPtr, _depthPtr, thisMotion?.GetNativeTexturePtr() ?? IntPtr.Zero, _outputPtr, _reactivePtr, thisOpaque?.GetNativeTexturePtr() ?? IntPtr.Zero, _upscaler.useReactiveMask);

                _needsUpdate = false;
                _lastMotion = thisMotion;
                _lastOpaque = thisOpaque;
                _lastTechnique = _upscaler.technique;
                _lastReactive = _upscaler.useReactiveMask;
            }

            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var cb = CommandBufferPool.Get("Upscale");
                if (!_upscaler.DisableUpscaling)
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
                            var cameraIsSame = renderingData.cameraData.camera.previousViewProjectionMatrix == renderingData.cameraData.GetViewMatrix() * renderingData.cameraData.GetProjectionMatrix();
                            var clipToPrevClip = renderingData.cameraData.camera.previousViewProjectionMatrix * (renderingData.cameraData.GetProjectionMatrix().inverse * renderingData.cameraData.GetViewMatrix().inverse);
                            cb.SetGlobalVectorArray("Conifer_Upscaler_ClipToPrevClip", new []{clipToPrevClip.GetRow(0), clipToPrevClip.GetRow(1), clipToPrevClip.GetRow(2), clipToPrevClip.GetRow(3)});
                            cb.SetGlobalVector("Conifer_Upscaler_RenderSize", (Vector2)_upscaler.InputResolution);
                            cb.SetGlobalVector("Conifer_Upscaler_OutputSize", (Vector2)_upscaler.OutputResolution);
                            cb.SetGlobalVector("Conifer_Upscaler_RenderSizeRcp", Vector2.one / _upscaler.InputResolution);
                            cb.SetGlobalVector("Conifer_Upscaler_OutputSizeRcp", Vector2.one / _upscaler.OutputResolution);
                            cb.SetGlobalVector("Conifer_Upscaler_JitterOffset", _upscaler.Jitter);
                            cb.SetGlobalVector("Conifer_Upscaler_ScaleRatio", new Vector4((float)_upscaler.OutputResolution.x / _upscaler.InputResolution.x, Mathf.Min(20.0f, Mathf.Pow((float)_upscaler.OutputResolution.x * _upscaler.OutputResolution.y / (_upscaler.InputResolution.x * _upscaler.InputResolution.y), 3.0f))));
                            cb.SetGlobalFloat("Conifer_Upscaler_CameraFovAngleHor", Mathf.Tan(Mathf.Deg2Rad * (renderingData.cameraData.camera.fieldOfView / 2)) * _upscaler.InputResolution.x / _upscaler.InputResolution.y);
                            cb.SetGlobalFloat("Conifer_Upscaler_MinLerpContribution", cameraIsSame ? 0.3f : 0.0f);
                            cb.SetGlobalFloat("Conifer_Upscaler_Reset", 0.0f);
                            cb.SetGlobalInt("Conifer_Upscaler_SameCamera", cameraIsSame ? 1 : 0);
                            cb.SetGlobalVector("Conifer_Upscaler_RenderSize", (Vector2)_upscaler.InputResolution);
                            cb.SetProjectionMatrix(Ortho);
                            cb.SetViewMatrix(LookAt);
                            // BlitDepth(cb, _cameraRenderResolutionDepthTarget, ConiferDepthID);
                            // cb.Blit(MotionID, ConiferMotionID);
                            cb.SetRenderTarget(_motionDepthClipAlphaBuffer);
                            cb.DrawMesh(_triangle, Matrix4x4.identity, SgsrConvertMaterial);
                            cb.SetGlobalTexture(MotionDepthClipAlphaBufferID, _motionDepthClipAlphaBuffer);
                            cb.SetGlobalTexture(PreviousOutputID, _previousOutput);
                            cb.SetGlobalTexture(BlitID, _cameraRenderResolutionColorTarget);
                            cb.SetRenderTarget(_cameraOutputResolutionColorTarget);
                            cb.DrawMesh(_triangle, Matrix4x4.identity, SgsrUpscaleMaterial);
                            cb.CopyTexture(_cameraOutputResolutionColorTarget, _previousOutput);
                            cb.ReleaseTemporaryRT(ConiferDepthID);
                            cb.ReleaseTemporaryRT(ConiferMotionID);
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
                BlitDepth(cb, _cameraRenderResolutionDepthTarget, _cameraOutputResolutionDepthTarget);
                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);
                renderingData.cameraData.renderer.ConfigureCameraTarget(_cameraOutputResolutionColorTarget, _cameraOutputResolutionDepthTarget);
            }

            public static void FreeMemory()
            {
                _colorPtr = IntPtr.Zero;
                _output?.Release();
                _output = null;
                _outputPtr = IntPtr.Zero;
                _reactive?.Release();
                _reactive = null;
                _reactivePtr = IntPtr.Zero;
            }
        }

        private readonly SetMipBias _setMipBias = new();
        private readonly Upscale _upscale = new();

        public override void Create()
        {
            name = "Upscaler";
            _triangle = new Mesh
            {
                vertices = new Vector3[] { new(-1, -1, 0), new(3, -1, 0), new(-1, 3, 0) },
                uv = new Vector2[] { new(0, 1), new(2, 1), new(0, -1) },
                triangles = new[] { 0, 1, 2 }
            };
            _blitMaterial = new Material(Shader.Find("Hidden/BlitToDepth"));
            Upscale.SgsrMaterial = new Material(Shader.Find("Conifer/Upscaler/Snapdragon Game Super Resolution/v1/Upscale"));
            Upscale.SgsrConvertMaterial = new Material(Shader.Find("Conifer/Upscaler/Snapdragon Game Super Resolution/v2/F2/Convert"));
            Upscale.SgsrUpscaleMaterial = new Material(Shader.Find("Conifer/Upscaler/Snapdragon Game Super Resolution/v2/F2/Upscale"));
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
                return;
            }

            if (renderingData.cameraData.camera.GetUniversalAdditionalCameraData().resetHistory) _upscaler.ResetHistory();

            _setMipBias.ConfigureInput(ScriptableRenderPassInput.None);
            _upscale.ConfigureInput(ScriptableRenderPassInput.Motion);
            if (_upscaler.IsTemporal()) renderer.EnqueuePass(_setMipBias);
            renderer.EnqueuePass(_upscale);
        }
    }
}