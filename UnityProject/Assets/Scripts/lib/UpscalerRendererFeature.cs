using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

public class UpscalerRendererFeature : ScriptableRendererFeature
{
    private class UpscalerRenderPass : ScriptableRenderPass
    {
        // RenderTextures
        private RenderTexture _outputTarget;
        private RenderTexture _inColorTarget;
        private RenderTexture _motionVectorTarget;
        private RenderTexture _cameraTarget;

        // CommandBuffers
        private readonly CommandBuffer _upscale = CommandBufferPool.Get("Upscale");
        private readonly CommandBuffer _postUpscaleNullCameraTarget = CommandBufferPool.Get("Post Upscale");
        private readonly CommandBuffer _postUpscaleValidCameraTarget = CommandBufferPool.Get("Post Upscale");

        // Camera
        private readonly Camera _camera;

        public UpscalerRenderPass(Camera camera) => _camera = camera;

        private void UpdateUpscaleCommandBuffer()
        {
            _upscale.Clear();
            _upscale.CopyTexture(BuiltinRenderTextureType.MotionVectors, _motionVectorTarget);
            _upscale.IssuePluginEvent(Plugin.GetRenderingEventCallback(), (int)Plugin.Event.Upscale);
        }

        public void UpdatePostUpscaleCommandBuffer()
        {
            _postUpscaleNullCameraTarget.Clear();
            _postUpscaleValidCameraTarget.Clear();

            // The two command buffers are the same save that one copies while the other blits. Copying is faster, but you can only blit to the screen. The command buffer that is actually used is decided during rendering based on the CameraTarget for the given frame.
            _postUpscaleNullCameraTarget.Blit(_outputTarget, _cameraTarget);
            _postUpscaleValidCameraTarget.CopyTexture(_outputTarget, _cameraTarget);

            TexMan.BlitToCameraDepth(_postUpscaleNullCameraTarget, _inColorTarget);
            TexMan.BlitToCameraDepth(_postUpscaleValidCameraTarget, _inColorTarget);
        }

        public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
        {
            // Execute the upscale
            context.ExecuteCommandBuffer(_upscale);
            _camera.targetTexture = _cameraTarget;
            RenderTexture.active = _cameraTarget;
            context.ExecuteCommandBuffer(_cameraTarget ? _postUpscaleValidCameraTarget : _postUpscaleNullCameraTarget);
        }

        public void PreUpscale()
        {
            _cameraTarget = _camera.targetTexture;
            _camera.targetTexture = _inColorTarget;
            RenderTexture.active = _inColorTarget;
        }

        public bool ManageOutputTarget(Plugin.Mode mode, Vector2Int upscalingResolution)
        {
            var dTarget = false;
            var cameraTargetIsOutputTarget = _camera.targetTexture == _outputTarget;
            if (_outputTarget && _outputTarget.IsCreated())
            {
                _outputTarget.Release();
                _outputTarget = null;
                dTarget = true;
            }

            if (mode == Plugin.Mode.None) return dTarget;

            if (!_camera.targetTexture | cameraTargetIsOutputTarget)
            {
                _outputTarget = new RenderTexture(upscalingResolution.x, upscalingResolution.y, 0, Plugin.ColorFormat(_camera.allowHDR))
                {
                    enableRandomWrite = true
                };
                _outputTarget.Create();
            }
            else
            {
                _outputTarget = _cameraTarget;
                /*todo Throw a warning if enableRandomWrite is false on _cameraTarget. */
            }

            Plugin.SetOutputColor(_outputTarget.GetNativeTexturePtr(), _outputTarget.graphicsFormat);
            return true;
        }

        public bool ManageMotionVectorTarget(Plugin.Mode mode, Vector2Int maximumDynamicRenderingResolution)
        {
            var dTarget = false;
            if (_motionVectorTarget && _motionVectorTarget.IsCreated())
            {
                _motionVectorTarget.Release();
                _motionVectorTarget = null;
                dTarget = true;
            }

            if (mode == Plugin.Mode.None) return dTarget;

            _motionVectorTarget = new RenderTexture(maximumDynamicRenderingResolution.x, maximumDynamicRenderingResolution.y, 0, Plugin.MotionFormat());
            _motionVectorTarget.Create();

            Plugin.SetMotionVectors(_motionVectorTarget.GetNativeTexturePtr(), _motionVectorTarget.graphicsFormat);
            UpdateUpscaleCommandBuffer();
            return true;
        }

        public bool ManageInColorTarget(Plugin.Mode mode, Vector2Int maximumDynamicRenderingResolution)
        {
            var dTarget = false;
            if (_inColorTarget && _inColorTarget.IsCreated())
            {
                _inColorTarget.Release();
                _inColorTarget = null;
                dTarget = true;
            }

            if (mode == Plugin.Mode.None) return dTarget;

            _inColorTarget =
                new RenderTexture(maximumDynamicRenderingResolution.x, maximumDynamicRenderingResolution.y, Plugin.ColorFormat(_camera.allowHDR), Plugin.DepthFormat())
                {
                    filterMode = FilterMode.Point
                };
            _inColorTarget.Create();

            Plugin.SetDepthBuffer(_inColorTarget.GetNativeDepthBufferPtr(), _inColorTarget.depthStencilFormat);
            Plugin.SetInputColor(_inColorTarget.GetNativeTexturePtr(), _inColorTarget.graphicsFormat);
            return true;
        }

        public void Shutdown()
        {
            // Release command buffers
            CommandBufferPool.Release(_upscale);
            CommandBufferPool.Release(_postUpscaleNullCameraTarget);
            CommandBufferPool.Release(_postUpscaleValidCameraTarget);

            if (_outputTarget && _outputTarget.IsCreated())
                _outputTarget.Release();

            if (_inColorTarget && _inColorTarget.IsCreated())
                _inColorTarget.Release();

            if (_motionVectorTarget && _motionVectorTarget.IsCreated())
                _motionVectorTarget.Release();
        }
    }

    // Camera
    public Camera camera;

    // Render passes
    private UpscalerRenderPass _upscalerRenderRenderPass;

    public override void Create() => _upscalerRenderRenderPass = new UpscalerRenderPass(camera);

    public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
    {
        _upscalerRenderRenderPass.renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing + 1;
        renderer.EnqueuePass(_upscalerRenderRenderPass);
        _upscalerRenderRenderPass.ConfigureInput(ScriptableRenderPassInput.Color | ScriptableRenderPassInput.Depth | ScriptableRenderPassInput.Motion);
    }

    public void PreUpscale() => _upscalerRenderRenderPass.PreUpscale();

    public void UpdatePostUpscaleCommandBuffer() => _upscalerRenderRenderPass.UpdatePostUpscaleCommandBuffer();

    public bool ManageOutputTarget(Plugin.Mode mode, Vector2Int upscalingResolution) =>
        _upscalerRenderRenderPass.ManageOutputTarget(mode, upscalingResolution);

    public bool ManageMotionVectorTarget(Plugin.Mode mode, Vector2Int upscalingResolution) =>
        _upscalerRenderRenderPass.ManageMotionVectorTarget(mode, upscalingResolution);

    public bool ManageInColorTarget(Plugin.Mode mode, Vector2Int maximumDynamicRenderingResolution) =>
        _upscalerRenderRenderPass.ManageInColorTarget(mode, maximumDynamicRenderingResolution);

    public void Shutdown() => _upscalerRenderRenderPass.Shutdown();
}