using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;

namespace Conifer.Upscaler.Scripts.impl
{
    internal class UpscalingData
    {
        public enum ImageID
        {
            SourceColor,
            SourceDepth,
            Motion,
            OutputColor,
        }
        
        internal RTHandle OutputColorTarget;
        internal RTHandle SourceDepthTarget;
        private static readonly Mesh Quad;
        private readonly Material _depthCopyMaterial = new(Shader.Find("Hidden/DepthCopy"));
        private readonly int _depthTex = Shader.PropertyToID("_Depth");

        static UpscalingData()
        {
            Quad = new Mesh();
            Quad.SetVertices(new Vector3[]
            {
                new(-1, -1, 0),
                new(1, -1, 0),
                new(-1, 1, 0),
                new(1, 1, 0)
            });
            Quad.SetUVs(0, new Vector2[]
            {
                new(0, 0),
                new(1, 0),
                new(0, 1),
                new(1, 1)
            });
            Quad.SetTriangles(new[] { 1, 0, 2, 1, 2, 3 }, 0);
        }

        internal void ManageOutputColorTarget(GraphicsFormat format, Settings.Upscaler upscaler, Vector2Int resolution) 
        {
            if (OutputColorTarget is not null)
            {
                OutputColorTarget.Release();
                OutputColorTarget = null;
            }
            if (upscaler == Settings.Upscaler.None) return;
            OutputColorTarget = RTHandles.Alloc(resolution.x, resolution.y, 1, DepthBits.None, format, FilterMode.Point, TextureWrapMode.Repeat, TextureDimension.Tex2D, true);
        }

        internal void ManageSourceDepthTarget(bool dynamicResolution, Settings.Upscaler upscaler, Vector2Int resolution)
        {
            if (SourceDepthTarget is not null)
            {
                SourceDepthTarget.Release();
                SourceDepthTarget = null;
            }
            if (upscaler == Settings.Upscaler.None) return;
            SourceDepthTarget = RTHandles.Alloc(resolution.x, resolution.y, 1, DepthBits.Depth32,
                SystemInfo.GetGraphicsFormat(DefaultFormat.DepthStencil), FilterMode.Point, TextureWrapMode.Repeat,
                TextureDimension.Tex2D, false, false, false, false, 1, 0f, MSAASamples.None, false, dynamicResolution);
        }

        internal void BlitToSourceDepth(CommandBuffer cb, RenderTexture source)
        {
            _depthCopyMaterial.SetTexture(_depthTex, source, RenderTextureSubElement.Depth);
            BlitToSourceDepth(cb);
        }
        
        internal void BlitToSourceDepth(CommandBuffer cb, Texture source)
        {
            _depthCopyMaterial.SetTexture(_depthTex, source);
            BlitToSourceDepth(cb);
        }

        private void BlitToSourceDepth(CommandBuffer cb)
        {
            cb.SetRenderTarget(SourceDepthTarget);
            cb.SetProjectionMatrix(Matrix4x4.Ortho(-1, 1, -1, 1, 1, -1));
            cb.SetViewMatrix(Matrix4x4.LookAt(Vector3.back, Vector3.forward, Vector3.up));
            cb.DrawMesh(Quad, Matrix4x4.identity, _depthCopyMaterial);
        }
        
        public void Release()
        {
            OutputColorTarget?.Release();
            SourceDepthTarget?.Release();
        }
    }
}