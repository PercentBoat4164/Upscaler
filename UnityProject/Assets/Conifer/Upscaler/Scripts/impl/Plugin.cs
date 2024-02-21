using System;
using System.Runtime.InteropServices;
using AOT;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
using SystemInfo = UnityEngine.Device.SystemInfo;

namespace Conifer.Upscaler.Scripts.impl
{
    public delegate void InternalErrorCallback(IntPtr data, Upscaler.UpscalerStatus er, IntPtr p);

    public delegate void InternalLogCallback(IntPtr msg);
    
    public struct CameraInfo
    {
        public CameraInfo(Camera camera)
        {
            _farPlane = camera.farClipPlane;
            _nearPlane = camera.nearClipPlane;
            _verticalFOV = camera.fieldOfView;
        }

        private float _farPlane;
        private float _nearPlane;
        private float _verticalFOV;
    }

    internal struct Native
    {
        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetRenderingEventCallback")]
        public static extern IntPtr GetRenderingEventCallback();

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_IsUpscalerSupported")]
        public static extern bool IsSupported(Upscaler.UpscalerMode mode);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_RegisterCamera")]
        public static extern void RegisterCamera(IntPtr camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_RegisterCameraErrorCallback")]
        public static extern void RegisterErrorCallback(IntPtr camera, IntPtr data, InternalErrorCallback errorCallback);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_RegisterLogCallback")]
        public static extern void RegisterLogCallback(InternalLogCallback logCallback);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraUpscaler")]
        public static extern Upscaler.UpscalerStatus SetUpscaler(IntPtr camera, Upscaler.UpscalerMode mode);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetCameraUpscalerStatus")]
        public static extern Upscaler.UpscalerStatus GetStatus(IntPtr camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetCameraUpscalerStatusMessage")]
        public static extern IntPtr GetStatusMessage(IntPtr camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_ResetCameraUpscalerStatus")]
        public static extern bool ResetStatus(IntPtr camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraFramebufferSettings")]
        public static extern Upscaler.UpscalerStatus SetFramebufferSettings(IntPtr camera, uint width, uint height,
            Upscaler.QualityMode qualityMode,
            bool hdr);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_GetRecommendedCameraResolution")]
        public static extern ulong GetRecommendedResolution(IntPtr camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraSharpnessValue")]
        public static extern Upscaler.UpscalerStatus SetSharpnessValue(IntPtr camera, float sharpness);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraJitterInformation")]
        public static extern void SetJitterInformation(IntPtr camera, float x, float y);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraFrameInformation")]
        public static extern void SetFrameInformation(IntPtr camera, float frameTime, CameraInfo cameraInfo);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_ResetCameraHistory")]
        public static extern void ResetHistory(IntPtr camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraDepth")]
        public static extern Upscaler.UpscalerStatus SetDepth(IntPtr camera, IntPtr handle, GraphicsFormat format);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraInputColor")]
        public static extern Upscaler.UpscalerStatus SetInputColor(IntPtr camera, IntPtr handle, GraphicsFormat format);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraMotionVectors")]
        public static extern Upscaler.UpscalerStatus SetMotionVectors(IntPtr camera, IntPtr handle, GraphicsFormat format);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_SetCameraOutputColor")]
        public static extern Upscaler.UpscalerStatus SetOutputColor(IntPtr camera, IntPtr handle, GraphicsFormat format);

        [DllImport("GfxPluginUpscaler", EntryPoint = "Upscaler_UnregisterCamera")]
        public static extern void UnregisterCamera(IntPtr camera);
    }

    public class Plugin
    {
        public readonly Camera Camera;
        private GCHandle _cameraHandle;
        private GCHandle _errorCallbackDataHandle;

        private enum Event
        {
            Upscale,
            Prepare
        }

        [MonoPInvokeCallback(typeof(InternalLogCallback))]
        internal static void InternalLogger(IntPtr msg)
        {
            Debug.Log(Marshal.PtrToStringAnsi(msg));
        }

        public static InternalLogCallback LogCallback { set => Native.RegisterLogCallback(value); }

        static Plugin() => LogCallback = InternalLogger;

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
            if (_errorCallbackDataHandle.IsAllocated) _errorCallbackDataHandle.Free();
        }

        public void Upscale(CommandBuffer cb) =>
            cb.IssuePluginEventAndData(Native.GetRenderingEventCallback(), (int)Event.Upscale, GCHandle.ToIntPtr(_cameraHandle));

        public void Prepare(CommandBuffer cb) =>
            cb.IssuePluginEventAndData(Native.GetRenderingEventCallback(), (int)Event.Prepare, GCHandle.ToIntPtr(_cameraHandle));

        public static bool IsSupported(Upscaler.UpscalerMode mode) =>
            Native.IsSupported(mode);

        public void RegisterErrorCallback(object data, InternalErrorCallback errorCallback)
        {
            if (_errorCallbackDataHandle.IsAllocated) _errorCallbackDataHandle.Free();
            _errorCallbackDataHandle = GCHandle.Alloc(data);
            Native.RegisterErrorCallback(GCHandle.ToIntPtr(_cameraHandle),
                GCHandle.ToIntPtr(_errorCallbackDataHandle), errorCallback);
        }

        public Upscaler.UpscalerStatus SetUpscaler(Upscaler.UpscalerMode mode) =>
            Native.SetUpscaler(GCHandle.ToIntPtr(_cameraHandle), mode);

        public Upscaler.UpscalerStatus GetStatus() =>
            Native.GetStatus(GCHandle.ToIntPtr(_cameraHandle));

        public string GetStatusMessage() =>
            Marshal.PtrToStringAnsi(Native.GetStatusMessage(GCHandle.ToIntPtr(_cameraHandle)));

        public bool ResetStatus() =>
            Native.ResetStatus(GCHandle.ToIntPtr(_cameraHandle));

        public Upscaler.UpscalerStatus SetFramebufferSettings(uint width, uint height, Upscaler.QualityMode qualityMode, bool hdr) =>
            Native.SetFramebufferSettings(GCHandle.ToIntPtr(_cameraHandle), width, height, qualityMode, hdr);

        public ulong GetRecommendedResolution() =>
            Native.GetRecommendedResolution(GCHandle.ToIntPtr(_cameraHandle));

        public Upscaler.UpscalerStatus SetSharpnessValue(float sharpness) =>
            Native.SetSharpnessValue(GCHandle.ToIntPtr(_cameraHandle), sharpness);

        public void SetJitterInformation(float x, float y) =>
            Native.SetJitterInformation(GCHandle.ToIntPtr(_cameraHandle), x, y);

        public void SetFrameInformation(float frameTime, CameraInfo cameraInfo) =>
            Native.SetFrameInformation(GCHandle.ToIntPtr(_cameraHandle), frameTime, cameraInfo);

        public void ResetHistory() =>
            Native.ResetHistory(GCHandle.ToIntPtr(_cameraHandle));

        public Upscaler.UpscalerStatus SetDepth(RenderTexture texture) =>
            Native.SetDepth(GCHandle.ToIntPtr(_cameraHandle), texture.GetNativeDepthBufferPtr(), texture.depthStencilFormat);

        public Upscaler.UpscalerStatus SetInputColor(RenderTexture texture) =>
            Native.SetInputColor(GCHandle.ToIntPtr(_cameraHandle), texture.GetNativeTexturePtr(), texture.graphicsFormat);

        public Upscaler.UpscalerStatus SetMotionVectors(RenderTexture texture) =>
            Native.SetMotionVectors(GCHandle.ToIntPtr(_cameraHandle), texture.GetNativeTexturePtr(), texture.graphicsFormat);

        public Upscaler.UpscalerStatus SetOutputColor(RenderTexture texture) =>
            Native.SetOutputColor(GCHandle.ToIntPtr(_cameraHandle), texture.GetNativeTexturePtr(), texture.graphicsFormat);

        public void UnregisterCamera() =>
            Native.UnregisterCamera(GCHandle.ToIntPtr(_cameraHandle));

        public static GraphicsFormat MotionFormat()
        {
            return GraphicsFormat.R16G16_SFloat;
        }

        public GraphicsFormat ColorFormat()
        {
            return SystemInfo.GetGraphicsFormat(Camera.allowHDR ? DefaultFormat.HDR : DefaultFormat.LDR);
        }

        public static GraphicsFormat DepthFormat()
        {
            return SystemInfo.GetGraphicsFormat(DefaultFormat.DepthStencil);
        }
    }
}