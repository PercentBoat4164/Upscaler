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
    public class UpscalerRendererFeature : ScriptableRendererFeature
    {
        private static Upscaler _upscaler;

        private class Upscale : ScriptableRenderPass
        {
            private static RTHandle _outputColor;
            private static RTHandle _outputDepth;
            private static Mesh _triangle;
            private static Material _blitMaterial;
            private static readonly int MotionID = Shader.PropertyToID("_MotionVectorTexture");
            private static readonly int DepthID = Shader.PropertyToID("_CameraDepthTexture");
            private static readonly int BlitID = Shader.PropertyToID("_MainTex");
            private static readonly Type PostProcessPass =
                typeof(UniversalRenderer).Assembly.GetType("UnityEngine.Rendering.Universal.PostProcessPass")!;
            private static readonly FieldInfo MDescriptor =
                PostProcessPass.GetField("m_Descriptor", BindingFlags.NonPublic | BindingFlags.Instance)!;
            private static readonly FieldInfo MPostProcessPasses =
                typeof(UniversalRenderer).GetField("m_PostProcessPasses", BindingFlags.NonPublic | BindingFlags.Instance)!;
            private static readonly FieldInfo MPostProcessPass =
                typeof(UniversalRenderer).Assembly.GetType("UnityEngine.Rendering.Universal.PostProcessPasses")!
                .GetField("m_PostProcessPass", BindingFlags.NonPublic | BindingFlags.Instance)!;
            private static readonly Type RenderTargetBufferSystem =
                typeof(UniversalRenderer).Assembly.GetType("UnityEngine.Rendering.Universal.Internal.RenderTargetBufferSystem")!;
            private static readonly FieldInfo MColorBufferSystem =
                typeof(UniversalRenderer).GetField("m_ColorBufferSystem", BindingFlags.NonPublic | BindingFlags.Instance)!;
            private static readonly FieldInfo MDesc =
                RenderTargetBufferSystem.GetField("m_Desc", BindingFlags.NonPublic | BindingFlags.Static)!;

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
                RenderingUtils.ReAllocateIfNeeded(ref _outputColor, descriptor, name: "UpscalerOutput");
                descriptor = cameraDescriptor;
                descriptor.colorFormat = RenderTextureFormat.Depth;
                RenderingUtils.ReAllocateIfNeeded(ref _outputDepth, descriptor, name: "HighResolutionDepth");
                cameraDescriptor.depthStencilFormat = GraphicsFormat.None;

                var renderer = renderingData.cameraData.renderer;
                var color = renderer.cameraColorTargetHandle;
                var depth = renderer.cameraDepthTargetHandle;
                var motion = Shader.GetGlobalTexture(MotionID);

                var upscale = CommandBufferPool.Get("Upscale");
                _upscaler.NativeInterface.Upscale(upscale, color, depth, motion, _outputColor);
                upscale.SetRenderTarget(_outputDepth);
                upscale.SetGlobalTexture(BlitID, depth);
                upscale.SetProjectionMatrix(Matrix4x4.Ortho(-1, 1, 1, -1, 1, -1));
                upscale.SetViewMatrix(Matrix4x4.LookAt(Vector3.back, Vector3.forward, Vector3.up));
                upscale.DrawMesh(_triangle, Matrix4x4.identity, _blitMaterial);
                upscale.SetGlobalTexture(DepthID, _outputDepth);
                var system = MColorBufferSystem.GetValue(renderer);
                MDesc.SetValue(system, cameraDescriptor);
                context.ExecuteCommandBuffer(upscale);
                upscale.Release();

                renderer.ConfigureCameraTarget(_outputColor, _outputDepth);
                MDescriptor.SetValue(MPostProcessPass.GetValue(MPostProcessPasses.GetValue(renderer)), cameraDescriptor);
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