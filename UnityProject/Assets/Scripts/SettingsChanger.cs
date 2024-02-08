using UnityEngine;

public class SettingsChanger : MonoBehaviour
{
    private Upscaler.Upscaler _upscaler;

    public void OnEnable()
    {
        _upscaler = Camera.main!.GetComponent<Upscaler.Upscaler>();
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
            _upscaler.upscalerMode = _upscaler.upscalerMode == Upscaler.Upscaler.UpscalerMode.None
                ? Upscaler.Upscaler.UpscalerMode.DLSS
                : Upscaler.Upscaler.UpscalerMode.None;
        }

        if (Input.GetKeyDown(KeyCode.Q))
        {
            _upscaler.qualityMode = (Upscaler.Upscaler.QualityMode)(((int)_upscaler.qualityMode + 1) % 5);
        }

        if (Input.GetKeyDown(KeyCode.E))
        {
            _upscaler.upscalerMode = (Upscaler.Upscaler.UpscalerMode)6;
        }
    }
}