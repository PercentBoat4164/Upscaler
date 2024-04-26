/**********************************************************************
 * This software contains source code provided by NVIDIA Corporation. *
 **********************************************************************/

using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;

namespace Conifer.Upscaler.impl
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

        internal RTHandle OutputColorTarget;
        internal RTHandle SourceColorTarget;

        private void CleanTarget(ref RTHandle target)
        {
            if (target is null) return;
            target.Release();
            target = null;
        }

        internal void ManageColorTargets(GraphicsFormat format, Settings.Upscaler upscaler, Vector2Int renderingResolution, Vector2Int outputResolution)
        {
            CleanTarget(ref OutputColorTarget);
            CleanTarget(ref SourceColorTarget);
            if (upscaler == Settings.Upscaler.None) return;
            OutputColorTarget = RTHandles.Alloc(outputResolution.x, outputResolution.y, colorFormat: format,
                enableRandomWrite: true, name: "UpscalerOutput");
            SourceColorTarget = RTHandles.Alloc(renderingResolution.x, renderingResolution.y,
                colorFormat: format, name: "UpscalerSource");
        }
        
        internal void Release() => OutputColorTarget?.Release();
    }
}