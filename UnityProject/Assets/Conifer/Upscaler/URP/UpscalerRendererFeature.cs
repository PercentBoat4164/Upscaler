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

#if UPSCALER_USE_URP
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

namespace Conifer.Upscaler.URP
{
    public class UpscalerRendererFeature : ScriptableRendererFeature
    {
        private static readonly int MotionID = Shader.PropertyToID("_MotionVectorTexture");
        private static Upscaler _upscaler;
        
        private class Upscale : ScriptableRenderPass
        {
            public Upscale() => renderPassEvent = RenderPassEvent.AfterRendering + 5;

            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var upscale = CommandBufferPool.Get("Upscale");
                _upscaler.Plugin.Upscale(upscale, _upscaler.UpscalingData.SourceColorTarget,
                    Shader.GetGlobalTexture(Upscaler.SourceDepthID), Shader.GetGlobalTexture(MotionID),
                    _upscaler.UpscalingData.OutputColorTarget);
                Blitter.BlitCameraTexture(upscale, _upscaler.UpscalingData.OutputColorTarget,
                    renderingData.cameraData.renderer.cameraColorTargetHandle, renderingData.cameraData.postProcessEnabled ? new Vector4(1, -1, 0, 1) : new Vector4(1, 1, 0, 0));
                context.ExecuteCommandBuffer(upscale);
                upscale.Release();
            }
        }

        private class Sample : ScriptableRenderPass
        {
            public Sample() => renderPassEvent = RenderPassEvent.AfterRenderingPostProcessing;

            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                // var sample = CommandBufferPool.Get("UpscaleSample");
                _upscaler.UpscalingData.SourceColorTarget = renderingData.cameraData.renderer.cameraColorTargetHandle;
                // sample.Blit(renderingData.cameraData.renderer.cameraColorTargetHandle,
                //     _upscaler.UpscalingData.SourceColorTarget);
                // context.ExecuteCommandBuffer(sample);
                // sample.Release();
            }
        }

        private readonly Sample _sample = new();
        private readonly Upscale _upscale = new();
        private static bool _registered;

        public override void Create()
        {
            name = "Upscaler";
            if (!_registered) RenderPipelineManager.beginCameraRendering += PreUpscale;
            _registered = true;
        }

        public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
        {
            if (!EditorApplication.isPlaying || renderingData.cameraData.cameraType != CameraType.Game || !_registered) return;
            if (_upscaler is null) Debug.Log("All cameras using the Upscaler Renderer Feature must have the Upscaler script attached to them as well.", renderingData.cameraData.camera);
            if (_upscaler is null || _upscaler.settings.upscaler == Settings.Upscaler.None) return;
            _sample.ConfigureInput(ScriptableRenderPassInput.Color);
            renderer.EnqueuePass(_sample);
            _upscale.ConfigureInput(ScriptableRenderPassInput.Motion | ScriptableRenderPassInput.Depth);
            renderer.EnqueuePass(_upscale);
        }

        private static void PreUpscale(ScriptableRenderContext context, Camera camera)
        {
            if (!EditorApplication.isPlaying) return;
            _upscaler = camera.GetComponent<Upscaler>();
            _upscaler?.OnPreCull();
        }

        protected override void Dispose(bool dispose)
        {
            if (!dispose) return;
            if (_registered) RenderPipelineManager.beginCameraRendering -= PreUpscale;
            _registered = false;
        }
    }
}
#endif