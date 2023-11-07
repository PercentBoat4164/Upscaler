//Shader "Upscaler/BlitToMotionTexture"
//{
//    SubShader
//    {
//        ZTest Always ZWrite Off Cull Off
//        Pass
//        {
//            Name "Copy Motion Vectors"
//            HLSLPROGRAM
//                // #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
//                // #include "Packages/com.unity.render-pipelines.core/Runtime/Utilities/Blit.hlsl"
//
//                TEXTURE2D_X(_MotionVectorTexture);
//
//                struct v2f {
//                    float4 pos : SV_POSITION;
//                    float2 uv : TEXCOORD0;
//                };
//
//                #pragma vertex vert
//                #pragma fragment frag
//
//                v2f vert(const Attributes input) {
//                    v2f output;
//
//                    output.pos  = GetFullScreenTriangleVertexPosition(input.vertexID);
//                    output.uv.xy = GetFullScreenTriangleTexCoord(input.vertexID);
//
//                    return output;
//                }
//
//                float2 frag(const v2f input) : SV_Target {
//                    return SAMPLE_TEXTURE2D_X(_MotionVectorTexture, sampler_LinearClamp, input.uv).xy;
//                }
//
//            ENDHLSL
//        }
//    }
//}

Shader "Upscaler/BlitToMotionTexture"
{
    SubShader
    {
        ZTest Always ZWrite Off Cull Off
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
                    output.uv = (input.vertex.xy + 1) * 0.5;
                    return output;
                }

                float2 frag(const v2f input) : SV_Target {
                    return tex2D(_MotionVectorTexture, input.uv).xy;
                }
            ENDHLSL
        }
    }
}