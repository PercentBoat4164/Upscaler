Shader "Conifer/Upscaler/Snapdragon Game Super Resolution/v1/Upscale"
{
    SubShader
    {
        Cull Off ZWrite Off ZTest Always

        Pass
        {
            HLSLPROGRAM
            #pragma multi_compile _ CONIFER_UPSCALER_USE_EDGE_DIRECTION
            #pragma target 5.0
			#pragma vertex vert
			#pragma fragment frag

            #include "UnityCG.cginc"

			struct VertexShaderOutput
			{
				float2 uv : TEXCOORD0;
				float4 pos : POSITION;
			};

			VertexShaderOutput vert(float4 pos : POSITION, float2 uv : TEXCOORD)
			{
				VertexShaderOutput o;
				o.pos = UnityObjectToClipPos(pos);
				o.uv = uv;
				return o;
			}

            SamplerState linearClampSampler : register(s0);
			Texture2D    _MainTex : register(t0);
            float        Conifer_Upscaler_UseEdgeDirection;
            float        Conifer_Upscaler_EdgeSharpness;
            float4       Conifer_Upscaler_ViewportInfo : register(b0);

#if defined(CONIFER_UPSCALER_USE_EDGE_DIRECTION)
			half2 weightY(half dx, half dy, half c, half3 data) {
				half  std     = data.x;
				half2 dir     = data.yz;
				half  edgeDis = dx * -dir.y + dy * dir.x;
				half  x       = dx * dx + dy * dy + edgeDis * edgeDis * (clamp(c * c * std, 0.0, 1.0) * 0.7 + -1.0);
#else
            half2 weightY(half dx, half dy, half c, half data) {
				half std = data;
				half x = (dx * dx + dy * dy) * half(0.5) + clamp(abs(c) * std, 0.0, 1.0);
#endif
				half wA = x - half(4.0);
				half wB = x * wA - wA;
				wA *= wA;
				half w = wB * wA;
				return half2(w, w * c);
			}

			half4 frag(float4 _ : POSITION, float2 uv : TEXCOORD) : SV_TARGET {
				half4 pix = half4(0, 0, 0, 1);
				pix.xyz = _MainTex.SampleLevel(linearClampSampler, uv, 0).xyz;

				float2 imgCoord = uv.xy * Conifer_Upscaler_ViewportInfo.zw + float2(-0.5, 0.5);
				float2 imgCoordPixel = floor(imgCoord);
				float2 coord = imgCoordPixel * Conifer_Upscaler_ViewportInfo.xy;
				half2 pl = imgCoord - imgCoordPixel;
				half4 left = _MainTex.GatherGreen(linearClampSampler, coord);

				if (abs(left.z - left.y) + abs(pix[1] - left.y) + abs(pix[1] - left.z) > 8.0 / 255.0) {
					coord.x += Conifer_Upscaler_ViewportInfo.x;

					half4 right = _MainTex.GatherGreen(linearClampSampler, coord + float2(Conifer_Upscaler_ViewportInfo.x,  0.0));
					half4 upDown = half4(
						_MainTex.GatherGreen(linearClampSampler, coord + float2(0.0, -Conifer_Upscaler_ViewportInfo.y)).wz,
						_MainTex.GatherGreen(linearClampSampler, coord + float2(0.0,  Conifer_Upscaler_ViewportInfo.y)).yx
					);

					half mean = (left.y + left.z + right.x + right.w) * half(0.25);
					left = left - half4(mean, mean, mean, mean);
					right = right - half4(mean, mean, mean, mean);
					upDown = upDown - half4(mean, mean, mean, mean);
					pix.w = pix.y - mean;

					half std = half(2.181818) / (abs(left.x) + abs(left.y) + abs(left.z) + abs(left.w) +
						       					 abs(right.x) + abs(right.y) + abs(right.z) + abs(right.w) +
						       					 abs(upDown.x) + abs(upDown.y) + abs(upDown.z) + abs(upDown.w));
#if defined(CONIFER_UPSCALER_USE_EDGE_DIRECTION)
					half  RxLz = right.x + -left.z;
					half  RwLy = right.w + -left.y;
					half2 delta = half2(RxLz + RwLy, RxLz + -RwLy);
					half lengthInv = rsqrt(delta.x * delta.x + 3.075740e-05 + delta.y * delta.y);
					half2 dir = half2(delta.x * lengthInv, delta.y * lengthInv);
					half3 data = half3(std, dir);
#else
					half data = std;
#endif

					half2 aWY = weightY(pl.x,       pl.y + 1.0, upDown.x, data) +
					            weightY(pl.x - 1.0, pl.y + 1.0, upDown.y, data) +
					            weightY(pl.x - 1.0, pl.y - 2.0, upDown.z, data) +
					            weightY(pl.x,       pl.y - 2.0, upDown.w, data) +
					            weightY(pl.x + 1.0, pl.y - 1.0, left.x,   data) +
					            weightY(pl.x,       pl.y - 1.0, left.y,   data) +
					            weightY(pl.x,       pl.y,       left.z,   data) +
					            weightY(pl.x + 1.0, pl.y,       left.w,   data) +
					            weightY(pl.x - 1.0, pl.y - 1.0, right.x,  data) +
					            weightY(pl.x - 2.0, pl.y - 1.0, right.y,  data) +
					            weightY(pl.x - 2.0, pl.y,       right.z,  data) +
					            weightY(pl.x - 1.0, pl.y,       right.w,  data);

					half finalY = aWY.y / aWY.x;

					half max4 = max(max(left.y, left.z), max(right.x, right.w));
					half min4 = min(min(left.y, left.z), min(right.x, right.w));
					finalY = clamp(Conifer_Upscaler_EdgeSharpness * finalY, min4, max4);

					half deltaY = finalY - pix.w;

					pix.x = saturate(pix.x + deltaY);
					pix.y = saturate(pix.y + deltaY);
					pix.z = saturate(pix.z + deltaY);
				}
				pix.w = 1.0;
				return pix;
			}
			ENDHLSL
        }
    }
}