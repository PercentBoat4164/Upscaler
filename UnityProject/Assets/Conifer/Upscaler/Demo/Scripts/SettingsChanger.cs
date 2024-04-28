using System;
using UnityEngine;

namespace Conifer.Upscaler.Demo
{
    public class SettingsChanger : MonoBehaviour
    {
        private Upscaler _upscaler;
        private bool _shouldChangeDynamicResolution;
        private float _scale;

        public void OnEnable()
        {
            _upscaler = Camera.main!.GetComponent<Upscaler>();
            _upscaler.ErrorCallback = (_, s) => Debug.Log(s);
        }

        public void Update()
        {
            if (!_upscaler) return;

            var settings = _upscaler.QuerySettings();
            if (Input.GetKeyDown(KeyCode.U))
                settings.upscaler = settings.upscaler == Settings.Upscaler.None
                    ? Settings.Upscaler.DeepLearningSuperSampling
                    : Settings.Upscaler.None;

            if (Input.GetKeyDown(KeyCode.Q))
                settings.quality = (Settings.Quality)(((int)settings.quality + 1) % 5);

            if (Input.GetKeyDown(KeyCode.E))
                settings.upscaler = (Settings.Upscaler)6;

            _shouldChangeDynamicResolution ^= Input.GetKeyDown(KeyCode.B);
            if (_shouldChangeDynamicResolution)
            {
                _scale = (float)(Math.Sin(Time.time) + 1) / 2 * (_upscaler.MaxRenderScale - _upscaler.MinRenderScale) + _upscaler.MinRenderScale;
                ScalableBufferManager.ResizeBuffers(_scale, _scale);
            }

            // Does nothing if settings have not changed.
            _upscaler.ApplySettings(settings);  // Be sure to handle errors here or have an error handler registered.
        }
    }
}