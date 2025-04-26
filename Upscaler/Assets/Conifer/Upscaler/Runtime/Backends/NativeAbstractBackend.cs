using System;
using System.Runtime.InteropServices;
using UnityEngine;

namespace Conifer.Upscaler
{
    public abstract class NativeAbstractBackend : UpscalerBackend
    {
        [DllImport("GfxPluginUpscaler")]
        protected static extern bool LoadedCorrectlyPlugin();

        [DllImport("GfxPluginUpscaler")]
        protected static extern Vector2Int GetRecommendedResolution(IntPtr handle);

        [DllImport("GfxPluginUpscaler")]
        protected static extern Vector2Int GetMinimumResolution(IntPtr handle);

        [DllImport("GfxPluginUpscaler")]
        protected static extern Vector2Int GetMaximumResolution(IntPtr handle);

        [DllImport("GfxPluginUpscaler")]
        protected static extern void DestroyContext(IntPtr handle);

        protected IntPtr DataHandle;
        protected RenderTexture Depth;
        protected RenderTexture Motion;
        protected readonly Material CopyDepth = new (Shader.Find("Hidden/Conifer/BlitDepth"));
        protected readonly int OpaqueID = Shader.PropertyToID("_CameraOpaqueTexture");
        protected readonly int DepthID = Shader.PropertyToID("_CameraDepthTexture");
        protected readonly int MotionVectorsID = Shader.PropertyToID("_CameraMotionVectorsTexture");
    }
}