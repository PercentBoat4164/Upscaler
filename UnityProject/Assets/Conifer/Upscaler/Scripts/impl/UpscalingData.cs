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
            OutputColor
        }

        internal RTHandle OutputColorTarget;
        internal RTHandle SourceDepthTarget;

        internal void ManageOutputColorTarget(GraphicsFormat format, Settings.Upscaler upscaler, Vector2Int resolution)
        {
            if (OutputColorTarget is not null)
            {
                OutputColorTarget.Release();
                OutputColorTarget = null;
            }

            if (upscaler == Settings.Upscaler.None) return;
            OutputColorTarget = RTHandles.Alloc(resolution.x, resolution.y, 1, DepthBits.None, format, FilterMode.Point,
                TextureWrapMode.Repeat, TextureDimension.Tex2D, true);
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

        public void Release()
        {
            OutputColorTarget?.Release();
            SourceDepthTarget?.Release();
        }
    }
}