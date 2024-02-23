using System;
using System.Runtime.InteropServices;
using AOT;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
using SystemInfo = UnityEngine.Device.SystemInfo;

namespace Conifer.Upscaler.Scripts.impl
{
    public delegate void LogCallbackDelegate(IntPtr msg);

    internal struct Native
    {
        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetRenderingEventCallback")]
        public static extern IntPtr GetRenderingEventCallback();

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_RegisterGlobalLogCallback")]
        public static extern void RegisterLogCallback(LogCallbackDelegate logCallback);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_IsUpscalerSupported")]
        public static extern bool IsSupported(Settings.Upscaler mode);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_RegisterCamera")]
        public static extern void RegisterCamera(IntPtr camera);
        
        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraUpscaler")]
        public static extern Upscaler.Status SetUpscaler(IntPtr camera, Settings.Upscaler mode);
        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetCameraUpscalerStatus")]
        public static extern Upscaler.Status GetStatus(IntPtr camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetCameraUpscalerStatusMessage")]
        public static extern IntPtr GetStatusMessage(IntPtr camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraPerFeatureSettings")]
        public static extern Upscaler.Status SetPerFeatureSettings(IntPtr camera, Settings.Resolution resolution,
            Settings.Upscaler upscaler, Settings.Quality quality, bool hdr);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetRecommendedCameraResolution")]
        public static extern Settings.Resolution GetRecommendedResolution(IntPtr camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraPerFrameData")]
        public static extern Upscaler.Status SetPerFrameData(IntPtr camera, float frameTime, float sharpness, Settings.CameraInfo cameraInfo);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetCameraJitter")]
        public static extern Settings.Jitter GetJitter(IntPtr camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_ResetCameraHistory")]
        public static extern void ResetHistory(IntPtr camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraDepth")]
        public static extern Upscaler.Status SetDepth(IntPtr camera, IntPtr handle, GraphicsFormat format);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraInputColor")]
        public static extern Upscaler.Status SetInputColor(IntPtr camera, IntPtr handle, GraphicsFormat format);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraMotionVectors")]
        public static extern Upscaler.Status SetMotionVectors(IntPtr camera, IntPtr handle, GraphicsFormat format);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraOutputColor")]
        public static extern Upscaler.Status SetOutputColor(IntPtr camera, IntPtr handle, GraphicsFormat format);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_UnregisterCamera")]
        public static extern void UnregisterCamera(IntPtr camera);
    }

    internal class Plugin
    {
        public readonly Camera Camera;
        private GCHandle _cameraHandle;

        private enum Event
        {
            Upscale,
            Prepare
        }

        [MonoPInvokeCallback(typeof(LogCallbackDelegate))]
        internal static void InternalLogCallback(IntPtr msg) => Debug.Log(Marshal.PtrToStringAnsi(msg));

        static Plugin() => Native.RegisterLogCallback(InternalLogCallback);

        public Plugin(Camera camera)
        {
            Camera = camera;
            _cameraHandle = GCHandle.Alloc(camera);
            Native.RegisterCamera(GCHandle.ToIntPtr(_cameraHandle));
        }

        ~Plugin()
        {
            Native.UnregisterCamera(GCHandle.ToIntPtr(_cameraHandle));
            _cameraHandle.Free();
        }

        internal void Upscale(CommandBuffer cb) =>
            cb.IssuePluginEventAndData(Native.GetRenderingEventCallback(), (int)Event.Upscale, GCHandle.ToIntPtr(_cameraHandle));

        internal void Prepare(CommandBuffer cb) =>
            cb.IssuePluginEventAndData(Native.GetRenderingEventCallback(), (int)Event.Prepare, GCHandle.ToIntPtr(_cameraHandle));

        internal static bool IsSupported(Settings.Upscaler mode) =>
            Native.IsSupported(mode);

        internal Upscaler.Status SetUpscaler(Settings.Upscaler mode) =>
            Native.SetUpscaler(GCHandle.ToIntPtr(_cameraHandle), mode);
        
        internal Upscaler.Status GetStatus() =>
            Native.GetStatus(GCHandle.ToIntPtr(_cameraHandle));

        internal string GetStatusMessage() =>
            Marshal.PtrToStringAnsi(Native.GetStatusMessage(GCHandle.ToIntPtr(_cameraHandle)));

        internal Upscaler.Status SetPerFeatureSettings(Settings.Resolution resolution, Settings.Upscaler upscaler, Settings.Quality quality, bool hdr) =>
            Native.SetPerFeatureSettings(GCHandle.ToIntPtr(_cameraHandle), resolution, upscaler, quality, hdr);

        internal Settings.Resolution GetRecommendedResolution() =>
            Native.GetRecommendedResolution(GCHandle.ToIntPtr(_cameraHandle));

        internal Upscaler.Status SetPerFrameData(float frameTime, float sharpness, Settings.CameraInfo cameraInfo) =>
            Native.SetPerFrameData(GCHandle.ToIntPtr(_cameraHandle), frameTime, sharpness, cameraInfo);

        internal Settings.Jitter GetJitter() =>
            Native.GetJitter(GCHandle.ToIntPtr(_cameraHandle));

        internal void ResetHistory() =>
            Native.ResetHistory(GCHandle.ToIntPtr(_cameraHandle));

        internal Upscaler.Status SetDepth(RenderTexture texture) =>
            Native.SetDepth(GCHandle.ToIntPtr(_cameraHandle), texture.GetNativeDepthBufferPtr(), texture.depthStencilFormat);

        internal Upscaler.Status SetInputColor(RenderTexture texture) =>
            Native.SetInputColor(GCHandle.ToIntPtr(_cameraHandle), texture.GetNativeTexturePtr(), texture.graphicsFormat);

        internal Upscaler.Status SetMotionVectors(RenderTexture texture) =>
            Native.SetMotionVectors(GCHandle.ToIntPtr(_cameraHandle), texture.GetNativeTexturePtr(), texture.graphicsFormat);

        internal Upscaler.Status SetOutputColor(RenderTexture texture) =>
            Native.SetOutputColor(GCHandle.ToIntPtr(_cameraHandle), texture.GetNativeTexturePtr(), texture.graphicsFormat);

        internal void UnregisterCamera() =>
            Native.UnregisterCamera(GCHandle.ToIntPtr(_cameraHandle));

        internal static GraphicsFormat MotionFormat()
        {
            return GraphicsFormat.R16G16_SFloat;
        }

        internal GraphicsFormat ColorFormat()
        {
            return SystemInfo.GetGraphicsFormat(Camera.allowHDR ? DefaultFormat.HDR : DefaultFormat.LDR);
        }

        internal static GraphicsFormat DepthFormat()
        {
            return SystemInfo.GetGraphicsFormat(DefaultFormat.DepthStencil);
        }
    }
}