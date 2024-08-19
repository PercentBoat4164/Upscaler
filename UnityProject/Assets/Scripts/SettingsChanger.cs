using UnityEngine;

namespace Conifer.Upscaler.Demo.Demo.Scripts
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

            if (Input.GetKey(KeyCode.U))
            {
                if (Input.GetKeyDown(KeyCode.Alpha1)) _upscaler.technique = Upscaler.Technique.None;
                if (Input.GetKeyDown(KeyCode.Alpha2)) _upscaler.technique = Upscaler.Technique.DeepLearningSuperSampling;
                if (Input.GetKeyDown(KeyCode.Alpha3)) _upscaler.technique = Upscaler.Technique.FidelityFXSuperResolution;
                if (Input.GetKeyDown(KeyCode.Alpha4)) _upscaler.technique = Upscaler.Technique.XeSuperSampling;
            }
            else if (Input.GetKey(KeyCode.Q))
            {
                if (Input.GetKeyDown(KeyCode.Alpha1)) _upscaler.quality = Upscaler.Quality.Auto;
                if (Input.GetKeyDown(KeyCode.Alpha2)) _upscaler.quality = Upscaler.Quality.AntiAliasing;
                if (Input.GetKeyDown(KeyCode.Alpha3)) _upscaler.quality = Upscaler.Quality.UltraQualityPlus;
                if (Input.GetKeyDown(KeyCode.Alpha4)) _upscaler.quality = Upscaler.Quality.UltraQuality;
                if (Input.GetKeyDown(KeyCode.Alpha5)) _upscaler.quality = Upscaler.Quality.Quality;
                if (Input.GetKeyDown(KeyCode.Alpha6)) _upscaler.quality = Upscaler.Quality.Balanced;
                if (Input.GetKeyDown(KeyCode.Alpha7)) _upscaler.quality = Upscaler.Quality.Performance;
                if (Input.GetKeyDown(KeyCode.Alpha8)) _upscaler.quality = Upscaler.Quality.UltraPerformance;
            }

            // Does nothing if settings have not changed.
            _upscaler.ApplySettings();  // Optional, be sure to handle errors here.
        }
    }
}