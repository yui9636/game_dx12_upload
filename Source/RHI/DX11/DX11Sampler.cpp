#include "DX11Sampler.h"

DX11Sampler::DX11Sampler(ID3D11Device* device, const D3D11_SAMPLER_DESC& desc) {
    HRESULT hr = device->CreateSamplerState(&desc, m_sampler.GetAddressOf());
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create DX11SamplerState.");
    }
}