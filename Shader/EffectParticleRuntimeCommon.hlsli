#define _COMPUTE_PARTICLE_DISABLE_CBSCENE_
#include "compute_particle.hlsli"

// まだ SoA v3 レイアウトへ移行していないシェーダ用の旧 counter offset。
static const uint EffectParticleCounterDeadCountOffset = 0;
static const uint EffectParticleCounterActiveCountOffset = 4;
static const uint EffectParticleCounterEmitCountOffset = 8;
static const uint EffectParticleCounterKillCountOffset = 12;
static const uint EffectParticleCounterOverflowCountOffset = 16;
static const float EffectParticleDeadDepth = -1000000.0f;

static const uint EffectParticleRibbonHistoryLength = 8u;

cbuffer EffectParticleSimulationParams : register(b0)
{
    float4 gOriginCurrentTime;
    float4 gTint;
    float4 gTintEnd;
    float4 gCameraPositionSortSign;
    float4 gCameraDirectionCapacity;
    float4 gAccelerationDrag;
    float4 gShapeParametersSizeBias;
    float4 gShapeTypeSpinAlphaBias;
    float4 gTiming;
    float4 gSizeSeed;
    float4 gSubUvParams;
    float4 gMotionParams;
    float4 gRandomParams;      // x は speedRange、y は sizeRange、z は lifeRange、w は windStrength。
    float4 gWindDirection;     // xyz は direction、w は turbulence。
    // フェーズ 1C
    float4 gSizeCurveValues;   // s0,s1,s2,s3
    float4 gSizeCurveTimes;    // t0,t1,t2,t3
    float4 gGradientColor0;    // t=0 の RGBA
    float4 gGradientColor1;    // t=t1 の RGBA
    float4 gGradientColor2;    // t=t2 の RGBA
    float4 gGradientColor3;    // t=1 の RGBA
    float4 gGradientTimes;     // t0,t1,t2,t3
    // フェーズ 2: Attractor
    float4 gAttractor0;        // xyz は position、w は strength。
    float4 gAttractor1;
    float4 gAttractor2;
    float4 gAttractor3;
    float4 gAttractorRadii;    // attractor ごとの radius。
    float4 gAttractorFalloff;  // 0 は constant、1 は linear、2 は quadratic。
    // フェーズ 2: コリジョン
    float4 gCollisionPlane;    // normal.xyz と d を保持する。
    float4 gCollisionSphere0;  // xyz は center、w は radius。
    float4 gCollisionSphere1;
    float4 gCollisionSphere2;
    float4 gCollisionSphere3;
    float4 gCollisionParams;   // x は restitution、y は friction、z は sphereCount、w は attractorCount。
    // メッシュパーティクル設定（gMeshFlags.x != 0 のときだけ使用）
    float4 gMeshInitialScale;        // xyz は scale、w は scaleRandomRange。
    float4 gMeshAngularAxisSpeed;    // xyz は正規化済み angularAxis、w は angularSpeed の rad/s。
    float4 gMeshAngularRandomOrient; // xyz は yaw / pitch / roll の乱数範囲、w は speed の乱数範囲。
    float4 gMeshFlags;               // x=isMeshMode。0 または 1。, yzw=reserved
}

Texture3D<float4> gCurlNoiseTexture : register(t1);
SamplerState gCurlNoiseSampler : register(s0);

float Hash01(uint value)
{
    value ^= value >> 17;
    value *= 0xED5AD4BBu;
    value ^= value >> 11;
    value *= 0xAC4C1B51u;
    value ^= value >> 15;
    value *= 0x31848BABu;
    value ^= value >> 14;
    return (value & 0x00FFFFFFu) / 16777215.0f;
}

float3 RandomDirection(uint index, uint seed)
{
    const float rand0 = Hash01(index * 9781u + seed * 6271u + 17u);
    const float rand1 = Hash01(index * 6271u + seed * 9781u + 53u);
    const float angle = rand0 * 6.2831853f;
    const float z = rand1 * 2.0f - 1.0f;
    const float r = sqrt(saturate(1.0f - z * z));
    return float3(cos(angle) * r, z, sin(angle) * r);
}

float4 ComputeSubUvRect(float ageSeconds, float totalLife)
{
    const uint columns = max(1u, (uint)(gSubUvParams.x + 0.5f));
    const uint rows = max(1u, (uint)(gSubUvParams.y + 0.5f));
    const uint totalFrames = columns * rows;
    if (totalFrames <= 1u) {
        return float4(0.0f, 0.0f, 1.0f, 1.0f);
    }

    const float frameRate = max(gSubUvParams.z, 0.0f);
    const float normalizedAge = saturate(totalLife > 0.0f ? (ageSeconds / totalLife) : 0.0f);
    uint frameIndex = 0u;
    frameIndex = min((uint)floor(normalizedAge * (float)totalFrames), totalFrames - 1u);
    if (frameRate > 0.0f) {
        frameIndex = min((uint)floor(max(ageSeconds, 0.0f) * frameRate), totalFrames - 1u);
    }

    const uint column = frameIndex % columns;
    const uint row = frameIndex / columns;
    const float2 frameScale = float2(1.0f / (float)columns, 1.0f / (float)rows);
    return float4((float)column * frameScale.x, (float)row * frameScale.y, frameScale.x, frameScale.y);
}

float EaseOutCubic(float t)
{
    t = saturate(t);
    return 1.0f - pow(1.0f - t, 3.0f);
}

float Hash31(float3 p)
{
    return frac(sin(dot(p, float3(12.9898f, 78.233f, 37.719f))) * 43758.5453f);
}

float3 SamplePseudoNoise3(float3 p)
{
    return float3(
        Hash31(p + float3(17.0f, 31.0f, 47.0f)) * 2.0f - 1.0f,
        Hash31(p + float3(53.0f, 67.0f, 79.0f)) * 2.0f - 1.0f,
        Hash31(p + float3(89.0f, 97.0f, 113.0f)) * 2.0f - 1.0f);
}

float3 SampleCurlNoiseVolume(float3 worldPosition, float ageSeconds)
{
    const float curlNoiseScale = max(gMotionParams.y, 0.01f);
    const float curlScrollSpeed = max(gMotionParams.z, 0.0f);
    const float3 uvw = worldPosition * curlNoiseScale + float3(0.0f, ageSeconds * curlScrollSpeed, 0.0f);
    return gCurlNoiseTexture.SampleLevel(gCurlNoiseSampler, uvw, 0.0f).xyz;
}

float ComputeBiasedAge(float normalizedAge, float bias)
{
    return saturate(pow(saturate(normalizedAge), max(bias, 0.05f)));
}

// フェーズ 1C: 4 キーサイズカーブ評価
float EvaluateSizeCurve(float normalizedAge)
{
    float t = saturate(normalizedAge);
    float4 sv = gSizeCurveValues;
    float4 st = gSizeCurveTimes;

    if (t <= st.y) {
        float segT = (st.y > st.x) ? saturate((t - st.x) / (st.y - st.x)) : 0.0f;
        return lerp(sv.x, sv.y, segT);
    }
    if (t <= st.z) {
        float segT = (st.z > st.y) ? saturate((t - st.y) / (st.z - st.y)) : 0.0f;
        return lerp(sv.y, sv.z, segT);
    }
    float segT = (st.w > st.z) ? saturate((t - st.z) / (st.w - st.z)) : 0.0f;
    return lerp(sv.z, sv.w, segT);
}

// フェーズ 1C: 4 キー色グラデーション評価
float4 EvaluateColorGradient(float normalizedAge)
{
    float t = saturate(normalizedAge);
    float4 gt = gGradientTimes;

    if (t <= gt.y) {
        float segT = (gt.y > gt.x) ? saturate((t - gt.x) / (gt.y - gt.x)) : 0.0f;
        return lerp(gGradientColor0, gGradientColor1, segT);
    }
    if (t <= gt.z) {
        float segT = (gt.z > gt.y) ? saturate((t - gt.y) / (gt.z - gt.y)) : 0.0f;
        return lerp(gGradientColor1, gGradientColor2, segT);
    }
    float segT = (gt.w > gt.z) ? saturate((t - gt.z) / (gt.w - gt.z)) : 0.0f;
    return lerp(gGradientColor2, gGradientColor3, segT);
}

float3 ComputeSpawnOffset(uint emitIndex, uint seed, uint shapeType, float3 shapeParams, out float3 direction)
{
    const float rand0 = Hash01(emitIndex * 9781u + seed * 6271u + 17u);
    const float rand1 = Hash01(emitIndex * 6271u + seed * 9781u + 53u);
    const float rand2 = Hash01(emitIndex * 2371u + seed * 3253u + 97u);
    const float angle = rand0 * 6.2831853f;

    float3 offset = float3(0.0f, 0.0f, 0.0f);
    direction = float3(0.0f, 1.0f, 0.0f);

    if (shapeType == 1u) {
        const float radius = max(shapeParams.x, 0.001f);
        offset = RandomDirection(emitIndex, seed) * radius * lerp(0.15f, 1.0f, rand1);
        direction = normalize(offset + float3(0.0f, radius * 0.5f, 0.0f));
    } else if (shapeType == 2u) {
        const float3 extents = max(shapeParams.xyz, float3(0.01f, 0.01f, 0.01f));
        offset = float3(
            lerp(-extents.x, extents.x, rand0),
            lerp(-extents.y, extents.y, rand1),
            lerp(-extents.z, extents.z, rand2));
        direction = normalize(float3(offset.x, abs(offset.y) + 0.25f, offset.z));
    } else if (shapeType == 3u) {
        const float angleDeg = max(shapeParams.x, 1.0f);
        const float baseRadius = max(shapeParams.y, 0.05f);
        const float coneHeight = max(shapeParams.z, 0.05f);
        const float coneAngle = radians(angleDeg);
        const float coneRadius = tan(coneAngle) * coneHeight;
        const float radius = lerp(baseRadius * 0.1f, coneRadius, rand1);
        offset = float3(cos(angle) * radius, 0.0f, sin(angle) * radius);
        direction = normalize(float3(cos(angle) * coneRadius, coneHeight, sin(angle) * coneRadius));
    } else if (shapeType == 4u) {
        const float radius = max(shapeParams.x, 0.05f);
        offset = float3(cos(angle) * radius, 0.0f, sin(angle) * radius);
        direction = normalize(float3(offset.x * 0.25f, 1.0f, offset.z * 0.25f));
    } else if (shapeType == 5u) {
        const float halfLength = max(shapeParams.x, 0.05f);
        offset = float3(lerp(-halfLength, halfLength, rand0), 0.0f, 0.0f);
        direction = normalize(float3(0.0f, 1.0f, lerp(-0.35f, 0.35f, rand1)));
    } else if (shapeType == 6u) {
        // Ring エミッター: XZ 平面の固定半径の円周上にスポーンする。
        // shapeParams.x = リングの半径。
        // 接線速度は呼び出し側（CSMain）が vortexStrength を使って別途加算する。
        // ここでは direction を接線方向（旋回方向）に向けておく。
        // vortexStrength が正なら反時計回り（+Y 軸まわり）で旋回する。
        const float ringRadius = max(shapeParams.x, 0.05f);
        offset = float3(cos(angle) * ringRadius, 0.0f, sin(angle) * ringRadius);
        // 接線ベクトル: 円周上の点を微分すると (-sin, 0, cos) になる。
        const float3 tangent = float3(-sin(angle), 0.0f, cos(angle));
        // vortexStrength の符号で旋回方向を決め、速度ベクトルに接線方向を合成する。
        // direction はここでは「スポーン後の初速方向」として使われる。
        // 放射状の拡散（外向き）と接線方向を混合し、純粋な環状フローにする。
        const float vortex = gMotionParams.w; // vortexStrength
        const float3 radial = normalize(float3(offset.x, 0.0f, offset.z));
        if (abs(vortex) > 0.001f) {
            // vortex が強いほど接線方向を優先する。saturate で 0-1 にクランプ。
            const float tangentWeight = saturate(abs(vortex) / (abs(vortex) + 1.0f));
            direction = normalize(lerp(radial + float3(0.0f, 0.5f, 0.0f),
                                       tangent * sign(vortex) + float3(0.0f, 0.1f, 0.0f),
                                       tangentWeight));
        } else {
            // vortex なし: 外向きに放射。
            direction = normalize(radial + float3(0.0f, 0.35f, 0.0f));
        }
    } else {
        const float radial = lerp(0.2f, 0.85f, rand1);
        const float lift = lerp(0.35f, 1.0f, rand2);
        direction = normalize(float3(cos(angle) * radial, lift, sin(angle) * radial));
    }

    return offset;
}
