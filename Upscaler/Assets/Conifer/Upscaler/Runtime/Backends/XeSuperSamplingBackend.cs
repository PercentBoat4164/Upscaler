using System;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;

namespace Conifer.Upscaler
{
    public class XeSuperSamplingBackend : NativeAbstractBackend
    {
        [DllImport("GfxPluginUpscaler")]
        private static extern IntPtr GetUpscaleCallbackXeSuperSampling();

        [DllImport("GfxPluginUpscaler")]
        private static extern bool LoadedCorrectlyXeSuperSampling();

        [DllImport("GfxPluginUpscaler")]
        private static extern IntPtr CreateContextXeSuperSampling();

        [DllImport("GfxPluginUpscaler")]
        private static extern Upscaler.Status UpdateContextXeSuperSampling(IntPtr handle, Vector2Int resolution, Upscaler.Quality mode, Flags flags);

        [DllImport("GfxPluginUpscaler")]
        private static extern Upscaler.Status SetImagesXeSuperSampling(IntPtr handle, IntPtr color, IntPtr depth, IntPtr motion, IntPtr output, IntPtr reactive, IntPtr opaque, bool autoReactive);

        [StructLayout(LayoutKind.Sequential)]
        private struct XeSuperSamplingUpscaleData
        {
            internal IntPtr handle;
            internal Vector2 jitter;
            internal Vector2Int inputResolution;
            internal bool resetHistory;
        }

        public static bool Supported { get; }
        private static readonly IntPtr EventCallback;
        private XeSuperSamplingUpscaleData _data;

        static XeSuperSamplingBackend()
        {
            Supported = true;
            try
            {
                if (!LoadedCorrectlyPlugin() || !LoadedCorrectlyXeSuperSampling())
                {
                    Supported = false;
                    return;
                }
                EventCallback = GetUpscaleCallbackXeSuperSampling();
                if (EventCallback == IntPtr.Zero || !SystemInfo.supportsMotionVectors || !SystemInfo.supportsComputeShaders)
                {
                    Supported = false;
                    return;
                }

                var backend = new XeSuperSamplingBackend();
                var status = UpdateContextXeSuperSampling(backend._data.handle, new Vector2Int(32, 32), Upscaler.Quality.Auto, Flags.None);
                backend.Dispose();
                Supported = Upscaler.Success(status);
            }
            catch (Exception e)
            {
                Debug.LogException(e);
                Supported = false;
            }
        }
        
        public XeSuperSamplingBackend()
        {
            if (!Supported) return;
            DataHandle = Marshal.AllocCoTaskMem(Marshal.SizeOf<XeSuperSamplingUpscaleData>());
            _data = new XeSuperSamplingUpscaleData
            {
                handle = CreateContextXeSuperSampling()
            };
        }

        public override Upscaler.Status ComputeInputResolutionConstraints(in Upscaler upscaler, Flags flags)
        {
            if (!Supported) return Upscaler.Status.FatalRuntimeError;
            var status = UpdateContextXeSuperSampling(_data.handle, upscaler.OutputResolution, upscaler.quality, flags);
            if (Upscaler.Failure(status)) return status;
            upscaler.RecommendedInputResolution = GetRecommendedResolution(_data.handle);
            upscaler.MinInputResolution = GetMinimumResolution(_data.handle);
            upscaler.MaxInputResolution = GetMaximumResolution(_data.handle);
            return status;
        }

        public override Upscaler.Status Update(in Upscaler upscaler, in Texture input, in Texture output, Flags flags)
        {
            if (!Supported) return Upscaler.Status.FatalRuntimeError;
            var inputsMatch = Input == input;
            var needsImageRefresh = !inputsMatch || Output != output;

            if (!inputsMatch || Depth == null)
            {
                needsImageRefresh = true;
                Depth?.Release();
                Depth = new RenderTexture(input.width, input.height, 32, RenderTextureFormat.Shadowmap);
                Depth.Create();
            }
            if (!inputsMatch || Motion == null)
            {
                needsImageRefresh = true;
                Motion?.Release();
                Motion = (flags & Flags.OutputResolutionMotionVectors) == Flags.OutputResolutionMotionVectors ?
                    new RenderTexture(output.width, output.height, 0, RenderTextureFormat.RGHalf) :
                    new RenderTexture(input.width, input.height, 0, RenderTextureFormat.RGHalf);
                Motion.Create();
            }

            Output = output;
            Input = input;

            return needsImageRefresh ? SetImagesXeSuperSampling(_data.handle, input.GetNativeTexturePtr(), Depth.GetNativeTexturePtr(), Motion.GetNativeTexturePtr(), output.GetNativeTexturePtr(), IntPtr.Zero, IntPtr.Zero, false) : Upscaler.Status.Success;
        }

        public override void Upscale(in Upscaler upscaler, in CommandBuffer commandBuffer, in Texture depth, in Texture motion, in Texture opaque = null)
        {
            if (!Supported) return;
            _data.jitter = upscaler.Jitter;
            _data.inputResolution = upscaler.InputResolution;
            _data.resetHistory = upscaler.shouldHistoryResetThisFrame;
            Marshal.StructureToPtr(_data, DataHandle, true);

            if (depth != Depth) commandBuffer.Blit(depth, Depth, CopyDepth, 0);
            if (motion != Motion) commandBuffer.CopyTexture(motion, Motion);
            commandBuffer.IssuePluginEventAndData(EventCallback, 0, DataHandle);
        }

        public override void Dispose()
        {
            Depth?.Release();
            Motion?.Release();
            DestroyContext(_data.handle);
            Marshal.FreeCoTaskMem(DataHandle);
        }
    }
}