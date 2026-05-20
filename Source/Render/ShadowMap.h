#pragma once

#include <array>
#include <memory>
#include <vector>
#include <DirectXMath.h>
#include "Model/Model.h"
#include "RHI/ITexture.h"
#include "RHI/ICommandList.h"

class IShader;
class IBuffer;
class ISampler;
class IInputLayout;
class IResourceFactory;
class IPipelineState;
struct RenderContext;
class ShadowMap
{
public:
    static const int CASCADE_COUNT = 3;

    // cascade shadow map 用 resource / shader / PSO を作成する。
    ShadowMap(IResourceFactory* factory);
    ~ShadowMap();

    // camera と light から cascade ごとの light view projection を更新する。
    void UpdateCascades(const RenderContext& rc);
    // 指定 cascade slice を depth render target として描画開始する。
    void BeginCascade(const RenderContext& rc, int cascadeIndex);
    // shadow map 全体を shader resource 状態へ戻す。
    void End(const RenderContext& rc);

    // 単体 model を shadow map へ描画する。
    void Draw(const RenderContext& rc, const ModelResource* modelResource, const DirectX::XMFLOAT4X4& worldMatrix);
    // instance buffer または indirect argument を使って shadow map へ描画する。
    void DrawInstanced(const RenderContext& rc, const ModelResource* modelResource,
        int meshIndex,
        IBuffer* instanceBuffer, uint32_t instanceStride, uint32_t firstInstance, uint32_t instanceCount,
        IBuffer* argumentBuffer = nullptr, uint32_t argumentOffsetBytes = 0);
    // 複数 indirect command をまとめて shadow map へ描画する。
    void DrawInstancedMulti(const RenderContext& rc, const ModelResource* modelResource,
        int meshIndex,
        IBuffer* instanceBuffer, uint32_t instanceStride,
        IBuffer* argumentBuffer, uint32_t argumentOffsetBytes,
        uint32_t commandCount, uint32_t commandStride);

    ITexture* GetTexture() const { return m_shadowTexture.get(); }
    ISampler* GetSamplerState() const { return samplerState.get(); }
    const DirectX::XMFLOAT4X4& GetLightViewProjection(int index) const { return shadowMatrices[index]; }
    float GetCascadeEnd(int index) const { return cascadeEndClips[index]; }
    float GetTexelSize() const { return 1.0f / static_cast<float>(textureSize); }

private:
    // view space frustum の 8 corner を world space へ戻す。
    std::array<DirectX::XMVECTOR, 8> GetFrustumCorners(float fov, float aspect, float nearZ, float farZ, const DirectX::XMFLOAT4X4& viewMat);
    // cascade 範囲にフィットする light view projection を計算する。
    DirectX::XMMATRIX CalcCascadeMatrix(const RenderContext& rc, float nearZ, float farZ);

    struct CbScene { DirectX::XMFLOAT4X4 lightViewProjection; }; // shadow pass 用 light 行列。
    struct CbSkeleton { DirectX::XMFLOAT4X4 boneTransforms[256]; }; // skinning 用 bone 行列。

    const UINT textureSize = 4096; // shadow map 1 cascade あたりの解像度。

    RhiViewport m_cachedViewport; // BeginCascade 前の viewport。End で復元する。
    ITexture* m_cachedRT = nullptr; // BeginCascade 前の main render target。非所有。
    ITexture* m_cachedDS = nullptr; // BeginCascade 前の main depth。非所有。

    std::unique_ptr<IPipelineState> m_pso; // 通常 mesh 用 shadow PSO。
    std::unique_ptr<IPipelineState> m_instancedPso; // instancing 用 shadow PSO。

    std::shared_ptr<ITexture> m_shadowTexture; // 全 cascade を含む texture array SRV。
    std::vector<std::shared_ptr<ITexture>> m_cascadeTextures; // cascade slice ごとの DSV wrapper。

    std::unique_ptr<ISampler> samplerState; // shadow map sampling 用 sampler。
    std::unique_ptr<IShader> vertexShader; // 通常 mesh 用 vertex shader。
    std::unique_ptr<IShader> instancedVertexShader; // instancing 用 vertex shader。
    std::unique_ptr<IInputLayout> inputLayout; // 通常 mesh 用 input layout。
    std::unique_ptr<IInputLayout> instancedInputLayout; // instancing 用 input layout。
    std::unique_ptr<IBuffer> sceneConstantBuffer; // CbScene 用 buffer。
    std::unique_ptr<IBuffer> skeletonConstantBuffer; // CbSkeleton 用 buffer。

    std::array<DirectX::XMFLOAT4X4, CASCADE_COUNT> shadowMatrices; // cascade ごとの light view projection。
    std::array<float, CASCADE_COUNT> cascadeEndClips; // camera far clip に対する cascade 終端距離。
    const std::array<float, CASCADE_COUNT> cascadeSplits = { 0.05f, 0.2f, 1.0f }; // cascade 分割比率。
};
