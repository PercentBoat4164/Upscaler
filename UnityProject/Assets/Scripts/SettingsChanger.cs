using Conifer.Upscaler.Scripts;
using UnityEngine;

public class SettingsChanger : MonoBehaviour
{
    private Upscaler _upscaler;

    public void OnEnable()
    {
        _upscaler = Camera.main!.GetComponent<Upscaler>();
        _upscaler.ErrorCallback = (status, s) => Debug.Log(s);
    }

    public void Update()
    {
        if (!_upscaler)
        {
            return;
        }

        if (Input.GetKeyDown(KeyCode.U))
        {
            _upscaler.upscalerMode = _upscaler.upscalerMode == Upscaler.UpscalerMode.FSR2
                ? Upscaler.UpscalerMode.DLSS
                : Upscaler.UpscalerMode.FSR2;
        }

        if (Input.GetKeyDown(KeyCode.Q))
        {
            _upscaler.qualityMode = (Upscaler.QualityMode)(((int)_upscaler.qualityMode + 1) % 5);
        }

        if (Input.GetKeyDown(KeyCode.E))
        {
            _upscaler.upscalerMode = (Upscaler.UpscalerMode)6;
        }
    }
}