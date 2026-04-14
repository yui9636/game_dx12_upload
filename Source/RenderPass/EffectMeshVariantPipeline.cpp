#include "EffectMeshVariantPipeline.h"

#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdio>

#include "Console/Logger.h"
#include "RHI/DX12/DX12Device.h"

using Microsoft::WRL::ComPtr;

namespace
{
    bool LoadVariantBlob(const wchar_t* path, ComPtr<ID3DBlob>& out)
    {
        out.Reset();
        const HRESULT hr = D3DReadFileToBlob(path, &out);
        if (FAILED(hr)) {
            LOG_ERROR("[MeshVariant] Shader not found: %ls", path);
            return false;
        }
        return true;
    }

    bool CreateVariantRootSignature(ID3D12Device* device, ID3D12RootSignature** ppRS)
    {
        D3D12_DESCRIPTOR_RANGE1 texRange = {};
        texRange.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        texRange.NumDescriptors     = 6;
        texRange.BaseShaderRegister = 2;
        texRange.RegisterSpace      = 0;
        texRange.Flags              = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
        texRange.OffsetInDescriptorsFromTableStart = 0;

        D3D12_ROOT_PARAMETER1 params[6] = {};
        params[0].ParameterType    = D3D12_ROOT_PARAMETER_TYPE_CBV;
        params[0].Descriptor       = { 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        params[1].ParameterType    = D3D12_ROOT_PARAMETER_TYPE_SRV;
        params[1].Descriptor       = { 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        params[2].ParameterType    = D3D12_ROOT_PARAMETER_TYPE_SRV;
        params[2].Descriptor       = { 1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        params[3].ParameterType    = D3D12_ROOT_PARAMETER_TYPE_CBV;
        params[3].Descriptor       = { 2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE };
        params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[4].DescriptorTable.NumDescriptorRanges = 1;
        params[4].DescriptorTable.pDescriptorRanges   = &texRange;
        params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        params[5].ParameterType    = D3D12_ROOT_PARAMETER_TYPE_CBV;
        params[5].Descriptor       = { 3, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE };
        params[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        sampler.ShaderRegister   = 1;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        sampler.ComparisonFunc   = D3D12_COMPARISON_FUNC_ALWAYS;
        sampler.MaxLOD           = D3D12_FLOAT32_MAX;

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = {};
        desc.Version                    = D3D_ROOT_SIGNATURE_VERSION_1_1;
        desc.Desc_1_1.NumParameters     = _countof(params);
        desc.Desc_1_1.pParameters       = params;
        desc.Desc_1_1.NumStaticSamplers = 1;
        desc.Desc_1_1.pStaticSamplers   = &sampler;
        desc.Desc_1_1.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> blob, err;
        HRESULT hr = D3D12SerializeVersionedRootSignature(&desc, &blob, &err);
        if (FAILED(hr)) {
            if (err) LOG_ERROR("[MeshVariant] RootSig serialize failed: %s",
                (const char*)err->GetBufferPointer());
            return false;
        }
        hr = device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
            IID_PPV_ARGS(ppRS));
        if (FAILED(hr)) { LOG_ERROR("[MeshVariant] CreateRootSignature failed"); return false; }
        return true;
    }

    static const D3D12_INPUT_ELEMENT_DESC kMeshInputLayout[] =
    {
        { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONE_WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONE_INDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",        0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

ID3D12PipelineState* MeshVariantPipelineCache::GetOrCreate(DX12Device* dxDev, uint32_t variantKey)
{
    for (auto& e : entries) {
        if (e.variantKey == variantKey) return e.pso.Get();
    }

    auto* device = dxDev ? dxDev->GetDevice() : nullptr;
    if (!device) return nullptr;

    if (!rootSignature) {
        if (!CreateVariantRootSignature(device, rootSignature.GetAddressOf())) return nullptr;
    }

    wchar_t psPath[256];
    swprintf_s(psPath, L"Data/Shader/EffectMeshVariantPS_0x%08x.cso", variantKey);

    ComPtr<ID3DBlob> vsBlob, psBlob;
    if (!LoadVariantBlob(L"Data/Shader/EffectMeshVariantVS.cso", vsBlob)) return nullptr;
    if (!LoadVariantBlob(psPath, psBlob)) {
        LOG_ERROR("[MeshVariant] Fallback to Slash_Basic for key 0x%08x", variantKey);
        if (!LoadVariantBlob(L"Data/Shader/EffectMeshVariantPS_0x00004003.cso", psBlob))
            return nullptr;
    }

    D3D12_BLEND_DESC blend = {};
    blend.RenderTarget[0].BlendEnable           = TRUE;
    blend.RenderTarget[0].SrcBlend              = D3D12_BLEND_ONE;
    blend.RenderTarget[0].DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp               = D3D12_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha         = D3D12_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha        = D3D12_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_RASTERIZER_DESC raster = {};
    raster.FillMode        = D3D12_FILL_MODE_SOLID;
    raster.CullMode        = D3D12_CULL_MODE_NONE;
    raster.DepthClipEnable = TRUE;

    D3D12_DEPTH_STENCIL_DESC depth = {};
    depth.DepthEnable    = TRUE;
    depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    depth.DepthFunc      = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature         = rootSignature.Get();
    psoDesc.VS                     = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS                     = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    psoDesc.BlendState             = blend;
    psoDesc.SampleMask             = UINT_MAX;
    psoDesc.RasterizerState        = raster;
    psoDesc.DepthStencilState      = depth;
    psoDesc.InputLayout            = { kMeshInputLayout, _countof(kMeshInputLayout) };
    psoDesc.PrimitiveTopologyType  = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets       = 1;
    psoDesc.RTVFormats[0]          = DXGI_FORMAT_R16G16B16A16_FLOAT;
    psoDesc.DSVFormat              = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count       = 1;

    MeshVariantPSOEntry entry;
    entry.variantKey = variantKey;
    const HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&entry.pso));
    if (FAILED(hr)) {
        LOG_ERROR("[MeshVariant] CreatePSO failed for key 0x%08x", variantKey);
        return nullptr;
    }

    entries.push_back(std::move(entry));
    return entries.back().pso.Get();
}

void MeshVariantPipelineCache::Reset()
{
    entries.clear();
    rootSignature.Reset();
}

MeshVariantPipelineCache& GetMeshVariantPipelineCache()
{
    static MeshVariantPipelineCache s_cache;
    return s_cache;
}
