/**************************************************
 * Upscaler v2.0.1                                *
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

namespace Conifer.Upscaler
{
    internal static class NativeInterface
    {
        [DllImport("GfxPluginUpscaler")]
        private static extern bool LoadedCorrectlyPlugin();

        [DllImport("GfxPluginUpscaler")]
        private static extern int GetEventIDBase();

        [DllImport("GfxPluginUpscaler")]
        private static extern void SetFrameGeneration(IntPtr hWnd);

        [DllImport("GfxPluginUpscaler")]
        public static extern void SetFrameGenerationImages(IntPtr color0, IntPtr color1, IntPtr depth, IntPtr motion);

        [DllImport("GfxPluginUpscaler")]
        public static extern GraphicsFormat GetBackBufferFormat();

        private static bool WarnOnBadLoad()
        {
            try
            {
                return LoadedCorrectlyPlugin();
            }
            catch (DllNotFoundException)
            {
                if (Application.platform == RuntimePlatform.WindowsEditor || Application.platform == RuntimePlatform.WindowsPlayer)
                    Debug.LogError("The Upscaler plugin could not be loaded. Please restart Unity. If this problem persists please reinstall Upscaler or contact Conifer support.");
                return false;
            }
        }

        internal static readonly bool Loaded = WarnOnBadLoad();
        private static readonly int FrameGenerateEventID = GetEventIDBase() + 1;
        private static readonly IntPtr _generateCallback = IntPtr.Zero;
        private static readonly IntPtr _frameGenerateDataPtr = Marshal.AllocHGlobal(Marshal.SizeOf<FrameGenerateData>());
        public static bool ShouldResetHistory = true;
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

        internal static void FrameGenerate(CommandBuffer cb, Upscaler upscaler, uint imageIndex)
        {
            Marshal.StructureToPtr(new FrameGenerateData(upscaler, imageIndex, ShouldResetHistory,
#if UNITY_EDITOR
                Vector2Int.RoundToInt(EditorOffset)
#else
                Vector2Int.zero
#endif
                ), _frameGenerateDataPtr, true);
            if (Loaded) cb.IssuePluginEventAndData(_generateCallback, FrameGenerateEventID, _frameGenerateDataPtr);
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
            if (!Mathf.Approximately(unityRect._left, targetRect._left) || !Mathf.Approximately(unityRect._top, targetRect._top) || !Mathf.Approximately(unityRect._right, targetRect._right) || !Mathf.Approximately(unityRect._bottom, targetRect._bottom))
                return true;
            data.Hwnd = childHwnd;
            return false;
        }
#endif

        private static IntPtr GetFrameGenerationTargetWindowHandle(int display)
        {
#if UNITY_EDITOR
            var playModeView = typeof(Editor).Assembly.GetType("UnityEditor.PlayModeView")!;
            var targetDisplay = playModeView.GetField("m_TargetDisplay", BindingFlags.NonPublic | BindingFlags.Instance)!;
            var allPlayModeViews = (IEnumerable)playModeView.GetField("s_PlayModeViews", BindingFlags.NonPublic | BindingFlags.Static)!.GetValue(null);
            var camerasPlayModeView = (from EditorWindow window in allPlayModeViews where display == (int)targetDisplay.GetValue(window) select window).FirstOrDefault();
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