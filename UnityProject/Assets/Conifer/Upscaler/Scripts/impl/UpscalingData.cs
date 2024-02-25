using UnityEngine;

namespace Conifer.Upscaler.Scripts.impl
{
    internal class UpscalingData
    {
        internal RenderTexture CameraTarget;
        internal RenderTexture ColorTarget;
        
        internal void ManageColorTarget(Plugin plugin, Settings.Upscaler upscaler, Vector2Int resolution)
        {
            if (ColorTarget && ColorTarget.IsCreated())
            {
                ColorTarget.Release();
                ColorTarget = null;
            }

            if (upscaler == Settings.Upscaler.None)
                return;

            ColorTarget =
                new RenderTexture(resolution.x, resolution.y, plugin.ColorFormat(), Plugin.DepthFormat())
                {
                    filterMode = FilterMode.Point
                };
            ColorTarget.Create();
        }

        public void Release()
        {
            if (ColorTarget && ColorTarget.IsCreated())
                ColorTarget.Release();
        }
    }
}