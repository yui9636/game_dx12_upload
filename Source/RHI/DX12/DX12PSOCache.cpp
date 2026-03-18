#include "DX12PSOCache.h"
#include "DX12Shader.h"
#include "DX12State.h"
#include "Console/Logger.h"
#include <cassert>

ID3D12PipelineState* DX12PSOCache::GetOrCreate(const PipelineStateDesc& desc) {
    size_t h = HashDesc(desc);
    auto it = m_cache.find(h);
    if (it != m_cache.end()) {
        return it->second.Get();
    }
    auto pso = CompilePSO(desc);
    if (!pso) return nullptr;
    auto* raw = pso.Get();
    m_cache[h] = std::move(pso);
    return raw;
}

size_t DX12PSOCache::HashDesc(const PipelineStateDesc& desc) {
    size_t h = 0;
    auto combine = [&h](size_t val) {
        h ^= val + 0x9e3779b9 + (h << 6) + (h >> 2);
    };
    combine(reinterpret_cast<size_t>(desc.vertexShader));
    combine(reinterpret_cast<size_t>(desc.pixelShader));
    combine(reinterpret_cast<size_t>(desc.geometryShader));
    combine(reinterpret_cast<size_t>(desc.inputLayout));
    combine(reinterpret_cast<size_t>(desc.blendState));
    combine(reinterpret_cast<size_t>(desc.rasterizerState));
    combine(reinterpret_cast<size_t>(desc.depthStencilState));
    combine(static_cast<size_t>(desc.primitiveTopology));
    combine(desc.numRenderTargets);
    for (uint32_t i = 0; i < desc.numRenderTargets; ++i)
        combine(static_cast<size_t>(desc.rtvFormats[i]));
    combine(static_cast<size_t>(desc.dsvFormat));
    combine(desc.sampleCount);
    combine(desc.sampleMask);
    return h;
}

DXGI_FORMAT DX12PSOCache::ToDXGIFormat(TextureFormat format) {
    switch (format) {
    case TextureFormat::RGBA8_UNORM:          return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::R16G16B16A16_FLOAT:   return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case TextureFormat::R32G32B32A32_FLOAT:   return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case TextureFormat::R32G32B32A32_UINT:    return DXGI_FORMAT_R32G32B32A32_UINT;
    case TextureFormat::R32G32B32_FLOAT:      return DXGI_FORMAT_R32G32B32_FLOAT;
    case TextureFormat::R32G32_FLOAT:         return DXGI_FORMAT_R32G32_FLOAT;
    case TextureFormat::R16G16_FLOAT:         return DXGI_FORMAT_R16G16_FLOAT;
    case TextureFormat::R8_UNORM:             return DXGI_FORMAT_R8_UNORM;
    case TextureFormat::D32_FLOAT:            return DXGI_FORMAT_D32_FLOAT;
    case TextureFormat::D24_UNORM_S8_UINT:    return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case TextureFormat::R32_TYPELESS:         return DXGI_FORMAT_D32_FLOAT;
    default:                                  return DXGI_FORMAT_UNKNOWN;
    }
}

ComPtr<ID3D12PipelineState> DX12PSOCache::CompilePSO(const PipelineStateDesc& desc) {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

    // Shaders
    if (desc.vertexShader) {
        auto* vs = static_cast<DX12Shader*>(desc.vertexShader);
        psoDesc.VS = { vs->GetByteCode(), vs->GetByteCodeSize() };
    }
    if (desc.pixelShader) {
        auto* ps = static_cast<DX12Shader*>(desc.pixelShader);
        psoDesc.PS = { ps->GetByteCode(), ps->GetByteCodeSize() };
    }
    if (desc.geometryShader) {
        auto* gs = static_cast<DX12Shader*>(desc.geometryShader);
        psoDesc.GS = { gs->GetByteCode(), gs->GetByteCodeSize() };
    }

    // Input layout
    if (desc.inputLayout) {
        auto* il = static_cast<DX12InputLayout*>(desc.inputLayout);
        psoDesc.InputLayout = { il->GetElements(), il->GetNumElements() };
    }

    // Depth stencil state
    if (desc.depthStencilState) {
        auto* dss = dynamic_cast<DX12DepthStencilState*>(desc.depthStencilState);
        if (dss) psoDesc.DepthStencilState = dss->GetDesc();
    } else {
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
    }

    // Rasterizer state
    if (desc.rasterizerState) {
        auto* rs = dynamic_cast<DX12RasterizerState*>(desc.rasterizerState);
        if (rs) psoDesc.RasterizerState = rs->GetDesc();
    } else {
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        psoDesc.RasterizerState.DepthClipEnable = TRUE;
    }

    // Blend state
    if (desc.blendState) {
        auto* bs = dynamic_cast<DX12BlendState*>(desc.blendState);
        if (bs) psoDesc.BlendState = bs->GetDesc();
    } else {
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }

    // Root signature
    psoDesc.pRootSignature = m_rootSig->Get();

    // Render target formats
    psoDesc.NumRenderTargets = desc.numRenderTargets;
    for (uint32_t i = 0; i < desc.numRenderTargets; ++i) {
        psoDesc.RTVFormats[i] = ToDXGIFormat(desc.rtvFormats[i]);
    }
    psoDesc.DSVFormat = ToDXGIFormat(desc.dsvFormat);

    // Primitive topology type
    switch (desc.primitiveTopology) {
    case PrimitiveTopology::TriangleList:
    case PrimitiveTopology::TriangleStrip:
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; break;
    case PrimitiveTopology::LineList:
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE; break;
    case PrimitiveTopology::PointList:
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; break;
    default:
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; break;
    }

    // MSAA
    psoDesc.SampleDesc.Count = (desc.sampleCount > 0) ? desc.sampleCount : 1;
    psoDesc.SampleDesc.Quality = 0;
    psoDesc.SampleMask = desc.sampleMask;

    ComPtr<ID3D12PipelineState> pso;
    HRESULT hr = m_device->GetDevice()->CreateGraphicsPipelineState(
        &psoDesc, IID_PPV_ARGS(&pso));
    if (FAILED(hr)) {
        LOG_ERROR("[DX12PSOCache] Failed to compile PSO hr=0x%08X rt0=%d dsv=%d numRT=%u topo=%d",
            static_cast<unsigned int>(hr),
            desc.numRenderTargets > 0 ? static_cast<int>(desc.rtvFormats[0]) : -1,
            static_cast<int>(desc.dsvFormat),
            desc.numRenderTargets,
            static_cast<int>(desc.primitiveTopology));
        return nullptr;
    }
    return pso;
}
