#pragma once
#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include "RHI/ITexture.h"
#include <memory>

struct ReflectionProbeComponent {
    DirectX::XMFLOAT3 position = { 0.0f, 1.5f, 0.0f };
    float radius = 10.0f;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cubemapSRV;

    bool needsBake = true;

    std::shared_ptr<ITexture> cubemapTexture;
};
