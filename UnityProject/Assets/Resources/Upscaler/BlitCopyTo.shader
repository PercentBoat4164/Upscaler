Shader "Upscaler/BlitCopyTo"
{
    Properties
    {
        _ScaleFactor("Scale Factor", Vector) = (1, 1, 1, 1)
    }

    SubShader
    {
        Cull Off ZTest Never

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
                float4 position : SV_POSITION;
                float2 uv : TEXCOORD0;
            };

            struct FragOut {
                float depth : SV_Depth;
            };

            sampler2D _CameraDepthTexture;
            float4 _ScaleFactor;

            // Vertex
            v2f vert(const appdata IN) {
                v2f OUT;
                OUT.position = UnityObjectToClipPos(IN.vertex);
                OUT.uv = IN.uv;
                return OUT;
            }

            // Fragment
            FragOut frag(const v2f IN) {
                FragOut o;
                o.depth = tex2D(_CameraDepthTexture, IN.uv * _ScaleFactor.xy);
                return o;
            }

            ENDCG
        }
    }
}