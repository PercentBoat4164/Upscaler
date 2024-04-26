using System;
using Conifer.Upscaler;
using UnityEngine;

namespace Conifer.Upscaler.Demo
{
    public class SettingsChanger : MonoBehaviour
    {
        private Upscaler _upscaler;
        private bool _shouldChangeDynamicResolution;
        private float scale;

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
                settings.upscaler = settings.upscaler == Settings.Upscaler.None
                    ? Settings.Upscaler.DLSS
                    : Settings.Upscaler.None;

            if (Input.GetKeyDown(KeyCode.Q))
                settings.quality = (Settings.Quality)(((int)settings.quality + 1) % 5);

            if (Input.GetKeyDown(KeyCode.E))
                settings.upscaler = (Settings.Upscaler)6;

            _shouldChangeDynamicResolution ^= Input.GetKeyDown(KeyCode.B);
            if (_shouldChangeDynamicResolution)
            {
                scale = (float)(Math.Sin(Time.time) + 1) / 2 * (_upscaler.MaxRenderScale - _upscaler.MinRenderScale) + _upscaler.MinRenderScale;
                ScalableBufferManager.ResizeBuffers(scale, scale);
            }

            // Does nothing if settings have not changed.
            _upscaler.ApplySettings(settings);  // Be sure to handle errors here or have an error handler registered.
        }

        private void OnGUI()
        {
            GUI.Label(new Rect(0,0,100,100), scale.ToString());
        }
    }
}