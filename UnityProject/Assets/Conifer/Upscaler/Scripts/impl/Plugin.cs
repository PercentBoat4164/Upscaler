using System;
using System.Runtime.InteropServices;
using UnityEngine.Device;
using UnityEngine.Experimental.Rendering;

namespace Conifer.Upscaler.Scripts.impl
{
    public static class Plugin
    {
        public enum Event
        {
            Upscale,
            Prepare
        }

        public static GraphicsFormat MotionFormat()
        {
            return GraphicsFormat.R16G16_SFloat;
        }

        public static GraphicsFormat ColorFormat(bool hdrActive)
        {
            return SystemInfo.GetGraphicsFormat(hdrActive ? DefaultFormat.HDR : DefaultFormat.LDR);
        }

        public static GraphicsFormat DepthFormat()
        {
            return SystemInfo.GetGraphicsFormat(DefaultFormat.DepthStencil);
        }

        public delegate void InternalErrorCallback(IntPtr data, Upscaler.UpscalerStatus er, IntPtr p);

        public delegate void InternalLogCallback(IntPtr msg);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetError")]
        public static extern Upscaler.UpscalerStatus GetError(Upscaler.UpscalerMode upscalerMode);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetErrorMessage")]
        public static extern IntPtr GetErrorMessage(Upscaler.UpscalerMode upscalerMode);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetCurrentError")]
        public static extern Upscaler.UpscalerStatus GetCurrentError();

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetCurrentErrorMessage")]
        public static extern IntPtr GetCurrentErrorMessage();

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_ResetHistory")]
        public static extern void ResetHistory();

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetDepthBuffer")]
        public static extern void SetDepthBuffer(IntPtr handle, GraphicsFormat format);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetInputColor")]
        public static extern void SetInputColor(IntPtr handle, GraphicsFormat format);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetMotionVectors")]
        public static extern void SetMotionVectors(IntPtr handle, GraphicsFormat format);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetOutputColor")]
        public static extern void SetOutputColor(IntPtr handle, GraphicsFormat format);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetUpscaler")]
        public static extern Upscaler.UpscalerStatus SetUpscaler(Upscaler.UpscalerMode upscalerMode);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetFramebufferSettings")]
        public static extern Upscaler.UpscalerStatus SetFramebufferSettings(uint width, uint height,
            Upscaler.QualityMode qualityMode,
            bool hdr);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetRecommendedInputResolution")]
        public static extern ulong GetRecommendedInputResolution();

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetSharpnessValue")]
        public static extern Upscaler.UpscalerStatus SetSharpnessValue(float sharpness);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_Shutdown")]
        public static extern void Shutdown();

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_ShutdownPlugin")]
        public static extern void ShutdownPlugin();

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetRenderingEventCallback")]
        public static extern IntPtr GetRenderingEventCallback();

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetJitterInformation")]
        public static extern void SetJitterInformation(float x, float y);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_InitializePlugin")]
        public static extern void Initialize(IntPtr upscalerObject, InternalErrorCallback errorCallback,
            InternalLogCallback logCallback=null);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_ResetStatus")]
        public static extern void ResetStatus();
    }
}