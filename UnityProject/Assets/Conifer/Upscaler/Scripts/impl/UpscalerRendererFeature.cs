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
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

namespace Conifer.Upscaler.Scripts.impl
{
    public class UpscalerRendererFeature : ScriptableRendererFeature
    {
        private static readonly int MotionID = Shader.PropertyToID("_MotionVectorTexture");
        private static readonly int SourceColorID = Shader.PropertyToID("_CameraColorTexture");
        private static readonly int SourceDepthID = Shader.PropertyToID("_CameraDepthTexture");
        private static Upscaler _upscaler;
        
        private class Upscale : ScriptableRenderPass
        {
            public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
            {
                var sourceDepth = Shader.GetGlobalTexture(SourceDepthID);
                if (sourceDepth is null) return;
                var outputColor = _upscaler.Camera.targetTexture ? _upscaler.Camera.targetTexture : _upscaler.UpscalingData.OutputColorTarget;
                var motion = Shader.GetGlobalTexture(MotionID);
                if (motion is null) return;
                
                var upscale = CommandBufferPool.Get("Upscale");
                UpscalingData.BlitDepth(upscale, sourceDepth, _upscaler.UpscalingData.SourceDepthTarget);
                _upscaler.Plugin.Upscale(upscale, Shader.GetGlobalTexture(SourceColorID),
                    _upscaler.UpscalingData.SourceDepthTarget, motion, outputColor);
                if (_upscaler.Camera.targetTexture is null)
                    upscale.Blit(outputColor.colorBuffer, _upscaler.Camera.targetTexture);
                else
                    UpscalingData.BlitDepth(upscale, _upscaler.UpscalingData.SourceDepthTarget, _upscaler.Camera.targetTexture.depthBuffer);

                context.ExecuteCommandBuffer(upscale);
                upscale.Release();
            }
        }

        // Render passes
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
            if (!Application.isPlaying || !_registered || _upscaler is null || _upscaler.settings.upscaler == Settings.Upscaler.None) return;
            _upscaler.Camera.rect = new Rect(0, 0, _upscaler.OutputResolution.x, _upscaler.OutputResolution.y);
            _upscale.ConfigureInput(ScriptableRenderPassInput.Motion);
            _upscale.ConfigureTarget(_upscaler.UpscalingData.OutputColorTarget);
            _upscale.renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing + 25;
            renderer.EnqueuePass(_upscale);
        }

        private static void PreUpscale(ScriptableRenderContext context, Camera camera)
        {
            if (!Application.isPlaying) return;
            _upscaler = camera.GetComponent<Upscaler>();
            if (_upscaler is null) Debug.Log("All cameras using the Upscaler Renderer Feature must have the Upscaler script attached to them as well.", camera);
            else _upscaler.OnPreCull();
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