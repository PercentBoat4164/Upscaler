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
        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetEventIDBase")]
        public static extern int GetEventIDBase();

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetRenderingEventCallback")]
        public static extern IntPtr GetRenderingEventCallback();

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_RegisterGlobalLogCallback")]
        public static extern void RegisterLogCallback(LogCallbackDelegate logCallback);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_IsUpscalerSupported")]
        public static extern bool IsSupported(Settings.Upscaler mode);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_RegisterCamera")]
        public static extern uint RegisterCamera();

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetCameraUpscalerStatus")]
        public static extern Upscaler.Status GetStatus(uint camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetCameraUpscalerStatusMessage")]
        public static extern IntPtr GetStatusMessage(uint camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraPerFeatureSettings")]
        public static extern Upscaler.Status SetPerFeatureSettings(uint camera, Settings.Resolution resolution,
            Settings.Upscaler upscaler, Settings.Quality quality, bool hdr);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetRecommendedCameraResolution")]
        public static extern Settings.Resolution GetRecommendedResolution(uint camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraPerFrameData")]
        public static extern Upscaler.Status SetPerFrameData(uint camera, float frameTime, float sharpness, Settings.CameraInfo cameraInfo);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetCameraJitter")]
        public static extern Settings.Jitter GetJitter(uint camera, bool advance);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_ResetCameraHistory")]
        public static extern void ResetHistory(uint camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_UnregisterCamera")]
        public static extern void UnregisterCamera(uint camera);
    }
    
    internal class Plugin
    {
        public readonly Camera Camera;
        private readonly uint _cameraID;
        private static readonly int EventIDBase = Native.GetEventIDBase();

        private enum Event
        {
            Prepare,
            Upscale,
        }

        [MonoPInvokeCallback(typeof(LogCallbackDelegate))]
        internal static void InternalLogCallback(IntPtr msg) => Debug.Log(Marshal.PtrToStringAnsi(msg));

        static Plugin() => Native.RegisterLogCallback(InternalLogCallback);

        public Plugin(Camera camera)
        {
            Camera = camera;
            _cameraID = Native.RegisterCamera();
        }

        ~Plugin() => Native.UnregisterCamera(_cameraID);

        internal void Upscale(CommandBuffer cb, Texture sourceColor, Texture sourceDepth, Texture motion, Texture outputColor)
        {
            if (sourceColor is null || sourceDepth is null || motion is null || outputColor is null)
                Debug.LogError("Upscaler received a null Texture object. Skipping the 'Upscale' step for this frame.");
            cb.IssuePluginCustomTextureUpdateV2(Native.GetRenderingEventCallback(), sourceColor, _cameraID | (int)UpscalingData.ImageID.SourceColor << 16);
            cb.IssuePluginCustomTextureUpdateV2(Native.GetRenderingEventCallback(), sourceDepth, _cameraID | (int)UpscalingData.ImageID.SourceDepth << 16);
            cb.IssuePluginCustomTextureUpdateV2(Native.GetRenderingEventCallback(), motion,      _cameraID | (int)UpscalingData.ImageID.Motion      << 16);
            cb.IssuePluginCustomTextureUpdateV2(Native.GetRenderingEventCallback(), outputColor, _cameraID | (int)UpscalingData.ImageID.OutputColor << 16);
            cb.IssuePluginEventAndData(Native.GetRenderingEventCallback(), (int)Event.Upscale + EventIDBase, new IntPtr(_cameraID));
        }

        internal void Prepare(CommandBuffer cb) =>
            cb.IssuePluginEventAndData(Native.GetRenderingEventCallback(), (int)Event.Prepare + EventIDBase, new IntPtr(_cameraID));

        internal static bool IsSupported(Settings.Upscaler mode) =>
            Native.IsSupported(mode);
        
        internal Upscaler.Status GetStatus() =>
            Native.GetStatus(_cameraID);

        internal string GetStatusMessage() =>
            Marshal.PtrToStringAnsi(Native.GetStatusMessage(_cameraID));

        internal Upscaler.Status SetPerFeatureSettings(Settings.Resolution resolution, Settings.Upscaler upscaler, Settings.Quality quality, bool hdr) =>
            Native.SetPerFeatureSettings(_cameraID, resolution, upscaler, quality, hdr);

        internal Settings.Resolution GetRecommendedResolution() =>
            Native.GetRecommendedResolution(_cameraID);

        internal Upscaler.Status SetPerFrameData(float frameTime, float sharpness, Settings.CameraInfo cameraInfo) =>
            Native.SetPerFrameData(_cameraID, frameTime, sharpness, cameraInfo);

        internal Settings.Jitter GetJitter(bool advance) =>
            Native.GetJitter(_cameraID, advance);

        internal void ResetHistory() => Native.ResetHistory(_cameraID);
        
        /**@todo Remove the camera from this class.*/
        internal RenderTextureFormat ColorFormat() => Camera.allowHDR ? RenderTextureFormat.DefaultHDR : RenderTextureFormat.Default;
    }
}