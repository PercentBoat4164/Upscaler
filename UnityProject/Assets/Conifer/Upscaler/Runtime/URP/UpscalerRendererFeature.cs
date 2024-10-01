/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

/**************************************************
 * Upscaler v1.1.1                                *
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
        private static Mesh _upsideDownTriangle;
        private static Material _depthBlitMaterial;
        private static Material _blitMaterial;
        private static readonly int BlitID = Shader.PropertyToID("_MainTex");
        private static readonly int MotionID = Shader.PropertyToID("_MotionVectorTexture");
        private static readonly Matrix4x4 Ortho = Matrix4x4.Ortho(-1, 1, 1, -1, 1, -1);
        private static readonly Matrix4x4 LookAt = Matrix4x4.LookAt(Vector3.back, Vector3.forward, Vector3.up);

        private static RTHandle _cameraRenderResolutionColorTarget;
        private static RTHandle _cameraRenderResolutionDepthTarget;
        private static RTHandle _cameraOutputResolutionColorTarget;
        private static RTHandle _cameraOutputResolutionDepthTarget;
        private static bool _renderTargetsUpdated;

        private static Upscaler _upscaler;

        private static void MultipurposeBlit(CommandBuffer cb, RenderTargetIdentifier src, RenderTargetIdentifier dst, bool depth, bool flip = false)
        {
            cb.SetRenderTarget(dst);
            cb.SetGlobalTexture(BlitID, src);
            cb.SetProjectionMatrix(Ortho);
            cb.SetViewMatrix(LookAt);
            cb.DrawMesh(flip ? _upsideDownTriangle : _triangle, Matrix4x4.identity, depth ? _depthBlitMaterial : _blitMaterial);
        }

        private class SetMipBias : ScriptableRenderPass
        {
            private static readonly int GlobalMipBias = Shader.PropertyToID("_GlobalMipBias");

            public SetMipBias() => renderPassEvent = RenderPassEvent.BeforeRenderingPrePasses;

            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var cb = CommandBufferPool.Get("SetMipBias");
                var mipBias = (float)Math.Log((float)_upscaler.InputResolution.x / _upscaler.OutputResolution.x, 2f) - 1f;
                cb.SetGlobalVector(GlobalMipBias, new Vector4(mipBias, mipBias * mipBias));
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

            public Upscale() => renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing;

            public override void OnCameraSetup(CommandBuffer cmd, ref RenderingData renderingData)
            {
                var cameraTextureDescriptor = renderingData.cameraData.cameraTargetDescriptor;
                cameraTextureDescriptor.width = _upscaler.InputResolution.x;
                cameraTextureDescriptor.height = _upscaler.InputResolution.y;
                var colorDescriptor = cameraTextureDescriptor;
                colorDescriptor.depthStencilFormat = GraphicsFormat.None;
                _renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _cameraRenderResolutionColorTarget, colorDescriptor, name: "Conifer_CameraColorTarget");
                var depthDescriptor = cameraTextureDescriptor;
                if (_upscaler.technique == Upscaler.Technique.FidelityFXSuperResolution)
                {
                    depthDescriptor.depthStencilFormat = GraphicsFormat.D32_SFloat;
                    _renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _cameraRenderResolutionDepthTarget, depthDescriptor, isShadowMap: true, name: "Conifer_CameraDepthTarget");
                }
                else
                {
                    depthDescriptor.colorFormat = RenderTextureFormat.Depth;
                    _renderTargetsUpdated |= RenderingUtils.ReAllocateIfNeeded(ref _cameraRenderResolutionDepthTarget, depthDescriptor, name: "Conifer_CameraDepthTarget");
                }
                MultipurposeBlit(cmd, Texture2D.blackTexture, _cameraRenderResolutionDepthTarget, true);

                _cameraOutputResolutionColorTarget = renderingData.cameraData.renderer.cameraColorTargetHandle;
                _cameraOutputResolutionDepthTarget = renderingData.cameraData.renderer.cameraDepthTargetHandle;
                cmd.Blit(Texture2D.blackTexture, _cameraOutputResolutionColorTarget);
                renderingData.cameraData.renderer.ConfigureCameraTarget(_cameraRenderResolutionColorTarget, _cameraRenderResolutionDepthTarget);
            }

            public override void Configure(CommandBuffer cmd, RenderTextureDescriptor descriptor)
            {
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
                _lastMotion = thisMotion;
                _lastOpaque = thisOpaque;
                _lastTechnique = _upscaler.technique;
                _lastReactive = _upscaler.useReactiveMask;
            }

            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                if (Time.frameCount == 1) return;
                var cb = CommandBufferPool.Get("Upscale");
                if (!_upscaler.DisableUpscaling) {
                    _upscaler.NativeInterface.Upscale(cb, _upscaler);
                    cb.CopyTexture(_output, _cameraOutputResolutionColorTarget);
                } else {
                    cb.Blit(_cameraRenderResolutionColorTarget, _cameraOutputResolutionColorTarget);
                }
                MultipurposeBlit(cb, _cameraRenderResolutionDepthTarget, _cameraOutputResolutionDepthTarget, true);
                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);
                renderingData.cameraData.renderer.ConfigureCameraTarget(_cameraOutputResolutionColorTarget, _cameraOutputResolutionDepthTarget);
            }

            public static void FreeMemory()
            {
                _output?.Release();
                _output = null;
                _reactive?.Release();
                _reactive = null;
            }
        }

        private class FrameGenerate : ScriptableRenderPass
        {
            private static RTHandle _hudless;
            private static RTHandle _flippedDepth;
            private static RTHandle _flippedMotion;

            public FrameGenerate() => renderPassEvent = RenderPassEvent.AfterRendering;

            public override void OnCameraSetup(CommandBuffer cmd, ref RenderingData renderingData)
            {
                var descriptor = renderingData.cameraData.cameraTargetDescriptor;
                descriptor.colorFormat = GraphicsFormatUtility.GetRenderTextureFormat(NativeInterface.GetBackBufferFormat());
                descriptor.depthStencilFormat = GraphicsFormat.None;
                var needsUpdate = _renderTargetsUpdated;
                needsUpdate |= RenderingUtils.ReAllocateIfNeeded(ref _hudless, descriptor, name: "Conifer_FrameGenHUDLessTarget");
                descriptor = _cameraRenderResolutionDepthTarget.rt.descriptor;
                needsUpdate |= RenderingUtils.ReAllocateIfNeeded(ref _flippedDepth, descriptor, name: "Conifer_FrameGenFlippedDepthTarget");
                descriptor = _cameraOutputResolutionColorTarget.rt.descriptor;
                descriptor.graphicsFormat = GraphicsFormat.R16G16_SFloat;
                needsUpdate |= RenderingUtils.ReAllocateIfNeeded(ref _flippedMotion, descriptor, name: "Conifer_FrameGenFlippedMotionVectorTarget");

                if (!needsUpdate) return;
                _upscaler.NativeInterface.SetFrameGenerationImages(_hudless.rt.GetNativeTexturePtr(), _flippedDepth.rt.GetNativeTexturePtr(), _flippedMotion.rt.GetNativeTexturePtr());
            }

            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                if (Time.frameCount == 1) return;
                var cb = CommandBufferPool.Get("Frame Generate");
                cb.Blit(null, _hudless);
                cb.Blit(MotionID, _flippedMotion);
                MultipurposeBlit(cb, _cameraOutputResolutionDepthTarget, _flippedDepth, true, true);
                // MultipurposeBlit(cb, MotionID, _flippedMotion, false, true);
                _upscaler.NativeInterface.FrameGenerate(cb, _upscaler);
                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);
            }

            public static void FreeMemory()
            {
                _hudless?.Release();
                _hudless = null;
            }
        }

        private readonly SetMipBias _setMipBias = new();
        private readonly Upscale _upscale = new();
        private readonly FrameGenerate _frameGenerate = new();
        private static readonly FieldInfo FMotionVectorsPersistentData = typeof(UniversalAdditionalCameraData).GetField("m_MotionVectorsPersistentData", BindingFlags.NonPublic | BindingFlags.Instance)!;
        private static readonly Type MotionVectorsPersistentData = typeof(UniversalAdditionalCameraData).Assembly.GetType("UnityEngine.Rendering.Universal.MotionVectorsPersistentData")!;
        private static readonly FieldInfo FLastFrameIndex = MotionVectorsPersistentData.GetField("m_LastFrameIndex", BindingFlags.NonPublic | BindingFlags.Instance);
        private static readonly FieldInfo FPreviousViewProjection = MotionVectorsPersistentData.GetField("m_PreviousViewProjection", BindingFlags.NonPublic | BindingFlags.Instance);
        private static readonly FieldInfo FViewProjection = MotionVectorsPersistentData.GetField("m_ViewProjection", BindingFlags.NonPublic | BindingFlags.Instance);

        public override void Create()
        {
            name = "Upscaler";
            _triangle = new Mesh
            {
                vertices = new Vector3[] { new(-1, -1, 0), new(3, -1, 0), new(-1, 3, 0) },
                uv = new Vector2[] { new(0, 1), new(2, 1), new(0, -1) },
                triangles = new[] { 0, 1, 2 }
            };
            _upsideDownTriangle = _triangle;
            _upsideDownTriangle.uv = new Vector2[] { new(0, 0), new(2, 0), new(0, 2) };

            _depthBlitMaterial = new Material(Shader.Find("Hidden/BlitToDepth"));
            _blitMaterial = new Material(Shader.Find("Hidden/BlitCopy"));
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

            var cameraData = renderingData.cameraData.camera.GetUniversalAdditionalCameraData();
            if (cameraData.resetHistory) _upscaler.ResetHistory();
            var mVecData = FMotionVectorsPersistentData.GetValue(cameraData);
            var lastFrameIndex = (int[])FLastFrameIndex.GetValue(mVecData);
            lastFrameIndex[0] = Time.frameCount + 1;
            FLastFrameIndex.SetValue(mVecData, lastFrameIndex);
            FPreviousViewProjection.SetValue(mVecData, FViewProjection.GetValue(mVecData));
            FViewProjection.SetValue(mVecData, new []{GL.GetGPUProjectionMatrix(renderingData.cameraData.camera.nonJitteredProjectionMatrix, true) * renderingData.cameraData.camera.worldToCameraMatrix});

            _setMipBias.ConfigureInput(ScriptableRenderPassInput.None);
            _upscale.ConfigureInput(ScriptableRenderPassInput.Motion);
            renderer.EnqueuePass(_setMipBias);
            renderer.EnqueuePass(_upscale);
            if (_upscaler.frameGeneration)
                renderer.EnqueuePass(_frameGenerate);
            _renderTargetsUpdated = false;
        }
    }
}