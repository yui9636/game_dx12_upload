#pragma once
#include <DirectXMath.h>

constexpr int MAX_POINT_LIGHTS = 8;

// GPU に渡すポイントライト 1 個分の情報。
struct PointLightData {
    DirectX::XMFLOAT3 position;
    float range;
    DirectX::XMFLOAT3 color;
    float intensity;
};
// シーン共通定数バッファ。カメラ行列、ライト、ジッター情報を描画パスへ渡す。
struct CbScene {
    DirectX::XMFLOAT4X4 viewProjection;
    DirectX::XMFLOAT4X4 viewProjectionUnjittered;
    DirectX::XMFLOAT4X4 prevViewProjection;
    DirectX::XMFLOAT4   lightDirection;
    DirectX::XMFLOAT4   lightColor;
    DirectX::XMFLOAT4   cameraPosition;
    DirectX::XMFLOAT4X4 lightViewProjection;
    DirectX::XMFLOAT4   shadowColor;

    float shadowTexelSize;
    float jitterX;
    float jitterY;
    float renderW;

    float renderH;
    float pointLightCount;
    float prevJitterX;
    float prevJitterY;

    PointLightData pointLights[MAX_POINT_LIGHTS];
};
// カスケードシャドウ用のライト行列と影パラメータ。
struct CbShadowMap {
    DirectX::XMFLOAT4X4 lightViewProjections[3];
    DirectX::XMFLOAT4   cascadeSplits;
    DirectX::XMFLOAT4   shadowColor;
    DirectX::XMFLOAT4   shadowBias;
};
