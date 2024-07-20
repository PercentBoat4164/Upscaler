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

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetCameraJitter")]
        internal static extern Vector2 GetJitter(ushort camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_ResetCameraHistory")]
        internal static extern void ResetHistory(ushort camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_UnregisterCamera")]
        internal static extern void UnregisterCamera(ushort camera);
    }

    internal class NativeInterface
    {
        internal readonly Camera Camera;
        internal static readonly bool Loaded;
        private readonly ushort _cameraID;
        private static readonly int EventIDBase;
        private readonly IntPtr _renderingEventCallback;
        private readonly IntPtr _dataPtr = Marshal.AllocHGlobal(Marshal.SizeOf<UpscaleData>());

        [MonoPInvokeCallback(typeof(LogCallbackDelegate))]
        internal static void InternalLogCallback(IntPtr msg) => Debug.Log(Marshal.PtrToStringAnsi(msg));

        private struct UpscaleData
        {
            internal UpscaleData(Upscaler upscaler, ushort cameraID, IntPtr color, IntPtr depth, IntPtr motion, IntPtr output, IntPtr reactive, IntPtr opaque)
            {
                _color = color;
                _depth = depth;
                _motion = motion;
                _output = output;
                _reactive = reactive;
                _opaque = opaque;
                _frameTime = Time.deltaTime * 1000.0F;
                _sharpness = upscaler.sharpness;
                _reactiveValue = upscaler.reactiveValue;
                _reactiveScale = upscaler.reactiveScale;
                _reactiveThreshold = upscaler.reactiveThreshold;
                var camera = upscaler.GetComponent<Camera>();
                _cameraInfo = new Vector3(camera.farClipPlane, camera.nearClipPlane, camera.fieldOfView);
                _camera = cameraID;
                _autoReactive = upscaler.useReactiveMask ? 1 : 0;
            }

            private IntPtr _color;
            private IntPtr _depth;
            private IntPtr _motion;
            private IntPtr _output;
            private IntPtr _reactive;
            private IntPtr _opaque;
            private float _frameTime;
            private float _sharpness;
            private float _reactiveValue;
            private float _reactiveScale;
            private float _reactiveThreshold;
            private Vector3 _cameraInfo;
            private ushort _camera;
            private int _autoReactive;
        }

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
            if (Loaded) Native.UnregisterCamera(_cameraID);
        }

        internal void Upscale(CommandBuffer cb, Upscaler upscaler, IntPtr color, IntPtr depth, IntPtr motion, IntPtr output, IntPtr reactive, IntPtr opaque)
        {
            Marshal.StructureToPtr(new UpscaleData(upscaler, _cameraID, color, depth, motion, output, reactive, opaque), _dataPtr, true);
            if (Loaded) cb.IssuePluginEventAndData(_renderingEventCallback, EventIDBase, _dataPtr);
        }

        internal static bool IsSupported(Upscaler.Technique type) => Loaded && Native.IsSupported(type);

        internal static bool IsSupported(Upscaler.Technique type, Upscaler.Quality mode) => Loaded && Native.IsSupported(type, mode);

        internal Upscaler.Status GetStatus() => Loaded ? Native.GetStatus(_cameraID) : Upscaler.Status.FatalRuntimeError;

        internal string GetStatusMessage() => Loaded ? Marshal.PtrToStringAnsi(Native.GetStatusMessage(_cameraID)) : "GfxPluginUpscaler shared library not found! A restart may resolve the problem.";

        internal Upscaler.Status SetStatus(Upscaler.Status status, string message) => Loaded ? Native.SetStatus(_cameraID, status, Marshal.StringToHGlobalAnsi(message)) : Upscaler.Status.Success;

        internal Upscaler.Status SetPerFeatureSettings(Vector2Int resolution, Upscaler.Technique technique, Upscaler.DlssPreset preset, Upscaler.Quality quality, float sharpness, bool hdr) => Loaded ? Native.SetPerFeatureSettings(_cameraID, resolution, technique, preset, quality, sharpness, hdr) : Upscaler.Status.FatalRuntimeError;

        internal Vector2Int GetRecommendedResolution() => Loaded ? Native.GetRecommendedResolution(_cameraID) : Vector2Int.zero;

        internal Vector2Int GetMaximumResolution() => Loaded ? Native.GetMaximumResolution(_cameraID) : Vector2Int.zero;

        internal Vector2Int GetMinimumResolution() => Loaded ? Native.GetMinimumResolution(_cameraID) : Vector2Int.zero;

        internal Vector2 GetJitter() => Loaded ? Native.GetJitter(_cameraID) : Vector2.zero;

        internal void ResetHistory()
        {
            if (Loaded) Native.ResetHistory(_cameraID);
        }
    }
}