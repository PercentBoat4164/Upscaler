//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

Shader "Conifer/Upscaler/Snapdragon Game Super Resolution/v1/Upscale"
{
    SubShader
    {
        Cull Off ZWrite Off ZTest Always

        Pass
        {
        	Name "Snapdragon Game Super Resolution 1"
            HLSLPROGRAM
            #pragma multi_compile _ CONIFER_UPSCALER_USE_EDGE_DIRECTION
            #pragma target 5.0

            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            #include "Packages/com.unity.render-pipelines.core/Runtime/Utilities/Blit.hlsl"

			#pragma vertex Vert
			#pragma fragment Frag

            SamplerState pointClampSampler : register(s0);
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

			half4 Frag(Varyings input) : SV_TARGET {
				UNITY_SETUP_STEREO_EYE_INDEX_POST_VERTEX(input);
				half4 pix = half4(0, 0, 0, 1);
				pix.xyz = SAMPLE_TEXTURE2D_X_LOD(_BlitTexture, pointClampSampler, input.texcoord * Conifer_Upscaler_ViewportInfo.xy * Conifer_Upscaler_ViewportInfo.zw, _BlitMipLevel).xyz;

				float2 imgCoord = input.texcoord.xy * Conifer_Upscaler_ViewportInfo.zw + float2(-0.5, 0.5);
				float2 imgCoordPixel = floor(imgCoord);
				float2 coord = imgCoordPixel * Conifer_Upscaler_ViewportInfo.xy;
				half2 pl = imgCoord - imgCoordPixel;
				half4 left = GATHER_GREEN_TEXTURE2D_X(_BlitTexture, pointClampSampler, coord);

				if (abs(left.z - left.y) + abs(pix[1] - left.y) + abs(pix[1] - left.z) > 8.0 / 255.0) {
					coord.x += Conifer_Upscaler_ViewportInfo.x;

					half4 right = GATHER_GREEN_TEXTURE2D_X(_BlitTexture, pointClampSampler, coord + float2(Conifer_Upscaler_ViewportInfo.x,  0.0));
					half4 upDown = half4(
						GATHER_GREEN_TEXTURE2D_X(_BlitTexture, pointClampSampler, coord + float2(0.0, -Conifer_Upscaler_ViewportInfo.y)).wz,
						GATHER_GREEN_TEXTURE2D_X(_BlitTexture, pointClampSampler, coord + float2(0.0,  Conifer_Upscaler_ViewportInfo.y)).yx
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