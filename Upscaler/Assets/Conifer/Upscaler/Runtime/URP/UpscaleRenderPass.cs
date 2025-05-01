using UnityEngine;
using UnityEngine.Rendering;
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
        private class Sgsr2F2PassData
        {
            internal Vector2Int Output;
            internal Vector2Int Input;
            internal Vector2 Jitter;
            internal bool CameraIsSame;
            internal float FOV;
            internal bool ShouldReset;
            internal Material ConvertMaterial;
            internal Material UpscaleMaterial;
            internal TextureHandle OutputTexture;
            internal TextureHandle InputTexture;
            internal TextureHandle HistoryTexture;
            internal TextureHandle MotionDepthAlphaTexture;
        }

        private class Sgsr2C2PassData
        {
            internal Vector2Int Output;
            internal Vector2Int Input;
            internal Vector2 Jitter;
            internal bool CameraIsSame;
            internal float FOV;
            internal bool ShouldReset;
            internal Vector3Int OutputThreadGroups;
            internal Vector3Int InputThreadGroups;
            internal ComputeShader ConvertShader;
            internal ComputeShader UpscaleShader;
            internal TextureHandle OutputTexture;
            internal TextureHandle InputTexture;
            internal TextureHandle HistoryTexture;
            internal TextureHandle TempHistoryTexture;
            internal TextureHandle TempMotionDepthAlphaTexture;
            internal TextureHandle TempLumaTexture;
        }

        private class Sgsr2C3PassData
        {
            internal Vector2Int Output;
            internal Vector2Int Input;
            internal Vector2 Jitter;
            internal bool CameraIsSame;
            internal float FOV;
            internal bool ShouldReset;
            internal Vector3Int OutputThreadGroups;
            internal Vector3Int InputThreadGroups;
            internal ComputeShader ConvertShader;
            internal ComputeShader ActivateShader;
            internal ComputeShader UpscaleShader;
            internal TextureHandle OutputTexture;
            internal TextureHandle InputTexture;
            internal TextureHandle HistoryTexture;
            internal TextureHandle TempHistoryTexture;
            internal TextureHandle TempMotionDepthAlphaTexture0;
            internal TextureHandle TempMotionDepthAlphaTexture1;
            internal TextureHandle LumaTexture;
            internal TextureHandle TempLumaTexture0;
            internal TextureHandle TempLumaTexture1;
        }

        private class NativeUpscalerData
        {
            internal Upscaler Upscaler;
        }

        public override void RecordRenderGraph(RenderGraph renderGraph, ContextContainer frameData)
        {
            var cameraData = frameData.Get<UniversalCameraData>();
            var upscaler = cameraData.camera.GetComponent<Upscaler>();
            var resources = frameData.Get<UniversalResourceData>();
            var renderResolutionDescriptor = renderGraph.GetTextureDesc(resources.cameraColor);
            var outputResolutionDescriptor = renderResolutionDescriptor;
            outputResolutionDescriptor.width = upscaler.OutputResolution.x;
            outputResolutionDescriptor.height = upscaler.OutputResolution.y;
            switch (upscaler.technique) {
                case Upscaler.Technique.SnapdragonGameSuperResolution1:
                    var descriptor = outputResolutionDescriptor;
                    descriptor.clearBuffer = false;
                    descriptor.discardBuffer = false;
                    descriptor.name = "Conifer | Upscaler | Output - Image";
                    var output = renderGraph.CreateTexture(descriptor);
                    var bmp =
 new RenderGraphUtils.BlitMaterialParameters(resources.cameraColor, output, _sgsr1Upscale, 0);
                    _sgsr1Upscale.SetVector(ViewportInfoID, new Vector4(1.0f / upscaler.InputResolution.x, 1.0f / upscaler.InputResolution.y, upscaler.InputResolution.x, upscaler.InputResolution.y));
                    _sgsr1Upscale.SetFloat(EdgeSharpnessID, upscaler.sharpness + 1);
                    renderGraph.AddBlitPass(bmp, passName: "Conifer | Upscaler | Snapdragon Game Super Resolution 1");
                    resources.cameraColor = output;
                    break;
                case Upscaler.Technique.SnapdragonGameSuperResolution2:
                    var importResourceParams = new ImportResourceParams { clearOnFirstUse = false, discardOnLastUse = false };
                    var historyTexture = renderGraph.ImportTexture(_history, importResourceParams);
                    descriptor = outputResolutionDescriptor;
                    descriptor.clearBuffer = false;
                    descriptor.discardBuffer = false;
                    descriptor.name = "Conifer | Upscaler | Output - Image";
                    if (upscaler.sgsrMethod != Upscaler.SgsrMethod.Fragment2Pass)
                        descriptor.enableRandomWrite = true;
                    output = renderGraph.CreateTexture(descriptor);
                    descriptor = renderResolutionDescriptor;
                    descriptor.format = GraphicsFormat.R16G16B16A16_SFloat;
                    descriptor.clearBuffer = false;
                    descriptor.discardBuffer = false;
                    descriptor.name = "Conifer | Upscaler | Motion Depth Alpha - Image";
                    if (upscaler.sgsrMethod != Upscaler.SgsrMethod.Fragment2Pass)
                        descriptor.enableRandomWrite = true;
                    var tempMotionDepthAlpha0 = renderGraph.CreateTexture(descriptor);
                    var current = cameraData.GetViewMatrix() * cameraData.GetProjectionMatrix();
                    float vpDiff = 0;
                    for (var i = 0; i < 4; i++) for (var j = 0; j < 4; j++) vpDiff += Math.Abs(current[i, j] - _previousMatrix[i, j]);
                    _previousMatrix = current;
                    switch (upscaler.sgsrMethod) {
                        case Upscaler.SgsrMethod.Compute3Pass:
                            var tempMotionDepthAlpha1 = renderGraph.CreateTexture(tempMotionDepthAlpha0);
                            var lumaHistory = renderGraph.ImportTexture(_lumaHistory, importResourceParams);
                            var tempLuma0 = renderGraph.CreateTexture(lumaHistory);
                            var tempLuma1 = renderGraph.CreateTexture(lumaHistory);
                            descriptor = renderGraph.GetTextureDesc(historyTexture);
                            descriptor.enableRandomWrite = true;
                            descriptor.name = "Conifer | Upscaler | Next History - Image";
                            var tempHistory0 = renderGraph.CreateTexture(descriptor);
                            using (var builder = renderGraph.AddComputePass<Sgsr2C3PassData>("Conifer | Upscaler | Snapdragon Game Super Resolution 2 - Compute 3 Pass", out var data))
                            {
                                data.Output = upscaler.OutputResolution;
                                data.Input = upscaler.InputResolution;
                                data.Jitter = upscaler.Jitter;
                                data.CameraIsSame = vpDiff < 1e-5;
                                data.FOV = cameraData.camera.fieldOfView;
                                data.ShouldReset = upscaler.NativeInterface.ShouldResetHistory;
                                data.OutputThreadGroups = Vector3Int.CeilToInt(new Vector3(upscaler.OutputResolution.x, upscaler.OutputResolution.y, 1) / 8);
                                data.InputThreadGroups = Vector3Int.CeilToInt(new Vector3(upscaler.InputResolution.x, upscaler.InputResolution.y, 1) / 8);
                                data.ConvertShader = _sgsr2C3Convert;
                                data.ActivateShader = _sgsr2C3Activate;
                                data.UpscaleShader = _sgsr2C3Upscale;
                                data.OutputTexture = output;
                                data.InputTexture = resources.cameraColor;
                                data.HistoryTexture = historyTexture;
                                data.TempHistoryTexture = tempHistory0;
                                data.TempMotionDepthAlphaTexture0 = tempMotionDepthAlpha0;
                                data.TempMotionDepthAlphaTexture1 = tempMotionDepthAlpha1;
                                data.LumaTexture = lumaHistory;
                                data.TempLumaTexture0 = tempLuma0;
                                data.TempLumaTexture1 = tempLuma1;
                                builder.UseTexture(output, AccessFlags.Write);
                                builder.UseTexture(resources.cameraColor);
                                builder.UseTexture(historyTexture);
                                builder.UseTexture(tempHistory0, AccessFlags.Write);
                                builder.UseTexture(tempMotionDepthAlpha0, AccessFlags.ReadWrite | AccessFlags.Discard);
                                builder.UseTexture(tempMotionDepthAlpha1, AccessFlags.ReadWrite | AccessFlags.Discard);
                                builder.UseTexture(lumaHistory, AccessFlags.ReadWrite);
                                builder.UseTexture(tempLuma0, AccessFlags.ReadWrite | AccessFlags.Discard);
                                builder.UseTexture(tempLuma1, AccessFlags.ReadWrite);
                                builder.UseTexture(resources.motionVectorColor);
                                builder.UseTexture(resources.cameraDepth);
                                builder.SetRenderFunc((Sgsr2C3PassData passData, ComputeGraphContext context) => ExecuteSgsr2C3(passData, context));
                                builder.AllowGlobalStateModification(true);
                            }
                            renderGraph.AddCopyPass(tempHistory0, historyTexture, passName: "Conifer | Upscaler | Snapdragon Game Super Resolution 2 - Copy Next History To Current History");
                            renderGraph.AddCopyPass(tempLuma1, lumaHistory, passName: "Conifer | Upscaler | Snapdragon Game Super Resolution 2 - Copy Next LumaHistory To Current LumaHistory");
                            break;
                        case Upscaler.SgsrMethod.Compute2Pass:
                            lumaHistory = renderGraph.ImportTexture(_lumaHistory, importResourceParams);
                            descriptor = renderGraph.GetTextureDesc(historyTexture);
                            descriptor.enableRandomWrite = true;
                            descriptor.name = "Conifer | Upscaler | Next History - Image";
                            tempHistory0 = renderGraph.CreateTexture(descriptor);
                            using (var builder =
 renderGraph.AddComputePass<Sgsr2C2PassData>("Conifer | Upscaler | Snapdragon Game Super Resolution 2 - Compute 2 Pass", out var data))
                            {
                                data.Output = upscaler.OutputResolution;
                                data.Input = upscaler.InputResolution;
                                data.Jitter = upscaler.Jitter;
                                data.CameraIsSame = vpDiff < 1e-5;
                                data.FOV = cameraData.camera.fieldOfView;
                                data.ShouldReset = upscaler.NativeInterface.ShouldResetHistory;
                                data.OutputThreadGroups =
 Vector3Int.CeilToInt(new Vector3(upscaler.OutputResolution.x, upscaler.OutputResolution.y, 1) / 8);
                                data.InputThreadGroups =
 Vector3Int.CeilToInt(new Vector3(upscaler.InputResolution.x, upscaler.InputResolution.y, 1) / 8);
                                data.ConvertShader = _sgsr2C2Convert;
                                data.UpscaleShader = _sgsr2C2Upscale;
                                data.OutputTexture = output;
                                data.InputTexture = resources.cameraColor;
                                data.HistoryTexture = historyTexture;
                                data.TempHistoryTexture = tempHistory0;
                                data.TempMotionDepthAlphaTexture = tempMotionDepthAlpha0;
                                data.TempLumaTexture = lumaHistory;
                                builder.UseTexture(output, AccessFlags.Write);
                                builder.UseTexture(resources.cameraColor);
                                builder.UseTexture(historyTexture);
                                builder.UseTexture(tempHistory0, AccessFlags.Write);
                                builder.UseTexture(tempMotionDepthAlpha0, AccessFlags.ReadWrite | AccessFlags.Discard);
                                builder.UseTexture(lumaHistory, AccessFlags.ReadWrite);
                                builder.UseTexture(resources.motionVectorColor);
                                builder.UseTexture(resources.cameraDepth);
                                builder.SetRenderFunc((Sgsr2C2PassData passData, ComputeGraphContext context) => ExecuteSgsr2C2(passData, context));
                                builder.AllowGlobalStateModification(true);
                            }
                            renderGraph.AddCopyPass(tempHistory0, historyTexture, passName: "Conifer | Upscaler | Snapdragon Game Super Resolution 2 - Copy Next History To Current History");
                            break;
                        case Upscaler.SgsrMethod.Fragment2Pass:
                            using (var builder =
 renderGraph.AddUnsafePass<Sgsr2F2PassData>("Conifer | Upscaler | Snapdragon Game Super Resolution 2 - Fragment 2 Pass", out var data))
                            {
                                data.Output = upscaler.OutputResolution;
                                data.Input =
 new Vector2Int(renderResolutionDescriptor.width, renderResolutionDescriptor.height);
                                data.Jitter = upscaler.Jitter;
                                data.CameraIsSame = vpDiff < 1e-5;
                                data.FOV = cameraData.camera.fieldOfView;
                                data.ShouldReset = upscaler.NativeInterface.ShouldResetHistory;
                                data.ConvertMaterial = _sgsr2F2Convert;
                                data.UpscaleMaterial = _sgsr2F2Upscale;
                                data.OutputTexture = output;
                                data.InputTexture = resources.cameraColor;
                                data.HistoryTexture = historyTexture;
                                data.MotionDepthAlphaTexture = tempMotionDepthAlpha0;
                                builder.UseTexture(output, AccessFlags.Write);
                                builder.UseTexture(resources.cameraColor);
                                builder.UseTexture(historyTexture, AccessFlags.ReadWrite);
                                builder.UseTexture(tempMotionDepthAlpha0, AccessFlags.ReadWrite | AccessFlags.Discard);
                                builder.UseTexture(resources.motionVectorColor);
                                builder.UseTexture(resources.cameraDepth);
                                builder.SetRenderFunc((Sgsr2F2PassData passData, UnsafeGraphContext context) => ExecuteSgsr2F2(passData, context));
                            }
                            renderGraph.AddCopyPass(output, historyTexture, passName: "Conifer | Upscaler | Snapdragon Game Super Resolution 2 - Copy Upscaled Output To History");
                            break;
                        default: throw new ArgumentOutOfRangeException(nameof(upscaler.sgsrMethod), upscaler.sgsrMethod, null);
                    }
                    resources.cameraColor = output;
                    break;
                case Upscaler.Technique.DeepLearningSuperSampling or Upscaler.Technique.FidelityFXSuperResolution or Upscaler.Technique.XeSuperSampling:
                    importResourceParams = new ImportResourceParams{ clearOnFirstUse = false, discardOnLastUse =
 false };
                    var depth = renderGraph.ImportTexture(_depth, importResourceParams);
                    var motion = renderGraph.ImportTexture(_motion, importResourceParams);
                    var color = renderGraph.ImportTexture(_color, importResourceParams);
                    var opaque = TextureHandle.nullHandle;
                    var reactive = TextureHandle.nullHandle;
                    output = renderGraph.ImportTexture(_output, importResourceParams);
                    renderGraph.AddBlitPass(resources.cameraDepth, depth, Vector2.one, Vector2.zero, passName: "Conifer | Upscaler | Blit Depth Input");
                    renderGraph.AddCopyPass(resources.motionVectorColor, motion, passName: "Conifer | Upscaler | Copy Motion Vectors");
                    renderGraph.AddCopyPass(resources.cameraColor, color, passName: "Conifer | Upscaler | Copy Camera Color");
                    if (upscaler.autoReactive && upscaler.technique == Upscaler.Technique.FidelityFXSuperResolution)
                    {
                        opaque = renderGraph.ImportTexture(_opaque, importResourceParams);
                        reactive = renderGraph.ImportTexture(_reactive, importResourceParams);
                        renderGraph.AddCopyPass(resources.cameraOpaqueTexture, opaque, passName: "Conifer | Upscaler | Copy Opaque Color");
                    }

                    var name = upscaler.technique switch
                    {
                        Upscaler.Technique.FidelityFXSuperResolution => "FidelityFX Super Resolution",
                        Upscaler.Technique.XeSuperSampling => "Xe Super Sampling",
                        Upscaler.Technique.DeepLearningSuperSampling => "Deep Learning Super Sampling",
                        Upscaler.Technique.None or Upscaler.Technique.SnapdragonGameSuperResolution1 or Upscaler.Technique.SnapdragonGameSuperResolution2 => throw new ArgumentOutOfRangeException(nameof(upscaler.technique), upscaler.technique, null),
                        _ => throw new ArgumentOutOfRangeException(nameof(upscaler.technique), upscaler.technique, null)
                    };
                    using (var builder =
 renderGraph.AddComputePass<NativeUpscalerData>("Conifer | Upscaler | " + name, out var data))
                    {
                        data.Upscaler = upscaler;
                        builder.UseTexture(depth, AccessFlags.Read | AccessFlags.Discard);
                        builder.UseTexture(motion, AccessFlags.Read | AccessFlags.Discard);
                        builder.UseTexture(color, AccessFlags.Read | AccessFlags.Discard);
                        builder.UseTexture(output, AccessFlags.Write);
                        if (upscaler.autoReactive && upscaler.technique == Upscaler.Technique.FidelityFXSuperResolution)
                        {
                            builder.UseTexture(opaque, AccessFlags.Read | AccessFlags.Discard);
                            builder.UseTexture(reactive, AccessFlags.ReadWrite);
                        }
                        builder.SetRenderFunc((NativeUpscalerData passData, ComputeGraphContext context) => passData.Upscaler.NativeInterface.Upscale(context.cmd, passData.Upscaler, upscaler.InputResolution));
                    }
                    resources.cameraColor = output;
                    break;
            }
        }

        private static void ExecuteSgsr2C3(Sgsr2C3PassData data, ComputeGraphContext context)
        {
            context.cmd.SetGlobalVector(RenderSizeID, (Vector2)data.Input);
            context.cmd.SetGlobalVector(OutputSizeID, (Vector2)data.Output);
            context.cmd.SetGlobalVector(RenderSizeRcpID, Vector2.one / data.Input);
            context.cmd.SetGlobalVector(OutputSizeRcpID, Vector2.one / data.Output);
            context.cmd.SetGlobalVector(JitterOffsetID, data.Jitter);
            context.cmd.SetGlobalVector(ScaleRatioID, new Vector4((float)data.Output.x / data.Input.x, Mathf.Min(20.0f, Mathf.Pow((float)data.Output.x * data.Output.y / (data.Input.x * data.Input.y), 3.0f))));
            context.cmd.SetGlobalFloat(PreExposureID, 1.0f);
            context.cmd.SetGlobalFloat(CameraFovAngleHorID, Mathf.Tan(Mathf.Deg2Rad * (data.FOV / 2)) * data.Input.x / data.Input.y);
            context.cmd.SetGlobalFloat(MinLerpContributionID, data.CameraIsSame ? 0.3f : 0.0f);
            context.cmd.SetGlobalFloat(ResetID, Convert.ToSingle(data.ShouldReset));
            context.cmd.SetGlobalInt(SameCameraID, data.CameraIsSame ? 1 : 0);
            context.cmd.SetGlobalTexture(ColorID, data.InputTexture);
            context.cmd.SetGlobalTexture(MotionDepthAlphaBufferSinkID, data.TempMotionDepthAlphaTexture0);
            context.cmd.SetGlobalTexture(LumaSinkID, data.TempLumaTexture0);
            context.cmd.DispatchCompute(data.ConvertShader, 0, data.InputThreadGroups.x, data.InputThreadGroups.y, data.InputThreadGroups.z);
            context.cmd.SetGlobalTexture(MotionDepthAlphaBufferSinkID, data.TempMotionDepthAlphaTexture1);
            context.cmd.SetGlobalTexture(MotionDepthAlphaBufferID, data.TempMotionDepthAlphaTexture0);
            context.cmd.SetGlobalTexture(LumaID, data.TempLumaTexture0);
            context.cmd.SetGlobalTexture(LumaSinkID, data.TempLumaTexture1);
            context.cmd.SetGlobalTexture(LumaHistoryID, data.LumaTexture);
            context.cmd.DispatchCompute(data.ActivateShader, 0, data.InputThreadGroups.x, data.InputThreadGroups.y, data.InputThreadGroups.z);
            context.cmd.SetGlobalTexture(MotionDepthAlphaBufferID, data.TempMotionDepthAlphaTexture1);
            context.cmd.SetGlobalTexture(NextHistoryID, data.TempHistoryTexture);
            context.cmd.SetGlobalTexture(HistoryID, data.HistoryTexture);
            context.cmd.SetGlobalTexture(OutputSinkID, data.OutputTexture);
            context.cmd.DispatchCompute(data.UpscaleShader, 0, data.OutputThreadGroups.x, data.OutputThreadGroups.y, data.OutputThreadGroups.z);
        }

        private static void ExecuteSgsr2C2(Sgsr2C2PassData data, ComputeGraphContext context)
        {
            context.cmd.SetGlobalVector(RenderSizeID, (Vector2)data.Input);
            context.cmd.SetGlobalVector(OutputSizeID, (Vector2)data.Output);
            context.cmd.SetGlobalVector(RenderSizeRcpID, Vector2.one / data.Input);
            context.cmd.SetGlobalVector(OutputSizeRcpID, Vector2.one / data.Output);
            context.cmd.SetGlobalVector(JitterOffsetID, data.Jitter);
            context.cmd.SetGlobalVector(ScaleRatioID, new Vector4((float)data.Output.x / data.Input.x, Mathf.Min(20.0f, Mathf.Pow((float)data.Output.x * data.Output.y / (data.Input.x * data.Input.y), 3.0f))));
            context.cmd.SetGlobalFloat(PreExposureID, 1.0f);
            context.cmd.SetGlobalFloat(CameraFovAngleHorID, Mathf.Tan(Mathf.Deg2Rad * (data.FOV / 2)) * data.Input.x / data.Input.y);
            context.cmd.SetGlobalFloat(MinLerpContributionID, data.CameraIsSame ? 0.3f : 0.0f);
            context.cmd.SetGlobalFloat(ResetID, Convert.ToSingle(data.ShouldReset));
            context.cmd.SetGlobalInt(SameCameraID, data.CameraIsSame ? 1 : 0);
            context.cmd.SetGlobalTexture(ColorID, data.InputTexture);
            context.cmd.SetGlobalTexture(MotionDepthAlphaBufferSinkID, data.TempMotionDepthAlphaTexture);
            context.cmd.SetGlobalTexture(LumaSinkID, data.TempLumaTexture);
            context.cmd.DispatchCompute(data.ConvertShader, 0, data.InputThreadGroups.x, data.InputThreadGroups.y, data.InputThreadGroups.z);
            context.cmd.SetGlobalTexture(MotionDepthAlphaBufferID, data.TempMotionDepthAlphaTexture);
            context.cmd.SetGlobalTexture(LumaID, data.TempLumaTexture);
            context.cmd.SetGlobalTexture(NextHistoryID, data.TempHistoryTexture);
            context.cmd.SetGlobalTexture(HistoryID, data.HistoryTexture);
            context.cmd.SetGlobalTexture(OutputSinkID, data.OutputTexture);
            context.cmd.DispatchCompute(data.UpscaleShader, 0, data.OutputThreadGroups.x, data.OutputThreadGroups.y, data.OutputThreadGroups.z);
        }

        private static void ExecuteSgsr2F2(Sgsr2F2PassData data, UnsafeGraphContext context)
        {
            context.cmd.SetGlobalVector(RenderSizeID, (Vector2)data.Input);
            context.cmd.SetGlobalVector(OutputSizeID, (Vector2)data.Output);
            context.cmd.SetGlobalVector(RenderSizeRcpID, Vector2.one / data.Input);
            context.cmd.SetGlobalVector(OutputSizeRcpID, Vector2.one / data.Output);
            context.cmd.SetGlobalVector(JitterOffsetID, data.Jitter);
            context.cmd.SetGlobalVector(ScaleRatioID, new Vector4((float)data.Output.x / data.Input.x, Mathf.Min(20.0f, Mathf.Pow((float)data.Output.x * data.Output.y / (data.Input.x * data.Input.y), 3.0f))));
            context.cmd.SetGlobalFloat(PreExposureID, 1.0f);
            context.cmd.SetGlobalFloat(CameraFovAngleHorID, Mathf.Tan(Mathf.Deg2Rad * (data.FOV / 2)) * data.Input.x / data.Input.y);
            context.cmd.SetGlobalFloat(MinLerpContributionID, data.CameraIsSame ? 0.3f : 0.0f);
            context.cmd.SetGlobalFloat(ResetID, Convert.ToSingle(data.ShouldReset));
            context.cmd.SetGlobalInt(SameCameraID, data.CameraIsSame ? 1 : 0);
            context.cmd.SetGlobalVector("_BlitScaleBias", new Vector4(1, 1, 0, 0));
            context.cmd.SetRenderTarget(data.MotionDepthAlphaTexture);
            context.cmd.DrawProcedural(Matrix4x4.identity, data.ConvertMaterial, 0, MeshTopology.Triangles, 3);
            context.cmd.SetGlobalTexture(MotionDepthAlphaBufferID, data.MotionDepthAlphaTexture);
            context.cmd.SetGlobalTexture(HistoryID, data.HistoryTexture);
            context.cmd.SetGlobalTexture(BlitID, data.InputTexture);
            context.cmd.SetRenderTarget(data.OutputTexture);
            context.cmd.DrawProcedural(Matrix4x4.identity, data.UpscaleMaterial, 0, MeshTopology.Triangles, 3);
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