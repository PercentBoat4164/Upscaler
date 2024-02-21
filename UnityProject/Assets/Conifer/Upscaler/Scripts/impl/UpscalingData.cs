using UnityEngine;

namespace Conifer.Upscaler.Scripts.impl
{
    public class UpscalingData
    {
        internal RenderTexture OutputTarget;
        internal RenderTexture InColorTarget;
        internal RenderTexture MotionVectorTarget;
        internal RenderTexture CameraTarget;

        private readonly Plugin _plugin;

        public UpscalingData(Plugin plugin)
        {
            _plugin = plugin;
        }

        public bool ManageOutputTarget(Upscaler.UpscalerMode upscalerMode, Vector2Int resolution)
        {
            var dTarget = false;
            var cameraTargetIsOutputTarget = _plugin.Camera.targetTexture == OutputTarget;
            if (OutputTarget && OutputTarget.IsCreated())
            {
                OutputTarget.Release();
                OutputTarget = null;
                dTarget = true;
            }

            if (upscalerMode == Upscaler.UpscalerMode.None)
            {
                return dTarget;
            }

            if (!_plugin.Camera.targetTexture | cameraTargetIsOutputTarget)
            {
                OutputTarget = new RenderTexture(resolution.x, resolution.y, 0, _plugin.ColorFormat())
                {
                    enableRandomWrite = true
                };
                OutputTarget.Create();
            }
            else
            {
                OutputTarget = _plugin.Camera.targetTexture;
                /*todo Throw an error if enableRandomWrite is false on _cameraTarget. */
            }

            _plugin.SetOutputColor(OutputTarget);
            return true;
        }

        public bool ManageMotionVectorTarget(Upscaler.UpscalerMode upscalerMode, Vector2Int resolution)
        {
            var dTarget = false;
            if (MotionVectorTarget && MotionVectorTarget.IsCreated())
            {
                MotionVectorTarget.Release();
                MotionVectorTarget = null;
                dTarget = true;
            }

            if (upscalerMode == Upscaler.UpscalerMode.None)
            {
                return dTarget;
            }

            MotionVectorTarget = new RenderTexture(resolution.x, resolution.y, 0, Plugin.MotionFormat());
            MotionVectorTarget.Create();

            _plugin.SetMotionVectors(MotionVectorTarget);
            return true;
        }

        public bool ManageInColorTarget(Upscaler.UpscalerMode upscalerMode, Vector2Int resolution)
        {
            var dTarget = false;
            if (InColorTarget && InColorTarget.IsCreated())
            {
                InColorTarget.Release();
                InColorTarget = null;
                dTarget = true;
            }

            if (upscalerMode == Upscaler.UpscalerMode.None)
            {
                return dTarget;
            }

            InColorTarget =
                new RenderTexture(resolution.x, resolution.y, _plugin.ColorFormat(), Plugin.DepthFormat())
                {
                    filterMode = FilterMode.Point
                };
            InColorTarget.Create();

            _plugin.SetDepth(InColorTarget);
            _plugin.SetInputColor(InColorTarget);
            return true;
        }

        public void Release()
        {
            if (OutputTarget && OutputTarget.IsCreated())
                OutputTarget.Release();

            if (InColorTarget && InColorTarget.IsCreated())
                InColorTarget.Release();

            if (MotionVectorTarget && MotionVectorTarget.IsCreated())
                MotionVectorTarget.Release();
        }
    }
}