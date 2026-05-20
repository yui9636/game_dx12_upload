// Deferred decal vertex shader.
// Emits a unit cube [-0.5,0.5]^3 from SV_VertexID alone (no vertex buffer needed).
// The cube is the projection volume; the pixel shader does the actual decal projection.

cbuffer CbDecal : register(b1)
{
    float4x4 decalWorldViewProj; // unit cube -> clip space
    float4x4 worldToDecal;       // world space -> box-local [-0.5,0.5]
    float4   tintOpacity;        // rgb = tint, a = opacity
    float4   params;             // x = angleFade, y = invWidth, z = invHeight
    float4   decalAxisWS;        // xyz = decal projection axis (box local +Z) in world
};

static const float3 kCorners[8] = {
    float3(-0.5f, -0.5f, -0.5f),
    float3( 0.5f, -0.5f, -0.5f),
    float3( 0.5f,  0.5f, -0.5f),
    float3(-0.5f,  0.5f, -0.5f),
    float3(-0.5f, -0.5f,  0.5f),
    float3( 0.5f, -0.5f,  0.5f),
    float3( 0.5f,  0.5f,  0.5f),
    float3(-0.5f,  0.5f,  0.5f),
};

static const uint kIndices[36] = {
    0, 1, 2,  0, 2, 3, // -Z
    5, 4, 7,  5, 7, 6, // +Z
    4, 0, 3,  4, 3, 7, // -X
    1, 5, 6,  1, 6, 2, // +X
    3, 2, 6,  3, 6, 7, // +Y
    4, 5, 1,  4, 1, 0, // -Y
};

struct VSOut { float4 position : SV_POSITION; };

VSOut main(uint vertexId : SV_VertexID)
{
    VSOut o;
    float3 corner = kCorners[kIndices[vertexId]];
    o.position = mul(float4(corner, 1.0f), decalWorldViewProj);
    return o;
}
