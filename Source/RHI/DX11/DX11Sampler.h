#pragma once
#include "../ISampler.h"
#include <d3d11.h>
#include <wrl.h>
#include <stdexcept>

class DX11Sampler : public ISampler {
public:
    DX11Sampler(ID3D11Device* device, const D3D11_SAMPLER_DESC& desc);
    ~DX11Sampler() override = default;

    ID3D11SamplerState* GetNative() const { return m_sampler.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler;
};
