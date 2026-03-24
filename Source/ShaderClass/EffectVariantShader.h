#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include "ShaderClass/Shader.h"

class Model;
struct RenderContext;

class EffectVariantShader : public EffectShader
{
public:
    EffectVariantShader(ID3D11Device* device);
    virtual ~EffectVariantShader() = default;

    void Begin(const RenderContext& rc) override;

    void Draw(const RenderContext& rc, const ModelResource* modelResource) override;

    void End(const RenderContext& rc) override;

private:
    // --------------------------------------------------------
    // --------------------------------------------------------

    struct CbScene
    {
        DirectX::XMFLOAT4X4 viewProjection;
        DirectX::XMFLOAT4   lightDirection;
        DirectX::XMFLOAT4   lightColor;
        DirectX::XMFLOAT4   cameraPosition;
        DirectX::XMFLOAT4X4 lightViewProjection;
        DirectX::XMFLOAT4   shadowColor;
    };

    struct CbSkeleton
    {
        DirectX::XMFLOAT4X4 boneTransforms[256];
    };

    // --------------------------------------------------------
    // --------------------------------------------------------
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  inputLayout;

    Microsoft::WRL::ComPtr<ID3D11Buffer> sceneConstantBuffer;    // b0
    Microsoft::WRL::ComPtr<ID3D11Buffer> skeletonConstantBuffer; // b6
};
