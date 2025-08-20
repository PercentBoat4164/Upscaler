#if UPSCALER_USE_URP
using System;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

namespace Upscaler.Runtime.URP
{
    public partial class UpscalerRendererFeature
    {
        private class GenerateRenderPass : ScriptableRenderPass, IDisposable
        {
            private readonly RTHandle[] _hudless = new RTHandle[2];
            private uint _hudlessBufferIndex;
#if UNITY_EDITOR
            private static readonly int TempColor = Shader.PropertyToID("Upscaler_TempColor");
#endif
            private RTHandle _flippedDepth;
            private RTHandle _flippedMotion;

            public GenerateRenderPass() => renderPassEvent = (RenderPassEvent)int.MaxValue;

#if UNITY_6000_0_OR_NEWER
            [Obsolete("This rendering path is for compatibility mode only (when Render Graph is disabled). Use Render Graph API instead.", false)]
#endif
            public override void OnCameraSetup(CommandBuffer cmd, ref RenderingData renderingData)
            {
                var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();

                // Frame generation expects the hudless image to be the size of the swapchain and contain only image data where the viewport is.
                var descriptor = renderingData.cameraData.cameraTargetDescriptor;
                descriptor.colorFormat = GraphicsFormatUtility.GetRenderTextureFormat(NativeInterface.GetBackBufferFormat());
                descriptor.depthStencilFormat = GraphicsFormat.None;
#if UNITY_EDITOR
                descriptor.width = (int)NativeInterface.EditorResolution.x;
                descriptor.height = (int)NativeInterface.EditorResolution.y;
#endif
                var needsUpdate = RenderingUtils.ReAllocateIfNeeded(ref _hudless[0], descriptor, name: "Upscaler_HUDLess0");
                needsUpdate |= RenderingUtils.ReAllocateIfNeeded(ref _hudless[1], descriptor, name: "Upscaler_HUDLess1");

                // Frame generation expects the motion vector texture to be the size of the swapchain but be entirely filled with motion vectors.
                descriptor = renderingData.cameraData.cameraTargetDescriptor;
#if UNITY_EDITOR
                descriptor.width = (int)NativeInterface.EditorResolution.x;
                descriptor.height = (int)NativeInterface.EditorResolution.y;
#endif
                descriptor.depthStencilFormat = GraphicsFormat.None;
                descriptor.graphicsFormat = GraphicsFormat.R16G16_SFloat;
                needsUpdate |= RenderingUtils.ReAllocateIfNeeded(ref _flippedMotion, descriptor, name: "Upscaler_FlippedMotion");

                // Frame generation expects the depth texture to be the size of the render resolution (when upscaling).
                descriptor = renderingData.cameraData.cameraTargetDescriptor;
                descriptor.width = upscaler.InputResolution.x;
                descriptor.height = upscaler.InputResolution.y;
                descriptor.colorFormat = RenderTextureFormat.Depth;
                needsUpdate |= RenderingUtils.ReAllocateIfNeeded(ref _flippedDepth, descriptor, isShadowMap: true, name: "Upscaler_FlippedDepth");

                if (!needsUpdate) return;
                NativeInterface.SetFrameGenerationImages(_hudless[0].rt.GetNativeTexturePtr(), _hudless[1].rt.GetNativeTexturePtr(), _flippedDepth.rt.GetNativeTexturePtr(), _flippedMotion.rt.GetNativeTexturePtr());
            }

#if UNITY_6000_0_OR_NEWER
            [Obsolete("This rendering path is for compatibility mode only (when Render Graph is disabled). Use Render Graph API instead.", false)]
#endif
            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
                var cb = CommandBufferPool.Get("Generate");
                // Oddity of Unity requires the backbuffer to be blitted to another image before it can be blitted to the hudless image, otherwise the offset does not happen.
#if UNITY_EDITOR
                cb.GetTemporaryRT(TempColor, renderingData.cameraData.cameraTargetDescriptor);
                cb.Blit(null, TempColor);
                cb.Blit(TempColor, _hudless[_hudlessBufferIndex], NativeInterface.EditorResolution / upscaler.OutputResolution, -NativeInterface.EditorOffset / NativeInterface.EditorResolution);
                cb.ReleaseTemporaryRT(TempColor);
#else
                cb.Blit(null, _hudless[_hudlessBufferIndex]);
#endif
                cb.Blit(Shader.GetGlobalTexture(MotionID), _flippedMotion, new Vector2(1, -1), new Vector2(0, 1));
                BlitDepth(cb, Shader.GetGlobalTexture(DepthID), _flippedDepth, new Vector2(1, -1), new Vector2(0, 1));
                NativeInterface.FrameGenerate(cb, upscaler, _hudlessBufferIndex);
                cb.SetRenderTarget(k_CameraTarget);
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
    }
}
#endif