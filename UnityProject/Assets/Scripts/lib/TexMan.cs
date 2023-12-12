using UnityEngine;
using UnityEngine.Rendering;

public static class TexMan
{
    private static Mesh _quad;
    private static Material _copyCameraDepthMaterial;
    private static Material _blitToMotionTextureMaterial;
    private static Material _blitToCameraDepthMaterial;
    private static readonly int Depth = Shader.PropertyToID("_Depth");

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
        _copyCameraDepthMaterial = new Material(Shader.Find("Upscaler/CopyCameraDepth"));
        _blitToMotionTextureMaterial = new Material(Shader.Find("Upscaler/BlitToMotionTexture"));
        _blitToCameraDepthMaterial = new Material(Shader.Find("Upscaler/BlitToCameraDepth"));
    }

    public static void BlitToMotionTexture(CommandBuffer cb, RenderTexture dest)
    {
        // Record command buffer
        cb.SetProjectionMatrix(Matrix4x4.Ortho(-1, 1, -1, 1, 1, -1));
        cb.SetViewMatrix(Matrix4x4.LookAt(Vector3.back, Vector3.forward, Vector3.up));
        cb.SetRenderTarget(dest.colorBuffer);
        cb.DrawMesh(_quad, Matrix4x4.identity, _blitToMotionTextureMaterial);
    }

    /// <summary>
    /// Blits `_CameraDepthTexture` into the currently active RenderTexture's depth buffer.
    /// </summary>
    /// <param name="cb">Command buffer to record the blit into.</param>
    public static void CopyCameraDepth(CommandBuffer cb)
    {
        // Record command buffer
        cb.SetProjectionMatrix(Matrix4x4.Ortho(-1, 1, -1, 1, 1, -1));
        cb.SetViewMatrix(Matrix4x4.LookAt(Vector3.back, Vector3.forward, Vector3.up));
        cb.DrawMesh(_quad, Matrix4x4.identity, _copyCameraDepthMaterial);
    }

    public static void BlitToCameraDepth(CommandBuffer cb, RenderTexture src)
    {
        _blitToCameraDepthMaterial.SetTexture(Depth, src, RenderTextureSubElement.Depth);

        // Record command buffer
        cb.SetProjectionMatrix(Matrix4x4.Ortho(-1, 1, -1, 1, 1, -1));
        cb.SetViewMatrix(Matrix4x4.LookAt(Vector3.back, Vector3.forward, Vector3.up));
        cb.DrawMesh(_quad, Matrix4x4.identity, _blitToCameraDepthMaterial);
    }
}