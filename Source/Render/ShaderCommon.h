#pragma once
#include <DirectXMath.h>

// 最大ポイントライト数
constexpr int MAX_POINT_LIGHTS = 8;

struct PointLightData {
    DirectX::XMFLOAT3 position;
    float range;
    DirectX::XMFLOAT3 color;
    float intensity;
};

// ==========================================
// 全パス共通のシーン情報 (b7 スロット固定)
// ==========================================
struct CbScene {
    DirectX::XMFLOAT4X4 viewProjection;
    DirectX::XMFLOAT4X4 viewProjectionUnjittered;
    DirectX::XMFLOAT4X4 prevViewProjection;
    DirectX::XMFLOAT4   lightDirection;
    DirectX::XMFLOAT4   lightColor;
    DirectX::XMFLOAT4   cameraPosition;
    DirectX::XMFLOAT4X4 lightViewProjection; // レガシー互換用
    DirectX::XMFLOAT4   shadowColor;

    // --- 16バイト境界 ---
    float shadowTexelSize;
    float jitterX;
    float jitterY;
    float renderW;

    // --- 16バイト境界 ---
    float renderH;
    float pointLightCount;
    float prevJitterX;
    float prevJitterY;

    PointLightData pointLights[MAX_POINT_LIGHTS];
};

// ==========================================
// 全パス共通の影情報 (b4 スロット固定)
// ==========================================
struct CbShadowMap {
    DirectX::XMFLOAT4X4 lightViewProjections[3];
    DirectX::XMFLOAT4   cascadeSplits;
    DirectX::XMFLOAT4   shadowColor;
    DirectX::XMFLOAT4   shadowBias;
};