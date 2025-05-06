#if CONIFER_UPSCALER_USE_URP
using System;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

namespace Conifer.Upscaler.URP
{
    public partial class UpscalerRendererFeature
    {
        private class GenerateRenderPass : ScriptableRenderPass, IDisposable
        {
            private readonly RTHandle[] _hudless = new RTHandle[2];
            private uint _hudlessBufferIndex;
            private static readonly int TempColor = Shader.PropertyToID("Conifer_Upscaler_TempColor");
            internal static readonly int TempMotion = Shader.PropertyToID("Conifer_Upscaler_TempMotion");
            private RTHandle _flippedDepth;
            private RTHandle _flippedMotion;

            public GenerateRenderPass() => renderPassEvent = (RenderPassEvent)int.MaxValue;

#if UNITY_6000_0_OR_NEWER
            [Obsolete("This rendering path is for compatibility mode only (when Render Graph is disabled). Use Render Graph API instead.", false)]
#endif
            public override void OnCameraSetup(CommandBuffer cmd, ref RenderingData renderingData)
            {
                var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
                var descriptor = renderingData.cameraData.cameraTargetDescriptor;
                descriptor.colorFormat = GraphicsFormatUtility.GetRenderTextureFormat(NativeInterface.GetBackBufferFormat());
                descriptor.depthStencilFormat = GraphicsFormat.None;
#if UNITY_EDITOR
                descriptor.width = (int)NativeInterface.EditorResolution.x;
                descriptor.height = (int)NativeInterface.EditorResolution.y;
#endif
                var needsUpdate = RenderingUtils.ReAllocateIfNeeded(ref _hudless[0], descriptor, name: "Conifer_Upscaler_HUDLess0");
                needsUpdate |= RenderingUtils.ReAllocateIfNeeded(ref _hudless[1], descriptor, name: "Conifer_Upscaler_HUDLess1");
                descriptor = renderingData.cameraData.cameraTargetDescriptor;
#if UNITY_EDITOR
                descriptor.width = (int)NativeInterface.EditorResolution.x;
                descriptor.height = (int)NativeInterface.EditorResolution.y;
#endif
                descriptor.depthStencilFormat = GraphicsFormat.None;
                descriptor.graphicsFormat = GraphicsFormat.R16G16_SFloat;
                needsUpdate |= RenderingUtils.ReAllocateIfNeeded(ref _flippedMotion, descriptor, name: "Conifer_Upscaler_FlippedMotion");
                descriptor = renderingData.cameraData.cameraTargetDescriptor;
#if UNITY_EDITOR
                descriptor.width = (int)NativeInterface.EditorResolution.x;
                descriptor.height = (int)NativeInterface.EditorResolution.y;
#endif
                descriptor.width = upscaler.InputResolution.x;
                descriptor.height = upscaler.InputResolution.y;
                descriptor.colorFormat = RenderTextureFormat.Depth;
                needsUpdate |= RenderingUtils.ReAllocateIfNeeded(ref _flippedDepth, descriptor, isShadowMap: true, name: "Conifer_Upscaler_FlippedDepth");

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
#if UNITY_EDITOR
                var srcRes = new Vector2(renderingData.cameraData.cameraTargetDescriptor.width, renderingData.cameraData.cameraTargetDescriptor.height);
#endif
                cb.GetTemporaryRT(TempColor, _hudless[_hudlessBufferIndex].rt.descriptor);
                cb.Blit(null, TempColor);
                cb.Blit(TempColor, _hudless[_hudlessBufferIndex]
#if UNITY_EDITOR
                    , Vector2.one, -NativeInterface.EditorOffset / srcRes
#endif
                );
                cb.Blit(TempMotion, _flippedMotion,
#if UNITY_EDITOR
                    NativeInterface.EditorResolution / srcRes *
#endif
                    new Vector2(1.0f, -1.0f),
#if UNITY_EDITOR
                    NativeInterface.EditorOffset / srcRes +
#endif
                    new Vector2(0.0f, 1.0f));
                BlitDepth(cb, Shader.GetGlobalTexture(DepthID), _flippedDepth,
#if UNITY_EDITOR
                    NativeInterface.EditorResolution / srcRes *
#endif
                    new Vector2(1.0f, -1.0f),
#if UNITY_EDITOR
                    NativeInterface.EditorOffset / srcRes +
#endif
                    new Vector2(0.0f, 1.0f));
                NativeInterface.FrameGenerate(cb, upscaler, _hudlessBufferIndex);
                cb.SetRenderTarget(k_CameraTarget);
                cb.ReleaseTemporaryRT(TempColor);
                cb.ReleaseTemporaryRT(TempMotion);
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