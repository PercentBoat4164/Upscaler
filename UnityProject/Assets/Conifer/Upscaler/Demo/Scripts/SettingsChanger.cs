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

            if (Input.GetKeyDown(KeyCode.U))
                _upscaler.technique = _upscaler.technique == Upscaler.Technique.None
                    ? Upscaler.Technique.DeepLearningSuperSampling
                    : Upscaler.Technique.None;

            if (Input.GetKeyDown(KeyCode.Q))
                _upscaler.quality = (Upscaler.Quality)(((int)_upscaler.quality + 1) % 5);

            if (Input.GetKeyDown(KeyCode.E))
                _upscaler.quality = (Upscaler.Quality)11;

            // Does nothing if settings have not changed.
            _upscaler.ApplySettings();  // Be sure to handle errors here or have an error handler registered.
        }
    }
}