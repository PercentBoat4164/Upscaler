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
        if (!_upscaler) return;

        var settings = _upscaler.QuerySettings();
        if (Input.GetKeyDown(KeyCode.U))
            settings.FeatureSettings.upscaler = settings.FeatureSettings.upscaler == Settings.Upscaler.None
                ? Settings.Upscaler.DLSS
                : Settings.Upscaler.None;

        if (Input.GetKeyDown(KeyCode.Q))
            settings.FeatureSettings.quality = (Settings.Quality)(((int)settings.FeatureSettings.quality + 1) % 5);

        if (Input.GetKeyDown(KeyCode.E))
            settings.FeatureSettings.upscaler = (Settings.Upscaler)6;

        _upscaler.ApplySettings(settings);  // Be sure to handle errors here or have an error handler registered.
    }
}