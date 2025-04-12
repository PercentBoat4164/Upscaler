Shader "Hidden/BlitDepth" {
    SubShader {
        PackageRequirements {
    		"com.unity.render-pipelines.universal"
    		"com.unity.render-pipelines.core"
    	}
    	Tags {"RenderPipeline" = "UniversalPipeline"}
    	ZTest Always Cull Off ZWrite On ColorMask 0
        Pass {
            Name "Blit Depth"
            HLSLPROGRAM
            #pragma multi_compile _ CONIFER__UPSCALER__USE_EDGE_DIRECTION
            #pragma target 5.0

            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            #include "Packages/com.unity.render-pipelines.core/Runtime/Utilities/Blit.hlsl"

			#pragma vertex Vert
			#pragma fragment FragDepth

			float4 FragDepth (Varyings input, out float depth : SV_Depth) : SV_Target
            {
                UNITY_SETUP_STEREO_EYE_INDEX_POST_VERTEX(input);
                depth = SAMPLE_TEXTURE2D_X_LOD(_BlitTexture, sampler_LinearRepeat, input.texcoord.xy, _BlitMipLevel).r;
                return float4(depth, 0, 0, 1);
            }
			ENDHLSL
        }
    }
    Fallback Off
}