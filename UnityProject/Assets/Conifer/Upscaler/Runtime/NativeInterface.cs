/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

/**************************************************
 * Upscaler v1.1.2                                *
 * See the UserManual.pdf for more information    *
 **************************************************/

using System;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;

namespace Conifer.Upscaler
{

    internal struct Native
    {
        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_LoadedCorrectly")]
        internal static extern bool LoadedCorrectly();

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_DLSSLoadedCorrectly")]
        internal static extern bool DLSSLoadedCorrectly();

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetLogLevel")]
        internal static extern void SetLogLevel(LogType type);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetEventIDBase")]
        internal static extern int GetEventIDBase();

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetRenderingEventCallback")]
        internal static extern IntPtr GetRenderingEventCallback();

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
        internal static extern Vector2 GetJitter(ushort camera, float inputWidth);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_ResetCameraHistory")]
        internal static extern void ResetHistory(ushort camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetImages")]
        internal static extern void SetImages(ushort camera, IntPtr color, IntPtr depth, IntPtr motion, IntPtr output, IntPtr reactive, IntPtr opaque, bool autoReactive);

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

        private struct UpscaleData
        {
            internal UpscaleData(Upscaler upscaler, ushort cameraID)
            {
                _frameTime = Time.deltaTime * 1000.0F;
                _sharpness = upscaler.sharpness;
                _reactiveValue = upscaler.reactiveMax;
                _reactiveScale = upscaler.reactiveScale;
                _reactiveThreshold = upscaler.reactiveThreshold;
                _camera = cameraID;
                var camera = upscaler.GetComponent<Camera>();
                _viewToClip = GL.GetGPUProjectionMatrix(camera.nonJitteredProjectionMatrix, true).inverse;
                _clipToView = _viewToClip.inverse;
                var cameraToWorld = GL.GetGPUProjectionMatrix(camera.worldToCameraMatrix, true).inverse;
                _clipToPrevClip = _clipToView * cameraToWorld * upscaler.LastWorldToCamera * upscaler.LastViewToClip;
                _prevClipToClip = _clipToPrevClip.inverse;
                var planes = camera.nonJitteredProjectionMatrix.decomposeProjection;
                _farPlane = planes.zFar;
                _nearPlane = planes.zNear;
                _verticalFOV = 2.0f * (float)Math.Atan(1.0f / camera.nonJitteredProjectionMatrix.m11) * 180.0f /
                               (float)Math.PI;
                _position = camera.transform.position;
                _up = camera.transform.up;
                _right = camera.transform.right;
                _forward = camera.transform.forward;
                _orthographic_debugView = (camera.orthographic ? 0b1U : 0b0U) | (upscaler.debugView ? 0b10U : 0b0U);
                upscaler.LastViewToClip = _viewToClip;
                upscaler.LastWorldToCamera = cameraToWorld.inverse;
            }

            private float _frameTime;
            private float _sharpness;
            private float _reactiveValue;
            private float _reactiveScale;
            private float _reactiveThreshold;
            private ushort _camera;
            private Matrix4x4 _viewToClip;
            private Matrix4x4 _clipToView;
            private Matrix4x4 _clipToPrevClip;
            private Matrix4x4 _prevClipToClip;
            private float _farPlane;
            private float _nearPlane;
            private float _verticalFOV;
            private Vector3 _position;
            private Vector3 _up;
            private Vector3 _right;
            private Vector3 _forward;
            private uint _orthographic_debugView;
        }

        static NativeInterface()
        {
            try
            {
                Loaded = Native.LoadedCorrectly();
            }
            catch (DllNotFoundException)
            {
                Debug.LogError("The Upscaler plugin could not be found. Please restart Unity. If this problem persists please reinstall Upscaler or contact Conifer support.");
                return;
            }

            if (Loaded) EventIDBase = Native.GetEventIDBase();
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

        internal bool DlssLoadedCorrectly() => Loaded && Native.DLSSLoadedCorrectly();

        internal void Upscale(CommandBuffer cb, Upscaler upscaler)
        {
            Marshal.StructureToPtr(new UpscaleData(upscaler, _cameraID), _dataPtr, true);
            if (Loaded) cb.IssuePluginEventAndData(_renderingEventCallback, EventIDBase, _dataPtr);
        }

        internal static void SetLogLevel(LogType type)
        {
            if (Loaded) Native.SetLogLevel(type);
        }

        internal static bool IsSupported(Upscaler.Technique type) => Loaded && Native.IsSupported(type);

        internal static bool IsSupported(Upscaler.Technique type, Upscaler.Quality mode) =>
            Loaded && Native.IsSupported(type, mode);

        internal Upscaler.Status GetStatus() =>
            Loaded ? Native.GetStatus(_cameraID) : Upscaler.Status.FatalRuntimeError;

        internal string GetStatusMessage() => Loaded
            ? Marshal.PtrToStringAnsi(Native.GetStatusMessage(_cameraID))
            : "GfxPluginUpscaler shared library not loaded! A restart may resolve the problem.";

        internal Upscaler.Status SetStatus(Upscaler.Status status, string message) => Loaded
            ? Native.SetStatus(_cameraID, status, Marshal.StringToHGlobalAnsi(message))
            : Upscaler.Status.Success;

        internal Upscaler.Status SetPerFeatureSettings(Vector2Int resolution, Upscaler.Technique technique,
            Upscaler.DlssPreset preset, Upscaler.Quality quality, float sharpness, bool hdr) => Loaded
            ? Native.SetPerFeatureSettings(_cameraID, resolution, technique, preset, quality, sharpness, hdr)
            : Upscaler.Status.FatalRuntimeError;

        internal Vector2Int GetRecommendedResolution() =>
            Loaded ? Native.GetRecommendedResolution(_cameraID) : Vector2Int.zero;

        internal Vector2Int GetMaximumResolution() => Loaded ? Native.GetMaximumResolution(_cameraID) : Vector2Int.zero;

        internal Vector2Int GetMinimumResolution() => Loaded ? Native.GetMinimumResolution(_cameraID) : Vector2Int.zero;

        internal Vector2 GetJitter(Vector2Int inputResolution) =>
            Loaded ? Native.GetJitter(_cameraID, inputResolution.x) : Vector2.zero;

        internal void ResetHistory()
        {
            if (Loaded) Native.ResetHistory(_cameraID);
        }

        internal void SetImages(IntPtr color, IntPtr depth, IntPtr motion, IntPtr output, IntPtr reactive, IntPtr opaque, bool autoReactive)
        {
            if (Loaded) Native.SetImages(_cameraID, color, depth, motion, output, reactive, opaque, autoReactive);
        }
    }
}