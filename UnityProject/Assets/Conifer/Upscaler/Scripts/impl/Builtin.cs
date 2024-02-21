using UnityEngine;
using UnityEngine.Rendering;

namespace Conifer.Upscaler.Scripts.impl
{
    public class Builtin
    {
        // CommandBuffers
        private readonly CommandBuffer _upscale;
        private readonly CommandBuffer _postUpscale;
        private readonly Plugin _plugin;

        public Builtin(Plugin plugin)
        {
            _plugin = plugin;
            // Set up command buffers
            _upscale = new CommandBuffer();
            _upscale.name = "Upscale";
            _postUpscale = new CommandBuffer();
            _postUpscale.name = "Post Upscale";
        }

        public void PrepareRendering(UpscalingData data)
        {
            if (!Application.isPlaying) return;
            data.CameraTarget = _plugin.Camera.targetTexture;
            _plugin.Camera.targetTexture = data.InColorTarget;
            RenderTexture.active = data.InColorTarget;
        }

        public void Upscale(UpscalingData data)
        {
            if (!Application.isPlaying) return;

            _upscale.Clear();
            _upscale.CopyTexture(BuiltinRenderTextureType.MotionVectors, data.MotionVectorTarget);
            _plugin.Upscale(_upscale);
            Graphics.ExecuteCommandBuffer(_upscale);

            _plugin.Camera.targetTexture = data.CameraTarget;
            RenderTexture.active = data.CameraTarget;

            _postUpscale.Clear();
            _postUpscale.Blit(data.OutputTarget, BuiltinRenderTextureType.CameraTarget);
            BlitLib.BlitToCameraDepth(_postUpscale, data.InColorTarget);
            Graphics.ExecuteCommandBuffer(_postUpscale);
        }
        
        public void Shutdown()
        {
            _upscale?.Release();
            _postUpscale?.Release();
        }
    }
}