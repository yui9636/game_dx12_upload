// ============================================================================
// Billboard GS (SoA): Reads from g_AliveList -> Hot + Warm streams
// Expands point to camera-facing quad with velocity stretch
// ============================================================================

#include "compute_particle.hlsli"
#include "EffectParticleSoA.hlsli"

StructuredBuffer<uint>           g_AliveList      : register(t0);
StructuredBuffer<BillboardHot>   g_BillboardHot   : register(t2);
StructuredBuffer<BillboardWarm>  g_BillboardWarm  : register(t3);

float4x4 matrix_scaling(float3 scale)
{
    float4x4 m = (float4x4)0;
    m._11 = scale.x; m._22 = scale.y; m._33 = scale.z; m._44 = 1.0f;
    return m;
}

float4x4 matrix_rotation_z(float angle)
{
    float c, s;
    sincos(angle, s, c);
    float4x4 m = (float4x4)0;
    m._11 = c;  m._12 = s;
    m._21 = -s; m._22 = c;
    m._33 = 1.0f; m._44 = 1.0f;
    return m;
}

float4x4 matrix_translation(float3 t)
{
    float4x4 m = (float4x4)0;
    m._11 = 1.0f; m._22 = 1.0f; m._33 = 1.0f; m._44 = 1.0f;
    m._41 = t.x; m._42 = t.y; m._43 = t.z;
    return m;
}

[maxvertexcount(4)]
void main(point GS_IN gin[1], inout TriangleStream<PS_IN> output)
{
    uint vertexId = gin[0].vertex_id;
    uint slot = g_AliveList[vertexId];

    // ── Read SoA streams ──
    BillboardHot hot = g_BillboardHot[slot];
    BillboardWarm warm = g_BillboardWarm[slot];

    float2 sizeSpin = UnpackHalf2(hot.sizeSpin);
    float currentSize = sizeSpin.x;
    float spinAngle = sizeSpin.y;

    if (currentSize <= 0.0f) return;

    float3 scale = float3(currentSize, currentSize, currentSize);
    float4 color = UnpackRGBA8(warm.packedColor);
    float2 texUV = UnpackHalf2(warm.texcoordPacked);

    // Billboard matrix (inverse view, kill translation)
    float4x4 billboard_matrix = inverseviewProjection;
    billboard_matrix._41_42_43 = float3(0, 0, 0);
    billboard_matrix._44 = 1.0f;

    // Velocity stretch
    float additional_roll = 0.0f;
    if (enable_velocity_stretch != 0)
    {
        float speed = length(hot.velocity);
        float3 camera_right = normalize(float3(billboard_matrix._11, billboard_matrix._12, billboard_matrix._13));
        float3 camera_up = normalize(float3(billboard_matrix._21, billboard_matrix._22, billboard_matrix._23));
        float vx = dot(hot.velocity, camera_right);
        float vy = dot(hot.velocity, camera_up);
        additional_roll = (abs(vx) + abs(vy) > 1e-6f) ? atan2(vx, vy) : 0.0f;
        float k = max(0.0f, speed - velocity_stretch_min_speed);
        float stretchFactor = min(velocity_stretch_max_aspect, 1.0f + k * velocity_stretch_scale);
        scale.y *= stretchFactor;
    }

    // Transform
    float4x4 scale_matrix = matrix_scaling(scale);
    float4x4 rotation_matrix = mul(billboard_matrix, matrix_rotation_z(spinAngle + additional_roll));
    float4x4 translation_matrix = matrix_translation(hot.position);
    float4x4 wvp = mul(mul(scale_matrix, rotation_matrix), mul(translation_matrix, viewProjection));

    // Sub-UV rect
    // texUV.xy = atlas origin, compute_particle uses texcoord.zw = frame size
    // For now pass through (sub-UV rect handled in update)
    float4 texcoord = float4(texUV.x, texUV.y, 1.0f, 1.0f);

    static const float4 vertex_positions[4] =
    {
        float4(-0.5f, -0.5f, 0, 1),
        float4(+0.5f, -0.5f, 0, 1),
        float4(-0.5f, +0.5f, 0, 1),
        float4(+0.5f, +0.5f, 0, 1),
    };
    static const float2 vertex_texcoord[4] =
    {
        float2(0, 0), float2(1, 0), float2(0, 1), float2(1, 1),
    };

    [unroll]
    for (uint i = 0; i < 4; ++i)
    {
        PS_IN o;
        o.position = mul(vertex_positions[i], wvp);
        o.texcoord = texcoord.xy + texcoord.zw * vertex_texcoord[i];
        o.color = color;
        output.Append(o);
    }
    output.RestartStrip();
}
