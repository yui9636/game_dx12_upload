// ────────────────────────────────────────────────────────────────
// compute_particle_update_cs.hlsl ―― HSV補間版 ――
//   ・スケール   : EaseOutCubic
//   ・回転       : EaseOutCubic
//   ・カラー     : HSV + EaseInOutQuad（美しく色相変化）
//   ・透明度     : 線形
// ────────────────────────────────────────────────────────────────
#include "compute_particle.hlsli"

RWStructuredBuffer<particle_data> particle_data_buffer : register(u0);
AppendStructuredBuffer<uint> particle_unused_buffer : register(u1);
RWByteAddressBuffer indirect_data_buffer : register(u2);
RWStructuredBuffer<particle_header> particle_header_buffer : register(u3);


Texture3D<float4> g_curlNoiseTexture : register(t2);
SamplerState g_curlSampler : register(s1); 
// ────── カーブ関数 ──────
float EaseOutCubic(float t)
{
    t = saturate(t);
    return 1.0f - pow(1.0f - t, 3.0f);
}

float EaseInOutQuad(float t)
{
    t = saturate(t);
    return (t < 0.5f) ? (2.0f * t * t)
                      : (1.0f - pow(-2.0f * t + 2.0f, 2.0f) * 0.5f);
}

// ────── RGB ? HSV変換関数 ──────
float3 RGBtoHSV(float3 c)
{
    float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = (c.g < c.b) ? float4(c.bg, K.wz) : float4(c.gb, K.xy);
    float4 q = (c.r < p.x) ? float4(p.xyw, c.r) : float4(c.r, p.yzx);
    float d = q.x - min(q.w, q.y);
    float e = 1e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)),
                  d / (q.x + e),
                  q.x);
}

float3 HSVtoRGB(float3 c)
{
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * lerp(K.xxx, saturate(p - K.xxx), c.y);
}

[numthreads(NumParticleThread, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    uint header_index = dispatch_thread_id.x;
    particle_header header = particle_header_buffer[header_index];
    if (header.alive == 0)
        return;

    uint data_index = header.particle_index;
    particle_data data = particle_data_buffer[data_index];

    // 寿命処理
    data.parameter.y -= elapsed_time;
    if (data.parameter.y < 0.0f)
    {
        header.alive = 0;
        particle_unused_buffer.Append(data_index);
        particle_header_buffer[header_index] = header;
        indirect_data_buffer.InterlockedAdd(IndirectArgumentsNumDeadParticle, 1);
        return;
    }

    
    // -------------------------------------------------------------
    // ★追加: カールノイズ (Curl Noise) の適用
    // -------------------------------------------------------------
    // 移動更新の「前」に、ノイズの力を速度に加算します
    if (curl_noise_strength > 0.001f)
    {
        // 1. サンプリング座標 (ワールド座標 * スケール)
        float3 uvw = data.position.xyz * curl_noise_scale;

        // 2. スクロール処理
        // パーティクルの年齢(age)を使って、Y軸(高さ)方向にノイズを動かします
        // これにより、煙が立ち昇るような表現になります
        float age = data.parameter.w - data.parameter.y; // TotalLife - RemainLife = CurrentAge
        uvw.y -= age * curl_move_speed;

        // 3. 3Dテクスチャからベクトルをサンプリング (MipLevel 0)
        // ※ComputeShaderではSampleLevel必須
        float3 noise = g_curlNoiseTexture.SampleLevel(g_curlSampler, uvw, 0).xyz;

        // 4. 速度に加算 (Force = Noise * Strength * dt)
        data.velocity.xyz += noise * curl_noise_strength * elapsed_time;
    }
    
    
    
    
    
    
    
    
    
    
    
    // 移動更新
    data.velocity.xyz += data.acceleration.xyz * elapsed_time;
    data.position.xyz += data.velocity.xyz * elapsed_time;

    // テクスチャアニメ
    uint startFrame = (uint) (data.parameter.x + 0.5f);
    uint totalFrames = (uint) (data.parameter.z + 0.5f);
    float fps = data.angular_velocity.w;
    float ageSec = data.parameter.w - data.parameter.y;
    uint frameOffset = (uint) (ageSec * fps + 0.5f);
    uint currentFrame = startFrame + (frameOffset % totalFrames); // ループ！

    
    float frameW = 1.0f / texture_split_count.x;
    float frameH = 1.0f / texture_split_count.y;
    float2 uvBase = float2(
        (currentFrame % texture_split_count.x) * frameW,
        (currentFrame / texture_split_count.x) * frameH);
    data.texcoord.xy = uvBase;
    data.texcoord.zw = float2(frameW, frameH);

    // 年齢比率とカーブ
    float remainTime = data.parameter.y;
    float ageRatio = saturate(1.0f - (remainTime / data.parameter.w));
    float curvedAge = EaseOutCubic(ageRatio);

    // スケール補間（EaseOut）
    data.scale.xyz = lerp(data.scale_begin.xyz, data.scale_end.xyz, curvedAge);

    // カラー補間（HSV + EaseInOutQuad）
    int keyCount = clamp(data.gradientCount, 1, MaxGradientKeys);
    float4 outColor = data.gradientColors[keyCount - 1].color;

    [unroll]
    for (int i = 1; i < keyCount; ++i)
    {
        float t0 = data.gradientColors[i - 1].time;
        float t1 = data.gradientColors[i].time;

        if (ageRatio <= t1 || i == keyCount - 1)
        {
            float rawT = saturate((ageRatio - t0) / (t1 - t0 + 1e-6f));
            float curvedT = EaseInOutQuad(rawT);

            float4 colA = data.gradientColors[i - 1].color;
            float4 colB = data.gradientColors[i].color;

            float3 hsvA = RGBtoHSV(colA.rgb);
            float3 hsvB = RGBtoHSV(colB.rgb);

            float hueDiff = hsvB.x - hsvA.x;
            if (abs(hueDiff) > 0.5)
            {
                if (hueDiff > 0)
                    hsvA.x += 1.0;
                else
                    hsvB.x += 1.0;
            }

            float3 hsvInterp = lerp(hsvA, hsvB, curvedT);
            hsvInterp.x = frac(hsvInterp.x);

            outColor.rgb = HSVtoRGB(hsvInterp);
            outColor.a = lerp(colA.a, colB.a, curvedT);
            break;
        }
    }

    data.color = outColor;

    // アルファフェード（直線）
    float fadeIn = data.fade.x;
    float fadeOut = data.fade.y;
    float alphaMul = 1.0f;
    if (fadeIn > 0.0001f && ageRatio < fadeIn)
        alphaMul = saturate(ageRatio / fadeIn);
    else if (fadeOut > 0.0001f && ageRatio > 1.0f - fadeOut)
        alphaMul = saturate((1.0f - ageRatio) / fadeOut);
    data.color.a *= alphaMul;

    // 回転（EaseOut）
    data.rotation.z = data.angular_velocity.z * curvedAge * data.parameter.w;

    // 深度
    header.depth = mul(float4(data.position.xyz, 1.0f), viewProjection).w;

    // 書き戻し
    particle_header_buffer[header_index] = header;
    particle_data_buffer[data_index] = data;
}
