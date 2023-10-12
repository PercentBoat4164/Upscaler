Shader "Upscaler/BlitToMotionTexture"
{
    Properties
    {
        _ScaleFactor("Scale Factor", Vector) = (1, 1, 1, 1)
    }

    SubShader
    {
        Cull Off ZTest Off

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

            struct frag_out {
                float2 mvs : SV_Target0;
            };

            sampler2D _CameraMotionVectorsTexture;
            float4 _ScaleFactor;

            // Vertex
            v2f vert(const appdata IN) {
                v2f OUT;
                OUT.position = UnityObjectToClipPos(IN.vertex);
                OUT.uv = IN.uv;
                return OUT;
            }

            // Fragment
            frag_out frag(const v2f IN) {
                frag_out o;
                o.mvs = tex2D(_CameraMotionVectorsTexture, IN.uv * _ScaleFactor.xy);
                return o;
            }

            ENDCG
        }
    }
}