#include "ffx_fsr2_tcr_autogen_pass_5efedef945dd980072b7ea6e69371a56.h"
#include "ffx_fsr2_tcr_autogen_pass_c478176584a74a868ed20f22c18be43f.h"

typedef union ffx_fsr2_tcr_autogen_pass_PermutationKey {
    struct {
        uint32_t FFX_FSR2_OPTION_REPROJECT_USE_LANCZOS_TYPE : 1;
        uint32_t FFX_FSR2_OPTION_HDR_COLOR_INPUT : 1;
        uint32_t FFX_FSR2_OPTION_LOW_RESOLUTION_MOTION_VECTORS : 1;
        uint32_t FFX_FSR2_OPTION_JITTERED_MOTION_VECTORS : 1;
        uint32_t FFX_FSR2_OPTION_INVERTED_DEPTH : 1;
        uint32_t FFX_FSR2_OPTION_APPLY_SHARPENING : 1;
    };
    uint32_t index;
} ffx_fsr2_tcr_autogen_pass_PermutationKey;

typedef struct ffx_fsr2_tcr_autogen_pass_PermutationInfo {
    const uint32_t       blobSize;
    const unsigned char* blobData;


    const uint32_t  numCBVResources;
    const char**    cbvResourceNames;
    const uint32_t* cbvResourceBindings;
    const uint32_t* cbvResourceCounts;
    const uint32_t* cbvResourceSpaces;

    const uint32_t  numSRVResources;
    const char**    srvResourceNames;
    const uint32_t* srvResourceBindings;
    const uint32_t* srvResourceCounts;
    const uint32_t* srvResourceSpaces;

    const uint32_t  numUAVResources;
    const char**    uavResourceNames;
    const uint32_t* uavResourceBindings;
    const uint32_t* uavResourceCounts;
    const uint32_t* uavResourceSpaces;

    const uint32_t  numSamplerResources;
    const char**    samplerResourceNames;
    const uint32_t* samplerResourceBindings;
    const uint32_t* samplerResourceCounts;
    const uint32_t* samplerResourceSpaces;

    const uint32_t  numRTAccelerationStructureResources;
    const char**    rtAccelerationStructureResourceNames;
    const uint32_t* rtAccelerationStructureResourceBindings;
    const uint32_t* rtAccelerationStructureResourceCounts;
    const uint32_t* rtAccelerationStructureResourceSpaces;
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
};

static const ffx_fsr2_tcr_autogen_pass_PermutationInfo g_ffx_fsr2_tcr_autogen_pass_PermutationInfo[] = {
    { g_ffx_fsr2_tcr_autogen_pass_5efedef945dd980072b7ea6e69371a56_size, g_ffx_fsr2_tcr_autogen_pass_5efedef945dd980072b7ea6e69371a56_data, 2, g_ffx_fsr2_tcr_autogen_pass_5efedef945dd980072b7ea6e69371a56_CBVResourceNames, g_ffx_fsr2_tcr_autogen_pass_5efedef945dd980072b7ea6e69371a56_CBVResourceBindings, g_ffx_fsr2_tcr_autogen_pass_5efedef945dd980072b7ea6e69371a56_CBVResourceCounts, g_ffx_fsr2_tcr_autogen_pass_5efedef945dd980072b7ea6e69371a56_CBVResourceSpaces, 7, g_ffx_fsr2_tcr_autogen_pass_5efedef945dd980072b7ea6e69371a56_SRVResourceNames, g_ffx_fsr2_tcr_autogen_pass_5efedef945dd980072b7ea6e69371a56_SRVResourceBindings, g_ffx_fsr2_tcr_autogen_pass_5efedef945dd980072b7ea6e69371a56_SRVResourceCounts, g_ffx_fsr2_tcr_autogen_pass_5efedef945dd980072b7ea6e69371a56_SRVResourceSpaces, 4, g_ffx_fsr2_tcr_autogen_pass_5efedef945dd980072b7ea6e69371a56_UAVResourceNames, g_ffx_fsr2_tcr_autogen_pass_5efedef945dd980072b7ea6e69371a56_UAVResourceBindings, g_ffx_fsr2_tcr_autogen_pass_5efedef945dd980072b7ea6e69371a56_UAVResourceCounts, g_ffx_fsr2_tcr_autogen_pass_5efedef945dd980072b7ea6e69371a56_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    { g_ffx_fsr2_tcr_autogen_pass_c478176584a74a868ed20f22c18be43f_size, g_ffx_fsr2_tcr_autogen_pass_c478176584a74a868ed20f22c18be43f_data, 2, g_ffx_fsr2_tcr_autogen_pass_c478176584a74a868ed20f22c18be43f_CBVResourceNames, g_ffx_fsr2_tcr_autogen_pass_c478176584a74a868ed20f22c18be43f_CBVResourceBindings, g_ffx_fsr2_tcr_autogen_pass_c478176584a74a868ed20f22c18be43f_CBVResourceCounts, g_ffx_fsr2_tcr_autogen_pass_c478176584a74a868ed20f22c18be43f_CBVResourceSpaces, 7, g_ffx_fsr2_tcr_autogen_pass_c478176584a74a868ed20f22c18be43f_SRVResourceNames, g_ffx_fsr2_tcr_autogen_pass_c478176584a74a868ed20f22c18be43f_SRVResourceBindings, g_ffx_fsr2_tcr_autogen_pass_c478176584a74a868ed20f22c18be43f_SRVResourceCounts, g_ffx_fsr2_tcr_autogen_pass_c478176584a74a868ed20f22c18be43f_SRVResourceSpaces, 4, g_ffx_fsr2_tcr_autogen_pass_c478176584a74a868ed20f22c18be43f_UAVResourceNames, g_ffx_fsr2_tcr_autogen_pass_c478176584a74a868ed20f22c18be43f_UAVResourceBindings, g_ffx_fsr2_tcr_autogen_pass_c478176584a74a868ed20f22c18be43f_UAVResourceCounts, g_ffx_fsr2_tcr_autogen_pass_c478176584a74a868ed20f22c18be43f_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
};

