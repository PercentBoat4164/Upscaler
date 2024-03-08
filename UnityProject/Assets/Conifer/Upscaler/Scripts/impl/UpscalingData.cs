using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;

namespace Conifer.Upscaler.Scripts.impl
{
    internal class UpscalingData
    {
        internal enum ImageID
        {
            SourceColor,
            SourceDepth,
            Motion,
            OutputColor,
        }

#if UPSCALER_USE_URP
        internal RTHandle OutputColorTarget;
        internal RTHandle SourceDepthTarget;
#else
        internal RenderTexture OutputColorTarget;
        internal RenderTexture SourceDepthTarget;
#endif
        private static readonly Mesh Quad;
        private static readonly Material DepthCopyMaterial = new(Shader.Find("Hidden/DepthCopy"));
        private static readonly int DepthTex = Shader.PropertyToID("_Depth");

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
#if UPSCALER_USE_URP
            OutputColorTarget = RTHandles.Alloc(resolution.x, resolution.y, 1, DepthBits.None, format, FilterMode.Point, TextureWrapMode.Repeat, TextureDimension.Tex2D, true);
#else
            OutputColorTarget = new RenderTexture(resolution.x, resolution.y, format, GraphicsFormat.None) {enableRandomWrite = true};
            OutputColorTarget.Create();
#endif
        }

        internal void ManageSourceDepthTarget(bool dynamicResolution, Settings.Upscaler upscaler, Vector2Int resolution)
        {
            if (SourceDepthTarget is not null)
            {
                SourceDepthTarget.Release();
                SourceDepthTarget = null;
            }
            if (upscaler == Settings.Upscaler.None) return;
#if UPSCALER_USE_URP
            SourceDepthTarget = RTHandles.Alloc(resolution.x, resolution.y, 1, DepthBits.Depth32, SystemInfo.GetGraphicsFormat(DefaultFormat.DepthStencil), FilterMode.Point, TextureWrapMode.Repeat, TextureDimension.Tex2D, false, false, false, false, 1, 0f, MSAASamples.None, false, dynamicResolution);
#else
            SourceDepthTarget = new RenderTexture(resolution.x, resolution.y, GraphicsFormat.None, SystemInfo.GetGraphicsFormat(DefaultFormat.DepthStencil)) {useDynamicScale = dynamicResolution};
            SourceDepthTarget.Create();
#endif
        }

        internal static void BlitDepth(CommandBuffer cb, RenderTexture source, RenderTargetIdentifier destination)
        {
            DepthCopyMaterial.SetTexture(DepthTex, source, RenderTextureSubElement.Depth);
            BlitDepth(cb, destination);
        }
        
        internal static void BlitDepth(CommandBuffer cb, Texture source, RenderTargetIdentifier destination)
        {
            DepthCopyMaterial.SetTexture(DepthTex, source);
            BlitDepth(cb, destination);
        }

        private static void BlitDepth(CommandBuffer cb, RenderTargetIdentifier destination)
        {
            cb.SetRenderTarget(destination);
            cb.SetProjectionMatrix(Matrix4x4.Ortho(-1, 1, -1, 1, 1, -1));
            cb.SetViewMatrix(Matrix4x4.LookAt(Vector3.back, Vector3.forward, Vector3.up));
            cb.DrawMesh(Quad, Matrix4x4.identity, DepthCopyMaterial);
        }
        
        internal void Release()
        {
            OutputColorTarget?.Release();
            SourceDepthTarget?.Release();
        }
    }
}