Shader "Upscaler/BlitToCameraDepth"
{
    Properties
    {
        [HideInInspector] _Depth("Depth Source", 2D) = "white"
    }

    SubShader
    {
        Cull Off ZTest Off

        Pass
        {
            CGPROGRAM

            #pragma vertex vert
            #pragma fragment frag

            sampler2D _Depth;

            float2 vert(float2 uv : TEXCOORD0) : TEXCOORD0 {
                return uv;
            }

            float frag(const float2 uv : TEXCOORD0) : SV_Depth {
                return tex2D(_Depth, uv).x;
            }

            ENDCG
        }
    }
}