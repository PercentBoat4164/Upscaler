/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

#if UPSCALER_USE_URP
using System;
using System.Collections.Generic;
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
        private static RTHandle _cameraColorTarget;
        private static RTHandle _cameraDepthTarget;

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
                RenderingUtils.ReAllocateIfNeeded(ref _cameraColorTarget, colorDescriptor, name: "Conifer_CameraColorTarget");
                var depthDescriptor = cameraDescriptor;
                depthDescriptor.colorFormat = RenderTextureFormat.Depth;
                RenderingUtils.ReAllocateIfNeeded(ref _cameraDepthTarget, depthDescriptor, name: "Conifer_CameraDepthTarget");
                renderingData.cameraData.renderer.ConfigureCameraTarget(_cameraColorTarget, _cameraDepthTarget);

                var cb = CommandBufferPool.Get("SetMipBias");
                var mipBias = (float)Math.Log((float)_upscaler.RenderingResolution.x / _upscaler.OutputResolution.x, 2f) - 1f;
                cb.SetGlobalVector(GlobalMipBias, new Vector4(mipBias, mipBias * mipBias));
                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);
            }
        }

        private class Upscale : ScriptableRenderPass
        {
            private static readonly Matrix4x4 Ortho = Matrix4x4.Ortho(-1, 1, 1, -1, 1, -1);
            private static readonly Matrix4x4 LookAt = Matrix4x4.LookAt(Vector3.back, Vector3.forward, Vector3.up);
            private static RTHandle _outputColor;
            private static RTHandle _outputDepth;
            private static Mesh _triangle;
            private static Material _blitMaterial;
            private static readonly int OpaqueID = Shader.PropertyToID("_CameraOpaqueTexture");
            private static readonly int MotionID = Shader.PropertyToID("_MotionVectorTexture");
            private static readonly int DepthID = Shader.PropertyToID("_CameraDepthTexture");
            private static readonly int BlitID = Shader.PropertyToID("_MainTex");
            private static readonly int UpscalerInputDepth = Shader.PropertyToID("Conifer_UpscalerInputDepth");
            private static readonly int UpscalerReactiveMask = Shader.PropertyToID("Conifer_UpscalerReactiveMask");
            private static readonly FieldInfo FDescriptor = typeof(UniversalRenderer).Assembly.GetType("UnityEngine.Rendering.Universal.PostProcessPass")!.GetField("m_Descriptor", BindingFlags.NonPublic | BindingFlags.Instance)!;
            private static readonly FieldInfo FPostProcessPasses = typeof(UniversalRenderer).GetField("m_PostProcessPasses", BindingFlags.NonPublic | BindingFlags.Instance)!;
            private static readonly FieldInfo FPostProcessPass = typeof(UniversalRenderer).Assembly.GetType("UnityEngine.Rendering.Universal.PostProcessPasses")!.GetField("m_PostProcessPass", BindingFlags.NonPublic | BindingFlags.Instance)!;
            private static readonly FieldInfo FColorBufferSystem = typeof(UniversalRenderer).GetField("m_ColorBufferSystem", BindingFlags.NonPublic | BindingFlags.Instance)!;
            private static readonly FieldInfo FDesc = typeof(UniversalRenderer).Assembly.GetType("UnityEngine.Rendering.Universal.Internal.RenderTargetBufferSystem")!.GetField("m_Desc", BindingFlags.NonPublic | BindingFlags.Static)!;

            public Upscale() => renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing;

            public static void Create()
            {
                _triangle = new Mesh
                {
                    vertices = new Vector3[] { new(-1, -1, 0), new(3, -1, 0), new(-1, 3, 0) },
                    uv = new Vector2[] { new(0, 1), new(2, 1), new(0, -1) },
                    triangles = new[] { 0, 1, 2 }
                };
                _blitMaterial = new Material(Shader.Find("Hidden/BlitToDepth"));
            }

            public override void Configure(CommandBuffer cmd, RenderTextureDescriptor cameraTextureDescriptor)
            {
                var outputResolutionCameraTextureDescriptor = cameraTextureDescriptor;
                outputResolutionCameraTextureDescriptor.width = _upscaler.OutputResolution.x;
                outputResolutionCameraTextureDescriptor.height = _upscaler.OutputResolution.y;
                var outputDescriptor = outputResolutionCameraTextureDescriptor;
                outputDescriptor.enableRandomWrite = true;
                outputDescriptor.depthStencilFormat = GraphicsFormat.None;
                RenderingUtils.ReAllocateIfNeeded(ref _outputColor, outputDescriptor, name: "Conifer_UpscalerOutput");
                var depthDescriptor = outputResolutionCameraTextureDescriptor;
                depthDescriptor.colorFormat = RenderTextureFormat.Depth;
                RenderingUtils.ReAllocateIfNeeded(ref _outputDepth, depthDescriptor, name: "Conifer_UpscalerDepth");

                if (_upscaler.settings.upscaler != Settings.Upscaler.FidelityFXSuperResolution2) return;

                var reactiveDescriptor = cameraTextureDescriptor;
                reactiveDescriptor.graphicsFormat = GraphicsFormat.R8_UNorm;
                reactiveDescriptor.depthStencilFormat = GraphicsFormat.None;
                cmd.GetTemporaryRT(Shader.PropertyToID("Conifer_UpscalerReactiveMask"), reactiveDescriptor);
                var inputDepthDescriptor = cameraTextureDescriptor;
                inputDepthDescriptor.graphicsFormat = GraphicsFormat.None;
                inputDepthDescriptor.depthStencilFormat = GraphicsFormat.D32_SFloat;
                cmd.GetTemporaryRT(Shader.PropertyToID("Conifer_UpscalerInputDepth"), inputDepthDescriptor);
            }

            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var renderer = renderingData.cameraData.renderer;
                Texture depth = renderer.cameraDepthTargetHandle;

                var cb = CommandBufferPool.Get("Upscale");
                if (_upscaler.settings.upscaler == Settings.Upscaler.FidelityFXSuperResolution2)
                {
                    _upscaler.NativeInterface.SetReactiveImages(cb, Shader.GetGlobalTexture(UpscalerReactiveMask), Shader.GetGlobalTexture(OpaqueID));

                    var inputDepth = Shader.GetGlobalTexture(UpscalerInputDepth);
                    cb.SetRenderTarget(inputDepth);
                    cb.SetGlobalTexture(BlitID, depth);
                    cb.SetProjectionMatrix(Ortho);
                    cb.SetViewMatrix(LookAt);
                    cb.DrawMesh(_triangle, Matrix4x4.identity, _blitMaterial);

                    depth = inputDepth;
                }

                _upscaler.NativeInterface.Upscale(cb, _cameraColorTarget, depth, Shader.GetGlobalTexture(MotionID), _outputColor);
                if (renderingData.cameraData.postProcessingRequiresDepthTexture)
                {
                    cb.SetRenderTarget(_outputDepth);
                    cb.SetGlobalTexture(BlitID, depth);
                    cb.SetProjectionMatrix(Ortho);
                    cb.SetViewMatrix(LookAt);
                    cb.DrawMesh(_triangle, Matrix4x4.identity, _blitMaterial);
                    cb.SetGlobalTexture(DepthID, _outputDepth);
                }

                cb.SetRenderTarget(_cameraDepthTarget);
                cb.SetGlobalTexture(BlitID, Texture2D.blackTexture);
                cb.SetProjectionMatrix(Ortho);
                cb.SetViewMatrix(LookAt);
                cb.DrawMesh(_triangle, Matrix4x4.identity, _blitMaterial);

                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);
                renderer.ConfigureCameraTarget(_outputColor, _outputDepth);

                var descriptor = renderingData.cameraData.cameraTargetDescriptor;
                descriptor.width = _upscaler.OutputResolution.x;
                descriptor.height = _upscaler.OutputResolution.y;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                FDesc.SetValue(FColorBufferSystem.GetValue(renderer), descriptor);
                FDescriptor.SetValue(FPostProcessPass.GetValue(FPostProcessPasses.GetValue(renderer)!)!, descriptor);
            }
        }

        private readonly PrepareRendering _prepareRendering = new();
        private readonly Upscale _upscale = new();
        private static readonly FieldInfo FMotionVectorsPersistentData = typeof(UniversalAdditionalCameraData).GetField("m_MotionVectorsPersistentData", BindingFlags.NonPublic | BindingFlags.Instance)!;
        private static readonly Type MotionVectorsPersistentData = typeof(UniversalAdditionalCameraData).Assembly.GetType("UnityEngine.Rendering.Universal.MotionVectorsPersistentData")!;
        private static readonly FieldInfo FLastFrameIndex = MotionVectorsPersistentData.GetField("m_LastFrameIndex", BindingFlags.NonPublic | BindingFlags.Instance);
        private static readonly FieldInfo FPreviousViewProjection = MotionVectorsPersistentData.GetField("m_PreviousViewProjection", BindingFlags.NonPublic | BindingFlags.Instance);
        private static readonly FieldInfo FViewProjection = MotionVectorsPersistentData.GetField("m_ViewProjection", BindingFlags.NonPublic | BindingFlags.Instance);

        public override void Create()
        {
            name = "Upscaler";
            Upscale.Create();
        }

        public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
        {
            if (!Application.isPlaying || renderingData.cameraData.cameraType != CameraType.Game) return;
            _upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
            if (_upscaler is null || _upscaler.settings.upscaler == Settings.Upscaler.None) return;

            var mVecData = FMotionVectorsPersistentData.GetValue(renderingData.cameraData.camera.GetUniversalAdditionalCameraData());
            var lastFrameIndex = (int[])FLastFrameIndex.GetValue(mVecData);
            lastFrameIndex[0] = Time.frameCount + 1;
            FLastFrameIndex.SetValue(mVecData, lastFrameIndex);
            FPreviousViewProjection.SetValue(mVecData, FViewProjection.GetValue(mVecData));
            FViewProjection.SetValue(mVecData, new []{GL.GetGPUProjectionMatrix(renderingData.cameraData.camera.nonJitteredProjectionMatrix, true) * renderingData.cameraData.camera.worldToCameraMatrix});

            _upscale.ConfigureInput(ScriptableRenderPassInput.Motion);
            renderer.EnqueuePass(_prepareRendering);
            renderer.EnqueuePass(_upscale);
        }
    }
}
#endif