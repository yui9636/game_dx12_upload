#include "DX12RootSignature.h"
#include <d3d12.h>
#include <cassert>

DX12RootSignature::DX12RootSignature(DX12Device* device) {
    // Root parameters: 8 CBVs (b0-b7) + 1 SRV descriptor table (t0-t15)
    D3D12_ROOT_PARAMETER1 rootParams[Slot::Count] = {};

    // b0-b7: CBV root parameters
    for (int i = 0; i <= 7; ++i) {
        rootParams[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParams[i].Descriptor.ShaderRegister = i;
        rootParams[i].Descriptor.RegisterSpace = 0;
        rootParams[i].Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
        rootParams[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    }

    // SRV table: t0~t63 (DeferredLightingPS 等が t35 を使用するため拡張)
    D3D12_DESCRIPTOR_RANGE1 srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 64;
    srvRange.BaseShaderRegister = 0;
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    srvRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE
                   | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

    rootParams[Slot::SRVTable].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[Slot::SRVTable].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[Slot::SRVTable].DescriptorTable.pDescriptorRanges = &srvRange;
    rootParams[Slot::SRVTable].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Static samplers (s0-s3)
    D3D12_STATIC_SAMPLER_DESC staticSamplers[4] = {};

    // s0: LinearWrap
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].MipLODBias = -1.0f;
    staticSamplers[0].MaxAnisotropy = 1;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    staticSamplers[0].MinLOD = 0.0f;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].RegisterSpace = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // s1: ShadowCompare
    staticSamplers[1] = staticSamplers[0];
    staticSamplers[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    staticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    staticSamplers[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    staticSamplers[1].MipLODBias = 0.0f;
    staticSamplers[1].ShaderRegister = 1;

    // s2: PointClamp
    staticSamplers[2] = staticSamplers[0];
    staticSamplers[2].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    staticSamplers[2].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[2].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[2].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[2].MipLODBias = 0.0f;
    staticSamplers[2].ShaderRegister = 2;

    // s3: LinearClamp
    staticSamplers[3] = staticSamplers[0];
    staticSamplers[3].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[3].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[3].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[3].ShaderRegister = 3;

    // Root signature desc
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rsDesc.Desc_1_1.NumParameters = Slot::Count;
    rsDesc.Desc_1_1.pParameters = rootParams;
    rsDesc.Desc_1_1.NumStaticSamplers = 4;
    rsDesc.Desc_1_1.pStaticSamplers = staticSamplers;
    rsDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeVersionedRootSignature(&rsDesc, &signature, &error);
    if (FAILED(hr)) {
        if (error) {
            OutputDebugStringA((const char*)error->GetBufferPointer());
        }
        assert(false && "Failed to serialize root signature");
    }

    hr = device->GetDevice()->CreateRootSignature(
        0, signature->GetBufferPointer(), signature->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignature));
    assert(SUCCEEDED(hr));
}
