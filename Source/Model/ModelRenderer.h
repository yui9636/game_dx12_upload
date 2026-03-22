#pragma once

#include <memory>
#include <vector>
#include <DirectXMath.h>
#include "Model/ModelResource.h"
#include "ShaderClass/Shader.h"
#include "RenderContext/RenderQueue.h"

class IBuffer;
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
    ~ModelRenderer();

    void Draw(ShaderId shaderId,
        std::shared_ptr<ModelResource> modelResource,
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
    void RenderPreparedOpaque(const RenderContext& rc, bool forceShaderId = false, ShaderId forcedShaderId = ShaderId::PBR);
    void RenderTransparent(const RenderContext& rc);

    void SetIBL(const std::string& diffusePath, const std::string& specularPath);

private:
    struct CbSkeleton
    {
        DirectX::XMFLOAT4X4 boneTransforms[256];
        DirectX::XMFLOAT4X4 prevBoneTransforms[256];
    };

    struct DrawInfo
    {
        ShaderId shaderId;
        std::shared_ptr<ModelResource> modelResource;
        DirectX::XMFLOAT4X4 worldMatrix;
        DirectX::XMFLOAT4X4 prevWorldMatrix;
        DirectX::XMFLOAT4 baseColor;
        float metallic;
        float roughness;
        float emissive;
        BlendState blendState;
        DepthState depthState;
        RasterizerState rasterizerState;
    };

    struct TransparencyDrawInfo
    {
        ShaderId shaderId;
        std::shared_ptr<ModelResource> modelResource;
        int meshIndex = -1;
        float distance = 0.0f;
        DirectX::XMFLOAT4X4 worldMatrix;
        DirectX::XMFLOAT4X4 prevWorldMatrix;
        DirectX::XMFLOAT4 baseColor;
        float metallic;
        float roughness;
        float emissive;
        BlendState blendState;
        DepthState depthState;
        RasterizerState rasterizerState;
    };

    void FillSkeletonConstantBuffer(const ModelResource::MeshResource& meshResource,
        const DirectX::XMFLOAT4X4& worldMatrix,
        const DirectX::XMFLOAT4X4& prevWorldMatrix,
        CbSkeleton& cbSkeleton) const;
    void ApplySkeletonConstantBuffer(const RenderContext& rc, const CbSkeleton& cbSkeleton) const;
    void ApplyMaterialOverrides(ShaderId shaderId, Shader* shader,
        const DirectX::XMFLOAT4& baseColor,
        float metallic,
        float roughness,
        float emissive) const;

    std::unique_ptr<Shader> shaders[static_cast<int>(ShaderId::EnumCount)];
    std::vector<DrawInfo> drawInfos;
    std::vector<TransparencyDrawInfo> transparencyDrawInfos;
    std::unique_ptr<IBuffer> skeletonConstantBuffer;
    std::shared_ptr<ITexture> currentDiffuseIBL;
    std::shared_ptr<ITexture> currentSpecularIBL;
};
