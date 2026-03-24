#include "SwordTrail.h"
#include "GpuResourceUtils.h"
#include <algorithm>
#include <cstring>


using namespace DirectX;

XMFLOAT3 SwordTrail::Catmull(const XMFLOAT3& p0, const XMFLOAT3& p1,
    const XMFLOAT3& p2, const XMFLOAT3& p3,
    float t)
{
    XMFLOAT3 result;
    XMStoreFloat3(&result,
        XMVectorCatmullRom(XMLoadFloat3(&p0),
            XMLoadFloat3(&p1),
            XMLoadFloat3(&p2),
            XMLoadFloat3(&p3), t));
    return result;
}

SwordTrail::SwordTrail(ID3D11Device* device)
{
 
    D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    GpuResourceUtils::LoadVertexShader(device, "Data/Shader/SwordTrailVS.cso",
        inputElementDesc, _countof(inputElementDesc),
        inputLayout.GetAddressOf(),
        vertexShader.GetAddressOf());

    GpuResourceUtils::LoadPixelShader(device, "Data/Shader/SwordTrailPS.cso",
        pixelShader.GetAddressOf());

    {
        D3D11_TEXTURE2D_DESC dummy{};
        //GpuResourceUtils::LoadTexture(device, "Data/Texture/Trail/BladeTrail.png",
        //    base.GetAddressOf());
        //GpuResourceUtils::LoadTexture(device, "Data/Texture/Trail/BladeTrail2.png",
        //    mask.GetAddressOf());
        //GpuResourceUtils::LoadTexture(device, "Data/Texture/Trail/BladeTrail3.png",
        //    noise.GetAddressOf());


 
    
     
        //Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediateContext;
        //device->GetImmediateContext(immediateContext.GetAddressOf());
        //immediateContext->GenerateMips(base.Get());
    }

    GpuResourceUtils::CreateConstantBuffer(device, sizeof(ConstantBufferScene),
        constantBuffer.GetAddressOf());

    GpuResourceUtils::CreateConstantBuffer(device,
        sizeof(Time),
        timeConstantBuffer.GetAddressOf());

    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.ByteWidth = sizeof(Vertex) * MaxRenderVertices;
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&vbDesc, nullptr, vertexBuffer.GetAddressOf());


    D3D11_SAMPLER_DESC sampDesc{};
    sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
    sampDesc.MaxAnisotropy = 8;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP; 
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    device->CreateSamplerState(&sampDesc, samplerState.GetAddressOf());


    D3D11_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->CreateBlendState(&blendDesc, alphaBlendState.GetAddressOf());

    usedPosArray.reserve(VertexCapacity);
}

//void SwordTrail::SetSwordPos(const XMFLOAT3& head, const XMFLOAT3& tail)
//{
//    usedPosArray.insert(usedPosArray.begin(), { head, tail });
//    if (usedPosArray.size() > VertexCapacity)
//        usedPosArray.resize(VertexCapacity);
//}
//


void SwordTrail::SetSwordPos(const XMFLOAT3& head, const XMFLOAT3& tail)
{
    usedPosArray.insert(usedPosArray.begin(),
        { head, tail, true, SegmentLifeSeconds });   
    if (usedPosArray.size() > VertexCapacity)
        usedPosArray.resize(VertexCapacity);
}

void SwordTrail::Update(float dt)                  
{
  
    for (auto& p : usedPosArray) p.life -= dt;      

    while (!usedPosArray.empty() && usedPosArray.back().life <= 0.0f)
        usedPosArray.pop_back();
}


void SwordTrail::Reset()
{

    usedPosArray.clear();
}

bool SwordTrail::GetTailPos(DirectX::XMFLOAT3& outPos) const
{
    if (usedPosArray.empty()) return false;
    outPos = usedPosArray.back().tail;
    return true;
}




void SwordTrail::Render(ID3D11DeviceContext* dc,
    const DirectX::XMFLOAT4X4& view,
    const DirectX::XMFLOAT4X4& projection,
    D3D11_PRIMITIVE_TOPOLOGY /*unused*/)
{
    using namespace DirectX;


    if (usedPosArray.size() < 2) return;

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    dc->GetDevice(device.GetAddressOf());

    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
    dc->PSSetShader(pixelShader.Get(), nullptr, 0);
    dc->IASetInputLayout(inputLayout.Get());

    ID3D11ShaderResourceView* srvs[3] = {
        base.Get(), mask.Get(), noise.Get()
    };
    dc->PSSetShaderResources(0, 3, srvs);

    const float blendFactor[4] = { 0,0,0,0 };
    dc->OMSetBlendState(alphaBlendState.Get(), blendFactor, 0xffffffff);

    ConstantBufferScene cb{};
    XMStoreFloat4x4(&cb.viewProjection,
        XMLoadFloat4x4(&view) * XMLoadFloat4x4(&projection));
    dc->UpdateSubresource(constantBuffer.Get(), 0, nullptr, &cb, 0, 0);
    dc->VSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());

    constexpr int  Subdiv = 16;                      
    const    int  segCount = static_cast<int>(usedPosArray.size()) - 1;
    const    int  sampleCount = segCount * Subdiv + 1;

    std::vector<Vertex> vertices;
    vertices.reserve(sampleCount * 2);

    auto catHead = [&](int idx, float t)
        {
            int i0 = (idx == 0) ? idx : idx - 1;
            int i1 = idx;
            int i2 = idx + 1;
            int i3 = (i2 == segCount) ? i2 : i2 + 1;
            return Catmull(usedPosArray[i0].head,
                usedPosArray[i1].head,
                usedPosArray[i2].head,
                usedPosArray[i3].head, t);
        };
    auto catTail = [&](int idx, float t)
        {
            int i0 = (idx == 0) ? idx : idx - 1;
            int i1 = idx;
            int i2 = idx + 1;
            int i3 = (i2 == segCount) ? i2 : i2 + 1;
            return Catmull(usedPosArray[i0].tail,
                usedPosArray[i1].tail,
                usedPosArray[i2].tail,
                usedPosArray[i3].tail, t);
        };

    float v = 0.0f;
    float vStep = 1.0f / float(sampleCount - 1);

    vertices.push_back({ catHead(0, 0.0f), XMFLOAT2(/* ì™ë§ */0.0f, v) });
    vertices.push_back({ catTail(0, 0.0f), XMFLOAT2(/* êKîˆë§ */1.0f, v) });

    for (int seg = 0; seg < segCount; ++seg)
    {
        for (int s = 1; s <= Subdiv; ++s)
        {
            v += vStep;
            float tLoc = float(s) / float(Subdiv);

            vertices.push_back({ catHead(seg, tLoc), XMFLOAT2(0.0f, v) });
            vertices.push_back({ catTail(seg, tLoc), XMFLOAT2(1.0f, v) });
        }
    }

    std::vector<uint32_t> indices;
    indices.reserve(vertices.size() * 3);

    for (uint32_t i = 0; i < vertices.size() - 2; i += 2)
    {
        indices.push_back(i);
        indices.push_back(i + 1);
        indices.push_back(i + 2);

        indices.push_back(i + 1);
        indices.push_back(i + 3);
        indices.push_back(i + 2);
    }

    D3D11_MAPPED_SUBRESOURCE mappedVB{};
    dc->Map(vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVB);
    std::memcpy(mappedVB.pData, vertices.data(),
        sizeof(Vertex) * vertices.size());
    dc->Unmap(vertexBuffer.Get(), 0);

    static Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
    static size_t indexCapacity = 0;

    if (!indexBuffer || indexCapacity < indices.size())
    {
        indexCapacity = indices.size() + 256;   
        D3D11_BUFFER_DESC ibDesc{};
        ibDesc.ByteWidth = UINT(sizeof(uint32_t) * indexCapacity);
        ibDesc.Usage = D3D11_USAGE_DYNAMIC;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer(&ibDesc, nullptr, indexBuffer.GetAddressOf());
    }

    D3D11_MAPPED_SUBRESOURCE mappedIB{};
    dc->Map(indexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedIB);
    std::memcpy(mappedIB.pData, indices.data(),
        sizeof(uint32_t) * indices.size());
    dc->Unmap(indexBuffer.Get(), 0);

    UINT stride = sizeof(Vertex), offset = 0;
    dc->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
    dc->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dc->DrawIndexed(UINT(indices.size()), 0, 0);

    ID3D11ShaderResourceView* nullSRV[3] = { nullptr, nullptr, nullptr };
    dc->PSSetShaderResources(0, 3, nullSRV);
    dc->OMSetBlendState(nullptr, nullptr, 0xffffffff);
}
