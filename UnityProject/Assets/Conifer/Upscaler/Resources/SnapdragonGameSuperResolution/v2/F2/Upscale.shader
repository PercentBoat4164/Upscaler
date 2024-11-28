Shader "Conifer/Upscaler/Snapdragon Game Super Resolution/v2/F2/Upscale"
{
    SubShader
    {
        Cull Off ZWrite Off ZTest Always

		Pass
		{
			HLSLPROGRAM
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

			//============================================================================================================
			//
			//
			//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
			//                              SPDX-License-Identifier: BSD-3-Clause
			//
			//============================================================================================================


			half FastLanczos(half base)
			{
				half y = base - 1.0f;
				half y2 = y * y;
				half y_temp = 0.75f * y + y2;
				return y_temp * y2;
			}

			SamplerState linearClampSampler : register(s0);
			Texture2D<half3> Conifer_Upscaler_PrevOutput;
			Texture2D<half4> Conifer_Upscaler_MotionDepthClipAlphaBuffer;
			Texture2D<half3> _MainTex;

		    float4 Conifer_Upscaler_ClipToPrevClip[4];
		    float2 Conifer_Upscaler_RenderSize;           // {InputResolution.x, InputResolution.y}
		    float2 Conifer_Upscaler_OutputSize;           // {OutputResolution.x, OutputResolution.y}
		    float2 Conifer_Upscaler_RenderSizeRcp;        // {1.0 / InputResolution.x, 1.0 / InputResolution.y}
		    float2 Conifer_Upscaler_OutputSizeRcp;        // {1.0 / OutputResolution.x, 1.0 / OutputResolution.y}
		    float2 Conifer_Upscaler_JitterOffset;         // {jitter.x, jitter.y},
		    float2 Conifer_Upscaler_ScaleRatio;           // {OutputResolution.x / InputResolution.x, min(20.0, pow((OutputResolution.x*OutputResolution.y) / (InputResolution.x*InputResolution.y), 3.0)},
		    float  Conifer_Upscaler_CameraFovAngleHor;    // tan(radians(m_Camera.verticalFOV / 2)) * InputResolution.x / InputResolution.y
		    float  Conifer_Upscaler_MinLerpContribution;  // sameCameraFrmNum? 0.3: 0.0;
		    float  Conifer_Upscaler_Reset;
		    uint   Conifer_Upscaler_SameCamera;           // the frame number where camera pose is exactly same with previous frame

			half3 frag(float4 _ : POSITION, float2 uv : TEXCOORD) : SV_Target
			{
			    int2 input_pos = int2(clamp(uv + Conifer_Upscaler_JitterOffset * Conifer_Upscaler_OutputSizeRcp, 0.0, 1.0) * Conifer_Upscaler_RenderSize);
			    float3 mda = Conifer_Upscaler_MotionDepthClipAlphaBuffer.Load(int3(input_pos, 0)).xyz;
			    float2 prev_uv = clamp(uv - mda.xy, 0.0, 1.0);
			    half3 history_color = Conifer_Upscaler_PrevOutput.SampleLevel(linearClampSampler, prev_uv, 0.0).xyz;

				// return HistoryColor;

			    /////upsample and compute box
			    half4 Upsampledcw = half4(0.0, 0.0, 0.0, 0.0);
			    half biasmin = max(1.0f, 0.3 + 0.3 * Conifer_Upscaler_ScaleRatio.x);
			    half biasfactor = 0.25f * mda.z;
			    half kernelbias = lerp(Conifer_Upscaler_ScaleRatio.x, biasmin, biasfactor);
			    half motion_viewport_len = length(mda.xy * Conifer_Upscaler_OutputSize);
			    half curvebias = lerp(-2.0, -3.0, clamp(motion_viewport_len * 0.02, 0.0, 1.0));

			    half3 rectboxcenter = half3(0.0, 0.0, 0.0);
			    half3 rectboxvar = half3(0.0, 0.0, 0.0);
			    half rectboxweight = 0.0;
			    float2 srcpos = float2(input_pos) + float2(0.5, 0.5) - Conifer_Upscaler_JitterOffset;

			    kernelbias *= 0.5f;
			    half kernelbias2 = kernelbias * kernelbias;
			    half2 srcpos_srcOutputPos = srcpos - uv * Conifer_Upscaler_RenderSize;  //srcOutputPos = uv * params.renderSize;
			    half3 rectboxmin;
			    half3 rectboxmax;
			    half3 topMid = _MainTex.Load(int3(input_pos + int2(0, 1), 0)).xyz;
			    {
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
			    half3 rightMid = _MainTex.Load(int3(input_pos + int2(1, 0), 0)).xyz;
			    {
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
			    half3 leftMid = _MainTex.Load(int3(input_pos + int2(-1, 0), 0)).xyz;
			    {
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
			    half3 centerMid = _MainTex.Load(int3(input_pos + int2(0, 0), 0)).xyz;
			    {
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
			    half3 btmMid = _MainTex.Load(int3(input_pos + int2(0, -1), 0)).xyz;
			    {
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

			    if (Conifer_Upscaler_SameCamera!=0u)  //maybe disable this for ultra performance
			    // if (false)  //maybe disable this for ultra performance, true could generate more realistic output
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
			            half3 btmRight = _MainTex.Load(int3(input_pos + int2(1, -1) , 0)).xyz;
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
			            half3 btmLeft = _MainTex.Load(int3(input_pos + int2(-1, -1) , 0)).xyz;
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

			    const half EPSILON = 1.192e-07f;
			    half boxscale = max(mda.z, clamp(motion_viewport_len * 0.05f, 0.0, 1.0));
			    half boxsize = lerp(Conifer_Upscaler_ScaleRatio.y, 1.0f, boxscale);
			    half3 sboxvar = rectboxvar * boxsize;
			    half3 boxmin = rectboxcenter - sboxvar;
			    half3 boxmax = rectboxcenter + sboxvar;
			    rectboxmax = min(rectboxmax, boxmax);
			    rectboxmin = max(rectboxmin, boxmin);

			    half3 clampedcolor = clamp(history_color, rectboxmin, rectboxmax);
			    half startLerpValue = Conifer_Upscaler_MinLerpContribution;
			    if (abs(mda.x) + abs(mda.y) > 0.000001) startLerpValue = 0.0;
			    half lerpcontribution = any(rectboxmin > history_color) || any(history_color > rectboxmax) ? startLerpValue : 1.0f;

			    history_color = lerp(clampedcolor, history_color, clamp(lerpcontribution, 0.0, 1.0));
			    half basemin = min(basealpha, 0.1f);
			    basealpha = lerp(basemin, basealpha, clamp(lerpcontribution, 0.0, 1.0));

			    ////blend color
			    half alphasum = max(EPSILON, basealpha + Upsampledcw.w);
			    half alpha = clamp(Upsampledcw.w / alphasum + Conifer_Upscaler_Reset, 0.0, 1.0);

			    Upsampledcw.xyz = lerp(history_color, Upsampledcw.xyz, alpha);

			    return half3(Upsampledcw.xyz);
			}
			ENDHLSL
		}
    }
}