#include "Sprite3D.h"
#include "GpuResourceUtils.h" 

using namespace DirectX;

Sprite3D::Sprite3D(ID3D11Device* device, const char* filename)
{
    HRESULT hr = S_OK;

    Vertex vertices[] = {
        { {-0.5f,  0.5f, 0.0f}, {1,1,1,1}, {0.0f, 0.0f} },
        { { 0.5f,  0.5f, 0.0f}, {1,1,1,1}, {1.0f, 0.0f} },
        { {-0.5f, -0.5f, 0.0f}, {1,1,1,1}, {0.0f, 1.0f} },
        { { 0.5f, -0.5f, 0.0f}, {1,1,1,1}, {1.0f, 1.0f} },
    };

    D3D11_BUFFER_DESC vbd = {};
    vbd.ByteWidth = sizeof(Vertex) * 4;
    vbd.Usage = D3D11_USAGE_IMMUTABLE;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;

    hr = device->CreateBuffer(&vbd, &initData, vertexBuffer.GetAddressOf());

    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    GpuResourceUtils::LoadVertexShader(
        device,
        "Data/Shader/Sprite3D_VS.cso",
        layoutDesc,
        ARRAYSIZE(layoutDesc),     
        inputLayout.GetAddressOf(), 
        vertexShader.GetAddressOf() 
    );

    GpuResourceUtils::LoadPixelShader(
        device, "Data/Shader/Sprite3D_PS.cso",
        pixelShader.GetAddressOf());

    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    cbd.ByteWidth = sizeof(MatrixData);
    device->CreateBuffer(&cbd, nullptr, matrixBuffer.GetAddressOf());

    cbd.ByteWidth = sizeof(UIConstants);
    device->CreateBuffer(&cbd, nullptr, uiConstantBuffer.GetAddressOf());

    if (filename)
    {
        D3D11_TEXTURE2D_DESC desc;
        GpuResourceUtils::LoadTexture(device, filename, shaderResourceView.GetAddressOf(), &desc);
        textureWidth = static_cast<float>(desc.Width);
        textureHeight = static_cast<float>(desc.Height);
    }
    else
    {
        GpuResourceUtils::CreateDummyTexture(device, 0xFFFFFFFF, shaderResourceView.GetAddressOf());
        textureWidth = 1.0f;
        textureHeight = 1.0f;
    }
}

void Sprite3D::Render(ID3D11DeviceContext* dc,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    const XMFLOAT3& position,
    const XMFLOAT3& rotation,
    const XMFLOAT2& size,
    const XMFLOAT4& color,
    float progress)
{
    XMMATRIX mScale = XMMatrixScaling(size.x, size.y, 1.0f);

    XMMATRIX mRot = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rotation.x),
        XMConvertToRadians(rotation.y),
        XMConvertToRadians(rotation.z)
    );

    XMMATRIX mTrans = XMMatrixTranslation(position.x, position.y, position.z);

    XMMATRIX world = mScale * mRot * mTrans;

    DrawInternal(dc, world, view, projection, color, progress);
}

void Sprite3D::RenderBillboard(ID3D11DeviceContext* dc,
    const XMMATRIX& view,
    const XMMATRIX& projection,
    const XMFLOAT3& position,
    const XMFLOAT2& size,
    const XMFLOAT4& color,
    float progress,
    bool verticalFixed)
{
    XMMATRIX mScale = XMMatrixScaling(size.x, size.y, 1.0f);
    XMMATRIX mTrans = XMMatrixTranslation(position.x, position.y, position.z);

    XMVECTOR det;
    XMMATRIX invView = XMMatrixInverse(&det, view);
    XMMATRIX mRot = invView;
    mRot.r[3] = XMVectorSet(0, 0, 0, 1);

    if (verticalFixed)
    {
    }

    XMMATRIX world = mScale * mRot * mTrans;
    DrawInternal(dc, world, view, projection, color, progress);
}

void Sprite3D::DrawInternal(ID3D11DeviceContext* dc, const XMMATRIX& world, const XMMATRIX& view, const XMMATRIX& projection, const XMFLOAT4& color, float progress)
{
    D3D11_MAPPED_SUBRESOURCE ms;

    if (SUCCEEDED(dc->Map(matrixBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms)))
    {
        MatrixData* data = (MatrixData*)ms.pData;
        data->world = XMMatrixTranspose(world);
        data->view = XMMatrixTranspose(view);
        data->projection = XMMatrixTranspose(projection);
        dc->Unmap(matrixBuffer.Get(), 0);
    }

    if (SUCCEEDED(dc->Map(uiConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms)))
    {
        UIConstants* data = (UIConstants*)ms.pData;
        data->colorCore = color;
        data->progress = progress;
        dc->Unmap(uiConstantBuffer.Get(), 0);
    }

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    dc->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
    dc->IASetInputLayout(inputLayout.Get());
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
    dc->VSSetConstantBuffers(1, 1, matrixBuffer.GetAddressOf()); // Matrix -> b1

    dc->PSSetShader(pixelShader.Get(), nullptr, 0);
    dc->PSSetConstantBuffers(0, 1, uiConstantBuffer.GetAddressOf()); // UI -> b0
    dc->PSSetShaderResources(0, 1, shaderResourceView.GetAddressOf());

    dc->Draw(4, 0);
}
