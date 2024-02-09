Shader "Upscaler/BlitToMotionTexture"
{
    SubShader
    {
        ZTest Off ZWrite Off Cull Off
        Pass
        {
            Name "Copy Motion Vectors"
            HLSLPROGRAM
                sampler2D _MotionVectorTexture;

                struct Attr {
                    float3 vertex : POSITION;
                };

                struct v2f {
                    float4 pos : SV_POSITION;
                    float2 uv : TEXCOORD0;
                };

                #pragma vertex vert
                #pragma fragment frag

                v2f vert(Attr input) {
                    v2f output;
                    output.pos = float4(input.vertex.xy, 0.0, 1.0);
                    output.uv = (input.vertex.xy + 1) * 0.5 * float2(1.0, -1.0) + float2(0.0, 1.0);
                    return output;
                }

                float2 frag(const v2f input) : SV_Target {
                    return tex2D(_MotionVectorTexture, input.uv).xy;
                }
            ENDHLSL
        }
    }
}