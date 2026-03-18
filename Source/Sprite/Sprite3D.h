#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl.h>
#include <string>
#include <memory>

class Sprite3D
{
public:
    Sprite3D(ID3D11Device* device, const char* filename);
    virtual ~Sprite3D() = default;

 
    void Render(ID3D11DeviceContext* dc,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection,
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT3& rotation,
        const DirectX::XMFLOAT2& size,
        const DirectX::XMFLOAT4& color,
        float progress = 1.0f
    );

    void RenderBillboard(ID3D11DeviceContext* dc,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection,
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT2& size,
        const DirectX::XMFLOAT4& color,
        float progress = 1.0f,
        bool verticalFixed = false
    );

    float GetTextureWidth() const { return textureWidth; }

 
    float GetTextureHeight() const { return textureHeight; }

private:
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT2 texcoord;
    };

    struct MatrixData
    {
        DirectX::XMMATRIX world;
        DirectX::XMMATRIX view;
        DirectX::XMMATRIX projection;
    };

    struct UIConstants
    {
        DirectX::XMFLOAT4 colorCore;
        float progress;
        float padding[3];
    };

    Microsoft::WRL::ComPtr<ID3D11VertexShader>       vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>        pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>        inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer>             vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>             matrixBuffer;     // b1
    Microsoft::WRL::ComPtr<ID3D11Buffer>             uiConstantBuffer; // b0
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;

    float textureWidth = 0.0f;
    float textureHeight = 0.0f;

    void DrawInternal(ID3D11DeviceContext* dc, const DirectX::XMMATRIX& world, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection, const DirectX::XMFLOAT4& color, float progress);
};