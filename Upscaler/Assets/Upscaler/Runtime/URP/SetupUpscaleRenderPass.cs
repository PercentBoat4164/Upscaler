#if UPSCALER_USE_URP
using System;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;
#if UNITY_6000_0_OR_NEWER
using UnityEngine.Rendering.RenderGraphModule;
#endif

namespace Upscaler.Runtime.URP
{
    public partial class UpscalerRendererFeature
    {
        private class SetupUpscaleRenderPass : ScriptableRenderPass
        {
#if ENABLE_VR && ENABLE_XR_MODULE
            private static readonly FieldInfo JitterMat = typeof(CameraData).GetField("m_JitterMatrix", BindingFlags.Instance | BindingFlags.NonPublic);
#endif

            private static readonly int GlobalMipBias = Shader.PropertyToID("_GlobalMipBias");

            public SetupUpscaleRenderPass() => renderPassEvent = RenderPassEvent.BeforeRenderingPrePasses;

#if UNITY_6000_0_OR_NEWER
            private class PassData
            {
                internal UniversalCameraData CameraData;
                internal Vector2 Jitter;
                internal float MipBias;
            }

            public override void RecordRenderGraph(RenderGraph renderGraph, ContextContainer frameData)
            {
                var cameraData = frameData.Get<UniversalCameraData>();
                var upscaler = cameraData.camera.GetComponent<Upscaler>();
                if (upscaler.IsTemporal())
                {
                    upscaler.Jitter = new Vector2(HaltonSequence.Get(upscaler.JitterIndex, 2), HaltonSequence.Get(upscaler.JitterIndex, 3));
                    upscaler.JitterIndex = (upscaler.JitterIndex + 1) % (int)Math.Ceiling(7 * Math.Pow((float)upscaler.OutputResolution.x / upscaler.InputResolution.x, 2));
                    using var builder = renderGraph.AddComputePass<PassData>("Upscaler | Setup Upscaling", out var data);
                    data.CameraData = cameraData;
                    data.Jitter = upscaler.Jitter / upscaler.InputResolution * 2;
                    data.MipBias = upscaler.MipBias;
                    builder.SetRenderFunc((PassData passData, ComputeGraphContext context) => ExecutePass(passData, context));
                    builder.AllowPassCulling(false);
                    builder.AllowGlobalStateModification(true);
                }
                var resources = frameData.Get<UniversalResourceData>();
                var descriptor = renderGraph.GetTextureDesc(resources.cameraColor);
                descriptor.width = upscaler.InputResolution.x;
                descriptor.height = upscaler.InputResolution.y;
                descriptor.name = "Upscaler | Input - Image";
                resources.cameraColor = renderGraph.CreateTexture(descriptor);
                descriptor = renderGraph.GetTextureDesc(resources.cameraDepth);
                descriptor.width = upscaler.InputResolution.x;
                descriptor.height = upscaler.InputResolution.y;
                descriptor.name = "Upscaler | Input Depth - Image";
                resources.cameraDepth = renderGraph.CreateTexture(descriptor);
            }

            private static void ExecutePass(PassData data, ComputeGraphContext context)
            {
                context.cmd.SetGlobalVector(GlobalMipBias, new Vector4(data.MipBias, data.MipBias * data.MipBias));
#if ENABLE_VR && ENABLE_XR_MODULE
                if (data.CameraData.xrRendering)
                {
                    JitterMat.SetValue(data.CameraData, Matrix4x4.Translate(new Vector3(data.Jitter.x, data.Jitter.y, 0.0f)));
                    ScriptableRenderer.SetCameraMatrices(context.cmd, data.CameraData, true);
                }
                else
#endif
                {
                    var projectionMatrix = data.CameraData.GetProjectionMatrix();
                    if (data.CameraData.camera.orthographic)
                    {
                        projectionMatrix.m03 += data.Jitter.x;
                        projectionMatrix.m13 += data.Jitter.y;
                    }
                    else
                    {
                        projectionMatrix.m02 -= data.Jitter.x;
                        projectionMatrix.m12 -= data.Jitter.y;
                    }
                    context.cmd.SetViewProjectionMatrices(data.CameraData.GetViewMatrix(), projectionMatrix);
                }
            }

            [Obsolete("This rendering path is for compatibility mode only (when Render Graph is disabled). Use Render Graph API instead.", false)]
#endif
            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
                if (upscaler.IsSpatial()) return;
                var cb = CommandBufferPool.Get("SetMipBias");
                cb.SetGlobalVector(GlobalMipBias, new Vector4(upscaler.MipBias, upscaler.MipBias * upscaler.MipBias));
                upscaler.Jitter = new Vector2(HaltonSequence.Get(upscaler.JitterIndex, 2), HaltonSequence.Get(upscaler.JitterIndex, 3));
                upscaler.JitterIndex = (upscaler.JitterIndex + 1) % (int)Math.Ceiling(7 * Math.Pow((float)upscaler.OutputResolution.x / upscaler.InputResolution.x, 2));
                var clipSpaceJitter = upscaler.Jitter / upscaler.InputResolution * 2;
#if ENABLE_VR && ENABLE_XR_MODULE
                if (renderingData.cameraData.xrRendering)
                {
                    object cameraData = renderingData.cameraData;
                    JitterMat.SetValue(cameraData, Matrix4x4.Translate(new Vector3(clipSpaceJitter.x, clipSpaceJitter.y, 0.0f)));
                    renderingData.cameraData = (CameraData)cameraData;
                    ScriptableRenderer.SetCameraMatrices(cb, ref renderingData.cameraData, true);
                }
                else
#endif
                {
                    var projectionMatrix = renderingData.cameraData.camera.nonJitteredProjectionMatrix;
                    if (renderingData.cameraData.camera.orthographic)
                    {
                        projectionMatrix.m03 += clipSpaceJitter.x;
                        projectionMatrix.m13 += clipSpaceJitter.y;
                    }
                    else
                    {
                        projectionMatrix.m02 -= clipSpaceJitter.x;
                        projectionMatrix.m12 -= clipSpaceJitter.y;
                    }
                    cb.SetViewProjectionMatrices(renderingData.cameraData.GetViewMatrix(), projectionMatrix);
                }
                context.ExecuteCommandBuffer(cb);
                CommandBufferPool.Release(cb);
            }
        }
    }
}
#endif