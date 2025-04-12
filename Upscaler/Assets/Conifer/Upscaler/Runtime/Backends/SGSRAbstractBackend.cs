/**************************************************
 * Upscaler v2.0.1                                *
 * See the UserManual.pdf for more information    *
 **************************************************/

using System;
using UnityEngine;

namespace Conifer.Upscaler
{
    public abstract class SGSRAbstractBackend : UpscalerBackend
    {
        public override Upscaler.Status ApplySettings(in Upscaler upscaler)
        {
            var outputResolution = upscaler.OutputResolution;
            double scale;
            switch (upscaler.quality)
            {
                case Upscaler.Quality.Auto:
                    scale = ((uint)outputResolution.x * (uint)outputResolution.y) switch
                    {
                        <= 2560 * 1440 => 0.769,
                        <= 3840 * 2160 => 0.667,
                        _ => 0.5
                    }; break;
                case Upscaler.Quality.AntiAliasing: return Upscaler.Status.RecoverableRuntimeError;
                case Upscaler.Quality.UltraQualityPlus: scale = 0.769; break;
                case Upscaler.Quality.UltraQuality: scale = 0.667; break;
                case Upscaler.Quality.Quality: scale = 0.588; break;
                case Upscaler.Quality.Balanced: scale = 0.5; break;
                case Upscaler.Quality.Performance: scale = 0.435; break;
                case Upscaler.Quality.UltraPerformance: scale = 0.333; break;
                default: return Upscaler.Status.RecoverableRuntimeError;
            }
            upscaler.RecommendedInputResolution = new Vector2Int((int)Math.Ceiling(outputResolution.x * scale), (int)Math.Ceiling(outputResolution.y * scale));
            upscaler.MaxInputResolution = outputResolution;
            upscaler.MinInputResolution = Vector2Int.one;

            return Upscaler.Status.Success;
        }
    }
}