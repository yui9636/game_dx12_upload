#pragma once
#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include "RHI/ITexture.h"
#include <memory>

struct ReflectionProbeComponent {
    DirectX::XMFLOAT3 position = { 0.0f, 1.5f, 0.0f }; // 撮影する座標
    float radius = 10.0f; // 反射の影響範囲（Phase 3の合成で使用します）

    // 生成されたCubemapのSRV（シェーダーに渡す本体）
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cubemapSRV;

    // 「撮影が必要か？」のフラグ（家具が動いた時などにtrueにする）
    bool needsBake = true;

    std::shared_ptr<ITexture> cubemapTexture;
};