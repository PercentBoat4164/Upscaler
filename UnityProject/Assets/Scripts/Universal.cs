    using System;
    using System.Reflection;
    using UnityEngine;
    using UnityEngine.Rendering;
    using UnityEngine.Rendering.Universal;

    public class Universal : RenderPipeline
    {
        private readonly UpscalerRendererFeature _upscalerRendererFeature;

        public Universal(Camera camera, Action onPreCull) : base(camera, PipelineType.Universal)
        {
            // Set up URP
            _upscalerRendererFeature = ScriptableObject.CreateInstance<UpscalerRendererFeature>();
            _upscalerRendererFeature.camera = camera;
            _upscalerRendererFeature.SetActive(true);
            // Use some reflection to grab the properties that hold and describe where the rendererFeatures are.
            var index = (int)typeof(UniversalRenderPipelineAsset)
                .GetField("m_DefaultRendererIndex", BindingFlags.NonPublic | BindingFlags.Instance)!
                .GetValue(UniversalRenderPipeline.asset);
            var rendererDataList = (ScriptableRendererData[])typeof(UniversalRenderPipelineAsset)
                .GetField("m_RendererDataList", BindingFlags.NonPublic | BindingFlags.Instance)!
                .GetValue(UniversalRenderPipeline.asset);
            // Add our newly created rendererFeature.
            rendererDataList[index].rendererFeatures.Add(_upscalerRendererFeature);
            // Add target management, and jittering to render pipeline events
            RenderPipelineManager.beginCameraRendering += (_, _) => onPreCull();
        }

        public override bool ManageOutputTarget(Plugin.Mode mode, Vector2Int upscalingResolution) =>
            _upscalerRendererFeature.ManageOutputTarget(mode, upscalingResolution);

        public override bool ManageMotionVectorTarget(Plugin.Mode mode, Vector2Int upscalingResolution) =>
            _upscalerRendererFeature.ManageMotionVectorTarget(mode, upscalingResolution);

        public override bool ManageInColorTarget(Plugin.Mode mode, Vector2Int maximumDynamicRenderingResolution) =>
            _upscalerRendererFeature.ManageInColorTarget(mode, maximumDynamicRenderingResolution);

        public override void Shutdown() => _upscalerRendererFeature.Shutdown();
    }