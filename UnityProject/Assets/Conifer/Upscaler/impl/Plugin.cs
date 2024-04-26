/**********************************************************************************************************************
 * Conifer Limited License                                                                                            *
 * This software is provided under a custom limited license, subject to the following terms and conditions:           *
 * Individuals or entities who have purchased this software are granted the right to modify the source code for their *
 *  personal or internal business use only.                                                                           *
 * Redistribution or copying of the source code, in whole or in part, including modified source code, is strictly     *
 *  prohibited without prior written consent from the original author.                                                *
 * Any usage of the source code, including custom modifications, must be accompanied by this license as well as a     *
 *  prominent credit in the final product attributing the original author of the software.                            *
 * The original author reserves all rights not expressly granted herein.                                              *
 * Copyright © 2024 Conifer Computing Company. All rights reserved.                                                   *
 **********************************************************************************************************************/

/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

using System;
using System.Runtime.InteropServices;
using AOT;
using UnityEngine;
using UnityEngine.Rendering;

namespace Conifer.Upscaler.impl
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
        internal static extern Upscaler.Status SetPerFeatureSettings(uint camera, Settings.Resolution resolution,
            Settings.Upscaler upscaler, Settings.DLSSPreset preset, Settings.Quality quality, bool hdr);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetRecommendedCameraResolution")]
        internal static extern Settings.Resolution GetRecommendedResolution(uint camera);
        
        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetMaximumCameraResolution")]
        internal static extern Settings.Resolution GetMaximumResolution(uint camera);
        
        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetMinimumCameraResolution")]
        internal static extern Settings.Resolution GetMinimumResolution(uint camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraPerFrameData")]
        internal static extern Upscaler.Status SetPerFrameData(uint camera, float frameTime, float sharpness, Settings.CameraInfo cameraInfo);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetCameraJitter")]
        internal static extern Settings.Jitter GetJitter(uint camera, bool advance);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_ResetCameraHistory")]
        internal static extern void ResetHistory(uint camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_UnregisterCamera")]
        internal static extern void UnregisterCamera(uint camera);
    }
    
    internal class Plugin
    {
        internal readonly Camera Camera;
        private readonly uint _cameraID = Native.RegisterCamera();
        private static readonly int EventIDBase = Native.GetEventIDBase();
        private readonly IntPtr _renderingEventCallback = Native.GetRenderingEventCallback();

        private enum Event
        {
            Prepare,
            Upscale,
        }

        [MonoPInvokeCallback(typeof(LogCallbackDelegate))]
        internal static void InternalLogCallback(IntPtr msg) => Debug.Log(Marshal.PtrToStringAnsi(msg));

        static Plugin() => Native.RegisterLogCallback(InternalLogCallback);

        ~Plugin() => Native.UnregisterCamera(_cameraID);

        internal void Upscale(CommandBuffer cb, Texture sourceColor, Texture sourceDepth, Texture motion, Texture outputColor)
        {
            if (sourceColor is null || sourceDepth is null || motion is null || outputColor is null) return;
            cb.IssuePluginCustomTextureUpdateV2(_renderingEventCallback, sourceColor, _cameraID | (int)UpscalingData.ImageID.SourceColor << 16);
            cb.IssuePluginCustomTextureUpdateV2(_renderingEventCallback, sourceDepth, _cameraID | (int)UpscalingData.ImageID.SourceDepth << 16);
            cb.IssuePluginCustomTextureUpdateV2(_renderingEventCallback, motion,      _cameraID | (int)UpscalingData.ImageID.Motion      << 16);
            cb.IssuePluginCustomTextureUpdateV2(_renderingEventCallback, outputColor, _cameraID | (int)UpscalingData.ImageID.OutputColor << 16);
            cb.IssuePluginEventAndData(_renderingEventCallback, (int)Event.Upscale + EventIDBase, new IntPtr(_cameraID));
        }

        internal void Prepare(CommandBuffer cb) =>
            cb.IssuePluginEventAndData(_renderingEventCallback, (int)Event.Prepare + EventIDBase, new IntPtr(_cameraID));

        internal static bool IsSupported(Settings.Upscaler mode) => Native.IsSupported(mode);
        
        internal Upscaler.Status GetStatus() => Native.GetStatus(_cameraID);

        internal string GetStatusMessage() => Marshal.PtrToStringAnsi(Native.GetStatusMessage(_cameraID));

        internal Upscaler.Status SetPerFeatureSettings(Settings.Resolution resolution, Settings.Upscaler upscaler, Settings.DLSSPreset preset, Settings.Quality quality, bool hdr) =>
            Native.SetPerFeatureSettings(_cameraID, resolution, upscaler, preset, quality, hdr);

        internal Settings.Resolution GetRecommendedResolution() =>
            Native.GetRecommendedResolution(_cameraID);

        internal Settings.Resolution GetMaximumResolution() => Native.GetMaximumResolution(_cameraID);

        internal Settings.Resolution GetMinimumResolution() => Native.GetMinimumResolution(_cameraID);

        internal Upscaler.Status SetPerFrameData(float frameTime, float sharpness, Settings.CameraInfo cameraInfo) =>
            Native.SetPerFrameData(_cameraID, frameTime, sharpness, cameraInfo);

        internal Settings.Jitter GetJitter(bool advance) => Native.GetJitter(_cameraID, advance);

        internal void ResetHistory() => Native.ResetHistory(_cameraID);
    }
}