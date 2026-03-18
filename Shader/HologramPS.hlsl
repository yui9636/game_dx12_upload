//=============================================================================
// HologramPS.hlsl
//=============================================================================
#include "Hologram.hlsl"

// C++の HologramShader::Draw で PSSetShaderResources(0, ...) されているテクスチャ
Texture2D noiseTexture : register(t0);
SamplerState samplerState : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    // -----------------------------------------------------------------------
    // 1. UVスクロール & ノイズサンプリング (テクスチャアニメーション)
    // -----------------------------------------------------------------------
    // 時間経過でUVをずらす
    float2 scrollUV = pin.texcoord;
    scrollUV.x += time * 0.05; // 横方向へゆっくり
    scrollUV.y += time * 0.1; // 縦方向へ少し速く

    // ノイズテクスチャから値を取得 (R成分のみ使用)
    // ※テクスチャがない場合は0になるか、黒として扱われます
    float noiseValue = noiseTexture.Sample(samplerState, scrollUV).r;

    // -----------------------------------------------------------------------
    // 2. ディゾルブ消滅 (断片化)
    // -----------------------------------------------------------------------
    // 全体の透明度(alpha)が下がってきたら、ノイズの明るい部分から穴をあける
    // alpha: 1.0(出現) -> 0.0(消滅)
    // threshold: 0.0 -> 1.0
    float dissolveThreshold = 1.0 - alpha;
    
    // ノイズ値が閾値より低いピクセルは描画しない (discard)
    // +0.05 は完全に消える直前のマージン
    clip(noiseValue - dissolveThreshold + 0.05);

    // -----------------------------------------------------------------------
    // 3. 色収差付き走査線 (Chromatic Aberration)
    // -----------------------------------------------------------------------
    // RGBそれぞれのチャンネルで、走査線の位相(phase)を少しずらす
    float3 scanlineColor;
    float y = pin.worldPos.y;
    
    // R: 少し遅らせる
    float scanR = sin(y * scanlineFreq - time * scanlineSpeed - 0.2);
    scanR = scanR * 0.5 + 0.5;
    scanR = pow(scanR, 2.0);

    // G: 基準
    float scanG = sin(y * scanlineFreq - time * scanlineSpeed);
    scanG = scanG * 0.5 + 0.5;
    scanG = pow(scanG, 2.0);

    // B: 少し進める
    float scanB = sin(y * scanlineFreq - time * scanlineSpeed + 0.2);
    scanB = scanB * 0.5 + 0.5;
    scanB = pow(scanB, 2.0);

    scanlineColor = float3(scanR, scanG, scanB);

    // -----------------------------------------------------------------------
    // 4. パルス発光 (Blinking Rim)
    // -----------------------------------------------------------------------
    // 時間経過で 0.5 ～ 1.0 の間を行き来する係数を作成
    float pulse = abs(sin(time * 3.0)) * 0.5 + 0.5;

    // -----------------------------------------------------------------------
    // フレネル (Rim Light) 計算
    // -----------------------------------------------------------------------
    float3 viewDir = normalize(cameraPosition.xyz - pin.worldPos);
    float3 N = normalize(pin.normal);
    
    // 視線と法線の内積
    float NdotV = saturate(dot(N, viewDir));
    
    // 反転して累乗
    float rim = pow(1.0 - NdotV, max(fresnelPower, 0.01));
    
    // ★パルスを適用 (明滅させる)
    rim *= pulse;

    // -----------------------------------------------------------------------
    // 最終合成
    // -----------------------------------------------------------------------
    
    // ベースカラーに色収差走査線を乗算
    float3 bodyColor = baseColor.rgb * scanlineColor;
    
    // 環境光の影響を受けない純粋な加算合成
    float3 finalRGB = bodyColor + (rimColor.rgb * rim);

    // アルファ計算
    // フェードアウト(alpha)に合わせて全体も消す
    float baseAlpha = baseColor.a * 0.5;
    float finalAlpha = baseAlpha + rim;
    finalAlpha *= alpha;

    // ディゾルブの縁（エッジ）を光らせる演出（オプション）
    // 消えかけの境界線(noiseValueが閾値に近い場所)に色を足す
    if (noiseValue < dissolveThreshold + 0.1)
    {
        finalRGB += rimColor.rgb * 2.0; // 強く発光
    }

    return float4(finalRGB, saturate(finalAlpha));
}