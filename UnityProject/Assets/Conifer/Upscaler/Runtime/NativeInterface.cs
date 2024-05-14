/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

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
        internal static extern bool IsSupported(Settings.Upscaler mode);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_RegisterCamera")]
        internal static extern uint RegisterCamera();

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetCameraUpscalerStatus")]
        internal static extern Upscaler.Status GetStatus(uint camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetCameraUpscalerStatusMessage")]
        internal static extern IntPtr GetStatusMessage(uint camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraPerFeatureSettings")]
        internal static extern Upscaler.Status SetPerFeatureSettings(uint camera, Settings.Resolution resolution, Settings.Upscaler upscaler, Settings.DLSSPreset preset, Settings.Quality quality, float sharpness, bool hdr);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetRecommendedCameraResolution")]
        internal static extern Settings.Resolution GetRecommendedResolution(uint camera);
        
        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetMaximumCameraResolution")]
        internal static extern Settings.Resolution GetMaximumResolution(uint camera);
        
        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetMinimumCameraResolution")]
        internal static extern Settings.Resolution GetMinimumResolution(uint camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraPerFrameData")]
        internal static extern Upscaler.Status SetPerFrameData(uint camera, float frameTime, float sharpness, Settings.CameraInfo cameraInfo, float tcThreshold, float tcScale, float reactiveScale, float reactiveMax);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetCameraJitter")]
        internal static extern Settings.Jitter GetJitter(uint camera, bool advance);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_ResetCameraHistory")]
        internal static extern void ResetHistory(uint camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_UnregisterCamera")]
        internal static extern void UnregisterCamera(uint camera);
    }

    internal class NativeInterface
    {
        internal readonly Camera Camera;
        internal static bool Loaded;
        private readonly uint _cameraID;
        private static readonly int EventIDBase;
        private readonly IntPtr _renderingEventCallback;

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
        }

        ~NativeInterface()
        {
            if (!Loaded) return;
            Native.UnregisterCamera(_cameraID);
        }

        internal void SetMaskImages(CommandBuffer cb, Texture reactiveMask, Texture tcMask, Texture opaqueColor)
        {
            if (!Loaded || reactiveMask is null || tcMask is null || opaqueColor is null) return;
            cb.IssuePluginCustomTextureUpdateV2(_renderingEventCallback, reactiveMask, _cameraID | (int)ImageID.ReactiveMask << 16);
            cb.IssuePluginCustomTextureUpdateV2(_renderingEventCallback, tcMask,       _cameraID | (int)ImageID.TcMask       << 16);
            cb.IssuePluginCustomTextureUpdateV2(_renderingEventCallback, opaqueColor,  _cameraID | (int)ImageID.OpaqueColor  << 16);
        }

        internal void Upscale(CommandBuffer cb, Texture sourceColor, Texture depth, Texture motion, Texture outputColor)
        {
            if (!Loaded || sourceColor is null || depth is null || motion is null || outputColor is null) return;
            cb.IssuePluginCustomTextureUpdateV2(_renderingEventCallback, sourceColor, _cameraID | (int)ImageID.SourceColor << 16);
            cb.IssuePluginCustomTextureUpdateV2(_renderingEventCallback, depth,       _cameraID | (int)ImageID.Depth       << 16);
            cb.IssuePluginCustomTextureUpdateV2(_renderingEventCallback, motion,      _cameraID | (int)ImageID.Motion      << 16);
            cb.IssuePluginCustomTextureUpdateV2(_renderingEventCallback, outputColor, _cameraID | (int)ImageID.OutputColor << 16);
            cb.IssuePluginEventAndData(_renderingEventCallback, (int)Event.Upscale + EventIDBase, new IntPtr(_cameraID));
        }

        internal void Prepare(CommandBuffer cb)
        {
            if (!Loaded) return;
            cb.IssuePluginEventAndData(_renderingEventCallback, (int)Event.Prepare + EventIDBase, new IntPtr(_cameraID));
        }

        internal static bool IsSupported(Settings.Upscaler mode) => Loaded && Native.IsSupported(mode);

        internal Upscaler.Status GetStatus() => Loaded ? Native.GetStatus(_cameraID) : Upscaler.Status.SoftwareError;

        internal string GetStatusMessage() => Loaded ? Marshal.PtrToStringAnsi(Native.GetStatusMessage(_cameraID)) : "";

        internal Upscaler.Status SetPerFeatureSettings(Settings.Resolution resolution, Settings.Upscaler upscaler, Settings.DLSSPreset preset, Settings.Quality quality, float sharpness, bool hdr) => Loaded ? Native.SetPerFeatureSettings(_cameraID, resolution, upscaler, preset, quality, sharpness, hdr) : Upscaler.Status.SoftwareError;

        internal Settings.Resolution GetRecommendedResolution() => Loaded ? Native.GetRecommendedResolution(_cameraID) : new Settings.Resolution();

        internal Settings.Resolution GetMaximumResolution() => Loaded ? Native.GetMaximumResolution(_cameraID) : new Settings.Resolution();

        internal Settings.Resolution GetMinimumResolution() => Loaded ? Native.GetMinimumResolution(_cameraID) : new Settings.Resolution();

        internal Upscaler.Status SetPerFrameData(float frameTime, float sharpness, Settings.CameraInfo cameraInfo, float tcThreshold, float tcScale, float reactiveScale, float reactiveMax) => Loaded ? Native.SetPerFrameData(_cameraID, frameTime, sharpness, cameraInfo, tcThreshold, tcScale, reactiveScale, reactiveMax) : Upscaler.Status.SoftwareError;

        internal Settings.Jitter GetJitter(bool advance) => Loaded ? Native.GetJitter(_cameraID, advance) : new Settings.Jitter();

        internal void ResetHistory()
        {
            if (!Loaded) return;
            Native.ResetHistory(_cameraID);
        }
    }
}