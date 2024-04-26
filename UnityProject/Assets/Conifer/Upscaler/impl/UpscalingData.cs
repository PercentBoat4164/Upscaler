/**********************************************************************************************************************
 * Conifer Limited License                                                                                            *
 * This software is provided under a custom limited license, subject to the following terms and conditions:           *
 * Individuals or entities who have purchased this software are granted the right to modify the source code for their *
 *  personal or internal business use only.                                                                           *
 * Redistribution or copying of the source code, in whole or in part, including modified source code, is strictly     *
 *  prohibited without prior written consent from the original author.                                                *
 * Any usage of the source code, including custom modifications, must be accompanied by this license as well as a     *
 *  prominent credit in the final product attributing the original author of the software.                            *
 * The original author reserves all rights not expressly granted herein.                                              *
 * Copyright © 2024 Conifer Computing Company. All rights reserved.                                                   *
 **********************************************************************************************************************/

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