using UnityEngine;
using UnityEngine.Rendering;

public static class TexMan
{
    private static Mesh _quad;
    private static Material _blitToCameraDepthMaterial;
    private static Material _blitToMotionTextureMaterial;

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
        _quad.SetTriangles(new[] { 1, 0, 2, 1, 2, 3 }, 0);

        // Set up materials
        _blitToCameraDepthMaterial = new Material(Shader.Find("Upscaler/BlitToCameraDepth"));
        _blitToMotionTextureMaterial = new Material(Shader.Find("Upscaler/BlitToMotionTexture"));
    }

    public static void BlitToMotionTexture(CommandBuffer cb, RenderTexture dest)
    {
        // Record command buffer
        cb.SetProjectionMatrix(Matrix4x4.Ortho(-1, 1, -1, 1, 1, -1));
        cb.SetViewMatrix(Matrix4x4.LookAt(new Vector3(0, 0, -1), new Vector3(0, 0, 1), Vector3.up));
        cb.SetRenderTarget(dest.colorBuffer);
        cb.DrawMesh(_quad, Matrix4x4.identity, _blitToMotionTextureMaterial);
    }

    public static void BlitToCameraDepth(CommandBuffer cb, RenderTexture src)
    {
        // Set up material
        _blitToCameraDepthMaterial.SetTexture(Shader.PropertyToID("_Depth"), src, RenderTextureSubElement.Depth);

        // Execute Graphics commands
        cb.SetProjectionMatrix(Matrix4x4.Ortho(-1, 1, -1, 1, 1, -1));
        cb.SetViewMatrix(Matrix4x4.LookAt(new Vector3(0, 0, -1), new Vector3(0, 0, 1), Vector3.up));
        cb.DrawMesh(_quad, Matrix4x4.identity, _blitToCameraDepthMaterial);
    }
}