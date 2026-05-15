// 地形 GBuffer 出力シェーダー。3 レイヤースプラット + PBR + Height-blend + Triplanar。
// Slot 配置:
//   t0: splat map (RGBA, R=Grass G=Dirt B=Rock)
//   t1..t3: albedo for layer 0..2
//   t4..t6: normal map for layer 0..2 (tangent space, RGB)
//   t7..t9: MRA for layer 0..2 (R=Metallic, G=Roughness, B=AO)
Texture2D gSplatMap : register(t0);
Texture2D gAlbedo0  : register(t1);
Texture2D gAlbedo1  : register(t2);
Texture2D gAlbedo2  : register(t3);
Texture2D gNormal0  : register(t4);
Texture2D gNormal1  : register(t5);
Texture2D gNormal2  : register(t6);
Texture2D gMRA0     : register(t7);
Texture2D gMRA1     : register(t8);
Texture2D gMRA2     : register(t9);
SamplerState gLinearWrap : register(s0);

cbuffer TerrainCB : register(b0)
{
    float4x4 viewProj;
    float4x4 viewProjectionUnjittered;
    float4x4 prevViewProjection;
    float4   chunkOffset;
    float    heightScale;
    float3   pad;
};

cbuffer TerrainMaterialCB : register(b1)
{
    float4 tileScales;          // xyz = レイヤー 0..2 の tileScale
    float4 triplanarParams;     // x = triplanar 強度 (0..1), yzw = unused
};

struct PS_INPUT
{
    float4 position    : SV_Position;
    float3 worldPos    : TEXCOORD0;
    float3 normal      : TEXCOORD1;
    float2 uv          : TEXCOORD2;
    float4 curClipPos  : TEXCOORD3;
    float4 prevClipPos : TEXCOORD4;
};

struct PS_OUTPUT
{
    float4 albedoMetallic  : SV_TARGET0;
    float4 normalRoughness : SV_TARGET1;
    float4 worldPosDepth   : SV_TARGET2;
    float2 velocity        : SV_TARGET3;
};

// --- Triplanar サンプリングヘルパー ---
struct TriplanarUVs {
    float2 xPlane;   // (z, y)
    float2 yPlane;   // (x, z)
    float2 zPlane;   // (x, y)
    float3 weights;  // ブレンド係数 (合計 = 1)
};

TriplanarUVs ComputeTriplanar(float3 worldPos, float3 normal, float tileScale, float strength)
{
    TriplanarUVs t;
    float ts = max(tileScale, 0.001f);
    // ワールド座標を tile スケール基準の UV へ
    t.xPlane = worldPos.zy * ts * 0.05f;
    t.yPlane = worldPos.xz * ts * 0.05f;
    t.zPlane = worldPos.xy * ts * 0.05f;

    // 平面ごとの重み (法線の絶対値 ^ pow)
    float3 w = pow(abs(normal), lerp(2.0f, 8.0f, strength));
    // 平面方向の影響を絞る (天面メインで sloped 部分のみ三方向)
    w.y *= lerp(1.0f, 2.5f, strength);   // 平地は yPlane 優位
    float wsum = max(w.x + w.y + w.z, 0.0001f);
    t.weights = w / wsum;
    return t;
}

float3 SampleAlbedoTriplanar(Texture2D tex, TriplanarUVs t)
{
    float3 ax = tex.Sample(gLinearWrap, t.xPlane).rgb;
    float3 ay = tex.Sample(gLinearWrap, t.yPlane).rgb;
    float3 az = tex.Sample(gLinearWrap, t.zPlane).rgb;
    return ax * t.weights.x + ay * t.weights.y + az * t.weights.z;
}

float3 SampleMRATriplanar(Texture2D tex, TriplanarUVs t)
{
    float3 ax = tex.Sample(gLinearWrap, t.xPlane).rgb;
    float3 ay = tex.Sample(gLinearWrap, t.yPlane).rgb;
    float3 az = tex.Sample(gLinearWrap, t.zPlane).rgb;
    return ax * t.weights.x + ay * t.weights.y + az * t.weights.z;
}

// 法線マップは平面ごとに「世界空間法線」を構築してから合成 (RNM: Reoriented Normal Mapping 簡易版)
float3 SampleNormalTriplanar(Texture2D tex, TriplanarUVs t, float3 worldNormal)
{
    // 各平面のタンジェントスペース法線 (.xy が摂動)
    float3 nx_ts = tex.Sample(gLinearWrap, t.xPlane).rgb * 2.0f - 1.0f;
    float3 ny_ts = tex.Sample(gLinearWrap, t.yPlane).rgb * 2.0f - 1.0f;
    float3 nz_ts = tex.Sample(gLinearWrap, t.zPlane).rgb * 2.0f - 1.0f;

    // UDN (Unity-style) 簡易合成: 平面の主軸に法線摂動を加える
    // X 平面: (z, y) 上の摂動を yz 軸へ展開
    float3 nx_world = float3(worldNormal.x + nx_ts.x * sign(worldNormal.x), nx_ts.y, nx_ts.z * 0.0f);
    float3 ny_world = float3(ny_ts.x, worldNormal.y + ny_ts.y * sign(worldNormal.y), ny_ts.z * 0.0f);
    // 上記式は不完全。UDN 標準形式: tsNormal の xy を base normal の接面上に乗せる。
    // ここでは Reoriented (BLEND) を使う:
    nx_world = float3(nx_ts.xy + worldNormal.zy, abs(nx_ts.z) * worldNormal.x);
    ny_world = float3(ny_ts.xy + worldNormal.xz, abs(ny_ts.z) * worldNormal.y);
    float3 nz_world = float3(nz_ts.xy + worldNormal.xy, abs(nz_ts.z) * worldNormal.z);
    // 軸入れ替え (Unity UDN 標準)
    nx_world = nx_world.zyx;
    ny_world = ny_world.xzy;

    float3 result = normalize(
        nx_world * t.weights.x +
        ny_world * t.weights.y +
        nz_world * t.weights.z
    );
    return result;
}

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;

    float3 worldNormal = normalize(input.normal);

    // === Step 1: スプラットウェイト取得 (R=Grass, G=Dirt, B=Rock) ===
    float4 splat = gSplatMap.Sample(gLinearWrap, input.uv);
    float3 weights = max(splat.rgb, 0.0f);
    float weightSum = weights.r + weights.g + weights.b;
    if (weightSum <= 0.0001f) {
        weights = float3(1.0f, 0.0f, 0.0f);  // 旧データ互換: グラスのみ
    } else {
        weights /= weightSum;
    }

    // === Step 2: レイヤーごとの Triplanar UV ===
    float triStrength = saturate(triplanarParams.x);
    TriplanarUVs tri0 = ComputeTriplanar(input.worldPos, worldNormal, tileScales.x, triStrength);
    TriplanarUVs tri1 = ComputeTriplanar(input.worldPos, worldNormal, tileScales.y, triStrength);
    TriplanarUVs tri2 = ComputeTriplanar(input.worldPos, worldNormal, tileScales.z, triStrength);

    // === Step 3: アルベド (Triplanar) ===
    float3 albedo0 = SampleAlbedoTriplanar(gAlbedo0, tri0);
    float3 albedo1 = SampleAlbedoTriplanar(gAlbedo1, tri1);
    float3 albedo2 = SampleAlbedoTriplanar(gAlbedo2, tri2);

    // === Step 4: Height-blend (albedo の輝度をハイト hint として使う) ===
    // 各レイヤーの「高さ」推定 = 輝度 + splat weight。最大値が勝つ blend。
    float h0 = dot(albedo0, float3(0.30f, 0.59f, 0.11f)) + weights.r * 1.4f;
    float h1 = dot(albedo1, float3(0.30f, 0.59f, 0.11f)) + weights.g * 1.4f;
    float h2 = dot(albedo2, float3(0.30f, 0.59f, 0.11f)) + weights.b * 1.4f;
    // 急峻なブレンド: max - blendRange より下のレイヤーは 0 にする
    const float blendRange = 0.18f;
    float hmax = max(h0, max(h1, h2));
    float w0 = max(h0 - hmax + blendRange, 0.0f);
    float w1 = max(h1 - hmax + blendRange, 0.0f);
    float w2 = max(h2 - hmax + blendRange, 0.0f);
    float wsum = max(w0 + w1 + w2, 0.0001f);
    w0 /= wsum;
    w1 /= wsum;
    w2 /= wsum;

    float3 albedoSrgb = albedo0 * w0 + albedo1 * w1 + albedo2 * w2;
    // DeferredLightingPS は線形空間アルベドを期待するので、sRGB から線形へ変換。
    float3 albedoLin = pow(saturate(albedoSrgb), 2.2f);

    // === Step 5: 法線マップ (Triplanar 合成) と MRA ===
    float3 n0 = SampleNormalTriplanar(gNormal0, tri0, worldNormal);
    float3 n1 = SampleNormalTriplanar(gNormal1, tri1, worldNormal);
    float3 n2 = SampleNormalTriplanar(gNormal2, tri2, worldNormal);
    float3 blendedNormal = n0 * w0 + n1 * w1 + n2 * w2;
    // 正規化前に長さチェック (normalize(0) は NaN になり、後段の length() 検出を裏切る)
    float blendedLen = length(blendedNormal);
    float3 finalNormal = (blendedLen > 0.001f) ? blendedNormal / blendedLen : worldNormal;

    float3 mra0 = SampleMRATriplanar(gMRA0, tri0);
    float3 mra1 = SampleMRATriplanar(gMRA1, tri1);
    float3 mra2 = SampleMRATriplanar(gMRA2, tri2);
    float3 mra = mra0 * w0 + mra1 * w1 + mra2 * w2;
    // MRA テクスチャが未設定 (全 0) なら地形の既定値を使う
    float metallic  = mra.r;
    float roughness = (mra.g > 0.001f) ? mra.g : 0.85f;
    float ao        = (mra.b > 0.001f) ? mra.b : 1.0f;

    // === Step 6: Velocity (motion vector) ===
    float2 currentNDC = input.curClipPos.xy  / input.curClipPos.w;
    float2 prevNDC    = input.prevClipPos.xy / input.prevClipPos.w;
    float2 currentUV  = currentNDC * float2(0.5f, -0.5f) + 0.5f;
    float2 prevUV     = prevNDC    * float2(0.5f, -0.5f) + 0.5f;

    // === Step 7: GBuffer 出力 ===
    output.albedoMetallic  = float4(saturate(albedoLin * ao), metallic);
    output.normalRoughness = float4(finalNormal, roughness);
    output.worldPosDepth   = float4(input.worldPos, input.position.z);
    output.velocity        = prevUV - currentUV;
    return output;
}
