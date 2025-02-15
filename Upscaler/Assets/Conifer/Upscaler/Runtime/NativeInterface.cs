/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

/**************************************************
 * Upscaler v2.0.0                                *
 * See the UserManual.pdf for more information    *
 **************************************************/

using System;
using System.Collections;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering.Universal;

namespace Conifer.Upscaler
{

    internal struct Native
    {
        [DllImport("GfxPluginUpscaler")]
        internal static extern bool LoadedCorrectly();

        [DllImport("GfxPluginUpscaler")]
        internal static extern void SetLogLevel(LogType type);

        [DllImport("GfxPluginUpscaler")]
        internal static extern void SetFrameGeneration(IntPtr hWnd);

        [DllImport("GfxPluginUpscaler")]
        internal static extern int GetEventIDBase();

        [DllImport("GfxPluginUpscaler")]
        internal static extern IntPtr GetRenderingEventCallback();

        [DllImport("GfxPluginUpscaler", EntryPoint = "IsUpscalerSupported")]
        internal static extern bool IsSupported(Upscaler.Technique type);

        [DllImport("GfxPluginUpscaler", EntryPoint = "IsQualitySupported")]
        internal static extern bool IsSupported(Upscaler.Technique type, Upscaler.Quality mode);

        [DllImport("GfxPluginUpscaler")]
        internal static extern ushort RegisterCamera();

        [DllImport("GfxPluginUpscaler", EntryPoint = "GetCameraUpscalerStatus")]
        internal static extern Upscaler.Status GetStatus(ushort camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "GetCameraUpscalerStatusMessage")]
        internal static extern IntPtr GetStatusMessage(ushort camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "SetCameraUpscalerStatus")]
        internal static extern Upscaler.Status SetStatus(ushort camera, Upscaler.Status status, IntPtr message);

        [DllImport("GfxPluginUpscaler", EntryPoint = "SetCameraPerFeatureSettings")]
        internal static extern Upscaler.Status SetPerFeatureSettings(ushort camera, Vector2Int resolution, Upscaler.Technique technique, Upscaler.DlssPreset preset, Upscaler.Quality quality, bool hdr);

        [DllImport("GfxPluginUpscaler", EntryPoint = "GetRecommendedCameraResolution")]
        internal static extern Vector2Int GetRecommendedResolution(ushort camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "GetMaximumCameraResolution")]
        internal static extern Vector2Int GetMaximumResolution(ushort camera);

        [DllImport("GfxPluginUpscaler", EntryPoint = "GetMinimumCameraResolution")]
        internal static extern Vector2Int GetMinimumResolution(ushort camera);

        [DllImport("GfxPluginUpscaler")]
        internal static extern void SetUpscalingImages(ushort camera, IntPtr color, IntPtr depth, IntPtr motion, IntPtr output, IntPtr reactive, IntPtr opaque, bool autoReactive);

        [DllImport("GfxPluginUpscaler")]
        internal static extern void SetFrameGenerationImages(IntPtr color0, IntPtr color1, IntPtr depth, IntPtr motion);

        [DllImport("GfxPluginUpscaler")]
        internal static extern GraphicsFormat GetBackBufferFormat();
        
        [DllImport("GfxPluginUpscaler")]
        internal static extern void UnregisterCamera(ushort camera);
    }

    internal class NativeInterface
    {
        internal static readonly bool Loaded;
        private readonly Camera _camera;
        private readonly ushort _cameraID;
        private static readonly int UpscaleEventID;
        private static readonly int FrameGenerateEventID;
        private readonly IntPtr _renderingEventCallback;
        private readonly IntPtr _upscaleDataPtr = Marshal.AllocHGlobal(Marshal.SizeOf<UpscaleData>());
        private readonly IntPtr _frameGenerateDataPtr = Marshal.AllocHGlobal(Marshal.SizeOf<FrameGenerateData>());
        public bool ShouldResetHistory = true;
#if UNITY_EDITOR
        internal Vector2 EditorOffset;
        internal Vector2 EditorResolution;
#endif

        private struct UpscaleData
        {
            internal UpscaleData(Upscaler upscaler, ushort cameraID, Vector2Int inputResolution, bool reset)
            {
                _frameTime = Time.deltaTime * 1000.0F;
                _sharpness = upscaler.sharpness;
                _reactiveValue = upscaler.reactiveMax;
                _reactiveScale = upscaler.reactiveScale;
                _reactiveThreshold = upscaler.reactiveThreshold;
                _camera = cameraID;
                _viewToClip = GL.GetGPUProjectionMatrix(upscaler.Camera.nonJitteredProjectionMatrix, true).inverse;
                _clipToView = _viewToClip.inverse;
                var cameraToWorld = GL.GetGPUProjectionMatrix(upscaler.Camera.worldToCameraMatrix, true).inverse;
                _clipToPrevClip = _clipToView * cameraToWorld * upscaler.LastWorldToCamera * upscaler.LastViewToClip;
                _prevClipToClip = _clipToPrevClip.inverse;
                var planes = upscaler.Camera.nonJitteredProjectionMatrix.decomposeProjection;
                _farPlane = planes.zFar;
                _nearPlane = planes.zNear;
                _verticalFOV = 2.0f * (float)Math.Atan(1.0f / upscaler.Camera.nonJitteredProjectionMatrix.m11) * 180.0f / (float)Math.PI;
                _position = upscaler.Camera.transform.position;
                _up = upscaler.Camera.transform.up;
                _right = upscaler.Camera.transform.right;
                _forward = upscaler.Camera.transform.forward;
                _jitter = upscaler.Jitter;
                _inputResolution = inputResolution;
                _options = Convert.ToUInt32(upscaler.Camera.orthographic) << 0 |
                           Convert.ToUInt32(upscaler.upscalingDebugView) << 1 |
                           Convert.ToUInt32(reset) << 2;
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
            private Vector2 _jitter;
            private Vector2Int _inputResolution;
            private uint _options;
        }

        private struct FrameGenerateData {
            internal FrameGenerateData(Upscaler upscaler, uint imageIndex, bool reset, Vector2Int offsets)
            {
                _enable = upscaler.frameGeneration;
                _generationRect =
#if UNITY_EDITOR
                    Application.isEditor ? new Rect(offsets.x, offsets.y, Screen.width, Screen.height) :
#endif
                    new Rect(0, 0, Screen.width, Screen.height);
                _renderSize = upscaler.InputResolution;
                _jitterOffset = upscaler.Jitter;
                _frameTime = Time.deltaTime * 1000.0f;
                var projMat = upscaler.GetComponent<Camera>().nonJitteredProjectionMatrix;
                var planes = projMat.decomposeProjection;
                _farPlane = planes.zFar;
                _nearPlane = planes.zNear;
                _verticalFOV = 2.0f * (float)Math.Atan(1.0f / projMat.m11) * 180.0f / (float)Math.PI;
                _index = imageIndex;
                _options = Convert.ToUInt32(upscaler.frameGenerationDebugView) << 0 |
                           Convert.ToUInt32(upscaler.showTearLines)            << 1 |
                           Convert.ToUInt32(upscaler.showResetIndicator)       << 2 |
                           Convert.ToUInt32(upscaler.showPacingIndicator)      << 3 |
                           Convert.ToUInt32(upscaler.onlyPresentGenerated)     << 4 |
                           Convert.ToUInt32(upscaler.useAsyncCompute)          << 5 |
                           Convert.ToUInt32(reset)                             << 6;
            }

            private bool _enable;
            private Rect _generationRect;
            private Vector2 _renderSize;
            private Vector2 _jitterOffset;
            private float _frameTime;
            private float _farPlane;
            private float _nearPlane;
            private float _verticalFOV;
            private uint _index;
            private uint _options;
        }

        static NativeInterface()
        {
            try
            {
                Loaded = Native.LoadedCorrectly();
            }
            catch (DllNotFoundException)
            {
                Loaded = false;
                if (Application.platform == RuntimePlatform.WindowsEditor || Application.platform == RuntimePlatform.WindowsPlayer)
                  Debug.LogError("The Upscaler plugin could not be loaded. Please restart Unity. If this problem persists please reinstall Upscaler or contact Conifer support.");
                return;
            }

            if (!Loaded) return;
            var eventIDBase = Native.GetEventIDBase();
            UpscaleEventID = eventIDBase;
            FrameGenerateEventID = ++eventIDBase;
        }

        internal NativeInterface(Camera camera)
        {
            if (!Loaded) return;
            _camera = camera;
            _cameraID = Native.RegisterCamera();
            _renderingEventCallback = Native.GetRenderingEventCallback();
        }

        ~NativeInterface()
        {
            if (Loaded) Native.UnregisterCamera(_cameraID);
        }

        internal void Upscale(CommandBuffer cb, Upscaler upscaler, Vector2Int inputResolution=default)
        {
            Marshal.StructureToPtr(new UpscaleData(upscaler, _cameraID, inputResolution, ShouldResetHistory), _upscaleDataPtr, true);
            if (Loaded) cb.IssuePluginEventAndData(_renderingEventCallback, UpscaleEventID, _upscaleDataPtr);
        }

        internal void FrameGenerate(CommandBuffer cb, Upscaler upscaler, uint imageIndex)
        {
            Marshal.StructureToPtr(new FrameGenerateData(upscaler, imageIndex, ShouldResetHistory, Vector2Int.RoundToInt(EditorOffset)), _frameGenerateDataPtr, true);
            if (Loaded) cb.IssuePluginEventAndData(_renderingEventCallback, FrameGenerateEventID, _frameGenerateDataPtr);
        }

        internal static void SetLogLevel(LogType type)
        {
            if (Loaded) Native.SetLogLevel(type);
        }

        internal void SetFrameGeneration(bool on)
        {
            if (!Loaded) return;
            Native.SetFrameGeneration(on ? GetFrameGenerationTargetHwnd() : IntPtr.Zero);
        }

        internal static bool IsSupported(Upscaler.Technique type) => Loaded && Native.IsSupported(type);

        internal static bool IsSupported(Upscaler.Technique type, Upscaler.Quality mode) =>
            Loaded && Native.IsSupported(type, mode);

        internal Upscaler.Status GetStatus() =>
            Loaded ? Native.GetStatus(_cameraID) : Upscaler.Status.Success;

        internal string GetStatusMessage() => Loaded
            ? Marshal.PtrToStringAnsi(Native.GetStatusMessage(_cameraID))
            : "GfxPluginUpscaler shared library not loaded; some upscalers may be unavailable! A restart may resolve the problem if you are on a supported platform.";

        internal Upscaler.Status SetStatus(Upscaler.Status status, string message) => Loaded
            ? Native.SetStatus(_cameraID, status, Marshal.StringToHGlobalAnsi(message))
            : Upscaler.Status.LibraryNotLoaded;

        internal Upscaler.Status SetPerFeatureSettings(Vector2Int resolution, Upscaler.Technique technique, Upscaler.DlssPreset preset, Upscaler.Quality quality, bool hdr) => Loaded
            ? Native.SetPerFeatureSettings(_cameraID, resolution, technique, preset, quality, hdr)
            : Upscaler.Status.LibraryNotLoaded;

        internal Vector2Int GetRecommendedResolution() =>
            Loaded ? Native.GetRecommendedResolution(_cameraID) : Vector2Int.zero;

        internal Vector2Int GetMaximumResolution() => Loaded ? Native.GetMaximumResolution(_cameraID) : Vector2Int.zero;

        internal Vector2Int GetMinimumResolution() => Loaded ? Native.GetMinimumResolution(_cameraID) : Vector2Int.zero;

        internal void SetUpscalingImages(RTHandle color, RTHandle depth, RTHandle motion, RTHandle output, RTHandle reactive, RTHandle opaque, bool autoReactive)
        {
            if (Loaded) Native.SetUpscalingImages(_cameraID, color?.rt.GetNativeTexturePtr() ?? IntPtr.Zero, depth?.rt.GetNativeTexturePtr() ?? IntPtr.Zero, motion?.rt.GetNativeTexturePtr() ?? IntPtr.Zero, output?.rt.GetNativeTexturePtr() ?? IntPtr.Zero, reactive?.rt.GetNativeTexturePtr() ?? IntPtr.Zero, opaque?.rt.GetNativeTexturePtr() ?? IntPtr.Zero, autoReactive);
        }

        internal static void SetFrameGenerationImages(RTHandle hudless0, RTHandle hudless1, RTHandle depth, RTHandle motion)
        {
            if (Loaded) Native.SetFrameGenerationImages(hudless0?.rt.GetNativeTexturePtr() ?? IntPtr.Zero, hudless1?.rt.GetNativeTexturePtr() ?? IntPtr.Zero, depth?.rt.GetNativeTexturePtr() ?? IntPtr.Zero, motion?.rt.GetNativeTexturePtr() ?? IntPtr.Zero);
        }

        internal static GraphicsFormat GetBackBufferFormat() => Loaded ? Native.GetBackBufferFormat() : GraphicsFormat.None;
        
#if UNITY_EDITOR
        private delegate bool EnumChildWindowsProc(IntPtr hWnd, ref EnumChildWindowsData lParam);
        
        [DllImport("user32.dll")]
        private static extern bool EnumChildWindows(IntPtr hWndParent, EnumChildWindowsProc lpEnumFunc, ref EnumChildWindowsData lParam);
        
        [DllImport("user32.dll")]
        private static extern bool GetWindowRect(IntPtr hWnd, out NativeRect lpRect);
        
        [DllImport("user32.dll")]
        private static extern bool GetClientRect(IntPtr hWnd, out NativeRect lpRect);
        
        [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        private static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);
        
        [DllImport("user32.dll")]
        private static extern int GetWindowTextLength(IntPtr hWnd);
        
        [StructLayout(LayoutKind.Sequential)]
        private struct NativeRect
        {
            public int _left;
            public int _top;
            public int _right;
            public int _bottom;

            public NativeRect(float windowRectXMin, float windowRectYMin, float windowRectXMax, float windowRectYMax)
            {
                _left = (int)windowRectXMin;
                _top = (int)windowRectYMin;
                _right = (int)windowRectXMax;
                _bottom = (int)windowRectYMax;
            }
        }
        
        private struct EnumChildWindowsData
        {
            public readonly NativeRect Rect;
            public IntPtr Hwnd;

            public EnumChildWindowsData(NativeRect rect, IntPtr hwnd)
            {
                Rect = rect;
                Hwnd = hwnd;
            }
        }
        
        private static bool EnumChildWindowsCallback(IntPtr childHwnd, ref EnumChildWindowsData data)
        {
            GetWindowRect(childHwnd, out var unityRect);
            var targetRect = data.Rect;
            if (!Mathf.Approximately(unityRect._left, targetRect._left) || !Mathf.Approximately(unityRect._top, targetRect._top) || !Mathf.Approximately(unityRect._right, targetRect._right) || !Mathf.Approximately(unityRect._bottom, targetRect._bottom))
                return true;
            data.Hwnd = childHwnd;
            return false;
        }
#endif

        private IntPtr GetFrameGenerationTargetHwnd()
        {
#if UNITY_EDITOR
            var playModeView = typeof(Editor).Assembly.GetType("UnityEditor.PlayModeView")!;
            var targetDisplay = playModeView.GetField("m_TargetDisplay", BindingFlags.NonPublic | BindingFlags.Instance)!;
            var allPlayModeViews = (IEnumerable)playModeView.GetField("s_PlayModeViews", BindingFlags.NonPublic | BindingFlags.Static)!.GetValue(null);
            var camerasPlayModeView = (from EditorWindow window in allPlayModeViews where _camera.targetDisplay == (int)targetDisplay.GetValue(window) select window).FirstOrDefault();
            if (camerasPlayModeView is null) return IntPtr.Zero;
            var editorWindow = typeof(Editor).Assembly.GetType("UnityEditor.EditorWindow")!;
            var viewRect = (Rect)editorWindow.GetField("m_GameViewRect", BindingFlags.NonPublic | BindingFlags.Instance)!.GetValue(camerasPlayModeView);
            EditorOffset = viewRect.position;
            var view = typeof(Editor).Assembly.GetType("UnityEditor.View")!;
            var position = view.GetField("m_Position", BindingFlags.NonPublic | BindingFlags.Instance)!;
            var parent = editorWindow.GetField("m_Parent", BindingFlags.NonPublic | BindingFlags.Instance)!.GetValue(camerasPlayModeView);
            var windowRect = (Rect)position.GetValue(parent);
            EditorResolution = windowRect.size;
            while ((parent = view.GetField("m_Parent", BindingFlags.NonPublic | BindingFlags.Instance)!.GetValue(parent)) is not null)
            {
                var thisViewPosition = (Rect)position.GetValue(parent);
                windowRect.xMin += thisViewPosition.xMin;
                windowRect.xMax += thisViewPosition.xMin;
                windowRect.yMin += thisViewPosition.yMin;
                windowRect.yMax += thisViewPosition.yMin;
            }
            var parentHwnd = System.Diagnostics.Process.GetCurrentProcess().MainWindowHandle;
            GetWindowRect(parentHwnd, out var parentRect);
            GetClientRect(parentHwnd, out var parentClientRect);
            var x = parentRect._right + parentRect._left - parentClientRect._right;
            var y = parentRect._bottom + parentRect._top - parentClientRect._bottom;
            var targetRect = new NativeRect(windowRect.xMin + x, windowRect.yMin + y, windowRect.xMax + x, windowRect.yMax + y);
            var data = new EnumChildWindowsData(targetRect, IntPtr.Zero);
            EnumChildWindows(parentHwnd, EnumChildWindowsCallback, ref data);
            return data.Hwnd;
#else
            return System.Diagnostics.Process.GetCurrentProcess().MainWindowHandle;
#endif
        }
    }
}