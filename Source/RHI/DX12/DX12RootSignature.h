#pragma once
#include "DX12Device.h"

// DX12 backend 全体で使う共通 root signature。
// 既存 DX11 の slot layout と対応させる。
//   b0: Scene CB。VS と PS で参照する。
//   b1: Object CB。VS と PS で参照する。
//   b2: Material CB。PS で参照する。
//   t0~t63: PS 用 SRV table。
//   s0~s3: PS 用 Sampler table。
class DX12RootSignature {
public:
    DX12RootSignature(DX12Device* device);
    ~DX12RootSignature() = default;

    ID3D12RootSignature* Get() const { return m_rootSignature.Get(); }

    // root parameter index は b0-b7 の CBV と SRV table を並べる。
    enum Slot {
        CBV_b0 = 0, CBV_b1 = 1, CBV_b2 = 2, CBV_b3 = 3,
        CBV_b4 = 4, CBV_b5 = 5, CBV_b6 = 6, CBV_b7 = 7,
        SRVTable = 8,
        Count = 9
    };

private:
    ComPtr<ID3D12RootSignature> m_rootSignature;
};
