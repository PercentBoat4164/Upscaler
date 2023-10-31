using System;
using UnityEngine;
using UnityEngine.Rendering;

public class Builtin : RenderPipeline
{
    // RenderTextures
    private RenderTexture _outputTarget;
    private RenderTexture _inColorTarget;
    private RenderTexture _motionVectorTarget;
    private RenderTexture _cameraTarget;

    // CommandBuffers
    private readonly CommandBuffer _upscale;

    public Builtin(Camera camera) : base(camera, PipelineType.Builtin)
    {
        // Set up command buffers
        _upscale = new CommandBuffer();
        _upscale.name = "Upscale";
    }

    public void PrepareRendering()
    {
        _cameraTarget = _camera.targetTexture;
        _camera.targetTexture = _inColorTarget;
        RenderTexture.active = _inColorTarget;
    }

    public void Upscale()
    {
        Graphics.ExecuteCommandBuffer(_upscale);
        _camera.targetTexture = _cameraTarget;
        RenderTexture.active = _cameraTarget;
        if (_cameraTarget)
            Graphics.CopyTexture(_outputTarget, _cameraTarget);
        else
            Graphics.Blit(_outputTarget, _cameraTarget);
        TexMan.BlitToCameraDepth(_inColorTarget);
    }

    private void UpdateUpscaleCommandBuffer()
    {
        _upscale.Clear();
        _upscale.CopyTexture(BuiltinRenderTextureType.MotionVectors, _motionVectorTarget);
        _upscale.IssuePluginEvent(Plugin.GetRenderingEventCallback(), (int)Plugin.Event.Upscale);
    }

    public override bool ManageOutputTarget(Plugin.Mode mode, Vector2Int upscalingResolution)
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

    public override bool ManageMotionVectorTarget(Plugin.Mode mode, Vector2Int maximumDynamicRenderingResolution)
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

    public override bool ManageInColorTarget(Plugin.Mode mode, Vector2Int maximumDynamicRenderingResolution)
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

    public override void Shutdown()
    {
        _upscale?.Release();

        if (_outputTarget && _outputTarget.IsCreated())
            _outputTarget.Release();

        if (_inColorTarget && _inColorTarget.IsCreated())
            _inColorTarget.Release();

        if (_motionVectorTarget && _motionVectorTarget.IsCreated())
            _motionVectorTarget.Release();
    }
}