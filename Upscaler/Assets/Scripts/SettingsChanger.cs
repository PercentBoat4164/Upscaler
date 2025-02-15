using Conifer.Upscaler;
using UnityEngine;

namespace Demo
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
                else if (Input.GetKeyDown(KeyCode.Alpha2)) _upscaler.technique = Upscaler.Technique.DeepLearningSuperSampling;
                else if (Input.GetKeyDown(KeyCode.Alpha3)) _upscaler.technique = Upscaler.Technique.FidelityFXSuperResolution;
                else if (Input.GetKeyDown(KeyCode.Alpha4)) _upscaler.technique = Upscaler.Technique.XeSuperSampling;
                else if (Input.GetKeyDown(KeyCode.Alpha5)) _upscaler.technique = Upscaler.Technique.SnapdragonGameSuperResolution1;
                else if (Input.GetKeyDown(KeyCode.Alpha6)) _upscaler.technique = Upscaler.Technique.SnapdragonGameSuperResolution2;
            }
            else if (Input.GetKey(KeyCode.Q))
            {
                if (Input.GetKeyDown(KeyCode.Alpha1)) _upscaler.quality = Upscaler.Quality.Auto;
                else if (Input.GetKeyDown(KeyCode.Alpha2)) _upscaler.quality = Upscaler.Quality.AntiAliasing;
                else if (Input.GetKeyDown(KeyCode.Alpha3)) _upscaler.quality = Upscaler.Quality.UltraQualityPlus;
                else if (Input.GetKeyDown(KeyCode.Alpha4)) _upscaler.quality = Upscaler.Quality.UltraQuality;
                else if (Input.GetKeyDown(KeyCode.Alpha5)) _upscaler.quality = Upscaler.Quality.Quality;
                else if (Input.GetKeyDown(KeyCode.Alpha6)) _upscaler.quality = Upscaler.Quality.Balanced;
                else if (Input.GetKeyDown(KeyCode.Alpha7)) _upscaler.quality = Upscaler.Quality.Performance;
                else if (Input.GetKeyDown(KeyCode.Alpha8)) _upscaler.quality = Upscaler.Quality.UltraPerformance;
            }
            else if (Input.GetKey(KeyCode.F))
            {
                if (Input.GetKeyDown(KeyCode.Alpha1)) _upscaler.frameGeneration = false;
                else if (Input.GetKeyDown(KeyCode.Alpha2)) _upscaler.frameGeneration = true;
            }
        }
    }
}