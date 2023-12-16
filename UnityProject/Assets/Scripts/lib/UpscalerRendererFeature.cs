#if UPSCALER_USE_URP
using System;
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
        private readonly CommandBuffer _postUpscale = CommandBufferPool.Get("Post Upscale");

        // Camera
        private readonly Camera _camera;

        // Action
        private readonly System.Action _onPreCull;

        public UpscalerRenderPass(Camera camera, Action onPreCull)
        {
            _camera = camera;
            _onPreCull = onPreCull;
        }

        private void UpdateUpscaleCommandBuffer()
        {
            _upscale.Clear();
            TexMan.BlitToMotionTexture(_upscale, _motionVectorTarget);
            _upscale.IssuePluginEvent(Plugin.GetRenderingEventCallback(), (int)Plugin.Event.Upscale);
        }

        public void UpdatePostUpscaleCommandBuffer()
        {
            _postUpscale.Clear();
            _postUpscale.Blit(_outputTarget, _cameraTarget);
            TexMan.CopyCameraDepth(_postUpscale);
        }

        public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
        {
            // Execute the upscale
            context.ExecuteCommandBuffer(_upscale);
            _camera.targetTexture = _cameraTarget;
            RenderTexture.active = _cameraTarget;
            context.ExecuteCommandBuffer(_postUpscale);
        }

        public void PreUpscale()
        {
            if (!_inColorTarget) return;

            _cameraTarget = _camera.targetTexture;
            _camera.targetTexture = _inColorTarget;
            RenderTexture.active = _inColorTarget;
        }

        public bool ManageOutputTarget(Plugin.UpscalerMode upscalerMode, Vector2Int resolution)
        {
            var dTarget = false;
            var cameraTargetIsOutputTarget = _camera.targetTexture == _outputTarget;
            if (_outputTarget && _outputTarget.IsCreated())
            {
                _outputTarget.Release();
                _outputTarget = null;
                dTarget = true;
            }

            if (upscalerMode == Plugin.UpscalerMode.None) return dTarget;

            if (!_camera.targetTexture | cameraTargetIsOutputTarget)
            {
                _outputTarget = new RenderTexture(resolution.x, resolution.y, 0, Plugin.ColorFormat(_camera.allowHDR))
                {
                    enableRandomWrite = true
                };
                _outputTarget.Create();
            }
            else
            {
                _outputTarget = _cameraTarget;
                /*todo Throw an error if enableRandomWrite is false on _cameraTarget. */
            }

            Plugin.SetOutputColor(_outputTarget.GetNativeTexturePtr(), _outputTarget.graphicsFormat);
            return true;
        }

        public bool ManageMotionVectorTarget(Plugin.UpscalerMode upscalerMode, Vector2Int resolution)
        {
            var dTarget = false;
            if (_motionVectorTarget && _motionVectorTarget.IsCreated())
            {
                _motionVectorTarget.Release();
                _motionVectorTarget = null;
                dTarget = true;
            }

            if (upscalerMode == Plugin.UpscalerMode.None) return dTarget;

            _motionVectorTarget = new RenderTexture(resolution.x, resolution.y, 0, Plugin.MotionFormat());
            _motionVectorTarget.Create();

            Plugin.SetMotionVectors(_motionVectorTarget.GetNativeTexturePtr(), _motionVectorTarget.graphicsFormat);
            UpdateUpscaleCommandBuffer();
            return true;
        }

        public bool ManageInColorTarget(Plugin.UpscalerMode upscalerMode, Vector2Int maximumDynamicRenderingResolution)
        {
            var dTarget = false;
            if (_inColorTarget && _inColorTarget.IsCreated())
            {
                _inColorTarget.Release();
                _inColorTarget = null;
                dTarget = true;
            }

            if (upscalerMode == Plugin.UpscalerMode.None) return dTarget;

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
            CommandBufferPool.Release(_postUpscale);

            if (_outputTarget && _outputTarget.IsCreated())
                _outputTarget.Release();

            if (_inColorTarget && _inColorTarget.IsCreated())
                _inColorTarget.Release();

            if (_motionVectorTarget && _motionVectorTarget.IsCreated())
                _motionVectorTarget.Release();
        }
    }

    // Camera
    [HideInInspector] public Camera camera;
    // OnPreCull action
    public System.Action OnPreCull;

    // Render passes
    private UpscalerRenderPass _upscalerRenderPass;

    public override void Create()
    {
        _upscalerRenderPass = new UpscalerRenderPass(camera, OnPreCull);
        name = "Upscaler";
    }

    public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
    {
        _upscalerRenderPass.ConfigureInput(ScriptableRenderPassInput.Motion);
        _upscalerRenderPass.renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing + 1;
        renderer.EnqueuePass(_upscalerRenderPass);
    }

    public void PreUpscale() => _upscalerRenderPass.PreUpscale();

    public void UpdatePostUpscaleCommandBuffer() => _upscalerRenderPass.UpdatePostUpscaleCommandBuffer();

    public bool ManageOutputTarget(Plugin.UpscalerMode upscalerMode, Vector2Int resolution) =>
        _upscalerRenderPass.ManageOutputTarget(upscalerMode, resolution);

    public bool ManageMotionVectorTarget(Plugin.UpscalerMode upscalerMode, Vector2Int resolution) =>
        _upscalerRenderPass.ManageMotionVectorTarget(upscalerMode, resolution);

    public bool ManageInColorTarget(Plugin.UpscalerMode upscalerMode, Vector2Int resolution) =>
        _upscalerRenderPass.ManageInColorTarget(upscalerMode, resolution);

    public void Shutdown() => _upscalerRenderPass.Shutdown();
}
#endif