using System;
using UnityEngine;

/**
 * A static library class that eases jittering the camera and passing the values used to the active upscaler. This class
 * is not intended for use by end users. The Upscaler plugin handles performing all camera jittering for you.
 */
public static class Jitter {
    /// The recommended number of samples per pixel as given by the DLSS documentation
    private const uint SamplesPerPixel = 8;
    /// The amount of upscaling to be applied (&lt;= 1) on each axis. Used to determine if the sequence is out-of-date.
    private static Vector2 _lastUpscalingFactor;
    /// The array used to store all stages of the jitter sequence.
    private static Vector2[] _sequence;
    /// The index of the current jitter stage.
    private static long _sequencePosition;

    /**
     *  Handles applying jitter to the camera and the upscaler.
     *  Apply selects the next jitter stage, then resets the camera's projection matrix, then shifts the camera by the
     * jitter amount on its x and y axes in clip space. The pixel space equivalent of that transformation is then sent
     * to the internal upscaler.
     * <param name="camera">The Camera to apply the jitter to.</param>
     * <param name="renderingResolution">The resolution that Unity renders at.</param>
     */
    public static void Apply(Camera camera, Vector2 renderingResolution)
    {
        if (_sequence.Length == 0) return;
        _sequencePosition %= _sequence.Length;
        var pixelSpaceJitter = _sequence[_sequencePosition++];
        var clipSpaceJitter = pixelSpaceJitter / renderingResolution * 2;
        /*@todo Change this so that the camera is not reset just before rendering every frame. No reason to do this if we can just subtract the last frame's jitters.*/
        camera.ResetProjectionMatrix();
        var tempProj = camera.projectionMatrix;
        tempProj.m02 += clipSpaceJitter.x;
        tempProj.m12 += clipSpaceJitter.y;
        camera.projectionMatrix = tempProj;
        Plugin.SetJitterInformation(-pixelSpaceJitter.x, -pixelSpaceJitter.y);
    }

    /**
     *  Used to determine if the current pre-computed jitter sequence is out-of-date.
     * <param name="upscalingFactor">The amount of upscaling to be applied (&lt;= 1) on each axis.</param>
     * <returns>`true` if the jitter sequence is out-of-date, `false` otherwise.</returns>
     */
    private static bool ShouldRegenerate(Vector2 upscalingFactor)
    {
        return upscalingFactor != _lastUpscalingFactor | _sequence == null;
    }

    /**
     *  Generates the required jitter sequence for the given rendering resolution and scaling factor. The generation
     * only happens if it is determined that the current sequence is out-of-date.
     * <param name="upscalingFactor">The amount of upscaling to be applied (&lt;= 1) on each axis.</param>
     */
    public static void Generate(Vector2 upscalingFactor)
    {
        // Abort early if the jitter sequence is not out-of-date
        if (!ShouldRegenerate(upscalingFactor)) return;
        _lastUpscalingFactor = upscalingFactor;
        /*@todo This could be sped up by not regenerating pre-existing samples and only generating the new ones.*/
        _sequence = new Vector2[(uint)Math.Ceiling(SamplesPerPixel * upscalingFactor.x * upscalingFactor.y)];
        for (var i = 0; i < 2; i++)
        {
            var seqBase = i + 2; // Bases 2 and 3 for x and y
            var n = 0;
            var d = 1;

            for (var index = 0; index < _sequence.Length; index++)
            {
                var x = d - n;
                if (x == 1)
                {
                    n = 1;
                    d *= seqBase;
                }
                else
                {
                    var y = d / seqBase;
                    while (x <= y) y /= seqBase;

                    n = (seqBase + 1) * y - x;
                }

                _sequence[index][i] = (float)n / d - 0.5f;
            }
        }
        _sequencePosition = 0;
    }
}