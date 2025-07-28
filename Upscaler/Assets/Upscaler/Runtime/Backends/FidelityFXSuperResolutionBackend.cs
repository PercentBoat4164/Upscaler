using System;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;

namespace Upscaler.Runtime.Backends
{
    public class FidelityFXSuperResolutionBackend : NativeAbstractBackend
    {
        [DllImport("GfxPluginUpscaler")]
        private static extern IntPtr GetUpscaleCallbackFidelityFXSuperResolution();

        [DllImport("GfxPluginUpscaler")]
        private static extern bool LoadedCorrectlyFidelityFXSuperResolution();

        [DllImport("GfxPluginUpscaler")]
        private static extern IntPtr CreateContextFidelityFXSuperResolution();

        [DllImport("GfxPluginUpscaler")]
        private static extern Upscaler.Status UpdateContextFidelityFXSuperResolution(IntPtr handle, Vector2Int resolution, Upscaler.Quality mode, Flags flags);

        [DllImport("GfxPluginUpscaler")]
        private static extern Upscaler.Status SetImagesFidelityFXSuperResolution(IntPtr handle, IntPtr color, IntPtr depth, IntPtr motion, IntPtr output, IntPtr reactive, IntPtr opaque, bool autoReactive);

        [StructLayout(LayoutKind.Sequential)]
        private struct FidelityFXSuperResolutionUpscaleData
        {
            internal IntPtr handle;
            internal float frameTime;
            internal float sharpness;
            internal float reactiveValue;
            internal float reactiveScale;
            internal float reactiveThreshold;
            internal float farPlane;
            internal float nearPlane;
            internal float verticalFOV;
            internal Vector2 jitter;
            internal Vector2Int inputResolution;
            internal uint options;
        }

        public static bool Supported { get; }
        private static readonly IntPtr EventCallback;
        private FidelityFXSuperResolutionUpscaleData _data;
        private RenderTexture _reactive;
        private RenderTexture _opaque;

        static FidelityFXSuperResolutionBackend()
        {
            Supported = true;
            try
            {
                if (!LoadedCorrectlyPlugin() || !LoadedCorrectlyFidelityFXSuperResolution())
                {
                    Supported = false;
                    return;
                }
                EventCallback = GetUpscaleCallbackFidelityFXSuperResolution();
                if (EventCallback == IntPtr.Zero || !SystemInfo.supportsMotionVectors || !SystemInfo.supportsComputeShaders)
                {
                    Supported = false;
                    return;
                }

                var backend = new FidelityFXSuperResolutionBackend();
                var status = UpdateContextFidelityFXSuperResolution(backend._data.handle, new Vector2Int(32, 32), Upscaler.Quality.Auto, Flags.None);
                backend.Dispose();
                Supported = Upscaler.Success(status);
            }
            catch
            {
                Supported = false;
            }
        }
        
        public FidelityFXSuperResolutionBackend()
        {
            if (!Supported) return;
            DataHandle = Marshal.AllocCoTaskMem(Marshal.SizeOf<FidelityFXSuperResolutionUpscaleData>());
            _data = new FidelityFXSuperResolutionUpscaleData
            {
                handle = CreateContextFidelityFXSuperResolution()
            };
        }

        public override Upscaler.Status ComputeInputResolutionConstraints(in Upscaler upscaler, Flags flags)
        {
            if (!Supported) return Upscaler.Status.FatalRuntimeError;
            var status = UpdateContextFidelityFXSuperResolution(_data.handle, upscaler.OutputResolution, upscaler.quality, flags);
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
            if (upscaler.autoReactive && (!inputsMatch || _reactive == null))
            {
                _reactive?.Release();
                _reactive = new RenderTexture(input.width, input.height, 0, GraphicsFormat.R8_UNorm)
                {
                    enableRandomWrite = true
                };
                _reactive.Create();
            }
            if (upscaler.autoReactive && (!inputsMatch || _opaque == null))
            {
                needsImageRefresh = true;
                _opaque?.Release();
                _opaque = new RenderTexture(input.width, input.height, 0, input.graphicsFormat);
                _opaque.Create();
            }

            Output = output;
            Input = input;

            _data.sharpness = upscaler.sharpness;
            _data.reactiveValue = upscaler.reactiveMax;
            _data.reactiveScale = upscaler.reactiveScale;
            _data.reactiveThreshold = upscaler.reactiveThreshold;
            return needsImageRefresh ? SetImagesFidelityFXSuperResolution(_data.handle, input.GetNativeTexturePtr(), Depth.GetNativeTexturePtr(), Motion.GetNativeTexturePtr(), output.GetNativeTexturePtr(), _reactive?.GetNativeTexturePtr() ?? IntPtr.Zero, _opaque?.GetNativeTexturePtr() ?? IntPtr.Zero, upscaler.autoReactive) : Upscaler.Status.Success;
        }

        public override void Upscale(in Upscaler upscaler, in CommandBuffer commandBuffer, in Texture depth, in Texture motion, in Texture opaque = null)
        {
            if (!Supported || Upscaler.Failure(upscaler.CurrentStatus)) return;
            var nonJitteredProjectionMatrix = upscaler.Camera.nonJitteredProjectionMatrix;
            var planes = nonJitteredProjectionMatrix.decomposeProjection;
            _data.frameTime = Time.deltaTime * 1000.0f;
            _data.farPlane = planes.zFar;
            _data.nearPlane = planes.zNear;
            _data.verticalFOV = 2.0f * (float)Math.Atan(1.0f / nonJitteredProjectionMatrix.m11) * 180.0f / (float)Math.PI;
            _data.jitter = upscaler.Jitter - new Vector2(0.5f, 0.5f);
            _data.inputResolution = upscaler.InputResolution;
            _data.options = Convert.ToUInt32(upscaler.upscalingDebugView) << 0 |
                            Convert.ToUInt32(upscaler.shouldHistoryResetThisFrame) << 1;
            Marshal.StructureToPtr(_data, DataHandle, true);

            if (depth != Depth) commandBuffer.Blit(depth, Depth, CopyDepth, 0);
            if (motion != Motion) commandBuffer.CopyTexture(motion, Motion);
            if (upscaler.autoReactive && motion != null) commandBuffer.CopyTexture(opaque, _opaque);
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