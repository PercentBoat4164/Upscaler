//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================

Shader "Upscaler/Snapdragon Game Super Resolution/V2Fragment2Pass"
{
	Properties { _MainTex ("Texture", 2D) = "" {} }
    SubShader
    {
        Cull Off ZWrite Off ZTest Always
        Pass
		{
			name "Upscaler | Snapdragon Game Super Resolution 2 - Fragment 2 Pass - Convert"
			CGPROGRAM
			#pragma target 5.0
			#pragma multi_compile_local_fragment _ _UPSCALER__ENABLE_OUTPUT_RESOLUTION_MOTION_VECTORS

            #include "UnityCG.cginc"

			#pragma vertex vert_img
			#pragma fragment Frag

			SamplerState     pointClampSampler : register(s0);
			Texture2D        _MainTex;
			Texture2D<half2> Upscaler_MotionVectors;

		    float2 Upscaler_RenderSize;
		    float2 Upscaler_OutputSize;
		    float2 Upscaler_RenderSizeRcp;
		    float  Upscaler_CameraFovAngleHor;

			#define EPSILON 1.19e-07f
			#define SEPARATION 1.37e-05f
			#define TOLERANCE 1.0e-05f

			float4 Frag(v2f_img input) : SV_Target
			{
				UNITY_SETUP_STEREO_EYE_INDEX_POST_VERTEX(input);
			    float2 gatherCoord = input.uv - float2(0.5, 0.5) * Upscaler_RenderSizeRcp;

			    float4 btmLeft     = _MainTex.Gather(pointClampSampler, gatherCoord);
			    float2 v10         = gatherCoord + float2(Upscaler_RenderSizeRcp.x * 2.0f, 0.0);
			    float4 btmRight    = _MainTex.Gather(pointClampSampler, v10);
			    float2 v12         = gatherCoord + float2(0.0, Upscaler_RenderSizeRcp.y * 2.0f);
				float4 topLeft     = _MainTex.Gather(pointClampSampler, v12);
				float2 v14         = gatherCoord + float2(Upscaler_RenderSizeRcp.x * 2.0f, Upscaler_RenderSizeRcp.y * 2.0f);
				float4 topRight    = _MainTex.Gather(pointClampSampler, v14);
				float  maxC        = max(max(max(btmLeft.z,btmRight.w),topLeft.y),topRight.x);
				float  btmLeft4    = max(max(max(btmLeft.y,btmLeft.x),btmLeft.z),btmLeft.w);

			    float depthclip = 0.0;
			    if (maxC > TOLERANCE)
			    {
			        float btmRight4 = min(min(min(btmRight.y,btmRight.x),btmRight.z),btmRight.w);
			        float topLeft4  = min(min(min(topLeft.y,topLeft.x),topLeft.z),topLeft.w);
			        float topRight4 = min(min(min(topRight.y,topRight.x),topRight.z),topRight.w);

			        float Wdepth = 0.0;
			        float Kfov = Upscaler_CameraFovAngleHor;
			        float diagonal_length = length(Upscaler_RenderSize);
			        float Ksep_Kfov_diagonal = SEPARATION * Kfov * diagonal_length;

					float Depthsep = Ksep_Kfov_diagonal * (1.0 - maxC);
					Wdepth += clamp(Depthsep / (abs(maxC - btmLeft4) + EPSILON), 0.0, 1.0);
					Wdepth += clamp(Depthsep / (abs(maxC - btmRight4) + EPSILON), 0.0, 1.0);
					Wdepth += clamp(Depthsep / (abs(maxC - topLeft4) + EPSILON), 0.0, 1.0);
					Wdepth += clamp(Depthsep / (abs(maxC - topRight4) + EPSILON), 0.0, 1.0);
			        depthclip = clamp(1.0f - Wdepth * 0.25, 0.0, 1.0);
			    }
#if defined(_UPSCALER__ENABLE_OUTPUT_RESOLUTION_MOTION_VECTORS)
				return float4(Upscaler_MotionVectors.Load(int3(input.uv * Upscaler_OutputSize, 0)), depthclip, 0.0);
#else
				return float4(Upscaler_MotionVectors.Load(int3(input.uv * Upscaler_RenderSize, 0)), depthclip, 0.0);
#endif
			}
			ENDCG
		}
		Pass
		{
			name "Upscaler | Snapdragon Game Super Resolution 2 - Fragment 2 Pass - Upscale"
			CGPROGRAM
			#pragma target 5.0

            #include "UnityCG.cginc"

			#pragma vertex vert_img
			#pragma fragment Frag

			#define EPSILON 1.19e-07f
			#define TOLERANCE 1.0e-05f

			SamplerState     pointClampSampler : register(s0);
			Texture2D        _MainTex;
			Texture2D<half3> Upscaler_History;
			Texture2D<half4> Upscaler_MotionDepthAlphaBuffer;

		    float2 Upscaler_RenderSize;
		    float2 Upscaler_OutputSize;
		    float2 Upscaler_OutputSizeRcp;
		    float2 Upscaler_JitterOffset;
		    float2 Upscaler_ScaleRatio;
		    float  Upscaler_MinLerpContribution;
		    float  Upscaler_Reset;
		    uint   Upscaler_SameCamera;

			half FastLanczos(half base)
			{
				half y = base - 1.0f;
				half y2 = y * y;
				half y_temp = 0.75f * y + y2;
				return y_temp * y2;
			}

			half3 Frag(v2f_img input) : SV_Target
			{
				UNITY_SETUP_STEREO_EYE_INDEX_POST_VERTEX(input);
			    int2 input_pos = int2(clamp(input.uv + Upscaler_JitterOffset * Upscaler_OutputSizeRcp, 0.0, 1.0) * Upscaler_RenderSize);
			    float3 mda = Upscaler_MotionDepthAlphaBuffer.Load(int3(input_pos, 0)).xyz;
			    float2 prev_uv = clamp(input.uv - mda.xy, 0.0, 1.0);
			    half3 history_color = Upscaler_History.SampleLevel(pointClampSampler, prev_uv, 0.0).xyz;

			    half4 Upsampledcw = 0.0;
			    half biasmin = max(1.0f, 0.3 + 0.3 * Upscaler_ScaleRatio.x);
			    half biasfactor = 0.25f * mda.z;
			    half kernelbias = lerp(Upscaler_ScaleRatio.x, biasmin, biasfactor);
			    half motion_viewport_len = length(mda.xy * Upscaler_OutputSize);
			    half curvebias = lerp(-2.0, -3.0, clamp(motion_viewport_len * 0.02, 0.0, 1.0));

			    half3 rectboxcenter = 0.0;
			    half3 rectboxvar = 0.0;
			    half rectboxweight = 0.0;
			    float2 srcpos = float2(input_pos) + 0.5 - Upscaler_JitterOffset;

			    kernelbias *= 0.5f;
			    half kernelbias2 = kernelbias * kernelbias;
			    half2 srcpos_srcOutputPos = srcpos - input.uv * Upscaler_RenderSize;
			    half3 rectboxmin;
			    half3 rectboxmax;
			    {
					half3 topMid = _MainTex.Load(int3(input_pos + int2(0, 1), 0)).xyz;
			        half3 samplecolor = topMid;
			        half2 baseoffset = srcpos_srcOutputPos + half2(0.0, 1.0);
			        half baseoffset_dot = dot(baseoffset, baseoffset);
			        half base = clamp(baseoffset_dot * kernelbias2, 0.0f, 1.0f);
			        half weight = FastLanczos(base);
			        Upsampledcw += half4(samplecolor * weight, weight);
			        half boxweight = exp(baseoffset_dot * curvebias);
			        rectboxmin = samplecolor;
			        rectboxmax = samplecolor;
			        half3 wsample = samplecolor * boxweight;
			        rectboxcenter += wsample;
			        rectboxvar += samplecolor * wsample;
			        rectboxweight += boxweight;
			    }
			    {
					half3 rightMid = _MainTex.Load(int3(input_pos + int2(1, 0), 0)).xyz;
			        half3 samplecolor = rightMid;
			        half2 baseoffset = srcpos_srcOutputPos + half2(1.0, 0.0);
			        half baseoffset_dot = dot(baseoffset, baseoffset);
			        half base = clamp(baseoffset_dot * kernelbias2, 0.0f, 1.0f);
			        half weight = FastLanczos(base);
			        Upsampledcw += half4(samplecolor * weight, weight);
			        half boxweight = exp(baseoffset_dot * curvebias);
			        rectboxmin = min(rectboxmin, samplecolor);
			        rectboxmax = max(rectboxmax, samplecolor);
			        half3 wsample = samplecolor * boxweight;
			        rectboxcenter += wsample;
			        rectboxvar += samplecolor * wsample;
			        rectboxweight += boxweight;
			    }
			    {
					half3 leftMid = _MainTex.Load(int3(input_pos + int2(-1, 0), 0)).xyz;
			        half3 samplecolor = leftMid;
			        half2 baseoffset = srcpos_srcOutputPos + half2(-1.0, 0.0);
			        half baseoffset_dot = dot(baseoffset, baseoffset);
			        half base = clamp(baseoffset_dot * kernelbias2, 0.0f, 1.0f);
			        half weight = FastLanczos(base);
			        Upsampledcw += half4(samplecolor * weight, weight);
			        half boxweight = exp(baseoffset_dot * curvebias);
			        rectboxmin = min(rectboxmin, samplecolor);
			        rectboxmax = max(rectboxmax, samplecolor);
			        half3 wsample = samplecolor * boxweight;
			        rectboxcenter += wsample;
			        rectboxvar += samplecolor * wsample;
			        rectboxweight += boxweight;
			    }
			    {
					half3 centerMid = _MainTex.Load(int3(input_pos + int2(0, 0), 0)).xyz;
			        half3 samplecolor = centerMid;
			        half2 baseoffset = srcpos_srcOutputPos;
			        half baseoffset_dot = dot(baseoffset, baseoffset);
			        half base = clamp(baseoffset_dot * kernelbias2, 0.0f, 1.0f);
			        half weight = FastLanczos(base);
			        Upsampledcw += half4(samplecolor * weight, weight);
			        half boxweight = exp(baseoffset_dot * curvebias);
			        rectboxmin = min(rectboxmin, samplecolor);
			        rectboxmax = max(rectboxmax, samplecolor);
			        half3 wsample = samplecolor * boxweight;
			        rectboxcenter += wsample;
			        rectboxvar += samplecolor * wsample;
			        rectboxweight += boxweight;
			    }
			    {
					half3 btmMid = _MainTex.Load(int3(input_pos + int2(0, -1), 0)).xyz;
			        half3 samplecolor = btmMid;
			        half2 baseoffset = srcpos_srcOutputPos + half2(0.0, -1.0);
			        half baseoffset_dot = dot(baseoffset, baseoffset);
			        half base = clamp(baseoffset_dot * kernelbias2, 0.0f, 1.0f);
			        half weight = FastLanczos(base);
			        Upsampledcw += half4(samplecolor * weight, weight);
			        half boxweight = exp(baseoffset_dot * curvebias);
			        rectboxmin = min(rectboxmin, samplecolor);
			        rectboxmax = max(rectboxmax, samplecolor);
			        half3 wsample = samplecolor * boxweight;
			        rectboxcenter += wsample;
			        rectboxvar += samplecolor * wsample;
			        rectboxweight += boxweight;
			    }

			    if (Upscaler_SameCamera)
			    {
			        {
			            half3 topRight = _MainTex.Load(int3(input_pos + int2(1, 1), 0)).xyz;
			            half3 samplecolor = topRight;
			            half2 baseoffset = srcpos_srcOutputPos + half2(1.0, 1.0);
			            half baseoffset_dot = dot(baseoffset, baseoffset);
			            half base = clamp(baseoffset_dot * kernelbias2, 0.0, 1.0);
			            half weight = FastLanczos(base);
			            Upsampledcw += half4(samplecolor * weight, weight);
			            half boxweight = exp(baseoffset_dot * curvebias);
			            rectboxmin = min(rectboxmin, samplecolor);
			            rectboxmax = max(rectboxmax, samplecolor);
			            half3 wsample = samplecolor * boxweight;
			            rectboxcenter += wsample;
			            rectboxvar += samplecolor * wsample;
			            rectboxweight += boxweight;
			        }
			        {
			            half3 topLeft = _MainTex.Load(int3(input_pos + int2(-1, 1), 0)).xyz;
			            half3 samplecolor = topLeft;
			            half2 baseoffset = srcpos_srcOutputPos + half2(-1.0, 1.0);
			            half baseoffset_dot = dot(baseoffset, baseoffset);
			            half base = clamp(baseoffset_dot * kernelbias2, 0.0f, 1.0f);
			            half weight = FastLanczos(base);
			            Upsampledcw += half4(samplecolor * weight, weight);
			            half boxweight = exp(baseoffset_dot * curvebias);
			            rectboxmin = min(rectboxmin, samplecolor);
			            rectboxmax = max(rectboxmax, samplecolor);
			            half3 wsample = samplecolor * boxweight;
			            rectboxcenter += wsample;
			            rectboxvar += samplecolor * wsample;
			            rectboxweight += boxweight;
			        }
			        {
			            half3 btmRight = _MainTex.Load(int3(input_pos + int2(1, -1), 0)).xyz;
			            half3 samplecolor = btmRight;
			            half2 baseoffset = srcpos_srcOutputPos + half2(1.0, -1.0);
			            half baseoffset_dot = dot(baseoffset, baseoffset);
			            half base = clamp(baseoffset_dot * kernelbias2, 0.0f, 1.0f);
			            half weight = FastLanczos(base);
			            Upsampledcw += half4(samplecolor * weight, weight);
			            half boxweight = exp(baseoffset_dot * curvebias);
			            rectboxmin = min(rectboxmin, samplecolor);
			            rectboxmax = max(rectboxmax, samplecolor);
			            half3 wsample = samplecolor * boxweight;
			            rectboxcenter += wsample;
			            rectboxvar += samplecolor * wsample;
			            rectboxweight += boxweight;
			        }

			        {
			            half3 btmLeft = _MainTex.Load(int3(input_pos + int2(-1, -1), 0)).xyz;
			            half3 samplecolor = btmLeft;
			            half2 baseoffset = srcpos_srcOutputPos + half2(-1.0, -1.0);
			            half baseoffset_dot = dot(baseoffset, baseoffset);
			            half base = clamp(baseoffset_dot * kernelbias2, 0.0f, 1.0f);
			            half weight = FastLanczos(base);
			            Upsampledcw += half4(samplecolor * weight, weight);
			            half boxweight = exp(baseoffset_dot * curvebias);
			            rectboxmin = min(rectboxmin, samplecolor);
			            rectboxmax = max(rectboxmax, samplecolor);
			            half3 wsample = samplecolor * boxweight;
			            rectboxcenter += wsample;
			            rectboxvar += samplecolor * wsample;
			            rectboxweight += boxweight;
			        }
			    }

			    rectboxweight = 1.0 / rectboxweight;
			    rectboxcenter *= rectboxweight;
			    rectboxvar *= rectboxweight;
			    rectboxvar = sqrt(abs(rectboxvar - rectboxcenter * rectboxcenter));

			    Upsampledcw.xyz =  clamp(Upsampledcw.xyz / Upsampledcw.w, rectboxmin - half3(0.075, 0.075, 0.075), rectboxmax + half3(0.075, 0.075, 0.075));
			    Upsampledcw.w = Upsampledcw.w * (1.0f / 3.0f) ;

			    half baseupdate = 1.0f - mda.z;
			    baseupdate = min(baseupdate, lerp(baseupdate, Upsampledcw.w *10.0f, clamp(10.0f* motion_viewport_len, 0.0, 1.0)));
			    baseupdate = min(baseupdate, lerp(baseupdate, Upsampledcw.w, clamp(motion_viewport_len *0.05f, 0.0, 1.0)));
			    half basealpha = baseupdate;

			    half boxscale = max(mda.z, clamp(motion_viewport_len * 0.05f, 0.0, 1.0));
			    half boxsize = lerp(Upscaler_ScaleRatio.y, 1.0f, boxscale);
			    half3 sboxvar = rectboxvar * boxsize;
			    half3 boxmin = rectboxcenter - sboxvar;
			    half3 boxmax = rectboxcenter + sboxvar;
			    rectboxmax = min(rectboxmax, boxmax);
			    rectboxmin = max(rectboxmin, boxmin);

			    half3 clamped_color = clamp(history_color, rectboxmin, rectboxmax);
			    half startLerpValue = Upscaler_MinLerpContribution;
			    if (abs(mda.x) + abs(mda.y) > TOLERANCE) startLerpValue = 0.0;
			    half lerpcontribution = any(rectboxmin > history_color) || any(history_color > rectboxmax) ? startLerpValue : 1.0f;

			    history_color = lerp(clamped_color, history_color, clamp(lerpcontribution, 0.0, 1.0));
			    half basemin = min(basealpha, 0.1f);
			    basealpha = lerp(basemin, basealpha, clamp(lerpcontribution, 0.0, 1.0));

			    half alphasum = max(EPSILON, basealpha + Upsampledcw.w);
			    half alpha = clamp(Upsampledcw.w / alphasum + Upscaler_Reset, 0.0, 1.0);

			    Upsampledcw.xyz = lerp(history_color, Upsampledcw.xyz, alpha);

			    return half3(Upsampledcw.xyz);
			}
			ENDCG
		}
    }
}