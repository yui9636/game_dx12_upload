#include "DX11CommandList.h"
#include <d3d11.h> 
#include <vector>

// 実装クラスのヘッダーをインクルード（ダウンキャスト用）
#include "RHI/ITexture.h" 
#include "RHI/IBuffer.h"
#include "RHI/DX11/DX11Buffer.h"
#include "RHI/IShader.h"
#include "RHI/DX11/DX11Shader.h"
#include "RHI/ISampler.h"
#include "RHI/DX11/DX11Sampler.h"
#include "RHI/IState.h"
#include "RHI/DX11/DX11State.h"
#include "DX11Texture.h"
#include "RHI/IPipelineState.h"

DX11CommandList::DX11CommandList(ID3D11DeviceContext* dc)
    : m_dc(dc)
{
}

DX11CommandList::~DX11CommandList() = default;

ID3D11DeviceContext* DX11CommandList::GetNativeContext()
{
    return m_dc.Get();
}

// =========================================================
// 描画・バッファ更新
// =========================================================

void DX11CommandList::Draw(uint32_t vertexCount, uint32_t startVertexLocation)
{
    if (m_dc) m_dc->Draw(vertexCount, startVertexLocation);
}

void DX11CommandList::DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation)
{
    if (m_dc) m_dc->DrawIndexed(indexCount, startIndexLocation, baseVertexLocation);
}

void DX11CommandList::UpdateBuffer(IBuffer* buffer, const void* data, uint32_t size) {
    auto dxBuffer = static_cast<DX11Buffer*>(buffer);
    if (dxBuffer && m_dc) {
        // IBuffer 経由ではなく、DX11Buffer の GetNative() を直接叩く
        m_dc->UpdateSubresource(dxBuffer->GetNative(), 0, nullptr, data, 0, 0);
    }
}

// =========================================================
// シェーダー・リソースバインド
// =========================================================

void DX11CommandList::VSSetShader(IShader* shader) {
    auto dxShader = static_cast<DX11Shader*>(shader);
    auto native = dxShader ? static_cast<ID3D11VertexShader*>(dxShader->GetNative()) : nullptr;
    m_dc->VSSetShader(native, nullptr, 0);
}

void DX11CommandList::PSSetShader(IShader* shader) {
    auto dxShader = static_cast<DX11Shader*>(shader);
    auto native = dxShader ? static_cast<ID3D11PixelShader*>(dxShader->GetNative()) : nullptr;
    m_dc->PSSetShader(native, nullptr, 0);
}

void DX11CommandList::GSSetShader(IShader* shader) {
    auto dxShader = static_cast<DX11Shader*>(shader);
    auto native = dxShader ? static_cast<ID3D11GeometryShader*>(dxShader->GetNative()) : nullptr;
    m_dc->GSSetShader(native, nullptr, 0);
}

void DX11CommandList::CSSetShader(IShader* shader) {
    auto dxShader = static_cast<DX11Shader*>(shader);
    auto native = dxShader ? static_cast<ID3D11ComputeShader*>(dxShader->GetNative()) : nullptr;
    m_dc->CSSetShader(native, nullptr, 0);
}

void DX11CommandList::VSSetConstantBuffer(uint32_t slot, IBuffer* buffer) {
    auto dxBuffer = static_cast<DX11Buffer*>(buffer);
    ID3D11Buffer* native = dxBuffer ? dxBuffer->GetNative() : nullptr;
    m_dc->VSSetConstantBuffers(slot, 1, &native);
}

void DX11CommandList::PSSetConstantBuffer(uint32_t slot, IBuffer* buffer) {
    auto dxBuffer = static_cast<DX11Buffer*>(buffer);
    ID3D11Buffer* native = dxBuffer ? dxBuffer->GetNative() : nullptr;
    m_dc->PSSetConstantBuffers(slot, 1, &native);
}

void DX11CommandList::CSSetConstantBuffer(uint32_t slot, IBuffer* buffer) {
    auto dxBuffer = static_cast<DX11Buffer*>(buffer);
    ID3D11Buffer* native = dxBuffer ? dxBuffer->GetNative() : nullptr;
    m_dc->CSSetConstantBuffers(slot, 1, &native);
}

void DX11CommandList::PSSetTexture(uint32_t slot, ITexture* texture) {
    auto dxTex = static_cast<DX11Texture*>(texture);
    ID3D11ShaderResourceView* srv = dxTex ? dxTex->GetNativeSRV() : nullptr;
    m_dc->PSSetShaderResources(slot, 1, &srv);
}

void DX11CommandList::PSSetTextures(uint32_t startSlot, uint32_t numTextures, ITexture* const* ppTextures) {
    if (numTextures == 0) return;

    // std::vector を使わず、スタック上の固定配列を使う（16スロットあれば十分）
    ID3D11ShaderResourceView* srvs[16];
    uint32_t count = (numTextures > 16) ? 16 : numTextures;

    for (uint32_t i = 0; i < count; ++i) {
        srvs[i] = ppTextures[i] ? static_cast<DX11Texture*>(ppTextures[i])->GetNativeSRV() : nullptr;
    }
    m_dc->PSSetShaderResources(startSlot, count, srvs);
}


void DX11CommandList::PSSetSampler(uint32_t slot, ISampler* sampler) {
    auto dxSampler = static_cast<DX11Sampler*>(sampler);
    ID3D11SamplerState* native = dxSampler ? dxSampler->GetNative() : nullptr;
    m_dc->PSSetSamplers(slot, 1, &native);
}

void DX11CommandList::PSSetSamplers(uint32_t startSlot, uint32_t numSamplers, ISampler* const* ppSamplers) {
    if (numSamplers == 0 || !ppSamplers) return;
    std::vector<ID3D11SamplerState*> nativeSamplers(numSamplers, nullptr);
    for (uint32_t i = 0; i < numSamplers; ++i) {
        if (ppSamplers[i]) {
            nativeSamplers[i] = static_cast<DX11Sampler*>(ppSamplers[i])->GetNative();
        }
    }
    m_dc->PSSetSamplers(startSlot, numSamplers, nativeSamplers.data());
}

// =========================================================
// ステート・パイプライン設定
// =========================================================

void DX11CommandList::SetViewport(const RhiViewport& viewport)
{
    D3D11_VIEWPORT dx11vp;
    dx11vp.TopLeftX = viewport.topLeftX;
    dx11vp.TopLeftY = viewport.topLeftY;
    dx11vp.Width = viewport.width;
    dx11vp.Height = viewport.height;
    dx11vp.MinDepth = viewport.minDepth;
    dx11vp.MaxDepth = viewport.maxDepth;

    m_dc->RSSetViewports(1, &dx11vp);
}

void DX11CommandList::SetPrimitiveTopology(PrimitiveTopology topology) {
    D3D11_PRIMITIVE_TOPOLOGY dxTopo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    switch (topology) {
    case PrimitiveTopology::TriangleList:  dxTopo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
    case PrimitiveTopology::TriangleStrip: dxTopo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
    case PrimitiveTopology::LineList:      dxTopo = D3D11_PRIMITIVE_TOPOLOGY_LINELIST; break;
    case PrimitiveTopology::PointList:     dxTopo = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST; break;
    }
    m_dc->IASetPrimitiveTopology(dxTopo);
}

void DX11CommandList::SetRenderTarget(ITexture* renderTarget, ITexture* depthStencil) {
    if (renderTarget == nullptr) {
        // 影の描画や解除時は、正しく「0個」として渡す
        SetRenderTargets(0, nullptr, depthStencil);
    }
    else {
        SetRenderTargets(1, &renderTarget, depthStencil);
    }
}

void DX11CommandList::SetRenderTargets(uint32_t numRenderTargets, ITexture* const* renderTargets, ITexture* depthStencil) {
    // ====================================================
    // ❌ 修正：ここにあった早期リターンを完全に削除！！！
    // 0個、かつ depthStencil == nullptr の時は「全解除」の重要コマンドです
    // ====================================================

    std::vector<ID3D11RenderTargetView*> rtvs;
    if (numRenderTargets > 0 && renderTargets) {
        rtvs.resize(numRenderTargets, nullptr);
        for (uint32_t i = 0; i < numRenderTargets; ++i) {
            if (renderTargets[i]) {
                rtvs[i] = static_cast<DX11Texture*>(renderTargets[i])->GetNativeRTV();
            }
        }
    }

    auto dxDS = static_cast<DX11Texture*>(depthStencil);
    ID3D11DepthStencilView* dsv = dxDS ? dxDS->GetNativeDSV() : nullptr;

    // NumViews が 0 の時は rtvs.data() ではなく nullptr を渡す
    m_dc->OMSetRenderTargets(numRenderTargets, numRenderTargets > 0 ? rtvs.data() : nullptr, dsv);
}


void DX11CommandList::ClearColor(ITexture* renderTarget, const float color[4]) {
    auto dxRT = static_cast<DX11Texture*>(renderTarget);
    if (dxRT && dxRT->GetNativeRTV()) {
        m_dc->ClearRenderTargetView(dxRT->GetNativeRTV(), color);
    }
}

void DX11CommandList::ClearDepthStencil(ITexture* depthStencil, float depth, uint8_t stencil) {
    auto dxDS = static_cast<DX11Texture*>(depthStencil);
    if (dxDS && dxDS->GetNativeDSV()) {
        m_dc->ClearDepthStencilView(dxDS->GetNativeDSV(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil);
    }
}

void DX11CommandList::TransitionBarrier(ITexture* texture, ResourceState newState)
{
    // 現在の状態と変わらないなら何もしない
    if (texture->GetCurrentState() == newState) return;

    // ★ 描画を復活させるための核心：RTV/DSVとして使う前に、全SRVスロットから強制解除する
    if (newState == ResourceState::RenderTarget || newState == ResourceState::DepthWrite) {
        ID3D11ShaderResourceView* nullSRVs[16] = { nullptr };
        // ピクセルシェーダーと頂点シェーダーの両方から引き剥がす
        m_dc->PSSetShaderResources(0, 16, nullSRVs);
        m_dc->VSSetShaderResources(0, 16, nullSRVs);
    }

    texture->SetCurrentState(newState);
}


void DX11CommandList::SetBindGroup(ShaderStage stage, uint32_t index, IBind* bind)
{
}

void DX11CommandList::SetPipelineState(IPipelineState* pso)
{
    if (!pso || !m_dc) return;

    const PipelineStateDesc& desc = pso->GetDesc();

    // 1. シェーダーのバインド
    VSSetShader(desc.vertexShader);
    PSSetShader(desc.pixelShader);
    GSSetShader(desc.geometryShader);
    CSSetShader(desc.computeShader);

    // 2. IA (Input Assembler) ステートのバインド
    SetInputLayout(desc.inputLayout);
    SetPrimitiveTopology(desc.primitiveTopology);

    // 3. RS / OM ステートのバインド
    SetRasterizerState(desc.rasterizerState);
    SetBlendState(desc.blendState, nullptr, desc.sampleMask);
    SetDepthStencilState(desc.depthStencilState, 0); // ステンシル参照値は通常別コマンドで指定

}



void DX11CommandList::SetInputLayout(IInputLayout* layout) {
    auto dxLayout = static_cast<DX11InputLayout*>(layout);
    m_dc->IASetInputLayout(dxLayout ? dxLayout->GetNative() : nullptr);
}

void DX11CommandList::SetVertexBuffer(uint32_t slot, IBuffer* buffer, uint32_t stride, uint32_t offset) {
    auto dxBuffer = static_cast<DX11Buffer*>(buffer);
    ID3D11Buffer* native = dxBuffer ? dxBuffer->GetNative() : nullptr;
    m_dc->IASetVertexBuffers(slot, 1, &native, &stride, &offset);
}

void DX11CommandList::SetIndexBuffer(IBuffer* buffer, IndexFormat format, uint32_t offset) {
    auto dxBuffer = static_cast<DX11Buffer*>(buffer);
    ID3D11Buffer* native = dxBuffer ? dxBuffer->GetNative() : nullptr;
    DXGI_FORMAT dxFormat = (format == IndexFormat::Uint16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    m_dc->IASetIndexBuffer(native, dxFormat, offset);
}

void DX11CommandList::SetDepthStencilState(IDepthStencilState* state, uint32_t stencilRef) {
    auto dxState = static_cast<DX11DepthStencilState*>(state);
    m_dc->OMSetDepthStencilState(dxState ? dxState->GetNative() : nullptr, stencilRef);
}

void DX11CommandList::SetRasterizerState(IRasterizerState* state) {
    auto dxState = static_cast<DX11RasterizerState*>(state);
    m_dc->RSSetState(dxState ? dxState->GetNative() : nullptr);
}

void DX11CommandList::SetBlendState(IBlendState* state, const float blendFactor[4], uint32_t sampleMask) {
    auto dxState = static_cast<DX11BlendState*>(state);
    m_dc->OMSetBlendState(dxState ? dxState->GetNative() : nullptr, blendFactor, sampleMask);
}


