#pragma once
#include <DirectXMath.h>

/**
 * @brief カメラのレンズ設定（投影データ）。
 */
struct CameraLensComponent {
    float fovY = 0.785398f;
    float nearZ = 0.1f;
    float farZ = 100000.0f;
    float aspect = 16.0f / 9.0f;
};

/**
 * @brief システムによって更新される行列データ。
 * RenderPass はこのコンポーネントを読み取って描画する。
 */
struct CameraMatricesComponent {
    DirectX::XMFLOAT4X4 view = {};
    DirectX::XMFLOAT4X4 projection = {};
    DirectX::XMFLOAT3   worldPos = { 0, 0, 0 };
    DirectX::XMFLOAT3   cameraFront = { 0, 0, 1 };
};

/**
 * @brief メインカメラ判定用タグ。
 */
struct  CameraMainTagComponent {};
