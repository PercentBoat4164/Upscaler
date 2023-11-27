#if UPSCALER_USE_URP
using System;
using System.Collections.Generic;
using System.Reflection;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

public class Universal : RenderPipeline
{
    private readonly UpscalerRendererFeature _upscalerRendererFeature;
    private readonly List<ScriptableRendererFeature> _features;
    private readonly int _featureIndex;
    private bool _initialized;

    public Universal(Camera camera, Action onPreCull) : base(camera, PipelineType.Universal)
    {
        // Set up URP
        _upscalerRendererFeature = ScriptableObject.CreateInstance<UpscalerRendererFeature>();
        _upscalerRendererFeature.camera = camera;
        _upscalerRendererFeature.SetActive(true);
        // Use some reflection to grab the properties that hold and describe where the rendererFeatures are.
        _features = ((ScriptableRendererData[])typeof(UniversalRenderPipelineAsset)
            .GetField("m_RendererDataList", BindingFlags.NonPublic | BindingFlags.Instance)!
            .GetValue(UniversalRenderPipeline.asset))[(int)typeof(UniversalRenderPipelineAsset)
            .GetField("m_DefaultRendererIndex", BindingFlags.NonPublic | BindingFlags.Instance)!
            .GetValue(UniversalRenderPipeline.asset)].rendererFeatures;
        _featureIndex = _features.Count;
        // Add our newly created rendererFeature.
        _features.Add(_upscalerRendererFeature);
        // Add target management, and jittering to render pipeline events
        RenderPipelineManager.beginCameraRendering += (_, _) =>
        {
            onPreCull();
            _upscalerRendererFeature.PreUpscale();
        };
        _initialized = true;
    }

    public override void UpdatePostUpscaleCommandBuffer() =>
        _upscalerRendererFeature.UpdatePostUpscaleCommandBuffer();

    public override bool ManageOutputTarget(Plugin.Mode mode, Vector2Int upscalingResolution) =>
        _upscalerRendererFeature.ManageOutputTarget(mode, upscalingResolution);

    public override bool ManageMotionVectorTarget(Plugin.Mode mode, Vector2Int upscalingResolution) =>
        _upscalerRendererFeature.ManageMotionVectorTarget(mode, upscalingResolution);

    public override bool ManageInColorTarget(Plugin.Mode mode, Vector2Int maximumDynamicRenderingResolution) =>
        _upscalerRendererFeature.ManageInColorTarget(mode, maximumDynamicRenderingResolution);

    public override void Shutdown()
    {
        if (!_initialized) return;

        _upscalerRendererFeature.Shutdown();
        _features.RemoveAt(_featureIndex);
        _initialized = false;
    }
}
#endif