cbuffer WaterCB : register(b0)
{
    float4x4 viewProj;
    float4 worldOffsetSeaLevel;
    float4 shallowColor;
    float4 deepColor;
    float4 params;             // x=time, y=waveSpeed, z=waveScale, w=depthFade
    float4 cameraPosition;
    float4 screenParams;       // x=W, y=H, z=1/W, w=1/H
    float4 waterFeatureFlags;  // x=hasRefraction
};

// PS4 クラス水: GBuffer2 (WorldPos/Depth) で真の水深, PrevScene でスクリーン空間屈折。
Texture2D gGBuffer2  : register(t0);  // .xyz = world position, .w = linear depth
Texture2D gPrevScene : register(t1);  // 前フレームのシーン色 (1f 遅延の屈折ソース)
SamplerState gLinearWrap   : register(s0);
SamplerState gPointClamp   : register(s2);
SamplerState gLinearClamp  : register(s3);

struct PS_INPUT
{
    float4 position  : SV_Position;
    float3 worldPos  : TEXCOORD0;
    float2 uv        : TEXCOORD1;
    float  shore     : TEXCOORD2;
    float3 baseNormal: TEXCOORD3;
    float  crest     : TEXCOORD4;
};

// --- 軽量ハッシュ + 値ノイズ + FBM ---
float Hash21(float2 p)
{
    p = frac(p * float2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

float ValueNoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float a = Hash21(i);
    float b = Hash21(i + float2(1, 0));
    float c = Hash21(i + float2(0, 1));
    float d = Hash21(i + float2(1, 1));
    float2 u = f * f * (3.0f - 2.0f * f);
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float Fbm3(float2 p)
{
    float v = 0.0f;
    float a = 0.5f;
    [unroll]
    for (int i = 0; i < 3; ++i) {
        v += a * ValueNoise(p);
        p *= 2.0f;
        a *= 0.55f;
    }
    return v;
}

// 3 方向スクロール短波ノイズから法線摂動を取り出す。
float3 HighFrequencyNormal(float2 worldXZ, float time, float waveSpeed)
{
    float2 dir1 = float2( 0.51f,  0.86f);
    float2 dir2 = float2(-0.78f,  0.62f);
    float2 dir3 = float2( 0.93f, -0.36f);
    float t = time * waveSpeed;

    float2 uv1 = worldXZ * 0.22f + dir1 * t * 0.40f;
    float2 uv2 = worldXZ * 0.46f + dir2 * t * 0.62f;
    float2 uv3 = worldXZ * 0.95f + dir3 * t * 0.85f;

    float e = 0.35f;
    float h1x = Fbm3(uv1 + float2(e, 0)) - Fbm3(uv1 - float2(e, 0));
    float h1z = Fbm3(uv1 + float2(0, e)) - Fbm3(uv1 - float2(0, e));
    float h2x = Fbm3(uv2 + float2(e, 0)) - Fbm3(uv2 - float2(e, 0));
    float h2z = Fbm3(uv2 + float2(0, e)) - Fbm3(uv2 - float2(0, e));
    float h3x = Fbm3(uv3 + float2(e, 0)) - Fbm3(uv3 - float2(e, 0));
    float h3z = Fbm3(uv3 + float2(0, e)) - Fbm3(uv3 - float2(0, e));

    float nx = (h1x * 0.6f + h2x * 0.35f + h3x * 0.18f);
    float nz = (h1z * 0.6f + h2z * 0.35f + h3z * 0.18f);
    return float3(-nx, 1.0f, -nz);
}

float SchlickFresnel(float cosTheta, float F0)
{
    float m = 1.0f - saturate(cosTheta);
    float m2 = m * m;
    return F0 + (1.0f - F0) * (m2 * m2 * m);
}

// プロシージャル空 (反射ベクトル -> 空色) + 太陽寄与
float3 SampleProceduralSky(float3 reflDir, float3 lightDir)
{
    float upDot = saturate(reflDir.y * 0.5f + 0.5f);
    float3 horizon = float3(0.62f, 0.74f, 0.82f);
    float3 zenith  = float3(0.18f, 0.36f, 0.62f);
    float3 sky = lerp(horizon, zenith, smoothstep(0.45f, 1.0f, upDot));
    sky += float3(0.10f, 0.05f, 0.0f) * (1.0f - upDot) * (1.0f - upDot);
    // 反射した太陽 (空のディスク)
    float sunDot = saturate(dot(reflDir, lightDir));
    sky += float3(2.4f, 2.1f, 1.6f) * pow(sunDot, 320.0f);
    return sky;
}

// 2 層交差サイン -> コースティクス模様 (海底に投影される明暗)
float CausticsPattern(float2 worldXZ, float time)
{
    float t = time * 0.45f;
    float2 a = worldXZ * 0.30f + float2( t,  t * 0.7f);
    float2 b = worldXZ * 0.18f + float2(-t * 0.6f, t * 0.85f);
    float p1 = sin(a.x) * sin(a.y);
    float p2 = sin(b.x + b.y) * cos(b.x - b.y);
    float c = saturate((p1 * 0.5f + 0.5f) * (p2 * 0.5f + 0.5f));
    // 強調 (光線が集中するように)
    c = pow(c, 1.8f);
    return c;
}

float4 main(PS_INPUT input) : SV_Target
{
    float time      = params.x;
    float waveSpeed = params.y;
    float depthFade = max(params.w, 0.001f);

    // === Step 1: 法線合成 (Gerstner + 高周波) ===
    float3 hfN = HighFrequencyNormal(input.worldPos.xz, time, waveSpeed);
    float3 normal = normalize(input.baseNormal + hfN * 0.6f);

    float3 viewDir  = normalize(cameraPosition.xyz - input.worldPos);
    float3 lightDir = normalize(float3(-0.35f, 0.82f, -0.28f));

    // 距離フェード (遠景は法線をフラットへ)
    float distView = length(cameraPosition.xyz - input.worldPos);
    float farFade  = saturate(1.0f - distView * 0.0020f);
    normal = normalize(lerp(float3(0, 1, 0), normal, farFade));

    // === Step 2: スクリーン UV / GBuffer サンプル ===
    float2 screenUV = input.position.xy * screenParams.zw;

    // 屈折用 UV 歪み: 法線 X/Z を画面平面の歪みとして適用
    float2 refractOffset = normal.xz * 0.045f * farFade;
    float2 refractUV = saturate(screenUV + refractOffset);

    // === Step 3: 水深を求める ===
    // GBuffer2 は PBR メッシュしか書き込まないので、地形 (Forward パス) のピクセルでは空。
    // -> GBuffer2 が有効か検出し、ダメなら shore (頂点段で算出済みの水深近似) を使う。
    float4 g2Sample = gGBuffer2.SampleLevel(gPointClamp, refractUV, 0);
    float4 g2Straight = gGBuffer2.SampleLevel(gPointClamp, screenUV, 0);
    float3 floorWorld = (length(g2Sample.xyz) > 0.01f) ? g2Sample.xyz : g2Straight.xyz;
    float gbufferValid = saturate((abs(floorWorld.x) + abs(floorWorld.y) + abs(floorWorld.z)) * 100.0f);

    // shore は VS 段で水面から見た海底深度を [0..1] に正規化したもの。実距離 m に概算復元。
    // shoreDepth ~= max(0.75, heightScale*0.06) なので保守的に 6m と仮定。
    float proceduralDepth = pow(input.shore, 1.1f) * 6.0f;
    proceduralDepth *= (1.0f + 0.18f * sin(input.worldPos.x * 0.05f + input.worldPos.z * 0.04f + time * 0.30f));

    float realDepth = max(input.worldPos.y - floorWorld.y, 0.0f);
    float waterDepth = lerp(proceduralDepth, realDepth, gbufferValid);

    // 視線が斜めに入ると光路長は伸びる
    float NdotV0 = saturate(dot(float3(0, 1, 0), viewDir));
    float opticalDepth = waterDepth / max(NdotV0, 0.18f);

    // === Step 4: 水の色 (Beer-Lambert 吸収) ===
    float3 authoredShallow = lerp(float3(0.18f, 0.42f, 0.52f), shallowColor.rgb, 0.40f);
    float3 authoredDeep    = lerp(float3(0.005f, 0.05f, 0.12f), deepColor.rgb,    0.40f);
    // RGB ごとの吸収係数 (赤が最も吸収される -> 深くなるほど青緑に)
    float3 absorption = float3(0.42f, 0.10f, 0.06f);
    float3 absorbed   = exp(-absorption * opticalDepth);
    float3 waterColor = authoredShallow * absorbed + authoredDeep * (1.0f - absorbed);

    // === Step 5: スクリーン空間屈折 (PrevScene があれば) ===
    float3 refractedScene = waterColor;
    if (waterFeatureFlags.x > 0.5f) {
        // 浅い場所では屈折の歪みを小さくする (海底が透ける)
        float distortMix = saturate(opticalDepth * 0.25f);
        float2 finalRefractUV = lerp(screenUV, refractUV, distortMix);
        float3 sceneSample = gPrevScene.SampleLevel(gLinearClamp, finalRefractUV, 0).rgb;
        // 海底色を水で着色 (深いほど水色が勝つ)
        refractedScene = sceneSample * absorbed + waterColor * (1.0f - absorbed);
    }

    // === Step 6: コースティクス (海底の光斑) ===
    // GBuffer2 無効時は水面ワールド座標で代用 (波の動きに同期するので自然)
    float2 causticUV = (gbufferValid > 0.5f) ? floorWorld.xz : input.worldPos.xz;
    float caustic = CausticsPattern(causticUV, time);
    // 浅い場所だけ強く出す (吸収で見えなくなる)
    float causticFade = saturate(absorbed.g * 1.5f);
    refractedScene += float3(0.45f, 0.55f, 0.42f) * caustic * causticFade * 0.55f;

    // === Step 7: フレネル + 空反射 ===
    float NdotV = saturate(dot(normal, viewDir));
    float fresnel = SchlickFresnel(NdotV, 0.02f);
    float3 refl = reflect(-viewDir, normal);
    float3 skyCol = SampleProceduralSky(refl, lightDir);

    // === Step 8: 太陽スペキュラ (シャープ + 広域) ===
    float3 halfV = normalize(lightDir + viewDir);
    float NdotH  = saturate(dot(normal, halfV));
    float sunSpec  = pow(NdotH, 280.0f) * 7.5f;
    float wideSpec = pow(NdotH, 22.0f)  * 0.40f * saturate(dot(normal, lightDir));

    // === Step 9: サブサーフェススキャッタリング (波の透過光) ===
    float backScatter = pow(saturate(dot(viewDir, -lightDir)), 4.0f) * (1.0f - saturate(opticalDepth * 0.5f)) * 0.30f;

    // === Step 10: フォーム (波頭 + 浅瀬, 真の水深ベース) ===
    float foamNoise = Fbm3(input.worldPos.xz * 0.6f + float2(time * 0.10f, time * 0.07f));
    float crestFoam = smoothstep(0.55f, 0.95f, input.crest) * (0.55f + 0.45f * foamNoise);
    // 浅瀬フォーム: waterDepth が ~0 で最大、深くなるほどフェード
    float shoreFoamMask = 1.0f - smoothstep(0.0f, 0.65f, waterDepth);
    float shoreFoam = shoreFoamMask * (0.40f + 0.60f * foamNoise);
    float foam = saturate(max(crestFoam, shoreFoam));

    // === Step 11: 合成 ===
    float3 color = lerp(refractedScene, skyCol, fresnel);
    color += float3(1.0f, 0.96f, 0.86f) * (sunSpec + wideSpec) * farFade;
    color += float3(0.45f, 0.62f, 0.78f) * backScatter;
    color  = lerp(color, float3(0.96f, 0.98f, 1.0f), foam * 0.90f);

    // === Step 12: アルファ (ソフトシャドウライン + フォーム) ===
    // waterDepth が小さいほど透明 (海底が見える)
    float softEdge = saturate(waterDepth * 0.85f);
    float alpha = saturate(softEdge * (0.55f + fresnel * 0.45f) + foam * 0.70f);

    return float4(color, alpha);
}
