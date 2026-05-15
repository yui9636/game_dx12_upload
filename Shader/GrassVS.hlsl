cbuffer GrassCB : register(b0)
{
    float4x4 viewProj;
    float4x4 viewProjectionUnjittered;
    float4x4 prevViewProjection;
    float4   windDirSpeed;          // xyz = wind dir (normalized), w = speed
    float4   windStrengthTime;      // x = strength, y = time
    float4   colorBottom;           // rgb = bottom color, w unused
    float4   colorTop;              // rgb = top color, w unused
    float4   cameraPosition;
};

// Per-vertex (slot 0)
struct VS_INPUT_VERTEX
{
    float3 position : POSITION;
    float2 uv       : TEXCOORD0;
};

// Per-instance (slot 1)
struct VS_INPUT_INSTANCE
{
    float3 instancePos    : TEXCOORD1;   // worldPos
    float  instanceScale  : TEXCOORD2;
    float3 instanceTint   : TEXCOORD3;
    float  instanceRotY   : TEXCOORD4;
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

    float sR, cR;
    sincos(inst.instanceRotY, sR, cR);

    // Local quad position -> scaled
    float3 localScaled = float3(v.position.x * inst.instanceScale,
                                v.position.y * inst.instanceScale,
                                v.position.z * inst.instanceScale);
    // Rotate around Y
    float3 rotated;
    rotated.x = localScaled.x * cR + localScaled.z * sR;
    rotated.y = localScaled.y;
    rotated.z = -localScaled.x * sR + localScaled.z * cR;

    // Translate to instance world position
    float3 worldPos = rotated + inst.instancePos;

    // --- Wind animation (only top vertices bend) ---
    // v.position.y in [0..1] (mesh authored as unit height)
    float topFactor = saturate(v.position.y);  // 0 at root, 1 at tip
    float time = windStrengthTime.y;
    float windSpeed = windDirSpeed.w;
    float windStrength = windStrengthTime.x;
    // 風相 = instance pos に基づくハッシュ + 時刻
    float phaseHash = dot(inst.instancePos.xz, float2(0.137f, 0.249f));
    float phase = phaseHash + time * windSpeed;
    float wave  = sin(phase) * 0.6f + sin(phase * 2.13f + 1.7f) * 0.4f;
    float3 windDir = normalize(float3(windDirSpeed.x, 0.0f, windDirSpeed.z + 0.0001f));
    float bend = wave * windStrength * topFactor * inst.instanceScale;
    worldPos += windDir * bend;
    // Slight vertical squash when bending heavily (preserve length)
    worldPos.y -= abs(bend) * 0.18f * topFactor;

    output.position    = mul(float4(worldPos, 1.0f), viewProj);
    output.worldPos    = worldPos;
    output.uv          = v.uv;
    // Y=0 (root) -> colorBottom, Y=1 (tip) -> colorTop. UV.y is 1 at root, 0 at top.
    float topT = 1.0f - v.uv.y;
    float3 grad = lerp(colorBottom.rgb, colorTop.rgb, topT);
    output.vertexTint = grad * inst.instanceTint;

    // Approximate normal: face roughly upward + slight outward to give some directionality.
    // Use rotated local right (perpendicular to blade plane). Then average with up.
    float3 bladeRight = float3(cR, 0.0f, -sR);
    float3 bladeUp    = float3(0.0f, 1.0f, 0.0f);
    output.normal = normalize(bladeUp * 0.7f + bladeRight * 0.3f);

    output.curClipPos  = mul(float4(worldPos, 1.0f), viewProjectionUnjittered);
    output.prevClipPos = mul(float4(worldPos - windDir * bend, 1.0f), prevViewProjection);
    return output;
}
