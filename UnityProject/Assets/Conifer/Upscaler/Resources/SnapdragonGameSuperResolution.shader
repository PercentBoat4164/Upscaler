Shader "Conifer/Upscaler/Snapdragon Game Super Resolution"
{
	HLSLINCLUDE
	#pragma target 5.0

	SamplerState linearClampSampler : register(s0);

	struct VertexShaderOutput
	{
		float2 uv : TEXCOORD0;
		float4 pos : POSITION;
	};

	VertexShaderOutput vert(float4 pos : POSITION, float2 uv : TEXCOORD)
	{
		#if !UNITY_UV_STARTS_AT_TOP
		   pos.y = -pos.y;
		#endif
		VertexShaderOutput o;
		o.pos = pos;
		o.uv = uv;
		return o;
	}
	ENDHLSL

    SubShader
    {
        Cull Off ZWrite Off ZTest Always

        Pass
        {
            HLSLPROGRAM
			#pragma vertex vert
			#pragma fragment SnapdragonGameSuperResolutionV1

			Texture2D _MainTex : register(t0);
			float Conifer_Upscaler_EdgeSharpness;
			float4 Conifer_Upscaler_ViewportInfo : register(b0);
			
			half2 weightY(half dx, half dy, half c, half std)
			{
				half x = (dx * dx + dy * dy) * half(0.5) + clamp(abs(c) * std, 0.0, 1.0);
				half wA = x - half(4.0);
				half wB = x * wA - wA;
				wA *= wA;
				half w = wB * wA;
				return half2(w, w * c);
			}

			half4 SnapdragonGameSuperResolutionV1(float4 pos : POSITION, float2 uv : TEXCOORD) : SV_TARGET {
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

					half2 aWY = weightY(pl.x,       pl.y + 1.0, upDown.x, std) +
					            weightY(pl.x - 1.0, pl.y + 1.0, upDown.y, std) +
					            weightY(pl.x - 1.0, pl.y - 2.0, upDown.z, std) +
					            weightY(pl.x,       pl.y - 2.0, upDown.w, std) +
					            weightY(pl.x + 1.0, pl.y - 1.0, left.x,   std) +
					            weightY(pl.x,       pl.y - 1.0, left.y,   std) +
					            weightY(pl.x,       pl.y,       left.z,   std) +
					            weightY(pl.x + 1.0, pl.y,       left.w,   std) +
					            weightY(pl.x - 1.0, pl.y - 1.0, right.x,  std) +
					            weightY(pl.x - 2.0, pl.y - 1.0, right.y,  std) +
					            weightY(pl.x - 2.0, pl.y,       right.z,  std) +
					            weightY(pl.x - 1.0, pl.y,       right.w,  std);

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
        Pass
		{
			HLSLPROGRAM
			#pragma vertex vert
			#pragma fragment SnapdragonGameSuperResolutionV2Convert

			//============================================================================================================
			//
			//
			//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
			//                              SPDX-License-Identifier: BSD-3-Clause
			//
			//============================================================================================================

			Texture2D<half> Conifer_Upscaler_Depth;
			Texture2D<half2> Conifer_Upscaler_Motion;

		    float4 Conifer_Upscaler_ClipToPrevClip[4];
		    float2 Conifer_Upscaler_RenderSize;
		    float2 Conifer_Upscaler_OutputSize;
		    float2 Conifer_Upscaler_RenderSizeRcp;
		    float2 Conifer_Upscaler_OutputSizeRcp;
		    float2 Conifer_Upscaler_JitterOffset;
		    float2 Conifer_Upscaler_ScaleRatio;
		    float  Conifer_Upscaler_CameraFovAngleHor;
		    float  Conifer_Upscaler_MinLerpContribution;
		    float  Conifer_Upscaler_Reset;
		    uint   Conifer_Upscaler_SameCamera;

			float2 decodeVelocityFromTexture(float2 ev) {
			    const float inv_div = 1.0f / (0.499f * 0.5f);
			    float2 dv;
			    dv.xy = ev.xy * inv_div - 32767.0f / 65535.0f * inv_div;
			    return dv;

				// return ev * -Conifer_Upscaler_RenderSize;
			}

			float4 SnapdragonGameSuperResolutionV2Convert(float4 pos : POSITION, float2 uv : TEXCOORD) : SV_Target
			{
			    uint2 InputPos = uint2(uv * Conifer_Upscaler_RenderSize);
			    float2 gatherCoord = uv - float2(0.5, 0.5) * Conifer_Upscaler_RenderSizeRcp;


			    // texture gather to find nearest depth
			    //      a  b  c  d
			    //      e  f  g  h
			    //      i  j  k  l
			    //      m  n  o  p
			    //btmLeft mnji
			    //btmRight oplk
			    //topLeft  efba
			    //topRight ghdc

			    float4 btmLeft     = Conifer_Upscaler_Depth.Gather(linearClampSampler, gatherCoord, 0);
			    float2 v10         = gatherCoord + float2(Conifer_Upscaler_RenderSizeRcp.x * 2.0f, 0.0);
			    float4 btmRight    = Conifer_Upscaler_Depth.Gather(linearClampSampler, v10, 0);
			    float2 v12         = gatherCoord + float2(0.0, Conifer_Upscaler_RenderSizeRcp.y * 2.0f);
				float4 topLeft     = Conifer_Upscaler_Depth.Gather(linearClampSampler, v12, 0);
				float2 v14         = gatherCoord + float2(Conifer_Upscaler_RenderSizeRcp.x * 2.0f, Conifer_Upscaler_RenderSizeRcp.y * 2.0f);
				float4 topRight    = Conifer_Upscaler_Depth.Gather(linearClampSampler, v14, 0);
				float  maxC        = max(max(max(btmLeft.z,btmRight.w),topLeft.y),topRight.x);
				float  btmLeft4    = max(max(max(btmLeft.y,btmLeft.x),btmLeft.z),btmLeft.w);
				float  btmLeftMax9 = max(topLeft.x,max(max(maxC,btmLeft4),btmRight.x));

			    float depthclip = 0.0;
			    if (maxC > 1.0e-05f)
			    {
			        float btmRight4 = min(min(min(btmRight.y,btmRight.x),btmRight.z),btmRight.w);
			        float topLeft4 = min(min(min(topLeft.y,topLeft.x),topLeft.z),topLeft.w);
			        float topRight4 = min(min(min(topRight.y,topRight.x),topRight.z),topRight.w);

			        float Wdepth = 0.0;
			        float Ksep = 1.37e-05f;
			        float Kfov = Conifer_Upscaler_CameraFovAngleHor;
			        float diagonal_length = length(Conifer_Upscaler_RenderSize);
			        float Ksep_Kfov_diagonal = Ksep * Kfov * diagonal_length;

					float Depthsep = Ksep_Kfov_diagonal * (1.0 - maxC);
					float EPSILON = 1.19e-07f;
					Wdepth += clamp(Depthsep / (abs(maxC - btmLeft4) + EPSILON), 0.0, 1.0);
					Wdepth += clamp(Depthsep / (abs(maxC - btmRight4) + EPSILON), 0.0, 1.0);
					Wdepth += clamp(Depthsep / (abs(maxC - topLeft4) + EPSILON), 0.0, 1.0);
					Wdepth += clamp(Depthsep / (abs(maxC - topRight4) + EPSILON), 0.0, 1.0);
			        depthclip = clamp(1.0f - Wdepth * 0.25, 0.0, 1.0);
			    }

			    //refer to ue/fsr2 PostProcessFFX_FSR2ConvertVelocity.usf, and using nearest depth for dilated motion

			    half2 EncodedVelocity = Conifer_Upscaler_Motion.Load(int3(InputPos, 0));

			    float2 motion;
			    // if (EncodedVelocity.x > 0.0)
			    // {
			        motion = decodeVelocityFromTexture(EncodedVelocity.xy);
			    // }
			    // else
			    // {
			    //     //float2 ScreenPos = float2(2.0f * texCoord.x - 1.0f, 1.0f - 2.0f * texCoord.y);
			    //     float2 ScreenPos = float2(2.0f * uv - 1.0f);       // NDC Y+ down from viewport Y+ down
			    //     float3 Position  = float3(ScreenPos, btmLeftMax9); //this_clip
			    //     float4 PreClip   = Conifer_Upscaler_ClipToPrevClip[3] + (Conifer_Upscaler_ClipToPrevClip[2] * Position.z + (Conifer_Upscaler_ClipToPrevClip[1] * ScreenPos.y + Conifer_Upscaler_ClipToPrevClip[0] * ScreenPos.x));
			    //     float2 PreScreen = PreClip.xy / PreClip.w;
			    //     motion           = Position.xy - PreScreen;
			    // }
			    return float4(motion, depthclip, 0.0);
			}
			ENDHLSL
		}
		Pass
		{
			HLSLPROGRAM
			#pragma vertex vert
			#pragma fragment SnapdragonGameSuperResolutionV2Upscale

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

			half3 SnapdragonGameSuperResolutionV2Upscale(float4 pos : POSITION, float2 uv : TEXCOORD) : SV_Target
			{
			    float Biasmax_viewportXScale = Conifer_Upscaler_ScaleRatio.x;
			    float scalefactor = Conifer_Upscaler_ScaleRatio.y;

			    float2 Hruv = uv;

			    float2 Jitteruv;
			    Jitteruv.x = clamp(Hruv.x + Conifer_Upscaler_JitterOffset.x * Conifer_Upscaler_OutputSizeRcp.x, 0.0, 1.0);
			    Jitteruv.y = clamp(Hruv.y + Conifer_Upscaler_JitterOffset.y * Conifer_Upscaler_OutputSizeRcp.y, 0.0, 1.0);

			    int2 InputPos = int2(Jitteruv * Conifer_Upscaler_RenderSize);

			    float3 mda = Conifer_Upscaler_MotionDepthClipAlphaBuffer.SampleLevel(linearClampSampler, Jitteruv, 0.0).xyz;
			    float2 Motion = mda.xy;

			    float2 PrevUV;
			    PrevUV.x = clamp(-0.5 * Motion.x + Hruv.x, 0.0, 1.0);
			    PrevUV.y = clamp(-0.5 * Motion.y + Hruv.y, 0.0, 1.0);

			    float depthfactor = mda.z;

			    half3 HistoryColor = Conifer_Upscaler_PrevOutput.SampleLevel(linearClampSampler, PrevUV, 0.0).xyz;

			    /////upsample and compute box
			    half4 Upsampledcw = half4(0.0, 0.0, 0.0, 0.0);
			    half biasmax = Biasmax_viewportXScale ;
			    half biasmin = max(1.0f, 0.3 + 0.3 * biasmax);
			    half biasfactor = 0.25f * depthfactor;
			    half kernelbias = lerp(biasmax, biasmin, biasfactor);
			    half motion_viewport_len = length(Motion * Conifer_Upscaler_OutputSize);
			    half curvebias = lerp(-2.0, -3.0, clamp(motion_viewport_len * 0.02, 0.0, 1.0));

			    half3 rectboxcenter = half3(0.0, 0.0, 0.0);
			    half3 rectboxvar = half3(0.0, 0.0, 0.0);
			    half rectboxweight = 0.0;
			    float2 srcpos = float2(InputPos) + float2(0.5, 0.5) - Conifer_Upscaler_JitterOffset;

			    kernelbias *= 0.5f;
			    half kernelbias2 = kernelbias * kernelbias;
			    half2 srcpos_srcOutputPos = srcpos - Hruv * Conifer_Upscaler_RenderSize;  //srcOutputPos = Hruv * params.renderSize;
			    half3 rectboxmin;
			    half3 rectboxmax;
			    half3 topMid = _MainTex.Load(int3(InputPos + int2(0, 1), 0)).xyz;
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
			    half3 rightMid = _MainTex.Load(int3(InputPos + int2(1, 0), 0)).xyz;
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
			    half3 leftMid = _MainTex.Load(int3(InputPos + int2(-1, 0), 0)).xyz;
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
			    half3 centerMid = _MainTex.Load(int3(InputPos + int2(0, 0), 0)).xyz;
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
			    half3 btmMid = _MainTex.Load(int3(InputPos + int2(0, -1), 0)).xyz;
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
			            half3 topRight = _MainTex.Load(int3(InputPos + int2(1, 1), 0)).xyz;
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
			            half3 topLeft = _MainTex.Load(int3(InputPos + int2(-1, 1), 0)).xyz;
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
			            half3 btmRight = _MainTex.Load(int3(InputPos + int2(1, -1) , 0)).xyz;
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
			            half3 btmLeft = _MainTex.Load(int3(InputPos + int2(-1, -1) , 0)).xyz;
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

			    half baseupdate = 1.0f - depthfactor;
			    baseupdate = min(baseupdate, lerp(baseupdate, Upsampledcw.w *10.0f, clamp(10.0f* motion_viewport_len, 0.0, 1.0)));
			    baseupdate = min(baseupdate, lerp(baseupdate, Upsampledcw.w, clamp(motion_viewport_len *0.05f, 0.0, 1.0)));
			    half basealpha = baseupdate;

			    const half EPSILON = 1.192e-07f;
			    half boxscale = max(depthfactor, clamp(motion_viewport_len * 0.05f, 0.0, 1.0));
			    half boxsize = lerp(scalefactor, 1.0f, boxscale);
			    half3 sboxvar = rectboxvar * boxsize;
			    half3 boxmin = rectboxcenter - sboxvar;
			    half3 boxmax = rectboxcenter + sboxvar;
			    rectboxmax = min(rectboxmax, boxmax);
			    rectboxmin = max(rectboxmin, boxmin);

			    half3 clampedcolor = clamp(HistoryColor, rectboxmin, rectboxmax);
			    half startLerpValue = Conifer_Upscaler_MinLerpContribution;
			    if (abs(mda.x) + abs(mda.y) > 0.000001) startLerpValue = 0.0;
			    half lerpcontribution = any(rectboxmin > HistoryColor) || any(HistoryColor > rectboxmax) ? startLerpValue : 1.0f;

			    HistoryColor = lerp(clampedcolor, HistoryColor, clamp(lerpcontribution, 0.0, 1.0));
			    half basemin = min(basealpha, 0.1f);
			    basealpha = lerp(basemin, basealpha, clamp(lerpcontribution, 0.0, 1.0));

			    ////blend color
			    half alphasum = max(EPSILON, basealpha + Upsampledcw.w);
			    half alpha = clamp(Upsampledcw.w / alphasum + Conifer_Upscaler_Reset, 0.0, 1.0);

			    Upsampledcw.xyz = lerp(HistoryColor, Upsampledcw.xyz, alpha);

			    return half3(Upsampledcw.xyz);
			}
			ENDHLSL
		}
    }
}