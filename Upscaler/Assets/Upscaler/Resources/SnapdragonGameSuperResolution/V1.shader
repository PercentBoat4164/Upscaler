//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

Shader "Upscaler/Snapdragon Game Super Resolution/V1"
{
	Properties { _MainTex ("Texture", 2D) = "" {} }
    SubShader
    {
        Cull Off ZWrite Off ZTest Always
        Pass
        {
        	Name "Upscaler | Snapdragon Game Super Resolution 1 - Upscale"
            CGPROGRAM
            #pragma multi_compile_local_fragment _ _UPSCALER__USE_EDGE_DIRECTION
            #pragma target 5.0

            #include "UnityCG.cginc"

			#pragma vertex vert_img
			#pragma fragment frag

            SamplerState linearClampSampler : register(s0);
            Texture2D    _MainTex : register(t0);
            float4       Upscaler_ViewportInfo;
            float2       Upscaler_InputScale;
            float        Upscaler_Sharpness;

#if defined(_UPSCALER__USE_EDGE_DIRECTION)
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

			half4 frag(v2f_img input) : SV_TARGET {
				UNITY_SETUP_STEREO_EYE_INDEX_POST_VERTEX(input);
				half4 pix = half4(0, 0, 0, 1);
				input.uv = input.uv * Upscaler_InputScale.xy;
				pix.xyz = _MainTex.Sample(linearClampSampler, input.uv).xyz;

				float2 imgCoord = input.uv * Upscaler_ViewportInfo.zw + float2(-0.5, 0.5);
				float2 imgCoordPixel = floor(imgCoord);
				float2 coord = imgCoordPixel * Upscaler_ViewportInfo.xy;
				half2 pl = imgCoord - imgCoordPixel;
				half4 left = _MainTex.GatherGreen(linearClampSampler, coord);

				if (abs(left.z - left.y) + abs(pix[1] - left.y) + abs(pix[1] - left.z) > 8.0 / 255.0) {
					coord.x += Upscaler_ViewportInfo.x;

					half4 right = _MainTex.GatherGreen(linearClampSampler, coord + float2(Upscaler_ViewportInfo.x, 0.0));
					half4 upDown = half4(
						_MainTex.GatherGreen(linearClampSampler, coord + float2(0.0, -Upscaler_ViewportInfo.y)).wz,
						_MainTex.GatherGreen(linearClampSampler, coord + float2(0.0,  Upscaler_ViewportInfo.y)).yx
					);

					half mean = (left.y + left.z + right.x + right.w) * half(0.25);
					left = left - half4(mean, mean, mean, mean);
					right = right - half4(mean, mean, mean, mean);
					upDown = upDown - half4(mean, mean, mean, mean);
					pix.w = pix.y - mean;

					half std = half(2.181818) / (abs(left.x) + abs(left.y) + abs(left.z) + abs(left.w) +
						       					 abs(right.x) + abs(right.y) + abs(right.z) + abs(right.w) +
						       					 abs(upDown.x) + abs(upDown.y) + abs(upDown.z) + abs(upDown.w));
#if defined(_UPSCALER__USE_EDGE_DIRECTION)
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
					finalY = clamp(Upscaler_Sharpness * finalY, min4, max4);

					half deltaY = finalY - pix.w;

					pix.x = saturate(pix.x + deltaY);
					pix.y = saturate(pix.y + deltaY);
					pix.z = saturate(pix.z + deltaY);
				}
				pix.w = 1.0;
				return pix;
			}
			ENDCG
        }
    }
}