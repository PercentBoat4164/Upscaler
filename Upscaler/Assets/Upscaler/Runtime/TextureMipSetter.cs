using UnityEngine;
using UnityEngine.Rendering;

namespace Upscaler.Runtime
{
    [RequireComponent(typeof(MeshRenderer))]
    [AddComponentMenu("Rendering/Texture Mip Setter")]
    [ExecuteAlways]
    public class TextureMipSetter : MonoBehaviour
    {
        private void OnRenderObject()
        {
            if (GraphicsSettings.currentRenderPipeline != null) return;

            var upscaler = Camera.main?.GetComponent<Upscaler>();
            if (upscaler == null) return;

            if (upscaler.PreviousMipBias.Equals(upscaler.MipBias) || upscaler.IsSpatial()) return;

            foreach (var material in GetComponent<MeshRenderer>().materials)
            {
                foreach (var textureID in material.GetTexturePropertyNameIDs())
                {
                    var texture = material.GetTexture(textureID);
                    if (texture == null) continue;
                    texture.mipMapBias = upscaler.MipBias;
                }
            }
        }
    }
}