/***********************************************
 * Upscaler v2.0.1                             *
 * See the UserManual.pdf for more information *
 ***********************************************/

#if UPSCALER_USE_URP
using System.Reflection;
using Upscaler.Runtime.Backends;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;
#if UNITY_6000_0_OR_NEWER
using System;
#else
using UnityEngine.Experimental.Rendering;
#endif

namespace Upscaler.Runtime.URP
{
    [DisallowMultipleRendererFeature("Upscaler Renderer Feature")]
    public partial class UpscalerRendererFeature : ScriptableRendererFeature
    {
        private static readonly int DepthID = Shader.PropertyToID("_CameraDepthTexture");
        private static readonly int MotionID = Shader.PropertyToID("_MotionVectorTexture");
        private static readonly int OpaqueID = Shader.PropertyToID("_CameraOpaqueTexture");

        private static readonly MethodInfo UseScaling = typeof(RTHandle).GetProperty("useScaling", BindingFlags.Instance | BindingFlags.Public)?.SetMethod!;
        private static readonly MethodInfo ScaleFactor = typeof(RTHandle).GetProperty("scaleFactor", BindingFlags.Instance | BindingFlags.Public)?.SetMethod!;

        private bool _isResizingThisFrame;
        private bool _lastCompatibilityMode;
        private readonly SetupUpscaleRenderPass _setupUpscale = new();
        private readonly UpscaleRenderPass _upscale = new();
#if !UNITY_6000_0_OR_NEWER
        private readonly GenerateRenderPass _generate = new();
#endif
        private readonly HistoryResetRenderPass _historyReset = new();

        public override void Create() => name = "Upscaler";

        public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
        {
            var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
            if (renderingData.cameraData.cameraType != CameraType.Game || upscaler == null || !upscaler.isActiveAndEnabled) return;

            var previousFrameGeneration = upscaler.PreviousFrameGeneration;
            var needsUpdate = upscaler.ApplySettings(UpscalerBackend.Flags.OutputResolutionMotionVectors | (upscaler.Camera.allowHDR ? UpscalerBackend.Flags.EnableHDR : UpscalerBackend.Flags.None));

            if (upscaler.technique == Upscaler.Technique.None || upscaler.Backend == null)
            {
                _upscale.Color?.Release();
                _upscale.Output?.Release();
                _upscale.Depth?.Release();
            }

#if UNITY_6000_0_OR_NEWER
            var compatibilityMode = GraphicsSettings.GetRenderPipelineSettings<RenderGraphSettings>().enableRenderCompatibilityMode;
            needsUpdate |= compatibilityMode != _lastCompatibilityMode;
            _lastCompatibilityMode = compatibilityMode;
#endif

            if (needsUpdate && upscaler.technique != Upscaler.Technique.None && upscaler.Backend != null)
            {
                var descriptor = new RenderTextureDescriptor(upscaler.OutputResolution.x, upscaler.OutputResolution.y, renderingData.cameraData.cameraTargetDescriptor.colorFormat)
                {
                    enableRandomWrite = true  //@todo: Only enable this if needed.
                };
#if UNITY_6000_0_OR_NEWER
                RenderingUtils.ReAllocateHandleIfNeeded(ref _upscale.Output, descriptor, name: "Upscaler_Destination");
                if (compatibilityMode) RenderingUtils.ReAllocateHandleIfNeeded(ref _upscale.Color, new RenderTextureDescriptor(upscaler.OutputResolution.x, upscaler.OutputResolution.y, renderingData.cameraData.cameraTargetDescriptor.colorFormat), name: "Upscaler_Source");
                else RenderingUtils.ReAllocateHandleIfNeeded(ref _upscale.Color, new RenderTextureDescriptor(upscaler.InputResolution.x, upscaler.InputResolution.y, renderingData.cameraData.cameraTargetDescriptor.colorFormat), name: "Upscaler_Source");
#else
                RenderingUtils.ReAllocateIfNeeded(ref _upscale.Output, descriptor, name: "Upscaler_Destination");
                RenderingUtils.ReAllocateIfNeeded(ref _upscale.Color, new RenderTextureDescriptor(upscaler.OutputResolution.x, upscaler.OutputResolution.y, renderingData.cameraData.cameraTargetDescriptor.colorFormat), name: "Upscaler_Source");
#endif
                var flags = upscaler.Camera.allowHDR ? UpscalerBackend.Flags.EnableHDR : UpscalerBackend.Flags.None;
                if (Upscaler.Failure(upscaler.CurrentStatus = upscaler.Backend.Update(upscaler, _upscale.Color, _upscale.Output, flags | UpscalerBackend.Flags.OutputResolutionMotionVectors))) return;
                if (upscaler.Backend is NativeAbstractBackend backend) _upscale.Depth = RTHandles.Alloc(backend.Depth);
#if UNITY_6000_0_OR_NEWER
                else
                    if (compatibilityMode) RenderingUtils.ReAllocateHandleIfNeeded(ref _upscale.Depth, new RenderTextureDescriptor(upscaler.OutputResolution.x, upscaler.OutputResolution.y, RenderTextureFormat.Shadowmap, 32));
                    else RenderingUtils.ReAllocateHandleIfNeeded(ref _upscale.Depth, new RenderTextureDescriptor(upscaler.InputResolution.x, upscaler.InputResolution.y, RenderTextureFormat.Shadowmap, 32));
#else
                else RenderingUtils.ReAllocateIfNeeded(ref _upscale.Depth, new RenderTextureDescriptor(upscaler.OutputResolution.x, upscaler.OutputResolution.y, RenderTextureFormat.Shadowmap, 32));
#endif
            }

#if !UNITY_6000_0_OR_NEWER
            if (needsUpdate && upscaler.frameGeneration && upscaler.FgBackend != null)
            {
                upscaler.FgBackend.Update(upscaler, renderingData.cameraData.cameraTargetDescriptor);
            }
#endif

            var needsHistoryReset = false;
            if (!_isResizingThisFrame && upscaler.technique != Upscaler.Technique.None && upscaler.Backend != null)
            {
                _setupUpscale.ConfigureInput(ScriptableRenderPassInput.None);
                renderer.EnqueuePass(_setupUpscale);
                _upscale.ConfigureInput((upscaler.IsTemporal() ? ScriptableRenderPassInput.Motion | ScriptableRenderPassInput.Depth : ScriptableRenderPassInput.None) | ScriptableRenderPassInput.Color);
                renderer.EnqueuePass(_upscale);
                needsHistoryReset = upscaler.IsTemporal();
            }
#if !UNITY_6000_0_OR_NEWER
            if (!_isResizingThisFrame && upscaler.frameGeneration && previousFrameGeneration && upscaler.FgBackend != null)
            {
                _generate.ConfigureInput(ScriptableRenderPassInput.None);
                renderer.EnqueuePass(_generate);
                VolumeManager.instance.stack.GetComponent<MotionBlur>().intensity.value /= 2.0f;
                needsHistoryReset = true;
            }
#endif
            if (!needsHistoryReset) return;
            _historyReset.ConfigureInput(ScriptableRenderPassInput.None);
            renderer.EnqueuePass(_historyReset);
        }

#if UNITY_6000_0_OR_NEWER
        [Obsolete("This rendering path is for compatibility mode only (when Render Graph is disabled). Use Render Graph API instead.")]
#endif
        public override void SetupRenderPasses(ScriptableRenderer renderer, in RenderingData renderingData)
        {
            var upscaler = renderingData.cameraData.camera.GetComponent<Upscaler>();
            if (renderingData.cameraData.cameraType != CameraType.Game || upscaler == null || !upscaler.isActiveAndEnabled || upscaler.technique == Upscaler.Technique.None || upscaler.Backend == null) return;
            if (!_isResizingThisFrame)
            {
                var args = new object[] { true };
                UseScaling.Invoke(_upscale.Color, args);
                UseScaling.Invoke(_upscale.Depth, args);
                args = new object[] { (Vector2)upscaler.InputResolution / upscaler.OutputResolution };
                ScaleFactor.Invoke(_upscale.Color, args);
                ScaleFactor.Invoke(_upscale.Depth, args);
            }
            renderer.ConfigureCameraTarget(_upscale.Color, _upscale.Depth);
        }
    }
}
#endif