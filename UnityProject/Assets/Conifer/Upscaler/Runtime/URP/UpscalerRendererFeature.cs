/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

#if UPSCALER_USE_URP
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

        private class Upscale : ScriptableRenderPass
        {
            private static readonly Matrix4x4 Ortho = Matrix4x4.Ortho(-1, 1, 1, -1, 1, -1);
            private static readonly Matrix4x4 LookAt = Matrix4x4.LookAt(Vector3.back, Vector3.forward, Vector3.up);
            private static RTHandle _outputColor;
            private static RTHandle _outputDepth;
            private static RTHandle _inputDepth;
            private static RTHandle _reactiveMask;
            private static RTHandle _tcMask;
            private static Mesh _triangle;
            private static Material _blitMaterial;
            private static readonly int OpaqueID = Shader.PropertyToID("_CameraOpaqueTexture");
            private static readonly int MotionID = Shader.PropertyToID("_MotionVectorTexture");
            private static readonly int DepthID = Shader.PropertyToID("_CameraDepthTexture");
            private static readonly int BlitID = Shader.PropertyToID("_MainTex");
            private static readonly FieldInfo MDescriptor = typeof(UniversalRenderer).Assembly.GetType("UnityEngine.Rendering.Universal.PostProcessPass")!.GetField("m_Descriptor", BindingFlags.NonPublic | BindingFlags.Instance)!;
            private static readonly FieldInfo MPostProcessPasses = typeof(UniversalRenderer).GetField("m_PostProcessPasses", BindingFlags.NonPublic | BindingFlags.Instance)!;
            private static readonly FieldInfo MPostProcessPass = typeof(UniversalRenderer).Assembly.GetType("UnityEngine.Rendering.Universal.PostProcessPasses")!.GetField("m_PostProcessPass", BindingFlags.NonPublic | BindingFlags.Instance)!;
            private static readonly FieldInfo MColorBufferSystem = typeof(UniversalRenderer).GetField("m_ColorBufferSystem", BindingFlags.NonPublic | BindingFlags.Instance)!;
            private static readonly FieldInfo MDesc = typeof(UniversalRenderer).Assembly.GetType("UnityEngine.Rendering.Universal.Internal.RenderTargetBufferSystem")!.GetField("m_Desc", BindingFlags.NonPublic | BindingFlags.Static)!;

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
                RenderingUtils.ReAllocateIfNeeded(ref _outputColor, descriptor, name: "UpscalerOutputColor");
                descriptor = cameraDescriptor;
                descriptor.graphicsFormat = GraphicsFormat.None;
                RenderingUtils.ReAllocateIfNeeded(ref _outputDepth, descriptor, name: "UpscalerOutputDepth");
                descriptor = cameraDescriptor;
                descriptor.graphicsFormat = GraphicsFormat.R8_UNorm;
                descriptor.depthStencilFormat = GraphicsFormat.None;
                RenderingUtils.ReAllocateIfNeeded(ref _reactiveMask, descriptor, name: "UpscalerReactiveMask");
                RenderingUtils.ReAllocateIfNeeded(ref _tcMask, descriptor, name: "UpscalerTCMask");
                cameraDescriptor.depthStencilFormat = GraphicsFormat.None;

                var renderer = renderingData.cameraData.renderer;
                var depth = renderer.cameraDepthTargetHandle;

                var cb = CommandBufferPool.Get("Upscale");
                if (_upscaler.settings.upscaler == Settings.Upscaler.FidelityFXSuperResolution2)
                {
                    _upscaler.NativeInterface.SetMaskImages(cb, _reactiveMask, _tcMask, Shader.GetGlobalTexture(OpaqueID));

                    descriptor = cameraDescriptor;
                    descriptor.width = _upscaler.RenderingResolution.x;
                    descriptor.width = _upscaler.RenderingResolution.y;
                    descriptor.graphicsFormat = GraphicsFormat.None;
                    descriptor.depthStencilFormat = GraphicsFormat.D32_SFloat;
                    RenderingUtils.ReAllocateIfNeeded(ref _inputDepth, descriptor, isShadowMap:true, name: "UpscalerInputDepth");

                    cb.SetRenderTarget(_inputDepth);
                    cb.SetGlobalTexture(BlitID, depth);
                    cb.SetProjectionMatrix(Ortho);
                    cb.SetViewMatrix(LookAt);
                    cb.DrawMesh(_triangle, Matrix4x4.identity, _blitMaterial);

                    depth = _inputDepth;
                }

                _upscaler.NativeInterface.Upscale(cb, renderer.cameraColorTargetHandle, depth, Shader.GetGlobalTexture(MotionID), _outputColor);
                if (renderingData.cameraData.postProcessingRequiresDepthTexture)
                {
                    cb.SetRenderTarget(_outputDepth);
                    cb.SetGlobalTexture(BlitID, depth);
                    cb.SetProjectionMatrix(Ortho);
                    cb.SetViewMatrix(LookAt);
                    cb.DrawMesh(_triangle, Matrix4x4.identity, _blitMaterial);
                    cb.SetGlobalTexture(DepthID, _outputDepth);
                }
                MDesc.SetValue(MColorBufferSystem.GetValue(renderer), cameraDescriptor);

                context.ExecuteCommandBuffer(cb);
                cb.Release();
                renderer.ConfigureCameraTarget(_outputColor, _outputDepth);
                MDescriptor.SetValue(MPostProcessPass.GetValue(MPostProcessPasses.GetValue(renderer)!)!, cameraDescriptor);
            }
        }

        private readonly Upscale _upscale = new();

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
            _upscale.ConfigureInput(ScriptableRenderPassInput.Motion);
            renderer.EnqueuePass(_upscale);
        }
    }
}
#endif