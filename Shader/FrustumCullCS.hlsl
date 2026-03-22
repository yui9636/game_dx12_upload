// GPU Frustum Culling Compute Shader — 2D Dispatch
// X axis: instance index within command
// Y axis: command index (no linear search)
#include "IndirectDrawCommon.hlsli"

cbuffer CullingParams : register(b0) {
    float4 frustumPlanes[6];
    uint   commandCount;
    uint   maxInstancesPerCmd;
    uint2  pad;
};

struct InstanceData {
    float4x4 worldMatrix;
    float4x4 prevWorldMatrix;
};

struct CullCommandMeta {
    uint  firstInstance;
    uint  instanceCount;
    uint  outputInstanceStart;
    uint  drawArgsIndex;
    uint  indexCount;
    int   baseVertex;
    float boundsCenterX, boundsCenterY, boundsCenterZ;
    float boundsRadius;
    uint2 metaPad;
};

StructuredBuffer<InstanceData>    inputInstances  : register(t0);
StructuredBuffer<CullCommandMeta> commands        : register(t1);
RWStructuredBuffer<InstanceData>  outputInstances : register(u0);
RWByteAddressBuffer               drawArgsBuffer  : register(u1);

// Transform local-space bounds to world space
void TransformBounds(
    float3 localCenter, float localRadius,
    float4x4 world,
    out float3 worldCenter, out float worldRadius)
{
    worldCenter = mul(float4(localCenter, 1.0), world).xyz;

    float sx = length(float3(world._11, world._12, world._13));
    float sy = length(float3(world._21, world._22, world._23));
    float sz = length(float3(world._31, world._32, world._33));
    worldRadius = localRadius * max(sx, max(sy, sz));
}

// Frustum sphere test
bool IsVisible(float3 worldCenter, float worldRadius)
{
    for (int p = 0; p < 6; p++) {
        float dist = dot(frustumPlanes[p].xyz, worldCenter) + frustumPlanes[p].w;
        if (dist < -worldRadius) return false;
    }
    return true;
}

[numthreads(CULL_THREAD_GROUP_SIZE, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint3 threadId : SV_GroupThreadID)
{
    uint cmdIdx   = groupId.y;
    uint localIdx = groupId.x * CULL_THREAD_GROUP_SIZE + threadId.x;

    if (cmdIdx >= commandCount) return;

    CullCommandMeta cmd = commands[cmdIdx];

    if (localIdx >= cmd.instanceCount) return;

    uint inputIdx = cmd.firstInstance + localIdx;

    // World-space bounds
    float4x4 world = inputInstances[inputIdx].worldMatrix;
    float3 localCenter = float3(cmd.boundsCenterX, cmd.boundsCenterY, cmd.boundsCenterZ);
    float3 worldCenter;
    float  worldRadius;
    TransformBounds(localCenter, cmd.boundsRadius, world, worldCenter, worldRadius);

    if (!IsVisible(worldCenter, worldRadius)) return;

    // Visible: atomic increment instanceCount in DrawArgs
    uint drawArgsOffset = cmd.drawArgsIndex * DRAW_ARGS_STRIDE
                        + DRAW_ARGS_INSTANCE_COUNT_OFFSET;
    uint outLocalIdx;
    drawArgsBuffer.InterlockedAdd(drawArgsOffset, 1, outLocalIdx);

    // Write to output buffer
    uint writePos = cmd.outputInstanceStart + outLocalIdx;
    outputInstances[writePos] = inputInstances[inputIdx];
}
