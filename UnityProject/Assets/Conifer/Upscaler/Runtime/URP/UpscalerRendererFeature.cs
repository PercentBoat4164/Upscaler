/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

/**************************************************
 * Upscaler v1.1.0                                *
 * See the OfflineManual.pdf for more information *
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
            private static readonly int DepthID = Shader.PropertyToID("_MotionVectorDepthTexture");
            private static Texture _lastDepth;
            private static readonly int MotionID = Shader.PropertyToID("_MotionVectorTexture");
            private static Texture _lastMotion;
            private static readonly int OpaqueID = Shader.PropertyToID("_CameraOpaqueTexture");
            private static Texture _lastOpaque;
            private static Upscaler.Technique _lastTechnique;
            private static bool _lastReactive;
            private static IntPtr _colorPtr;
            private static RTHandle _output;
            private static IntPtr _outputPtr;
            private static RTHandle _reactive;
            private static IntPtr _reactivePtr;
            private static bool _needsUpdate;

            public Upscale() => renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing;

            public override void OnCameraSetup(CommandBuffer cmd, ref RenderingData renderingData)
            {
                if (Time.frameCount == 1)
                {
                    Shader.SetGlobalTexture(DepthID, Texture2D.blackTexture);
                }
                var cameraTextureDescriptor = renderingData.cameraData.cameraTargetDescriptor;
                cameraTextureDescriptor.width = _upscaler.InputResolution.x;
                cameraTextureDescriptor.height = _upscaler.InputResolution.y;
                var colorDescriptor = cameraTextureDescriptor;
                colorDescriptor.depthStencilFormat = GraphicsFormat.None;
                if (RenderingUtils.ReAllocateIfNeeded(ref _cameraRenderResolutionColorTarget, colorDescriptor, name: "Conifer_CameraColorTarget"))
                {
                    _needsUpdate = true;
                    _colorPtr = _cameraRenderResolutionColorTarget.rt.GetNativeTexturePtr();
                }
                var depthDescriptor = cameraTextureDescriptor;
                depthDescriptor.colorFormat = RenderTextureFormat.Depth;
                RenderingUtils.ReAllocateIfNeeded(ref _cameraRenderResolutionDepthTarget, depthDescriptor, name: "Conifer_CameraDepthTarget");
                BlitDepth(cmd, Texture2D.blackTexture, _cameraRenderResolutionDepthTarget);

                _cameraOutputResolutionColorTarget = renderingData.cameraData.renderer.cameraColorTargetHandle;
                _cameraOutputResolutionDepthTarget = renderingData.cameraData.renderer.cameraDepthTargetHandle;
                renderingData.cameraData.renderer.ConfigureCameraTarget(_cameraRenderResolutionColorTarget, _cameraRenderResolutionDepthTarget);
            }

            public override void Configure(CommandBuffer cmd, RenderTextureDescriptor descriptor)
            {
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
                    // _reactive?.Release();
                    // _reactive = null;
                    // _reactivePtr = IntPtr.Zero;
                }

                if (Time.frameCount == 1) return;
                var thisDepth = Shader.GetGlobalTexture(DepthID);
                var thisMotion = Shader.GetGlobalTexture(MotionID);
                var thisOpaque = Shader.GetGlobalTexture(OpaqueID);
                if (_needsUpdate || thisDepth != _lastDepth || thisMotion != _lastMotion || thisOpaque != _lastOpaque || _upscaler.technique != _lastTechnique || _upscaler.useReactiveMask != _lastReactive)
                    _upscaler.NativeInterface.SetImages(_colorPtr, thisDepth?.GetNativeTexturePtr() ?? IntPtr.Zero, thisMotion?.GetNativeTexturePtr() ?? IntPtr.Zero, _outputPtr, _reactivePtr, thisOpaque?.GetNativeTexturePtr() ?? IntPtr.Zero, _upscaler.useReactiveMask);
                _needsUpdate = false;
                _lastDepth = thisDepth;
                _lastMotion = thisMotion;
                _lastOpaque = thisOpaque;
                _lastTechnique = _upscaler.technique;
                _lastReactive = _upscaler.useReactiveMask;
            }

            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                if (Time.frameCount == 1) return;
                var cb = CommandBufferPool.Get("Upscale");
                _upscaler.NativeInterface.Upscale(cb, _upscaler);
                cb.CopyTexture(_output, _cameraOutputResolutionColorTarget);
                BlitDepth(cb, DepthID, _cameraOutputResolutionDepthTarget);
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
            _blitMaterial = new Material(Shader.Find("Hidden/BlitToDepth"));
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
        }
    }
}