Shader "Hidden/Upscaler/BlitDepth" {
	Properties { _MainTex ("Texture", 2D) = "" {} }
    SubShader {
		Cull Off ZWrite On ZTest Always
        Pass
		{
			name "Upscaler | Blit Depth"
			CGPROGRAM
			#include "UnityCG.cginc"

			#pragma vertex vert_img
			#pragma fragment Frag

			SamplerState     pointClampSampler : register(s0);
			Texture2D<float> _MainTex;
			float4           _BlitScaleBias;

			float Frag(v2f_img input) : SV_Depth
			{
				return _MainTex.SampleLevel(pointClampSampler, input.uv * _BlitScaleBias.xy + _BlitScaleBias.zw, 0);
			}
			ENDCG
        }
    }
}