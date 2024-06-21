/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

/**************************************************
 * Upscaler v1.0.0                                *
 * See the OfflineManual.pdf for more information *
 **************************************************/

#if UPSCALER_USE_URP
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
            private static readonly int OpaqueID = Shader.PropertyToID("_CameraOpaqueTexture");
            private static readonly int MotionID = Shader.PropertyToID("_MotionVectorTexture");
            private static RTHandle _output;
            private static IntPtr _outputPtr;
            private static RTHandle _upscalerDepth;
            private static IntPtr _upscalerDepthPtr;
            private static RTHandle _inputDepth;
            private static IntPtr _inputDepthPtr;
            private static RTHandle _reactiveMask;
            private static IntPtr _reactiveMaskPtr;
            private static RTHandle _motion;
            private static IntPtr _motionPtr;
            private static Texture _lastOpaque;
            private static IntPtr _opaquePtr;
            private static RTHandle _lastColor;
            private static IntPtr _colorPtr;

            public Upscale() => renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing;

            public override void OnCameraSetup(CommandBuffer cmd, ref RenderingData renderingData)
            {
                var cameraTextureDescriptor = renderingData.cameraData.cameraTargetDescriptor;
                cameraTextureDescriptor.width = _upscaler.InputResolution.x;
                cameraTextureDescriptor.height = _upscaler.InputResolution.y;
                var colorDescriptor = cameraTextureDescriptor;
                colorDescriptor.depthStencilFormat = GraphicsFormat.None;
                RenderingUtils.ReAllocateIfNeeded(ref _cameraRenderResolutionColorTarget, colorDescriptor, name: "Conifer_CameraColorTarget");
                var depthDescriptor = cameraTextureDescriptor;
                depthDescriptor.colorFormat = RenderTextureFormat.Depth;
                RenderingUtils.ReAllocateIfNeeded(ref _cameraRenderResolutionDepthTarget, depthDescriptor, name: "Conifer_CameraDepthTarget");
                BlitDepth(cmd, Texture2D.blackTexture, _cameraRenderResolutionDepthTarget);

                _cameraOutputResolutionColorTarget = renderingData.cameraData.renderer.cameraColorTargetHandle;
                _cameraOutputResolutionDepthTarget = renderingData.cameraData.renderer.cameraDepthTargetHandle;
                renderingData.cameraData.renderer.ConfigureCameraTarget(_cameraRenderResolutionColorTarget, _cameraRenderResolutionDepthTarget);
            }

            public override void Configure(CommandBuffer cmd, RenderTextureDescriptor cameraTextureDescriptor)
            {
                var outputDescriptor = cameraTextureDescriptor;
                outputDescriptor.enableRandomWrite = true;
                outputDescriptor.depthStencilFormat = GraphicsFormat.None;
                if (RenderingUtils.ReAllocateIfNeeded(ref _output, outputDescriptor, name: "Conifer_UpscalerOutput")) _outputPtr = _output.rt.GetNativeTexturePtr();
                var inputDescriptor = cameraTextureDescriptor;
                inputDescriptor.width = _upscaler.InputResolution.x;
                inputDescriptor.height = _upscaler.InputResolution.y;
                inputDescriptor.graphicsFormat = GraphicsFormat.R16G16_SFloat;
                inputDescriptor.depthStencilFormat = GraphicsFormat.None;
                if (RenderingUtils.ReAllocateIfNeeded(ref _motion, inputDescriptor, name: "Conifer_UpscalerMotion")) _motionPtr = _motion.rt.GetNativeTexturePtr();

                if (_upscaler.technique != Upscaler.Technique.FidelityFXSuperResolution2) return;
                inputDescriptor.graphicsFormat = GraphicsFormat.R8_UNorm;
                inputDescriptor.depthStencilFormat = GraphicsFormat.None;
                if (RenderingUtils.ReAllocateIfNeeded(ref _reactiveMask, inputDescriptor, name: "Conifer_UpscalerReactiveMask")) _reactiveMaskPtr = _reactiveMask.rt.GetNativeTexturePtr();
                inputDescriptor.graphicsFormat = GraphicsFormat.None;
                inputDescriptor.depthStencilFormat = GraphicsFormat.D32_SFloat;
                if (RenderingUtils.ReAllocateIfNeeded(ref _upscalerDepth, inputDescriptor, name: "Conifer_UpscalerInputDepth")) _upscalerDepthPtr = _upscalerDepth.rt.GetNativeDepthBufferPtr();
            }

            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                IntPtr depthPtr;
                var cb = CommandBufferPool.Get("Upscale");
                if (_upscaler.technique == Upscaler.Technique.FidelityFXSuperResolution2)
                {
                    UpdateCachedNativeTexturePtrFromTexture(Shader.GetGlobalTexture(OpaqueID), ref _lastOpaque, ref _opaquePtr);
                    BlitDepth(cb, _cameraRenderResolutionDepthTarget, _upscalerDepth);
                    depthPtr = _upscalerDepthPtr;
                }
                else depthPtr = UpdateCachedNativeTexturePtrFromRTHandle(_cameraRenderResolutionDepthTarget, ref _inputDepth, ref _inputDepthPtr);
                cb.Blit(Shader.GetGlobalTexture(MotionID), _motion);
                UpdateCachedNativeTexturePtrFromRTHandle(_cameraRenderResolutionColorTarget, ref _lastColor, ref _colorPtr);
                _upscaler.NativeInterface.Upscale(cb, _upscaler, _colorPtr, depthPtr, _motionPtr, _outputPtr, _reactiveMaskPtr, _opaquePtr);
                cb.CopyTexture(_output, _cameraOutputResolutionColorTarget);
                BlitDepth(cb, _cameraRenderResolutionDepthTarget, _cameraOutputResolutionDepthTarget);

                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);

                renderingData.cameraData.renderer.ConfigureCameraTarget(_cameraOutputResolutionColorTarget, _cameraOutputResolutionDepthTarget);
                return;

                void UpdateCachedNativeTexturePtrFromTexture(Texture current, ref Texture old, ref IntPtr currentPtr)
                {
                    if (current == old) return;
                    old = current;
                    currentPtr = current.GetNativeTexturePtr();
                }

                IntPtr UpdateCachedNativeTexturePtrFromRTHandle(RTHandle current, ref RTHandle old, ref IntPtr currentPtr)
                {
                    if (current == old) return currentPtr;
                    old = current;
                    currentPtr = current.rt.GetNativeTexturePtr();
                    return currentPtr;
                }
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
            if (!Application.isPlaying || renderingData.cameraData.cameraType != CameraType.Game) return;
            _upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
            if (_upscaler is null || !_upscaler.isActiveAndEnabled || _upscaler.technique == Upscaler.Technique.None) return;

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
#endif