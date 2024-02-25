using UnityEngine;
using UnityEngine.Rendering;

namespace Conifer.Upscaler.Scripts.impl
{
    internal class Builtin
    {
        private static readonly int ColorID = Shader.PropertyToID("_CameraColorTexture");
        private static readonly int DepthID = Shader.PropertyToID("_CameraDepthTexture");
        private static readonly int MotionID = Shader.PropertyToID("_CameraMotionVectorsTexture");
        private static readonly int OutputID = Shader.PropertyToID("Upscaler_OutputTexture");
        
        // CommandBuffers
        private readonly CommandBuffer _upscale;
        private readonly Plugin _plugin;

        internal Builtin(Plugin plugin)
        {
            _plugin = plugin;
            // Set up command buffers
            _upscale = new CommandBuffer();
            _upscale.name = "Upscale";
        }

        internal void PrepareRendering(Upscaler upscaler)
        {
            if (!Application.isPlaying) return;
            upscaler.UpscalingData.CameraTarget = _plugin.Camera.targetTexture;
            _plugin.Camera.targetTexture = upscaler.UpscalingData.ColorTarget;
        }

        internal void Upscale(Upscaler upscaler)
        {
            if (!Application.isPlaying) return;
            
            _upscale.Clear();
            _upscale.GetTemporaryRT(OutputID, upscaler.OutputResolution.x, upscaler.OutputResolution.y, 0,
                FilterMode.Point, _plugin.ColorFormat(), 1, true, RenderTextureMemoryless.None, false);
            Shader.SetGlobalTexture(ColorID, upscaler.Plugin.Camera.targetTexture, RenderTextureSubElement.Color);
            var color = Shader.GetGlobalTexture(ColorID);
            var depth = Shader.GetGlobalTexture(DepthID);
            var motion = Shader.GetGlobalTexture(MotionID);
            var output = Shader.GetGlobalTexture(OutputID);
            if (color is not null && depth is not null && motion is not null && output is not null)
                _plugin.Upscale(_upscale, color, depth, motion, output);
            _upscale.Blit(BuiltinRenderTextureType.CameraTarget, upscaler.UpscalingData.CameraTarget);
            Graphics.ExecuteCommandBuffer(_upscale);
            
            _plugin.Camera.targetTexture = upscaler.UpscalingData.CameraTarget;
            
            _upscale.Clear();
            _upscale.Blit(output, _plugin.Camera.targetTexture);
            _upscale.ReleaseTemporaryRT(OutputID);
            Graphics.ExecuteCommandBuffer(_upscale);
        }
        
        internal void Shutdown() => _upscale?.Release();
    }
}