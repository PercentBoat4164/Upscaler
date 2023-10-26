using UnityEngine;
using UnityEngine.Rendering;

public static class TexMan
{
    private static Mesh _quad;
    private static Material _blitToCameraDepthMaterial;

    public static void Setup()
    {
        // Set up quad for depth blits
        _quad = new Mesh();
        _quad.SetVertices(new Vector3[]
        {
            new(-1, -1, 0),
            new(1, -1, 0),
            new(-1, 1, 0),
            new(1, 1, 0)
        });
        _quad.SetUVs(0, new Vector2[]
        {
            new(0, 0),
            new(1, 0),
            new(0, 1),
            new(1, 1)
        });
        _quad.SetIndices(new[] { 2, 1, 0, 1, 2, 3 }, MeshTopology.Triangles, 0);

        // Set up materials
        _blitToCameraDepthMaterial = new Material(Shader.Find("Upscaler/BlitToCameraDepth"));
    }

    public static void BlitToCameraDepth(RenderTexture src, Vector2? scale = null)
    {
        // Set up material
        _blitToCameraDepthMaterial.SetVector(Shader.PropertyToID("_ScaleFactor"), scale ?? Vector2.one);
        _blitToCameraDepthMaterial.SetTexture(Shader.PropertyToID("_Depth"), src, RenderTextureSubElement.Depth);

        // Execute Graphics commands
        Graphics.RenderMesh(new RenderParams(_blitToCameraDepthMaterial), _quad, 0, Matrix4x4.identity);
    }
}