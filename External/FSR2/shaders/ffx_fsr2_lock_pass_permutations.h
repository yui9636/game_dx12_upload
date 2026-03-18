#include "ffx_fsr2_lock_pass_f98a0261140e91f0b33483aae03d4baa.h"
#include "ffx_fsr2_lock_pass_c86c0517229fa67cecf7a66b2be9d4b0.h"

typedef union ffx_fsr2_lock_pass_PermutationKey {
    struct {
        uint32_t FFX_FSR2_OPTION_REPROJECT_USE_LANCZOS_TYPE : 1;
        uint32_t FFX_FSR2_OPTION_HDR_COLOR_INPUT : 1;
        uint32_t FFX_FSR2_OPTION_LOW_RESOLUTION_MOTION_VECTORS : 1;
        uint32_t FFX_FSR2_OPTION_JITTERED_MOTION_VECTORS : 1;
        uint32_t FFX_FSR2_OPTION_INVERTED_DEPTH : 1;
        uint32_t FFX_FSR2_OPTION_APPLY_SHARPENING : 1;
    };
    uint32_t index;
} ffx_fsr2_lock_pass_PermutationKey;

typedef struct ffx_fsr2_lock_pass_PermutationInfo {
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
} ffx_fsr2_lock_pass_PermutationInfo;

static const uint32_t g_ffx_fsr2_lock_pass_IndirectionTable[] = {
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
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
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

static const ffx_fsr2_lock_pass_PermutationInfo g_ffx_fsr2_lock_pass_PermutationInfo[] = {
    { g_ffx_fsr2_lock_pass_f98a0261140e91f0b33483aae03d4baa_size, g_ffx_fsr2_lock_pass_f98a0261140e91f0b33483aae03d4baa_data, 1, g_ffx_fsr2_lock_pass_f98a0261140e91f0b33483aae03d4baa_CBVResourceNames, g_ffx_fsr2_lock_pass_f98a0261140e91f0b33483aae03d4baa_CBVResourceBindings, g_ffx_fsr2_lock_pass_f98a0261140e91f0b33483aae03d4baa_CBVResourceCounts, g_ffx_fsr2_lock_pass_f98a0261140e91f0b33483aae03d4baa_CBVResourceSpaces, 1, g_ffx_fsr2_lock_pass_f98a0261140e91f0b33483aae03d4baa_SRVResourceNames, g_ffx_fsr2_lock_pass_f98a0261140e91f0b33483aae03d4baa_SRVResourceBindings, g_ffx_fsr2_lock_pass_f98a0261140e91f0b33483aae03d4baa_SRVResourceCounts, g_ffx_fsr2_lock_pass_f98a0261140e91f0b33483aae03d4baa_SRVResourceSpaces, 2, g_ffx_fsr2_lock_pass_f98a0261140e91f0b33483aae03d4baa_UAVResourceNames, g_ffx_fsr2_lock_pass_f98a0261140e91f0b33483aae03d4baa_UAVResourceBindings, g_ffx_fsr2_lock_pass_f98a0261140e91f0b33483aae03d4baa_UAVResourceCounts, g_ffx_fsr2_lock_pass_f98a0261140e91f0b33483aae03d4baa_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    { g_ffx_fsr2_lock_pass_c86c0517229fa67cecf7a66b2be9d4b0_size, g_ffx_fsr2_lock_pass_c86c0517229fa67cecf7a66b2be9d4b0_data, 1, g_ffx_fsr2_lock_pass_c86c0517229fa67cecf7a66b2be9d4b0_CBVResourceNames, g_ffx_fsr2_lock_pass_c86c0517229fa67cecf7a66b2be9d4b0_CBVResourceBindings, g_ffx_fsr2_lock_pass_c86c0517229fa67cecf7a66b2be9d4b0_CBVResourceCounts, g_ffx_fsr2_lock_pass_c86c0517229fa67cecf7a66b2be9d4b0_CBVResourceSpaces, 1, g_ffx_fsr2_lock_pass_c86c0517229fa67cecf7a66b2be9d4b0_SRVResourceNames, g_ffx_fsr2_lock_pass_c86c0517229fa67cecf7a66b2be9d4b0_SRVResourceBindings, g_ffx_fsr2_lock_pass_c86c0517229fa67cecf7a66b2be9d4b0_SRVResourceCounts, g_ffx_fsr2_lock_pass_c86c0517229fa67cecf7a66b2be9d4b0_SRVResourceSpaces, 2, g_ffx_fsr2_lock_pass_c86c0517229fa67cecf7a66b2be9d4b0_UAVResourceNames, g_ffx_fsr2_lock_pass_c86c0517229fa67cecf7a66b2be9d4b0_UAVResourceBindings, g_ffx_fsr2_lock_pass_c86c0517229fa67cecf7a66b2be9d4b0_UAVResourceCounts, g_ffx_fsr2_lock_pass_c86c0517229fa67cecf7a66b2be9d4b0_UAVResourceSpaces, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
};

