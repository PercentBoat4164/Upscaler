#if CONIFER_UPSCALER_USE_URP
using System;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.RenderGraphModule;
using UnityEngine.Rendering.Universal;

namespace Conifer.Upscaler.URP
{
    public partial class UpscalerRendererFeature
    {
        public class UpscaleRenderPass : ScriptableRenderPass
        {
            private Matrix4x4 _previousMatrix;
            internal RTHandle Color;
            internal RTHandle Output;
            internal RTHandle Depth;

            public UpscaleRenderPass() => renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing;

#if UNITY_6000_0_OR_NEWER
            private class UpscaleData
            {
                internal Upscaler Upscaler;
                internal TextureHandle Color;
                internal TextureHandle Depth;
                internal TextureHandle MotionVectors;
                internal TextureHandle Opaque;
            }

            private static void Freeco(UpscaleData passData, UnsafeGraphContext context)
            {
                var cmd = CommandBufferHelpers.GetNativeCommandBuffer(context.cmd);
                cmd.CopyTexture(passData.Color, 0, 0, 0, 0, passData.Upscaler.InputResolution.x, passData.Upscaler.InputResolution.y, passData.Upscaler.Backend.Input, 0, 0, 0, 0);
                passData.Upscaler.Backend.Upscale(passData.Upscaler, cmd, passData.Depth, passData.MotionVectors, passData.Opaque);
            }

            public override void RecordRenderGraph(RenderGraph renderGraph, ContextContainer frameData)
            {
                var cameraData = frameData.Get<UniversalCameraData>();
                var upscaler = cameraData.camera.GetComponent<Upscaler>();
                var resources = frameData.Get<UniversalResourceData>();
                using var builder = renderGraph.AddUnsafePass("Conifer | Upscaler | Upscale", out UpscaleData data);
                data.Upscaler = upscaler;
                data.Color = resources.cameraColor;
                data.Depth = resources.cameraDepth;
                data.MotionVectors = resources.motionVectorColor;
                data.Opaque = resources.cameraOpaqueTexture;
                builder.UseTexture(data.Color);
                builder.UseTexture(data.Depth);
                builder.UseTexture(data.MotionVectors);
                builder.UseTexture(data.Opaque);
                builder.AllowPassCulling(false);
                builder.SetRenderFunc((UpscaleData passData, UnsafeGraphContext context) => Freeco(passData, context));
                var param = new ImportResourceParams
                {
                    clearOnFirstUse = false,
                    discardOnLastUse = false
                };
                resources.cameraColor = renderGraph.ImportTexture(Output, param);
            }

            [Obsolete("This rendering path is for compatibility mode only (when Render Graph is disabled). Use Render Graph API instead.", false)]
#endif
            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
                var commandBuffer = CommandBufferPool.Get("Upscale");
                upscaler.Backend.Upscale(upscaler, commandBuffer, Depth, Shader.GetGlobalTexture(MotionID), Shader.GetGlobalTexture(OpaqueID));
                commandBuffer.CopyTexture(Output, renderingData.cameraData.renderer.cameraColorTargetHandle);
                context.ExecuteCommandBuffer(commandBuffer);
                CommandBufferPool.Release(commandBuffer);
                var args = new object[] { false };
                UseScaling.Invoke(renderingData.cameraData.renderer.cameraColorTargetHandle, args);
                UseScaling.Invoke(renderingData.cameraData.renderer.cameraDepthTargetHandle, args);
            }
        }
    }
}
#endif