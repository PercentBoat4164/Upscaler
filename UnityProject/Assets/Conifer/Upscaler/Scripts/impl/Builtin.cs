using UnityEngine;
using UnityEngine.Rendering;

namespace Conifer.Upscaler.Scripts.impl
{
    internal class Builtin
    {
        private static readonly int MotionID = Shader.PropertyToID("_CameraMotionVectorsTexture");
        private static readonly int OutputID = Shader.PropertyToID("Upscaler_OutputTexture");
        
        // CommandBuffers
        private readonly CommandBuffer _upscale;

        internal Builtin()
        {
            _upscale = new CommandBuffer();
            _upscale.name = "Upscale";
        }

        internal void PrepareRendering(Upscaler upscaler)
        {
            if (!Application.isPlaying) return;
            upscaler.UpscalingData.CameraTarget = upscaler.Plugin.Camera.targetTexture;
            upscaler.Plugin.Camera.targetTexture = upscaler.UpscalingData.ColorTarget;
        }

        internal void Upscale(Upscaler upscaler)
        {
            if (!Application.isPlaying) return;
            
            _upscale.Clear();
            _upscale.GetTemporaryRT(OutputID, upscaler.OutputResolution.x, upscaler.OutputResolution.y, 0,
                FilterMode.Point, upscaler.Plugin.ColorFormat(), 1, true, RenderTextureMemoryless.None, false);
            var motion = Shader.GetGlobalTexture(MotionID);
            var output = Shader.GetGlobalTexture(OutputID);
            if (motion is not null && output is not null)
                upscaler.Plugin.Upscale(_upscale, upscaler.Plugin.Camera.targetTexture, motion, output);
            _upscale.Blit(BuiltinRenderTextureType.CameraTarget, upscaler.UpscalingData.CameraTarget);
            Graphics.ExecuteCommandBuffer(_upscale);
            
            upscaler.Plugin.Camera.targetTexture = upscaler.UpscalingData.CameraTarget;
            
            _upscale.Clear();
            _upscale.Blit(output, upscaler.Plugin.Camera.targetTexture);
            _upscale.ReleaseTemporaryRT(OutputID);
            Graphics.ExecuteCommandBuffer(_upscale);
        }
        
        internal void Shutdown() => _upscale?.Release();
    }
}