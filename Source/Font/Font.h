#pragma once

#include <vector>
#include <memory>
#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <string>

class Font
{
public:
    Font(ID3D11Device* device, const char* filename, int maxSpriteCount = 2048);
    virtual ~Font() = default;

    void Begin(ID3D11DeviceContext* context);

    void Draw(float x, float y, const wchar_t* string);

    void Draw3D(DirectX::CXMMATRIX world, DirectX::CXMMATRIX view, DirectX::CXMMATRIX projection, const wchar_t* string);

    float GetTextWidth(const wchar_t* string);

    void End(ID3D11DeviceContext* context);

    void SetScale(float x, float y) { scaleX = x; scaleY = y; }
    void SetColor(const DirectX::XMFLOAT4& color) { fontColor = color; }

    void SetSDFParams(float threshold = 0.5f, float softness = 0.5f);

private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader>       vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>        pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>        inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer>             vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>             indexBuffer;

    Microsoft::WRL::ComPtr<ID3D11Buffer>             sdfConstantBuffer;

    Microsoft::WRL::ComPtr<ID3D11Buffer>             matrixBuffer;

    Microsoft::WRL::ComPtr<ID3D11BlendState>         blendState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>    rasterizerState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  depthStencilState;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>       samplerState;

    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> shaderResourceViews;

    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT2 texcoord;
    };

    struct SDFData
    {
        DirectX::XMFLOAT4 Color;
        float Threshold;            // 臒l
        float Softness;
        float Padding[2];
    };

    struct CBMatrix
    {
        DirectX::XMFLOAT4X4 World;
        DirectX::XMFLOAT4X4 View;
        DirectX::XMFLOAT4X4 Projection;
    };

    
    struct CharacterInfo
    {
        static const WORD NonCode = 0;
        static const WORD EndCode = 0xFFFF;
        static const WORD ReturnCode = 0xFFFE;
        static const WORD TabCode = 0xFFFD;
        static const WORD SpaceCode = 0xFFFC;

        float left, top, right, bottom;
        float xoffset, yoffset, xadvance;
        float width, height;
        int   page;
        bool  ascii;
    };

    struct Subset
    {
        ID3D11ShaderResourceView* shaderResourceView;
        UINT startIndex;
        UINT indexCount;
    };

    float fontWidth;
    float fontHeight;
    int   textureCount;
    int   characterCount;

    std::vector<CharacterInfo> characterInfos;
    std::vector<WORD>          characterIndices;
    std::vector<Subset>        subsets;

    Vertex* currentVertex = nullptr;
    UINT    currentIndexCount = 0;
    int     currentPage = -1;

    float screenWidth = 0.0f;
    float screenHeight = 0.0f;

    float scaleX = 1.0f;
    float scaleY = 1.0f;
    DirectX::XMFLOAT4 fontColor = { 1.0f, 1.0f, 1.0f, 1.0f };

    float sdfThreshold = 0.5f;
    float sdfSoftness = 0.5f;

    bool is3DMode = false;
    
    DirectX::XMFLOAT4X4 currentWorld;
    DirectX::XMFLOAT4X4 currentView;
    DirectX::XMFLOAT4X4 currentProj;
};
