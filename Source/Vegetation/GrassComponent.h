#pragma once
#include <DirectXMath.h>
#include <cstdint>

// 地形エンティティに付与する草パラメータ。
// 実体メッシュ/インスタンスバッファは GrassBuildSystem が管理する。
struct GrassComponent {
    bool enabled = true;
    bool needsRebuild = true;

    // 配置密度: 1.0 = 各セルに最大 maxBladesPerCell 本、splat の草重みで線形にスケール。
    float    densityMultiplier  = 1.0f;        // 0..2
    uint32_t maxBladesPerCell   = 4;           // 1..8 (パフォーマンス安全弁)
    float    densityThreshold   = 0.20f;       // splat.r がこれ未満のセルは草を生やさない

    // ブレード形状
    float bladeHeight         = 0.6f;          // 平均高さ (world units)
    float bladeHeightVariance = 0.35f;         // ±比率
    float bladeWidth          = 0.10f;

    // 風
    DirectX::XMFLOAT3 windDirection = { 1.0f, 0.0f, 0.6f };
    float windStrength = 0.35f;
    float windSpeed    = 1.4f;

    // 色 (頂点ごとに primary↔secondary を頂点ハッシュで混ぜる)
    DirectX::XMFLOAT3 colorBottom = { 0.16f, 0.28f, 0.10f };
    DirectX::XMFLOAT3 colorTop    = { 0.55f, 0.78f, 0.30f };
    DirectX::XMFLOAT3 colorTintVariance = { 0.10f, 0.10f, 0.05f };

    // ドロー距離 (これ以上離れたチャンクは描画しない -- 簡易距離カル)
    float drawDistance = 80.0f;

    // 乱数シード (再生成で結果再現したいとき固定)
    int seed = 1234;

    bool showInEditor = true;
};
