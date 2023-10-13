using System;
using System.Runtime.InteropServices;
using UnityEngine;

public class Jitter {
    private const uint SamplesPerPixel = 8;
    private Vector2 _upscalingFactor;
    private uint SequenceLength => (uint)Math.Ceiling(SamplesPerPixel * _upscalingFactor.x * _upscalingFactor.y);
    private Vector2 _renderingResolution;
    private Vector2[] _jitterSequence;
    private uint _sequencePosition;

    public void Apply(Camera camera)
    {
        var pixelSpaceJitter = _jitterSequence[_sequencePosition++];
        _sequencePosition %= SequenceLength;
        // Clip space jitter must be the negative of the pixel space jitter. Why?
        var clipSpaceJitter = -pixelSpaceJitter / _renderingResolution * 2;
        camera.ResetProjectionMatrix();
        var tempProj = camera.projectionMatrix;
        tempProj.m02 += clipSpaceJitter.x;
        tempProj.m12 += clipSpaceJitter.y;
        camera.projectionMatrix = tempProj;
        // The sign of the jitter passed to DLSS must match the sign of the MVScale.
        Upscaler_SetJitterInformation(pixelSpaceJitter.x, pixelSpaceJitter.y);
    }

    private bool ShouldRegenerate(Vector2 renderingResolution, Vector2 upscalingFactor)
    {
        return renderingResolution != _renderingResolution | upscalingFactor != _upscalingFactor |
               _jitterSequence == null;
    }

    public void Generate(Vector2 renderingResolution, Vector2 upscalingFactor)
    {
        if (!ShouldRegenerate(renderingResolution, upscalingFactor)) return;
        _renderingResolution = renderingResolution;
        _upscalingFactor = upscalingFactor;
        _jitterSequence = new Vector2[SequenceLength];
        for (var i = 0; i < 2; i++)
        {
            var seqBase = i + 2; // Bases 2 and 3 for x and y
            var n = 0;
            var d = 1;

            for (var index = 0; index < _jitterSequence.Length; index++)
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

                _jitterSequence[index][i] = (float)n / d - 0.5f;
            }
        }

        _sequencePosition = 0;
    }

    [DllImport("GfxPluginDLSSPlugin")]
    private static extern void Upscaler_SetJitterInformation(float x, float y);
}