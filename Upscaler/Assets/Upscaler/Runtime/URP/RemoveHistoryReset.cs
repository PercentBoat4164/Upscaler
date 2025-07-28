#if UPSCALER_USE_URP
using System;
using UnityEngine.Rendering;
using UnityEngine.Rendering.RenderGraphModule;
using UnityEngine.Rendering.Universal;

namespace Upscaler.Runtime.URP
{
    public partial class UpscalerRendererFeature
    {
        private class HistoryResetRenderPass : ScriptableRenderPass
        {
            public HistoryResetRenderPass() => renderPassEvent = RenderPassEvent.AfterRendering;

#if UNITY_6000_0_OR_NEWER
            public override void RecordRenderGraph(RenderGraph renderGraph, ContextContainer frameData) => frameData.Get<UniversalCameraData>().camera.GetComponent<Upscaler>().shouldHistoryResetThisFrame = false;

            [Obsolete("This rendering path is for compatibility mode only (when Render Graph is disabled). Use Render Graph API instead.", false)]
#endif
            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData) => renderingData.cameraData.camera.GetComponent<Upscaler>().shouldHistoryResetThisFrame = false;
        }
    }
}
#endif