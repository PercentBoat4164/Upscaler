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
    private readonly CommandBuffer _postUpscaleNullCameraTarget;
    private readonly CommandBuffer _postUpscaleValidCameraTarget;

    public Builtin(Camera camera) : base(camera, PipelineType.Builtin)
    {
        // Set up command buffers
        _upscale = new CommandBuffer();
        _upscale.name = "Upscale";
        _postUpscaleNullCameraTarget = new CommandBuffer();
        _postUpscaleNullCameraTarget.name = "Post Upscale";
        _postUpscaleValidCameraTarget = new CommandBuffer();
        _postUpscaleValidCameraTarget.name = "Post Upscale";
    }

    public void PrepareRendering()
    {
        _cameraTarget = Camera.targetTexture;
        Camera.targetTexture = _inColorTarget;
        RenderTexture.active = _inColorTarget;
    }

    public void Upscale()
    {
        Graphics.ExecuteCommandBuffer(_upscale);
        Camera.targetTexture = _cameraTarget;
        RenderTexture.active = _cameraTarget;
        Graphics.ExecuteCommandBuffer(_cameraTarget ? _postUpscaleValidCameraTarget : _postUpscaleNullCameraTarget);
        if (Input.GetKey(KeyCode.V))
            Graphics.Blit(_inColorTarget, _cameraTarget);
    }

    private void UpdateUpscaleCommandBuffer()
    {
        _upscale.Clear();
        _upscale.CopyTexture(BuiltinRenderTextureType.MotionVectors, _motionVectorTarget);
        _upscale.IssuePluginEvent(Plugin.GetRenderingEventCallback(), (int)Plugin.Event.Upscale);
    }

    public override void UpdatePostUpscaleCommandBuffer()
    {
        _postUpscaleNullCameraTarget.Clear();
        _postUpscaleValidCameraTarget.Clear();

        // The two command buffers are the same save that one copies while the other blits. Copying is faster, but you can only blit to the screen. The command buffer that is actually used is decided during rendering based on the CameraTarget for the given frame.
        _postUpscaleNullCameraTarget.Blit(_outputTarget, BuiltinRenderTextureType.CameraTarget);
        _postUpscaleValidCameraTarget.CopyTexture(_outputTarget, BuiltinRenderTextureType.CameraTarget);

        TexMan.BlitToCameraDepth(_postUpscaleNullCameraTarget, _inColorTarget);
        TexMan.BlitToCameraDepth(_postUpscaleValidCameraTarget, _inColorTarget);
    }

    public override bool ManageOutputTarget(Plugin.Mode mode, Vector2Int upscalingResolution)
    {
        var dTarget = false;
        var cameraTargetIsOutputTarget = Camera.targetTexture == _outputTarget;
        if (_outputTarget && _outputTarget.IsCreated())
        {
            _outputTarget.Release();
            _outputTarget = null;
            dTarget = true;
        }

        if (mode == Plugin.Mode.None) return dTarget;

        if (!Camera.targetTexture | cameraTargetIsOutputTarget)
        {
            _outputTarget = new RenderTexture(upscalingResolution.x, upscalingResolution.y, 0, Plugin.ColorFormat(Camera.allowHDR))
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

        _motionVectorTarget = new RenderTexture(maximumDynamicRenderingResolution.x, maximumDynamicRenderingResolution.y, 0, Plugin.MotionFormat())
        {
            useDynamicScale = true
        };
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
            new RenderTexture(maximumDynamicRenderingResolution.x, maximumDynamicRenderingResolution.y, Plugin.ColorFormat(Camera.allowHDR), Plugin.DepthFormat())
            {
                filterMode = FilterMode.Point, useDynamicScale = true
            };
        _inColorTarget.Create();

        Plugin.SetDepthBuffer(_inColorTarget.GetNativeDepthBufferPtr(), _inColorTarget.depthStencilFormat);
        Plugin.SetInputColor(_inColorTarget.GetNativeTexturePtr(), _inColorTarget.graphicsFormat);
        return true;
    }

    public override void Shutdown()
    {
        _upscale?.Release();
        _postUpscaleNullCameraTarget?.Release();
        _postUpscaleValidCameraTarget?.Release();

        if (_outputTarget && _outputTarget.IsCreated())
            _outputTarget.Release();

        if (_inColorTarget && _inColorTarget.IsCreated())
            _inColorTarget.Release();

        if (_motionVectorTarget && _motionVectorTarget.IsCreated())
            _motionVectorTarget.Release();
    }
}