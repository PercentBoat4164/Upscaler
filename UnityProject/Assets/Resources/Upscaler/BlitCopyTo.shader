Shader "Upscaler/BlitCopyTo"
{
    Properties
    {
        _ScaleFactor("Scale Factor", Vector) = (1, 1, 1, 1)
    }

    SubShader
    {
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
                // float4 color : SV_Target0;
                // float4 motion : SV_Target1;
                float depth : SV_Depth;
            };

            // sampler2D _CameraColorTexture;
            // sampler2D _CameraMotionVectorsTexture;
            sampler2D _CameraDepthTexture;
            float4 _ScaleFactor;

            // Vertex
            v2f vert(appdata IN) {
                v2f OUT;
                OUT.position = UnityObjectToClipPos(IN.vertex);
                OUT.uv = IN.uv;
                return OUT;
            }

            // Fragment
            FragOut frag(v2f IN) {
                FragOut o;
                // float2 NDC_UV = IN.uv * 2 - 1;
                // o.color = tex2D(_CameraColorTexture, IN.uv * _ScaleFactor.xy);
                // o.motion = tex2D(_CameraMotionVectorsTexture, IN.uv);
                o.depth = tex2D(_CameraDepthTexture, IN.uv * _ScaleFactor.xy);
                // o.color = float4(IN.uv, 0.0, 0.0);
                return o;
            }

            ENDCG
        }
    }
}