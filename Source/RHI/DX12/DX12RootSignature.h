#pragma once
#include "DX12Device.h"

// Universal root signature for DX12 backend
// Matches existing DX11 slot layout:
//   b0: Scene CB (VS+PS)
//   b1: Object CB (VS+PS)
//   b2: Material CB (PS)
//   t0~t63: SRV table (PS)
//   s0~s3: Sampler table (PS)
class DX12RootSignature {
public:
    DX12RootSignature(DX12Device* device);
    ~DX12RootSignature() = default;

    ID3D12RootSignature* Get() const { return m_rootSignature.Get(); }

    // Root parameter indices (b0-b7 CBVs + SRV table)
    enum Slot {
        CBV_b0 = 0, CBV_b1 = 1, CBV_b2 = 2, CBV_b3 = 3,
        CBV_b4 = 4, CBV_b5 = 5, CBV_b6 = 6, CBV_b7 = 7,
        SRVTable = 8,
        Count = 9
    };

private:
    ComPtr<ID3D12RootSignature> m_rootSignature;
};
