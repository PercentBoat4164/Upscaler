using System;
using System.Collections;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using UnityEditor;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

namespace Upscaler.Runtime.Backends
{
    public class FrameGeneratorBackend : IDisposable
    {
        [DllImport("GfxPluginUpscaler")]
        private static extern bool LoadedCorrectlyPlugin();

        [DllImport("GfxPluginUpscaler")]
        private static extern bool LoadedCorrectlyFidelityFXSuperResolution();

        [DllImport("GfxPluginUpscaler")]
        private static extern void SetFrameGeneration(IntPtr hWnd);

        [DllImport("GfxPluginUpscaler")]
        private static extern void SetFrameGenerationImages(IntPtr color0, IntPtr color1, IntPtr depth, IntPtr motion);

        [DllImport("GfxPluginUpscaler")]
        private static extern GraphicsFormat GetBackBufferFormat(IntPtr hWnd);

        [DllImport("GfxPluginUpscaler")]
        private static extern IntPtr GetGenerateCallbackFidelityFXSuperResolution();

        [StructLayout(LayoutKind.Sequential)]
        private struct FrameGenerateData {
            internal Rect generationRect;
            internal Vector3 cameraPosition;
            internal Vector3 cameraUp;
            internal Vector3 cameraRight;
            internal Vector3 cameraForward;
            internal Vector2 renderSize;
            internal Vector2 jitterOffset;
            internal float frameTime;
            internal float farPlane;
            internal float nearPlane;
            internal float verticalFOV;
            internal uint index;
            internal uint options;
            internal bool enable;
        }

        public static bool Supported { get; }
        private IntPtr DataHandle;
        private static readonly IntPtr EventCallback;
        private FrameGenerateData _data;
        private static readonly Material _depthBlitMaterial = new (Shader.Find("Hidden/Upscaler/BlitDepth"));
        private static readonly int BlitScaleBiasID = Shader.PropertyToID("_BlitScaleBias");


#if UNITY_EDITOR
        private static readonly int TempColor = Shader.PropertyToID("Upscaler_TempColor");
        private Vector2 _editorOffset;
        private Vector2 _editorResolution;
#endif
        private RenderTextureDescriptor _inputDescriptor;
        private uint _hudlessBufferIndex;
        private IntPtr hWnd;

        private readonly RTHandle[] _hudless = new RTHandle[2];
        private RTHandle _flippedDepth;
        private RTHandle _flippedMotion;

        static FrameGeneratorBackend()
        {
#if UNITY_6000_0_OR_NEWER
            Supported = false;
            return;
#endif
            Supported = true;
            try
            {
                if (!LoadedCorrectlyPlugin() || !LoadedCorrectlyFidelityFXSuperResolution())
                {
                    Supported = false;
                    return;
                }
                EventCallback = GetGenerateCallbackFidelityFXSuperResolution();
                if (EventCallback == IntPtr.Zero || !SystemInfo.supportsMotionVectors || !SystemInfo.supportsComputeShaders)
                {
                    Supported = false;
                    return;
                }

                var backend = new FrameGeneratorBackend();
                backend.Dispose();
            }
            catch
            {
                Supported = false;
            }
        }

        public FrameGeneratorBackend(int targetDisplay=0)
        {
            if (!Supported) return;
            DataHandle = Marshal.AllocCoTaskMem(Marshal.SizeOf<FrameGenerateData>());
            _data = new FrameGenerateData();
            hWnd = GetFrameGenerationTargetWindowHandle(targetDisplay);
            SetFrameGeneration(hWnd);
        }

        public void Update(in Upscaler upscaler, RenderTextureDescriptor descriptor)
        {
            if (!Supported) return;
            hWnd = GetFrameGenerationTargetWindowHandle(upscaler.Camera.targetDisplay);
            SetFrameGeneration(hWnd);
            _inputDescriptor = descriptor;
            // Frame generation expects the hudless image to be the size of the swapchain and contain only image data where the viewport is.
            descriptor.colorFormat = GraphicsFormatUtility.GetRenderTextureFormat(GetBackBufferFormat(hWnd));
            descriptor.depthStencilFormat = GraphicsFormat.None;
#if UNITY_EDITOR
            descriptor.width = (int)_editorResolution.x;
            descriptor.height = (int)_editorResolution.y;
#endif
            var needsUpdate = RenderingUtils.ReAllocateIfNeeded(ref _hudless[0], descriptor, name: "Upscaler_HUDLess0");
            needsUpdate |= RenderingUtils.ReAllocateIfNeeded(ref _hudless[1], descriptor, name: "Upscaler_HUDLess1");

            // Frame generation expects the motion vector image to be the size of the swapchain but be entirely filled with motion vectors.
            descriptor = _inputDescriptor;
#if UNITY_EDITOR
            descriptor.width = (int)_editorResolution.x;
            descriptor.height = (int)_editorResolution.y;
#endif
            descriptor.graphicsFormat = GraphicsFormat.R16G16_SFloat;
            descriptor.depthStencilFormat = GraphicsFormat.None;
            needsUpdate |= RenderingUtils.ReAllocateIfNeeded(ref _flippedMotion, descriptor, name: "Upscaler_FlippedMotion");

            // Frame generation expects the depth image to be at render resolution (when upscaling).
            descriptor = _inputDescriptor;
            descriptor.width = upscaler.InputResolution.x;
            descriptor.height = upscaler.InputResolution.y;
            descriptor.colorFormat = RenderTextureFormat.Depth;
            needsUpdate |= RenderingUtils.ReAllocateIfNeeded(ref _flippedDepth, descriptor, isShadowMap: true, name: "Upscaler_FlippedDepth");

            _inputDescriptor.depthStencilFormat = GraphicsFormat.None;

            if (!needsUpdate) return;
            SetFrameGenerationImages(_hudless[0].rt.GetNativeTexturePtr(), _hudless[1].rt.GetNativeTexturePtr(), _flippedDepth.rt.GetNativeTexturePtr(), _flippedMotion.rt.GetNativeTexturePtr());
        }

        public void Generate(in Upscaler upscaler, in CommandBuffer commandBuffer, in Texture depth, in Texture motion)
        {
            if (!Supported) return;
            var camera = upscaler.Camera;
            var projMat = camera.nonJitteredProjectionMatrix;
            var planes = projMat.decomposeProjection;
            _data.cameraPosition = camera.transform.position;
            _data.cameraUp = camera.transform.up;
            _data.cameraRight = camera.transform.right;
            _data.cameraForward = camera.transform.forward;
            _data.generationRect = new Rect(
#if UNITY_EDITOR
                _editorOffset,
#else
                Vector2.zero,
#endif
                upscaler.OutputResolution);
            _data.renderSize = upscaler.InputResolution;
            _data.jitterOffset = upscaler.Jitter;
            _data.frameTime = Time.deltaTime * 1000.0f;
            _data.farPlane = planes.zFar;
            _data.nearPlane = planes.zNear;
            _data.verticalFOV = 2.0f * (float)Math.Atan(1.0f / projMat.m11) * 180.0f / (float)Math.PI;
            _data.index = _hudlessBufferIndex;
            _data.options = Convert.ToUInt32(upscaler.frameGenerationDebugView)    << 0 |
                            Convert.ToUInt32(upscaler.showTearLines)               << 1 |
                            Convert.ToUInt32(upscaler.showResetIndicator)          << 2 |
                            Convert.ToUInt32(upscaler.showPacingIndicator)         << 3 |
                            Convert.ToUInt32(upscaler.onlyPresentGenerated)        << 4 |
                            Convert.ToUInt32(upscaler.useAsyncCompute)             << 5 |
                            Convert.ToUInt32(upscaler.shouldHistoryResetThisFrame) << 6;
            _data.enable = upscaler.frameGeneration;
            Marshal.StructureToPtr(_data, DataHandle, true);

#if UNITY_EDITOR
            // Oddity of Unity requires the backbuffer to be blitted to another image before it can be blitted to the hudless image, otherwise the offset does not happen.
            commandBuffer.GetTemporaryRT(TempColor, _inputDescriptor);
            commandBuffer.Blit(null, TempColor);
            commandBuffer.Blit(TempColor, _hudless[_hudlessBufferIndex], _editorResolution / upscaler.OutputResolution, -_editorOffset / _editorResolution);
            commandBuffer.ReleaseTemporaryRT(TempColor);
#else
            commandBuffer.Blit(null, _hudless[_hudlessBufferIndex]);
#endif
            if (depth != _flippedDepth.rt)
            {
                commandBuffer.SetGlobalVector(BlitScaleBiasID, new Vector4(1, -1, 0, 1));
                commandBuffer.Blit(depth, _flippedDepth, _depthBlitMaterial, 0);
            }
            if (motion != _flippedMotion.rt) commandBuffer.Blit(motion, _flippedMotion, new Vector2(1, -1), new Vector2(0, 1));
            commandBuffer.IssuePluginEventAndData(EventCallback, 0, DataHandle);
            _hudlessBufferIndex = (_hudlessBufferIndex + 1U) % (uint)_hudless.Length;
        }

#if UNITY_EDITOR
        private delegate bool EnumChildWindowsProc(IntPtr hWnd, ref EnumChildWindowsData lParam);

        [DllImport("user32.dll")]
        private static extern bool EnumChildWindows(IntPtr hWndParent, EnumChildWindowsProc lpEnumFunc, ref EnumChildWindowsData lParam);

        [DllImport("user32.dll")]
        private static extern bool GetWindowRect(IntPtr hWnd, out NativeRect lpRect);

        [DllImport("user32.dll")]
        private static extern int MapWindowPoints(IntPtr hWndFrom, IntPtr hWndTo, ref NativeRect lpPoints, int cPoints=2);

        [StructLayout(LayoutKind.Sequential)]
        private struct NativeRect
        {
            internal int _left;
            internal int _top;
            internal int _right;
            internal int _bottom;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct EnumChildWindowsData
        {
            internal NativeRect Rect;
            internal IntPtr Hwnd;
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

        private IntPtr GetFrameGenerationTargetWindowHandle(int display)
        {
#if UNITY_EDITOR

            var allPlayModeViews = (IEnumerable)PlayModeViews.GetValue(null);
            var camerasPlayModeView = (from EditorWindow window in allPlayModeViews where display == (int)TargetDisplay.GetValue(window) select window).FirstOrDefault();
            if (camerasPlayModeView is null) return IntPtr.Zero;
            var viewRect = (Rect)GameViewRect.GetValue(camerasPlayModeView);
            _editorOffset = viewRect.position;
            var parent = EditorWindowParent.GetValue(camerasPlayModeView);
            var windowRect = (Rect)Position.GetValue(parent);
            _editorResolution = windowRect.size;
            while ((parent = ViewParent.GetValue(parent)) is not null)
            {
                var thisViewPosition = (Rect)Position.GetValue(parent);
                windowRect.xMin += thisViewPosition.xMin;
                windowRect.xMax += thisViewPosition.xMin;
                windowRect.yMin += thisViewPosition.yMin;
                windowRect.yMax += thisViewPosition.yMin;
            }
            var parentHwnd = System.Diagnostics.Process.GetCurrentProcess().MainWindowHandle;
            var targetRect = new NativeRect
            {
                _left = (int)windowRect.xMin,
                _top = (int)windowRect.yMin,
                _right = (int)windowRect.xMax,
                _bottom = (int)windowRect.yMax
            };
            MapWindowPoints(parentHwnd, IntPtr.Zero, ref targetRect);
            var data = new EnumChildWindowsData
            {
                Rect = targetRect,
                Hwnd = IntPtr.Zero
            };
            EnumChildWindows(parentHwnd, EnumChildWindowsCallback, ref data);
            return data.Hwnd;
#else
            return System.Diagnostics.Process.GetCurrentProcess().MainWindowHandle;
#endif
        }

        public void Dispose()
        {
            Marshal.FreeCoTaskMem(DataHandle);
            SetFrameGeneration(IntPtr.Zero);
        }
    }
}