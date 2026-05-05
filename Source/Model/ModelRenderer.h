#pragma once
// モデル描画用のレンダラと描画情報を定義するヘッダです。

#include <memory>
#include <vector>
#include <DirectXMath.h>
#include "Model/ModelResource.h"
#include "ShaderClass/Shader.h"
#include "RenderContext/RenderQueue.h"

class IBuffer;
class ITexture;
class IResourceFactory;
class MaterialAsset;

// モデル描画に使うシェーダー種別を表します。
enum class ShaderId
{
    Phong,
    PBR,
    Toon,
    GBufferPBR,
    EnumCount
};

// モデル描画要求を管理し、シェーダーごとに描画を実行します。
class ModelRenderer
{
public:
    // ModelRenderer を生成し、描画リソースを初期化します。
    ModelRenderer(IResourceFactory* factory);
    // ModelRenderer が保持する描画リソースを破棄します。
    ~ModelRenderer();

    // モデル描画要求を登録します。
    void Draw(ShaderId shaderId,
        std::shared_ptr<ModelResource> modelResource,
        const DirectX::XMFLOAT4X4& worldMatrix,
        const DirectX::XMFLOAT4X4& prevWorldMatrix,
        const DirectX::XMFLOAT4& baseColor,
        float metallic,
        float roughness,
        float emissive,
        const MaterialAsset* materialAsset = nullptr,
        BlendState blend = BlendState::Opaque,
        DepthState depth = DepthState::TestAndWrite,
        RasterizerState raster = RasterizerState::SolidCullBack);

    // 描画キューに登録されたモデルを描画します。
    void Render(const RenderContext& rc, const RenderQueue& queue);
    // 不透明モデルを描画します。
    void RenderOpaque(const RenderContext& rc);
    // 準備済みの不透明メッシュを描画します。
    void RenderPreparedOpaque(const RenderContext& rc, bool forceShaderId = false, ShaderId forcedShaderId = ShaderId::PBR);
    // 透明モデルを描画します。
    void RenderTransparent(const RenderContext& rc);

    // IBL テクスチャを設定します。
    void SetIBL(const std::string& diffusePath, const std::string& specularPath);

private:
    // スキニング用のボーン行列をシェーダーへ渡す定数バッファです。
    struct CbSkeleton
    {
        DirectX::XMFLOAT4X4 boneTransforms[256];
        DirectX::XMFLOAT4X4 prevBoneTransforms[256];
    };

    // 1 回のモデル描画に必要な情報をまとめます。
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
        const MaterialAsset* materialAsset = nullptr;
        BlendState blendState;
        DepthState depthState;
        RasterizerState rasterizerState;
    };

    // 透明モデルのソートに必要な描画情報をまとめます。
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
        const MaterialAsset* materialAsset = nullptr;
        BlendState blendState;
        DepthState depthState;
        RasterizerState rasterizerState;
    };

    // スケルトン用定数バッファの内容を作成します。
    void FillSkeletonConstantBuffer(const ModelResource::MeshResource& meshResource,
        const DirectX::XMFLOAT4X4& worldMatrix,
        const DirectX::XMFLOAT4X4& prevWorldMatrix,
        CbSkeleton& cbSkeleton) const;
    // スケルトン用定数バッファを描画へ適用します。
    void ApplySkeletonConstantBuffer(const RenderContext& rc, const CbSkeleton& cbSkeleton) const;
    // マテリアルの上書き値をシェーダーへ適用します。
    void ApplyMaterialOverrides(ShaderId shaderId, Shader* shader,
        const DirectX::XMFLOAT4& baseColor,
        float metallic,
        float roughness,
        float emissive,
        const MaterialAsset* materialAsset) const;

    std::unique_ptr<Shader> shaders[static_cast<int>(ShaderId::EnumCount)];
    std::vector<DrawInfo> drawInfos;
    std::vector<TransparencyDrawInfo> transparencyDrawInfos;
    std::unique_ptr<IBuffer> skeletonConstantBuffer;
    std::shared_ptr<ITexture> currentDiffuseIBL;
    std::shared_ptr<ITexture> currentSpecularIBL;
};
