#pragma once
#include <DirectXMath.h>

constexpr int MAX_POINT_LIGHTS = 8;

// GPU に渡すポイントライト 1 個分の情報。
struct PointLightData {
    DirectX::XMFLOAT3 position; // world space light position。
    float range;                // 影響半径。
    DirectX::XMFLOAT3 color;    // RGB light color。
    float intensity;            // 光量倍率。
};
// シーン共通定数バッファ。カメラ行列、ライト、ジッター情報を描画パスへ渡す。
struct CbScene {
    DirectX::XMFLOAT4X4 viewProjection; // 現 frame の jitter 込み view projection。
    DirectX::XMFLOAT4X4 viewProjectionUnjittered; // jitter なし view projection。
    DirectX::XMFLOAT4X4 prevViewProjection; // 前 frame の view projection。
    DirectX::XMFLOAT4   lightDirection; // directional light の向き。
    DirectX::XMFLOAT4   lightColor; // directional light の色と強度。
    DirectX::XMFLOAT4   cameraPosition; // world space camera position。
    DirectX::XMFLOAT4X4 lightViewProjection; // main light の view projection。
    DirectX::XMFLOAT4   shadowColor; // shadow 合成時の色。

    float shadowTexelSize; // shadow map 1 texel の UV サイズ。
    float jitterX;         // 現 frame の TAA jitter X。
    float jitterY;         // 現 frame の TAA jitter Y。
    float renderW;         // render target 幅。

    float renderH;         // render target 高さ。
    float pointLightCount; // pointLights の有効数。
    float prevJitterX;     // 前 frame の TAA jitter X。
    float prevJitterY;     // 前 frame の TAA jitter Y。

    PointLightData pointLights[MAX_POINT_LIGHTS]; // GPU に渡す point light 配列。
};
// カスケードシャドウ用のライト行列と影パラメータ。
struct CbShadowMap {
    DirectX::XMFLOAT4X4 lightViewProjections[3]; // cascade ごとの light view projection。
    DirectX::XMFLOAT4   cascadeSplits; // 各 cascade の終端距離。
    DirectX::XMFLOAT4   shadowColor;   // shadow 合成色。
    DirectX::XMFLOAT4   shadowBias;    // bias / normal bias などの調整値。
};
