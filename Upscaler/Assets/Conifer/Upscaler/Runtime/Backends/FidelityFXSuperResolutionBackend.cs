using System;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;

namespace Conifer.Upscaler
{
    public class FidelityFXSuperResolutionBackend : UpscalerBackend
    {
        [DllImport("GfxPluginUpscaler")]
        private static extern int GetEventIDBase();

        [DllImport("GfxPluginUpscaler")]
        private static extern IntPtr GetRenderingEventCallback();

        [DllImport("GfxPluginUpscaler")]
        private static extern IntPtr CreateContextFidelityFXSuperResolution();

        [DllImport("GfxPluginUpscaler")]
        private static extern Upscaler.Status UpdateContextFidelityFXSuperResolution(IntPtr handle, Vector2Int resolution, Upscaler.Quality mode, bool hdr);

        [DllImport("GfxPluginUpscaler", EntryPoint = "GetRecommendedCameraResolution")]
        private static extern Vector2Int GetRecommendedResolution(IntPtr handle);

        [DllImport("GfxPluginUpscaler", EntryPoint = "GetMaximumCameraResolution")]
        private static extern Vector2Int GetMaximumResolution(IntPtr handle);

        [DllImport("GfxPluginUpscaler", EntryPoint = "GetMinimumCameraResolution")]
        private static extern Vector2Int GetMinimumResolution(IntPtr handle);

        [DllImport("GfxPluginUpscaler")]
        private static extern Upscaler.Status SetImagesFidelityFXSuperResolution(IntPtr handle, IntPtr color, IntPtr depth, IntPtr motion, IntPtr output, IntPtr reactive, IntPtr opaque, bool autoReactive);

        [DllImport("GfxPluginUpscaler")]
        private static extern void Destroy(IntPtr handle);

        [StructLayout(LayoutKind.Sequential)]
        private struct FidelityFXSuperResolutionUpscaleData
        {
            internal float frameTime;
            internal float sharpness;
            internal float reactiveValue;
            internal float reactiveScale;
            internal float reactiveThreshold;
            internal IntPtr handle;
            internal float farPlane;
            internal float nearPlane;
            internal float verticalFOV;
            internal Vector2 jitter;
            internal Vector2Int inputResolution;
            internal uint options;
        }

        private readonly IntPtr _nativeHandle = CreateContextFidelityFXSuperResolution();
        private static readonly int UpscaleEventID = GetEventIDBase();
        private static readonly IntPtr EventCallback = GetRenderingEventCallback();
        private GCHandle _data = GCHandle.Alloc(new FidelityFXSuperResolutionUpscaleData(), GCHandleType.Pinned);
        private Texture _input;
        private Texture _output;
        private RenderTexture _depth;
        private RenderTexture _motion;

        public FidelityFXSuperResolutionBackend()
        {
            var data = (FidelityFXSuperResolutionUpscaleData)_data.Target;
            data.handle = _nativeHandle;
            _data.Target = data;
        }

        public override Upscaler.Status ComputeInputResolutionConstraints(in Upscaler upscaler)
        {
            var status = UpdateContextFidelityFXSuperResolution(_nativeHandle, upscaler.OutputResolution, upscaler.quality, upscaler.Camera.allowHDR);
            if (Upscaler.Failure(status)) return status;
            upscaler.MaxInputResolution = GetMaximumResolution(_nativeHandle);
            upscaler.MinInputResolution = GetMinimumResolution(_nativeHandle);
            upscaler.RecommendedInputResolution = GetRecommendedResolution(_nativeHandle);
            return status;
        }

        public override bool Update(in Upscaler upscaler, in Texture input, in Texture output)
        {
            var inputsMatch = _input == input;
            var needsImageRefresh = !inputsMatch || _output != output;

            if (!inputsMatch || _depth == null)
            {
                needsImageRefresh = true;
                _depth?.Release();
                _depth = new RenderTexture(input.width, input.height, 32, RenderTextureFormat.Shadowmap);
                _depth.Create();
            }
            if (!inputsMatch || _motion == null)
            {
                needsImageRefresh = true;
                _motion?.Release();
                _motion = new RenderTexture(input.width, input.height, 0, RenderTextureFormat.RGHalf);
                _motion.Create();
            }

            _output = output;
            _input = input;

            if (needsImageRefresh && Upscaler.Failure(SetImagesFidelityFXSuperResolution(_nativeHandle, input.GetNativeTexturePtr(), _depth.GetNativeTexturePtr(), _motion.GetNativeTexturePtr(), output.GetNativeTexturePtr(), IntPtr.Zero, IntPtr.Zero, false))) return false;
            var data = (FidelityFXSuperResolutionUpscaleData)_data.Target;
            data.sharpness = upscaler.sharpness;
            data.reactiveValue = upscaler.reactiveMax;
            data.reactiveScale = upscaler.reactiveScale;
            data.reactiveThreshold = upscaler.reactiveThreshold;
            _data.Target = data;
            return true;
        }

        public override void Upscale(in Upscaler upscaler, in CommandBuffer commandBuffer = null)
        {
            var nonJitteredProjectionMatrix = upscaler.Camera.nonJitteredProjectionMatrix;
            var planes = nonJitteredProjectionMatrix.decomposeProjection;
            var data = (FidelityFXSuperResolutionUpscaleData)_data.Target;
            data.frameTime = Time.deltaTime * 1000.0f;
            data.farPlane = planes.zFar;
            data.nearPlane = planes.zNear;
            data.verticalFOV = 2.0f * (float)Math.Atan(1.0f / nonJitteredProjectionMatrix.m11) * 180.0f / (float)Math.PI;
            data.jitter = upscaler.Jitter;
            data.inputResolution = upscaler.InputResolution;
            data.options = Convert.ToUInt32(upscaler.Camera.orthographic) << 0 |
                           Convert.ToUInt32(upscaler.upscalingDebugView) << 1 |
                           Convert.ToUInt32(upscaler.shouldHistoryResetThisFrame) << 2;
            _data.Target = data;

            if (commandBuffer == null)
            {
                Graphics.Blit(Shader.GetGlobalTexture("_CameraDepthTexture"), _depth, new Material(Shader.Find("Hidden/Conifer/BlitDepth")), 0);
                Graphics.CopyTexture(Shader.GetGlobalTexture("_CameraMotionVectorsTexture"), _motion);
                var cmdBuf = new CommandBuffer();
                cmdBuf.IssuePluginEventAndData(EventCallback, UpscaleEventID, _data.AddrOfPinnedObject());
                Graphics.ExecuteCommandBuffer(cmdBuf);
            }
            else
            {
                commandBuffer.Blit("_CameraDepthTexture", _depth, new Material(Shader.Find("Hidden/Conifer/BlitDepth")), 0);
                commandBuffer.CopyTexture("_CameraMotionVectorsTexture", _motion);
                commandBuffer.IssuePluginEventAndData(EventCallback, UpscaleEventID, _data.AddrOfPinnedObject());
            }
        }

        public new void Dispose()
        {
            _depth?.Release();
            _motion?.Release();
            _data.Free();
            Destroy(_nativeHandle);
        }
    }
}