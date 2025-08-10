using System;
using System.Collections;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using UnityEditor;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;

namespace Upscaler.Runtime
{
    internal static class NativeInterface
    {
        [DllImport("GfxPluginUpscaler")]
        private static extern bool LoadedCorrectlyPlugin();

        [DllImport("GfxPluginUpscaler")]
        private static extern void SetFrameGeneration(IntPtr hWnd);

        [DllImport("GfxPluginUpscaler")]
        public static extern void SetFrameGenerationImages(IntPtr color0, IntPtr color1, IntPtr depth, IntPtr motion);

        [DllImport("GfxPluginUpscaler")]
        public static extern GraphicsFormat GetBackBufferFormat();

        [DllImport("GfxPluginUpscaler")]
        public static extern IntPtr GetGenerateCallbackFidelityFXSuperResolution();

        private static bool WarnOnBadLoad()
        {
            try
            {
                return LoadedCorrectlyPlugin();
            }
            catch (DllNotFoundException)
            {
                if (Application.platform == RuntimePlatform.WindowsEditor || Application.platform == RuntimePlatform.WindowsPlayer)
                    Debug.LogError("The Upscaler plugin could not be loaded. Please restart Unity. If this problem persists please reinstall Upscaler or submit an issue on Upscaler's GitHub: https://github.com/PercentBoat4164/Upscaler.");
                return false;
            }
        }

        internal static readonly bool Loaded = WarnOnBadLoad();
        private static readonly IntPtr _generateCallback = GetGenerateCallbackFidelityFXSuperResolution();
        private static readonly IntPtr _frameGenerateDataPtr = Marshal.AllocHGlobal(Marshal.SizeOf<FrameGenerateData>());
        public static bool ShouldResetHistory = false;
#if UNITY_EDITOR
        internal static Vector2 EditorOffset;
        internal static Vector2 EditorResolution;
#endif

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

            private Rect _generationRect;
            private Vector2 _renderSize;
            private Vector2 _jitterOffset;
            private float _frameTime;
            private float _farPlane;
            private float _nearPlane;
            private float _verticalFOV;
            private uint _index;
            private uint _options;
            private bool _enable;
        }

        internal static void FrameGenerate(CommandBuffer cb, Upscaler upscaler, uint imageIndex)
        {
            Marshal.StructureToPtr(new FrameGenerateData(upscaler, imageIndex, ShouldResetHistory,
#if UNITY_EDITOR
                Vector2Int.RoundToInt(EditorOffset)
#else
                Vector2Int.zero
#endif
                ), _frameGenerateDataPtr, true);
            if (Loaded) cb.IssuePluginEventAndData(_generateCallback, 0, _frameGenerateDataPtr);
        }

        internal static void SetFrameGeneration(int display) => SetFrameGeneration(display >= 0 ? GetFrameGenerationTargetWindowHandle(display) : IntPtr.Zero);

#if UNITY_EDITOR
        private delegate bool EnumChildWindowsProc(IntPtr hWnd, ref EnumChildWindowsData lParam);
        
        [DllImport("user32.dll")]
        private static extern bool EnumChildWindows(IntPtr hWndParent, EnumChildWindowsProc lpEnumFunc, ref EnumChildWindowsData lParam);
        
        [DllImport("user32.dll")]
        private static extern bool GetWindowRect(IntPtr hWnd, out NativeRect lpRect);
        
        [DllImport("user32.dll")]
        private static extern bool GetClientRect(IntPtr hWnd, out NativeRect lpRect);

        [DllImport("user32.dll")]
        private static extern int MapWindowPoints(IntPtr hWndFrom, IntPtr hWndTo, ref NativeRect lpPoints, int cPoints=2);
        
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

            public NativeRect(int windowRectXMin, int windowRectYMin, int windowRectXMax, int windowRectYMax)
            {
                _left = windowRectXMin;
                _top = windowRectYMin;
                _right = windowRectXMax;
                _bottom = windowRectYMax;
            }
        }

        [StructLayout(LayoutKind.Sequential)]
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
            if (targetRect._bottom != unityRect._bottom || targetRect._top != unityRect._top || targetRect._left != unityRect._left || targetRect._right != unityRect._right)
                return true;
            data.Hwnd = childHwnd;
            return false;
        }

        private static readonly Type PlayModeView = typeof(Editor).Assembly.GetType("UnityEditor.PlayModeView")!;
        private static readonly FieldInfo TargetDisplay = PlayModeView.GetField("m_TargetDisplay", BindingFlags.NonPublic | BindingFlags.Instance)!;
        private static readonly FieldInfo PlayModeViews = PlayModeView.GetField("s_PlayModeViews", BindingFlags.NonPublic | BindingFlags.Static)!;
        private static readonly Type EditorWindow = typeof(Editor).Assembly.GetType("UnityEditor.EditorWindow")!;
        private static readonly FieldInfo GameViewRect = EditorWindow.GetField("m_GameViewRect", BindingFlags.NonPublic | BindingFlags.Instance)!;
        private static readonly FieldInfo EditorWindowParent = EditorWindow.GetField("m_Parent", BindingFlags.NonPublic | BindingFlags.Instance)!;
        private static readonly Type View = typeof(Editor).Assembly.GetType("UnityEditor.View")!;
        private static readonly FieldInfo Position = View.GetField("m_Position", BindingFlags.NonPublic | BindingFlags.Instance)!;
        private static readonly FieldInfo ViewParent = View.GetField("m_Parent", BindingFlags.NonPublic | BindingFlags.Instance)!;
#endif

        private static IntPtr GetFrameGenerationTargetWindowHandle(int display)
        {
#if UNITY_EDITOR

            var allPlayModeViews = (IEnumerable)PlayModeViews.GetValue(null);
            var camerasPlayModeView = (from EditorWindow window in allPlayModeViews where display == (int)TargetDisplay.GetValue(window) select window).FirstOrDefault();
            if (camerasPlayModeView is null) return IntPtr.Zero;
            var viewRect = (Rect)GameViewRect.GetValue(camerasPlayModeView);
            EditorOffset = viewRect.position;
            var parent = EditorWindowParent.GetValue(camerasPlayModeView);
            var windowRect = (Rect)Position.GetValue(parent);
            EditorResolution = windowRect.size;
            while ((parent = ViewParent.GetValue(parent)) is not null)
            {
                var thisViewPosition = (Rect)Position.GetValue(parent);
                windowRect.xMin += thisViewPosition.xMin;
                windowRect.xMax += thisViewPosition.xMin;
                windowRect.yMin += thisViewPosition.yMin;
                windowRect.yMax += thisViewPosition.yMin;
            }
            var parentHwnd = System.Diagnostics.Process.GetCurrentProcess().MainWindowHandle;
            var targetRect = new NativeRect((int)windowRect.xMin, (int)windowRect.yMin, (int)windowRect.xMax, (int)windowRect.yMax);
            MapWindowPoints(parentHwnd, IntPtr.Zero, ref targetRect);
            var data = new EnumChildWindowsData(targetRect, IntPtr.Zero);
            EnumChildWindows(parentHwnd, EnumChildWindowsCallback, ref data);
            if (data.Hwnd == IntPtr.Zero) Debug.LogError("Could not find target window");
            else
            {
                var length = GetWindowTextLength(data.Hwnd) + 1;
                var builder = new StringBuilder(length);
                GetWindowText(data.Hwnd, builder, length);
                Debug.Log($"Found target window: {builder} (hWnd: {data.Hwnd})");
            }
            return data.Hwnd;
#else
            return System.Diagnostics.Process.GetCurrentProcess().MainWindowHandle;
#endif
        }
    }
}