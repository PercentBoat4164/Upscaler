#include "ffx_fsr2_tcr_autogen_pass_ee5cd9d13b7ae336c8f5a7336c8da15c.h"
#include "ffx_fsr2_tcr_autogen_pass_e3d7ae37bc9e45800176cfd8d2b0cd40.h"
#include "ffx_fsr2_tcr_autogen_pass_4ec8740b7257df5649bc59992fa35847.h"
#include "ffx_fsr2_tcr_autogen_pass_bac77095e97869df4e2fdb361d116a63.h"

typedef union ffx_fsr2_tcr_autogen_pass_PermutationKey {
	struct {
		uint32_t FFX_FSR2_OPTION_REPROJECT_USE_LANCZOS_TYPE : 1;
		uint32_t FFX_FSR2_OPTION_HDR_COLOR_INPUT : 1;
		uint32_t FFX_FSR2_OPTION_LOW_RESOLUTION_MOTION_VECTORS : 1;
		uint32_t FFX_FSR2_OPTION_JITTERED_MOTION_VECTORS : 1;
		uint32_t FFX_FSR2_OPTION_INVERTED_DEPTH : 1;
		uint32_t FFX_FSR2_OPTION_APPLY_SHARPENING : 1;
		uint32_t FFX_HALF : 1;
	};
	uint32_t index;
} ffx_fsr2_tcr_autogen_pass_PermutationKey;

typedef struct ffx_fsr2_tcr_autogen_pass_PermutationInfo {
	const uint32_t       blobSize;
	const unsigned char* blobData;

	const uint32_t  numSamplerResources;
	const char**    samplerResourceNames;
	const uint32_t* samplerResourceBindings;
	const uint32_t* samplerResourceCounts;
	const uint32_t* samplerResourceSets;

	const uint32_t  numCombinedSamplerResources;
	const char**    combinedSamplerResourceNames;
	const uint32_t* combinedSamplerResourceBindings;
	const uint32_t* combinedSamplerResourceCounts;
	const uint32_t* combinedSamplerResourceSets;

	const uint32_t  numSampledImageResources;
	const char**    sampledImageResourceNames;
	const uint32_t* sampledImageResourceBindings;
	const uint32_t* sampledImageResourceCounts;
	const uint32_t* sampledImageResourceSets;

	const uint32_t  numStorageImageResources;
	const char**    storageImageResourceNames;
	const uint32_t* storageImageResourceBindings;
	const uint32_t* storageImageResourceCounts;
	const uint32_t* storageImageResourceSets;

	const uint32_t  numUniformTexelBufferResources;
	const char**    uniformTexelBufferResourceNames;
	const uint32_t* uniformTexelBufferResourceBindings;
	const uint32_t* uniformTexelBufferResourceCounts;
	const uint32_t* uniformTexelBufferResourceSets;

	const uint32_t  numStorageTexelBufferResources;
	const char**    storageTexelBufferResourceNames;
	const uint32_t* storageTexelBufferResourceBindings;
	const uint32_t* storageTexelBufferResourceCounts;
	const uint32_t* storageTexelBufferResourceSets;

	const uint32_t  numUniformBufferResources;
	const char**    uniformBufferResourceNames;
	const uint32_t* uniformBufferResourceBindings;
	const uint32_t* uniformBufferResourceCounts;
	const uint32_t* uniformBufferResourceSets;

	const uint32_t  numStorageBufferResources;
	const char**    storageBufferResourceNames;
	const uint32_t* storageBufferResourceBindings;
	const uint32_t* storageBufferResourceCounts;
	const uint32_t* storageBufferResourceSets;

	const uint32_t  numInputAttachmentResources;
	const char**    inputAttachmentResourceNames;
	const uint32_t* inputAttachmentResourceBindings;
	const uint32_t* inputAttachmentResourceCounts;
	const uint32_t* inputAttachmentResourceSets;

	const uint32_t  numRTAccelerationStructureResources;
	const char**    rtAccelerationStructureResourceNames;
	const uint32_t* rtAccelerationStructureResourceBindings;
	const uint32_t* rtAccelerationStructureResourceCounts;
	const uint32_t* rtAccelerationStructureResourceSets;

} ffx_fsr2_tcr_autogen_pass_PermutationInfo;

static const uint32_t g_ffx_fsr2_tcr_autogen_pass_IndirectionTable[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	1,
	2,
	2,
	2,
	2,
	2,
	2,
	2,
	2,
	3,
	3,
	3,
	3,
	3,
	3,
	3,
	3,
	2,
	2,
	2,
	2,
	2,
	2,
	2,
	2,
	3,
	3,
	3,
	3,
	3,
	3,
	3,
	3,
	2,
	2,
	2,
	2,
	2,
	2,
	2,
	2,
	3,
	3,
	3,
	3,
	3,
	3,
	3,
	3,
	2,
	2,
	2,
	2,
	2,
	2,
	2,
	2,
	3,
	3,
	3,
	3,
	3,
	3,
	3,
	3,
};

static const ffx_fsr2_tcr_autogen_pass_PermutationInfo g_ffx_fsr2_tcr_autogen_pass_PermutationInfo[] = {
	{
		g_ffx_fsr2_tcr_autogen_pass_ee5cd9d13b7ae336c8f5a7336c8da15c_size,
		g_ffx_fsr2_tcr_autogen_pass_ee5cd9d13b7ae336c8f5a7336c8da15c_data,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
		7,
		g_ffx_fsr2_tcr_autogen_pass_ee5cd9d13b7ae336c8f5a7336c8da15c_SampledImageResourceNames,
		g_ffx_fsr2_tcr_autogen_pass_ee5cd9d13b7ae336c8f5a7336c8da15c_SampledImageResourceBindings,
		g_ffx_fsr2_tcr_autogen_pass_ee5cd9d13b7ae336c8f5a7336c8da15c_SampledImageResourceCounts,
		g_ffx_fsr2_tcr_autogen_pass_ee5cd9d13b7ae336c8f5a7336c8da15c_SampledImageResourceSets,
		4,
		g_ffx_fsr2_tcr_autogen_pass_ee5cd9d13b7ae336c8f5a7336c8da15c_StorageImageResourceNames,
		g_ffx_fsr2_tcr_autogen_pass_ee5cd9d13b7ae336c8f5a7336c8da15c_StorageImageResourceBindings,
		g_ffx_fsr2_tcr_autogen_pass_ee5cd9d13b7ae336c8f5a7336c8da15c_StorageImageResourceCounts,
		g_ffx_fsr2_tcr_autogen_pass_ee5cd9d13b7ae336c8f5a7336c8da15c_StorageImageResourceSets,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
		2,
		g_ffx_fsr2_tcr_autogen_pass_ee5cd9d13b7ae336c8f5a7336c8da15c_UniformBufferResourceNames,
		g_ffx_fsr2_tcr_autogen_pass_ee5cd9d13b7ae336c8f5a7336c8da15c_UniformBufferResourceBindings,
		g_ffx_fsr2_tcr_autogen_pass_ee5cd9d13b7ae336c8f5a7336c8da15c_UniformBufferResourceCounts,
		g_ffx_fsr2_tcr_autogen_pass_ee5cd9d13b7ae336c8f5a7336c8da15c_UniformBufferResourceSets,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
	},
	{
		g_ffx_fsr2_tcr_autogen_pass_e3d7ae37bc9e45800176cfd8d2b0cd40_size,
		g_ffx_fsr2_tcr_autogen_pass_e3d7ae37bc9e45800176cfd8d2b0cd40_data,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
		7,
		g_ffx_fsr2_tcr_autogen_pass_e3d7ae37bc9e45800176cfd8d2b0cd40_SampledImageResourceNames,
		g_ffx_fsr2_tcr_autogen_pass_e3d7ae37bc9e45800176cfd8d2b0cd40_SampledImageResourceBindings,
		g_ffx_fsr2_tcr_autogen_pass_e3d7ae37bc9e45800176cfd8d2b0cd40_SampledImageResourceCounts,
		g_ffx_fsr2_tcr_autogen_pass_e3d7ae37bc9e45800176cfd8d2b0cd40_SampledImageResourceSets,
		4,
		g_ffx_fsr2_tcr_autogen_pass_e3d7ae37bc9e45800176cfd8d2b0cd40_StorageImageResourceNames,
		g_ffx_fsr2_tcr_autogen_pass_e3d7ae37bc9e45800176cfd8d2b0cd40_StorageImageResourceBindings,
		g_ffx_fsr2_tcr_autogen_pass_e3d7ae37bc9e45800176cfd8d2b0cd40_StorageImageResourceCounts,
		g_ffx_fsr2_tcr_autogen_pass_e3d7ae37bc9e45800176cfd8d2b0cd40_StorageImageResourceSets,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
		2,
		g_ffx_fsr2_tcr_autogen_pass_e3d7ae37bc9e45800176cfd8d2b0cd40_UniformBufferResourceNames,
		g_ffx_fsr2_tcr_autogen_pass_e3d7ae37bc9e45800176cfd8d2b0cd40_UniformBufferResourceBindings,
		g_ffx_fsr2_tcr_autogen_pass_e3d7ae37bc9e45800176cfd8d2b0cd40_UniformBufferResourceCounts,
		g_ffx_fsr2_tcr_autogen_pass_e3d7ae37bc9e45800176cfd8d2b0cd40_UniformBufferResourceSets,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
	},
	{
		g_ffx_fsr2_tcr_autogen_pass_4ec8740b7257df5649bc59992fa35847_size,
		g_ffx_fsr2_tcr_autogen_pass_4ec8740b7257df5649bc59992fa35847_data,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
		7,
		g_ffx_fsr2_tcr_autogen_pass_4ec8740b7257df5649bc59992fa35847_SampledImageResourceNames,
		g_ffx_fsr2_tcr_autogen_pass_4ec8740b7257df5649bc59992fa35847_SampledImageResourceBindings,
		g_ffx_fsr2_tcr_autogen_pass_4ec8740b7257df5649bc59992fa35847_SampledImageResourceCounts,
		g_ffx_fsr2_tcr_autogen_pass_4ec8740b7257df5649bc59992fa35847_SampledImageResourceSets,
		4,
		g_ffx_fsr2_tcr_autogen_pass_4ec8740b7257df5649bc59992fa35847_StorageImageResourceNames,
		g_ffx_fsr2_tcr_autogen_pass_4ec8740b7257df5649bc59992fa35847_StorageImageResourceBindings,
		g_ffx_fsr2_tcr_autogen_pass_4ec8740b7257df5649bc59992fa35847_StorageImageResourceCounts,
		g_ffx_fsr2_tcr_autogen_pass_4ec8740b7257df5649bc59992fa35847_StorageImageResourceSets,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
		2,
		g_ffx_fsr2_tcr_autogen_pass_4ec8740b7257df5649bc59992fa35847_UniformBufferResourceNames,
		g_ffx_fsr2_tcr_autogen_pass_4ec8740b7257df5649bc59992fa35847_UniformBufferResourceBindings,
		g_ffx_fsr2_tcr_autogen_pass_4ec8740b7257df5649bc59992fa35847_UniformBufferResourceCounts,
		g_ffx_fsr2_tcr_autogen_pass_4ec8740b7257df5649bc59992fa35847_UniformBufferResourceSets,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
	},
	{
		g_ffx_fsr2_tcr_autogen_pass_bac77095e97869df4e2fdb361d116a63_size,
		g_ffx_fsr2_tcr_autogen_pass_bac77095e97869df4e2fdb361d116a63_data,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
		7,
		g_ffx_fsr2_tcr_autogen_pass_bac77095e97869df4e2fdb361d116a63_SampledImageResourceNames,
		g_ffx_fsr2_tcr_autogen_pass_bac77095e97869df4e2fdb361d116a63_SampledImageResourceBindings,
		g_ffx_fsr2_tcr_autogen_pass_bac77095e97869df4e2fdb361d116a63_SampledImageResourceCounts,
		g_ffx_fsr2_tcr_autogen_pass_bac77095e97869df4e2fdb361d116a63_SampledImageResourceSets,
		4,
		g_ffx_fsr2_tcr_autogen_pass_bac77095e97869df4e2fdb361d116a63_StorageImageResourceNames,
		g_ffx_fsr2_tcr_autogen_pass_bac77095e97869df4e2fdb361d116a63_StorageImageResourceBindings,
		g_ffx_fsr2_tcr_autogen_pass_bac77095e97869df4e2fdb361d116a63_StorageImageResourceCounts,
		g_ffx_fsr2_tcr_autogen_pass_bac77095e97869df4e2fdb361d116a63_StorageImageResourceSets,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
		2,
		g_ffx_fsr2_tcr_autogen_pass_bac77095e97869df4e2fdb361d116a63_UniformBufferResourceNames,
		g_ffx_fsr2_tcr_autogen_pass_bac77095e97869df4e2fdb361d116a63_UniformBufferResourceBindings,
		g_ffx_fsr2_tcr_autogen_pass_bac77095e97869df4e2fdb361d116a63_UniformBufferResourceCounts,
		g_ffx_fsr2_tcr_autogen_pass_bac77095e97869df4e2fdb361d116a63_UniformBufferResourceSets,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
		0, 0, 0, 0, 0,
	},
};