using UnityEngine;

namespace Conifer.Upscaler.Scripts.impl
{
    internal class UpscalingData
    {
        internal RenderTexture OutputTarget;
        internal RenderTexture InColorTarget;
        internal RenderTexture MotionVectorTarget;
        internal RenderTexture CameraTarget;

        private readonly Plugin _plugin;

        internal UpscalingData(Plugin plugin)
        {
            _plugin = plugin;
        }

        internal Upscaler.Status ManageOutputTarget(Settings.Upscaler upscaler, Vector2Int resolution)
        {
            var cameraTargetIsOutputTarget = _plugin.Camera.targetTexture == OutputTarget;
            if (OutputTarget && OutputTarget.IsCreated())
            {
                OutputTarget.Release();
                OutputTarget = null;
            }

            if (upscaler == Settings.Upscaler.None)
                return Upscaler.Status.Success;

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

            return _plugin.SetOutputColor(OutputTarget);
        }

        internal Upscaler.Status ManageMotionVectorTarget(Settings.Upscaler upscaler, Vector2Int resolution)
        {
            if (MotionVectorTarget && MotionVectorTarget.IsCreated())
            {
                MotionVectorTarget.Release();
                MotionVectorTarget = null;
            }

            if (upscaler == Settings.Upscaler.None)
                return Upscaler.Status.Success;

            MotionVectorTarget = new RenderTexture(resolution.x, resolution.y, 0, Plugin.MotionFormat());
            MotionVectorTarget.Create();

            return _plugin.SetMotionVectors(MotionVectorTarget);
        }

        internal Upscaler.Status ManageInColorTarget(Settings.Upscaler upscaler, Vector2Int resolution)
        {
            if (InColorTarget && InColorTarget.IsCreated())
            {
                InColorTarget.Release();
                InColorTarget = null;
            }

            if (upscaler == Settings.Upscaler.None)
                return Upscaler.Status.Success;

            InColorTarget =
                new RenderTexture(resolution.x, resolution.y, _plugin.ColorFormat(), Plugin.DepthFormat())
                {
                    filterMode = FilterMode.Point
                };
            InColorTarget.Create();

            var status = _plugin.SetDepth(InColorTarget);
            if (Upscaler.Failure(status))
                return status;
            status = _plugin.SetInputColor(InColorTarget);
            return status;
        }

        internal void Release()
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