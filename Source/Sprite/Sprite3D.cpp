#include "Sprite3D.h"
#include "GpuResourceUtils.h" 

using namespace DirectX;

Sprite3D::Sprite3D(ID3D11Device* device, const char* filename)
{
    HRESULT hr = S_OK;

    // 1. 頂点バッファ作成 (原点中心 1x1 の平面)
    Vertex vertices[] = {
        { {-0.5f,  0.5f, 0.0f}, {1,1,1,1}, {0.0f, 0.0f} }, // 左上
        { { 0.5f,  0.5f, 0.0f}, {1,1,1,1}, {1.0f, 0.0f} }, // 右上
        { {-0.5f, -0.5f, 0.0f}, {1,1,1,1}, {0.0f, 1.0f} }, // 左下
        { { 0.5f, -0.5f, 0.0f}, {1,1,1,1}, {1.0f, 1.0f} }, // 右下
    };

    D3D11_BUFFER_DESC vbd = {};
    vbd.ByteWidth = sizeof(Vertex) * 4;
    vbd.Usage = D3D11_USAGE_IMMUTABLE;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;

    hr = device->CreateBuffer(&vbd, &initData, vertexBuffer.GetAddressOf());

    // 2. 入力レイアウト定義 & シェーダーロード
    // ★修正点: 先にレイアウト定義を作り、LoadVertexShaderに渡すことで一発で作ります
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    // VS: 3D行列計算用
    // 引数に layoutDesc と inputLayout を渡すことで、内部で CreateInputLayout してくれます
    GpuResourceUtils::LoadVertexShader(
        device,
        "Data/Shader/Sprite3D_VS.cso",
        layoutDesc,
        ARRAYSIZE(layoutDesc),     
        inputLayout.GetAddressOf(), 
        vertexShader.GetAddressOf() 
    );

    // PS: UI描画用
    GpuResourceUtils::LoadPixelShader(
        device, "Data/Shader/Sprite3D_PS.cso",
        pixelShader.GetAddressOf());

    // 3. 定数バッファ作成
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    // 行列用
    cbd.ByteWidth = sizeof(MatrixData);
    device->CreateBuffer(&cbd, nullptr, matrixBuffer.GetAddressOf());

    // UIパラメータ用
    cbd.ByteWidth = sizeof(UIConstants);
    device->CreateBuffer(&cbd, nullptr, uiConstantBuffer.GetAddressOf());

    // 4. テクスチャロード
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
    // 1. ワールド行列の計算 (Scale -> Rotate -> Translate)
    XMMATRIX mScale = XMMatrixScaling(size.x, size.y, 1.0f);

    // 回転 (Y軸回転で斜め配置を実現)
    XMMATRIX mRot = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rotation.x),
        XMConvertToRadians(rotation.y),
        XMConvertToRadians(rotation.z)
    );

    XMMATRIX mTrans = XMMatrixTranslation(position.x, position.y, position.z);

    // 行列合成
    XMMATRIX world = mScale * mRot * mTrans;

    // 共通描画処理へ
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
    // ビルボード行列の計算
    XMMATRIX mScale = XMMatrixScaling(size.x, size.y, 1.0f);
    XMMATRIX mTrans = XMMatrixTranslation(position.x, position.y, position.z);

    // ビュー行列の逆行列を計算して回転成分を取得
    XMVECTOR det;
    XMMATRIX invView = XMMatrixInverse(&det, view);
    XMMATRIX mRot = invView;
    mRot.r[3] = XMVectorSet(0, 0, 0, 1);

    if (verticalFixed)
    {
        // 簡易的なY軸固定（必要なら実装）
    }

    XMMATRIX world = mScale * mRot * mTrans;
    DrawInternal(dc, world, view, projection, color, progress);
}

void Sprite3D::DrawInternal(ID3D11DeviceContext* dc, const XMMATRIX& world, const XMMATRIX& view, const XMMATRIX& projection, const XMFLOAT4& color, float progress)
{
    D3D11_MAPPED_SUBRESOURCE ms;

    // 1. 定数バッファ更新: 行列 (Slot 1)
    if (SUCCEEDED(dc->Map(matrixBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms)))
    {
        MatrixData* data = (MatrixData*)ms.pData;
        data->world = XMMatrixTranspose(world);
        data->view = XMMatrixTranspose(view);
        data->projection = XMMatrixTranspose(projection);
        dc->Unmap(matrixBuffer.Get(), 0);
    }

    // 2. 定数バッファ更新: UIパラメータ (Slot 0)
    if (SUCCEEDED(dc->Map(uiConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms)))
    {
        UIConstants* data = (UIConstants*)ms.pData;
        data->colorCore = color;
        data->progress = progress;
        dc->Unmap(uiConstantBuffer.Get(), 0);
    }

    // 3. パイプライン設定
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    dc->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
    dc->IASetInputLayout(inputLayout.Get());
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // シェーダーバインド
    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
    dc->VSSetConstantBuffers(1, 1, matrixBuffer.GetAddressOf()); // Matrix -> b1

    dc->PSSetShader(pixelShader.Get(), nullptr, 0);
    dc->PSSetConstantBuffers(0, 1, uiConstantBuffer.GetAddressOf()); // UI -> b0
    dc->PSSetShaderResources(0, 1, shaderResourceView.GetAddressOf());

    // 描画
    dc->Draw(4, 0);
}