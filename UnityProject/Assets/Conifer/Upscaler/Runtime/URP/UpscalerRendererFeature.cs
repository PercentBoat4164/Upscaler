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
        private static Upscaler _upscaler;

        private class SetMipBias : ScriptableRenderPass
        {
            private static readonly int GlobalMipBias = Shader.PropertyToID("_GlobalMipBias");

            public SetMipBias() => renderPassEvent = RenderPassEvent.BeforeRenderingPrePasses;

            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var mipBias =
                    (float)Math.Log((float)_upscaler.RenderingResolution.x / _upscaler.OutputResolution.x, 2f) - 1f;

                var cb = CommandBufferPool.Get("SetMipBias");
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
            private static readonly int MotionID = Shader.PropertyToID("_MotionVectorTexture");
            private static readonly int DepthID = Shader.PropertyToID("_CameraDepthTexture");
            private static readonly int BlitID = Shader.PropertyToID("_MainTex");
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

            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var cameraDescriptor = renderingData.cameraData.cameraTargetDescriptor;
                cameraDescriptor.width = _upscaler.OutputResolution.x;
                cameraDescriptor.height = _upscaler.OutputResolution.y;
                var descriptor = cameraDescriptor;
                descriptor.enableRandomWrite = true;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                RenderingUtils.ReAllocateIfNeeded(ref _outputColor, descriptor, name: "Conifer_UpscalerOutput");
                descriptor = cameraDescriptor;
                descriptor.colorFormat = RenderTextureFormat.Depth;
                RenderingUtils.ReAllocateIfNeeded(ref _outputDepth, descriptor, name: "Conifer_UpscalerDepth");

                var renderer = renderingData.cameraData.renderer;
                var depth = renderer.cameraDepthTargetHandle;
                var motion = Shader.GetGlobalTexture(MotionID);
                if (motion is null) return;

                var cb = CommandBufferPool.Get("Upscale");
                _upscaler.NativeInterface.Upscale(cb, renderer.cameraColorTargetHandle, depth, motion, _outputColor);
                if (renderingData.cameraData.postProcessingRequiresDepthTexture)
                {
                    cb.SetRenderTarget(_outputDepth);
                    cb.SetGlobalTexture(BlitID, depth);
                    cb.SetProjectionMatrix(Ortho);
                    cb.SetViewMatrix(LookAt);
                    cb.DrawMesh(_triangle, Matrix4x4.identity, _blitMaterial);
                    cb.SetGlobalTexture(DepthID, _outputDepth);
                }

                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);
                renderer.ConfigureCameraTarget(_outputColor, _outputDepth);
                cameraDescriptor.depthStencilFormat = GraphicsFormat.None;
                FDesc.SetValue(FColorBufferSystem.GetValue(renderer), cameraDescriptor);
                FDescriptor.SetValue(FPostProcessPass.GetValue(FPostProcessPasses.GetValue(renderer)!)!, cameraDescriptor);
            }
        }

        private readonly SetMipBias _setMipBias = new();
        private readonly Upscale _upscale = new();
        private static readonly MethodInfo MSetViewProjectionAndJitterMatrix = typeof(CameraData).GetMethod("SetViewProjectionAndJitterMatrix", BindingFlags.NonPublic | BindingFlags.Instance)!;
        private static readonly FieldInfo FMotionVectorsPersistentData = typeof(UniversalAdditionalCameraData).GetField("m_MotionVectorsPersistentData", BindingFlags.NonPublic | BindingFlags.Instance)!;
        private static readonly Type MotionVectorsPersistentData = typeof(UniversalAdditionalCameraData).Assembly.GetType("UnityEngine.Rendering.Universal.MotionVectorsPersistentData")!;
        private static readonly FieldInfo FLastFrameIndex = MotionVectorsPersistentData.GetField("m_LastFrameIndex", BindingFlags.NonPublic | BindingFlags.Instance);
        private static readonly MethodInfo MMotionVectorsPersistentDataUpdate = MotionVectorsPersistentData.GetMethod("Update", BindingFlags.Public | BindingFlags.Instance);

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

            MSetViewProjectionAndJitterMatrix.Invoke(renderingData.cameraData, new object[] {renderingData.cameraData.camera.cameraToWorldMatrix, renderingData.cameraData.camera.nonJitteredProjectionMatrix, Matrix4x4.Translate(new Vector3(_upscaler.Jitter.x, _upscaler.Jitter.y, 0))});
            var mVecData = FMotionVectorsPersistentData.GetValue(renderingData.cameraData.camera.GetUniversalAdditionalCameraData());
            var frameIndex = (int[])FLastFrameIndex.GetValue(mVecData);
            frameIndex[0] = -1;
            FLastFrameIndex.SetValue(mVecData, frameIndex);
            MMotionVectorsPersistentDataUpdate.Invoke(mVecData, new object[]{renderingData.cameraData});
            frameIndex[0] = Time.frameCount + 1;
            FLastFrameIndex.SetValue(mVecData, frameIndex);

            _upscale.ConfigureInput(ScriptableRenderPassInput.Motion);
            renderer.EnqueuePass(_setMipBias);
            renderer.EnqueuePass(_upscale);
        }
    }
}
#endif