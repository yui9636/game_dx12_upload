cbuffer WaterCB : register(b0)
{
    float4x4 viewProj;
    float4 worldOffsetSeaLevel;
    float4 shallowColor;
    float4 deepColor;
    float4 params;          // x=time, y=waveSpeed, z=waveScale, w=depthFade
    float4 cameraPosition;
};

struct VS_INPUT
{
    float3 position : POSITION;
    float2 uv       : TEXCOORD0;
    float2 shoreData : TEXCOORD1;
};

struct VS_OUTPUT
{
    float4 position  : SV_Position;
    float3 worldPos  : TEXCOORD0;
    float2 uv        : TEXCOORD1;
    float  shore     : TEXCOORD2;
    float3 baseNormal: TEXCOORD3;   // Gerstner サマリ法線 (PS 高周波法線の土台)
    float  crest     : TEXCOORD4;   // 波頭判定 [0..1]
};

// 6 方向 Gerstner-like 波。tangent / binormal を蓄積して厳密法線を得る。
float3 GerstnerAccumulate(float2 worldXZ, float time, float globalScale, float globalSpeed,
                          out float3 tangent, out float3 binormal, out float crestOut)
{
    float3 disp = 0;
    tangent = float3(1, 0, 0);
    binormal = float3(0, 0, 1);
    float crest = 0;

    // 方向、波長スケール、振幅、急峻度。海洋的に長波 + 短波を混ぜる。
    const float2 dirs[6] = {
        float2( 0.86,  0.51),
        float2(-0.71,  0.70),
        float2( 0.20, -0.98),
        float2(-0.95, -0.31),
        float2( 0.46,  0.89),
        float2( 0.97, -0.24),
    };
    const float wavelengths[6] = { 22.0, 14.0, 9.0, 6.0, 4.0, 2.6 };
    const float steepnesses[6] = { 0.30, 0.24, 0.18, 0.14, 0.10, 0.08 };
    const float speeds[6]      = { 0.90, 1.10, 1.30, 1.55, 1.80, 2.10 };

    [unroll]
    for (int i = 0; i < 6; ++i) {
        float wavelength = wavelengths[i] / max(globalScale, 0.001f);
        float k = 6.28318f / wavelength;                // 角周波数 (空間)
        float c = sqrt(9.8f / max(k, 0.0001f)) * globalSpeed; // 物理的位相速度に近い値
        float2 d = dirs[i];
        float f = k * (d.x * worldXZ.x + d.y * worldXZ.y) - time * c * 0.6f;
        float a = steepnesses[i] / max(k, 0.0001f);

        float sinF = sin(f);
        float cosF = cos(f);

        disp.x += d.x * a * cosF * 0.35f;
        disp.z += d.y * a * cosF * 0.35f;
        disp.y += a * sinF;

        // 接線の偏微分蓄積 (Gerstner 法線公式)
        tangent.x  -= d.x * d.x * steepnesses[i] * sinF;
        tangent.y  += d.x       * steepnesses[i] * cosF;
        tangent.z  -= d.x * d.y * steepnesses[i] * sinF;

        binormal.x -= d.x * d.y * steepnesses[i] * sinF;
        binormal.y += d.y       * steepnesses[i] * cosF;
        binormal.z -= d.y * d.y * steepnesses[i] * sinF;

        // 波頭は sin がピーク付近 (上面) かつ振幅大の波で発生しやすい
        crest += saturate(sinF * steepnesses[i] * 4.0f);
    }
    crestOut = saturate(crest * 0.45f);
    return disp;
}

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    float3 worldPos = input.position + worldOffsetSeaLevel.xyz;
    float time      = params.x;
    float waveSpeed = params.y;
    float waveScale = params.z;

    float shore = saturate(input.shoreData.x);
    // 岸際は波が小さくなる (浅い場所で減衰)。中央ほど大きい。
    float waveMask = pow(shore, 0.8f);

    float3 tangent, binormal;
    float crest;
    float3 disp = GerstnerAccumulate(worldPos.xz, time, max(waveScale * 60.0f, 0.4f), waveSpeed,
                                     tangent, binormal, crest);
    worldPos.xyz += disp * waveMask;

    float3 normal = normalize(cross(binormal, tangent));
    // cross 結果は +Y を保証しない。下向きなら反転。
    if (normal.y < 0.0f) normal = -normal;

    output.position   = mul(float4(worldPos, 1.0f), viewProj);
    output.worldPos   = worldPos;
    output.uv         = input.uv;
    output.shore      = shore;
    output.baseNormal = normal;
    output.crest      = crest * waveMask;
    return output;
}
