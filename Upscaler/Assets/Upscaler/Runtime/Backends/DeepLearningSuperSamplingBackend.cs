using System;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;

namespace Upscaler.Runtime.Backends
{
    public class DeepLearningSuperSamplingBackend : NativeAbstractBackend
    {
        [DllImport("GfxPluginUpscaler")]
        private static extern IntPtr GetUpscaleCallbackDeepLearningSuperSampling();

        [DllImport("GfxPluginUpscaler")]
        private static extern bool LoadedCorrectlyDeepLearningSuperSampling();

        [DllImport("GfxPluginUpscaler")]
        private static extern IntPtr CreateContextDeepLearningSuperSampling();

        [DllImport("GfxPluginUpscaler")]
        private static extern Upscaler.Status UpdateContextDeepLearningSuperSampling(IntPtr handle, Vector2Int resolution, Upscaler.Preset preset, Upscaler.Quality mode, Flags flags);

        [DllImport("GfxPluginUpscaler")]
        private static extern Upscaler.Status SetImagesDeepLearningSuperSampling(IntPtr handle, IntPtr color, IntPtr depth, IntPtr motion, IntPtr output, IntPtr reactive, IntPtr opaque, bool autoReactive);

        [StructLayout(LayoutKind.Sequential)]
        private struct DeepLearningSuperSamplingUpscaleData
        {
            internal IntPtr handle;
            internal Matrix4x4 viewToClip;
            internal Matrix4x4 clipToView;
            internal Matrix4x4 clipToPrevClip;
            internal Matrix4x4 prevClipToClip;
            internal Vector3 position;
            internal Vector3 up;
            internal Vector3 right;
            internal Vector3 forward;
            internal float farPlane;
            internal float nearPlane;
            internal float verticalFOV;
            internal Vector2 jitter;
            internal Vector2Int inputResolution;
            internal bool resetHistory;
        }

        public static bool Supported { get; }
        private static readonly IntPtr EventCallback;
        private DeepLearningSuperSamplingUpscaleData _data;
        private Matrix4x4 _lastViewToClip;
        private Matrix4x4 _lastWorldToCamera;

        static DeepLearningSuperSamplingBackend()
        {
            Supported = true;
            try
            {
                if (!LoadedCorrectlyPlugin() || !LoadedCorrectlyDeepLearningSuperSampling())
                {
                    Supported = false;
                    return;
                }
                EventCallback = GetUpscaleCallbackDeepLearningSuperSampling();
                if (EventCallback == IntPtr.Zero || !SystemInfo.supportsMotionVectors || !SystemInfo.supportsComputeShaders)
                {
                    Supported = false;
                    return;
                }

                var backend = new DeepLearningSuperSamplingBackend();
                var status = UpdateContextDeepLearningSuperSampling(backend._data.handle, new Vector2Int(32, 32), Upscaler.Preset.Default, Upscaler.Quality.Auto, Flags.None);
                backend.Dispose();
                Supported = Upscaler.Success(status);
            }
            catch
            {
                Supported = false;
            }
        }

        public DeepLearningSuperSamplingBackend()
        {
            if (!Supported) return;
            DataHandle = Marshal.AllocCoTaskMem(Marshal.SizeOf<DeepLearningSuperSamplingUpscaleData>());
            _data = new DeepLearningSuperSamplingUpscaleData
            {
                handle = CreateContextDeepLearningSuperSampling()
            };
        }

        public override Upscaler.Status ComputeInputResolutionConstraints(in Upscaler upscaler, Flags flags)
        {
            if (!Supported) return Upscaler.Status.FatalRuntimeError;
            var status = UpdateContextDeepLearningSuperSampling(_data.handle, upscaler.OutputResolution, upscaler.preset, upscaler.quality, flags);
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

            return !needsImageRefresh ? Upscaler.Status.Success : SetImagesDeepLearningSuperSampling(_data.handle, input.GetNativeTexturePtr(), Depth.GetNativeTexturePtr(), Motion.GetNativeTexturePtr(), output.GetNativeTexturePtr(), IntPtr.Zero, IntPtr.Zero, false);
        }

        public override void Upscale(in Upscaler upscaler, in CommandBuffer commandBuffer, in Texture depth, in Texture motion, in Texture opaque = null)
        {
            if (!Supported) return;
            var cam = upscaler.Camera;
            var nonJitteredProjectionMatrix = cam.nonJitteredProjectionMatrix;
            _data.viewToClip = GL.GetGPUProjectionMatrix(nonJitteredProjectionMatrix, true).inverse;
            _data.clipToView = _data.viewToClip.inverse;
            var cameraToWorld = GL.GetGPUProjectionMatrix(cam.worldToCameraMatrix, true).inverse;
            _data.clipToPrevClip = _data.clipToView * cameraToWorld * _lastWorldToCamera * _lastViewToClip;
            _data.prevClipToClip = _data.clipToPrevClip.inverse;
            var planes = nonJitteredProjectionMatrix.decomposeProjection;
            _data.farPlane = planes.zFar;
            _data.nearPlane = planes.zNear;
            _data.verticalFOV = 2.0f * (float)Math.Atan(1.0f / nonJitteredProjectionMatrix.m11) * 180.0f / (float)Math.PI;
            var transform = cam.transform;
            _data.position = transform.position;
            _data.up = transform.up;
            _data.right = transform.right;
            _data.forward = transform.forward;
            _data.jitter = upscaler.Jitter - new Vector2(0.5f, 0.5f);
            _data.inputResolution = upscaler.InputResolution;
            _data.resetHistory = upscaler.shouldHistoryResetThisFrame;
            Marshal.StructureToPtr(_data, DataHandle, true);

            _lastViewToClip = _data.viewToClip;
            _lastWorldToCamera = cameraToWorld.inverse;

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