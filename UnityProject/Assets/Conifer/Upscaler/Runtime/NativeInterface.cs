/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

/**************************************************
 * Upscaler v1.0.0                                *
 * See the OfflineManual.pdf for more information *
 **************************************************/

using System;
using System.Runtime.InteropServices;
using AOT;
using UnityEngine;
using UnityEngine.Rendering;

namespace Conifer.Upscaler
{
    public delegate void LogCallbackDelegate(IntPtr msg);

    internal struct Native
    {
        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetEventIDBase")]
        internal static extern int GetEventIDBase();

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetRenderingEventCallback")]
        internal static extern IntPtr GetRenderingEventCallback();

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_RegisterGlobalLogCallback")]
        internal static extern void RegisterLogCallback(LogCallbackDelegate logCallback);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_IsUpscalerSupported")]
        internal static extern bool IsSupported(Upscaler.Technique type);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_IsQualitySupported")]
        internal static extern bool IsSupported(Upscaler.Technique type, Upscaler.Quality mode);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_RegisterCamera")]
        internal static extern ushort RegisterCamera();

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetCameraUpscalerStatus")]
        internal static extern Upscaler.Status GetStatus(ushort camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetCameraUpscalerStatusMessage")]
        internal static extern IntPtr GetStatusMessage(ushort camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_ResetCameraUpscalerStatus")]
        internal static extern IntPtr ResetStatus(ushort camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraUpscalerStatus")]
        internal static extern Upscaler.Status SetStatus(ushort camera, Upscaler.Status status, IntPtr message);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraPerFeatureSettings")]
        internal static extern Upscaler.Status SetPerFeatureSettings(ushort camera, Vector2Int resolution, Upscaler.Technique technique, Upscaler.DlssPreset preset, Upscaler.Quality quality, float sharpness, bool hdr);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetRecommendedCameraResolution")]
        internal static extern Vector2Int GetRecommendedResolution(ushort camera);
        
        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetMaximumCameraResolution")]
        internal static extern Vector2Int GetMaximumResolution(ushort camera);
        
        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetMinimumCameraResolution")]
        internal static extern Vector2Int GetMinimumResolution(ushort camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraPerFrameData")]
        internal static extern void SetPerFrameData(ushort camera, float frameTime, float sharpness, Vector3 cameraInfo, bool autoReactive, float tcThreshold, float tcScale, float reactiveScale, float reactiveMax);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetCameraJitter")]
        internal static extern Vector2 GetJitter(ushort camera, bool advance);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_ResetCameraHistory")]
        internal static extern void ResetHistory(ushort camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_UnregisterCamera")]
        internal static extern void UnregisterCamera(ushort camera);
    }

    internal class NativeInterface
    {
        internal readonly Camera Camera;
        internal static bool Loaded;
        private readonly ushort _cameraID;
        private static readonly int EventIDBase;
        private readonly IntPtr _renderingEventCallback;
        private readonly CommandBuffer _prepareCommandBuffer;

        private enum Event
        {
            Prepare,
            Upscale,
        }

        private enum ImageID
        {
            SourceColor,
            Depth,
            Motion,
            OutputColor,
            ReactiveMask,
            TcMask,
            OpaqueColor
        }

        [MonoPInvokeCallback(typeof(LogCallbackDelegate))]
        internal static void InternalLogCallback(IntPtr msg) => Debug.Log(Marshal.PtrToStringAnsi(msg));

        static NativeInterface()
        {
            try
            {
                EventIDBase = Native.GetEventIDBase();
            }
            catch (DllNotFoundException)
            {
                return;
            }
            Loaded = true;
            Native.RegisterLogCallback(InternalLogCallback);
        }

        internal NativeInterface()
        {
            if (!Loaded) return;
            _cameraID = Native.RegisterCamera();
            _renderingEventCallback = Native.GetRenderingEventCallback();
            _prepareCommandBuffer = new CommandBuffer();
            _prepareCommandBuffer.IssuePluginEventAndData(_renderingEventCallback, (int)Event.Prepare + EventIDBase, new IntPtr(_cameraID));
        }

        ~NativeInterface()
        {
            if (!Loaded) return;
            Native.UnregisterCamera(_cameraID);
        }

        internal void SetReactiveImages(CommandBuffer cb, Texture reactiveMask, Texture opaqueColor)
        {
            if (!Loaded || reactiveMask is null || opaqueColor is null) return;
            cb.IssuePluginCustomTextureUpdateV2(_renderingEventCallback, reactiveMask, ((uint)ImageID.ReactiveMask << 16) | _cameraID);
            cb.IssuePluginCustomTextureUpdateV2(_renderingEventCallback, opaqueColor,  ((uint)ImageID.OpaqueColor  << 16) | _cameraID);
        }

        internal void Upscale(CommandBuffer cb, Texture sourceColor, Texture depth, Texture motion, Texture outputColor)
        {
            if (!Loaded || sourceColor is null || depth is null || motion is null || outputColor is null) return;
            cb.IssuePluginCustomTextureUpdateV2(_renderingEventCallback, sourceColor, ((uint)ImageID.SourceColor << 16) | _cameraID);
            cb.IssuePluginCustomTextureUpdateV2(_renderingEventCallback, depth,       ((uint)ImageID.Depth       << 16) | _cameraID);
            cb.IssuePluginCustomTextureUpdateV2(_renderingEventCallback, motion,      ((uint)ImageID.Motion      << 16) | _cameraID);
            cb.IssuePluginCustomTextureUpdateV2(_renderingEventCallback, outputColor, ((uint)ImageID.OutputColor << 16) | _cameraID);
            cb.IssuePluginEventAndData(_renderingEventCallback, (int)Event.Upscale + EventIDBase, new IntPtr(_cameraID));
        }

        internal static bool IsSupported(Upscaler.Technique type) => Loaded && Native.IsSupported(type);

        internal static bool IsSupported(Upscaler.Technique type, Upscaler.Quality mode) => Loaded && Native.IsSupported(type, mode);

        internal Upscaler.Status GetStatus() => Loaded ? Native.GetStatus(_cameraID) : Upscaler.Status.FatalRuntimeError;

        internal string GetStatusMessage() => Loaded ? Marshal.PtrToStringAnsi(Native.GetStatusMessage(_cameraID)) : "GfxPluginUpscaler shared library not found! A restart may resolve the problem.";

        internal void ResetStatus()
        {
            if (Loaded) Native.ResetStatus(_cameraID);
        }

        internal Upscaler.Status SetStatus(Upscaler.Status status, string message) => Loaded ? Native.SetStatus(_cameraID, status, Marshal.StringToHGlobalAnsi(message)) : Upscaler.Status.Success;

        internal Upscaler.Status SetPerFeatureSettings(Vector2Int resolution, Upscaler.Technique technique, Upscaler.DlssPreset preset, Upscaler.Quality quality, float sharpness, bool hdr)
        {
            if (!Loaded) return Upscaler.Status.FatalRuntimeError;
            var status = Native.SetPerFeatureSettings(_cameraID, resolution, technique, preset, quality, sharpness, hdr);
            if (Upscaler.Failure(status)) return status;
            Graphics.ExecuteCommandBuffer(_prepareCommandBuffer);
            return GetStatus();
        }

        internal Vector2Int GetRecommendedResolution() => Loaded ? Native.GetRecommendedResolution(_cameraID) : Vector2Int.zero;

        internal Vector2Int GetMaximumResolution() => Loaded ? Native.GetMaximumResolution(_cameraID) : Vector2Int.zero;

        internal Vector2Int GetMinimumResolution() => Loaded ? Native.GetMinimumResolution(_cameraID) : Vector2Int.zero;

        internal void SetPerFrameData(float frameTime, float sharpness, Vector3 cameraInfo, bool autoReactive, float tcThreshold, float tcScale, float reactiveScale, float reactiveMax)
        {
            if (Loaded) Native.SetPerFrameData(_cameraID, frameTime, sharpness, cameraInfo, autoReactive, tcThreshold, tcScale, reactiveScale, reactiveMax);
        }

        internal Vector2 GetJitter(bool advance) => Loaded ? Native.GetJitter(_cameraID, advance) : Vector2.zero;

        internal void ResetHistory()
        {
            if (Loaded) Native.ResetHistory(_cameraID);
        }
    }
}