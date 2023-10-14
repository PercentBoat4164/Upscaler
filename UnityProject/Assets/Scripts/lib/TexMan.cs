using UnityEngine;
using UnityEngine.Rendering;

public static class TexMan
{
    private static Mesh _quad;
    private static Material _blitToDepthTextureMaterial;
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
        _blitToDepthTextureMaterial = new Material(Shader.Find("Upscaler/BlitToDepthTexture"));
        _blitToCameraDepthMaterial = new Material(Shader.Find("Upscaler/BlitToCameraDepth"));
    }

    public static void BlitToDepthTexture(CommandBuffer cb, RenderTexture dest, Vector2? scale = null)
    {
        // Set up material
        _blitToDepthTextureMaterial.SetVector(Shader.PropertyToID("_ScaleFactor"), scale ?? Vector2.one);

        // Record command buffer
        cb.SetProjectionMatrix(Matrix4x4.Ortho(-1, 1, -1, 1, 1, -1));
        cb.SetViewMatrix(Matrix4x4.LookAt(new Vector3(0, 0, -1), new Vector3(0, 0, 1), Vector3.up));
        cb.SetRenderTarget(dest.depthBuffer);
        cb.DrawMesh(_quad, Matrix4x4.identity, _blitToDepthTextureMaterial);
    }

    public static void BlitToCameraDepth(CommandBuffer cb, RenderTexture src, Vector2? scale = null)
    {
        // Set up material
        _blitToCameraDepthMaterial.SetVector(Shader.PropertyToID("_ScaleFactor"), scale ?? Vector2.one);
        _blitToCameraDepthMaterial.SetTexture(Shader.PropertyToID("_Depth"), src, RenderTextureSubElement.Depth);

        // Record command buffer
        cb.SetProjectionMatrix(Matrix4x4.Ortho(-1, 1, -1, 1, 1, -1));
        cb.SetViewMatrix(Matrix4x4.LookAt(new Vector3(0, 0, -1), new Vector3(0, 0, 1), new Vector3(0, 1, 0)));
        cb.SetRenderTarget(scale != null ? src.depthBuffer : BuiltinRenderTextureType.CameraTarget);
        cb.DrawMesh(_quad, Matrix4x4.identity, _blitToCameraDepthMaterial);
    }
}