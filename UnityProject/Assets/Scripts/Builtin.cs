using UnityEngine;
using UnityEngine.Rendering;

public class Builtin : RenderPipeline
{
    // RenderTextures
    private RenderTexture _outputTarget;
    private RenderTexture _inColorTarget;
    private RenderTexture _motionVectorTarget;

    // CommandBuffers
    private readonly CommandBuffer _setRenderingResolution;
    private readonly CommandBuffer _upscale;

    private readonly Camera _camera;

    public Builtin(Camera camera)
    {
        _camera = camera;

        // Set up command buffers
        _setRenderingResolution = new CommandBuffer();
        _setRenderingResolution.name = "Set Render Resolution";
        _upscale = new CommandBuffer();
        _upscale.name = "Upscale";

        _camera.AddCommandBuffer(CameraEvent.BeforeGBuffer, _setRenderingResolution);
        _camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque, _setRenderingResolution);
        _camera.AddCommandBuffer(CameraEvent.BeforeSkybox, _setRenderingResolution);

        _camera.AddCommandBuffer(CameraEvent.BeforeImageEffectsOpaque, _upscale);
    }

    public override void PrepareRendering(
        Vector2 renderingResolution,
        Vector2 upscalingResolution,
        Plugin.Mode mode
    ) {
        _setRenderingResolution.Clear();
        _upscale.Clear();

        if (mode == Plugin.Mode.None) return;

        _setRenderingResolution.SetViewport(new Rect(Vector2.zero, renderingResolution));

        /*@todo Fix shadows when using the deferred rendering path. */
        /*@todo Use the full resolution depth buffers when using the forward rendering path. */
        _upscale.CopyTexture(BuiltinRenderTextureType.MotionVectors, 0, 0, 0, 0, (int)upscalingResolution.x,
            (int)upscalingResolution.y, _motionVectorTarget, 0, 0, 0, 0);
        _upscale.CopyTexture(BuiltinRenderTextureType.CameraTarget, 0, 0, 0, 0, (int)renderingResolution.x,
            (int)renderingResolution.y, _inColorTarget, 0, 0, 0, 0);
        TexMan.BlitToDepthTexture(_upscale, _inColorTarget, renderingResolution / upscalingResolution);
        _upscale.SetViewport(new Rect(Vector2.zero, upscalingResolution));
        _upscale.IssuePluginEvent(Plugin.GetRenderingEventCallback(), (int)Plugin.Event.Upscale);
        TexMan.BlitToCameraDepth(_upscale, _inColorTarget);
        _upscale.CopyTexture(_outputTarget, BuiltinRenderTextureType.CameraTarget);
    }

    public override void Shutdown()
    {
        if (_setRenderingResolution != null)
        {
            _camera.RemoveCommandBuffer(CameraEvent.BeforeGBuffer, _setRenderingResolution);
            _camera.RemoveCommandBuffer(CameraEvent.BeforeForwardOpaque, _setRenderingResolution);
            _camera.RemoveCommandBuffer(CameraEvent.BeforeSkybox, _setRenderingResolution);
            _setRenderingResolution.Release();
        }

        // ReSharper disable once InvertIf
        if (_upscale != null)
        {
            _camera.RemoveCommandBuffer(CameraEvent.BeforeImageEffectsOpaque, _upscale);
            _upscale.Release();
        }
    }

    public override bool ManageOutputTarget(Plugin.Mode mode, Vector2Int upscalingResolution)
    {
        var dTarget = false;
        if (_outputTarget != null && _outputTarget.IsCreated())
        {
            _outputTarget.Release();
            _outputTarget = null;
            dTarget = true;
        }

        /*@todo If the camera's target is null, use Display.displays[_camera.targetDisplay].colorBuffer */

        if (mode == Plugin.Mode.None) return dTarget;
        _outputTarget =
            new RenderTexture(upscalingResolution.x, upscalingResolution.y, 0, ColorFormat(_camera.allowHDR))
            {
                enableRandomWrite = true
            };
        _outputTarget.Create();

        Plugin.SetOutputColor(_outputTarget.GetNativeTexturePtr(), _outputTarget.graphicsFormat);
        return true;
    }

    public override bool ManageMotionVectorTarget(Plugin.Mode mode, Vector2Int upscalingResolution)
    {
        var dTarget = false;
        if (_motionVectorTarget != null && _motionVectorTarget.IsCreated())
        {
            _motionVectorTarget.Release();
            _motionVectorTarget = null;
            dTarget = true;
        }

        if (mode == Plugin.Mode.None) return dTarget;

        _motionVectorTarget = new RenderTexture(upscalingResolution.x, upscalingResolution.y, 0, MotionFormat);
        _motionVectorTarget.Create();

        Plugin.SetMotionVectors(_motionVectorTarget.GetNativeTexturePtr(), _motionVectorTarget.graphicsFormat);
        return true;
    }

    public override bool ManageInColorTarget(Plugin.Mode mode, Vector2Int upscalingResolution)
    {
        var dTarget = false;
        if (_inColorTarget != null && _inColorTarget.IsCreated())
        {
            _inColorTarget.Release();
            _inColorTarget = null;
            dTarget = true;
        }

        if (mode == Plugin.Mode.None) return dTarget;

        _inColorTarget =
            new RenderTexture(upscalingResolution.x, upscalingResolution.y, ColorFormat(_camera.allowHDR), DepthFormat)
            {
                filterMode = FilterMode.Point
            };
        _inColorTarget.Create();

        Plugin.SetDepthBuffer(_inColorTarget.GetNativeDepthBufferPtr(), _inColorTarget.depthStencilFormat);
        Plugin.SetInputColor(_inColorTarget.GetNativeTexturePtr(), _inColorTarget.graphicsFormat);
        return true;
    }
}