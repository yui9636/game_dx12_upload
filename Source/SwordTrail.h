#pragma once
#include <vector>
#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>




class SwordTrail
{
public:
    explicit SwordTrail(ID3D11Device* device);

    void SetSwordPos(const DirectX::XMFLOAT3& headWorld,
        const DirectX::XMFLOAT3& tailWorld);
    void Update(float dt);

    void Clear() { usedPosArray.clear(); }

    void Reset();        

    bool GetTailPos(DirectX::XMFLOAT3& outPos) const;

    void Render(ID3D11DeviceContext* deviceContext,
        const DirectX::XMFLOAT4X4& view,
        const DirectX::XMFLOAT4X4& projection,
        D3D11_PRIMITIVE_TOPOLOGY topology);

private:
    static const UINT VertexCapacity = 3 * 1024;
    static constexpr float SegmentLifeSeconds = 0.05f;   


    static const UINT SubdivisionPerSegment = 8;       
    static const UINT MaxRenderVertices=
    VertexCapacity * SubdivisionPerSegment * 6 + 4;





    struct ConstantBufferScene
    {
        DirectX::XMFLOAT4X4 viewProjection;


    };

    struct PosBuffer
    {
        DirectX::XMFLOAT3 head;
        DirectX::XMFLOAT3 tail;
        bool isUsed;
        float life;            
    };

    struct Vertex
    {
        DirectX::XMFLOAT3 pos;   
        DirectX::XMFLOAT2 uv;
    };


    struct Time
    {
        float elapsedSeconds;   
        float padding[3]; 
    };

    struct FlowConst 
    { 
        float swingLength;
        float pad[3];
    };

    static DirectX::XMFLOAT3 Catmull(const DirectX::XMFLOAT3& p0,
        const DirectX::XMFLOAT3& p1,
        const DirectX::XMFLOAT3& p2,
        const DirectX::XMFLOAT3& p3,
        float t);

    std::vector<PosBuffer> usedPosArray;   


    Microsoft::WRL::ComPtr<ID3D11Buffer>            vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>            constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>      vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>       pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>       inputLayout;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>base;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>mask;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>noise;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>normal;


    Microsoft::WRL::ComPtr<ID3D11SamplerState>      samplerState;
    Microsoft::WRL::ComPtr<ID3D11BlendState>        alphaBlendState;

    Microsoft::WRL::ComPtr<ID3D11Buffer> timeConstantBuffer;



};



