using UnityEngine;
using Upscaler.impl;

public class SettingsChanger : MonoBehaviour
{
    private Upscaler.Upscaler _upscaler;
    
    public void OnEnable() {
        _upscaler = Camera.main!.GetComponent<Upscaler.Upscaler>();
    }

    public void Update()
    {
        if (!_upscaler) return;
        if (Input.GetKeyDown(KeyCode.U))
        {
            _upscaler.upscalerMode = _upscaler.upscalerMode == Plugin.UpscalerMode.None
                ? Plugin.UpscalerMode.DLSS
                : Plugin.UpscalerMode.None;
        }

        if (Input.GetKeyDown(KeyCode.Q))
        {
            _upscaler.qualityMode = (Plugin.QualityMode)(((int)_upscaler.qualityMode + 1) % 5);
        }
    }
}