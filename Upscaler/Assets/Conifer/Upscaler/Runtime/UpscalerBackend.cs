using System;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;

namespace Conifer.Upscaler
{
    public abstract class UpscalerBackend : IDisposable
    {
        protected enum SupportState
        {
            Untested,
            Unsupported,
            Supported
        }

        protected static readonly int BlitTextureID = Shader.PropertyToID("_BlitTexture");
#if CONIFER_UPSCALER_USE_URP
        protected static readonly int BlitScaleBiasID = Shader.PropertyToID("_BlitScaleBias");
#endif
        protected static readonly Matrix4x4 Ortho = Matrix4x4.Ortho(-1, 1, 1, -1, 1, -1);
        protected static readonly Matrix4x4 LookAt = Matrix4x4.LookAt(Vector3.back, Vector3.forward, Vector3.up);

        [DllImport("GfxPluginUpscaler")]
        protected static extern bool Native_LoadedCorrectly();

        [DllImport("GfxPluginUpscaler")]
        protected static extern void Native_SetLogLevel(LogType type);

        [DllImport("GfxPluginUpscaler")]
        protected static extern void Native_SetFrameGeneration(IntPtr hWnd);

        [DllImport("GfxPluginUpscaler")]
        protected static extern int Native_GetEventIDBase();

        [DllImport("GfxPluginUpscaler")]
        protected static extern IntPtr Native_GetRenderingEventCallback();

        [DllImport("GfxPluginUpscaler", EntryPoint = "IsUpscalerSupported")]
        protected static extern bool Native_IsSupported(Upscaler.Technique type);

        [DllImport("GfxPluginUpscaler", EntryPoint = "IsQualitySupported")]
        protected static extern bool Native_IsSupported(Upscaler.Technique type, Upscaler.Quality mode);

        [DllImport("GfxPluginUpscaler")]
        protected static extern ushort Native_RegisterCamera();

        [DllImport("GfxPluginUpscaler", EntryPoint = "GetCameraUpscalerStatus")]
        protected static extern Upscaler.Status Native_GetStatus(ushort camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "GetCameraUpscalerStatusMessage")]
        protected static extern IntPtr Native_GetStatusMessage(ushort camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "SetCameraUpscalerStatus")]
        protected static extern Upscaler.Status Native_SetStatus(ushort camera, Upscaler.Status status, IntPtr message);

        [DllImport("GfxPluginUpscaler", EntryPoint = "SetCameraPerFeatureSettings")]
        protected static extern Upscaler.Status Native_SetPerFeatureSettings(ushort camera, Vector2Int resolution, Upscaler.Technique technique, Upscaler.DlssPreset preset, Upscaler.Quality quality, bool hdr);

        [DllImport("GfxPluginUpscaler", EntryPoint = "GetRecommendedCameraResolution")]
        protected static extern Vector2Int Native_GetRecommendedResolution(ushort camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "GetMaximumCameraResolution")]
        protected static extern Vector2Int Native_GetMaximumResolution(ushort camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "GetMinimumCameraResolution")]
        protected static extern Vector2Int Native_GetMinimumResolution(ushort camera);

        [DllImport("GfxPluginUpscaler")]
        protected static extern void Native_SetUpscalingImages(ushort camera, IntPtr color, IntPtr depth, IntPtr motion, IntPtr output, IntPtr reactive, IntPtr opaque, bool autoReactive);

        [DllImport("GfxPluginUpscaler")]
        protected static extern void Native_UnregisterCamera(ushort camera);

        protected ushort Camera;

        public abstract Upscaler.Status ApplySettings(in Upscaler upscaler);
        public abstract bool ApplyRefresh(in Upscaler upscaler, in Texture input, in Texture output, in Texture depth=null, in Texture motion=null, bool autoReactive=false, in Texture opaque=null, in Texture reactive=null);
        public abstract void Upscale(in CommandBuffer commandBuffer = null);

        public void Dispose() { }
    }
}