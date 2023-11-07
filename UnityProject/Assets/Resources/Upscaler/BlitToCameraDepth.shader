Shader "Upscaler/BlitToCameraDepth"
{
    SubShader
    {
        ZTest Off Cull Off
        Pass
        {
            Name "Copy Depth Buffer"
            HLSLPROGRAM
                sampler2D _CameraDepthTexture;

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
                    output.uv = (input.vertex.xy + 1) * 0.5;
                    return output;
                }

                float frag(const v2f input) : SV_Depth {
                    return tex2D(_CameraDepthTexture, input.uv);
                }
            ENDHLSL
        }
    }
}