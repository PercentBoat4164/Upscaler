Shader "Conifer/Upscaler/Snapdragon Game Super Resolution/v2/F2/Convert"
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

			SamplerState linearClampSampler : register(s0);
			Texture2D<half> _CameraDepthTexture;
			Texture2D<half2> _MotionVectorTexture;

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

			float2 decodeVelocityFromTexture(half2 ev) {
			    const float inv_div = 1.0f / (0.499f * 0.5f);
			    return ev.xy * inv_div - 32767.0f / 65535.0f * inv_div;
			}

			float4 frag(float4 _ : POSITION, float2 uv : TEXCOORD) : SV_Target
			{
			    uint2 InputPos = uint2(uv * Conifer_Upscaler_OutputSize);
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

			    float4 btmLeft     = _CameraDepthTexture.Gather(linearClampSampler, gatherCoord, 0);
			    float2 v10         = gatherCoord + float2(Conifer_Upscaler_RenderSizeRcp.x * 2.0f, 0.0);
			    float4 btmRight    = _CameraDepthTexture.Gather(linearClampSampler, v10, 0);
			    float2 v12         = gatherCoord + float2(0.0, Conifer_Upscaler_RenderSizeRcp.y * 2.0f);
				float4 topLeft     = _CameraDepthTexture.Gather(linearClampSampler, v12, 0);
				float2 v14         = gatherCoord + float2(Conifer_Upscaler_RenderSizeRcp.x * 2.0f, Conifer_Upscaler_RenderSizeRcp.y * 2.0f);
				float4 topRight    = _CameraDepthTexture.Gather(linearClampSampler, v14, 0);
				float  maxC        = max(max(max(btmLeft.z,btmRight.w),topLeft.y),topRight.x);
				float  btmLeft4    = max(max(max(btmLeft.y,btmLeft.x),btmLeft.z),btmLeft.w);

			    float depthclip = 0.0;
			    if (maxC > 1.0e-05f)
			    {
			        float btmRight4 = min(min(min(btmRight.y,btmRight.x),btmRight.z),btmRight.w);
			        float topLeft4  = min(min(min(topLeft.y,topLeft.x),topLeft.z),topLeft.w);
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
			    return float4(_MotionVectorTexture.Load(int3(InputPos, 0)), depthclip, 0.0);
			}
			ENDHLSL
		}
    }
}