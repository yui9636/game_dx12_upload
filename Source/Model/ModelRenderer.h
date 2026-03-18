//#pragma once
//
//#include <memory>
//#include <vector>
//#include <wrl.h>
//#include <d3d11.h>
//#include <DirectXMath.h>
//#include "Model/Model.h"
//#include "ShaderClass/Shader.h"
//#include "RenderContext/RenderQueue.h"
//
//enum class ShaderId
//{
//    Phong,
//    PBR,
//    Toon,
//    GBufferPBR,
//    EnumCount
//};
//
//class ModelRenderer
//{
//public:
//    ModelRenderer(ID3D11Device* device);
//    ~ModelRenderer() {}
//
//    void Draw(ShaderId shaderId,
//        std::shared_ptr<Model> model,
//        const DirectX::XMFLOAT4X4& worldMatrix,
//        const DirectX::XMFLOAT4X4& prevWorldMatrix,
//        const DirectX::XMFLOAT4& baseColor,
//        float metallic,
//        float roughness,
//        float emissive,
//        BlendState blend = BlendState::Opaque,
//        DepthState depth = DepthState::TestAndWrite,
//        RasterizerState raster = RasterizerState::SolidCullBack);
//
//    // �Â� Render �͕s�v�ɂȂ�\��ł����݊����̂��ߎc���Ă��܂�
//    void Render(const RenderContext& rc, const RenderQueue& queue);
//    void RenderOpaque(const RenderContext& rc);
//    void RenderTransparent(const RenderContext& rc);
//
//    void SetIBL(const std::string& diffusePath, const std::string& specularPath);
//
//private:
//    // =========================================================
//    // �� �폜: CbScene, CbShadowMap, UpdateConstantBuffers �͏��ł��܂����I
//    // =========================================================
//
//    // �� �X�P���g���p�萔�o�b�t�@�����̓��f���ŗL�̃f�[�^�Ȃ̂Ŏc���܂�
//    struct CbSkeleton
//    {
//        DirectX::XMFLOAT4X4	boneTransforms[256];
//        DirectX::XMFLOAT4X4	prevBoneTransforms[256];
//    };
//
//    struct DrawInfo
//    {
//        ShaderId                shaderId;
//        std::shared_ptr<Model>  model;
//
//        DirectX::XMFLOAT4X4     worldMatrix;
//        DirectX::XMFLOAT4X4     prevWorldMatrix;
//
//        DirectX::XMFLOAT4       baseColor;
//        float                   metallic;
//        float                   roughness;
//        float                   emissive;
//
//        BlendState              blendState;
//        DepthState              depthState;
//        RasterizerState         rasterizerState;
//    };
//
//    struct TransparencyDrawInfo
//    {
//        ShaderId                shaderId;
//        const Model::Mesh* mesh;
//        float                   distance;
//
//        DirectX::XMFLOAT4X4     worldMatrix;
//        DirectX::XMFLOAT4X4     prevWorldMatrix;
//
//        DirectX::XMFLOAT4       baseColor;
//        float                   metallic;
//        float                   roughness;
//        float                   emissive;
//
//        BlendState              blendState;
//        DepthState              depthState;
//        RasterizerState         rasterizerState;
//    };
//
//    std::unique_ptr<Shader>                 shaders[static_cast<int>(ShaderId::EnumCount)];
//    std::vector<DrawInfo>                   drawInfos;
//    std::vector<TransparencyDrawInfo>       transparencyDrawInfos;
//
//    // �� �폜: sceneConstantBuffer, shadowConstantBuffer �͏��ł��܂����I
//    Microsoft::WRL::ComPtr<ID3D11Buffer>    skeletonConstantBuffer;
//
//    ID3D11Device* m_device = nullptr;
//
//    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> currentDiffuseIBL;
//    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> currentSpecularIBL;
//};
#pragma once

#include <memory>
#include <vector>
#include <DirectXMath.h>
#include "Model/Model.h"
#include "ShaderClass/Shader.h"
#include "RenderContext/RenderQueue.h"

// �� �O���錾��ǉ�
class IBuffer;
class IShader;
class ITexture;
class IResourceFactory;

enum class ShaderId
{
    Phong,
    PBR,
    Toon,
    GBufferPBR,
    EnumCount
};

class ModelRenderer
{
public:
    ModelRenderer(IResourceFactory* factory);

    // �� �C���F�f�X�g���N�^��錾�݂̂ɂ���
    ~ModelRenderer();

    void Draw(ShaderId shaderId,
        std::shared_ptr<Model> model,
        const DirectX::XMFLOAT4X4& worldMatrix,
        const DirectX::XMFLOAT4X4& prevWorldMatrix,
        const DirectX::XMFLOAT4& baseColor,
        float metallic,
        float roughness,
        float emissive,
        BlendState blend = BlendState::Opaque,
        DepthState depth = DepthState::TestAndWrite,
        RasterizerState raster = RasterizerState::SolidCullBack);

    void Render(const RenderContext& rc, const RenderQueue& queue);
    void RenderOpaque(const RenderContext& rc);
    void RenderTransparent(const RenderContext& rc);

    void SetIBL(const std::string& diffusePath, const std::string& specularPath);

private:
    struct CbSkeleton
    {
        DirectX::XMFLOAT4X4	boneTransforms[256];
        DirectX::XMFLOAT4X4	prevBoneTransforms[256];
    };

    struct DrawInfo
    {
        ShaderId                shaderId;
        std::shared_ptr<Model>  model;
        DirectX::XMFLOAT4X4     worldMatrix;
        DirectX::XMFLOAT4X4     prevWorldMatrix;
        DirectX::XMFLOAT4       baseColor;
        float                   metallic;
        float                   roughness;
        float                   emissive;
        BlendState              blendState;
        DepthState              depthState;
        RasterizerState         rasterizerState;
    };

    struct TransparencyDrawInfo
    {
        ShaderId                shaderId;
        const Model::Mesh* mesh;
        float                   distance;
        DirectX::XMFLOAT4X4     worldMatrix;
        DirectX::XMFLOAT4X4     prevWorldMatrix;
        DirectX::XMFLOAT4       baseColor;
        float                   metallic;
        float                   roughness;
        float                   emissive;
        BlendState              blendState;
        DepthState              depthState;
        RasterizerState         rasterizerState;
    };

    // �� �C���FShader ���x�[�X�N���X�� RHI �����Ă���ꍇ�� unique_ptr<IShader> ����
    std::unique_ptr<Shader>           shaders[static_cast<int>(ShaderId::EnumCount)];
    std::vector<DrawInfo>             drawInfos;
    std::vector<TransparencyDrawInfo> transparencyDrawInfos;

    // �� �C���F�X�P���g���o�b�t�@�� RHI ���I
    std::unique_ptr<IBuffer>          skeletonConstantBuffer;

    std::shared_ptr<ITexture> currentDiffuseIBL;
    std::shared_ptr<ITexture> currentSpecularIBL;
};