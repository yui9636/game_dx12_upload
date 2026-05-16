cbuffer GrassCB : register(b0)
{
    float4x4 viewProj;
    float4x4 viewProjectionUnjittered;
    float4x4 prevViewProjection;
    float4   windDirSpeed;          // xyz = wind dir (normalized), w = speed
    float4   windStrengthTime;      // x = strength, y = time, z = alphaCutoff
    float4   colorBottom;
    float4   colorTop;
    float4   cameraPosition;
    float4   meshLocalMin;          // xyz = mesh local min
    float4   meshLocalMax;          // xyz = mesh local max
};

// Per-vertex (slot 0) - matches engine Model::Vertex layout, fields we use.
struct VS_INPUT_VERTEX
{
    float3 position : POSITION;
    float2 uv       : TEXCOORD0;
    float3 normal   : NORMAL;
};

// Per-instance (slot 1): packed as two float4
//   instanceA.xyz = worldPos, instanceA.w = scale
//   instanceB.xyz = colorTint, instanceB.w = rotationY
struct VS_INPUT_INSTANCE
{
    float4 instanceA : TEXCOORD1;
    float4 instanceB : TEXCOORD2;
};

struct VS_OUTPUT
{
    float4 position    : SV_Position;
    float3 worldPos    : TEXCOORD0;
    float2 uv          : TEXCOORD1;
    float3 vertexTint  : TEXCOORD2;   // colorBottom..colorTop blended by Y, multiplied by instanceTint
    float3 normal      : TEXCOORD3;
    float4 curClipPos  : TEXCOORD4;
    float4 prevClipPos : TEXCOORD5;
};

VS_OUTPUT main(VS_INPUT_VERTEX v, VS_INPUT_INSTANCE inst)
{
    VS_OUTPUT output;
    float3 instancePos   = inst.instanceA.xyz;
    float  instanceScale = inst.instanceA.w;
    float3 instanceTint  = inst.instanceB.xyz;
    float  instanceRotY  = inst.instanceB.w;

    float sR, cR;
    sincos(instanceRotY, sR, cR);

    // Normalize mesh local space using the LARGEST axis so artist proportions are
    // preserved (flat flowers stay flat, tall blades stay tall). instanceScale ends
    // up being the size of the largest dimension in world meters.
    float3 meshSize   = max(meshLocalMax.xyz - meshLocalMin.xyz, float3(0.001f, 0.001f, 0.001f));
    float3 meshCenter = (meshLocalMax.xyz + meshLocalMin.xyz) * 0.5f;
    float  maxDim     = max(max(meshSize.x, meshSize.y), meshSize.z);
    float3 normLocal;
    normLocal.x = (v.position.x - meshCenter.x) / maxDim;
    normLocal.y = (v.position.y - meshLocalMin.y) / maxDim;  // start at 0 (root at ground)
    normLocal.z = (v.position.z - meshCenter.z) / maxDim;

    float3 localScaled = normLocal * instanceScale;
    // Rotate around Y
    float3 rotated;
    rotated.x = localScaled.x * cR + localScaled.z * sR;
    rotated.y = localScaled.y;
    rotated.z = -localScaled.x * sR + localScaled.z * cR;

    // Translate to instance world position
    float3 worldPos = rotated + instancePos;

    // --- Wind animation (only top vertices bend) ---
    // Normalized Y is in [0..1] from root to tip.
    float topFactor = saturate(normLocal.y);  // 0 at root, 1 at tip
    float time = windStrengthTime.y;
    float windSpeed = windDirSpeed.w;
    float windStrength = windStrengthTime.x;
    // 風相 = instance pos に基づくハッシュ + 時刻
    float phaseHash = dot(instancePos.xz, float2(0.137f, 0.249f));
    float phase = phaseHash + time * windSpeed;
    float wave  = sin(phase) * 0.6f + sin(phase * 2.13f + 1.7f) * 0.4f;
    float3 windDir = normalize(float3(windDirSpeed.x, 0.0f, windDirSpeed.z + 0.0001f));
    float bend = wave * windStrength * topFactor * instanceScale;
    worldPos += windDir * bend;
    // Slight vertical squash when bending heavily (preserve length)
    worldPos.y -= abs(bend) * 0.18f * topFactor;

    output.position    = mul(float4(worldPos, 1.0f), viewProj);
    output.worldPos    = worldPos;
    output.uv          = v.uv;
    // Color gradient driven by normalized Y (0=root, 1=tip).
    float3 grad = lerp(colorBottom.rgb, colorTop.rgb, normLocal.y);
    output.vertexTint = grad * instanceTint;

    // Rotate the model normal around Y by the instance rotation (so the
    // grass blade silhouette gets a per-instance facing direction).
    float3 rotN;
    rotN.x = v.normal.x * cR + v.normal.z * sR;
    rotN.y = v.normal.y;
    rotN.z = -v.normal.x * sR + v.normal.z * cR;
    // Bias slightly upward so flat planes still receive lighting from above.
    output.normal = normalize(rotN + float3(0.0f, 0.5f, 0.0f));

    output.curClipPos  = mul(float4(worldPos, 1.0f), viewProjectionUnjittered);
    output.prevClipPos = mul(float4(worldPos - windDir * bend, 1.0f), prevViewProjection);
    return output;
}
