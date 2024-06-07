/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

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

        private class PrepareRendering : ScriptableRenderPass
        {
            private static readonly int GlobalMipBias = Shader.PropertyToID("_GlobalMipBias");

            public PrepareRendering() => renderPassEvent = RenderPassEvent.BeforeRenderingPrePasses;

            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var cameraDescriptor = renderingData.cameraData.cameraTargetDescriptor;
                cameraDescriptor.width = _upscaler.RenderingResolution.x;
                cameraDescriptor.height = _upscaler.RenderingResolution.y;
                var colorDescriptor = cameraDescriptor;
                colorDescriptor.depthStencilFormat = GraphicsFormat.None;
                RenderingUtils.ReAllocateIfNeeded(ref _cameraRenderResolutionColorTarget, colorDescriptor, name: "Conifer_CameraColorTarget");
                var depthDescriptor = cameraDescriptor;
                depthDescriptor.colorFormat = RenderTextureFormat.Depth;
                RenderingUtils.ReAllocateIfNeeded(ref _cameraRenderResolutionDepthTarget, depthDescriptor, name: "Conifer_CameraDepthTarget");
                _cameraOutputResolutionColorTarget = renderingData.cameraData.renderer.cameraColorTargetHandle;
                _cameraOutputResolutionDepthTarget = renderingData.cameraData.renderer.cameraDepthTargetHandle;
                renderingData.cameraData.renderer.ConfigureCameraTarget(_cameraRenderResolutionColorTarget, _cameraRenderResolutionDepthTarget);

                var cb = CommandBufferPool.Get("SetMipBias");
                var mipBias = (float)Math.Log((float)_upscaler.RenderingResolution.x / _upscaler.OutputResolution.x, 2f) - 1f;
                cb.SetGlobalVector(GlobalMipBias, new Vector4(mipBias, mipBias * mipBias));
                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);
            }
        }

        private class ResizeFrame : ScriptableRenderPass
        {
            public ResizeFrame() => renderPassEvent = RenderPassEvent.AfterRenderingTransparents;

            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var cb = CommandBufferPool.Get("ResizeFrame");
                BlitDepth(cb, _cameraRenderResolutionDepthTarget, _cameraOutputResolutionDepthTarget);
                BlitDepth(cb, Texture2D.blackTexture, _cameraRenderResolutionDepthTarget);
                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);

                renderingData.cameraData.renderer.ConfigureCameraTarget(_cameraOutputResolutionColorTarget, _cameraOutputResolutionDepthTarget);
            }
        }

        private class Upscale : ScriptableRenderPass
        {
            private static RTHandle _outputColor;
            private static readonly int OpaqueID = Shader.PropertyToID("_CameraOpaqueTexture");
            private static readonly int MotionID = Shader.PropertyToID("_MotionVectorTexture");
            private static readonly int UpscalerInputDepth = Shader.PropertyToID("Conifer_UpscalerInputDepth");
            private static readonly int UpscalerReactiveMask = Shader.PropertyToID("Conifer_UpscalerReactiveMask");

            public Upscale() => renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing;

            public override void Configure(CommandBuffer cmd, RenderTextureDescriptor cameraTextureDescriptor)
            {
                var outputResolutionCameraTextureDescriptor = cameraTextureDescriptor;
                outputResolutionCameraTextureDescriptor.width = _upscaler.OutputResolution.x;
                outputResolutionCameraTextureDescriptor.height = _upscaler.OutputResolution.y;
                var outputDescriptor = outputResolutionCameraTextureDescriptor;
                outputDescriptor.enableRandomWrite = true;
                outputDescriptor.depthStencilFormat = GraphicsFormat.None;
                RenderingUtils.ReAllocateIfNeeded(ref _outputColor, outputDescriptor, name: "Conifer_UpscalerOutput");

                if (_upscaler.settings.upscaler != Settings.Upscaler.FidelityFXSuperResolution2) return;

                var reactiveDescriptor = cameraTextureDescriptor;
                reactiveDescriptor.graphicsFormat = GraphicsFormat.R8_UNorm;
                reactiveDescriptor.depthStencilFormat = GraphicsFormat.None;
                cmd.GetTemporaryRT(UpscalerReactiveMask, reactiveDescriptor);
                var inputDepthDescriptor = cameraTextureDescriptor;
                inputDepthDescriptor.graphicsFormat = GraphicsFormat.None;
                inputDepthDescriptor.depthStencilFormat = GraphicsFormat.D32_SFloat;
                cmd.GetTemporaryRT(UpscalerInputDepth, inputDepthDescriptor);
            }

            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var renderer = renderingData.cameraData.renderer;
                Texture depth = renderer.cameraDepthTargetHandle;

                var cb = CommandBufferPool.Get("Upscale");
                if (_upscaler.settings.upscaler == Settings.Upscaler.FidelityFXSuperResolution2)
                {
                    /*@todo Create an Opaque Texture if the renderer does not provide one.*/
                    _upscaler.NativeInterface.SetReactiveImages(cb, Shader.GetGlobalTexture(UpscalerReactiveMask), Shader.GetGlobalTexture(OpaqueID));

                    var inputDepth = Shader.GetGlobalTexture(UpscalerInputDepth);
                    BlitDepth(cb, depth, inputDepth);
                    depth = inputDepth;
                }

                /*@todo Only create the output color if the original camera target cannot be written to from compute.*/
                _upscaler.NativeInterface.Upscale(cb, _cameraRenderResolutionColorTarget, depth, Shader.GetGlobalTexture(MotionID), _outputColor);
                cb.CopyTexture(_outputColor, _cameraOutputResolutionColorTarget);

                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);
            }

            public override void OnCameraCleanup(CommandBuffer cmd)
            {
                if (_upscaler.settings.upscaler != Settings.Upscaler.FidelityFXSuperResolution2) return;
                cmd.ReleaseTemporaryRT(UpscalerReactiveMask);
                cmd.ReleaseTemporaryRT(UpscalerInputDepth);
            }
        }

        private readonly PrepareRendering _prepareRendering = new();
        private readonly ResizeFrame _resizeFrame = new();
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
            if (_upscaler is null || !_upscaler.isActiveAndEnabled || _upscaler.settings.upscaler == Settings.Upscaler.None) return;

            var mVecData = FMotionVectorsPersistentData.GetValue(renderingData.cameraData.camera.GetUniversalAdditionalCameraData());
            var lastFrameIndex = (int[])FLastFrameIndex.GetValue(mVecData);
            lastFrameIndex[0] = Time.frameCount + 1;
            FLastFrameIndex.SetValue(mVecData, lastFrameIndex);
            FPreviousViewProjection.SetValue(mVecData, FViewProjection.GetValue(mVecData));
            FViewProjection.SetValue(mVecData, new []{GL.GetGPUProjectionMatrix(renderingData.cameraData.camera.nonJitteredProjectionMatrix, true) * renderingData.cameraData.camera.worldToCameraMatrix});

            _prepareRendering.ConfigureInput(ScriptableRenderPassInput.None);
            _resizeFrame.ConfigureInput(ScriptableRenderPassInput.None);
            _upscale.ConfigureInput(ScriptableRenderPassInput.Motion);
            renderer.EnqueuePass(_prepareRendering);
            renderer.EnqueuePass(_resizeFrame);
            renderer.EnqueuePass(_upscale);
        }
    }
}
#endif