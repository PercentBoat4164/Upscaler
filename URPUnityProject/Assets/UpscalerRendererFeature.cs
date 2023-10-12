using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

public class UpscalerRendererFeature : ScriptableRendererFeature
{
    private class PreUpscalerPass : ScriptableRenderPass
    {
        private BackendUpscaler _parent;
        private static Material _blitToMotionTextureMaterial;
        private static Mesh _quad;

        public PreUpscalerPass(BackendUpscaler parent)
        {
            _parent = parent;

            _quad = new Mesh();
            _quad.SetVertices(new Vector3[]
            {
                new(-1, -1, 0),
                new(1, -1, 0),
                new(-1, 1, 0),
                new(1, 1, 0)
            });
            _quad.SetUVs(0, new Vector2[]
            {
                new(0, 0),
                new(1, 0),
                new(0, 1),
                new(1, 1)
            });
            _quad.SetIndices(new[] { 2, 1, 0, 1, 2, 3 }, MeshTopology.Triangles, 0);
        }

        private static void BlitToMotionTexture(CommandBuffer cb, RenderTexture dest, Vector2? scale = null)
        {
            // Set up material
            _blitToMotionTextureMaterial.SetVector(Shader.PropertyToID("_ScaleFactor"), scale ?? Vector2.one);

            // Record command buffer
            cb.SetProjectionMatrix(Matrix4x4.Ortho(-1, 1, -1, 1, 1, -1));
            cb.SetViewMatrix(Matrix4x4.LookAt(new Vector3(0, 0, -1), new Vector3(0, 0, 1), Vector3.up));
            cb.SetRenderTarget(dest.colorBuffer);
            cb.DrawMesh(_quad, Matrix4x4.identity, _blitToMotionTextureMaterial);
        }

        public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
        {
            var cb = CommandBufferPool.Get("PreUpscale");
            BlitToMotionTexture(cb, _parent._motionVectorTarget);
            context.ExecuteCommandBuffer(cb);
            CommandBufferPool.Release(cb);
        }
    }

    private class UpscalerPass : ScriptableRenderPass
    {
        private BackendUpscaler _parent;

        private static Material _blitToMotionTextureMaterial;
        private static Mesh _quad;

        public UpscalerPass(BackendUpscaler parent)
        {
            _parent = parent;
        }

        public override void Execute(ScriptableRenderContext context, ref RenderingData renderingData)
        {
            var cb = CommandBufferPool.Get("Upscale");
            cb.Blit(renderingData.cameraData.targetTexture, _parent._inColorTarget);
            // cb.CopyTexture(renderingData.cameraData.targetTexture, 0, 0, 0, 0, (int)_parent.RenderingResolution.x,
            //     (int)_parent.RenderingResolution.y, _parent._inColorTarget, 0, 0, 0, 0);
            BackendUpscaler.BlitToDepthTexture(cb, _parent._inColorTarget);
            cb.SetViewport(new Rect(0, 0, _parent.UpscalingResolution.x, _parent.UpscalingResolution.y));
            cb.IssuePluginEvent(BackendUpscaler.Upscaler_GetRenderingEventCallback(), (int)BackendUpscaler.Event.Upscale);
            BackendUpscaler.BlitToCameraDepth(cb, _parent._inColorTarget);
            cb.Blit(_parent._outputTarget, renderingData.cameraData.targetTexture);
            context.ExecuteCommandBuffer(cb);
            CommandBufferPool.Release(cb);
        }
    }

    public BackendUpscaler upscaler;
    private PreUpscalerPass _pup;
    private UpscalerPass _up;

    public override void Create()
    {
        _pup = new PreUpscalerPass(upscaler);
        _up = new UpscalerPass(upscaler);
    }

    public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
    {
        UniversalRenderPipeline.asset.upscalingFilter = UpscalingFilterSelection.Point;
        UniversalRenderPipeline.asset.renderScale = upscaler.RenderingResolution.x / upscaler.UpscalingResolution.x;

        _pup.ConfigureInput(ScriptableRenderPassInput.Motion);
        _pup.renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing + 1;
        renderer.EnqueuePass(_pup);

        _up.ConfigureInput(ScriptableRenderPassInput.Depth | ScriptableRenderPassInput.Color);
        _up.renderPassEvent = RenderPassEvent.BeforeRenderingPostProcessing + 2;
        renderer.EnqueuePass(_up);
    }
}