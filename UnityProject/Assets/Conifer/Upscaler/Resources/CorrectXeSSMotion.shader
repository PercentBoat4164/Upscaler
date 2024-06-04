Shader "Hidden/Conifer/Upscaler/CorrectXeSSMotion"
{
    SubShader
    {
        Cull Off ZWrite Off ZTest Always

        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag

            #include "UnityCG.cginc"

            struct appdata {
                float4 vertex : POSITION;
                float2 uv : TEXCOORD0;
            };

            struct v2f {
                float2 uv : TEXCOORD0;
                float4 vertex : SV_POSITION;
            };

            v2f vert(const appdata v) {
                v2f o;
                o.vertex = UnityObjectToClipPos(v.vertex);
                o.uv     = v.uv;
                return o;
            }

            sampler2D _MotionVectorTexture;
            float4 UpscalerXeSSMotionScale;

            float2 frag(const v2f i) : SV_Target {
                return tex2D(_MotionVectorTexture, i.uv).rg * -UpscalerXeSSMotionScale.xy;
            }
            ENDCG
        }
    }
}