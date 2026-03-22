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
class Actor;

class ShadowMap
{
public:
    static const int CASCADE_COUNT = 3;

    ShadowMap(IResourceFactory* factory);
    ~ShadowMap();

    void UpdateCascades(const RenderContext& rc);
    void BeginCascade(const RenderContext& rc, int cascadeIndex);
    void End(const RenderContext& rc);

    void DrawSceneImmediate(const RenderContext& rc, const std::vector<std::shared_ptr<Actor>>& actors);
    void Draw(const RenderContext& rc, const ModelResource* modelResource, const DirectX::XMFLOAT4X4& worldMatrix);
    void DrawInstanced(const RenderContext& rc, const ModelResource* modelResource,
        int meshIndex,
        IBuffer* instanceBuffer, uint32_t instanceStride, uint32_t firstInstance, uint32_t instanceCount,
        IBuffer* argumentBuffer = nullptr, uint32_t argumentOffsetBytes = 0);

    ITexture* GetTexture() const { return m_shadowTexture.get(); }
    ISampler* GetSamplerState() const { return samplerState.get(); }
    const DirectX::XMFLOAT4X4& GetLightViewProjection(int index) const { return shadowMatrices[index]; }
    float GetCascadeEnd(int index) const { return cascadeEndClips[index]; }
    float GetTexelSize() const { return 1.0f / static_cast<float>(textureSize); }

private:
    std::array<DirectX::XMVECTOR, 8> GetFrustumCorners(float fov, float aspect, float nearZ, float farZ, const DirectX::XMFLOAT4X4& viewMat);
    DirectX::XMMATRIX CalcCascadeMatrix(const RenderContext& rc, float nearZ, float farZ);

    struct CbScene { DirectX::XMFLOAT4X4 lightViewProjection; };
    struct CbSkeleton { DirectX::XMFLOAT4X4 boneTransforms[256]; };

    const UINT textureSize = 4096;

    RhiViewport m_cachedViewport;
    ITexture* m_cachedRT = nullptr;
    ITexture* m_cachedDS = nullptr;

    std::unique_ptr<IPipelineState> m_pso;
    std::unique_ptr<IPipelineState> m_instancedPso;

    std::shared_ptr<ITexture> m_shadowTexture;
    std::vector<std::shared_ptr<ITexture>> m_cascadeTextures;

    std::unique_ptr<ISampler> samplerState;
    std::unique_ptr<IShader> vertexShader;
    std::unique_ptr<IShader> instancedVertexShader;
    std::unique_ptr<IInputLayout> inputLayout;
    std::unique_ptr<IInputLayout> instancedInputLayout;
    std::unique_ptr<IBuffer> sceneConstantBuffer;
    std::unique_ptr<IBuffer> skeletonConstantBuffer;

    std::array<DirectX::XMFLOAT4X4, CASCADE_COUNT> shadowMatrices;
    std::array<float, CASCADE_COUNT> cascadeEndClips;
    const std::array<float, CASCADE_COUNT> cascadeSplits = { 0.05f, 0.2f, 1.0f };
};
